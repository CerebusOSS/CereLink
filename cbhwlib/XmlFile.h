/* =STS=> XmlFile.h[2739].aa05   open     SMID:5 */
//////////////////////////////////////////////////////////////////////////////
//
// (c) Copyright 2010 - 2011 Blackrock Microsystems
//
// $Workfile: XmlFile.h $
// $Archive: /common/XmlFile.h $
// $Revision: 1 $
// $Date: 6/15/10 9:05a $
// $Author: Ehsan $
//
// $NoKeywords: $
//
//////////////////////////////////////////////////////////////////////////////
//
// PURPOSE:
//
// Common XML file handling as a tree
// The functions are taken to resemmble QSettings class
//

#ifndef XMLFILE_H_INCLUDED
#define XMLFILE_H_INCLUDED

#include <QString>
#include <QtXml>
#include <QList>

// Author & Date: Ehsan Azar       15 June 2010
// Purpose: XML handling
class XmlFile
{
public:
    XmlFile(const QString fname = "", bool bRead = true);
    bool setContent(); // set content from XML file
    bool setContent(const QString & content);

public:
    bool beginGroup(const QString & nodeName = "", const QString & attrName = "",
            const QVariant & attrValue = 0, const QVariant & value = QVariant()); // Begin a node
    bool beginGroup(QString nodeName, const QMap<QString, QVariant> attribs, const QVariant & value = QVariant()); // Begin a node
    bool endGroup(bool bSave = true); // End last node
    bool addGroup(const QString & nodeName = "", const QString & attrName = "",
            const QVariant & attrValue = 0, const QVariant & value = QVariant()); // Begin and end a node
    QString attribute(const QString & attrName); // Get attribute value
    QVariant value(const QVariant & val = QVariant()); // value of the current element
    bool contains (const QString & nodeName); // If current element contains the given element
    QStringList childKeys(int count = -1) const;  // Return all the child elements
    QString firstChildKey() const; // Returns the first child element
    QString toString(); // String equivalent of the node or document
    QDomDocumentFragment getFragment(); // Get document fragment
    bool HasError() {return m_bError;} // If XML parsing did not succeed (possibly non-XML)
private:
    bool AddList(QVariantList & list, QString nodeName); // Add list iteratively
private:
    QList<QDomElement> m_nodes; // a list of the elements currently traversed in the tree
    QList<int> m_levels;        // a list of the levels currently traversed in the tree
    QDomDocument m_doc; // xml document
    QString m_fname; // XML filename 
    bool m_bError; // If File is empty
};


#endif // include guard
