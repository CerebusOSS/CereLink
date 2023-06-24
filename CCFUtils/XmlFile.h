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

#include <map>
#include <vector>
#include <any>
#include "pugixml.hpp"


// Author & Date: Ehsan Azar       15 June 2010
// Purpose: XML handling
class XmlFile
{
public:
    XmlFile(std::string fname = "", bool bRead = true);
    bool setContent(); // set content from XML file
    bool setContent(const std::string & content);

public:
    bool beginGroup(const std::string & nodeName = "", const std::string & attrName = "",
            const std::any & attrValue = 0, const std::any & value = std::any()); // Begin a node
    bool beginGroup(std::string nodeName, const std::map<std::string, std::any> attribs, const std::any & value = std::any()); // Begin a node
    bool endGroup(bool bSave = true); // End last node
    bool addGroup(const std::string & nodeName = "", const std::string & attrName = "",
            const std::any & attrValue = 0, const std::any & value = std::any()); // Begin and end a node
    std::string attribute(const std::string & attrName); // Get attribute value
    std::any value(const std::any & val = std::any()); // value of the current element
    bool contains (const std::string & nodeName); // If current element contains the given element
    [[nodiscard]] std::vector<std::string> childKeys(int count = -1) const;  // Return all the child elements
    [[nodiscard]] std::string firstChildKey() const; // Returns the first child element
    std::string toString(); // String equivalent of the node or document
    pugi::xml_document getFragment(); // Get document fragment
    bool HasError() {return m_bError;} // If XML parsing did not succeed (possibly non-XML)
private:
    bool AddList(std::vector<std::any> & list, std::string nodeName); // Add list iteratively
private:
    std::vector<pugi::xml_node> m_nodes; // a list of the elements currently traversed in the tree
    std::vector<int> m_levels;        // a list of the levels currently traversed in the tree
    pugi::xml_document m_doc;  // xml document
    std::string m_fname; // XML filename
    bool m_bError; // If File is empty
};


#endif // include guard
