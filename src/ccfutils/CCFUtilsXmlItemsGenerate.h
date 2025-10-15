/* =STS=> CCFUtilsXmlItemsGenerate.h[4880].aa01   open     SMID:1 */
//////////////////////////////////////////////////////////////////////
//
// (c) Copyright 2012 - 2013 Blackrock Microsystems
//
// $Workfile: CCFUtilsXmlItemsGenerate.h $
// $Archive: /Cerebus/Human/WindowsApps/cbhwlib/CCFUtilsXmlItemsGenerate.h $
// $Revision: 1 $
// $Date: 4/16/12 10:49a $
// $Author: Ehsan $
//
// $NoKeywords: $
//
//////////////////////////////////////////////////////////////////////
//
// Purpose: Interface to generate final CCF XML items (to be written)
//

#ifndef CCFUTILSXMLITEMSGENERATE_H_INCLUDED
#define CCFUTILSXMLITEMSGENERATE_H_INCLUDED

#include <any>
#include <string>

// Namespace for Cerebus Config Files
namespace ccf
{
    //----------------------------------------------------
    // Abstract interface for generating CCFUtilsXmlItems
    //----------------------------------------------------
    template <typename T>
    std::any GetCCFXmlItem(T & pkt, std::string strName);
    template <typename T>
    std::any GetCCFXmlItem(T pkt[], int count, std::string strName);
    template <typename T>
    std::any GetCCFXmlItem(T ppkt[], int count1, int count2, std::string strName1, std::string strName2);
    template <>
    std::any GetCCFXmlItem<char>(char pkt[], int count, std::string strName);
};      // namespace ccf

#endif // include guard
