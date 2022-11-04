// =STS=> XmlFile.cpp[2738].aa05   open     SMID:5 
//////////////////////////////////////////////////////////////////////////////
//
// (c) Copyright 2010 - 2011 Blackrock Microsystems
//
// $Workfile: XmlFile.cpp $
// $Archive: /common/XmlFile.cpp $
// $Revision: 1 $
// $Date: 6/15/10 9:05a $
// $Author: Ehsan $
//
// $NoKeywords: $
//
//////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "XmlFile.h"
#include "XmlItem.h"
#include "debugmacs.h"

// Uncomment to debug
//#define DEBUG_XMLFILE

// Author & Date: Ehsan Azar       16 April 2012
// Purpose: Copy Constructor for Xml item
XmlItem::XmlItem(const XmlItem & other)
{
    m_xmlValue = other.m_xmlValue;
    m_xmlTag = other.m_xmlTag;
    m_xmlAttribs = other.m_xmlAttribs;
}

// Author & Date: Ehsan Azar       15 June 2010
// Purpose: Constructor for Xml handling class
// Inputs:
//   fname - the xml file name
//   bRead - if file should be read
XmlFile::XmlFile(const QString fname, bool bRead) :
    m_fname(fname), m_bError(false)
{
    if (fname.isEmpty())
    {
        m_doc.setContent(QString("")); // Start with empty xml
    } else { 
        if (!bRead || setContent())
        {
            m_doc.setContent(QString("")); // Start with empty xml
            // Add XML version
            QDomProcessingInstruction instr = m_doc.createProcessingInstruction("xml", "version='1.0' encoding='UTF-8'");
            m_doc.appendChild(instr);
        }
    }
}

// Author & Date: Ehsan Azar       18 April 2012
// Purpose: Set XML content from XML filename for parsing
// Outputs:
//  Returns false if successful, otherwise returns true
bool XmlFile::setContent()
{
    QString errMsg;
    QFile file(m_fname);
    m_bError = !file.open(QIODevice::ReadOnly);
    if (m_bError)
        return true;
    // read previous content to xml
    m_bError = !m_doc.setContent(&file, &errMsg);
    file.close();
    return m_bError;
}

// Author & Date: Ehsan Azar       4 April 2012
// Purpose: Set XML content for parsing
// Inputs:
//   content - valid XML content
// Outputs:
//  Returns false if successful, otherwise returns true
bool XmlFile::setContent(const QString & content)
{
    QString errMsg;
    m_bError = !m_doc.setContent(content, &errMsg);
    return m_bError;
}

// Author & Date: Ehsan Azar       19 April 2012
// Purpose: Add list iteratively
// Inputs:
//   list     - list to add to current node
//   nodeName - last node name
// Outputs:
//   Returns true if this is an array added
bool XmlFile::AddList(QVariantList & list, QString nodeName)
{
    if (list.isEmpty())
        return false;
    QMap<QString, int> mapItemCount;
    int count = 0, subcount = 1;
    // itemize the list
    for (int j = 0; j < list.size(); ++j)
    {
        QVariant subval = list[j];
        if (!subval.isValid())
            continue;
        count++; // count all valid items
        QString strSubKey;
        QMap<QString, QVariant> attribs;
        if (subval.typeId() == QMetaType::User)
        {
            const auto * item = static_cast<const XmlItem *>(subval.data());
            strSubKey = item->XmlName();
            attribs = item->XmlAttribs();
        }
        if (strSubKey.isEmpty())
            strSubKey = nodeName + "_item";
        subcount = mapItemCount[strSubKey];
        mapItemCount[strSubKey] = subcount + 1;
        if (subcount)
            strSubKey = strSubKey + QString("<%1>").arg(subcount);
        // Recursively add this item
        beginGroup(strSubKey, attribs, subval);
        endGroup();
    } // end for (int j
    // If all items had same tag name, it is an array
    if (count > 1 && count == subcount + 1)
        return true;
    return false;
}

// Author & Date: Ehsan Azar       3 April 2012
// Purpose: Begin a new Xml group or navigate to it if it exists
// Inputs:
//   nodeName  - the node tag (it can be in the form of relative/path/to/node)
//                node index (0-based) is enclosed (relative/path/to/node<2>/subnode)
//             Note: nodeName tags are always created if they do not exist
//   attribs   - attribute name, attribute value pairs
//   value     - new node value, if non-empty
// Outputs:
//  Returns true if nodeName is created
bool XmlFile::beginGroup(QString nodeName, const QMap<QString, QVariant> attribs, const QVariant & value)
{
    bool bRet = false;
    QDomElement set;

    if (nodeName.isEmpty())
        nodeName = firstChildKey();
    if (nodeName.isEmpty())
        nodeName = "XmlItem";
    // Get the node path
    QStringList nodepath = nodeName.split("/");
    int level = nodepath.count();
    m_levels.append(level);
    for (int i = 0; i < level; ++i)
    {
        QString strTagName = nodepath[i];
        // Extract index
        int index = 0;
        {
            int idx = strTagName.indexOf("<");
            if (idx >= 0)
            {
                index = strTagName.mid(idx + 1, strTagName.length() - idx - 2).toInt();
                strTagName = strTagName.left(idx);
                nodepath[i] = strTagName;
            }
        }
        // Look if the element (with given index) exists then get it
        QDomNode parent;
        if (!m_nodes.isEmpty())
        {
            // Get the current node
            parent = m_nodes.last();
        } else {
            parent = m_doc;
        }
        int count = 0;
        for(QDomElement elem = parent.firstChildElement(strTagName); !elem.isNull(); elem = elem.nextSiblingElement(strTagName))
        {
            count++;
            if (count == index + 1)
            {
                set = elem;
                break;
            }
        }
        // Create all new subnodes
        for (int j = 0; j < (index + 1 - count); ++j)
        {
            bRet = true;
            set = m_doc.createElement(strTagName);
            parent.appendChild(set);
        }
        // Add all the parent nodes without attribute or value
        if (i < level - 1)
            m_nodes.append(set);
    }
    
    // Now add the node to the end of the list
    m_nodes.append(set);

    // if there is some text/value to set
    if (value.isValid())
    {
        bool bTextLeaf = false;
        QVariantList varlst;
        switch (value.typeId())
        {
        case QMetaType::QStringList:
        case QMetaType::QVariantList:
            varlst = value.toList();
            if (AddList(varlst, nodepath.last()))
                set.setAttribute("Type", "Array");
            break;
        default:
            QString text;
            if (value.typeId() == QMetaType::User)
            {
                const auto * item = static_cast<const XmlItem *>(value.data());
                QVariant subval = item->XmlValue();
                
                if (subval.typeId() == QMetaType::User)
                {
                    const auto * subitem = static_cast<const XmlItem *>(subval.data());
                    QString strSubKey = subitem->XmlName();
                    QMap<QString, QVariant> _attribs = subitem->XmlAttribs();
                    // Recursively add this item
                    beginGroup(strSubKey, _attribs, subval);
                    endGroup();
                } 
                else if (subval.typeId() == QMetaType::QVariantList || subval.typeId() == QMetaType::QStringList)
                {
                    varlst = subval.toList();
                    if (AddList(varlst, nodepath.last()))
                        set.setAttribute("Type", "Array");
                } else {
                    text = subval.toString();
                    bTextLeaf = true;
                }
            } else {
                text = value.toString();
                bTextLeaf = true;
            }
            if (bTextLeaf)
            {
                // Remove all the children
                while (set.hasChildNodes())
                    set.removeChild(set.lastChild());
                // See if this is Xml fragment string
                XmlFile xml;
                if (!xml.setContent(text))
                {
                    QDomNode frag = m_doc.importNode(xml.getFragment(), true);
                    set.appendChild(frag);
                } else {
                    QDomText domText = m_doc.createTextNode(text);
                    set.appendChild(domText);
                }
            }
        }
    }

    // Add all the additional attributes
    QMap<QString, QVariant>::const_iterator iterator;
    for (iterator = attribs.begin(); iterator != attribs.end(); ++iterator)
    {
        const QString& attrName = iterator.key();
        const QVariant& attrValue = iterator.value();
        switch (attrValue.typeId())
        {
        case QMetaType::QString:
            set.setAttribute(attrName, attrValue.toString());
            break;
        case QMetaType::Int:
            set.setAttribute(attrName, attrValue.toInt());
            break;
        case QMetaType::UInt:
            set.setAttribute(attrName, attrValue.toUInt());
            break;
        case QMetaType::LongLong:
            set.setAttribute(attrName, attrValue.toLongLong());
            break;
        case QMetaType::ULongLong:
            set.setAttribute(attrName, attrValue.toULongLong());
            break;
        default:
            // everything else is treated as double floating point
            set.setAttribute(attrName, attrValue.toDouble());
            break;
        }
    }
    return bRet;
}

// Author & Date: Ehsan Azar       15 June 2010
// Purpose: Begin a new Xml group or navigate to it if it exists 
//           (overloaded for single attribute case)
// Inputs:
//   nodeName  - the node tag (it can be in the form of relative/path/to/node)
//                node index (0-based) is enclosed (relative/path/to/node<2>/subnode)
//             Note: nodeName tags are always created if they do not exist
//   attrName  - node attribute name, if non-empty
//   attrValue - new node attribute value, if attrName is non-empty
//   value     - new node value, if non-empty
// Outputs:
//  Returns true if nodeName is created
bool XmlFile::beginGroup(const QString & nodeName, const QString & attrName,
                         const QVariant & attrValue, const QVariant & value)
{
    QMap<QString, QVariant> attribs;
    if (!attrName.isEmpty())
        attribs.insert(attrName, attrValue);
    return beginGroup(nodeName, attribs, value);
}

// Author & Date: Ehsan Azar       23 April 2012
// Purpose: Begin and end a new Xml group
//           (overloaded for single attribute case)
//           (simplify single node addition)
// Inputs:
//   nodeName  - the node tag (it can be in the form of relative/path/to/node)
//                node index (0-based) is enclosed (relative/path/to/node<2>/subnode)
//             Note: nodeName tags are always created if they do not exist
//   attrName  - node attribute name, if non-empty
//   attrValue - new node attribute value, if attrName is non-empty
//   value     - new node value, if non-empty
// Outputs:
//  Returns true if nodeName is created
bool XmlFile::addGroup(const QString & nodeName, const QString & attrName,
                         const QVariant & attrValue, const QVariant & value)
{
    bool bRes = beginGroup(nodeName, attrName, attrValue, value);
    endGroup();
    return bRes;
}

// Author & Date: Ehsan Azar       15 June 2010
// Purpose: End last group
// Inputs:
//   bSave - if true and ending the last group, xml is saved to disk
// Output:
//   Returns false if successful, true otherwise
bool XmlFile::endGroup(bool bSave /* = true */)
{
    // fetch and pop the last level
    int level = 1;
    if (!m_levels.isEmpty())
    {
        level = m_levels.last();
        m_levels.removeLast();
    }
    // pop all the last tags
    for (int i = 0; i < level; ++i)
        if (!m_nodes.isEmpty())
            m_nodes.removeLast();

    // if there is no more node
    if (m_nodes.isEmpty())
    {
        // If we should save to disk
        if (bSave)
        {
            QFile file(m_fname);
            // Overwrite xml
            if (!file.open(QIODevice::WriteOnly))
                return true;
            // write to disk
            QTextStream ts(&file);
            ts << m_doc.toString();
            file.close();
        }
#ifdef DEBUG_XMLFILE
        if (bSave)
            TRACE("save doc: %s\n", m_doc.toString().toLatin1().constData());
        else
            TRACE("load doc: %s\n", m_doc.toString().toLatin1().constData());
#endif
    }

    return false; // no error
}

// Author & Date: Ehsan Azar       12 April 2012
// Purpose: Get attribute of the current node 
// Inputs:
//   attrName  - node attribute name
// Outputs:
//   Returns node attribute value
QString XmlFile::attribute(const QString & attrName)
{
    QString res;
    if (!m_nodes.isEmpty())
    {
        // Get the current node
        QDomElement node = m_nodes.last();
        res = node.attribute(attrName);
    }
    return res;
}

// Author & Date: Ehsan Azar       15 June 2010
// Purpose: Get current node value
// Inputs:
//   val - the default value
QVariant XmlFile::value(const QVariant & val)
{
    QVariant res = val;

    if (!m_nodes.isEmpty())
    {
        // Get the current node
        QDomElement node = m_nodes.last();
        if (!node.isNull())
        {
            // Array Type is how we distinguish lists
            if (node.attribute("Type").compare("Array", Qt::CaseInsensitive) == 0)
            {
                QVariantList varlist;
                QStringList keys = childKeys();
                for (int i = 0; i < keys.count(); ++i)
                {
                    QString key = keys[i];
                    if (i > 0 && key == keys[i - 1])
                        key = QString(key + "<%1>").arg(i);
                    // Recursively return the list
                    beginGroup(key);
                    QVariant nodevalue = value(QString());
                    endGroup();
                    // Make sure value is meaningful
                    if (nodevalue.isValid())
                        varlist += nodevalue; // add new value
                }
                if (!keys.isEmpty())
                    res = varlist;
            } else {
                QDomNode child = node.firstChild();
                if (!child.isNull())
                {
                    QDomText domText = child.toText();
                    if (!domText.isNull())
                        res = domText.data();
                    else
                        return toString();
                }
            }
        }
    }
    return res;
}

// Author & Date: Ehsan Azar       23 Aug 2011
// Purpose: Check if a node exists
// Inputs:
//   nodeName  - the node tag (it can be in the form of relative/path/to/node)
// Outputs:
//   Returns true if the current node contains the given node
bool XmlFile::contains(const QString & nodeName)
{
    QDomElement parent; // parent element
    QDomElement set;

    QStringList nodepath = nodeName.split("/");
    int level = nodepath.count();

    // If it is the first node
    if (m_nodes.isEmpty())
    {
        set = m_doc.namedItem(nodepath[0]).toElement();
    } else {
        // Get the parent node
        parent = m_nodes.last();
        // Look if the node exists then get it
        set = parent.namedItem(nodepath[0]).toElement();
    }
    if (set.isNull())
        return false; // not found

    // Look for higher levels
    for (int i = 1; i < level; ++i)
    {
        parent = set;
        set = parent.namedItem(nodepath[i]).toElement();
        if (set.isNull())
            return false; // not found
    }
    return true;
}

// Author & Date: Ehsan Azar       23 Aug 2011
// Purpose: Get all child elements
// Inputs:
//   count - maximum number of children to retrieve (-1 means all)
// Outputs:
//   Returns a list of element childern of the current element node
QStringList XmlFile::childKeys(int count) const
{
    QStringList res;

    QDomNode parent;
    if (!m_nodes.isEmpty())
    {
        // Get the current node
        parent = m_nodes.last();
    } else {
        parent = m_doc;
    }
    if (!parent.isNull())
    {
        for(QDomElement elem = parent.firstChildElement(); !elem.isNull(); elem = elem.nextSiblingElement())
        {
            res += elem.nodeName();
            if (count > 0)
            {
                count--;
                if (count == 0)
                    break;
            }
        }
    }
    return res;
}

// Author & Date: Ehsan Azar       6 April 2012
// Purpose: Get the first child element
// Outputs:
//   Returns the name of the first child element
QString XmlFile::firstChildKey() const
{
    QStringList list = childKeys(1);
    return list.last();
}

// Author & Date: Ehsan Azar       3 April 2012
// Purpose: Get the string equivalent of the current node or document
// Outputs:
//   Returns a string of the current node (and its children) as separate XML document string
QString XmlFile::toString()
{
    QDomElement node;
    // If it is the first node
    if (m_nodes.isEmpty())
    {
        return m_doc.toString();
    }
    // Create a document with new root 
    QDomDocument doc;
    node = m_nodes.last().cloneNode().toElement();
    if (!node.isNull())
    {
        doc.importNode(node, true);
        doc.appendChild(node);
    }
    return doc.toString();
}

// Author & Date: Ehsan Azar       5 April 2012
// Purpose: Get document fragment one level deep
//           this fragment can be imported in other documents
// Outputs:
//   Returns a document fragment
QDomDocumentFragment XmlFile::getFragment()
{
    QDomElement node;
    if (m_nodes.isEmpty())
        node = m_doc.firstChildElement();
    else
        node = m_nodes.last();
    QDomDocumentFragment frag = m_doc.createDocumentFragment();
    QDomNodeList list = node.childNodes();
    for (int i = 0; i < list.length(); ++i)
    {
        QDomNode node = list.item(i).cloneNode();
        frag.appendChild(node);
    }
    return frag;
}
