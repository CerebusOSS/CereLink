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

#include <string>
#include <any>
#include <map>


// Author & Date: Ehsan Azar       4 April 2012
// Purpose: Any user data that can be stored in XML
class XmlItem
{
public:
    XmlItem() {;}
    virtual ~XmlItem() {;}
    XmlItem(const XmlItem & other);

public:
    virtual std::any XmlValue() const {return m_xmlValue;}
    virtual std::string XmlName() const {return m_xmlTag;}
    virtual std::map<std::string, std::any>  XmlAttribs() const {return m_xmlAttribs;}
    bool IsValid() const {return m_xmlValue.has_value();}
    std::string Attribute(std::string strKey) { return std::any_cast<std::string>(m_xmlAttribs[strKey]); }

protected:
    std::any m_xmlValue;
    std::string m_xmlTag;
    std::map<std::string, std::any> m_xmlAttribs;
};

#endif // include guard
