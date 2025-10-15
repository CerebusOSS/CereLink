//////////////////////////////////////////////////////////////////////////////
//
// (c) Copyright 2012 Blackrock Microsystems
//
// $Workfile: n2h5.h $
// $Archive: /n2h5/n2h5.h $
// $Revision: 1 $
// $Date: 11/1/12 1:00p $
// $Author: Ehsan $
//
// $NoKeywords: $
//
//////////////////////////////////////////////////////////////////////////////
//
// Note: 
//   for simple tools to better understand the format keep variable length types to the end
//

#ifndef N2H5_H_
#define N2H5_H_

#include "cbhwlib.h"
#include "hdf5.h"
#include "hdf5_hl.h"

#define cbFIRST_DIGIN_CHAN    (cbNUM_FE_CHANS + cbNUM_ANAIN_CHANS + cbNUM_ANAOUT_CHANS + cbNUM_AUDOUT_CHANS)
#define cbFIRST_SERIAL_CHAN   (cbFIRST_DIGIN_CHAN + cbNUM_DIGIN_CHANS)

//
// Basic channel attributes
//
typedef struct {
    uint16_t id;          // channel id
    char szLabel[64];     // Channel label
} BmiChanAttr_t;

hid_t CreateChanAttrType(hid_t loc);

//
// Sample rate attributes
//
typedef struct {
    float  fClock;       // global clock used for this data set
    float  fSampleRate;  // sampling done for this channel
    uint8_t  nSampleBits;  // Number of bits in each sample
} BmiSamplingAttr_t;

hid_t CreateSamplingAttrType(hid_t loc);

//
// Channel filter attributes
//
typedef struct {
    // High pass filter info
    uint32_t hpfreq;         // Filter frequency in mHz
    uint32_t hporder;        // Filter order
    uint16_t hptype;         // Filter type
    // Low pass filter info
    uint32_t lpfreq;         // Filter frequency in mHz
    uint32_t lporder;        // Filter order
    uint16_t lptype;         // Filter type
} BmiFiltAttr_t;

hid_t CreateFiltAttrType(hid_t loc);

//
// Channel extra attributes addition 1
//
typedef struct {
    // These may only appear in NEV extra headers
    uint8_t sortCount;       // Number of sorted units
    uint32_t energy_thresh;
    int32_t high_thresh;
    int32_t low_thresh;
} BmiChanExt1Attr_t;

hid_t CreateChanExt1AttrType(hid_t loc);

//
// Channel extra attributes addition 2
//
typedef struct {
    // These may only appear in NSx extra headers
    int32_t digmin;          // Minimum digital value
    int32_t digmax;          // Maximum digital value
    int32_t anamin;          // Minimum analog Value
    int32_t anamax;          // Maximum analog Value
    char anaunit[16];      // Units for the Analog Value (e.g. "mV)
} BmiChanExt2Attr_t;

hid_t CreateChanExt2AttrType(hid_t loc);

//
// Channel extra attributes
//
typedef struct {
    double dFactor;        // nano volts per LSB (used in conversion between digital and analog values)
    uint8_t  phys_connector;
    uint8_t  connector_pin;
} BmiChanExtAttr_t;

hid_t CreateChanExtAttrType(hid_t loc);

//
// Header may not change with each experiment
//  and thus root-group attribute
//
typedef struct {
    uint32_t nMajorVersion;
    uint32_t nMinorVersion;
    uint32_t nFlags;
    uint32_t nGroupCount; // Number of data groups withing this file
    char szDate[64];         // File creation date-time in SQL format
    char szApplication[64];  // Which application created this file
    char szComment[1024];    // File Comment
} BmiRootAttr_t;

hid_t CreateRootAttrType(hid_t loc);

//
// Synch general information
//
typedef struct {
    uint16_t id; // video source ID
    float fFps;
    char szLabel[64];        // Name of the video source
} BmiSynchAttr_t;

hid_t CreateSynchAttrType(hid_t loc);

//
// Video source general information
//
typedef struct {
    uint16_t type;     // trackable type
    uint16_t trackID;  // trackable ID
    uint16_t maxPoints;
    char szLabel[128];  // Name of the trackable
} BmiTrackingAttr_t;

hid_t CreateTrackingAttrType(hid_t loc);

// Spike data (of int16_t samples)
typedef struct {
    uint32_t dwTimestamp;
    uint8_t  unit;
    uint8_t  res;
    // This must be the last
    int16_t  wave[cbMAX_PNTS]; // Currently up to cbMAX_PNTS
} BmiSpike16_t;

hid_t CreateSpike16Type(hid_t loc, uint16_t spikeLength);

// Digital/serial data (of int16_t samples)
typedef struct {
    uint32_t dwTimestamp;
    uint16_t  value;
} BmiDig16_t;

hid_t CreateDig16Type(hid_t loc);

// Video synchronization
typedef struct {
    uint32_t dwTimestamp;
    uint16_t  split;
    uint32_t  frame;
    uint32_t  etime; // Elapsed time in milli-seconds
} BmiSynch_t;

hid_t CreateSynchType(hid_t loc);

// Video tracking
typedef struct {
    uint32_t dwTimestamp;
    uint16_t parentID;
    uint16_t nodeCount;
    // This must be the last
    hvl_t coords;
} BmiTracking_t;

// Video tracking
typedef struct {
    uint32_t dwTimestamp;
    uint16_t parentID;
    uint16_t nodeCount;
    uint32_t etime;
    // This must be the last
    uint16_t coords[cbMAX_TRACKCOORDS];
} BmiTracking_fl_t;

hid_t CreateTrackingType(hid_t loc, int dim, int width);

#define BMI_COMMENT_LEN 256
// Comment or user event
typedef struct {
    uint32_t dwTimestamp;
    uint8_t  flags;
    uint32_t data;
    char szComment[BMI_COMMENT_LEN];
} BmiComment_t;

hid_t CreateCommentType(hid_t loc);

#endif /* N2H5_H_ */
