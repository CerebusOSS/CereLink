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

/// @brief Analog-to-digital data type
/// Used for continuous data samples in cbPKT_GROUP
typedef int16_t A2D_DATA;

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
/// Ground truth from upstream/cbproto/cbproto.h lines 193-211
/// @{

#define cbNET_UDP_ADDR_INST     "192.168.137.1"     ///< Cerebus default address
#define cbNET_UDP_ADDR_CNT      "192.168.137.128"   ///< Gemini NSP default control address
#define cbNET_UDP_ADDR_BCAST    "192.168.137.255"   ///< NSP default broadcast address
#define cbNET_UDP_PORT_BCAST    51002               ///< Neuroflow Data Port
#define cbNET_UDP_PORT_CNT      51001               ///< Neuroflow Control Port

// Maximum UDP datagram size used to transport cerebus packets, taken from MTU size
#define cbCER_UDP_SIZE_MAX      58080               ///< Note that multiple packets may reside in one udp datagram as aggregate

// Gemini network configuration
#define cbNET_TCP_PORT_GEMINI       51005             ///< Neuroflow Data Port
#define cbNET_TCP_ADDR_GEMINI_HUB   "192.168.137.200" ///< NSP default control address

#define cbNET_UDP_ADDR_HOST         "192.168.137.199"   ///< Cerebus (central) default address
#define cbNET_UDP_ADDR_GEMINI_NSP   "192.168.137.128"   ///< NSP default control address
#define cbNET_UDP_ADDR_GEMINI_HUB   "192.168.137.200"   ///< HUB default control address
#define cbNET_UDP_ADDR_GEMINI_HUB2  "192.168.137.201"   ///< HUB2 default control address
#define cbNET_UDP_ADDR_GEMINI_HUB3  "192.168.137.202"   ///< HUB3 default control address
#define cbNET_UDP_PORT_GEMINI_NSP   51001               ///< Gemini NSP Port
#define cbNET_UDP_PORT_GEMINI_HUB   51002               ///< Gemini HUB Port
#define cbNET_UDP_PORT_GEMINI_HUB2  51003               ///< Gemini HUB2 Port
#define cbNET_UDP_PORT_GEMINI_HUB3  51004               ///< Gemini HUB3 Port

// Protocol types
#define PROTOCOL_UDP        0   ///< UDP protocol
#define PROTOCOL_TCP        1   ///< TCP protocol

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

/// @brief Adaptation type enumeration
///
/// To control and keep track of how long an element of spike sorting has been adapting.
enum ADAPT_TYPE {
    ADAPT_NEVER,    ///< 0 - do not adapt at all
    ADAPT_ALWAYS,   ///< 1 - always adapt
    ADAPT_TIMED     ///< 2 - adapt if timer not timed out
};

/// @brief Adaptive Control structure
typedef struct {
    uint32_t nMode;           ///< 0-do not adapt at all, 1-always adapt, 2-adapt if timer not timed out
    float fTimeOutMinutes;    ///< how many minutes until time out
    float fElapsedMinutes;    ///< the amount of time that has elapsed
} cbAdaptControl;

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
#define cbPKTTYPE_SYSHEARTBEAT          0x00    ///< System heartbeat packet
#define cbPKTTYPE_SYSPROTOCOLMONITOR    0x01    ///< Protocol monitoring packet
#define cbPKTTYPE_PREVREPSTREAM         0x02    ///< Preview reply stream
#define cbPKTTYPE_PREVREP               0x03    ///< Preview reply
#define cbPKTTYPE_PREVREPLNC            0x04    ///< Preview reply LNC
#define cbPKTTYPE_REPCONFIGALL          0x08    ///< Response that NSP got your request
#define cbPKTTYPE_SYSREP                0x10    ///< System reply
#define cbPKTTYPE_SYSREPSPKLEN          0x11    ///< System reply spike length
#define cbPKTTYPE_SYSREPRUNLEV          0x12    ///< System reply runlevel

// Processor, bank, and filter information packets
#define cbPKTTYPE_PROCREP               0x21    ///< Processor information reply
#define cbPKTTYPE_BANKREP               0x22    ///< Bank information reply
#define cbPKTTYPE_FILTREP               0x23    ///< Filter information reply
#define cbPKTTYPE_CHANRESETREP          0x24    ///< Channel reset reply
#define cbPKTTYPE_ADAPTFILTREP          0x25    ///< Adaptive filter reply
#define cbPKTTYPE_REFELECFILTREP        0x26    ///< Reference electrode filter reply
#define cbPKTTYPE_REPNTRODEINFO         0x27    ///< N-Trode information reply
#define cbPKTTYPE_LNCREP                0x28    ///< LNC reply
#define cbPKTTYPE_VIDEOSYNCHREP         0x29    ///< Video sync reply

// Sample group and configuration packets
#define cbPKTTYPE_GROUPREP              0x30    ///< Sample group information reply
#define cbPKTTYPE_COMMENTREP            0x31    ///< Comment reply
#define cbPKTTYPE_NMREP                 0x32    ///< NeuroMotive (NM) reply
#define cbPKTTYPE_WAVEFORMREP           0x33    ///< Waveform reply
#define cbPKTTYPE_STIMULATIONREP        0x34    ///< Stimulation reply

// Channel information packets - Reply (0x40-0x4F)
#define cbPKTTYPE_CHANREP               0x40    ///< Channel information reply
#define cbPKTTYPE_CHANREPLABEL          0x41    ///< Channel label reply
#define cbPKTTYPE_CHANREPSCALE          0x42    ///< Channel scale reply
#define cbPKTTYPE_CHANREPDOUT           0x43    ///< Channel digital output reply
#define cbPKTTYPE_CHANREPDINP           0x44    ///< Channel digital input reply
#define cbPKTTYPE_CHANREPAOUT           0x45    ///< Channel analog output reply
#define cbPKTTYPE_CHANREPDISP           0x46    ///< Channel display reply
#define cbPKTTYPE_CHANREPAINP           0x47    ///< Channel analog input reply
#define cbPKTTYPE_CHANREPSMP            0x48    ///< Channel sampling reply
#define cbPKTTYPE_CHANREPSPK            0x49    ///< Channel spike reply
#define cbPKTTYPE_CHANREPSPKTHR         0x4A    ///< Channel spike threshold reply
#define cbPKTTYPE_CHANREPSPKHPS         0x4B    ///< Channel spike hoops reply
#define cbPKTTYPE_CHANREPUNITOVERRIDES  0x4C    ///< Channel unit overrides reply
#define cbPKTTYPE_CHANREPNTRODEGROUP    0x4D    ///< Channel n-trode group reply
#define cbPKTTYPE_CHANREPREJECTAMPLITUDE 0x4E   ///< Channel reject amplitude reply
#define cbPKTTYPE_CHANREPAUTOTHRESHOLD  0x4F    ///< Channel auto threshold reply

// Spike sorting packets - Reply (0x50-0x5F)
#define cbPKTTYPE_SS_MODELALLREP        0x50    ///< Spike sorting model all reply
#define cbPKTTYPE_SS_MODELREP           0x51    ///< Spike sorting model reply
#define cbPKTTYPE_SS_DETECTREP          0x52    ///< Spike sorting detect reply
#define cbPKTTYPE_SS_ARTIF_REJECTREP    0x53    ///< Spike sorting artifact reject reply
#define cbPKTTYPE_SS_NOISE_BOUNDARYREP  0x54    ///< Spike sorting noise boundary reply
#define cbPKTTYPE_SS_STATISTICSREP      0x55    ///< Spike sorting statistics reply
#define cbPKTTYPE_SS_RESETREP           0x56    ///< Spike sorting reset reply
#define cbPKTTYPE_SS_STATUSREP          0x57    ///< Spike sorting status reply
#define cbPKTTYPE_SS_RESET_MODEL_REP    0x58    ///< Spike sorting reset model reply
#define cbPKTTYPE_SS_RECALCREP          0x59    ///< Spike sorting recalculate reply
#define cbPKTTYPE_FS_BASISREP           0x5B    ///< Feature space basis reply
#define cbPKTTYPE_NPLAYREP              0x5C    ///< NPlay reply
#define cbPKTTYPE_SET_DOUTREP           0x5D    ///< Set digital output reply
#define cbPKTTYPE_TRIGGERREP            0x5E    ///< Trigger reply
#define cbPKTTYPE_VIDEOTRACKREP         0x5F    ///< Video track reply

// File and configuration packets - Reply (0x60-0x6F)
#define cbPKTTYPE_REPFILECFG            0x61    ///< File configuration reply
#define cbPKTTYPE_REPUNITSELECTION      0x62    ///< Unit selection reply
#define cbPKTTYPE_LOGREP                0x63    ///< Log reply
#define cbPKTTYPE_REPPATIENTINFO        0x64    ///< Patient info reply
#define cbPKTTYPE_REPIMPEDANCE          0x65    ///< Impedance reply
#define cbPKTTYPE_REPINITIMPEDANCE      0x66    ///< Initial impedance reply
#define cbPKTTYPE_REPPOLL               0x67    ///< Poll reply
#define cbPKTTYPE_REPMAPFILE            0x68    ///< Map file reply

// Update packets
#define cbPKTTYPE_UPDATEREP             0x71    ///< Update reply

// Preview packets - Set
#define cbPKTTYPE_PREVSETSTREAM         0x82    ///< Preview set stream
#define cbPKTTYPE_PREVSET               0x83    ///< Preview set
#define cbPKTTYPE_PREVSETLNC            0x84    ///< Preview set LNC

// System packets - Request (0x88-0x9F)
#define cbPKTTYPE_REQCONFIGALL          0x88    ///< Request for ALL configuration information
#define cbPKTTYPE_SYSSET                0x90    ///< System set
#define cbPKTTYPE_SYSSETSPKLEN          0x91    ///< System set spike length
#define cbPKTTYPE_SYSSETRUNLEV          0x92    ///< System set runlevel

// Filter and configuration packets - Set (0xA0-0xAF)
#define cbPKTTYPE_FILTSET               0xA3    ///< Filter set
#define cbPKTTYPE_CHANRESET             0xA4    ///< Channel reset
#define cbPKTTYPE_ADAPTFILTSET          0xA5    ///< Adaptive filter set
#define cbPKTTYPE_REFELECFILTSET        0xA6    ///< Reference electrode filter set
#define cbPKTTYPE_SETNTRODEINFO         0xA7    ///< N-Trode information set
#define cbPKTTYPE_LNCSET                0xA8    ///< LNC set
#define cbPKTTYPE_VIDEOSYNCHSET         0xA9    ///< Video sync set

// Sample group and waveform packets - Set (0xB0-0xBF)
#define cbPKTTYPE_GROUPSET              0xB0    ///< Sample group set
#define cbPKTTYPE_COMMENTSET            0xB1    ///< Comment set
#define cbPKTTYPE_NMSET                 0xB2    ///< NeuroMotive set
#define cbPKTTYPE_WAVEFORMSET           0xB3    ///< Waveform set
#define cbPKTTYPE_STIMULATIONSET        0xB4    ///< Stimulation set

// Channel information packets - Set (0xC0-0xCF)
#define cbPKTTYPE_CHANSET               0xC0    ///< Channel information set
#define cbPKTTYPE_CHANSETLABEL          0xC1    ///< Channel label set
#define cbPKTTYPE_CHANSETSCALE          0xC2    ///< Channel scale set
#define cbPKTTYPE_CHANSETDOUT           0xC3    ///< Channel digital output set
#define cbPKTTYPE_CHANSETDINP           0xC4    ///< Channel digital input set
#define cbPKTTYPE_CHANSETAOUT           0xC5    ///< Channel analog output set
#define cbPKTTYPE_CHANSETDISP           0xC6    ///< Channel display set
#define cbPKTTYPE_CHANSETAINP           0xC7    ///< Channel analog input set
#define cbPKTTYPE_CHANSETSMP            0xC8    ///< Channel sampling set
#define cbPKTTYPE_CHANSETSPK            0xC9    ///< Channel spike set
#define cbPKTTYPE_CHANSETSPKTHR         0xCA    ///< Channel spike threshold set
#define cbPKTTYPE_CHANSETSPKHPS         0xCB    ///< Channel spike hoops set
#define cbPKTTYPE_CHANSETUNITOVERRIDES  0xCC    ///< Channel unit overrides set
#define cbPKTTYPE_CHANSETNTRODEGROUP    0xCD    ///< Channel n-trode group set
#define cbPKTTYPE_CHANSETREJECTAMPLITUDE 0xCE   ///< Channel reject amplitude set
#define cbPKTTYPE_CHANSETAUTOTHRESHOLD  0xCF    ///< Channel auto threshold set

// Spike sorting packets - Set (0xD0-0xDF)
#define cbPKTTYPE_SS_MODELALLSET        0xD0    ///< Spike sorting model all set
#define cbPKTTYPE_SS_MODELSET           0xD1    ///< Spike sorting model set
#define cbPKTTYPE_SS_DETECTSET          0xD2    ///< Spike sorting detect set
#define cbPKTTYPE_SS_ARTIF_REJECTSET    0xD3    ///< Spike sorting artifact reject set
#define cbPKTTYPE_SS_NOISE_BOUNDARYSET  0xD4    ///< Spike sorting noise boundary set
#define cbPKTTYPE_SS_STATISTICSSET      0xD5    ///< Spike sorting statistics set
#define cbPKTTYPE_SS_RESETSET           0xD6    ///< Spike sorting reset set
#define cbPKTTYPE_SS_STATUSSET          0xD7    ///< Spike sorting status set
#define cbPKTTYPE_SS_RESET_MODEL_SET    0xD8    ///< Spike sorting reset model set
#define cbPKTTYPE_SS_RECALCSET          0xD9    ///< Spike sorting recalculate set
#define cbPKTTYPE_FS_BASISSET           0xDB    ///< Feature space basis set
#define cbPKTTYPE_NPLAYSET              0xDC    ///< NPlay set
#define cbPKTTYPE_SET_DOUTSET           0xDD    ///< Set digital output set
#define cbPKTTYPE_TRIGGERSET            0xDE    ///< Trigger set
#define cbPKTTYPE_VIDEOTRACKSET         0xDF    ///< Video track set

// Packet type masks and conversion constants
#define cbPKTTYPE_MASKED_REFLECTED              0xE0    ///< Masked reflected packet type
#define cbPKTTYPE_COMPARE_MASK_REFLECTED        0xF0    ///< Compare mask for reflected packets
#define cbPKTTYPE_REFLECTED_CONVERSION_MASK     0x7F    ///< Reflected conversion mask

// File and configuration packets - Set (0xE0-0xEF)
#define cbPKTTYPE_SETFILECFG            0xE1    ///< File configuration set
#define cbPKTTYPE_SETUNITSELECTION      0xE2    ///< Unit selection set
#define cbPKTTYPE_LOGSET                0xE3    ///< Log set
#define cbPKTTYPE_SETPATIENTINFO        0xE4    ///< Patient info set
#define cbPKTTYPE_SETIMPEDANCE          0xE5    ///< Impedance set
#define cbPKTTYPE_SETINITIMPEDANCE      0xE6    ///< Initial impedance set
#define cbPKTTYPE_SETPOLL               0xE7    ///< Poll set
#define cbPKTTYPE_SETMAPFILE            0xE8    ///< Map file set

// Update packets - Set
#define cbPKTTYPE_UPDATESET             0xF1    ///< Update set

/// @}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name Channel Constants
/// @{

#define cbPKTCHAN_CONFIGURATION         0x8000  ///< Channel # to mean configuration

/// @}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name Channel Capability Flags
///
/// Ground truth from upstream/cbproto/cbproto.h lines 554-562
/// These flags define the capabilities of each channel
/// @{

#define cbCHAN_EXISTS       0x00000001  ///< Channel id is allocated
#define cbCHAN_CONNECTED    0x00000002  ///< Channel is connected and mapped and ready to use
#define cbCHAN_ISOLATED     0x00000004  ///< Channel is electrically isolated
#define cbCHAN_AINP         0x00000100  ///< Channel has analog input capabilities
#define cbCHAN_AOUT         0x00000200  ///< Channel has analog output capabilities
#define cbCHAN_DINP         0x00000400  ///< Channel has digital input capabilities
#define cbCHAN_DOUT         0x00000800  ///< Channel has digital output capabilities
#define cbCHAN_GYRO         0x00001000  ///< Channel has gyroscope/accelerometer/magnetometer/temperature capabilities

/// @}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name Digital Input Capability and Option Flags
///
/// Ground truth from upstream/cbproto/cbproto.h lines 569-590
/// @{

#define  cbDINP_SERIALMASK   0x000000FF  ///< Bit mask used to detect RS232 Serial Baud Rates
#define  cbDINP_BAUD2400     0x00000001  ///< RS232 Serial Port operates at 2400   (n-8-1)
#define  cbDINP_BAUD9600     0x00000002  ///< RS232 Serial Port operates at 9600   (n-8-1)
#define  cbDINP_BAUD19200    0x00000004  ///< RS232 Serial Port operates at 19200  (n-8-1)
#define  cbDINP_BAUD38400    0x00000008  ///< RS232 Serial Port operates at 38400  (n-8-1)
#define  cbDINP_BAUD57600    0x00000010  ///< RS232 Serial Port operates at 57600  (n-8-1)
#define  cbDINP_BAUD115200   0x00000020  ///< RS232 Serial Port operates at 115200 (n-8-1)
#define  cbDINP_1BIT         0x00000100  ///< Port has a single input bit (eg single BNC input)
#define  cbDINP_8BIT         0x00000200  ///< Port has 8 input bits
#define  cbDINP_16BIT        0x00000400  ///< Port has 16 input bits
#define  cbDINP_32BIT        0x00000800  ///< Port has 32 input bits
#define  cbDINP_ANYBIT       0x00001000  ///< Capture the port value when any bit changes.
#define  cbDINP_WRDSTRB      0x00002000  ///< Capture the port when a word-write line is strobed
#define  cbDINP_PKTCHAR      0x00004000  ///< Capture packets using an End of Packet Character
#define  cbDINP_PKTSTRB      0x00008000  ///< Capture packets using an End of Packet Logic Input
#define  cbDINP_MONITOR      0x00010000  ///< Port controls other ports or system events
#define  cbDINP_REDGE        0x00020000  ///< Capture the port value when any bit changes lo-2-hi (rising edge)
#define  cbDINP_FEDGE        0x00040000  ///< Capture the port value when any bit changes hi-2-lo (falling edge)
#define  cbDINP_STRBANY      0x00080000  ///< Capture packets using 8-bit strobe/8-bit any Input
#define  cbDINP_STRBRIS      0x00100000  ///< Capture packets using 8-bit strobe/8-bit rising edge Input
#define  cbDINP_STRBFAL      0x00200000  ///< Capture packets using 8-bit strobe/8-bit falling edge Input
#define  cbDINP_MASK         (cbDINP_ANYBIT | cbDINP_WRDSTRB | cbDINP_PKTCHAR | cbDINP_PKTSTRB | cbDINP_MONITOR | cbDINP_REDGE | cbDINP_FEDGE | cbDINP_STRBANY | cbDINP_STRBRIS | cbDINP_STRBFAL)

/// @}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name Digital Output Capability and Option Flags
///
/// Ground truth from upstream/cbproto/cbproto.h lines 599-630
/// @{

#define  cbDOUT_SERIALMASK          0x000000FF  ///< Port operates as an RS232 Serial Connection
#define  cbDOUT_BAUD2400            0x00000001  ///< Serial Port operates at 2400   (n-8-1)
#define  cbDOUT_BAUD9600            0x00000002  ///< Serial Port operates at 9600   (n-8-1)
#define  cbDOUT_BAUD19200           0x00000004  ///< Serial Port operates at 19200  (n-8-1)
#define  cbDOUT_BAUD38400           0x00000008  ///< Serial Port operates at 38400  (n-8-1)
#define  cbDOUT_BAUD57600           0x00000010  ///< Serial Port operates at 57600  (n-8-1)
#define  cbDOUT_BAUD115200          0x00000020  ///< Serial Port operates at 115200 (n-8-1)
#define  cbDOUT_1BIT                0x00000100  ///< Port has a single output bit (eg single BNC output)
#define  cbDOUT_8BIT                0x00000200  ///< Port has 8 output bits
#define  cbDOUT_16BIT               0x00000400  ///< Port has 16 output bits
#define  cbDOUT_32BIT               0x00000800  ///< Port has 32 output bits
#define  cbDOUT_VALUE               0x00010000  ///< Port can be manually configured
#define  cbDOUT_TRACK               0x00020000  ///< Port should track the most recently selected channel
#define  cbDOUT_FREQUENCY           0x00040000  ///< Port can output a frequency
#define  cbDOUT_TRIGGERED           0x00080000  ///< Port can be triggered
#define  cbDOUT_MONITOR_UNIT0       0x01000000  ///< Can monitor unit 0 = UNCLASSIFIED
#define  cbDOUT_MONITOR_UNIT1       0x02000000  ///< Can monitor unit 1
#define  cbDOUT_MONITOR_UNIT2       0x04000000  ///< Can monitor unit 2
#define  cbDOUT_MONITOR_UNIT3       0x08000000  ///< Can monitor unit 3
#define  cbDOUT_MONITOR_UNIT4       0x10000000  ///< Can monitor unit 4
#define  cbDOUT_MONITOR_UNIT5       0x20000000  ///< Can monitor unit 5
#define  cbDOUT_MONITOR_UNIT_ALL    0x3F000000  ///< Can monitor ALL units
#define  cbDOUT_MONITOR_SHIFT_TO_FIRST_UNIT 24  ///< This tells us how many bit places to get to unit 1

// Trigger types for Digital Output channels
#define  cbDOUT_TRIGGER_NONE            0   ///< instant software trigger
#define  cbDOUT_TRIGGER_DINPRISING      1   ///< digital input rising edge trigger
#define  cbDOUT_TRIGGER_DINPFALLING     2   ///< digital input falling edge trigger
#define  cbDOUT_TRIGGER_SPIKEUNIT       3   ///< spike unit
#define  cbDOUT_TRIGGER_NM              4   ///< comment RGBA color (A being big byte)
#define  cbDOUT_TRIGGER_RECORDINGSTART  5   ///< recording start trigger
#define  cbDOUT_TRIGGER_EXTENSION       6   ///< extension trigger

/// @}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name Analog Input Capability and Option Flags
///
/// Ground truth from upstream/cbproto/cbproto.h lines 696-723
/// @{

#define  cbAINP_RAWPREVIEW          0x00000001      ///< Generate scrolling preview data for the raw channel
#define  cbAINP_LNC                 0x00000002      ///< Line Noise Cancellation
#define  cbAINP_LNCPREVIEW          0x00000004      ///< Retrieve the LNC correction waveform
#define  cbAINP_SMPSTREAM           0x00000010      ///< stream the analog input stream directly to disk
#define  cbAINP_SMPFILTER           0x00000020      ///< Digitally filter the analog input stream
#define  cbAINP_RAWSTREAM           0x00000040      ///< Raw data stream available
#define  cbAINP_SPKSTREAM           0x00000100      ///< Spike Stream is available
#define  cbAINP_SPKFILTER           0x00000200      ///< Selectable Filters
#define  cbAINP_SPKPREVIEW          0x00000400      ///< Generate scrolling preview of the spike channel
#define  cbAINP_SPKPROC             0x00000800      ///< Channel is able to do online spike processing
#define  cbAINP_OFFSET_CORRECT_CAP  0x00001000      ///< Offset correction mode (0-disabled 1-enabled)

#define  cbAINP_LNC_OFF             0x00000000      ///< Line Noise Cancellation disabled
#define  cbAINP_LNC_RUN_HARD        0x00000001      ///< Hardware-based LNC running and adapting according to the adaptation const
#define  cbAINP_LNC_RUN_SOFT        0x00000002      ///< Software-based LNC running and adapting according to the adaptation const
#define  cbAINP_LNC_HOLD            0x00000004      ///< LNC running, but not adapting
#define  cbAINP_LNC_MASK            0x00000007      ///< Mask for LNC Flags
#define  cbAINP_REFELEC_LFPSPK      0x00000010      ///< Apply reference electrode to LFP & Spike
#define  cbAINP_REFELEC_SPK         0x00000020      ///< Apply reference electrode to Spikes only
#define  cbAINP_REFELEC_MASK        0x00000030      ///< Mask for Reference Electrode flags
#define  cbAINP_RAWSTREAM_ENABLED   0x00000040      ///< Raw data stream enabled
#define  cbAINP_OFFSET_CORRECT      0x00000100      ///< Offset correction mode (0-disabled 1-enabled)

// Preview request packet identifiers
#define cbAINPPREV_LNC    0x81
#define cbAINPPREV_STREAM 0x82
#define cbAINPPREV_ALL    0x83

/// @}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name Analog Input Spike Processing Flags
///
/// Ground truth from upstream/cbproto/cbproto.h lines 728-749
/// @{

#define  cbAINPSPK_EXTRACT      0x00000001  ///< Time-stamp and packet to first superthreshold peak
#define  cbAINPSPK_REJART       0x00000002  ///< Reject around clipped signals on multiple channels
#define  cbAINPSPK_REJCLIP      0x00000004  ///< Reject clipped signals on the channel
#define  cbAINPSPK_ALIGNPK      0x00000008  ///<
#define  cbAINPSPK_REJAMPL      0x00000010  ///< Reject based on amplitude
#define  cbAINPSPK_THRLEVEL     0x00000100  ///< Analog level threshold detection
#define  cbAINPSPK_THRENERGY    0x00000200  ///< Energy threshold detection
#define  cbAINPSPK_THRAUTO      0x00000400  ///< Auto threshold detection
#define  cbAINPSPK_SPREADSORT   0x00001000  ///< Enable Auto spread Sorting
#define  cbAINPSPK_CORRSORT     0x00002000  ///< Enable Auto Histogram Correlation Sorting
#define  cbAINPSPK_PEAKMAJSORT  0x00004000  ///< Enable Auto Histogram Peak Major Sorting
#define  cbAINPSPK_PEAKFISHSORT 0x00008000  ///< Enable Auto Histogram Peak Fisher Sorting
#define  cbAINPSPK_HOOPSORT     0x00010000  ///< Enable Manual Hoop Sorting
#define  cbAINPSPK_PCAMANSORT   0x00020000  ///< Enable Manual PCA Sorting
#define  cbAINPSPK_PCAKMEANSORT 0x00040000  ///< Enable K-means PCA Sorting
#define  cbAINPSPK_PCAEMSORT    0x00080000  ///< Enable EM-clustering PCA Sorting
#define  cbAINPSPK_PCADBSORT    0x00100000  ///< Enable DBSCAN PCA Sorting
#define  cbAINPSPK_AUTOSORT     (cbAINPSPK_SPREADSORT | cbAINPSPK_CORRSORT | cbAINPSPK_PEAKMAJSORT | cbAINPSPK_PEAKFISHSORT) ///< old auto sorting methods
#define  cbAINPSPK_NOSORT       0x00000000  ///< No sorting
#define  cbAINPSPK_PCAAUTOSORT  (cbAINPSPK_PCAKMEANSORT | cbAINPSPK_PCAEMSORT | cbAINPSPK_PCADBSORT)  ///< All PCA sorting auto algorithms
#define  cbAINPSPK_PCASORT      (cbAINPSPK_PCAMANSORT | cbAINPSPK_PCAAUTOSORT)  ///< All PCA sorting algorithms
#define  cbAINPSPK_ALLSORT      (cbAINPSPK_AUTOSORT | cbAINPSPK_HOOPSORT | cbAINPSPK_PCASORT) ///< All sorting algorithms

/// @}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name Analog Output Capability Flags
///
/// Ground truth from upstream/cbproto/cbproto.h lines 768-778
/// @{

#define  cbAOUT_AUDIO        0x00000001  ///< Channel is physically optimized for audio output
#define  cbAOUT_SCALE        0x00000002  ///< Output a static value
#define  cbAOUT_TRACK        0x00000004  ///< Output a static value
#define  cbAOUT_STATIC       0x00000008  ///< Output a static value
#define  cbAOUT_MONITORRAW   0x00000010  ///< Monitor an analog signal line - RAW data
#define  cbAOUT_MONITORLNC   0x00000020  ///< Monitor an analog signal line - Line Noise Cancelation
#define  cbAOUT_MONITORSMP   0x00000040  ///< Monitor an analog signal line - Continuous
#define  cbAOUT_MONITORSPK   0x00000080  ///< Monitor an analog signal line - spike
#define  cbAOUT_STIMULATE    0x00000100  ///< Stimulation waveform functions are available.
#define  cbAOUT_WAVEFORM     0x00000200  ///< Custom Waveform
#define  cbAOUT_EXTENSION    0x00000400  ///< Output Waveform from Extension

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
/// @name Continuous Data Packets
/// @{

/// @brief Data packet - Sample Group data packet
///
/// This packet contains each sample for the specified group. The group is specified in the type member of the
/// header. Groups are currently 1=500S/s, 2=1kS/s, 3=2kS/s, 4=10kS/s, 5=30kS/s, 6=raw (30kS/s no filter). The
/// list of channels associated with each group is transmitted using the cbPKT_GROUPINFO when the list of channels
/// changes. cbpkt_header.chid is always zero. cbpkt_header.type is the group number
typedef struct {
    cbPKT_HEADER cbpkt_header;  ///< packet header
    A2D_DATA data[cbNUM_ANALOG_CHANS];    ///< variable length address list
} cbPKT_GROUP;

/// @}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name Digital Input/Output Data Packets
/// @{

#define DINP_EVENT_ANYBIT   0x00000001  ///< Digital input event: any bit changed
#define DINP_EVENT_STROBE   0x00000002  ///< Digital input event: strobe detected

/// @brief Data packet - Digital input data value
///
/// This packet is sent when a digital input value has met the criteria set in Hardware Configuration.
typedef struct {
    cbPKT_HEADER cbpkt_header;  ///< packet header
    uint32_t valueRead;         ///< data read from the digital input port
    uint32_t bitsChanged;       ///< bits that have changed from the last packet sent
    uint32_t eventType;         ///< type of event, eg DINP_EVENT_ANYBIT, DINP_EVENT_STROBE
} cbPKT_DINP;

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
