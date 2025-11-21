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
#define cbFIRST_DIGITAL_FILTER  13  ///< (0-based) filter number, must be less than cbMAXFILTS
#define cbNUM_DIGITAL_FILTERS   4   ///< Number of custom digital filters
#define cbMAXHOOPS      4   ///< Maximum number of hoops for spike sorting
#define cbMAXSITES      4   ///< Maximum number of electrodes in an n-trode group
#define cbMAXSITEPLOTS  ((cbMAXSITES - 1) * cbMAXSITES / 2)  ///< Combination of 2 out of n
#define cbMAXUNITS      5   ///< Maximum number of sorted units per channel
#define cbMAXNTRODES    (cbNUM_ANALOG_CHANS / 2)  ///< Maximum n-trodes (stereotrode minimum)
#define cbMAX_PNTS      128 ///< Maximum spike waveform points
#define cbMAXVIDEOSOURCE 1  ///< Maximum number of video sources
#define cbMAXTRACKOBJ   20  ///< Maximum number of trackable objects
#define cbMAX_AOUT_TRIGGER 5 ///< Maximum number of per-channel (analog output, or digital output) triggers

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

/// @brief Number of analog/audio output channels with gain
/// This is the number of AOUT channels with gain. Conveniently, the 4 Analog Outputs
/// and the 2 Audio Outputs are right next to each other in the channel numbering sequence.
#define AOUT_NUM_GAIN_CHANS     (cbNUM_ANAOUT_CHANS + cbNUM_AUDOUT_CHANS)

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
#define cbMAX_COMMENT           128     ///< Maximum comment length (must be multiple of 4)

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
/// @name Old Packet Header and Generic (for CCF file compatibility)
///
/// Ground truth from upstream/cbproto/cbproto.h lines 385-420
/// @{

/// @brief Old Cerebus packet header data structure
///
/// This is used to read old CCF files
typedef struct {
    uint32_t time;    ///< system clock timestamp
    uint16_t chid;    ///< channel identifier
    uint8_t  type;    ///< packet type
    uint8_t  dlen;    ///< length of data field in 32-bit chunks
} cbPKT_HEADER_OLD;

#define cbPKT_HEADER_SIZE_OLD       sizeof(cbPKT_HEADER_OLD)    ///< define the size of the old packet header in bytes

/// @brief Old Generic Cerebus packet data structure (1024 bytes total)
///
/// This is used to read old CCF files
typedef struct {
    cbPKT_HEADER_OLD cbpkt_header;

    uint32_t data[(cbPKT_MAX_SIZE - cbPKT_HEADER_SIZE_OLD) / 4];   ///< data buffer (up to 1016 bytes)
} cbPKT_GENERIC_OLD;

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

#define cbPKTTYPE_SYSHEARTBEAT    0x00
#define cbPKTDLEN_SYSHEARTBEAT    ((sizeof(cbPKT_SYSHEARTBEAT)/4) - cbPKT_HEADER_32SIZE)
#define HEARTBEAT_MS              10

/// @brief PKT Set:N/A  Rep:0x00 - System Heartbeat packet
///
/// Ground truth from upstream/cbproto/cbproto.h lines 903-914
/// This is sent every 10ms
typedef struct {
    cbPKT_HEADER cbpkt_header;  ///< packet header
} cbPKT_SYSHEARTBEAT;

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

/// @brief Old system info packet
///
/// Ground truth from upstream/cbproto/cbproto.h lines 1086-1099
/// Used for backward compatibility with old CCF files
#define cbPKTDLEN_OLDSYSINFO       ((sizeof(cbPKT_OLDSYSINFO)/4) - 2)

typedef struct {
    uint32_t time;        ///< system clock timestamp
    uint16_t chid;        ///< 0x8000
    uint8_t type;      ///< PKTTYPE_SYS*
    uint8_t dlen;      ///< cbPKT_OLDSYSINFODLEN

    uint32_t sysfreq;     ///< System clock frequency in Hz
    uint32_t spikelen;    ///< The length of the spike events
    uint32_t spikepre;    ///< Spike pre-trigger samples
    uint32_t resetque;    ///< The channel for the reset to que on
    uint32_t runlevel;    ///< System runlevel
    uint32_t runflags;
} cbPKT_OLDSYSINFO;

/// @brief PKT Set:N/A  Rep:0x01 - System protocol monitor
///
/// Packets are sent via UDP. This packet is sent by the NSP periodically (approximately every 10ms)
/// telling the client how many packets have been sent since the last cbPKT_SYSPROTOCOLMONITOR.
/// The client can compare this with the number of packets it has received to detect packet loss.
///
/// Ground truth from upstream/cbproto/cbproto.h lines 1048-1055
///
typedef struct {
    cbPKT_HEADER cbpkt_header;  ///< packet header

    uint32_t sentpkts;    ///< Packets sent since last cbPKT_SYSPROTOCOLMONITOR (or 0 if timestamp=0)
                          ///< The cbPKT_SYSPROTOCOLMONITOR packets are counted as well so this must be >= 1
    uint32_t counter;     ///< Counter of cbPKT_SYSPROTOCOLMONITOR packets sent since beginning of NSP time
} cbPKT_SYSPROTOCOLMONITOR;

#define cbPKTDLEN_SYSPROTOCOLMONITOR    ((sizeof(cbPKT_SYSPROTOCOLMONITOR)/4) - cbPKT_HEADER_32SIZE)

/// @brief PKT Set:0xB1 Rep:0x31 - Comment annotation packet
///
/// This packet injects a comment into the data stream which gets recorded in the file and displayed on Raster.
typedef struct {
    cbPKT_HEADER cbpkt_header;  //!< packet header

    struct {
        uint8_t   charset;        //!< Character set (0 - ANSI, 1 - UTF16, 255 - NeuroMotive ANSI)
        uint8_t   reserved[3];    //!< Reserved (must be 0)
    } info;
    PROCTIME timeStarted;       //!< Start time of when the user started typing the comment
    uint32_t  rgba;               //!< rgba to color the comment
    char   comment[cbMAX_COMMENT]; //!< Comment
} cbPKT_COMMENT;

#define cbPKTDLEN_COMMENT       ((sizeof(cbPKT_COMMENT)/4) - cbPKT_HEADER_32SIZE)
#define cbPKTDLEN_COMMENTSHORT  (cbPKTDLEN_COMMENT - ((sizeof(uint8_t)*cbMAX_COMMENT)/4))

/// @}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name Preview Packets
///
/// Ground truth from upstream/cbproto/cbproto.h lines 2039-2084
/// @{

// PCA collection states
#define cbPCA_START_COLLECTION            0  ///< start collecting samples
#define cbPCA_START_BASIS                 1  ///< start basis calculation
#define cbPCA_MANUAL_LAST_SAMPLE          2  ///< the manual-only PCA, samples at zero, calculates PCA basis at 1 and stops at 2

// Stream preview flags
#define cbSTREAMPREV_NONE               0x00000000
#define cbSTREAMPREV_PCABASIS_NONEMPTY  0x00000001

#define cbPKTTYPE_PREVREPSTREAM  0x02
#define cbPKTDLEN_PREVREPSTREAM  ((sizeof(cbPKT_STREAMPREV)/4) - cbPKT_HEADER_32SIZE)

/// @brief Preview packet
///
/// Sends preview of various data points. This is sent every 10ms
typedef struct {
    cbPKT_HEADER cbpkt_header;  ///< packet header

    int16_t  rawmin;      ///< minimum raw channel value over last preview period
    int16_t  rawmax;      ///< maximum raw channel value over last preview period
    int16_t  smpmin;      ///< minimum sample channel value over last preview period
    int16_t  smpmax;      ///< maximum sample channel value over last preview period
    int16_t  spkmin;      ///< minimum spike channel value over last preview period
    int16_t  spkmax;      ///< maximum spike channel value over last preview period
    uint32_t spkmos;      ///< mean of squares
    uint32_t eventflag;   ///< flag to detail the units that happend in the last sample period
    int16_t  envmin;      ///< minimum envelope channel value over the last preview period
    int16_t  envmax;      ///< maximum envelope channel value over the last preview period
    int32_t  spkthrlevel; ///< preview of spike threshold level
    uint32_t nWaveNum;    ///< this tracks the number of waveforms collected in the WCM for each channel
    uint32_t nSampleRows; ///< tracks number of sample vectors of waves
    uint32_t nFlags;      ///< cbSTREAMPREV_*
} cbPKT_STREAMPREV;

#define cbPKTTYPE_PREVREPLNC    0x04
#define cbPKTDLEN_PREVREPLNC    ((sizeof(cbPKT_LNCPREV)/4) - cbPKT_HEADER_32SIZE)

/// @brief Preview packet - Line Noise preview
///
/// Sends a preview of the line noise waveform.
typedef struct {
    cbPKT_HEADER cbpkt_header;  ///< packet header

    uint32_t freq;        ///< Estimated line noise frequency * 1000 (zero means not valid)
    int16_t  wave[300];   ///< lnc cancellation waveform (downsampled by 2)
} cbPKT_LNCPREV;

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

/// @brief Gyro Data packet - Gyro input data value.
///
/// Ground truth from upstream/cbproto/cbproto.h lines 890-901
/// This packet is sent when gyro data has changed.
typedef struct {
    cbPKT_HEADER cbpkt_header;  ///< packet header

    uint8_t gyroscope[4];       ///< X, Y, Z values read from the gyroscope, last byte set to zero
    uint8_t accelerometer[4];   ///< X, Y, Z values read from the accelerometer, last byte set to zero
    uint8_t magnetometer[4];    ///< X, Y, Z values read from the magnetometer, last byte set to zero
    uint16_t temperature;       ///< temperature data
    uint16_t reserved;          ///< set to zero
} cbPKT_GYRO;

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

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name Channel Reset
///
/// Ground truth from upstream/cbproto/cbproto.h lines 1264-1308
/// @{

#define cbPKTTYPE_CHANRESETREP  0x24        ///< NSP->PC response...ignore all values
#define cbPKTTYPE_CHANRESET     0xA4        ///< PC->NSP request
#define cbPKTDLEN_CHANRESET ((sizeof(cbPKT_CHANRESET) / 4) - cbPKT_HEADER_32SIZE)

/// @brief PKT Set:0xA4 Rep:0x24 - Channel reset packet
///
/// This resets various aspects of a channel.  For each member, 0 doesn't change the value, any non-zero value resets
/// the property to factory defaults
/// This is currently not used in the system.
typedef struct {
    cbPKT_HEADER cbpkt_header;  ///< packet header

    uint32_t chan;           ///< actual channel id of the channel being configured

    // For all of the values that follow
    // 0 = NOT change value; nonzero = reset to factory defaults

    uint8_t  label;          ///< Channel label
    uint8_t  userflags;      ///< User flags for the channel state
    uint8_t  position;       ///< reserved for future position information
    uint8_t  scalin;         ///< user-defined scaling information
    uint8_t  scalout;        ///< user-defined scaling information
    uint8_t  doutopts;       ///< digital output options (composed of cbDOUT_* flags)
    uint8_t  dinpopts;       ///< digital input options (composed of cbDINP_* flags)
    uint8_t  aoutopts;       ///< analog output options
    uint8_t  eopchar;        ///< the end of packet character
    uint8_t  moninst;        ///< instrument number of channel to monitor
    uint8_t  monchan;        ///< channel to monitor
    uint8_t  outvalue;       ///< output value
    uint8_t  ainpopts;       ///< analog input options (composed of cbAINP_* flags)
    uint8_t  lncrate;        ///< line noise cancellation filter adaptation rate
    uint8_t  smpfilter;      ///< continuous-time pathway filter id
    uint8_t  smpgroup;       ///< continuous-time pathway sample group
    uint8_t  smpdispmin;     ///< continuous-time pathway display factor
    uint8_t  smpdispmax;     ///< continuous-time pathway display factor
    uint8_t  spkfilter;      ///< spike pathway filter id
    uint8_t  spkdispmax;     ///< spike pathway display factor
    uint8_t  lncdispmax;     ///< Line Noise pathway display factor
    uint8_t  spkopts;        ///< spike processing options
    uint8_t  spkthrlevel;    ///< spike threshold level
    uint8_t  spkthrlimit;    ///<
    uint8_t  spkgroup;       ///< NTrodeGroup this electrode belongs to - 0 is single unit, non-0 indicates a multi-trode grouping
    uint8_t  spkhoops;       ///< spike hoop sorting set
} cbPKT_CHANRESET;

/// @}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name Adaptive Filtering
///
/// Ground truth from upstream/cbproto/cbproto.h lines 1310-1333
/// @{

#define cbPKTTYPE_ADAPTFILTREP  0x25        ///< NSP->PC response
#define cbPKTTYPE_ADAPTFILTSET  0xA5        ///< PC->NSP request
#define cbPKTDLEN_ADAPTFILTINFO ((sizeof(cbPKT_ADAPTFILTINFO) / 4) - cbPKT_HEADER_32SIZE)

// Adaptive filter settings
#define ADAPT_FILT_DISABLED     0
#define ADAPT_FILT_ALL          1
#define ADAPT_FILT_SPIKES       2

/// @brief PKT Set:0xA5 Rep:0x25 - Adaptive filtering
///
/// This sets the parameters for the adaptive filtering.
typedef struct {
    cbPKT_HEADER cbpkt_header;  ///< packet header

    uint32_t chan;           ///< Ignored

    uint32_t nMode;          ///< 0=disabled, 1=filter continuous & spikes, 2=filter spikes
    float  dLearningRate;  ///< speed at which adaptation happens. Very small. e.g. 5e-12
    uint32_t nRefChan1;      ///< The first reference channel (1 based).
    uint32_t nRefChan2;      ///< The second reference channel (1 based).

} cbPKT_ADAPTFILTINFO;

/// @}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name Reference Electrode Filtering
///
/// Ground truth from upstream/cbproto/cbproto.h lines 1335-1355
/// @{

#define cbPKTTYPE_REFELECFILTREP  0x26        ///< NSP->PC response
#define cbPKTTYPE_REFELECFILTSET  0xA6        ///< PC->NSP request
#define cbPKTDLEN_REFELECFILTINFO ((sizeof(cbPKT_REFELECFILTINFO) / 4) - cbPKT_HEADER_32SIZE)

// Reference electrode filter settings
#define REFELEC_FILT_DISABLED   0
#define REFELEC_FILT_ALL        1
#define REFELEC_FILT_SPIKES     2

/// @brief PKT Set:0xA6 Rep:0x26 - Reference Electrode Information.
///
/// This configures a channel to be referenced by another channel.
typedef struct {
    cbPKT_HEADER cbpkt_header;  ///< packet header

    uint32_t chan;           ///< Ignored

    uint32_t nMode;          ///< 0=disabled, 1=filter continuous & spikes, 2=filter spikes
    uint32_t nRefChan;       ///< The reference channel (1 based).
} cbPKT_REFELECFILTINFO;

/// @}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name Line Noise Cancellation
///
/// Ground truth from upstream/cbproto/cbproto.h lines 1910-1926
/// @{

#define cbPKTTYPE_LNCREP       0x28        ///< NSP->PC response
#define cbPKTTYPE_LNCSET       0xA8        ///< PC->NSP request
#define cbPKTDLEN_LNC ((sizeof(cbPKT_LNC) / 4) - cbPKT_HEADER_32SIZE)

/// @brief PKT Set:0xA8 Rep:0x28 - Line Noise Cancellation
///
/// This packet holds the Line Noise Cancellation parameters
typedef struct
{
    cbPKT_HEADER cbpkt_header;  ///< packet header

    uint32_t lncFreq;        ///< Nominal line noise frequency to be canceled  (in Hz)
    uint32_t lncRefChan;     ///< Reference channel for lnc synch (1-based)
    uint32_t lncGlobalMode;  ///< reserved
} cbPKT_LNC;

/// @}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name Video Synchronization
///
/// Ground truth from upstream/cbproto/cbproto.h lines 1113-1128
/// @{

#define cbPKTTYPE_VIDEOSYNCHREP   0x29  ///< NSP->PC response
#define cbPKTTYPE_VIDEOSYNCHSET   0xA9  ///< PC->NSP request
#define cbPKTDLEN_VIDEOSYNCH  ((sizeof(cbPKT_VIDEOSYNCH)/4) - cbPKT_HEADER_32SIZE)

/// @brief PKT Set:0xA9 Rep:0x29 - Video/external synchronization packet.
///
/// This packet comes from NeuroMotive through network or RS232
/// then is transmitted to the Central after spike or LFP packets to stamp them.
typedef struct {
    cbPKT_HEADER cbpkt_header;  ///< packet header

    uint16_t split;      ///< file split number of the video file
    uint32_t frame;      ///< frame number in last video
    uint32_t etime;      ///< capture elapsed time (in milliseconds)
    uint16_t id;         ///< video source id
} cbPKT_VIDEOSYNCH;

/// @}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name File Configuration
///
/// Ground truth from upstream/cbproto/cbproto.h lines 1550-1584
/// @{

// file config options
#define cbFILECFG_OPT_NONE          0x00000000  ///< Launch File dialog, set file info, start or stop recording
#define cbFILECFG_OPT_KEEPALIVE     0x00000001  ///< Keep-alive message
#define cbFILECFG_OPT_REC           0x00000002  ///< Recording is in progress
#define cbFILECFG_OPT_STOP          0x00000003  ///< Recording stopped
#define cbFILECFG_OPT_NMREC         0x00000004  ///< NeuroMotive recording status
#define cbFILECFG_OPT_CLOSE         0x00000005  ///< Close file application
#define cbFILECFG_OPT_SYNCH         0x00000006  ///< Recording datetime
#define cbFILECFG_OPT_OPEN          0x00000007  ///< Launch File dialog, do not set or do anything
#define cbFILECFG_OPT_TIMEOUT       0x00000008  ///< Keep alive not received so it timed out
#define cbFILECFG_OPT_PAUSE         0x00000009  ///< Recording paused

// file save configuration packet
#define cbPKTTYPE_REPFILECFG 0x61
#define cbPKTTYPE_SETFILECFG 0xE1
#define cbPKTDLEN_FILECFG ((sizeof(cbPKT_FILECFG)/4) - cbPKT_HEADER_32SIZE)
#define cbPKTDLEN_FILECFGSHORT (cbPKTDLEN_FILECFG - ((sizeof(char)*3*cbLEN_STR_COMMENT)/4)) ///< used for keep-alive messages

/// @brief PKT Set:0xE1 Rep:0x61 - File configuration packet
///
/// File recording can be started or stopped externally using this packet.  It also contains a timeout mechanism to notify
/// if file isn't still recording.
typedef struct {
    cbPKT_HEADER cbpkt_header;  ///< packet header

    uint32_t options;         ///< cbFILECFG_OPT_*
    uint32_t duration;
    uint32_t recording;      ///< If cbFILECFG_OPT_NONE this option starts/stops recording remotely
    uint32_t extctrl;        ///< If cbFILECFG_OPT_REC this is split number (0 for non-TOC)
                           ///< If cbFILECFG_OPT_STOP this is error code (0 means no error)

    char   username[cbLEN_STR_COMMENT];     ///< name of computer issuing the packet
    union {
        char   filename[cbLEN_STR_COMMENT]; ///< filename to record to
        char   datetime[cbLEN_STR_COMMENT]; ///<
    };
    char   comment[cbLEN_STR_COMMENT];  ///< comment to include in the file
} cbPKT_FILECFG;

/// @}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name Log Packet
///
/// Ground truth from upstream/cbproto/cbproto.h lines 1008-1037
/// @{

// Log modes
#define cbLOG_MODE_NONE         0   ///< Normal log
#define cbLOG_MODE_CRITICAL     1   ///< Critical log
#define cbLOG_MODE_RPC          2   ///< PC->NSP: Remote Procedure Call (RPC)
#define cbLOG_MODE_PLUGINFO     3   ///< NSP->PC: Plugin information
#define cbLOG_MODE_RPC_RES      4   ///< NSP->PC: Remote Procedure Call Results
#define cbLOG_MODE_PLUGINERR    5   ///< NSP->PC: Plugin error information
#define cbLOG_MODE_RPC_END      6   ///< NSP->PC: Last RPC packet
#define cbLOG_MODE_RPC_KILL     7   ///< PC->NSP: terminate last RPC
#define cbLOG_MODE_RPC_INPUT    8   ///< PC->NSP: RPC command input
#define cbLOG_MODE_UPLOAD_RES   9   ///< NSP->PC: Upload result
#define cbLOG_MODE_ENDPLUGIN   10   ///< PC->NSP: Signal the plugin to end
#define cbLOG_MODE_NSP_REBOOT  11   ///< PC->NSP: Reboot the NSP

#define cbMAX_LOG          130  ///< Maximum log description
#define cbPKTTYPE_LOGREP   0x63 ///< NPLAY->PC response
#define cbPKTTYPE_LOGSET   0xE3 ///< PC->NPLAY request
#define cbPKTDLEN_LOG    ((sizeof(cbPKT_LOG)/4) - cbPKT_HEADER_32SIZE)
#define cbPKTDLEN_LOGSHORT    (cbPKTDLEN_LOG - ((sizeof(char)*cbMAX_LOG)/4)) ///< All but description

/// @brief PKT Set:0xE3 Rep:0x63 - Log packet
///
/// Similar to the comment packet but used for internal NSP events and extension communication.
typedef struct {
    cbPKT_HEADER cbpkt_header;  ///< packet header

    uint16_t mode;        ///< log mode (cbLOG_MODE_*)
    char   name[cbLEN_STR_LABEL];  ///< Logger source name (Computer name, Plugin name, ...)
    char   desc[cbMAX_LOG];        ///< description of the change (will fill the rest of the packet)
} cbPKT_LOG;

/// @}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name Patient Information
///
/// Ground truth from upstream/cbproto/cbproto.h lines 1586-1605
/// @{

#define cbMAX_PATIENTSTRING  128     ///< Maximum patient string length

#define cbPKTTYPE_REPPATIENTINFO 0x64
#define cbPKTTYPE_SETPATIENTINFO 0xE4
#define cbPKTDLEN_PATIENTINFO ((sizeof(cbPKT_PATIENTINFO)/4) - cbPKT_HEADER_32SIZE)

/// @brief PKT Set:0xE4 Rep:0x64 - Patient information packet.
///
/// This can be used to externally set the patient information of a file.
typedef struct {
    cbPKT_HEADER cbpkt_header;  ///< packet header

    char   ID[cbMAX_PATIENTSTRING];         ///< Patient identification
    char   firstname[cbMAX_PATIENTSTRING];  ///< Patient first name
    char   lastname[cbMAX_PATIENTSTRING];   ///< Patient last name
    uint32_t DOBMonth;    ///< Patient birth month
    uint32_t DOBDay;      ///< Patient birth day
    uint32_t DOBYear;     ///< Patient birth year
} cbPKT_PATIENTINFO;

/// @}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name Impedance Packets (Deprecated)
///
/// Ground truth from upstream/cbproto/cbproto.h lines 1607-1659
/// @{

#define cbPKTTYPE_REPIMPEDANCE   0x65
#define cbPKTTYPE_SETIMPEDANCE   0xE5
#define cbPKTDLEN_IMPEDANCE ((sizeof(cbPKT_IMPEDANCE)/4) - cbPKT_HEADER_32SIZE)

/// @brief *Deprecated* Send impedance data
typedef struct {
    cbPKT_HEADER cbpkt_header;  ///< packet header

    float  data[(cbPKT_MAX_SIZE - cbPKT_HEADER_SIZE) / sizeof(float)];  ///< variable length address list
} cbPKT_IMPEDANCE;

#define cbPKTTYPE_REPINITIMPEDANCE   0x66
#define cbPKTTYPE_SETINITIMPEDANCE   0xE6
#define cbPKTDLEN_INITIMPEDANCE ((sizeof(cbPKT_INITIMPEDANCE)/4) - cbPKT_HEADER_32SIZE)

/// @brief *Deprecated* Initiate impedance calculations
typedef struct {
    cbPKT_HEADER cbpkt_header;  ///< packet header

    uint32_t initiate;    ///< on set call -> 1 to start autoimpedance
                        ///< on response -> 1 initiated
} cbPKT_INITIMPEDANCE;

/// @}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name Poll Packet (Deprecated)
///
/// Ground truth from upstream/cbproto/cbproto.h lines 1619-1645
/// @{

// Poll packet command
#define cbPOLL_MODE_NONE            0   ///< no command (parameters)
#define cbPOLL_MODE_APPSTATUS       1   ///< Poll or response to poll about the status of an application

// Poll packet status flags
#define cbPOLL_FLAG_NONE            0   ///< no flag (parameters)
#define cbPOLL_FLAG_RESPONSE        1   ///< Response to the query

// Extra information
#define cbPOLL_EXT_NONE             0   ///< No extra information
#define cbPOLL_EXT_EXISTS           1   ///< App exists
#define cbPOLL_EXT_RUNNING          2   ///< App is running

#define cbPKTTYPE_REPPOLL   0x67
#define cbPKTTYPE_SETPOLL   0xE7
#define cbPKTDLEN_POLL ((sizeof(cbPKT_POLL)/4) - cbPKT_HEADER_32SIZE)

/// @brief *Deprecated* Poll for packet mechanism
typedef struct {
    cbPKT_HEADER cbpkt_header;  ///< packet header

    uint32_t  mode;          ///< any of cbPOLL_MODE_* commands
    uint32_t  flags;         ///< any of the cbPOLL_FLAG_* status
    uint32_t  extra;         ///< Extra parameters depending on flags and mode
    char    appname[32];   ///< name of program to apply command specified by mode
    char    username[256]; ///< return your computername
    uint32_t  res[32];       ///< reserved for the future
} cbPKT_POLL;

/// @}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name Map File Configuration
///
/// Ground truth from upstream/cbproto/cbproto.h lines 1661-1673
/// @{

#define cbPKTTYPE_REPMAPFILE 0x68
#define cbPKTTYPE_SETMAPFILE 0xE8
#define cbPKTDLEN_MAPFILE ((sizeof(cbPKT_MAPFILE)/4) - cbPKT_HEADER_32SIZE)

/// @brief PKT Set:0xE8 Rep:0x68 - Map file
///
/// Sets the mapfile for applications that use a mapfile so they all display similarly.
typedef struct {
    cbPKT_HEADER cbpkt_header;  ///< packet header

    char   filename[512];   ///< filename of the mapfile to use
} cbPKT_MAPFILE;

/// @}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name Spike Sorting Packets
///
/// Ground truth from upstream/cbproto/cbproto.h lines 1690-1849
/// @{

// Spike sorting algorithm identifiers
#define cbAUTOALG_NONE                 0   ///< No sorting
#define cbAUTOALG_SPREAD               1   ///< Auto spread
#define cbAUTOALG_HIST_CORR_MAJ        2   ///< Auto Hist Correlation
#define cbAUTOALG_HIST_PEAK_COUNT_MAJ  3   ///< Auto Hist Peak Maj
#define cbAUTOALG_HIST_PEAK_COUNT_FISH 4   ///< Auto Hist Peak Fish
#define cbAUTOALG_PCA                  5   ///< Manual PCA
#define cbAUTOALG_HOOPS                6   ///< Manual Hoops
#define cbAUTOALG_PCA_KMEANS           7   ///< K-means PCA
#define cbAUTOALG_PCA_EM               8   ///< EM-clustering PCA
#define cbAUTOALG_PCA_DBSCAN           9   ///< DBSCAN PCA

// Spike sorting mode commands
#define cbAUTOALG_MODE_SETTING         0   ///< Change the settings and leave sorting the same (PC->NSP request)
#define cbAUTOALG_MODE_APPLY           1   ///< Change settings and apply this sorting to all channels (PC->NSP request)

// SS Model All constants
#define cbPKTTYPE_SS_MODELALLREP   0x50        ///< NSP->PC response
#define cbPKTTYPE_SS_MODELALLSET   0xD0        ///< PC->NSP request
#define cbPKTDLEN_SS_MODELALLSET ((sizeof(cbPKT_SS_MODELALLSET) / 4) - cbPKT_HEADER_32SIZE)

/// @brief PKT Set:0xD0 Rep:0x50 - Get the spike sorting model for all channels (Histogram Peak Count)
///
/// This packet says, "Give me all of the model". In response, you will get a series of cbPKTTYPE_MODELREP
typedef struct {
    cbPKT_HEADER cbpkt_header;  ///< packet header
} cbPKT_SS_MODELALLSET;

// SS Model constants
#define cbPKTTYPE_SS_MODELREP      0x51        ///< NSP->PC response
#define cbPKTTYPE_SS_MODELSET      0xD1        ///< PC->NSP request
#define cbPKTDLEN_SS_MODELSET ((sizeof(cbPKT_SS_MODELSET) / 4) - cbPKT_HEADER_32SIZE)
#define MAX_REPEL_POINTS 3

/// @brief PKT Set:0xD1 Rep:0x51 - Get the spike sorting model for a single channel (Histogram Peak Count)
///
/// The system replys with the model of a specific channel.
typedef struct {
    cbPKT_HEADER cbpkt_header;  ///< packet header

    uint32_t chan;           ///< actual channel id of the channel being configured (0 based)
    uint32_t unit_number;    ///< unit label (0 based, 0 is noise cluster)
    uint32_t valid;          ///< 1 = valid unit, 0 = not a unit, in other words just deleted when NSP -> PC
    uint32_t inverted;       ///< 0 = not inverted, 1 = inverted

    // Block statistics (change from block to block)
    int32_t  num_samples;    ///< non-zero value means that the block stats are valid
    float  mu_x[2];
    float  Sigma_x[2][2];
    float  determinant_Sigma_x;
    ///// Only needed if we are using a Bayesian classification model
    float  Sigma_x_inv[2][2];
    float  log_determinant_Sigma_x;
    /////
    float  subcluster_spread_factor_numerator;
    float  subcluster_spread_factor_denominator;
    float  mu_e;
    float  sigma_e_squared;
} cbPKT_SS_MODELSET;

// SS Detect constants
#define cbPKTTYPE_SS_DETECTREP  0x52        ///< NSP->PC response
#define cbPKTTYPE_SS_DETECTSET  0xD2        ///< PC->NSP request
#define cbPKTDLEN_SS_DETECT ((sizeof(cbPKT_SS_DETECT) / 4) - cbPKT_HEADER_32SIZE)

/// @brief PKT Set:0xD2 Rep:0x52 - Auto threshold parameters
///
/// Set the auto threshold parameters
typedef struct {
    cbPKT_HEADER cbpkt_header;  ///< packet header

    float  fThreshold;     ///< current detection threshold
    float  fMultiplier;    ///< multiplier
} cbPKT_SS_DETECT;

// SS Artifact Reject constants
#define cbPKTTYPE_SS_ARTIF_REJECTREP  0x53        ///< NSP->PC response
#define cbPKTTYPE_SS_ARTIF_REJECTSET  0xD3        ///< PC->NSP request
#define cbPKTDLEN_SS_ARTIF_REJECT ((sizeof(cbPKT_SS_ARTIF_REJECT) / 4) - cbPKT_HEADER_32SIZE)

/// @brief PKT Set:0xD3 Rep:0x53 - Artifact reject
///
/// Sets the artifact rejection parameters.
typedef struct {
    cbPKT_HEADER cbpkt_header;  ///< packet header

    uint32_t nMaxSimulChans;     ///< how many channels can fire exactly at the same time???
    uint32_t nRefractoryCount;   ///< for how many samples (30 kHz) is a neuron refractory, so can't re-trigger
} cbPKT_SS_ARTIF_REJECT;

// SS Noise Boundary constants
#define cbPKTTYPE_SS_NOISE_BOUNDARYREP  0x54        ///< NSP->PC response
#define cbPKTTYPE_SS_NOISE_BOUNDARYSET  0xD4        ///< PC->NSP request
#define cbPKTDLEN_SS_NOISE_BOUNDARY ((sizeof(cbPKT_SS_NOISE_BOUNDARY) / 4) - cbPKT_HEADER_32SIZE)

/// @brief PKT Set:0xD4 Rep:0x54 - Noise boundary
///
/// Sets the noise boundary parameters
typedef struct {
    cbPKT_HEADER cbpkt_header;  ///< packet header

    uint32_t chan;        ///< which channel we belong to
    float  afc[3];      ///< the center of the ellipsoid
    float  afS[3][3];   ///< an array of the axes for the ellipsoid
} cbPKT_SS_NOISE_BOUNDARY;

// SS Statistics constants
#define cbPKTTYPE_SS_STATISTICSREP  0x55        ///< NSP->PC response
#define cbPKTTYPE_SS_STATISTICSSET  0xD5        ///< PC->NSP request
#define cbPKTDLEN_SS_STATISTICS ((sizeof(cbPKT_SS_STATISTICS) / 4) - cbPKT_HEADER_32SIZE)

/// @brief PKT Set:0xD5 Rep:0x55 - Spike sourting statistics (Histogram peak count)
typedef struct {
    cbPKT_HEADER cbpkt_header;  ///< packet header

    uint32_t nUpdateSpikes;  ///< update rate in spike counts

    uint32_t nAutoalg;       ///< sorting algorithm (0=none 1=spread, 2=hist_corr_maj, 3=hist_peak_count_maj, 4=hist_peak_count_maj_fisher, 5=pca, 6=hoops)
    uint32_t nMode;          ///< cbAUTOALG_MODE_SETTING,

    float  fMinClusterPairSpreadFactor;       ///< larger number = more apt to combine 2 clusters into 1
    float  fMaxSubclusterSpreadFactor;        ///< larger number = less apt to split because of 2 clusers

    float  fMinClusterHistCorrMajMeasure;     ///< larger number = more apt to split 1 cluster into 2
    float  fMaxClusterPairHistCorrMajMeasure; ///< larger number = less apt to combine 2 clusters into 1

    float  fClusterHistValleyPercentage;      ///< larger number = less apt to split nearby clusters
    float  fClusterHistClosePeakPercentage;   ///< larger number = less apt to split nearby clusters
    float  fClusterHistMinPeakPercentage;     ///< larger number = less apt to split separated clusters

    uint32_t nWaveBasisSize;                     ///< number of wave to collect to calculate the basis,
                                                ///< must be greater than spike length
    uint32_t nWaveSampleSize;                    ///< number of samples sorted with the same basis before re-calculating the basis
                                                ///< 0=manual re-calculation
                                                ///< nWaveBasisSize * nWaveSampleSize is the number of waves/spikes to run against
                                                ///< the same PCA basis before next
} cbPKT_SS_STATISTICS;

// SS Status constants
#define cbPKTTYPE_SS_STATUSREP  0x57        ///< NSP->PC response
#define cbPKTTYPE_SS_STATUSSET  0xD7        ///< PC->NSP request
#define cbPKTDLEN_SS_STATUS ((sizeof(cbPKT_SS_STATUS) / 4) - cbPKT_HEADER_32SIZE)

/// @brief PKT Set:0xD7 Rep:0x57 - Spike sorting status (Histogram peak count)
///
/// This packet contains the status of the automatic spike sorting.
typedef struct {
    cbPKT_HEADER cbpkt_header;  ///< packet header

    cbAdaptControl cntlUnitStats;   ///<
    cbAdaptControl cntlNumUnits;    ///<
} cbPKT_SS_STATUS;

// SS Reset constants
#define cbPKTTYPE_SS_RESETREP       0x56        ///< NSP->PC response
#define cbPKTTYPE_SS_RESETSET       0xD6        ///< PC->NSP request
#define cbPKTDLEN_SS_RESET ((sizeof(cbPKT_SS_RESET) / 4) - cbPKT_HEADER_32SIZE)

/// @brief PKT Set:0xD6 Rep:0x56 - Spike sorting reset
///
/// Send this packet to the NSP to tell it to reset all spike sorting to default values
typedef struct
{
    cbPKT_HEADER cbpkt_header;  ///< packet header
} cbPKT_SS_RESET;

// SS Reset Model constants
#define cbPKTTYPE_SS_RESET_MODEL_REP    0x58        ///< NSP->PC response
#define cbPKTTYPE_SS_RESET_MODEL_SET    0xD8        ///< PC->NSP request
#define cbPKTDLEN_SS_RESET_MODEL ((sizeof(cbPKT_SS_RESET_MODEL) / 4) - cbPKT_HEADER_32SIZE)

/// @brief PKT Set:0xD8 Rep:0x58 - Spike sorting reset model
///
/// Send this packet to the NSP to tell it to reset all spike sorting models
typedef struct
{
    cbPKT_HEADER cbpkt_header;  ///< packet header
} cbPKT_SS_RESET_MODEL;

// Feature space commands and status changes
#define cbPCA_RECALC_START             0    ///< PC ->NSP start recalculation
#define cbPCA_RECALC_STOPPED           1    ///< NSP->PC  finished recalculation
#define cbPCA_COLLECTION_STARTED       2    ///< NSP->PC  waveform collection started
#define cbBASIS_CHANGE                 3    ///< Change the basis of feature space
#define cbUNDO_BASIS_CHANGE            4
#define cbREDO_BASIS_CHANGE            5
#define cbINVALIDATE_BASIS             6

// SS Recalc constants
#define cbPKTTYPE_SS_RECALCREP       0x59        ///< NSP->PC response
#define cbPKTTYPE_SS_RECALCSET       0xD9        ///< PC->NSP request
#define cbPKTDLEN_SS_RECALC ((sizeof(cbPKT_SS_RECALC) / 4) - cbPKT_HEADER_32SIZE)

/// @brief PKT Set:0xD9 Rep:0x59 - Spike Sorting recalculate PCA
///
/// Send this packet to the NSP to tell it to re calculate all PCA Basis Vectors and Values
typedef struct
{
    cbPKT_HEADER cbpkt_header;  ///< packet header

    uint32_t chan;           ///< 1 based channel we want to recalc (0 = All channels)
    uint32_t mode;           ///< cbPCA_RECALC_START -> Start PCA basis, cbPCA_RECALC_STOPPED-> PCA basis stopped, cbPCA_COLLECTION_STARTED -> PCA waveform collection started
} cbPKT_SS_RECALC;

/// @}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name Feature Space Basis
///
/// Ground truth from upstream/cbproto/cbproto.h lines 1889-1909
/// @{

#define cbPKTTYPE_FS_BASISREP       0x5B        ///< NSP->PC response
#define cbPKTTYPE_FS_BASISSET       0xDB        ///< PC->NSP request
#define cbPKTDLEN_FS_BASIS ((sizeof(cbPKT_FS_BASIS) / 4) - cbPKT_HEADER_32SIZE)
#define cbPKTDLEN_FS_BASISSHORT (cbPKTDLEN_FS_BASIS - ((sizeof(float)* cbMAX_PNTS * 3)/4))

/// @brief PKT Set:0xDB Rep:0x5B - Feature Space Basis
///
/// This packet holds the calculated basis of the feature space from NSP to Central
/// Or it has the previous basis retrieved and transmitted by central to NSP
typedef struct
{
    cbPKT_HEADER cbpkt_header;  ///< packet header

    uint32_t chan;           ///< 1-based channel number
    uint32_t mode;           ///< cbBASIS_CHANGE, cbUNDO_BASIS_CHANGE, cbREDO_BASIS_CHANGE, cbINVALIDATE_BASIS ...
    uint32_t fs;             ///< Feature space: cbAUTOALG_PCA
    /// basis must be the last item in the structure because it can be variable length to a max of cbMAX_PNTS
    float  basis[cbMAX_PNTS][3];    ///< Room for all possible points collected
} cbPKT_FS_BASIS;

/// @}


///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name Video and Tracking (NeuroMotive)
///
/// Ground truth from upstream/cbproto/cbproto.h lines 493-511
/// @{

/// @brief NeuroMotive video source
typedef struct {
    char    name[cbLEN_STR_LABEL];  ///< filename of the video file
    float   fps;                    ///< nominal record fps
} cbVIDEOSOURCE;

// Track object types
#define cbTRACKOBJ_TYPE_UNDEFINED  0  ///< Undefined track object type
#define cbTRACKOBJ_TYPE_2DMARKERS  1  ///< 2D marker tracking
#define cbTRACKOBJ_TYPE_2DBLOB     2  ///< 2D blob tracking
#define cbTRACKOBJ_TYPE_3DMARKERS  3  ///< 3D marker tracking
#define cbTRACKOBJ_TYPE_2DBOUNDARY 4  ///< 2D boundary tracking
#define cbTRACKOBJ_TYPE_1DSIZE     5  ///< 1D size tracking

/// @brief Track object structure for NeuroMotive
typedef struct {
    char     name[cbLEN_STR_LABEL];  ///< name of the object
    uint16_t type;                   ///< trackable type (cbTRACKOBJ_TYPE_*)
    uint16_t pointCount;             ///< maximum number of points
} cbTRACKOBJ;

// NeuroMotive status
#define cbNM_STATUS_IDLE                0  ///< NeuroMotive is idle
#define cbNM_STATUS_EXIT                1  ///< NeuroMotive is exiting
#define cbNM_STATUS_REC                 2  ///< NeuroMotive is recording
#define cbNM_STATUS_PLAY                3  ///< NeuroMotive is playing video file
#define cbNM_STATUS_CAP                 4  ///< NeuroMotive is capturing from camera
#define cbNM_STATUS_STOP                5  ///< NeuroMotive is stopping
#define cbNM_STATUS_PAUSED              6  ///< NeuroMotive is paused
#define cbNM_STATUS_COUNT               7  ///< This is the count of status options

// NeuroMotive commands and status changes (cbPKT_NM.mode)
#define cbNM_MODE_NONE                  0  ///< No command
#define cbNM_MODE_CONFIG                1  ///< Ask NeuroMotive for configuration
#define cbNM_MODE_SETVIDEOSOURCE        2  ///< Configure video source
#define cbNM_MODE_SETTRACKABLE          3  ///< Configure trackable
#define cbNM_MODE_STATUS                4  ///< NeuroMotive status reporting (cbNM_STATUS_*)
#define cbNM_MODE_TSCOUNT               5  ///< Timestamp count (value is the period with 0 to disable this mode)
#define cbNM_MODE_SYNCHCLOCK            6  ///< Start (or stop) synchronization clock (fps*1000 specified by value, zero fps to stop capture)
#define cbNM_MODE_ASYNCHCLOCK           7  ///< Asynchronous clock

#define cbNM_FLAG_NONE                  0  ///< No flags

#define cbPKTTYPE_NMREP   0x32  ///< NSP->PC response
#define cbPKTTYPE_NMSET   0xB2  ///< PC->NSP request
#define cbPKTDLEN_NM  ((sizeof(cbPKT_NM)/4) - cbPKT_HEADER_32SIZE)

/// @brief PKT Set:0xB2 Rep:0x32 - NeuroMotive packet structure
///
/// Used for video tracking and NeuroMotive configuration
typedef struct {
    cbPKT_HEADER cbpkt_header;  ///< packet header

    uint32_t mode;        ///< cbNM_MODE_* command to NeuroMotive or Central
    uint32_t flags;       ///< cbNM_FLAG_* status of NeuroMotive
    uint32_t value;       ///< value assigned to this mode
    union {
        uint32_t opt[cbLEN_STR_LABEL / 4]; ///< Additional options for this mode
        char   name[cbLEN_STR_LABEL];      ///< name associated with this mode
    };
} cbPKT_NM;

/// @}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name N-Trode Information
///
/// Ground truth from upstream/cbproto/cbproto.h lines 1357-1375
/// @{

// N-Trode feature space modes
enum cbNTRODEINFO_FS_MODE {
    cbNTRODEINFO_FS_PEAK,      ///< Feature space based on peak
    cbNTRODEINFO_FS_VALLEY,    ///< Feature space based on valley
    cbNTRODEINFO_FS_AMPLITUDE, ///< Feature space based on amplitude
    cbNTRODEINFO_FS_COUNT      ///< Number of feature space modes
};

#define cbPKTTYPE_REPNTRODEINFO      0x27        ///< NSP->PC response
#define cbPKTTYPE_SETNTRODEINFO      0xA7        ///< PC->NSP request
#define cbPKTDLEN_NTRODEINFO         ((sizeof(cbPKT_NTRODEINFO) / 4) - cbPKT_HEADER_32SIZE)

/// @brief PKT Set:0xA7 Rep:0x27 - N-Trode information packets
///
/// Sets information about an N-Trode.  The user can change the name, number of sites, sites (channels)
typedef struct {
    cbPKT_HEADER cbpkt_header;  ///< packet header

    uint32_t ntrode;         ///< ntrode with which we are working (1-based)
    char   label[cbLEN_STR_LABEL];   ///< Label of the Ntrode (null terminated if < 16 characters)
    cbMANUALUNITMAPPING ellipses[cbMAXSITEPLOTS][cbMAXUNITS];  ///< unit mapping
    uint16_t nSite;          ///< number channels in this NTrode ( 0 <= nSite <= cbMAXSITES)
    uint16_t fs;             ///< NTrode feature space cbNTRODEINFO_FS_*
    uint16_t nChan[cbMAXSITES];  ///< group of channels in this NTrode
} cbPKT_NTRODEINFO;

/// @}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name Analog Output Waveform
///
/// Ground truth from upstream/cbproto/cbproto.h lines 1943-2008
/// @{

#define cbMAX_WAVEFORM_PHASES  ((cbPKT_MAX_SIZE - cbPKT_HEADER_SIZE - 24) / 4)   ///< Maximum number of phases in a waveform

/// @brief Analog output waveform data
///
/// Contains the parameters to define a waveform for Analog Output channels
typedef struct
{
    int16_t   offset;         ///< DC offset
    union {
        struct {
            uint16_t sineFrequency;  ///< sine wave Hz
            int16_t  sineAmplitude;  ///< sine amplitude
        };
        struct {
            uint16_t seq;            ///< Wave sequence number (for file playback)
            uint16_t seqTotal;       ///< total number of sequences
            uint16_t phases;         ///< Number of valid phases in this wave (maximum is cbMAX_WAVEFORM_PHASES)
            uint16_t duration[cbMAX_WAVEFORM_PHASES];     ///< array of durations for each phase
            int16_t  amplitude[cbMAX_WAVEFORM_PHASES];    ///< array of amplitude for each phase
        };
    };
} cbWaveformData;

// Signal generator waveform type
#define cbWAVEFORM_MODE_NONE          0   ///< waveform is disabled
#define cbWAVEFORM_MODE_PARAMETERS    1   ///< waveform is a repeated sequence
#define cbWAVEFORM_MODE_SINE          2   ///< waveform is a sinusoids

// Signal generator waveform trigger type
#define cbWAVEFORM_TRIGGER_NONE              0   ///< instant software trigger
#define cbWAVEFORM_TRIGGER_DINPREG           1   ///< digital input rising edge trigger
#define cbWAVEFORM_TRIGGER_DINPFEG           2   ///< digital input falling edge trigger
#define cbWAVEFORM_TRIGGER_SPIKEUNIT         3   ///< spike unit
#define cbWAVEFORM_TRIGGER_COMMENTCOLOR      4   ///< comment RGBA color (A being big byte)
#define cbWAVEFORM_TRIGGER_RECORDINGSTART    5   ///< recording start trigger
#define cbWAVEFORM_TRIGGER_EXTENSION         6   ///< extension trigger

// AOUT signal generator waveform data
#define cbPKTTYPE_WAVEFORMREP       0x33        ///< NSP->PC response
#define cbPKTTYPE_WAVEFORMSET       0xB3        ///< PC->NSP request
#define cbPKTDLEN_WAVEFORM   ((sizeof(cbPKT_AOUT_WAVEFORM)/4) - cbPKT_HEADER_32SIZE)

/// @brief PKT Set:0xB3 Rep:0x33 - AOUT waveform
///
/// This sets a user defined waveform for one or multiple Analog & Audio Output channels.
typedef struct {
    cbPKT_HEADER cbpkt_header;  ///< packet header

    uint16_t chan;        ///< which analog output/audio output channel (1-based, will equal chan from GetDoutCaps)

    /// Each file may contain multiple sequences.
    /// Each sequence consists of phases
    /// Each phase is defined by amplitude and duration

    /// Waveform parameter information
    uint16_t  mode;              ///< Can be any of cbWAVEFORM_MODE_*
    uint32_t  repeats;           ///< Number of repeats (0 means forever)
    uint8_t   trig;              ///< Can be any of cbWAVEFORM_TRIGGER_*
    uint8_t   trigInst;          ///< Instrument the trigChan belongs
    uint16_t  trigChan;          ///< Depends on trig:
                                 ///  for cbWAVEFORM_TRIGGER_DINP* 1-based trigChan (1-16) is digin1, (17-32) is digin2, ...
                                 ///  for cbWAVEFORM_TRIGGER_SPIKEUNIT 1-based trigChan (1-156) is channel number
                                 ///  for cbWAVEFORM_TRIGGER_COMMENTCOLOR trigChan is A->B in A->B->G->R
    uint16_t  trigValue;         ///< Trigger value (spike unit, G-R comment color, ...)
    uint8_t   trigNum;           ///< trigger number (0-based) (can be up to cbMAX_AOUT_TRIGGER-1)
    uint8_t   active;            ///< status of trigger
    cbWaveformData wave;         ///< Actual waveform data
} cbPKT_AOUT_WAVEFORM;

/// @}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name Stimulation
///
/// Ground truth from upstream/cbproto/cbproto.h lines 2010-2022
/// @{

#define cbPKTTYPE_STIMULATIONREP       0x34        ///< NSP->PC response
#define cbPKTTYPE_STIMULATIONSET       0xB4        ///< PC->NSP request
#define cbPKTDLEN_STIMULATION   ((sizeof(cbPKT_STIMULATION)/4) - cbPKT_HEADER_32SIZE)

/// @brief PKT Set:0xB4 Rep:0x34 - Stimulation command
///
/// This sets a user defined stimulation for stim/record headstages
typedef struct {
    cbPKT_HEADER cbpkt_header;  ///< packet header

    uint8_t commandBytes[40];   ///< series of bytes to control stimulation
} cbPKT_STIMULATION;

/// @}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name nPlay Configuration
///
/// Ground truth from upstream/cbproto/cbproto.h lines 916-965
/// @{

// Audio commands "val"
#define cbAUDIO_CMD_NONE             0   ///< PC->NPLAY query audio status

// nPlay file version (first byte NSx version, second byte NEV version)
#define cbNPLAY_FILE_NS21            1   ///< NSX 2.1 file
#define cbNPLAY_FILE_NS22            2   ///< NSX 2.2 file
#define cbNPLAY_FILE_NS30            3   ///< NSX 3.0 file
#define cbNPLAY_FILE_NEV21           (1 << 8)   ///< Nev 2.1 file
#define cbNPLAY_FILE_NEV22           (2 << 8)   ///< Nev 2.2 file
#define cbNPLAY_FILE_NEV23           (3 << 8)   ///< Nev 2.3 file
#define cbNPLAY_FILE_NEV30           (4 << 8)   ///< Nev 3.0 file

// nPlay commands and status changes (cbPKT_NPLAY.mode)
#define cbNPLAY_FNAME_LEN            (cbPKT_MAX_SIZE - cbPKT_HEADER_SIZE - 40)   ///< length of the file name (with terminating null)
#define cbNPLAY_MODE_NONE            0    ///< no command (parameters)
#define cbNPLAY_MODE_PAUSE           1    ///< PC->NPLAY pause if "val" is non-zero, un-pause otherwise
#define cbNPLAY_MODE_SEEK            2    ///< PC->NPLAY seek to time "val"
#define cbNPLAY_MODE_CONFIG          3    ///< PC<->NPLAY request full config
#define cbNPLAY_MODE_OPEN            4    ///< PC->NPLAY open new file in "val" for playback
#define cbNPLAY_MODE_PATH            5    ///< PC->NPLAY use the directory path in fname
#define cbNPLAY_MODE_CONFIGMAIN      6    ///< PC<->NPLAY request main config packet
#define cbNPLAY_MODE_STEP            7    ///< PC<->NPLAY run "val" procTime steps and pause, then send cbNPLAY_FLAG_STEPPED
#define cbNPLAY_MODE_SINGLE          8    ///< PC->NPLAY single mode if "val" is non-zero, wrap otherwise
#define cbNPLAY_MODE_RESET           9    ///< PC->NPLAY reset nPlay
#define cbNPLAY_MODE_NEVRESORT       10   ///< PC->NPLAY resort NEV if "val" is non-zero, do not if otherwise
#define cbNPLAY_MODE_AUDIO_CMD       11   ///< PC->NPLAY perform audio command in "val" (cbAUDIO_CMD_*), with option "opt"

#define cbNPLAY_FLAG_NONE            0x00  ///< no flag
#define cbNPLAY_FLAG_CONF            0x01  ///< NPLAY->PC config packet ("val" is "fname" file index)
#define cbNPLAY_FLAG_MAIN           (0x02 | cbNPLAY_FLAG_CONF)  ///< NPLAY->PC main config packet ("val" is file version)
#define cbNPLAY_FLAG_DONE            0x02  ///< NPLAY->PC step command done

// nPlay configuration packet(sent on restart together with config packet)
#define cbPKTTYPE_NPLAYREP   0x5C ///< NPLAY->PC response
#define cbPKTTYPE_NPLAYSET   0xDC ///< PC->NPLAY request
#define cbPKTDLEN_NPLAY    ((sizeof(cbPKT_NPLAY)/4) - cbPKT_HEADER_32SIZE)

/// @brief PKT Set:0xDC Rep:0x5C - nPlay configuration packet
///
/// Sent on restart together with config packet
typedef struct {
    cbPKT_HEADER cbpkt_header;  ///< packet header

    union {
        PROCTIME ftime;   ///< the total time of the file.
        PROCTIME opt;     ///< optional value
    };
    PROCTIME stime;       ///< start time
    PROCTIME etime;       ///< stime < end time < ftime
    PROCTIME val;         ///< Used for current time to traverse, file index, file version, ...
    uint16_t mode;        ///< cbNPLAY_MODE_* command to nPlay
    uint16_t flags;       ///< cbNPLAY_FLAG_* status of nPlay
    float speed;          ///< positive means fast forward, negative means rewind, 0 means go as fast as you can.
    char  fname[cbNPLAY_FNAME_LEN];   ///< This is a String with the file name.
} cbPKT_NPLAY;

/// @}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name Set Digital Output
///
/// Ground truth from upstream/cbproto/cbproto.h lines 1928-1941
/// @{

#define cbPKTTYPE_SET_DOUTREP       0x5D        ///< NSP->PC response
#define cbPKTTYPE_SET_DOUTSET       0xDD        ///< PC->NSP request
#define cbPKTDLEN_SET_DOUT ((sizeof(cbPKT_SET_DOUT) / 4) - cbPKT_HEADER_32SIZE)

/// @brief PKT Set:0xDD Rep:0x5D - Set Digital Output
///
/// Allows setting the digital output value if not assigned set to monitor a channel or timed waveform or triggered
typedef struct {
    cbPKT_HEADER cbpkt_header;  ///< packet header

    uint16_t chan;        ///< which digital output channel (1 based, will equal chan from GetDoutCaps)
    uint16_t value;       ///< Which value to set? zero = 0; non-zero = 1 (output is 1 bit)
} cbPKT_SET_DOUT;

/// @}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name Trigger and Video Tracking
///
/// Ground truth from upstream/cbproto/cbproto.h lines 967-1005
/// @{

#define cbTRIGGER_MODE_UNDEFINED        0
#define cbTRIGGER_MODE_BUTTONPRESS      1 ///< Patient button press event
#define cbTRIGGER_MODE_EVENTRESET       2 ///< event reset

#define cbPKTTYPE_TRIGGERREP   0x5E ///< NPLAY->PC response
#define cbPKTTYPE_TRIGGERSET   0xDE ///< PC->NPLAY request
#define cbPKTDLEN_TRIGGER    ((sizeof(cbPKT_TRIGGER)/4) - cbPKT_HEADER_32SIZE)

/// @brief PKT Set:0xDE Rep:0x5E - Trigger Packet used for Cervello system
typedef struct {
    cbPKT_HEADER cbpkt_header;  ///< packet header

    uint32_t mode;        ///< cbTRIGGER_MODE_*
} cbPKT_TRIGGER;

#define cbMAX_TRACKCOORDS (128) ///< Maximum number of coordinates (must be an even number)
#define cbPKTTYPE_VIDEOTRACKREP   0x5F ///< NPLAY->PC response
#define cbPKTTYPE_VIDEOTRACKSET   0xDF ///< PC->NPLAY request
#define cbPKTDLEN_VIDEOTRACK    ((sizeof(cbPKT_VIDEOTRACK)/4) - cbPKT_HEADER_32SIZE)
#define cbPKTDLEN_VIDEOTRACKSHORT (cbPKTDLEN_VIDEOTRACK - ((sizeof(uint16_t)*cbMAX_TRACKCOORDS)/4))

/// @brief PKT Set:0xDF Rep:0x5F - Video tracking event packet
typedef struct {
    cbPKT_HEADER cbpkt_header;  ///< packet header

    uint16_t parentID;    ///< parent ID
    uint16_t nodeID;      ///< node ID (cross-referenced in the TrackObj header)
    uint16_t nodeCount;   ///< Children count
    uint16_t pointCount;  ///< number of points at this node
    ///< this must be the last item in the structure because it can be variable length to a max of cbMAX_TRACKCOORDS
    union {
        uint16_t coords[cbMAX_TRACKCOORDS];
        uint32_t sizes[cbMAX_TRACKCOORDS / 2];
    };
} cbPKT_VIDEOTRACK;

/// @}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name Configuration Tables (Central/cbhwlib only)
///
/// Ground truth from upstream/cbhwlib/cbhwlib.h lines 906-931
/// These are used in shared memory structures for Central application
/// @{

#ifndef COLORREF
#define COLORREF uint32_t
#endif

/// @brief Color table for Central application
///
/// Used for display configuration in Central
typedef struct {
    COLORREF winrsvd[48];       ///< Reserved for Windows
    COLORREF dispback;          ///< Display background color
    COLORREF dispgridmaj;       ///< Display major grid color
    COLORREF dispgridmin;       ///< Display minor grid color
    COLORREF disptext;          ///< Display text color
    COLORREF dispwave;          ///< Display waveform color
    COLORREF dispwavewarn;      ///< Display waveform warning color
    COLORREF dispwaveclip;      ///< Display waveform clipping color
    COLORREF dispthresh;        ///< Display threshold color
    COLORREF dispmultunit;      ///< Display multi-unit color
    COLORREF dispunit[16];      ///< Display unit colors (0 = unclassified)
    COLORREF dispnoise;         ///< Display noise color
    COLORREF dispchansel[3];    ///< Display channel selection colors
    COLORREF disptemp[5];       ///< Display temporary colors
    COLORREF disprsvd[14];      ///< Reserved display colors
} cbCOLORTABLE;

/// @brief Option table for Central application
///
/// Used for configuration options in Central
typedef struct {
    float fRMSAutoThresholdDistance;    ///< multiplier to use for autothresholding when using RMS to guess noise
    uint32_t reserved[31];              ///< Reserved for future use
} cbOPTIONTABLE;

/// @}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name Firmware Update Packets
///
/// Ground truth from upstream/cbproto/cbproto.h lines 800-838
/// @{

#define cbRUNLEVEL_UPDATE   78
#define cbPKTTYPE_UPDATESET 0xF1
#define cbPKTTYPE_UPDATEREP 0x71
#define cbPKTDLEN_UPDATE    (sizeof(cbPKT_UPDATE)/4)-2

/// @brief PKT Set:0xF1 Rep:0x71 - Update Packet
///
/// Update the firmware of the NSP.  This will copy data received into files in a temporary location and if
/// completed, on reboot will copy the files to the proper location to run.
typedef struct {
    cbPKT_HEADER cbpkt_header;  ///< packet header

    char   filename[64];    ///< filename to be updated
    uint32_t blockseq;        ///< sequence of the current block
    uint32_t blockend;        ///< last block of the current file
    uint32_t blocksiz;        ///< block size of the current block
    uint8_t  block[512];      ///< block data
} cbPKT_UPDATE;

#define cbPKTDLEN_UPDATE_OLD    (sizeof(cbPKT_UPDATE_OLD)/4)-2

/// @brief PKT Set:0xF1 Rep:0x71 - Old Update Packet
///
/// Update the firmware of the NSP.  This will copy data received into files in a temporary location and if
/// completed, on reboot will copy the files to the proper location to run.
///
/// Since the NSP needs to work with old versions of the firmware, this packet retains the old header format.
typedef struct {
    uint32_t time;            ///< system clock timestamp
    uint16_t chan;            ///< channel identifier
    uint8_t  type;            ///< packet type
    uint8_t  dlen;            ///< length of data field in 32-bit chunks
    char   filename[64];    ///< filename to be updated
    uint32_t blockseq;        ///< sequence of the current block
    uint32_t blockend;        ///< last block of the current file
    uint32_t blocksiz;        ///< block size of the current block
    uint8_t  block[512];      ///< block data
} cbPKT_UPDATE_OLD;

/// @}

#ifdef __cplusplus
}
#endif

#pragma pack(pop)

#endif // CBPROTO_TYPES_H
