/* =STS=> CCFUtilsXmlItems.h[4905].aa01   open     SMID:1 */
//////////////////////////////////////////////////////////////////////
//
// (c) Copyright 2012 - 2013 Blackrock Microsystems
//
// $Workfile: CCFUtilsXmlItems.h $
// $Archive: /Cerebus/Human/WindowsApps/cbhwlib/CCFUtilsXmlItems.h $
// $Revision: 1 $
// $Date: 4/13/12 3:47p $
// $Author: Ehsan $
//
// $NoKeywords: $
//
//////////////////////////////////////////////////////////////////////
//
// Purpose: Serialize latest CCF XML items (to be written)
//
// Note: Do not directly use this class, instead use interface:
//        CCFUtilsXmlItemsInterface
//
//

#ifndef CCFUTILSXMLITEMS_H_INCLUDED
#define CCFUTILSXMLITEMS_H_INCLUDED

#include "cbproto.h"

#include "XmlItem.h"
#include <vector>

//---------------------------------
class CCFXmlItem : public XmlItem
{
public:
    CCFXmlItem() {;}
    CCFXmlItem(cbPKT_CHANINFO & pkt, std::string strName = "");
    CCFXmlItem(cbPKT_ADAPTFILTINFO & pkt, std::string strName = "");
    CCFXmlItem(cbPKT_SS_DETECT & pkt, std::string strName = "");
    CCFXmlItem(cbPKT_SS_ARTIF_REJECT & pkt, std::string strName = "");
    CCFXmlItem(cbPKT_SS_NOISE_BOUNDARY & pkt, std::string strName = "");
    CCFXmlItem(cbPKT_SS_STATISTICS & pkt, std::string strName = "");
    CCFXmlItem(cbPKT_SS_STATUS & pkt, std::string strName = "");
    CCFXmlItem(cbPKT_SYSINFO & pkt, std::string strName = "");
    CCFXmlItem(cbPKT_NTRODEINFO & pkt, std::string strName = "");
    CCFXmlItem(cbPKT_LNC & pkt, std::string strName = "");
    CCFXmlItem(cbPKT_FILTINFO & pkt, std::string strName = "");
    CCFXmlItem(cbPKT_AOUT_WAVEFORM & pkt, std::string strName = "");
    CCFXmlItem(cbSCALING & pkt, std::string strName = "");
    CCFXmlItem(cbFILTDESC & pkt, std::string strName = "");
    CCFXmlItem(cbMANUALUNITMAPPING & pkt, std::string strName = "");
    CCFXmlItem(cbHOOP & pkt, std::string strName = "");
    CCFXmlItem(cbAdaptControl & pkt, std::string strName = "");
    CCFXmlItem(cbWaveformData & pkt, uint16_t  mode, std::string strName = "");

    CCFXmlItem(uint32_t & number, std::string strName = "");
    CCFXmlItem(int32_t & number, std::string strName = "");
    CCFXmlItem(uint16_t & number, std::string strName = "");
    CCFXmlItem(int16_t & number, std::string strName = "");
    CCFXmlItem(uint8_t & number, std::string strName = "");
    CCFXmlItem(int8_t & number, std::string strName = "");
    CCFXmlItem(float & number, std::string strName = "");
    CCFXmlItem(double & number, std::string strName = "");

    // Array constructor
    CCFXmlItem(std::vector<std::any> lst, std::string strName);
    // We treat character array as string
    CCFXmlItem(char cstr[], int count, std::string strName);
};


#endif // include guard

