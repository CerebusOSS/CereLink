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

#include <QVariant>

// Namespace for Cerebus Config Files
namespace ccf
{
    //----------------------------------------------------
    // Abstract interface for generating CCFUtilsXmlItems
    //----------------------------------------------------
    template <typename T>
    QVariant GetCCFXmlItem(T & pkt, QString strName);
    template <typename T>
    QVariant GetCCFXmlItem(T pkt[], int count, QString strName);
    template <typename T>
    QVariant GetCCFXmlItem(T ppkt[], int count1, int count2, QString strName1, QString strName2);
    template <>
    QVariant GetCCFXmlItem<char>(char pkt[], int count, QString strName);
};      // namespace ccf

#endif // include guard
