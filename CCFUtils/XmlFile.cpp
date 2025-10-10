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

#include "XmlFile.h"
#include "XmlItem.h"
#include <string>
#include <sstream>
#include <vector>
#include <algorithm>
#include <cstdint>

// Uncomment to debug
//#define DEBUG_XMLFILE

std::vector<std::string> split(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(str);
    while (std::getline(tokenStream, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

// Helper function to convert std::any to string
// Unlike QVariant::toString(), std::any doesn't auto-convert types
std::string anyToString(const std::any& val) {
    if (!val.has_value()) {
        return "";
    }

    // Direct string types
    if (val.type() == typeid(std::string)) {
        return std::any_cast<std::string>(val);
    }
    if (val.type() == typeid(const char*)) {
        return std::string(std::any_cast<const char*>(val));
    }
    if (val.type() == typeid(char*)) {
        return std::string(std::any_cast<char*>(val));
    }

    // Numeric type conversions
    // Note: We check both standard types and fixed-width types since they may differ
    if (val.type() == typeid(int) || val.type() == typeid(int32_t)) {
        if (val.type() == typeid(int)) {
            return std::to_string(std::any_cast<int>(val));
        } else {
            return std::to_string(std::any_cast<int32_t>(val));
        }
    }
    if (val.type() == typeid(unsigned int) || val.type() == typeid(uint32_t)) {
        if (val.type() == typeid(unsigned int)) {
            return std::to_string(std::any_cast<unsigned int>(val));
        } else {
            return std::to_string(std::any_cast<uint32_t>(val));
        }
    }
    if (val.type() == typeid(short) || val.type() == typeid(int16_t)) {
        if (val.type() == typeid(short)) {
            return std::to_string(std::any_cast<short>(val));
        } else {
            return std::to_string(std::any_cast<int16_t>(val));
        }
    }
    if (val.type() == typeid(unsigned short) || val.type() == typeid(uint16_t)) {
        if (val.type() == typeid(unsigned short)) {
            return std::to_string(std::any_cast<unsigned short>(val));
        } else {
            return std::to_string(std::any_cast<uint16_t>(val));
        }
    }
    if (val.type() == typeid(char) || val.type() == typeid(int8_t)) {
        if (val.type() == typeid(char)) {
            return std::to_string(static_cast<int>(std::any_cast<char>(val)));
        } else {
            return std::to_string(static_cast<int>(std::any_cast<int8_t>(val)));
        }
    }
    if (val.type() == typeid(unsigned char) || val.type() == typeid(uint8_t)) {
        if (val.type() == typeid(unsigned char)) {
            return std::to_string(static_cast<unsigned int>(std::any_cast<unsigned char>(val)));
        } else {
            return std::to_string(static_cast<unsigned int>(std::any_cast<uint8_t>(val)));
        }
    }
    if (val.type() == typeid(signed char)) {
        return std::to_string(static_cast<int>(std::any_cast<signed char>(val)));
    }
    if (val.type() == typeid(long)) {
        return std::to_string(std::any_cast<long>(val));
    }
    if (val.type() == typeid(unsigned long)) {
        return std::to_string(std::any_cast<unsigned long>(val));
    }
    if (val.type() == typeid(long long)) {
        return std::to_string(std::any_cast<long long>(val));
    }
    if (val.type() == typeid(unsigned long long)) {
        return std::to_string(std::any_cast<unsigned long long>(val));
    }
    if (val.type() == typeid(float)) {
        return std::to_string(std::any_cast<float>(val));
    }
    if (val.type() == typeid(double)) {
        return std::to_string(std::any_cast<double>(val));
    }

    // If we can't convert, return type name for debugging
    // In Qt, QVariant would auto-convert unknown types to empty string
    // For std::any, we'll return the type name to help identify the issue
    std::string type_name = val.type().name();
    // Try one last time to cast to string
    try {
        return std::any_cast<std::string>(val);
    } catch (const std::bad_any_cast&) {
        // Return empty string like QVariant would have
        return "";
    }
}


bool iequals(const std::string& a, const std::string& b)
{
    return std::equal(a.begin(), a.end(),
                      b.begin(), b.end(),
                      [](char a, char b) {
                          return tolower(a) == tolower(b);
                      });
}

pugi::xml_node find_or_create(pugi::xml_node& parent, const std::string& name) {
    pugi::xml_node node = parent.child(name.c_str());

    // If the node doesn't exist, create a new one
    if (!node) {
        node = parent.append_child(name.c_str());
    }

    return node;
}

pugi::xml_attribute find_or_create_attribute(pugi::xml_node& node, const std::string& name) {
    pugi::xml_attribute attr = node.attribute(name.c_str());
    // If the attribute doesn't exist, add a new one
    if (!attr)
        attr = node.append_attribute(name.c_str());
    return attr;
}

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
XmlFile::XmlFile(std::string fname, bool bRead) :
    m_fname(fname), m_bError(false)
{
    if (m_fname.empty()) {
        m_doc.reset();
        return; // Empty in-memory document; declaration deferred until first save
    }
    if (bRead) {
        // Attempt to load; set error flag on failure but leave an empty doc
        if (setContent()) {
            m_bError = true;
            m_doc.reset();
            // Provide a declaration so subsequent writes still produce valid XML
            auto decl = m_doc.prepend_child(pugi::node_declaration);
            decl.append_attribute("version") = "1.0";
            decl.append_attribute("encoding") = "UTF-8";
        }
        // On success, m_doc already contains parsed content; do not reset or add duplicate declaration
    } else {
        // Write mode: start blank and add declaration now
        m_doc.reset();
        auto decl = m_doc.prepend_child(pugi::node_declaration);
        decl.append_attribute("version") = "1.0";
        decl.append_attribute("encoding") = "UTF-8";
    }
}

// Purpose: Set XML content from XML filename for parsing
// Outputs:
//  Returns false if successful, otherwise returns true
bool XmlFile::setContent()
{
    pugi::xml_parse_result res = m_doc.load_file(m_fname.c_str());
    return res.status != pugi::xml_parse_status::status_ok;
}

// Author & Date: Ehsan Azar       4 April 2012
// Purpose: Set XML content for parsing
// Inputs:
//   content - valid XML content
// Outputs:
//  Returns false if successful, otherwise returns true
bool XmlFile::setContent(const std::string & content)
{
    pugi::xml_parse_result result = m_doc.load_string(content.c_str());
    return result != pugi::xml_parse_status::status_ok;
}

// Author & Date: Ehsan Azar       19 April 2012
// Purpose: Add list iteratively
// Inputs:
//   list     - list to add to current node
//   nodeName - last node name
// Outputs:
//   Returns true if this is an array added
bool XmlFile::AddList(std::vector<std::any> & list, std::string nodeName)
{
    if (list.empty())
        return false;
    std::map<std::string, int> mapItemCount;
    int count = 0;
    int subcount = 1;
    // itemize the list
    for (auto subval : list)
    {
        if (!subval.has_value())
            continue;
        count++; // count all valid items
        std::string strSubKey;
        std::map<std::string, std::any> attribs;
        if (subval.type() == typeid(const XmlItem *))
        {
            const auto * item = std::any_cast<const XmlItem *>(subval);
            // const auto * item = std::get<XmlItem *>(subval);
            strSubKey = item->XmlName();
            attribs = item->XmlAttribs();
        }
        if (strSubKey.empty())
            strSubKey = nodeName + "_item";
        subcount = mapItemCount[strSubKey];
        mapItemCount[strSubKey] = subcount + 1;
        if (subcount)
            strSubKey = strSubKey + "<" + std::to_string(subcount) + ">";

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
//   attribs   - attribute <name, value> pairs
//   value     - new node value, if non-empty
// Outputs:
//  Returns true if nodeName is created
bool XmlFile::beginGroup(std::string nodeName, const std::map<std::string, std::any> attribs, const std::any & value)
{
    bool bRet = false;

    if (nodeName.empty())
        nodeName = firstChildKey();
    if (nodeName.empty())
        nodeName = "XmlItem";

    // Get the node path
    std::vector<std::string> nodepath = split(nodeName, '/');

    int level = nodepath.size();
    m_levels.push_back(level);

    pugi::xml_node parent;
    pugi::xml_node set;

    for (int i = 0; i < level; ++i)
    {
        std::string strTagName = nodepath[i];
        // Extract index
        int index = 0;
        {
            int idx = strTagName.find('<');
            if (idx >= 0)
            {
                index = std::stoi(strTagName.substr(idx + 1, strTagName.length() - idx - 2));
                strTagName = strTagName.substr(0, idx);
                nodepath[i] = strTagName;
            }
        }
        // Look if the element (with given index) exists then get it
        if (!m_nodes.empty())
        {
            // Get the current node
            parent = m_nodes.back();
        } else {
            parent = m_doc;
        }
        int count = 0;
        for(pugi::xml_node node = parent.child(strTagName.c_str()); node; node = node.next_sibling(strTagName.c_str()))
        {
            count++;
            if (count == index + 1)
            {
                set = node;
                break;
            }
        }
        // Create all new subnodes
        for (int j = 0; j < (index + 1 - count); ++j)
        {
            bRet = true;
            set = parent.append_child(strTagName.c_str());
        }
        // Add all the parent nodes without attribute or value
        if (i < level - 1)
            m_nodes.push_back(set);
    }
    
    // Now add the node to the end of the list
    m_nodes.push_back(set);

    // if there is some text/value to set
    if (value.has_value())
    {
        bool bTextLeaf = false;
        std::vector<std::any> vec;
        if ((value.type() == typeid(std::vector<std::string>)) || (value.type() == typeid(std::vector<std::any>))) {
            if (value.type() == typeid(std::vector<std::any>)) {
                vec = std::any_cast<std::vector<std::any>>(value);
            } else {
                const auto & vstr = std::any_cast<const std::vector<std::string>&>(value);
                vec.reserve(vstr.size());
                for (const auto & s : vstr) vec.push_back(std::any(s));
            }
            if (AddList(vec, nodepath.back()))
                set.append_attribute("Type") = "Array";
        }
        else
        {
            std::string text;
            if (value.type() == typeid(const XmlItem *))
            {
                const auto * item = std::any_cast<const XmlItem *>(value);
                std::any subval = item->XmlValue();
                
                if (subval.type() == typeid(const XmlItem *))
                {
                    const auto * subitem = std::any_cast<const XmlItem *>(subval);
                    std::string strSubKey = subitem->XmlName();
                    std::map<std::string, std::any> _attribs = subitem->XmlAttribs();
                    // Recursively add this item
                    beginGroup(strSubKey, _attribs, subval);
                    endGroup();
                } 
                else if ((subval.type() == typeid(std::vector<std::string>)) || (subval.type() == typeid(std::vector<std::any>)))
                {
                    if (subval.type() == typeid(std::vector<std::any>)) {
                        vec = std::any_cast<std::vector<std::any>>(subval);
                    } else {
                        const auto & vstr = std::any_cast<const std::vector<std::string>&>(subval);
                        vec.reserve(vstr.size());
                        for (const auto & s : vstr) vec.push_back(std::any(s));
                    }
                    if (AddList(vec, nodepath.back()))
                        set.append_attribute("Type") = "Array";
                } else {
                    text = anyToString(subval);
                    bTextLeaf = true;
                }
            } else {
                text = anyToString(value);
                bTextLeaf = true;
            }
            if (bTextLeaf)
            {
                // Remove all the children
                while (set.first_child())
                    set.remove_child(set.last_child());
                // See if this is Xml fragment string
                XmlFile xml;
                if (!xml.setContent(text))
                {
                    pugi::xml_document doc;
                    pugi::xml_parse_result result = doc.load_string(text.c_str());
                    if (result)
                        set.append_copy(doc.document_element());
                } else {
                    set.text().set(text.c_str());
                }
            }
        }
    }

    // Add all the additional attributes
    std::map<std::string, std::any>::const_iterator iterator;
    for (iterator = attribs.begin(); iterator != attribs.end(); ++iterator)
    {
        const std::string& attrName = iterator->first;
        const std::any& attrValue = iterator->second;
        if (attrValue.type() == typeid(std::string))
            find_or_create_attribute(set, attrName) = std::any_cast<std::string>(attrValue).c_str();
        else if (attrValue.type() == typeid(int))
            find_or_create_attribute(set, attrName) = std::any_cast<int>(attrValue);
        else if (attrValue.type() == typeid(unsigned int))
            find_or_create_attribute(set, attrName) = std::any_cast<unsigned int>(attrValue);
        else if (attrValue.type() == typeid(long long))
            find_or_create_attribute(set, attrName) = std::any_cast<long long>(attrValue);
        else if (attrValue.type() == typeid(unsigned long long))
            find_or_create_attribute(set, attrName) = std::any_cast<unsigned long long>(attrValue);
        else
            find_or_create_attribute(set, attrName) = std::any_cast<double>(attrValue);
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
bool XmlFile::beginGroup(const std::string & nodeName, const std::string & attrName,
                         const std::any & attrValue, const std::any & value)
{
    std::map<std::string, std::any> attribs;
    if (!attrName.empty())
        attribs.insert({attrName, attrValue});
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
bool XmlFile::addGroup(const std::string & nodeName, const std::string & attrName,
                         const std::any & attrValue, const std::any & value)
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
    if (!m_levels.empty())
    {
        level = m_levels.back();
        m_levels.pop_back();
    }
    // pop all the last tags
    for (int i = 0; i < level; ++i)
        if (!m_nodes.empty())
            m_nodes.pop_back();

    // if there is no more node
    if (m_nodes.empty())
    {
        // If we should save to disk
        if (bSave)
        {
            bool saveSucceeded = m_doc.save_file(m_fname.c_str(), PUGIXML_TEXT("  "));
            if (!saveSucceeded)
                return true;
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
std::string XmlFile::attribute(const std::string & attrName)
{
    std::string res;
    if (!m_nodes.empty())
    {
        // Get the current node
        pugi::xml_node node = m_nodes.back();
        res = node.attribute(attrName.c_str()).value();
    }
    return res;
}

// Author & Date: Ehsan Azar       15 June 2010
// Purpose: Get current node value
// Inputs:
//   val - the default value
std::any XmlFile::value(const std::any & val)
{
    std::any res = val;  // TODO: Can be std::any

    if (!m_nodes.empty())
    {
        // Get the current node
        pugi::xml_node node = m_nodes.back();
        if (node)
        {
            // Array Type is how we distinguish lists
            if (iequals(node.attribute("Type").value(), "Array"))
            {
                std::vector<std::any> varlist;
                std::vector<std::string> keys = childKeys();
                for (int i = 0; i < keys.size(); ++i)
                {
                    std::string key = keys[i];
                    if (i > 0 && key == keys[i - 1])
                        key = key + "<" + std::to_string(i) + ">";
                    // Recursively return the list
                    beginGroup(key);
                    std::any nodevalue = value("");
                    endGroup();
                    // Make sure value is meaningful
                    if (nodevalue.has_value())
                        varlist.push_back(nodevalue); // add new value
                }
                if (!keys.empty())
                    res = varlist;
            } else {
                pugi::xml_node child = node.first_child();
                if (child)
                {
                    std::string childValue = child.text().as_string();
                    if (!childValue.empty())
                        res = childValue;
                    else
                        res = toString();
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
bool XmlFile::contains(const std::string & nodeName)
{
    pugi::xml_node parent; // parent element
    pugi::xml_node set;

    std::vector<std::string> nodepath = split(nodeName, '/');
    int level = nodepath.size();

    // If it is the first node
    if (m_nodes.empty())
    {
        set = m_doc.child(nodepath[0].c_str());
    } else {
        // Get the parent node
        parent = m_nodes.back();
        // Look if the node exists then get it
        set = parent.child(nodepath[0].c_str());
    }
    if (!set)
        return false; // not found

    // Look for higher levels
    for (int i = 1; i < level; ++i)
    {
        parent = set;
        set = parent.child(nodepath[i].c_str());
        if (!set)
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
std::vector<std::string> XmlFile::childKeys(int count) const
{
    std::vector<std::string> res;

    pugi::xml_node parent;
    if (!m_nodes.empty())
        parent = m_nodes.back();
    else
        parent = m_doc;
    if (parent)
    {
        for(pugi::xml_node elem = parent.first_child(); elem; elem = elem.next_sibling())
        {
            // Skip declaration or unnamed nodes to keep API expectations simple
            if (elem.type() == pugi::node_declaration) continue;
            const char * nm = elem.name();
            if (!nm || !nm[0]) continue;
            res.push_back(nm);
            if (count > 0) {
                if (--count == 0) break;
            }
        }
    }
    return res;
}

// Author & Date: Ehsan Azar       6 April 2012
// Purpose: Get the first child element
// Outputs:
//   Returns the name of the first child element
std::string XmlFile::firstChildKey() const
{
    std::vector<std::string> list = childKeys(1);
    return list.back();
}

// Author & Date: Ehsan Azar       3 April 2012
// Purpose: Get the string equivalent of the current node or document
// Outputs:
//   Returns a string of the current node (and its children) as separate XML document string
std::string XmlFile::toString()
{
    pugi::xml_node node;
    // If it is the first node
    if (m_nodes.empty())
    {
        std::ostringstream ss;
        m_doc.save(ss);
        return ss.str();
    }
    // Create a document with new root 
    pugi::xml_document doc;
    if (m_nodes.back())
    {
        doc.append_copy(m_nodes.back());
    }
    std::ostringstream ss;
    doc.save(ss);
    return ss.str();
}

// Author & Date: Ehsan Azar       5 April 2012
// Purpose: Get document fragment one level deep
//           this fragment can be imported in other documents
// Outputs:
//   Returns a document fragment
pugi::xml_document XmlFile::getFragment()
{
    pugi::xml_node node;
    if (m_nodes.empty())
        node = m_doc.first_child();
    else
        node = m_nodes.back();

    pugi::xml_document frag; // the "fragment"
    for (pugi::xml_node child = node.first_child(); child; child = child.next_sibling())
    {
        pugi::xml_node cloned_node = frag.append_copy(child);
    }
    return frag;
}
