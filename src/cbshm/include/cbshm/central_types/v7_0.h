///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   v7_0.h
/// @author Caden Shmookler
/// @date   2026-05-18
///
/// @brief  Central-compatible shared memory structure definitions
///
/// This file defines the shared memory structures using Central's constants to
/// ensure compatibility with Central when it creates shared memory.
///
/// CRITICAL: These structures MUST match Central's cbhwlib.h exactly!
///
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef CBSHM_CENTRAL_TYPES_V7_0_H
#define CBSHM_CENTRAL_TYPES_V7_0_H

#include <cstdint>

// Ensure tight packing for shared memory structures
#pragma pack(push, 1)

namespace cbshm {

namespace central_v7_0 {

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name Protocol Version
/// @{

constexpr uint32_t cbVERSION_MAJOR = 3;
constexpr uint32_t cbVERSION_MINOR = 11;

/// @}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name Central Constants
/// @{

// These MUST match Central's constants
constexpr uint32_t cbMAXPROCS = 1;          ///< Central supports up to 1 NSPs
constexpr uint32_t cbNUM_FE_CHANS = 256;    ///< Central supports 256 FE channels
constexpr uint32_t cbMAXGROUPS = 8;         ///< Sample rate groups
constexpr uint32_t cbMAXFILTS = 32;         ///< Digital filters
constexpr uint32_t cbMAXVIDEOSOURCE = 1;    ///< Maximum number of video sources
constexpr uint32_t cbMAXTRACKOBJ = 20;      ///< Maximum number of trackable objects
constexpr uint32_t cbMAXHOOPS = 4;          ///< Maximum number of hoops for spike sorting
constexpr uint32_t cbMAXSITES = 4;          ///< Maximum number of electrodes in an n-trode group
constexpr uint32_t cbMAXSITEPLOTS = ((cbMAXSITES - 1) * cbMAXSITES / 2);  ///< Combination of 2 out of n
constexpr uint32_t cbMAXUNITS = 5;          ///< Maximum number of sorted units per channel
constexpr uint32_t cbMAX_PNTS = 128;        ///< Maximum spike waveform points
constexpr uint32_t cbMAX_AOUT_TRIGGER = 5;  ///< Maximum number of per-channel (analog output, or digital output) triggers

// Channel counts
constexpr uint32_t cbNUM_ANAIN_CHANS = 16 * cbMAXPROCS;
constexpr uint32_t cbNUM_ANALOG_CHANS = cbNUM_FE_CHANS + cbNUM_ANAIN_CHANS;
constexpr uint32_t cbNUM_ANAOUT_CHANS = 4 * cbMAXPROCS;
constexpr uint32_t cbNUM_AUDOUT_CHANS = 2 * cbMAXPROCS;
constexpr uint32_t cbNUM_ANALOGOUT_CHANS = cbNUM_ANAOUT_CHANS + cbNUM_AUDOUT_CHANS;
constexpr uint32_t cbNUM_DIGIN_CHANS = 1 * cbMAXPROCS;
constexpr uint32_t cbNUM_SERIAL_CHANS = 1 * cbMAXPROCS;
constexpr uint32_t cbNUM_DIGOUT_CHANS = 4 * cbMAXPROCS;

// Total channels
constexpr uint32_t cbMAXCHANS = (cbNUM_ANALOG_CHANS + cbNUM_ANALOGOUT_CHANS +
                                          cbNUM_DIGIN_CHANS + cbNUM_SERIAL_CHANS +
                                          cbNUM_DIGOUT_CHANS);

// Bank definitions
constexpr uint32_t cbCHAN_PER_BANK = 32;
constexpr uint32_t cbNUM_FE_BANKS = cbNUM_FE_CHANS / cbCHAN_PER_BANK;
constexpr uint32_t cbNUM_ANAIN_BANKS = 1;
constexpr uint32_t cbNUM_ANAOUT_BANKS = 1;
constexpr uint32_t cbNUM_AUDOUT_BANKS = 1;
constexpr uint32_t cbNUM_DIGIN_BANKS = 1;
constexpr uint32_t cbNUM_SERIAL_BANKS = 1;
constexpr uint32_t cbNUM_DIGOUT_BANKS = 1;

constexpr uint32_t cbMAXBANKS = (cbNUM_FE_BANKS + cbNUM_ANAIN_BANKS +
                                          cbNUM_ANAOUT_BANKS + cbNUM_AUDOUT_BANKS +
                                          cbNUM_DIGIN_BANKS + cbNUM_SERIAL_BANKS +
                                          cbNUM_DIGOUT_BANKS);

/// @}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name Buffer Size Constants (must be defined before structures)
/// @{

/// Max UDP packet size (from Central)
constexpr uint32_t cbCER_UDP_SIZE_MAX = 1452;

/// Transmit buffer sizes (Central-compatible)
constexpr uint32_t cbXMT_GLOBAL_BUFFLEN = ((cbCER_UDP_SIZE_MAX / 4) * 5000 + 2);  ///< Room for 5000 packet-sized slots
constexpr uint32_t cbXMT_LOCAL_BUFFLEN = ((cbCER_UDP_SIZE_MAX / 4) * 2000 + 2);   ///< Room for 2000 packet-sized slots

/// N-Trode count
constexpr uint32_t cbMAXNTRODES = cbNUM_ANALOG_CHANS / 2;  ///< = 136

/// Analog output gain channels (Central's multi-instrument count)
constexpr uint32_t AOUT_NUM_GAIN_CHANS = cbNUM_ANAOUT_CHANS + cbNUM_AUDOUT_CHANS;  ///< = 6

/// Spike cache constants
constexpr uint32_t cbPKT_SPKCACHEPKTCNT = 400;                          ///< Packets per channel cache
constexpr uint32_t cbPKT_SPKCACHELINECNT = cbNUM_ANALOG_CHANS;  ///< One cache per channel

/// Receive buffer size
constexpr uint32_t cbRECBUFFLEN = cbNUM_FE_CHANS * 32768 * 4;

/// @}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name String Length Constants
/// @{

constexpr uint32_t cbLEN_STR_UNIT = 8;       ///< Length of unit string
constexpr uint32_t cbLEN_STR_LABEL = 16;      ///< Length of label string
constexpr uint32_t cbLEN_STR_FILT_LABEL = 16;      ///< Length of filter label string
constexpr uint32_t cbLEN_STR_IDENT = 64;      ///< Length of identity string
constexpr uint32_t cbLEN_STR_COMMENT = 256;     ///< Length of comment string
constexpr uint32_t cbMAX_COMMENT = 128;     ///< Maximum comment length (must be multiple of 4)

/// @}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name Central config buffer subtypes
/// @{

typedef uint32_t PROCTIME;

/// @brief Cerebus packet header data structure
///
/// Known as 'cbPKT_HEADER_OLD' in 4.0+
typedef struct {
    PROCTIME time;        ///< System clock timestamp
    uint16_t chid;        ///< Channel identifier
    uint8_t  type;        ///< Packet type
    uint8_t  dlen;        ///< Length of data field in 32-bit chunks
} cbPKT_HEADER;

constexpr uint32_t cbPKT_MAX_SIZE = 1024;                    ///< Maximum packet size in bytes
constexpr uint32_t cbPKT_HEADER_SIZE = sizeof(cbPKT_HEADER);    ///< Packet header size in bytes

/// @brief Option table for Central application
///
/// Used for configuration options in Central
typedef struct {
    float fRMSAutoThresholdDistance;    ///< multiplier to use for autothresholding when using RMS to guess noise
    uint32_t reserved[31];              ///< Reserved for future use
} cbOPTIONTABLE;

/// @brief Color table for Central application
///
/// Used for display configuration in Central
typedef struct {
    uint32_t winrsvd[48];       ///< Reserved for Windows
    uint32_t dispback;          ///< Display background color
    uint32_t dispgridmaj;       ///< Display major grid color
    uint32_t dispgridmin;       ///< Display minor grid color
    uint32_t disptext;          ///< Display text color
    uint32_t dispwave;          ///< Display waveform color
    uint32_t dispwavewarn;      ///< Display waveform warning color
    uint32_t dispwaveclip;      ///< Display waveform clipping color
    uint32_t dispthresh;        ///< Display threshold color
    uint32_t dispmultunit;      ///< Display multi-unit color
    uint32_t dispunit[16];      ///< Display unit colors (0 = unclassified)
    uint32_t dispnoise;         ///< Display noise color
    uint32_t dispchansel[3];    ///< Display channel selection colors
    uint32_t disptemp[5];       ///< Display temporary colors
    uint32_t disprsvd[14];      ///< Reserved display colors
} cbCOLORTABLE;

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
    uint32_t sortmethod;  ///< sort method (0=manual, 1=automatic spike sorting)
    uint32_t version;     ///< current version of libraries
} cbPKT_PROCINFO;

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

/// @brief PKT Set:0xA6 Rep:0x26 - Reference Electrode Information.
///
/// This configures a channel to be referenced by another channel.
typedef struct {
    cbPKT_HEADER cbpkt_header;  ///< packet header

    uint32_t chan;           ///< Ignored

    uint32_t nMode;          ///< 0=disabled, 1=filter continuous & spikes, 2=filter spikes
    uint32_t nRefChan;       ///< The reference channel (1 based).
} cbPKT_REFELECFILTINFO;

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
            uint32_t  monsource;      ///< address of channel to monitor
            int32_t   outvalue;       ///< output value
        };
        struct {    // used for digout timed output
            uint16_t  lowsamples;     ///< number of samples to set low for timed output
            uint16_t  highsamples;    ///< number of samples to set high for timed output
            int32_t   offset;         ///< number of samples to offset the transitions for timed output
        };
    };
    uint8_t               trigtype;       ///< trigger type (see cbDOUT_TRIGGER_*)
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

/// @brief PKT Set:0xD2 Rep:0x52 - Auto threshold parameters
///
/// Set the auto threshold parameters
typedef struct {
    cbPKT_HEADER cbpkt_header;  ///< packet header

    float  fThreshold;     ///< current detection threshold
    float  fMultiplier;    ///< multiplier
} cbPKT_SS_DETECT;

/// @brief PKT Set:0xD3 Rep:0x53 - Artifact reject
///
/// Sets the artifact rejection parameters.
typedef struct {
    cbPKT_HEADER cbpkt_header;  ///< packet header

    uint32_t nMaxSimulChans;     ///< how many channels can fire exactly at the same time???
    uint32_t nRefractoryCount;   ///< for how many samples (30 kHz) is a neuron refractory, so can't re-trigger
} cbPKT_SS_ARTIF_REJECT;

/// @brief PKT Set:0xD4 Rep:0x54 - Noise boundary
///
/// Sets the noise boundary parameters
typedef struct {
    cbPKT_HEADER cbpkt_header;  ///< packet header

    uint32_t chan;        ///< which channel we belong to
    float  afc[3];      ///< the center of the ellipsoid
    float  afS[3][3];   ///< an array of the axes for the ellipsoid
} cbPKT_SS_NOISE_BOUNDARY;

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

/// @brief Adaptive Control structure
typedef struct {
    uint32_t nMode;           ///< 0-do not adapt at all, 1-always adapt, 2-adapt if timer not timed out
    float fTimeOutMinutes;    ///< how many minutes until time out
    float fElapsedMinutes;    ///< the amount of time that has elapsed
} cbAdaptControl;

/// @brief PKT Set:0xD7 Rep:0x57 - Spike sorting status (Histogram peak count)
///
/// This packet contains the status of the automatic spike sorting.
typedef struct {
    cbPKT_HEADER cbpkt_header;  ///< packet header

    cbAdaptControl cntlUnitStats;   ///<
    cbAdaptControl cntlNumUnits;    ///<
} cbPKT_SS_STATUS;

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Spike sorting configuration
///
/// Groups all spike-sorting related configuration packets together.
typedef struct {
    cbPKT_FS_BASIS          asBasis[cbMAXCHANS];    ///< PCA basis values per channel
    cbPKT_SS_MODELSET       asSortModel[cbMAXCHANS][cbMAXUNITS + 2];    ///< Sorting models/rules per channel

    //////// These are spike sorting options
    cbPKT_SS_DETECT         pktDetect;        ///< Detection parameters
    cbPKT_SS_ARTIF_REJECT   pktArtifReject;   ///< Artifact rejection parameters
    cbPKT_SS_NOISE_BOUNDARY pktNoiseBoundary[cbNUM_ANALOG_CHANS]; ///< Noise boundaries per channel
    cbPKT_SS_STATISTICS     pktStatistics;    ///< Spike statistics
    cbPKT_SS_STATUS         pktStatus;        ///< Spike sorting status
} cbSPIKE_SORTING;

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

constexpr uint32_t cbMAX_WAVEFORM_PHASES = ((cbPKT_MAX_SIZE - cbPKT_HEADER_SIZE - 24) / 4);   ///< Maximum number of phases in a waveform

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
    uint16_t  trig;              ///< Can be any of cbWAVEFORM_TRIGGER*
    uint16_t  trigChan;          ///< Depends on trig:
                                 ///  for cbWAVEFORM_TRIGGER_DINP* 1-based trigChan (1-16) is digin1, (17-32) is digin2, ...
                                 ///  for cbWAVEFORM_TRIGGER_SPIKEUNIT 1-based trigChan (1-156) is channel number
                                 ///  for cbWAVEFORM_TRIGGER_COMMENTCOLOR trigChan is A->B in A->B->G->R
    uint16_t  trigValue;         ///< Trigger value (spike unit, G-R comment color, ...)
    uint8_t   trigNum;           ///< trigger number (0-based) (can be up to cbMAX_AOUT_TRIGGER-1)
    uint8_t   active;            ///< status of trigger
    cbWaveformData wave;         ///< Actual waveform data
} cbPKT_AOUT_WAVEFORM;

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

constexpr uint32_t cbNPLAY_FNAME_LEN = (cbPKT_MAX_SIZE - cbPKT_HEADER_SIZE - 40);   ///< length of the file name (with terminating null)

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

/// @brief NeuroMotive video source
typedef struct {
    char    name[cbLEN_STR_LABEL];  ///< filename of the video file
    float   fps;                    ///< nominal record fps
} cbVIDEOSOURCE;

/// @brief Track object structure for NeuroMotive
typedef struct {
    char     name[cbLEN_STR_LABEL];  ///< name of the object
    uint16_t type;                   ///< trackable type (cbTRACKOBJ_TYPE_*)
    uint16_t pointCount;             ///< maximum number of points
} cbTRACKOBJ;

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
/// @brief Central's actual binary layout
///
/// This struct matches Central's cbCFGBUFF field order EXACTLY (from cbhwlib.h).
/// It is NOT the same as CereLink's cbConfigBuffer (which reorders fields and adds
/// instrument_status). This struct is used in CENTRAL mode to read Central's shared
/// memory as a CLIENT.
///
/// Key differences from CereLink's cbConfigBuffer:
/// - optiontable/colortable: 3rd/4th fields here (after sysflags), last fields in CereLink
/// - instrument_status: absent here (Central has no such concept)
/// - isLnc: after isWaveform here, before chaninfo in CereLink
/// - hwndCentral: omitted (at end, variable size, not needed)
///
struct cbCFGBUFF {
    uint32_t          version;
    uint32_t          sysflags;
    cbOPTIONTABLE     optiontable;
    cbCOLORTABLE      colortable;
    cbPKT_SYSINFO     sysinfo;
    cbPKT_PROCINFO    procinfo[cbMAXPROCS];
    cbPKT_BANKINFO    bankinfo[cbMAXPROCS][cbMAXBANKS];
    cbPKT_GROUPINFO   groupinfo[cbMAXPROCS][cbMAXGROUPS];
    cbPKT_FILTINFO    filtinfo[cbMAXPROCS][cbMAXFILTS];
    cbPKT_ADAPTFILTINFO adaptinfo[cbMAXPROCS];
    cbPKT_REFELECFILTINFO refelecinfo[cbMAXPROCS];
    cbPKT_CHANINFO    chaninfo[cbMAXCHANS];
    cbSPIKE_SORTING   isSortingOptions;
    cbPKT_NTRODEINFO  isNTrodeInfo[cbMAXNTRODES];
    cbPKT_AOUT_WAVEFORM isWaveform[AOUT_NUM_GAIN_CHANS][cbMAX_AOUT_TRIGGER];
    cbPKT_LNC         isLnc[cbMAXPROCS];
    cbPKT_NPLAY       isNPlay;
    cbVIDEOSOURCE     isVideoSource[cbMAXVIDEOSOURCE];
    cbTRACKOBJ        isTrackObj[cbMAXTRACKOBJ];
    cbPKT_FILECFG     fileinfo;
    // hwndCentral omitted (at end, variable size, not needed by CereLink)
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Transmit buffer for outgoing packets (Global - sent to device)
///
/// Ring buffer for packets waiting to be transmitted to device via UDP.
/// Buffer stores raw packet data as uint32_t words (Central's format).
///
/// This is stored in a separate shared memory segment (not embedded in config buffer)
/// to match Central's architecture.
///
struct cbXMTBUFF {
    uint32_t transmitted;                                   ///< How many packets have been sent
    uint32_t headindex;                                     ///< First empty position (write index)
    uint32_t tailindex;                                     ///< One past last emptied position (read index)
    uint32_t last_valid_index;                              ///< Greatest valid starting index
    uint32_t bufferlen;                                     ///< Number of indices in buffer
    uint32_t buffer[cbXMT_GLOBAL_BUFFLEN];          ///< Ring buffer for packet data
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Local transmit buffer (IPC-only packets)
///
/// Smaller than Global buffer, used for cbSendLoopbackPacket() - packets meant only for
/// local processes, not sent to device.
///
struct cbXMTBUFFLOCAL {
    uint32_t transmitted;                                   ///< How many packets have been sent
    uint32_t headindex;                                     ///< First empty position (write index)
    uint32_t tailindex;                                     ///< One past last emptied position (read index)
    uint32_t last_valid_index;                              ///< Greatest valid starting index
    uint32_t bufferlen;                                     ///< Number of indices in buffer
    uint32_t buffer[cbXMT_LOCAL_BUFFLEN];           ///< Ring buffer for packet data
};

///////////////////////////////////////////////////////////////////////////////////////////////////
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

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Spike cache buffer
///
/// Caches recent spike packets for each channel to allow quick access without
/// scanning the entire receive buffer.
///
struct cbSPKCACHE {
    uint32_t chid;                                          ///< Channel ID
    uint32_t pktcnt;                                        ///< Number of packets that can be saved
    uint32_t pktsize;                                       ///< Size of individual packet
    uint32_t head;                                          ///< Where to place next packet (circular)
    uint32_t valid;                                         ///< How many packets since last config
    cbPKT_SPK spkpkt[cbPKT_SPKCACHEPKTCNT];        ///< Circular buffer of cached spikes
};

struct cbSPKBUFF {
    uint32_t flags;                                         ///< Status flags
    uint32_t chidmax;                                       ///< Maximum channel ID
    uint32_t linesize;                                      ///< Size of each cache line
    uint32_t spkcount;                                      ///< Total spike count
    cbSPKCACHE cache[cbPKT_SPKCACHELINECNT]; ///< Per-channel spike caches
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Instrument status flags (bit field)
///
/// Used to track which instruments are active in shared memory
///
enum class InstrumentStatus : uint32_t {
    INACTIVE = 0x00000000,  ///< Instrument slot is not in use
    ACTIVE   = 0x00000001,  ///< Instrument is active and has data
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief PC Status buffer (flattened from cbPcStatus class)
///
/// IMPROVEMENT: Flattened to C struct for ABI stability and cross-compiler compatibility.
/// Central uses a C++ class which is fragile across different build environments.
///
enum class NSPStatus : uint32_t {
    NSP_INIT = 0,
    NSP_NOIPADDR = 1,
    NSP_NOREPLY = 2,
    NSP_FOUND = 3,
    NSP_INVALID = 4
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Unit Selection
///
/// Packet which says that these channels are now selected
typedef struct
{
    cbPKT_HEADER cbpkt_header;  ///< packet header

    int32_t  lastchan;         ///< Which channel was clicked last.
    uint16_t   abyUnitSelections[cbMAXCHANS];     ///< one for each channel, channels are 0 based here, shows units selected
} cbPKT_UNIT_SELECTION;

struct cbPcStatus {
    // Public data
    cbPKT_UNIT_SELECTION isSelection[cbMAXPROCS];   ///< Unit selection per instrument

    // Status fields (was private in cbPcStatus)
    int32_t  m_iBlockRecording;                             ///< Recording block counter
    uint32_t m_nPCStatusFlags;                              ///< PC status flags
    uint32_t m_nNumFEChans;                                 ///< Number of FE channels
    uint32_t m_nNumAnainChans;                              ///< Number of analog input channels
    uint32_t m_nNumAnalogChans;                             ///< Number of analog channels
    uint32_t m_nNumAoutChans;                               ///< Number of analog output channels
    uint32_t m_nNumAudioChans;                              ///< Number of audio channels
    uint32_t m_nNumAnalogoutChans;                          ///< Number of analog output channels
    uint32_t m_nNumDiginChans;                              ///< Number of digital input channels
    uint32_t m_nNumSerialChans;                             ///< Number of serial channels
    uint32_t m_nNumDigoutChans;                             ///< Number of digital output channels
    uint32_t m_nNumTotalChans;                              ///< Total channel count
    // VER: Everything below here added at 4.0+
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Receive buffer for incoming packets (simplified for Phase 2)
///
struct cbRECBUFF {
    uint32_t received;                          ///< Number of packets received
    PROCTIME lasttime;                          ///< Last timestamp
    uint32_t headwrap;                          ///< Head wrap counter
    uint32_t headindex;                         ///< Current head index
    uint32_t buffer[cbRECBUFFLEN];      ///< Packet buffer
};

} // namespace central_v7_0

} // namespace cbshm

#pragma pack(pop)

#endif // CBSHMEM_CENTRAL_TYPES_V7_0_H
