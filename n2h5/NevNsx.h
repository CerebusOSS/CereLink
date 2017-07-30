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

#include "cbhwlib.h"

#pragma pack(push, 1)

#ifndef WIN32
typedef struct _SYSTEMTIME
{
    uint16_t wYear;
    uint16_t wMonth;
    uint16_t wDayOfWeek;
    uint16_t wDay;
    uint16_t wHour;
    uint16_t wMinute;
    uint16_t wSecond;
    uint16_t wMilliseconds;
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
    uint32_t nPeriod;       // How many ticks per sample (30,000 = rate)
    uint32_t cnChannels;    // How many channels

    // Next comes
    //   uint32_t           // Which channels?
    // then
    //   int16_t wData;   // Data from each channel
} Nsx21Hdr;

typedef struct
{
    char achFileID[8];      // Always set to NEURALCD
    unsigned char nMajor;   // Major version number
    unsigned char nMinor;   // Minor version number
    uint32_t nBytesInHdrs;  // Bytes in all headers also pointer to first data pkt
    char szGroup[16];       // Label of the group
    char szComment[256];    // File comment
    uint32_t nPeriod;       // How many ticks per sample (30,000 = rate)
    uint32_t nResolution;   // Time resolution of time stamps
    SYSTEMTIME isAcqTime;  // Windows time structure
    uint32_t cnChannels;    // How many channels
} Nsx22Hdr;

typedef struct 
{
    char achExtHdrID[2];   // Always set to CC
    uint16_t id;             // Which channel
    char label[16];        // What is the "name" of this electrode?
    uint8_t phys_connector;  // Which connector (e.g. bank 1)
    uint8_t connector_pin;   // Which pin on that collector
    int16_t digmin;          // Minimum digital value
    int16_t digmax;          // Maximum digital value
    int16_t anamin;          // Minimum Analog Value
    int16_t anamax;          // Maximum Analog Value
    char anaunit[16];      // Units for the Analog Value (e.g. "mV)
    uint32_t hpfreq;
    uint32_t hporder;
    int16_t  hptype;
    uint32_t lpfreq;
    uint32_t lporder;
    int16_t  lptype;
} Nsx22ExtHdr;

typedef struct
{
    char nHdr;                // Always set to 0x01
    uint32_t nTimestamp;        // Which channel
    uint32_t nNumDatapoints;    // Number of datapoints

    // Next comes
    //   int16_t DataPoint    // Data points
} Nsx22DataHdr;

// This is the basic header data structure as recorded in the NEV file
typedef struct 
{
    char achFileType[8];            // should always be "NEURALEV"
    uint8_t byFileRevMajor;           // Major version of the file
    uint8_t byFileRevMinor;           // Minor version of the file
    uint16_t wFileFlags;              // currently set to "0x01" to mean all data is 16 bit
    uint32_t dwStartOfData;           // how many bytes are are ALL of the headers
    uint32_t dwBytesPerPacket;        // All "data" is fixed length...what is that length
    uint32_t dwTimeStampResolutionHz; // How many counts per second for the global clock (now 30,000)
    uint32_t dwSampleResolutionHz;    // How many counts per second for the samples (now 30,000 as well)
    SYSTEMTIME isAcqTime;           // Greenwich Mean Time when data was collected
    char szApplication[32];         // Which application created this? NULL terminated
    char szComment[256];            // Comments (NULL terminated)
    uint32_t dwNumOfExtendedHeaders;  // How many extended headers are there?
} NevHdr;

// This is the extra header data structure as recorded in the NEV file
typedef struct
{
    char achPacketID[8];            // "NEUWAV", "NEULABL", "NEUFLT", ...
     union {
         struct {
            uint16_t id;
             union {
                struct {
                    uint8_t  phys_connector;
                    uint8_t  connector_pin;
                    uint16_t digital_factor;
                    uint16_t energy_thresh;
                    int16_t  high_thresh;
                    int16_t  low_thresh;
                    uint8_t  sorted_count;
                    uint8_t  wave_bytes;
                    uint16_t wave_samples;
                    char   achReserved[8];
                } neuwav;
                struct {
                    char   label[16];
                    char   achReserved[6];
                } neulabel;
                struct {
                    uint32_t hpfreq;
                    uint32_t hporder;
                    int16_t  hptype;
                    uint32_t lpfreq;
                    uint32_t lporder;
                    int16_t  lptype;
                    char   achReserved[2];
                } neuflt;
                struct {
                    char   label[16];
                    float  fFps;
                    char   achReserved[2];
                } videosyn;
                struct {
                    uint16_t trackID;
                    uint16_t maxPoints;
                    char   label[16];
                    char   achReserved[2];
                } trackobj;
            };
        };
         struct {
            char   label[16];
            uint8_t  mode;
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
    uint32_t dwTimestamp;
    uint16_t wPacketID;
     union {
        struct {
            uint8_t  byInsertionReason;
            uint8_t  byReserved;
            uint16_t wDigitalValue;
            char   achReserved[260];
        } digital; // digital or serial data
        struct {
            uint8_t  charset;
            uint8_t  flags;
            uint32_t data;
            char   comment[258];
        } comment; // comment data
        struct {
            uint16_t  split;
            uint32_t  frame;
            uint32_t  etime;
            uint32_t  id;
            char   achReserved[250];
        } synch; // synchronization data
        struct {
            uint16_t parentID;
            uint16_t nodeID;
            uint16_t nodeCount;
            uint16_t coordsLength;
            uint16_t coords[cbMAX_TRACKCOORDS];
        } track; // tracking data
        struct {
            uint8_t  unit;
            uint8_t  res;
            int16_t  wave[cbMAX_PNTS];
            char   achReserved[6];
        } spike; // spike data
    };
    char   achReserved[754];
} NevData;

#pragma pack(pop)

#endif /* NEVNSX_H_ */
