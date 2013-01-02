//////////////////////////////////////////////////////////////////////////////
//
// (c) Copyright 2011 - 2012 Blackrock Microsystems
//
// $Workfile: XmlItem.h $
// $Archive: /common/XmlItem.h $
// $Revision: 1 $
// $Date: 4/4/12 4:24p $
// $Author: Ehsan $
//
// $NoKeywords: $
//
//////////////////////////////////////////////////////////////////////////////
//
// PURPOSE:
//
// Common XML element 
//

#ifndef XMLITEM_H_INCLUDED
#define XMLITEM_H_INCLUDED

#include <QMetaType>
#include <QString>
#include <QMap>
#include <QVariant>

// Author & Date: Ehsan Azar       4 April 2012
// Purpose: Any user data that can be stored in XML
class XmlItem
{
public:
    XmlItem() {;}
    virtual ~XmlItem() {;}
    XmlItem(const XmlItem & other);

public:
    virtual QVariant XmlValue() const {return m_xmlValue;}
    virtual QString XmlName() const {return m_xmlTag;}
    virtual QMap<QString, QVariant>  XmlAttribs() const {return m_xmlAttribs;}
    bool IsValid() const {return m_xmlValue.isValid();}
    QString Attribute(QString strKey) { return m_xmlAttribs.value(strKey).toString(); }

protected:
    QVariant m_xmlValue;
    QString m_xmlTag;
    QMap<QString, QVariant> m_xmlAttribs;
};

#endif // include guard
