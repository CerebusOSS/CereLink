///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   types.h
/// @author CereLink Development Team
/// @date   2025-11-11
///
/// @brief  Core protocol types and constants
///
/// This file contains the fundamental types and constants that define the Cerebus protocol.
/// These MUST match Blackrock's cbproto.h exactly to ensure compatibility with Central.
///
/// DO NOT MODIFY unless updating from upstream protocol changes.
///
/// Reference: cbproto.h (Protocol Version 4.2)
///
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef CBPROTO_TYPES_H
#define CBPROTO_TYPES_H

#include <stdint.h>

// Ensure tight packing for network protocol structures
#pragma pack(push, 1)

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name Protocol Version
/// @{

#define cbVERSION_MAJOR  4
#define cbVERSION_MINOR  2

/// @}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name Time Type
/// @{

/// @brief Processor time type
/// Protocol 4.0+ uses 64-bit timestamps
/// Protocol 3.x uses 32-bit timestamps (compile with CBPROTO_311)
#ifdef CBPROTO_311
typedef uint32_t PROCTIME;
#else
typedef uint64_t PROCTIME;
#endif

/// @}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name Maximum Entity Ranges
///
/// Ground truth from upstream/cbproto/cbproto.h lines 237-262
/// These define the maximum number of instruments, channels, etc.
/// @{

#define cbNSP1          1   ///< First instrument ID (1-based)
#define cbNSP2          2   ///< Second instrument ID (1-based)
#define cbNSP3          3   ///< Third instrument ID (1-based)
#define cbNSP4          4   ///< Fourth instrument ID (1-based)

#define cbMAXOPEN       4   ///< Maximum number of open cbhwlib's (instruments)
#define cbMAXPROCS      1   ///< Number of processors per NSP

#define cbNUM_FE_CHANS  256 ///< Front-end channels per NSP

#define cbMAXGROUPS     8   ///< Number of sample rate groups
#define cbMAXFILTS      32  ///< Maximum number of filters
#define cbMAXHOOPS      4   ///< Maximum number of hoops for spike sorting
#define cbMAXSITES      4   ///< Maximum number of electrodes in an n-trode group
#define cbMAXSITEPLOTS  ((cbMAXSITES - 1) * cbMAXSITES / 2)  ///< Combination of 2 out of n
#define cbMAXUNITS      5   ///< Maximum number of sorted units per channel
#define cbMAXNTRODES    (cbNUM_ANALOG_CHANS / 2)  ///< Maximum n-trodes (stereotrode minimum)
#define cbMAX_PNTS      128 ///< Maximum spike waveform points

/// @}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name Channel Counts
///
/// Ground truth from upstream/cbproto/cbproto.h lines 252-262
/// Note: Some channel counts depend on cbMAXPROCS
/// @{

#define cbNUM_ANAIN_CHANS       (16 * cbMAXPROCS)   ///< Analog input channels
#define cbNUM_ANALOG_CHANS      (cbNUM_FE_CHANS + cbNUM_ANAIN_CHANS)  ///< Total analog inputs
#define cbNUM_ANAOUT_CHANS      (4 * cbMAXPROCS)    ///< Analog output channels
#define cbNUM_AUDOUT_CHANS      (2 * cbMAXPROCS)    ///< Audio output channels
#define cbNUM_ANALOGOUT_CHANS   (cbNUM_ANAOUT_CHANS + cbNUM_AUDOUT_CHANS)  ///< Total analog outputs
#define cbNUM_DIGIN_CHANS       (1 * cbMAXPROCS)    ///< Digital input channels
#define cbNUM_SERIAL_CHANS      (1 * cbMAXPROCS)    ///< Serial input channels
#define cbNUM_DIGOUT_CHANS      (4 * cbMAXPROCS)    ///< Digital output channels

/// @brief Total number of channels
#define cbMAXCHANS  (cbNUM_ANALOG_CHANS + cbNUM_ANALOGOUT_CHANS + \
                     cbNUM_DIGIN_CHANS + cbNUM_SERIAL_CHANS + cbNUM_DIGOUT_CHANS)

/// @}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name Network Configuration
///
/// Ground truth from upstream/cbproto/cbproto.h lines 193-196
/// @{

#define cbNET_UDP_ADDR_INST     "192.168.137.1"     ///< Cerebus default address
#define cbNET_UDP_ADDR_CNT      "192.168.137.128"   ///< Gemini NSP default control address
#define cbNET_UDP_ADDR_BCAST    "192.168.137.255"   ///< NSP default broadcast address
#define cbNET_UDP_PORT_BCAST    51002               ///< Neuroflow Data Port
#define cbNET_UDP_PORT_CNT      51001               ///< Neuroflow Control Port

/// @}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name String Length Constants
/// @{

#define cbLEN_STR_UNIT          8       ///< Length of unit string
#define cbLEN_STR_LABEL         16      ///< Length of label string
#define cbLEN_STR_FILT_LABEL    16      ///< Length of filter label string
#define cbLEN_STR_IDENT         64      ///< Length of identity string
#define cbLEN_STR_COMMENT       256     ///< Length of comment string

/// @}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name Packet Header
///
/// Ground truth from upstream/cbproto/cbproto.h lines 376-383
/// Every packet contains this header (must be a multiple of uint32_t)
/// @{

/// @brief Cerebus packet header data structure
typedef struct {
    PROCTIME time;        ///< System clock timestamp
    uint16_t chid;        ///< Channel identifier
    uint16_t type;        ///< Packet type
    uint16_t dlen;        ///< Length of data field in 32-bit chunks
    uint8_t  instrument;  ///< Instrument number (0-based in packet, despite cbNSP1=1!)
    uint8_t  reserved;    ///< Reserved for future use
} cbPKT_HEADER;

#define cbPKT_MAX_SIZE      1024                    ///< Maximum packet size in bytes
#define cbPKT_HEADER_SIZE   sizeof(cbPKT_HEADER)    ///< Packet header size in bytes
#define cbPKT_HEADER_32SIZE (cbPKT_HEADER_SIZE / 4) ///< Packet header size in uint32_t's

/// @}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name Generic Packet
///
/// Base packet structure for all packet types
/// @{

/// @brief Generic packet structure
typedef struct {
    cbPKT_HEADER cbpkt_header;  ///< Packet header
    union {
        uint8_t  data_u8[cbPKT_MAX_SIZE - cbPKT_HEADER_SIZE];   ///< Data as uint8_t array
        uint16_t data_u16[(cbPKT_MAX_SIZE - cbPKT_HEADER_SIZE) / 2];  ///< Data as uint16_t array
        uint32_t data_u32[(cbPKT_MAX_SIZE - cbPKT_HEADER_SIZE) / 4];  ///< Data as uint32_t array
    };
} cbPKT_GENERIC;

/// @}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name Dependent Structures
///
/// These structures are used within packet structures
/// @{

/// @brief Filter description structure
///
/// Filter description used in cbPKT_CHANINFO
typedef struct {
    char    label[cbLEN_STR_FILT_LABEL];
    uint32_t  hpfreq;     ///< high-pass corner frequency in milliHertz
    uint32_t  hporder;    ///< high-pass filter order
    uint32_t  hptype;     ///< high-pass filter type
    uint32_t  lpfreq;     ///< low-pass frequency in milliHertz
    uint32_t  lporder;    ///< low-pass filter order
    uint32_t  lptype;     ///< low-pass filter type
} cbFILTDESC;

/// @brief Scaling structure
///
/// Structure used in cbPKT_CHANINFO
typedef struct {
    int16_t   digmin;     ///< digital value that cooresponds with the anamin value
    int16_t   digmax;     ///< digital value that cooresponds with the anamax value
    int32_t   anamin;     ///< the minimum analog value present in the signal
    int32_t   anamax;     ///< the maximum analog value present in the signal
    int32_t   anagain;    ///< the gain applied to the default analog values to get the analog values
    char    anaunit[cbLEN_STR_UNIT]; ///< the unit for the analog signal (eg, "uV" or "MPa")
} cbSCALING;

/// @brief Amplitude Rejection structure
typedef struct {
    uint32_t bEnabled;    ///< BOOL implemented as uint32_t - for structure alignment at paragraph boundary
    int16_t  nAmplPos;    ///< any spike that has a value above nAmplPos will be rejected
    int16_t  nAmplNeg;    ///< any spike that has a value below nAmplNeg will be rejected
} cbAMPLITUDEREJECT;

/// @brief Manual Unit Mapping structure
///
/// Defines an ellipsoid for sorting.  Used in cbPKT_CHANINFO and cbPKT_NTRODEINFO
typedef struct {
    int16_t       nOverride;      ///< override to unit if in ellipsoid
    int16_t       afOrigin[3];    ///< ellipsoid origin
    int16_t       afShape[3][3];  ///< ellipsoid shape
    int16_t       aPhi;           ///<
    uint32_t      bValid;         ///< is this unit in use at this time?
                                  ///< BOOL implemented as uint32_t - for structure alignment at paragraph boundary
} cbMANUALUNITMAPPING;

/// @brief Hoop definition structure
///
/// Defines the hoop used for sorting.  There can be up to 5 hoops per unit.  Used in cbPKT_CHANINFO
typedef struct {
    uint16_t valid; ///< 0=undefined, 1 for valid
    int16_t  time;  ///< time offset into spike window
    int16_t  min;   ///< minimum value for the hoop window
    int16_t  max;   ///< maximum value for the hoop window
} cbHOOP;

/// @}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name Configuration Structures
///
/// Non-packet structures used for configuration
/// @{

/// @brief Signal Processor Configuration Structure
typedef struct {
    uint32_t idcode;      ///< manufacturer part and rom ID code of the Signal Processor
    char   ident[cbLEN_STR_IDENT];   ///< ID string with the equipment name of the Signal Processor
    uint32_t chanbase;    ///< lowest channel identifier claimed by this processor
    uint32_t chancount;   ///< number of channel identifiers claimed by this processor
    uint32_t bankcount;   ///< number of signal banks supported by the processor
    uint32_t groupcount;  ///< number of sample groups supported by the processor
    uint32_t filtcount;   ///< number of digital filters supported by the processor
    uint32_t sortcount;   ///< number of channels supported for spike sorting (reserved for future)
    uint32_t unitcount;   ///< number of supported units for spike sorting    (reserved for future)
    uint32_t hoopcount;   ///< number of supported hoops for spike sorting    (reserved for future)
    uint32_t reserved;    ///< reserved for future use, set to 0
    uint32_t version;     ///< current version of libraries
} cbPROCINFO;

/// @brief Signal Bank Configuration Structure
typedef struct {
    uint32_t idcode;     ///< manufacturer part and rom ID code of the module addressed to this bank
    char   ident[cbLEN_STR_IDENT];  ///< ID string with the equipment name of the Signal Bank hardware module
    char   label[cbLEN_STR_LABEL];  ///< Label on the instrument for the signal bank, eg "Analog In"
    uint32_t chanbase;   ///< lowest channel identifier claimed by this bank
    uint32_t chancount;  ///< number of channel identifiers claimed by this bank
} cbBANKINFO;

/// @}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name Packet Type Constants
///
/// Ground truth from upstream/cbproto/cbproto.h
/// These define all the packet types in the Cerebus protocol
/// @{

// System packets
#define cbPKTTYPE_SYSHEARTBEAT          0x00
#define cbPKTTYPE_SYSPROTOCOLMONITOR    0x01
#define cbPKTTYPE_REPCONFIGALL          0x08    ///< Response that NSP got your request
#define cbPKTTYPE_SYSREP                0x10
#define cbPKTTYPE_SYSREPSPKLEN          0x11
#define cbPKTTYPE_SYSREPRUNLEV          0x12
#define cbPKTTYPE_REQCONFIGALL          0x88    ///< Request for ALL configuration information
#define cbPKTTYPE_SYSSET                0x90
#define cbPKTTYPE_SYSSETSPKLEN          0x91
#define cbPKTTYPE_SYSSETRUNLEV          0x92

// Processor and bank information packets
#define cbPKTTYPE_PROCREP               0x21
#define cbPKTTYPE_BANKREP               0x22

// Filter information packets
#define cbPKTTYPE_FILTREP               0x23
#define cbPKTTYPE_FILTSET               0xA3

// Sample group information packets
#define cbPKTTYPE_GROUPREP              0x30
#define cbPKTTYPE_GROUPSET              0xB0

// Channel information packets
#define cbPKTTYPE_CHANREP               0x40
#define cbPKTTYPE_CHANSET               0xC0

// Unit selection packets
#define cbPKTTYPE_REPUNITSELECTION      0x62
#define cbPKTTYPE_SETUNITSELECTION      0xE2

/// @}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name Channel Constants
/// @{

#define cbPKTCHAN_CONFIGURATION         0x8000  ///< Channel # to mean configuration

/// @}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name Runlevel Constants
///
/// Ground truth from upstream/cbproto/cbproto.h
/// These define the system runlevel states
/// @{

#define cbRUNLEVEL_UPDATE       78
#define cbRUNLEVEL_STARTUP      10
#define cbRUNLEVEL_HARDRESET    20
#define cbRUNLEVEL_STANDBY      30
#define cbRUNLEVEL_RESET        40
#define cbRUNLEVEL_RUNNING      50
#define cbRUNLEVEL_STRESSED     60
#define cbRUNLEVEL_ERROR        70
#define cbRUNLEVEL_SHUTDOWN     80

/// @}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name System Packet Structures
///
/// Ground truth from upstream/cbproto/cbproto.h
/// @{

/// @brief PKT Set:0x92 Rep:0x12 - System info
///
/// Contains system information including the runlevel
typedef struct {
    cbPKT_HEADER cbpkt_header;  ///< Packet header

    uint32_t sysfreq;     ///< System sampling clock frequency in Hz
    uint32_t spikelen;    ///< The length of the spike events
    uint32_t spikepre;    ///< Spike pre-trigger samples
    uint32_t resetque;    ///< The channel for the reset to que on
    uint32_t runlevel;    ///< System runlevel
    uint32_t runflags;    ///< Lock recording after reset
} cbPKT_SYSINFO;

#define cbPKTDLEN_SYSINFO       ((sizeof(cbPKT_SYSINFO)/4) - cbPKT_HEADER_32SIZE)

/// @}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name Additional Configuration Packets
/// @{

/// @brief PKT Set:N/A  Rep:0x21 - Info about the processor
///
/// Includes information about the counts of various features of the processor
typedef struct {
    cbPKT_HEADER cbpkt_header;  ///< packet header

    uint32_t proc;        ///< index of the bank
    uint32_t idcode;      ///< manufacturer part and rom ID code of the Signal Processor
    char   ident[cbLEN_STR_IDENT];   ///< ID string with the equipment name of the Signal Processor
    uint32_t chanbase;    ///< lowest channel number of channel id range claimed by this processor
    uint32_t chancount;   ///< number of channel identifiers claimed by this processor
    uint32_t bankcount;   ///< number of signal banks supported by the processor
    uint32_t groupcount;  ///< number of sample groups supported by the processor
    uint32_t filtcount;   ///< number of digital filters supported by the processor
    uint32_t sortcount;   ///< number of channels supported for spike sorting (reserved for future)
    uint32_t unitcount;   ///< number of supported units for spike sorting    (reserved for future)
    uint32_t hoopcount;   ///< number of supported units for spike sorting    (reserved for future)
    uint32_t reserved;    ///< reserved for future use, set to 0
    uint32_t version;     ///< current version of libraries
} cbPKT_PROCINFO;

#define cbPKTDLEN_PROCINFO  ((sizeof(cbPKT_PROCINFO)/4) - cbPKT_HEADER_32SIZE)

/// @brief PKT Set:N/A  Rep:0x22 - Information about the banks in the processor
typedef struct {
    cbPKT_HEADER cbpkt_header;  ///< packet header

    uint32_t proc;        ///< the address of the processor on which the bank resides
    uint32_t bank;        ///< the address of the bank reported by the packet
    uint32_t idcode;      ///< manufacturer part and rom ID code of the module addressed to this bank
    char   ident[cbLEN_STR_IDENT];   ///< ID string with the equipment name of the Signal Bank hardware module
    char   label[cbLEN_STR_LABEL];   ///< Label on the instrument for the signal bank, eg "Analog In"
    uint32_t chanbase;    ///< lowest channel number of channel id range claimed by this bank
    uint32_t chancount;   ///< number of channel identifiers claimed by this bank
} cbPKT_BANKINFO;

#define cbPKTDLEN_BANKINFO  ((sizeof(cbPKT_BANKINFO)/4) - cbPKT_HEADER_32SIZE)

/// @brief PKT Set:0xA3 Rep:0x23 - Filter Information Packet
///
/// Describes the filters contained in the NSP including the filter coefficients
typedef struct {
    cbPKT_HEADER cbpkt_header;  ///< packet header

    uint32_t proc;       ///<
    uint32_t filt;       ///<
    char   label[cbLEN_STR_FILT_LABEL];  // name of the filter
    uint32_t hpfreq;     ///< high-pass corner frequency in milliHertz
    uint32_t hporder;    ///< high-pass filter order
    uint32_t hptype;     ///< high-pass filter type
    uint32_t lpfreq;     ///< low-pass frequency in milliHertz
    uint32_t lporder;    ///< low-pass filter order
    uint32_t lptype;     ///< low-pass filter type
    ///< These are for sending the NSP filter info, otherwise the NSP has this stuff in NSPDefaults.c
    double gain;        ///< filter gain
    double sos1a1;      ///< filter coefficient
    double sos1a2;      ///< filter coefficient
    double sos1b1;      ///< filter coefficient
    double sos1b2;      ///< filter coefficient
    double sos2a1;      ///< filter coefficient
    double sos2a2;      ///< filter coefficient
    double sos2b1;      ///< filter coefficient
    double sos2b2;      ///< filter coefficient
} cbPKT_FILTINFO;

#define cbPKTDLEN_FILTINFO  ((sizeof(cbPKT_FILTINFO)/4) - cbPKT_HEADER_32SIZE)

/// @brief PKT Set:0xB0 Rep:0x30 - Sample Group (GROUP) Information Packets
///
/// Contains information including the name and list of channels for each sample group.  The cbPKT_GROUP packet transmits
/// the data for each group based on the list contained here.
typedef struct {
    cbPKT_HEADER cbpkt_header;  ///< packet header

    uint32_t proc;       ///< processor number
    uint32_t group;      ///< group number
    char   label[cbLEN_STR_LABEL];  ///< sampling group label
    uint32_t period;     ///< sampling period for the group
    uint32_t length;     ///< number of channels in the list
    uint16_t list[cbNUM_ANALOG_CHANS];   ///< variable length list. The max size is the total number of analog channels
} cbPKT_GROUPINFO;

#define cbPKTDLEN_GROUPINFO  ((sizeof(cbPKT_GROUPINFO)/4) - cbPKT_HEADER_32SIZE)
#define cbPKTDLEN_GROUPINFOSHORT  (8)       // basic length without list

/// @brief PKT Set:0xCx Rep:0x4x - Channel Information
///
/// This contains the details for each channel within the system.
typedef struct {
    cbPKT_HEADER cbpkt_header;  ///< packet header

    uint32_t     chan;           ///< actual channel id of the channel being configured
    uint32_t     proc;           ///< the address of the processor on which the channel resides
    uint32_t     bank;           ///< the address of the bank on which the channel resides
    uint32_t     term;           ///< the terminal number of the channel within it's bank
    uint32_t     chancaps;       ///< general channel capablities (given by cbCHAN_* flags)
    uint32_t     doutcaps;       ///< digital output capablities (composed of cbDOUT_* flags)
    uint32_t     dinpcaps;       ///< digital input capablities (composed of cbDINP_* flags)
    uint32_t     aoutcaps;       ///< analog output capablities (composed of cbAOUT_* flags)
    uint32_t     ainpcaps;       ///< analog input capablities (composed of cbAINP_* flags)
    uint32_t     spkcaps;        ///< spike processing capabilities
    cbSCALING  physcalin;      ///< physical channel scaling information
    cbFILTDESC phyfiltin;      ///< physical channel filter definition
    cbSCALING  physcalout;     ///< physical channel scaling information
    cbFILTDESC phyfiltout;     ///< physical channel filter definition
    char       label[cbLEN_STR_LABEL];   ///< Label of the channel (null terminated if <16 characters)
    uint32_t     userflags;      ///< User flags for the channel state
    int32_t      position[4];    ///< reserved for future position information
    cbSCALING  scalin;         ///< user-defined scaling information for AINP
    cbSCALING  scalout;        ///< user-defined scaling information for AOUT
    uint32_t     doutopts;       ///< digital output options (composed of cbDOUT_* flags)
    uint32_t     dinpopts;       ///< digital input options (composed of cbDINP_* flags)
    uint32_t     aoutopts;       ///< analog output options
    uint32_t     eopchar;        ///< digital input capablities (given by cbDINP_* flags)
    union {
        struct {    // separate system channel to instrument specific channel number
            uint16_t  moninst;        ///< instrument of channel to monitor
            uint16_t  monchan;        ///< channel to monitor
            int32_t   outvalue;       ///< output value
        };
        struct {    // used for digout timed output
            uint16_t  lowsamples;     ///< number of samples to set low for timed output
            uint16_t  highsamples;    ///< number of samples to set high for timed output
            int32_t   offset;         ///< number of samples to offset the transitions for timed output
        };
    };
    uint8_t               trigtype;       ///< trigger type (see cbDOUT_TRIGGER_*)
    uint8_t               reserved[2];    ///< 2 bytes reserved
    uint8_t               triginst;       ///< instrument of the trigger channel
    uint16_t              trigchan;       ///< trigger channel
    uint16_t              trigval;        ///< trigger value
    uint32_t              ainpopts;       ///< analog input options (composed of cbAINP* flags)
    uint32_t              lncrate;        ///< line noise cancellation filter adaptation rate
    uint32_t              smpfilter;      ///< continuous-time pathway filter id
    uint32_t              smpgroup;       ///< continuous-time pathway sample group
    int32_t               smpdispmin;     ///< continuous-time pathway display factor
    int32_t               smpdispmax;     ///< continuous-time pathway display factor
    uint32_t              spkfilter;      ///< spike pathway filter id
    int32_t               spkdispmax;     ///< spike pathway display factor
    int32_t               lncdispmax;     ///< Line Noise pathway display factor
    uint32_t              spkopts;        ///< spike processing options
    int32_t               spkthrlevel;    ///< spike threshold level
    int32_t               spkthrlimit;    ///<
    uint32_t              spkgroup;       ///< NTrodeGroup this electrode belongs to - 0 is single unit, non-0 indicates a multi-trode grouping
    int16_t               amplrejpos;     ///< Amplitude rejection positive value
    int16_t               amplrejneg;     ///< Amplitude rejection negative value
    uint32_t              refelecchan;    ///< Software reference electrode channel
    cbMANUALUNITMAPPING unitmapping[cbMAXUNITS];            ///< manual unit mapping
    cbHOOP              spkhoops[cbMAXUNITS][cbMAXHOOPS];   ///< spike hoop sorting set
} cbPKT_CHANINFO;

#define cbPKTDLEN_CHANINFO      ((sizeof(cbPKT_CHANINFO)/4) - cbPKT_HEADER_32SIZE)
#define cbPKTDLEN_CHANINFOSHORT (cbPKTDLEN_CHANINFO - ((sizeof(cbHOOP)*cbMAXUNITS*cbMAXHOOPS)/4))

/// @}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name Spike Data Packets
/// @{

#define cbPKTDLEN_SPK   ((sizeof(cbPKT_SPK)/4) - cbPKT_HEADER_32SIZE)
#define cbPKTDLEN_SPKSHORT (cbPKTDLEN_SPK - ((sizeof(int16_t)*cbMAX_PNTS)/4))

/// @brief Data packet - Spike waveform data
///
/// Detected spikes are sent through this packet.  The spike waveform may or may not be sent depending
/// on the dlen contained in the header.  The waveform can be anywhere from 30 samples to 128 samples
/// based on user configuration.  The default spike length is 48 samples.  cbpkt_header.chid is the
/// channel number of the spike.  cbpkt_header.type is the sorted unit number (0=unsorted, 255=noise).
typedef struct {
    cbPKT_HEADER cbpkt_header;  ///< in the header for this packet, the type is used as the unit number

    float  fPattern[3]; ///< values of the pattern space (Normal uses only 2, PCA uses third)
    int16_t  nPeak;       ///< highest datapoint of the waveform
    int16_t  nValley;     ///< lowest datapoint of the waveform

    int16_t  wave[cbMAX_PNTS];    ///< datapoints of each sample of the waveform. Room for all possible points collected
    ///< wave must be the last item in the structure because it can be variable length to a max of cbMAX_PNTS
} cbPKT_SPK;

/// @}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name Unit Selection
/// @{

// Unit selection masks
enum
{
    UNIT_UNCLASS_MASK = 0x01,       // mask to use to say unclassified units are selected
    UNIT_1_MASK = 0x02,       // mask to use to say unit 1 is selected
    UNIT_2_MASK = 0x04,       // mask to use to say unit 2 is selected
    UNIT_3_MASK = 0x08,       // mask to use to say unit 3 is selected
    UNIT_4_MASK = 0x10,       // mask to use to say unit 4 is selected
    UNIT_5_MASK = 0x20,       // mask to use to say unit 5 is selected
    CONTINUOUS_MASK = 0x40,       // mask to use to say the continuous signal is selected

    UNIT_ALL_MASK = UNIT_UNCLASS_MASK |
    UNIT_1_MASK |   // This means the channel is completely selected
    UNIT_2_MASK |
    UNIT_3_MASK |
    UNIT_4_MASK |
    UNIT_5_MASK |
    CONTINUOUS_MASK |
    0xFF80,         // this is here to select all digital input bits in raster when expanded
};

#define cbPKTDLEN_UNITSELECTION ((sizeof(cbPKT_UNIT_SELECTION) / 4) - cbPKT_HEADER_32SIZE)

/// @brief Unit Selection
///
/// Packet which says that these channels are now selected
typedef struct
{
    cbPKT_HEADER cbpkt_header;  ///< packet header

    int32_t  lastchan;         ///< Which channel was clicked last.
    uint16_t   abyUnitSelections[(cbPKT_MAX_SIZE - cbPKT_HEADER_SIZE - sizeof(int32_t))];     ///< one for each channel, channels are 0 based here, shows units selected
} cbPKT_UNIT_SELECTION;

/// @}

#ifdef __cplusplus
}
#endif

#pragma pack(pop)

#endif // CBPROTO_TYPES_H
