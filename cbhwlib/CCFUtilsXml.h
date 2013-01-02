/* =STS=> CCFUtilsXml.h[4904].aa02   open     SMID:3 */
//////////////////////////////////////////////////////////////////////
//
// (c) Copyright 2012 - 2013 Blackrock Microsystems
//
// $Workfile: CCFUtilsXml.h $
// $Archive: /Cerebus/Human/WindowsApps/cbhwlib/CCFUtilsXml.h $
// $Revision: 1 $
// $Date: 4/10/12 1:40p $
// $Author: Ehsan $
//
// $NoKeywords: $
//
//////////////////////////////////////////////////////////////////////
//
// Purpose: Read anything after 3.9 binary CCF file
//

#ifndef CCFUTILSXML_H_INCLUDED
#define CCFUTILSXML_H_INCLUDED

#include "CCFUtilsBinary.h"

// Define the latest version to use for writing
#define ccfXml CCFUtilsXml_v1
namespace ccf
{
// define data structure for the latest version
//  Do not increase the version unless new version can not make sense of the old version
typedef cbCCF ccfXmlData_v1;

};      // namespace ccf

//------------------------------------------
// XML CCF Version v1
class CCFUtilsXml_v1 : public CCFUtilsBinary
{
public:
    CCFUtilsXml_v1();

public:
    // Purpose: load the channel configuration from the file
    ccf::ccfResult ReadCCF(LPCSTR szFileName, bool bConvert);
    ccf::ccfResult ReadVersion(LPCSTR szFileName); // Read the version alone

protected:
    // Convert from old config (generic)
    virtual CCFUtils * Convert(CCFUtils * pOldConfig);
    // Write to file as XML v1
    virtual ccf::ccfResult WriteCCFNoPrompt(LPCSTR szFileName);

private:
    // Convert from old data format (specific)
    void Convert(ccf::ccfBinaryData & data);

public:
    // We give it public access to make it easy
    ccf::ccfXmlData_v1 m_data; // Internal structure holding actual config parameters
};

#endif // include guard
