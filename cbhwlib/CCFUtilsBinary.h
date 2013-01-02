/* =STS=> CCFUtilsBinary.h[4875].aa02   open     SMID:2 */
//////////////////////////////////////////////////////////////////////
//
// (c) Copyright 2012 - 2013 Blackrock Microsystems
//
// $Workfile: CCFUtilsBinary.h $
// $Archive: /Cerebus/Human/WindowsApps/cbhwlib/CCFUtilsBinary.h $
// $Revision: 1 $
// $Date: 4/10/12 1:40p $
// $Author: Ehsan $
//
// $NoKeywords: $
//
//////////////////////////////////////////////////////////////////////
//
// Purpose: Read anything up to 3.9 binary CCF file
//

#ifndef CCFUTILSBINARY_H_INCLUDED
#define CCFUTILSBINARY_H_INCLUDED

#include "CCFUtils.h"
#include <stdio.h>

#define cbLEGACY_MAXCHANS  160
#define cbLEGACY_NUM_ANALOG_CHANS    144
#define cbLEGACY_MAXUNITS  5
#define cbLEGACY_MAXHOOPS  4
#define cbLEGACY_MAXNTRODES (cbLEGACY_NUM_ANALOG_CHANS / 2)     // minimum is stereotrode so max n-trodes is max chans / 2
#define cbLEGACY_MAXSITES  4     // maximum number of electrodes that can be included in an n-trode group
#define cbLEGACY_MAXSITEPLOTS ((cbLEGACY_MAXSITES - 1) * cbLEGACY_MAXSITES / 2)  // combination of 2 out of n is n!/((n-2)!2!)
///////////////////////////////////////////////////////////////////////////////////////////////////
//  Some of the string length constants
#define cbLEGACY_LEN_STR_UNIT          8
#define cbLEGACY_LEN_STR_LABEL         16

// Namespace for Cerebus Config Files
namespace ccf
{

enum {CCFUTILSBINARY_LASTVERSION = 11};

// Generic Cerebus packet data structure (1024 bytes total)
typedef struct {
    UINT32 time;        // system clock timestamp
    UINT16 chid;        // channel identifier
    UINT8  type;        // packet type
    UINT8  dlen;        // length of data field in 32-bit chunks
    UINT32 data[254];   // data buffer (up to 1016 bytes)
} cbPKT_GENERIC_CB2003_10;

typedef struct {
    INT16   digmin;     // digital value that cooresponds with the anamin value
    INT16   digmax;     // digital value that cooresponds with the anamax value
    INT32   anamin;     // the minimum analog value present in the signal
    INT32   anamax;     // the maximum analog value present in the signal
    char    anaunit[cbLEGACY_LEN_STR_UNIT]; // the unit for the analog signal (eg, "uV" or "MPa")
} cbSCALING_NO_ANAGAIN;

typedef struct {
    INT16   digmin;     // digital value that cooresponds with the anamin value
    INT16   digmax;     // digital value that cooresponds with the anamax value
    INT32   anamin;     // the minimum analog value present in the signal
    INT32   anamax;     // the maximum analog value present in the signal
    INT32   anagain;    // the gain applied to the default analog values to get the analog values
    char    anaunit[cbLEGACY_LEN_STR_UNIT]; // the unit for the analog signal (eg, "uV" or "MPa")
} cbSCALING_30;

typedef struct {
    UINT16      nOverride;
    INT16       xOrigin;
    INT16       yOrigin;
    UINT16      radius;
} cbMANUALUNITMAPPING_31;

typedef struct {
    UINT32      nOverride;
    float       afOrigin[2];
    float       afShape[2][2];
    float       aPhi;
    UINT32      bValid; // is this unit in use at this time?
                        // BOOL implemented as UINT2 - for structure alignment at paragraph boundary
} cbMANUALUNITMAPPING_34;

typedef struct {
    UINT32      nOverride;
    float       afOrigin[3];
    float       afShape[3][3];
    float       aPhi;
    UINT32      bValid; // is this unit in use at this time?
                        // BOOL implemented as UINT2 - for structure alignment at paragraph boundary
} cbMANUALUNITMAPPING_36;

typedef struct {
    INT16       nOverride;
    INT16       afOrigin[3];
    INT16       afShape[3][3];
    INT16       aPhi;
    UINT32      bValid; // is this unit in use at this time?
                        // BOOL implemented as UINT32 - for structure alignment at paragraph boundary
} cbMANUALUNITMAPPING_37;

typedef struct {
    char    label[cbLEGACY_LEN_STR_LABEL];
    UINT32  hpfreq;     // high-pass corner frequency in milliHertz
    UINT32  hporder;    // high-pass filter order
    UINT32  hptype;     // high-pass filter type
    UINT32  lpfreq;     // low-pass frequency in milliHertz
    UINT32  lporder;    // low-pass filter order
    UINT32  lptype;     // low-pass filter type
} cbFILTDESC_10;

typedef struct {
    UINT16 valid; // 0=undefined, 1 for valid
    INT16  time;  // time offset into spike window
    INT16  min;   // minimum value for the hoop window
    INT16  max;   // maximum value for the hoop window
} cbHOOP_10;

typedef struct {
    UINT32     time;        // system clock timestamp
    UINT16     chid;        // 0x8000
    UINT8      type;        // cbPKTTYPE_AINP*
    UINT8      dlen;        // cbPKT_DLENCHANINFO
    UINT32     chan;	    // actual channel id of the channel being configured
    UINT32     proc;        // the address of the processor on which the channel resides
    UINT32     bank;        // the address of the bank on which the channel resides
    UINT32     term;        // the terminal number of the channel within it's bank
    UINT32     chancaps;    // general channel capablities (given by cbCHAN_* flags)
    UINT32     doutcaps;    // digital output capablities (composed of cbDOUT_* flags)
    UINT32     dinpcaps;    // digital input capablities (composed of cbDINP_* flags)
    UINT32     aoutcaps;    // analog input capablities (composed of cbAOUT_* flags)
    UINT32     ainpcaps;    // analog input capablities (composed of cbAINP_* flags)
    UINT32     spkcaps;     // spike processing capabilities
    cbSCALING_NO_ANAGAIN  physcalin;   // physical channel scaling information
    cbFILTDESC_10 phyfiltin;   // physical channel filter definition
    cbSCALING_NO_ANAGAIN  physcalout;  // physical channel scaling information
    cbFILTDESC_10 phyfiltout;  // physical channel filter definition
    char       label[cbLEGACY_LEN_STR_LABEL];   // Label of the channel (null terminated if <16 characters)
    UINT32     userflags;   // User flags for the channel state
    INT32      position[4]; // reserved for future position information
    cbSCALING_NO_ANAGAIN  scalin;      // user-defined scaling information
    cbSCALING_NO_ANAGAIN  scalout;     // user-defined scaling information
    UINT32     doutopts;    // digital output options (composed of cbDOUT_* flags)
    UINT32     dinpopts;    // digital input options (composed of cbDINP_* flags)
    UINT32     aoutopts;    // analog output options
    UINT32     eopchar;     // digital input capablities (given by cbDINP_* flags)
    UINT32     monsource;   // address of channel to monitor
    INT32      outvalue;    // output value
    UINT32     lncmode;     // line noise cancellation filter mode
    UINT32     lncrate;     // line noise cancellation filter adaptation rate
    UINT32     smpfilter;   // continuous-time pathway filter id
    UINT32     smpgroup;    // continuous-time pathway sample group
    INT32      smpdispmin;  // continuous-time pathway display factor
    INT32      smpdispmax;  // continuous-time pathway display factor
    UINT32     spkfilter;   // spike pathway filter id
    INT32      spkdispmax;  // spike pathway display factor
    UINT32     spkopts;     // spike processing options
    INT32      spkthrlevel; // spike threshold level
    INT32      spkthrlimit; //
    UINT32     spkgroup;    // placeholder for future multi-trode processing group
    cbHOOP_10     spkhoops[cbLEGACY_MAXUNITS][cbLEGACY_MAXHOOPS];    // spike hoop sorting set
} cbPKT_CHANINFO_CB2003_10A;

typedef struct {
    UINT32     time;        // system clock timestamp
    UINT16     chid;        // 0x8000
    UINT8      type;        // cbPKTTYPE_AINP*
    UINT8      dlen;        // cbPKT_DLENCHANINFO
    UINT32     chan;	    // actual channel id of the channel being configured
    UINT32     proc;        // the address of the processor on which the channel resides
    UINT32     bank;        // the address of the bank on which the channel resides
    UINT32     term;        // the terminal number of the channel within it's bank
    UINT32     chancaps;    // general channel capablities (given by cbCHAN_* flags)
    UINT32     doutcaps;    // digital output capablities (composed of cbDOUT_* flags)
    UINT32     dinpcaps;    // digital input capablities (composed of cbDINP_* flags)
    UINT32     aoutcaps;    // analog input capablities (composed of cbAOUT_* flags)
    UINT32     ainpcaps;    // analog input capablities (composed of cbAINP_* flags)
    UINT32     spkcaps;     // spike processing capabilities
    cbSCALING_NO_ANAGAIN  physcalin;   // physical channel scaling information
    cbFILTDESC_10 phyfiltin;   // physical channel filter definition
    cbSCALING_NO_ANAGAIN  physcalout;  // physical channel scaling information
    cbFILTDESC_10 phyfiltout;  // physical channel filter definition
    char       label[cbLEGACY_LEN_STR_LABEL];   // Label of the channel (null terminated if <16 characters)
    UINT32     userflags;   // User flags for the channel state
    INT32      position[4]; // reserved for future position information
    cbSCALING_NO_ANAGAIN  scalin;      // user-defined scaling information
    cbSCALING_NO_ANAGAIN  scalout;     // user-defined scaling information
    UINT32     doutopts;    // digital output options (composed of cbDOUT_* flags)
    UINT32     dinpopts;    // digital input options (composed of cbDINP_* flags)
    UINT32     aoutopts;    // analog output options
    UINT32     eopchar;     // digital input capablities (given by cbDINP_* flags)
    UINT32     monsource;   // address of channel to monitor
    INT32      outvalue;    // output value
    UINT32     lncmode;     // line noise cancellation filter mode
    UINT32     lncrate;     // line noise cancellation filter adaptation rate
    UINT32     smpfilter;   // continuous-time pathway filter id
    UINT32     smpgroup;    // continuous-time pathway sample group
    INT32      smpdispmin;  // continuous-time pathway display factor
    INT32      smpdispmax;  // continuous-time pathway display factor
    UINT32     spkfilter;   // spike pathway filter id
    INT32      spkdispmax;  // spike pathway display factor
    UINT32     spkopts;     // spike processing options
    INT32      spkthrlevel; // spike threshold level
    INT32      spkthrlimit; //
    cbHOOP_10     spkhoops[cbLEGACY_MAXUNITS][cbLEGACY_MAXHOOPS];    // spike hoop sorting set
} cbPKT_CHANINFO_CB2005_25;

typedef struct {
    UINT32          time;        // system clock timestamp
    UINT16          chid;        // 0x8000
    UINT8           type;        // cbPKTTYPE_AINP*
    UINT8           dlen;        // cbPKT_DLENCHANINFO
    UINT32          chan;	    // actual channel id of the channel being configured
    UINT32          proc;        // the address of the processor on which the channel resides
    UINT32          bank;        // the address of the bank on which the channel resides
    UINT32          term;        // the terminal number of the channel within it's bank
    UINT32          chancaps;    // general channel capablities (given by cbCHAN_* flags)
    UINT32          doutcaps;    // digital output capablities (composed of cbDOUT_* flags)
    UINT32          dinpcaps;    // digital input capablities (composed of cbDINP_* flags)
    UINT32          aoutcaps;    // analog input capablities (composed of cbAOUT_* flags)
    UINT32          ainpcaps;    // analog input capablities (composed of cbAINP_* flags)
    UINT32          spkcaps;     // spike processing capabilities
    cbSCALING_30       physcalin;   // physical channel scaling information
    cbFILTDESC_10      phyfiltin;   // physical channel filter definition
    cbSCALING_30       physcalout;  // physical channel scaling information
    cbFILTDESC_10      phyfiltout;  // physical channel filter definition
    char            label[cbLEGACY_LEN_STR_LABEL];   // Label of the channel (null terminated if <16 characters)
    UINT32          userflags;   // User flags for the channel state
    INT32           position[4]; // reserved for future position information
    cbSCALING_30       scalin;      // user-defined scaling information
    cbSCALING_30       scalout;     // user-defined scaling information
    UINT32          doutopts;    // digital output options (composed of cbDOUT_* flags)
    UINT32          dinpopts;    // digital input options (composed of cbDINP_* flags)
    UINT32          aoutopts;    // analog output options
    UINT32          eopchar;     // digital input capablities (given by cbDINP_* flags)
    UINT32          monsource;   // address of channel to monitor
    INT32           outvalue;    // output value
    UINT32          lncmode;     // line noise cancellation filter mode
    UINT32          lncrate;     // line noise cancellation filter adaptation rate
    UINT32          smpfilter;   // continuous-time pathway filter id
    UINT32          smpgroup;    // continuous-time pathway sample group
    INT32           smpdispmin;  // continuous-time pathway display factor
    INT32           smpdispmax;  // continuous-time pathway display factor
    UINT32          spkfilter;   // spike pathway filter id
    INT32           spkdispmax;  // spike pathway display factor
    UINT32          spkopts;     // spike processing options
    INT32           spkthrlevel; // spike threshold level
    INT32           spkthrlimit; //
    UINT32          spkgroup;    // placeholder for future multi-trode processing group
    cbHOOP_10          spkhoops[cbLEGACY_MAXUNITS][cbLEGACY_MAXHOOPS];    // spike hoop sorting set
} cbPKT_CHANINFO_CB2005_30;

typedef struct {
    UINT32              time;        // system clock timestamp
    UINT16              chid;        // 0x8000
    UINT8               type;        // cbPKTTYPE_AINP*
    UINT8               dlen;        // cbPKT_DLENCHANINFO
    UINT32              chan;	    // actual channel id of the channel being configured
    UINT32              proc;        // the address of the processor on which the channel resides
    UINT32              bank;        // the address of the bank on which the channel resides
    UINT32              term;        // the terminal number of the channel within it's bank
    UINT32              chancaps;    // general channel capablities (given by cbCHAN_* flags)
    UINT32              doutcaps;    // digital output capablities (composed of cbDOUT_* flags)
    UINT32              dinpcaps;    // digital input capablities (composed of cbDINP_* flags)
    UINT32              aoutcaps;    // analog input capablities (composed of cbAOUT_* flags)
    UINT32              ainpcaps;    // analog input capablities (composed of cbAINP_* flags)
    UINT32              spkcaps;     // spike processing capabilities
    cbSCALING_30           physcalin;   // physical channel scaling information
    cbFILTDESC_10          phyfiltin;   // physical channel filter definition
    cbSCALING_30           physcalout;  // physical channel scaling information
    cbFILTDESC_10          phyfiltout;  // physical channel filter definition
    char                label[cbLEGACY_LEN_STR_LABEL];   // Label of the channel (null terminated if <16 characters)
    UINT32              userflags;   // User flags for the channel state
    INT32               position[4]; // reserved for future position information
    cbSCALING_30           scalin;      // user-defined scaling information
    cbSCALING_30           scalout;     // user-defined scaling information
    UINT32              doutopts;    // digital output options (composed of cbDOUT_* flags)
    UINT32              dinpopts;    // digital input options (composed of cbDINP_* flags)
    UINT32              aoutopts;    // analog output options
    UINT32              eopchar;     // digital input capablities (given by cbDINP_* flags)
    UINT32              monsource;   // address of channel to monitor
    INT32               outvalue;    // output value
    UINT32              lncmode;     // line noise cancellation filter mode
    UINT32              lncrate;     // line noise cancellation filter adaptation rate
    UINT32              smpfilter;   // continuous-time pathway filter id
    UINT32              smpgroup;    // continuous-time pathway sample group
    INT32               smpdispmin;  // continuous-time pathway display factor
    INT32               smpdispmax;  // continuous-time pathway display factor
    UINT32              spkfilter;   // spike pathway filter id
    INT32               spkdispmax;  // spike pathway display factor
    UINT32              spkopts;     // spike processing options
    INT32               spkthrlevel; // spike threshold level
    INT32               spkthrlimit; //
    UINT32              spkgroup;    // placeholder for future multi-trode processing group
    cbMANUALUNITMAPPING_31 unitmapping[cbLEGACY_MAXUNITS];            // manual unit mapping
    cbHOOP_10              spkhoops[cbLEGACY_MAXUNITS][cbLEGACY_MAXHOOPS];   // spike hoop sorting set
} cbPKT_CHANINFO_CB2005_31;

typedef struct {
    UINT32              time;        // system clock timestamp
    UINT16              chid;        // 0x8000
    UINT8               type;        // cbPKTTYPE_AINP*
    UINT8               dlen;        // cbPKT_DLENCHANINFO
    UINT32              chan;	    // actual channel id of the channel being configured
    UINT32              proc;        // the address of the processor on which the channel resides
    UINT32              bank;        // the address of the bank on which the channel resides
    UINT32              term;        // the terminal number of the channel within it's bank
    UINT32              chancaps;    // general channel capablities (given by cbCHAN_* flags)
    UINT32              doutcaps;    // digital output capablities (composed of cbDOUT_* flags)
    UINT32              dinpcaps;    // digital input capablities (composed of cbDINP_* flags)
    UINT32              aoutcaps;    // analog input capablities (composed of cbAOUT_* flags)
    UINT32              ainpcaps;    // analog input capablities (composed of cbAINP_* flags)
    UINT32              spkcaps;     // spike processing capabilities
    cbSCALING_30           physcalin;   // physical channel scaling information
    cbFILTDESC_10          phyfiltin;   // physical channel filter definition
    cbSCALING_30           physcalout;  // physical channel scaling information
    cbFILTDESC_10          phyfiltout;  // physical channel filter definition
    char                label[cbLEGACY_LEN_STR_LABEL];   // Label of the channel (null terminated if <16 characters)
    UINT32              userflags;   // User flags for the channel state
    INT32               position[4]; // reserved for future position information
    cbSCALING_30           scalin;      // user-defined scaling information
    cbSCALING_30           scalout;     // user-defined scaling information
    UINT32              doutopts;    // digital output options (composed of cbDOUT_* flags)
    UINT32              dinpopts;    // digital input options (composed of cbDINP_* flags)
    UINT32              aoutopts;    // analog output options
    UINT32              eopchar;     // digital input capablities (given by cbDINP_* flags)
    UINT32              monsource;   // address of channel to monitor
    INT32               outvalue;    // output value
    UINT32              lncmode;     // line noise cancellation filter mode
    UINT32              lncrate;     // line noise cancellation filter adaptation rate
    UINT32              smpfilter;   // continuous-time pathway filter id
    UINT32              smpgroup;    // continuous-time pathway sample group
    INT32               smpdispmin;  // continuous-time pathway display factor
    INT32               smpdispmax;  // continuous-time pathway display factor
    UINT32              spkfilter;   // spike pathway filter id
    INT32               spkdispmax;  // spike pathway display factor
    UINT32              spkopts;     // spike processing options
    INT32               spkthrlevel; // spike threshold level
    INT32               spkthrlimit; //
    UINT32              spkgroup;    // placeholder for future multi-trode processing group
    cbMANUALUNITMAPPING_34 unitmapping[cbLEGACY_MAXUNITS];            // manual unit mapping
    cbHOOP_10              spkhoops[cbLEGACY_MAXUNITS][cbLEGACY_MAXHOOPS];   // spike hoop sorting set
} cbPKT_CHANINFO_CB2005_34;

typedef struct {
    UINT32              time;           // system clock timestamp
    UINT16              chid;           // 0x8000
    UINT8               type;           // cbPKTTYPE_AINP*
    UINT8               dlen;           // cbPKT_DLENCHANINFO
    UINT32              chan;	        // actual channel id of the channel being configured
    UINT32              proc;           // the address of the processor on which the channel resides
    UINT32              bank;           // the address of the bank on which the channel resides
    UINT32              term;           // the terminal number of the channel within it's bank
    UINT32              chancaps;       // general channel capablities (given by cbCHAN_* flags)
    UINT32              doutcaps;       // digital output capablities (composed of cbDOUT_* flags)
    UINT32              dinpcaps;       // digital input capablities (composed of cbDINP_* flags)
    UINT32              aoutcaps;       // analog output capablities (composed of cbAOUT_* flags)
    UINT32              ainpcaps;       // analog input capablities (composed of cbAINP_* flags)
    UINT32              spkcaps;        // spike processing capabilities
    cbSCALING_30           physcalin;      // physical channel scaling information
    cbFILTDESC_10          phyfiltin;      // physical channel filter definition
    cbSCALING_30           physcalout;     // physical channel scaling information
    cbFILTDESC_10          phyfiltout;     // physical channel filter definition
    char                label[cbLEGACY_LEN_STR_LABEL];   // Label of the channel (null terminated if <16 characters)
    UINT32              userflags;      // User flags for the channel state
    INT32               position[4];    // reserved for future position information
    cbSCALING_30           scalin;         // user-defined scaling information
    cbSCALING_30           scalout;        // user-defined scaling information
    UINT32              doutopts;       // digital output options (composed of cbDOUT_* flags)
    UINT32              dinpopts;       // digital input options (composed of cbDINP_* flags)
    UINT32              aoutopts;       // analog output options
    UINT32              eopchar;        // digital input capablities (given by cbDINP_* flags)
    union {
        struct {
            UINT32              monsource;      // address of channel to monitor
            INT32               outvalue;       // output value
        };
        struct {
            UINT16              lowsamples;     // address of channel to monitor
            UINT16              highsamples;    // address of channel to monitor
            INT32               offset;         // output value
        };
    };
    UINT32              lncmode;        // line noise cancellation filter mode
    UINT32              lncrate;        // line noise cancellation filter adaptation rate
    UINT32              smpfilter;      // continuous-time pathway filter id
    UINT32              smpgroup;       // continuous-time pathway sample group
    INT32               smpdispmin;     // continuous-time pathway display factor
    INT32               smpdispmax;     // continuous-time pathway display factor
    UINT32              spkfilter;      // spike pathway filter id
    INT32               spkdispmax;     // spike pathway display factor
    UINT32              spkopts;        // spike processing options
    INT32               spkthrlevel;    // spike threshold level
    INT32               spkthrlimit;    //
    UINT32              spkgroup;       // NTrodeGroup this electrode belongs to - 0 is single unit, non-0 indicates a multi-trode grouping
    INT16               amplrejpos;     // Amplitude rejection positive value
    INT16               amplrejneg;     // Amplitude rejection negative value
    cbMANUALUNITMAPPING_34 unitmapping[cbLEGACY_MAXUNITS];            // manual unit mapping
    cbHOOP_10              spkhoops[cbLEGACY_MAXUNITS][cbLEGACY_MAXHOOPS];   // spike hoop sorting set
} cbPKT_CHANINFO_CB2005_35;

typedef struct {
    UINT32              time;           // system clock timestamp
    UINT16              chid;           // 0x8000
    UINT8               type;           // cbPKTTYPE_AINP*
    UINT8               dlen;           // cbPKT_DLENCHANINFO
    UINT32              chan;	        // actual channel id of the channel being configured
    UINT32              proc;           // the address of the processor on which the channel resides
    UINT32              bank;           // the address of the bank on which the channel resides
    UINT32              term;           // the terminal number of the channel within it's bank
    UINT32              chancaps;       // general channel capablities (given by cbCHAN_* flags)
    UINT32              doutcaps;       // digital output capablities (composed of cbDOUT_* flags)
    UINT32              dinpcaps;       // digital input capablities (composed of cbDINP_* flags)
    UINT32              aoutcaps;       // analog output capablities (composed of cbAOUT_* flags)
    UINT32              ainpcaps;       // analog input capablities (composed of cbAINP_* flags)
    UINT32              spkcaps;        // spike processing capabilities
    cbSCALING_30           physcalin;      // physical channel scaling information
    cbFILTDESC_10          phyfiltin;      // physical channel filter definition
    cbSCALING_30           physcalout;     // physical channel scaling information
    cbFILTDESC_10          phyfiltout;     // physical channel filter definition
    char                label[cbLEGACY_LEN_STR_LABEL];   // Label of the channel (null terminated if <16 characters)
    UINT32              userflags;      // User flags for the channel state
    INT32               position[4];    // reserved for future position information
    cbSCALING_30           scalin;         // user-defined scaling information for AINP
    cbSCALING_30           scalout;        // user-defined scaling information for AOUT
    UINT32              doutopts;       // digital output options (composed of cbDOUT_* flags)
    UINT32              dinpopts;       // digital input options (composed of cbDINP_* flags)
    UINT32              aoutopts;       // analog output options
    UINT32              eopchar;        // digital input capablities (given by cbDINP_* flags)
    union {
        struct {
            UINT32              monsource;      // address of channel to monitor
            INT32               outvalue;       // output value
        };
        struct {
            UINT16              lowsamples;     // address of channel to monitor
            UINT16              highsamples;    // address of channel to monitor
            INT32               offset;         // output value
        };
    };
    UINT32              ainpopts;       // analog input options (composed of cbAINP* flags)
    UINT32              lncrate;          // line noise cancellation filter adaptation rate
    UINT32              smpfilter;        // continuous-time pathway filter id
    UINT32              smpgroup;         // continuous-time pathway sample group
    INT32               smpdispmin;       // continuous-time pathway display factor
    INT32               smpdispmax;       // continuous-time pathway display factor
    UINT32              spkfilter;        // spike pathway filter id
    INT32               spkdispmax;       // spike pathway display factor
    INT32               lncdispmax;       // Line Noise pathway display factor
    UINT32              spkopts;          // spike processing options
    INT32               spkthrlevel;      // spike threshold level
    INT32               spkthrlimit;      //
    UINT32              spkgroup;         // NTrodeGroup this electrode belongs to - 0 is single unit, non-0 indicates a multi-trode grouping
    INT16               amplrejpos;       // Amplitude rejection positive value
    INT16               amplrejneg;       // Amplitude rejection negative value
    UINT32              refelecchan;      // Software reference electrode channel
    cbMANUALUNITMAPPING_36 unitmapping[cbLEGACY_MAXUNITS];            // manual unit mapping
    cbHOOP_10              spkhoops[cbLEGACY_MAXUNITS][cbLEGACY_MAXHOOPS];   // spike hoop sorting set
} cbPKT_CHANINFO_CB2005_36;

typedef struct {
    UINT32              time;           // system clock timestamp
    UINT16              chid;           // 0x8000
    UINT8               type;           // cbPKTTYPE_AINP*
    UINT8               dlen;           // cbPKT_DLENCHANINFO
    UINT32              chan;	        // actual channel id of the channel being configured
    UINT32              proc;           // the address of the processor on which the channel resides
    UINT32              bank;           // the address of the bank on which the channel resides
    UINT32              term;           // the terminal number of the channel within it's bank
    UINT32              chancaps;       // general channel capablities (given by cbCHAN_* flags)
    UINT32              doutcaps;       // digital output capablities (composed of cbDOUT_* flags)
    UINT32              dinpcaps;       // digital input capablities (composed of cbDINP_* flags)
    UINT32              aoutcaps;       // analog output capablities (composed of cbAOUT_* flags)
    UINT32              ainpcaps;       // analog input capablities (composed of cbAINP_* flags)
    UINT32              spkcaps;        // spike processing capabilities
    cbSCALING_30        physcalin;      // physical channel scaling information
    cbFILTDESC_10       phyfiltin;      // physical channel filter definition
    cbSCALING_30        physcalout;     // physical channel scaling information
    cbFILTDESC_10       phyfiltout;     // physical channel filter definition
    char                label[cbLEGACY_LEN_STR_LABEL];   // Label of the channel (null terminated if <16 characters)
    UINT32              userflags;      // User flags for the channel state
    INT32               position[4];    // reserved for future position information
    cbSCALING_30        scalin;         // user-defined scaling information for AINP
    cbSCALING_30        scalout;        // user-defined scaling information for AOUT
    UINT32              doutopts;       // digital output options (composed of cbDOUT_* flags)
    UINT32              dinpopts;       // digital input options (composed of cbDINP_* flags)
    UINT32              aoutopts;       // analog output options
    UINT32              eopchar;        // digital input capablities (given by cbDINP_* flags)
    union {
        struct {
            UINT32              monsource;      // address of channel to monitor
            INT32               outvalue;       // output value
        };
        struct {
            UINT16              lowsamples;     // address of channel to monitor
            UINT16              highsamples;    // address of channel to monitor
            INT32               offset;         // output value
        };
    };
    UINT32              ainpopts;       // analog input options (composed of cbAINP* flags)
    UINT32              lncrate;          // line noise cancellation filter adaptation rate
    UINT32              smpfilter;        // continuous-time pathway filter id
    UINT32              smpgroup;         // continuous-time pathway sample group
    INT32               smpdispmin;       // continuous-time pathway display factor
    INT32               smpdispmax;       // continuous-time pathway display factor
    UINT32              spkfilter;        // spike pathway filter id
    INT32               spkdispmax;       // spike pathway display factor
    INT32               lncdispmax;       // Line Noise pathway display factor
    UINT32              spkopts;          // spike processing options
    INT32               spkthrlevel;      // spike threshold level
    INT32               spkthrlimit;      //
    UINT32              spkgroup;         // NTrodeGroup this electrode belongs to - 0 is single unit, non-0 indicates a multi-trode grouping
    INT16               amplrejpos;       // Amplitude rejection positive value
    INT16               amplrejneg;       // Amplitude rejection negative value
    UINT32              refelecchan;      // Software reference electrode channel
    cbMANUALUNITMAPPING_37 unitmapping[cbLEGACY_MAXUNITS];            // manual unit mapping
    cbHOOP_10              spkhoops[cbLEGACY_MAXUNITS][cbLEGACY_MAXHOOPS];   // spike hoop sorting set
} cbPKT_CHANINFO_CB2005_37;
#define cbPKTDLEN_CHANINFO_CB2005_37      ((sizeof(cbPKT_CHANINFO_CB2005_37)/4)-2)

typedef struct {
    UINT32 time;           // system clock timestamp
    UINT16 chid;           // 0x8000
    UINT8 type;            // cbPKTTYPE_REPNTRODEINFO or cbPKTTYPE_SETNTRODEINFO
    UINT8 dlen;            // cbPKTDLEN_NTRODEGRPINFO
    UINT32 ntrode;         // ntrode with which we are working (1-based)
    char   label[cbLEGACY_LEN_STR_LABEL];   // Label of the Ntrode (null terminated if < 16 characters)
    cbMANUALUNITMAPPING_37 ellipses[cbLEGACY_MAXSITEPLOTS][cbLEGACY_MAXUNITS];  // unit mapping
    UINT16 nSite;          // number channels in this NTrode ( 0 <= nSite <= cbLEGACY_MAXSITES)
    UINT16 fs;             // NTrode feature space cbNTRODEINFO_FS_*
    UINT16 nChan[cbLEGACY_MAXSITES];  // group of channels in this NTrode
} cbPKT_NTRODEINFO_CB2005_37;  // ntrode information packet

typedef struct {
    UINT32  time;           // system clock timestamp
    UINT16  chid;           // 0x8000
    UINT8   type;           // cbPKTTYPE_SS_DETECTREP or cbPKTTYPE_SS_DETECTSET depending on the direction
    UINT8   dlen;           // cbPKTDLEN_SS_DETECT

    float   fThreshold;     // current detection threshold
    float   fMultiplier;    // multiplier
} cbPKT_SS_DETECT_CB2005_37;

typedef struct {
    UINT32  time;               // system clock timestamp
    UINT16  chid;               // 0x8000
    UINT8   type;               // cbPKTTYPE_SS_ARTIF_REJECTREP or cbPKTTYPE_SS_ARTIF_REJECTSET depending on the direction
    UINT8   dlen;               // cbPKTDLEN_SS_ARTIF_REJECT

    UINT32  nMaxSimulChans;     // how many channels can fire exactly at the same time???
    UINT32  nRefractoryCount;   // for how many samples (30 kHz) is a neuron refractory, so can't re-trigger
} cbPKT_SS_ARTIF_REJECT_CB2005_37;

typedef struct {
    UINT32 nMode;           // 0-do not adapt at all, 1-always adapt, 2-adapt if timer not timed out
    float fTimeOutMinutes;  // how many minutes until time out
    float fElapsedMinutes;  // the amount of time that has elapsed
} cbAdaptControl_37;

typedef struct {
    UINT32  time;           // system clock timestamp
    UINT16  chid;           // 0x8000
    UINT8   type;           // cbPKTTYPE_SS_STATUSREP or cbPKTTYPE_SS_STATUSSET depending on the direction
    UINT8   dlen;           // cbPKTDLEN_SS_STATUS

    cbAdaptControl_37 cntlUnitStats;
    cbAdaptControl_37 cntlNumUnits;
} cbPKT_SS_STATUS_CB2005_37;

typedef struct {
    UINT32  time;           // system clock timestamp
    UINT16  chid;           // 0x8000
    UINT8   type;           // cbPKTTYPE_SS_STATISTICSREP or cbPKTTYPE_SS_STATISTICSSET depending on the direction
    UINT8   dlen;           // cbPKTDLEN_SS_STATISTICS

    UINT32  nUpdateSpikes;  // update rate in spike counts

    float   fMinClusterSpreadFactor;    // larger number = more apt to combine 2 clusters into 1
    float   fMaxSubclusterSpreadFactor; // larger number = less apt to split because of 2 clusers
    UINT32  nReserved;                  // reserved for future use
} cbPKT_SS_STATISTICS_CB2005_30;

typedef struct {
    UINT32  time;           // system clock timestamp
    UINT16  chid;           // 0x8000
    UINT8   type;           // cbPKTTYPE_SS_STATISTICSREP or cbPKTTYPE_SS_STATISTICSSET depending on the direction
    UINT8   dlen;           // cbPKTDLEN_SS_STATISTICS

    UINT32  nUpdateSpikes;  // update rate in spike counts

    UINT32  nAutoalg;       // automatic sorting algorithm (0=spread, 1=hist_corr_maj, 2=hist_peak_count_maj)

    float   fMinClusterPairSpreadFactor;       // larger number = more apt to combine 2 clusters into 1
    float   fMaxSubclusterSpreadFactor;        // larger number = less apt to split because of 2 clusers

    float   fMinClusterHistCorrMajMeasure;     // larger number = more apt to split 1 cluster into 2
    float   fMaxClusterPairHistCorrMajMeasure; // larger number = less apt to combine 2 clusters into 1

    float   fClusterHistMajValleyPercentage;   // larger number = less apt to split nearby clusters
    float   fClusterHistMajPeakPercentage;     // larger number = less apt to split separated clusters
} cbPKT_SS_STATISTICS_CB2005_31;

typedef struct {
    UINT32  time;           // system clock timestamp
    UINT16  chid;           // 0x8000
    UINT8   type;           // cbPKTTYPE_SS_STATISTICSREP or cbPKTTYPE_SS_STATISTICSSET depending on the direction
    UINT8   dlen;           // cbPKTDLEN_SS_STATISTICS

    UINT32  nUpdateSpikes;  // update rate in spike counts

    UINT32  nAutoalg;       // automatic sorting algorithm (0=spread, 1=hist_corr_maj, 2=hist_peak_count_maj)

    float   fMinClusterPairSpreadFactor;       // larger number = more apt to combine 2 clusters into 1
    float   fMaxSubclusterSpreadFactor;        // larger number = less apt to split because of 2 clusers

    float   fMinClusterHistCorrMajMeasure;     // larger number = more apt to split 1 cluster into 2
    float   fMaxClusterPairHistCorrMajMeasure; // larger number = less apt to combine 2 clusters into 1

    float   fClusterHistValleyPercentage;      // larger number = less apt to split nearby clusters
    float   fClusterHistClosePeakPercentage;   // larger number = less apt to split nearby clusters
    float   fClusterHistMinPeakPercentage;     // larger number = less apt to split separated clusters
} cbPKT_SS_STATISTICS_CB2005_32;

typedef struct {
    UINT32  time;           // system clock timestamp
    UINT16  chid;           // 0x8000
    UINT8   type;           // cbPKTTYPE_SS_STATISTICSREP or cbPKTTYPE_SS_STATISTICSSET depending on the direction
    UINT8   dlen;           // cbPKTDLEN_SS_STATISTICS

    UINT32  nUpdateSpikes;  // update rate in spike counts

    UINT32  nAutoalg;       // sorting algorithm (0=none 1=spread, 2=hist_corr_maj, 3=hist_peak_count_maj, 4=hist_peak_count_maj_fisher, 5=pca, 6=hoops)
    UINT32  nMode;          // cbAUTOALG_MODE_SETTING,

    float   fMinClusterPairSpreadFactor;       // larger number = more apt to combine 2 clusters into 1
    float   fMaxSubclusterSpreadFactor;        // larger number = less apt to split because of 2 clusers

    float   fMinClusterHistCorrMajMeasure;     // larger number = more apt to split 1 cluster into 2
    float   fMaxClusterPairHistCorrMajMeasure; // larger number = less apt to combine 2 clusters into 1

    float   fClusterHistValleyPercentage;      // larger number = less apt to split nearby clusters
    float   fClusterHistClosePeakPercentage;   // larger number = less apt to split nearby clusters
    float   fClusterHistMinPeakPercentage;     // larger number = less apt to split separated clusters

    UINT32  nWaveBasisSize;                     // number of wave to collect to calculate the basis,
                                                // must be greater than spike length
    UINT32  nWaveSampleSize;                    // number of samples sorted with the same basis before re-calculating the basis
                                                // 0=manual re-calculation
                                                // nWaveBasisSize * nWaveSampleSize is the number of waves/spikes to run against
                                                // the same PCA basis before next
#ifdef __cplusplus
    void set(UINT32 nUpdateSpikes, UINT32 nAutoalg, UINT32 nMode, float fMinClusterPairSpreadFactor, float fMaxSubclusterSpreadFactor,
             float fMinClusterHistCorrMajMeasure, float fMaxClusterPairHistCorrMajMeasure,
             float fClusterHistValleyPercentage, float fClusterHistClosePeakPercentage, float fClusterHistMinPeakPercentage,
             UINT32 nWaveBasisSize, UINT32 nWaveSampleSize)
    {
        chid = cbPKTCHAN_CONFIGURATION;
        type = cbPKTTYPE_SS_STATISTICSSET;
        dlen = ((sizeof(*this) / 4) - 2);

        this->nUpdateSpikes = nUpdateSpikes;
        this->nAutoalg = nAutoalg;
        this->nMode = nMode;
        this->fMinClusterPairSpreadFactor = fMinClusterPairSpreadFactor;
        this->fMaxSubclusterSpreadFactor = fMaxSubclusterSpreadFactor;
        this->fMinClusterHistCorrMajMeasure = fMinClusterHistCorrMajMeasure;
        this->fMaxClusterPairHistCorrMajMeasure = fMaxClusterPairHistCorrMajMeasure;
        this->fClusterHistValleyPercentage = fClusterHistValleyPercentage;
        this->fClusterHistClosePeakPercentage = fClusterHistClosePeakPercentage;
        this->fClusterHistMinPeakPercentage = fClusterHistMinPeakPercentage;
        this->nWaveBasisSize = nWaveBasisSize;
        this->nWaveSampleSize = nWaveSampleSize;
    }
#endif

} cbPKT_SS_STATISTICS_CB2005_37;

typedef struct {
    UINT32  time;               // system clock timestamp
    UINT16  chid;               // 0x8000
    UINT8   type;               // cbPKTTYPE_SS_NOISE_BOUNDARYREP or cbPKTTYPE_SS_NOISE_BOUNDARYSET depending on the direction
    UINT8   dlen;               // cbPKTDLEN_SS_ARTIF_REJECT

    float   afc[2];             // the center of an ellipse
    float   afS[2][2];          // the shape and size of an ellipse
} cbPKT_SS_NOISE_BOUNDARY_CB2005_31;

typedef struct {
    UINT32  time;               // system clock timestamp
    UINT16  chid;               // 0x8000
    UINT8   type;               // cbPKTTYPE_SS_NOISE_BOUNDARYREP or cbPKTTYPE_SS_NOISE_BOUNDARYSET depending on the direction
    UINT8   dlen;               // cbPKTDLEN_SS_ARTIF_REJECT

    float   afc[2];             // the center of an ellipse
    float   afS[2][2];          // the shape and size of an ellipse
    float   aTheta;              // the angle of rotation for the Noise Boundary (ellipse)
} cbPKT_SS_NOISE_BOUNDARY_CB2005_33;

typedef struct {
    UINT32  time;               // system clock timestamp
    UINT16  chid;               // 0x8000
    UINT8   type;               // cbPKTTYPE_SS_NOISE_BOUNDARYREP or cbPKTTYPE_SS_NOISE_BOUNDARYSET depending on the direction
    UINT8   dlen;               // cbPKTDLEN_SS_ARTIF_REJECT

    UINT32  chan;               // which channel we belong to
    float   afc[2];             // the center of an ellipse
    float   afS[2][2];          // the shape and size of an ellipse
    float   aTheta;             // the angle of rotation for the Noise Boundary (ellipse)
} cbPKT_SS_NOISE_BOUNDARY_CB2005_34;

typedef struct {
    UINT32  time;               // system clock timestamp
    UINT16  chid;               // 0x8000
    UINT8   type;               // cbPKTTYPE_SS_NOISE_BOUNDARYREP or cbPKTTYPE_SS_NOISE_BOUNDARYSET depending on the direction
    UINT8   dlen;               // cbPKTDLEN_SS_ARTIF_REJECT

    UINT32  chan;               // which channel we belong to
    float   afc[3];             // the center of the ellipsoid
    float   afS[3][3];          // an array of the axes for the ellipsoid

#ifdef __cplusplus
    void set(UINT32 chan, float afc1, float afc2, float afS11, float afS12, float afS21, float afS22, float /*theta = 0*/)
    {// theta is ignored, but kept for backward compatibility
        set(chan, afc1, afc2, 0, afS11, afS12, 0, afS21, afS22, 0, 0, 0, 50);
    }

    void set(UINT32 chan, float cen1,float cen2, float cen3, float maj1, float maj2, float maj3,
        float min11, float min12, float min13, float min21, float min22, float min23)
    {
        chid = cbPKTCHAN_CONFIGURATION;
        type = cbPKTTYPE_SS_NOISE_BOUNDARYSET;
        dlen = ((sizeof(*this) / 4) - 2);
        this->chan = chan;
        this->afc[0] = cen1;
        this->afc[1] = cen2;
        this->afc[2] = cen3;
        this->afS[0][0] = maj1;
        this->afS[0][1] = maj2;
        this->afS[0][2] = maj3;
        this->afS[1][0] = min11;
        this->afS[1][1] = min12;
        this->afS[1][2] = min13;
        this->afS[2][0] = min21;
        this->afS[2][1] = min22;
        this->afS[2][2] = min23;
    }

    // The information obtained by these functions is implicit within the structure data,
    // but they are provided for convenience
    void GetAxisLengths(float afAxisLen[3])const;
    void GetRotationAngles(float afTheta[3])const;

#endif

} cbPKT_SS_NOISE_BOUNDARY_CB2005_37;

typedef struct {
    UINT32 time;        // system clock timestamp
    UINT16 chid;        // 0x8000
    UINT8  type;        // PKTTYPE_SYS*
    UINT8  dlen;        // cbPKT_SYSINFODLEN
    UINT32 sysfreq;     // System clock frequency in Hz
    UINT32 spikelen;    // The length of the spike events
    UINT32 spikepre;    // Spike pre-trigger samples
    UINT32 resetque;    // The channel for the reset to que on
    UINT32 runlevel;    // System runlevel
    UINT32 runflags;
} cbPKT_SYSINFO_CB2005_37;

typedef struct {
    UINT32  time;           // system clock timestamp
    UINT16  chid;           // 0x8000
    UINT8   type;           // cbPKTTYPE_ADAPTFILTSET or cbPKTTYPE_ADAPTFILTREP
    UINT8   dlen;           // cbPKTDLEN_ADAPTFILTINFO
    UINT32  chan;           // Ignored

    UINT32  nMode;          // 0=disabled, 1=filter continuous & spikes, 2=filter spikes
    float   dLearningRate;  // speed at which adaptation happens. Very small. e.g. 5e-12
    UINT32  nRefChan1;      // The first reference channel (1 based).
    UINT32  nRefChan2;      // The second reference channel (1 based).

} cbPKT_ADAPTFILTINFO_CB2005_37;

typedef struct {
    cbPKT_CHANINFO_CB2005_37 isChan[cbLEGACY_MAXCHANS];
    cbPKT_ADAPTFILTINFO_CB2005_37 isAdaptInfo;
    cbPKT_SS_DETECT_CB2005_37 isSS_Detect;
    cbPKT_SS_ARTIF_REJECT_CB2005_37 isSS_ArtifactReject;
    cbPKT_SS_NOISE_BOUNDARY_CB2005_37 isSS_NoiseBoundary[cbLEGACY_NUM_ANALOG_CHANS];
    cbPKT_SS_STATISTICS_CB2005_37 isSS_Statistics;
    cbPKT_SS_STATUS_CB2005_37 isSS_Status;
    cbPKT_SYSINFO_CB2005_37 isSysInfo;
    cbPKT_NTRODEINFO_CB2005_37 isNTrodeInfo[cbLEGACY_MAXNTRODES];
} ccfBinaryData;

};      // namespace ccf

// A snapshot of the binary CCF in the last binary version (3.9)
class CCFUtilsBinary : public CCFUtils
{
public:
    CCFUtilsBinary();

public:
    // Purpose: load the channel configuration from the file
    ccf::ccfResult ReadCCF(LPCSTR szFileName, bool bConvert);
    ccf::ccfResult ReadVersion(LPCSTR szFileName); // Read the version alone

public:
    // We give it public access to make it easy
    ccf::ccfBinaryData m_data; // Internal structure holding actual config parameters

protected:
    // Convert from old config (generic)
    virtual CCFUtils * Convert(CCFUtils * pOldConfig);
    // Keep old binary writing for possible backward porting
    virtual ccf::ccfResult WriteCCFNoPrompt(LPCSTR szFileName);

private:
    // Used as identifiers in the channel configuration files
    static const char m_szConfigFileHeader[7];
    static const char m_szConfigFileVersion[11];
    static const int m_iPreviousHeaderSize = 9;

    inline void cpyScaling(ccf::cbSCALING_30 &dest, ccf::cbSCALING_NO_ANAGAIN &src)
    {
        dest.anagain = 1;
        dest.anamax = src.anamax;
        dest.anamin = src.anamin;
        memcpy(dest.anaunit, src.anaunit, sizeof(dest.anaunit));
        dest.digmax = src.digmax;
        dest.digmin = src.digmin;
    }

    void SetChannelDefaults(ccf::cbPKT_CHANINFO_CB2005_37 &isChan);
    UINT32 TranslateAutoFilter(UINT32 filter);
    void ReadSpikeSortingPackets(ccf::cbPKT_GENERIC_CB2003_10 *pPkt);
    // Purpose: after reading channel info from the config file, read the rest of the
    //          data as individual packets to the binary settings
    void ReadAsPackets(FILE * hFile);
    // Purpose: load the channel configuration from different binary file versions
    ccf::ccfResult ReadCCFData_cb2003_10(FILE * hFile);
    ccf::ccfResult ReadCCFData_cb2003_10_a(FILE * hFile);
    ccf::ccfResult ReadCCFData_cb2003_10_b(FILE * hFile);
    ccf::ccfResult ReadCCFData_cb2005_25(FILE * hFile);
    ccf::ccfResult ReadCCFData_cb2005_30(FILE * hFile);
    ccf::ccfResult ReadCCFData_cb2005_31(FILE * hFile);
    ccf::ccfResult ReadCCFData_cb2005_34(FILE * hFile);
    ccf::ccfResult ReadCCFData_cb2005_35(FILE * hFile);
    ccf::ccfResult ReadCCFData_cb2005_36(FILE * hFile);
    ccf::ccfResult ReadCCFData_cb2005_37(FILE * hFile);
};

#endif // include guard
