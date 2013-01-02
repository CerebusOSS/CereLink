//////////////////////////////////////////////////////////////////////////////
//
// (c) Copyright 2012 Blackrock Microsystems
//
// $Workfile: NevNsx.h $
// $Archive: /n2h5/NevNsx.h $
// $Revision: 1 $
// $Date: 11/1/12 1:00p $
// $Author: Ehsan $
//
// $NoKeywords: $
//
//////////////////////////////////////////////////////////////////////////////

#ifndef NEVNSX_H_
#define NEVNSX_H_

#include "../common/cbhwlib.h"

#pragma pack(push, 1)

#ifndef WIN32
typedef struct _SYSTEMTIME
{
    UINT16 wYear;
    UINT16 wMonth;
    UINT16 wDayOfWeek;
    UINT16 wDay;
    UINT16 wHour;
    UINT16 wMinute;
    UINT16 wSecond;
    UINT16 wMilliseconds;
} SYSTEMTIME;
#endif

// SQL time format
typedef struct
{
        char        chYear[4];
        char        ch4;
        char        chMonth[2];
        char        ch7;
        char        chDay[2];
        char        ch10;
        char        chHour[2];
        char        ch13;
        char        chMinute[2];
        char        ch16;
        char        chSecond[2];
        char        ch19;
        char        chMicroSecond[6];
        char        chNull;
} TIMSTM;

typedef struct
{
    char achFileID[8];      // Always set to NEURALSG
    char szGroup[16];       // Label of the group
    UINT32 nPeriod;       // How many ticks per sample (30,000 = rate)
    UINT32 cnChannels;    // How many channels

    // Next comes
    //   UINT32           // Which channels?
    // then
    //   INT16 wData;   // Data from each channel
} Nsx21Hdr;

typedef struct
{
    char achFileID[8];      // Always set to NEURALCD
    unsigned char nMajor;   // Major version number
    unsigned char nMinor;   // Minor version number
    UINT32 nBytesInHdrs;  // Bytes in all headers also pointer to first data pkt
    char szGroup[16];       // Label of the group
    char szComment[256];    // File comment
    UINT32 nPeriod;       // How many ticks per sample (30,000 = rate)
    UINT32 nResolution;   // Time resolution of time stamps
    SYSTEMTIME isAcqTime;  // Windows time structure
    UINT32 cnChannels;    // How many channels
} Nsx22Hdr;

typedef struct 
{
    char achExtHdrID[2];   // Always set to CC
    UINT16 id;             // Which channel
    char label[16];        // What is the "name" of this electrode?
    UINT8 phys_connector;  // Which connector (e.g. bank 1)
    UINT8 connector_pin;   // Which pin on that collector
    INT16 digmin;          // Minimum digital value
    INT16 digmax;          // Maximum digital value
    INT16 anamin;          // Minimum Analog Value
    INT16 anamax;          // Maximum Analog Value
    char anaunit[16];      // Units for the Analog Value (e.g. "mV)
    UINT32 hpfreq;
    UINT32 hporder;
    INT16  hptype;
    UINT32 lpfreq;
    UINT32 lporder;
    INT16  lptype;
} Nsx22ExtHdr;

typedef struct
{
    char nHdr;                // Always set to 0x01
    UINT32 nTimestamp;        // Which channel
    UINT32 nNumDatapoints;    // Number of datapoints

    // Next comes
    //   INT16 DataPoint    // Data points
} Nsx22DataHdr;

// This is the basic header data structure as recorded in the NEV file
typedef struct 
{
    char achFileType[8];            // should always be "NEURALEV"
    UINT8 byFileRevMajor;           // Major version of the file
    UINT8 byFileRevMinor;           // Minor version of the file
    UINT16 wFileFlags;              // currently set to "0x01" to mean all data is 16 bit
    UINT32 dwStartOfData;           // how many bytes are are ALL of the headers
    UINT32 dwBytesPerPacket;        // All "data" is fixed length...what is that length
    UINT32 dwTimeStampResolutionHz; // How many counts per second for the global clock (now 30,000)
    UINT32 dwSampleResolutionHz;    // How many counts per second for the samples (now 30,000 as well)
    SYSTEMTIME isAcqTime;           // Greenwich Mean Time when data was collected
    char szApplication[32];         // Which application created this? NULL terminated
    char szComment[256];            // Comments (NULL terminated)
    UINT32 dwNumOfExtendedHeaders;  // How many extended headers are there?
} NevHdr;

// This is the extra header data structure as recorded in the NEV file
typedef struct
{
    char achPacketID[8];            // "NEUWAV", "NEULABL", "NEUFLT", ...
     union {
         struct {
            UINT16 id;
             union {
                struct {
                    UINT8  phys_connector;
                    UINT8  connector_pin;
                    UINT16 digital_factor;
                    UINT16 energy_thresh;
                    INT16  high_thresh;
                    INT16  low_thresh;
                    UINT8  sorted_count;
                    UINT8  wave_bytes;
                    UINT16 wave_samples;
                    char   achReserved[8];
                } neuwav;
                struct {
                    char   label[16];
                    char   achReserved[6];
                } neulabel;
                struct {
                    UINT32 hpfreq;
                    UINT32 hporder;
                    INT16  hptype;
                    UINT32 lpfreq;
                    UINT32 lporder;
                    INT16  lptype;
                    char   achReserved[2];
                } neuflt;
                struct {
                    char   label[16];
                    float  fFps;
                    char   achReserved[2];
                } videosyn;
                struct {
                    UINT16 trackID;
                    UINT16 maxPoints;
                    char   label[16];
                    char   achReserved[2];
                } trackobj;
            };
        };
         struct {
            char   label[16];
            UINT8  mode;
            char   achReserved[7];
        } diglabel;
         struct {
            char   label[24];
        } mapfile;
    };
} NevExtHdr;

// This is the data structure as recorded in the NEV file
//  The size is larger than any packet to safely read them from file
typedef struct
{
    UINT32 dwTimestamp;
    UINT16 wPacketID;
     union {
        struct {
            UINT8  byInsertionReason;
            UINT8  byReserved;
            UINT16 wDigitalValue;
            char   achReserved[260];
        } digital; // digital or serial data
        struct {
            UINT8  charset;
            UINT8  flags;
            UINT32 data;
            char   comment[258];
        } comment; // comment data
        struct {
            UINT16  split;
            UINT32  frame;
            UINT32  etime;
            UINT32  id;
            char   achReserved[250];
        } synch; // synchronization data
        struct {
            UINT16 parentID;
            UINT16 nodeID;
            UINT16 nodeCount;
            UINT16 coordsLength;
            UINT16 coords[cbMAX_TRACKCOORDS];
        } track; // tracking data
        struct {
            UINT8  unit;
            UINT8  res;
            INT16  wave[cbMAX_PNTS];
            char   achReserved[6];
        } spike; // spike data
    };
    char   achReserved[754];
} NevData;

#pragma pack(pop)

#endif /* NEVNSX_H_ */
