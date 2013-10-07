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

//
// Basic channel attributes
//
typedef struct {
    UINT16 id;          // channel id
    char * szLabel;     // Channel label
} BmiChanAttr_t;

hid_t CreateChanAttrType(hid_t loc);

//
// Sample rate attributes
//
typedef struct {
    float  fClock;       // global clock used for this data set
    float  fSampleRate;  // sampling done for this channel
    UINT8  nSampleBits;  // Number of bits in each sample
} BmiSamplingAttr_t;

hid_t CreateSamplingAttrType(hid_t loc);

//
// Channel filter attributes
//
typedef struct {
    // High pass filter info
    UINT32 hpfreq;         // Filter frequency in mHz
    UINT32 hporder;        // Filter order
    UINT16 hptype;         // Filter type
    // Low pass filter info
    UINT32 lpfreq;         // Filter frequency in mHz
    UINT32 lporder;        // Filter order
    UINT16 lptype;         // Filter type
} BmiFiltAttr_t;

hid_t CreateFiltAttrType(hid_t loc);

//
// Channel extra attributes addition 1
//
typedef struct {
    // These may only appear in NEV extra headers
    UINT8 sortCount;       // Number of sorted units
    UINT32 energy_thresh;
    INT32 high_thresh;
    INT32 low_thresh;
} BmiChanExt1Attr_t;

hid_t CreateChanExt1AttrType(hid_t loc);

//
// Channel extra attributes addition 2
//
typedef struct {
    // These may only appear in NSx extra headers
    INT32 digmin;          // Minimum digital value
    INT32 digmax;          // Maximum digital value
    INT32 anamin;          // Minimum analog Value
    INT32 anamax;          // Maximum analog Value
    char anaunit[16];      // Units for the Analog Value (e.g. "mV)
} BmiChanExt2Attr_t;

hid_t CreateChanExt2AttrType(hid_t loc);

//
// Channel extra attributes
//
typedef struct {
    double dFactor;        // nano volts per LSB (used in conversion between digital and analog values)
    UINT8  phys_connector;
    UINT8  connector_pin;
} BmiChanExtAttr_t;

hid_t CreateChanExtAttrType(hid_t loc);

//
// Header may not change with each experiment
//  and thus root-group attribute
//
typedef struct {
    UINT32 nMajorVersion;
    UINT32 nMinorVersion;
    UINT32 nFlags;
    UINT32 nGroupCount; // Number of data groups withing this file
    char * szDate;          // File creation date-time in SQL format
    char  * szApplication;  // Which application created this file
    char * szComment;       // File Comment 
} BmiRootAttr_t;

hid_t CreateRootAttrType(hid_t loc);

//
// Synch general information
//
typedef struct {
    UINT16 id; // video source ID
    float fFps;
    char * szLabel;        // Name of the video source
} BmiSynchAttr_t;

hid_t CreateSynchAttrType(hid_t loc);

//
// Video source general information
//
typedef struct {
    UINT16 type;     // trackable type
    UINT16 trackID;  // trackable ID
    UINT16 maxPoints;
    char * szLabel;  // Name of the trackable
} BmiTrackingAttr_t;

hid_t CreateTrackingAttrType(hid_t loc);

// Spike data (of INT16 samples)
typedef struct {
    UINT32 dwTimestamp;
    UINT8  unit;
    UINT8  res;
    // This must be the last
    INT16  wave[cbMAX_PNTS]; // Currently up to cbMAX_PNTS
} BmiSpike16_t;

hid_t CreateSpike16Type(hid_t loc, UINT16 spikeLength);

// Digital/serial data (of INT16 samples)
typedef struct {
    UINT32 dwTimestamp;
    UINT16  value;
} BmiDig16_t;

hid_t CreateDig16Type(hid_t loc);

// Video synchronization
typedef struct {
    UINT32 dwTimestamp;
    UINT16  split;
    UINT32  frame;
    UINT32  etime; // Elapsed time in milli-seconds
} BmiSynch_t;

hid_t CreateSynchType(hid_t loc);

// Video tracking
typedef struct {
    UINT32 dwTimestamp;
    UINT16 parentID;
    UINT16 nodeCount;
    // This must be the last
    hvl_t coords;
} BmiTracking_t;

// Video tracking
typedef struct {
    UINT32 dwTimestamp;
    UINT16 parentID;
    UINT16 nodeCount;
    // This must be the last
    UINT16 coords[cbMAX_TRACKCOORDS];
} BmiTracking_fl_t;

hid_t CreateTrackingType(hid_t loc, int dim, int width);

#define BMI_COMMENT_LEN 256
// Comment or user event
typedef struct {
    UINT32 dwTimestamp;
    UINT8  flags;
    UINT32 data;
    char szComment[BMI_COMMENT_LEN];
} BmiComment_t;

hid_t CreateCommentType(hid_t loc);

#endif /* N2H5_H_ */
