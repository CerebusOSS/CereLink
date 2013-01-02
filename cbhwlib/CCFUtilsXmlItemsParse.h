/* =STS=> CCFUtilsXmlItemsParse.h[4881].aa02   open     SMID:2 */
//////////////////////////////////////////////////////////////////////
//
// (c) Copyright 2012 - 2013 Blackrock Microsystems
//
// $Workfile: CCFUtilsXmlItemsParse.h $
// $Archive: /Cerebus/Human/WindowsApps/cbhwlib/CCFUtilsXmlItemsParse.h $
// $Revision: 1 $
// $Date: 4/16/12 10:49a $
// $Author: Ehsan $
//
// $NoKeywords: $
//
//////////////////////////////////////////////////////////////////////
//
// Purpose: Interface to parse different versions of CCF XML items
//

#ifndef CCFUTILSXMLITEMSPARSE_H_INCLUDED
#define CCFUTILSXMLITEMSPARSE_H_INCLUDED

#include "XmlFile.h"
#include "CCFUtilsXml.h"

// Namespace for Cerebus Config Files
namespace ccf
{
    //-------------------------------------------------
    // Abstract interface for reading CCFUtilsXmlItems
    //-------------------------------------------------
    template <typename T>
    void ReadItem(XmlFile * const xml, T item[], int count);
    template<>
    void ReadItem(XmlFile * const xml, char item[], int count);
    template <typename T>
    void ReadItem(XmlFile * const xml, T item[], int count, QString strName);
    template <typename T>
    void ReadItem(XmlFile * const xml, T item[], int count1, int count2, QString strName);
    template <typename T>
    void ReadItem(XmlFile * const xml, T & item, QString strName);
    // Enumerate items that can be counted
    template <typename T>
    int ItemNumber(XmlFile * const xml, T & item, QString strName);
    template <typename T>
    int ItemNumber(XmlFile * const xml, T & item);
    template<>
    int ItemNumber(XmlFile * const xml, cbPKT_CHANINFO & item);
    template<>
    int ItemNumber(XmlFile * const xml, cbPKT_NTRODEINFO & item);
    template<>
    int ItemNumber(XmlFile * const xml, cbPKT_SS_NOISE_BOUNDARY & item);
    // This is for basic types
    template <typename T>
    void ReadItem(XmlFile * const xml, T & item);
    // All specialized types come after here
    template<>
    void ReadItem(XmlFile * const xml, cbPKT_CHANINFO & item);
    template<>
    void ReadItem(XmlFile * const xml, cbPKT_SYSINFO & item);
    template<>
    void ReadItem(XmlFile * const xml, cbPKT_NTRODEINFO & item);
    template<>
    void ReadItem(XmlFile * const xml, cbPKT_ADAPTFILTINFO & item);
    template<>
    void ReadItem(XmlFile * const xml, cbPKT_SS_STATISTICS & item);
    template<>
    void ReadItem(XmlFile * const xml, cbPKT_SS_NOISE_BOUNDARY & item);
    template<>
    void ReadItem(XmlFile * const xml, cbPKT_SS_DETECT & item);
    template<>
    void ReadItem(XmlFile * const xml, cbPKT_SS_ARTIF_REJECT & item);
    template<>
    void ReadItem(XmlFile * const xml, cbPKT_SS_STATUS & item);
    template<>
    void ReadItem(XmlFile * const xml, cbPKT_LNC & item);
    template<>
    void ReadItem(XmlFile * const xml, cbPKT_FILTINFO & item);
    template<>
    void ReadItem(XmlFile * const xml, cbPKT_AOUT_WAVEFORM & item);
    template<>
    void ReadItem(XmlFile * const xml, cbSCALING & item);
    template<>
    void ReadItem(XmlFile * const xml, cbFILTDESC & item);
    template<>
    void ReadItem(XmlFile * const xml, cbMANUALUNITMAPPING & item);
    template<>
    void ReadItem(XmlFile * const xml, cbHOOP & item);
    template<>
    void ReadItem(XmlFile * const xml, cbAdaptControl & item);
};      // namespace ccf

#endif // include guard
