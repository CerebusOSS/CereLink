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

// Currently in Cerebus we use Qt only for XML handling.
//  inclusion of these header files should not spread throughout the project or the solution
#include "XmlItem.h"

//---------------------------------
class CCFXmlItem : public XmlItem
{
public:
    CCFXmlItem() {;}
    CCFXmlItem(cbPKT_CHANINFO & pkt, QString strName = "");
    CCFXmlItem(cbPKT_ADAPTFILTINFO & pkt, QString strName = "");
    CCFXmlItem(cbPKT_SS_DETECT & pkt, QString strName = "");
    CCFXmlItem(cbPKT_SS_ARTIF_REJECT & pkt, QString strName = "");
    CCFXmlItem(cbPKT_SS_NOISE_BOUNDARY & pkt, QString strName = "");
    CCFXmlItem(cbPKT_SS_STATISTICS & pkt, QString strName = "");
    CCFXmlItem(cbPKT_SS_STATUS & pkt, QString strName = "");
    CCFXmlItem(cbPKT_SYSINFO & pkt, QString strName = "");
    CCFXmlItem(cbPKT_NTRODEINFO & pkt, QString strName = "");
    CCFXmlItem(cbPKT_LNC & pkt, QString strName = "");
    CCFXmlItem(cbPKT_FILTINFO & pkt, QString strName = "");
    CCFXmlItem(cbPKT_AOUT_WAVEFORM & pkt, QString strName = "");
    CCFXmlItem(cbSCALING & pkt, QString strName = "");
    CCFXmlItem(cbFILTDESC & pkt, QString strName = "");
    CCFXmlItem(cbMANUALUNITMAPPING & pkt, QString strName = "");
    CCFXmlItem(cbHOOP & pkt, QString strName = "");
    CCFXmlItem(cbAdaptControl & pkt, QString strName = "");
    CCFXmlItem(cbWaveformData & pkt, uint16_t  mode, QString strName = "");

    CCFXmlItem(uint32_t & number, QString strName = "");
    CCFXmlItem(int32_t & number, QString strName = "");
    CCFXmlItem(uint16_t & number, QString strName = "");
    CCFXmlItem(int16_t & number, QString strName = "");
    CCFXmlItem(uint8_t & number, QString strName = "");
    CCFXmlItem(int8_t & number, QString strName = "");
    CCFXmlItem(float & number, QString strName = "");
    CCFXmlItem(double & number, QString strName = "");

    // Array constructor
    CCFXmlItem(QVariantList lst, QString strName);
    // We treat character array as string
    CCFXmlItem(char cstr[], int count, QString strName);

public:
    operator const QVariant() const;
};
Q_DECLARE_METATYPE(CCFXmlItem)


#endif // include guard

