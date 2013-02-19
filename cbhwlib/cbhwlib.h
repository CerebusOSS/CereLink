/* =STS=> cbhwlib.h[1687].aa78   submit   SMID:82 */
///////////////////////////////////////////////////////////////////////////////////////////////////
//
// (c) Copyright 2002-2008 Cyberkinetics, Inc.
// (c) Copyright 2008-2011 Blackrock Microsystems
//
// $Workfile: cbhwlib.h $
// $Archive: /Cerebus/Human/LinuxApps/player/cbhwlib.h $
// $Revision: 50 $
// $Date: 5/17/05 10:07a $
// $Author: Dsebald $
//
// $NoKeywords: $
//
///////////////////////////////////////////////////////////////////////////////////////////////////

//
// PURPOSE:
//
// This code libary defines an standardized control and data acess interface for microelectrode
// neurophysiology equipment.  The interface allows several applications to simultaneously access
// the control and data stream for the equipment through a central control application.  This
// central application governs the flow of information to the user interface applications and
// performs hardware specific data processing for the instruments.  This is diagrammed as follows:
//
//   Instruments <---> Central Control App <--+--> Cerebus Library <---> User Application
//                                            +--> Cerebus Library <---> User Application
//                                            +--> Cerebus Library <---> User Application
//
// The Central Control Application can also exchange window/application configuration data so that
// the Central Application can save and restore instrument and application window settings.
//
// All hardware configuration, hardware acknowledgement, and data information are passed on the
// system in packet form.  Cerebus user applications interact with the hardware in the system by
// sending and receiving configuration and data packets through the Central Control Application.
// In order to aid efficiency, the Central Control App caches information regarding hardware
// configuration so that multiple applications do not need to request hardware configuration
// packets from the system.  The Neuromatic Library provides high-level functions for retreiving
// data from this cache and high-level functions for transmitting configuration packets to the
// hardware.  Neuromatic applications must provide a callback function for receiving data and
// configuration acknowledgement packets.
//
// The data stream from the hardware is composed of "neural data" to be saved in experiment files
// and "preview data" that provides information such as compressed real-time channel data for
// scrolling displays and Line Noise Cancellation waveforms to update the user.
//
///////////////////////////////////////////////////////////////////////////////////////////////////

// Standard include guards
#ifndef CBHWLIB_H_INCLUDED
#define CBHWLIB_H_INCLUDED

// Only standard headers might be included here
#if defined(WIN32)
#ifndef _CRT_SECURE_NO_DEPRECATE
#define _CRT_SECURE_NO_DEPRECATE
#endif
// It has to be in this order for right version of sockets
#ifdef NO_AFX
#include <winsock2.h>
#include <windows.h>
#endif
#endif
#ifdef __cplusplus
#include <string.h>
#endif

#pragma pack(push, 1)

#define cbVERSION_MAJOR  3
#define cbVERSION_MINOR	 8

// Version history:
//  3.8 - 10 Oct 2011 hls - added map info packet
//        15 Apr 2011 tkr - added poll packet
//        13 Apr 2011 tkr - added initialize auto impedance packet
//        04 Apr 2011 tkr - added impedance data packet
//        30 Mar 2011 tkr - added patient info for file recording
//        27 Aug 2010 rpa - Added NTrode ellipse support for NTrode sorting
//  3.7 - 04 May 2010 eaz - Added comment, video synchronization and NeuroMotive configuration packets
//        25 May 2010 eaz - Added tracking, logging and patient trigger configuration packets
//        03 Jul 2010 eaz - Added NeuroMotive status and file config reporting
//        03 Jul 2010 eaz - Added nPlay stepping
//        04 Jul 2010 eaz - Added cbOpen under custom stand-alone application
//  3.6 - 10 Nov 2008 jrs - Added extra space for 3D boundaries & Packet for
//                          recalculating PCA Basis Vectors and Values
//        10 Apr 2009 eaz - Added PCA basis calculation info
//        12 May 2009 eaz - Added LNC parameters and type (nominal frequency, lock channel and method)
//        20 May 2009 eaz - Added LNC waveform display scaling
//        20 May 2009 hls - Added software reference electrode per channel
//        21 May 2009 eaz - Added Auto/Manual threshold packet
//        14 Jun 2009 eaz - Added Waveform Collection Matrix info
//        23 Jun 2009 eaz - Added Video Synch packet
//        25 Jun 2009 eaz - Added stand-alone application capability to library
//        20 Oct 2009 eaz - Expanded the file config packet to control and monitor recording
//        02 Nov 2009 eaz - Expanded nPlay packet to control and monitor nPlay
//  3.5 -  1 Aug 2007 hls - Added env min/max for threshold preview
//  3.4 - 12 Jun 2007 mko - Change Spike Sort Override Circles to Ellipses
//        31 May 2007 hls - Added cbPKTTYPE_REFELECFILTSET
//        19 Jun 2007 hls - Added cbPKT_SS_RESET_MODEL
//  3.3 - 27 Mar 2007 mko - Include angle of rotation with Noise Boundary variables
//  3.2 - 7  Dec 2006 mko - Make spike width variable - to max length of 128 samples
//        20 Dec 2006 hls - move wave to end of spike packet
//  3.1 - 25 Sep 2006 hls - Added unit mapping functionality
//        17 Sep 2006 djs - Changed from noise line to noise ellipse and renamed
//                          variables with NOISE_LINE to NOISE_BOUNDARY for
//                          better generalization context
//        15 Aug 2006 djs - Changed exponential measure to histogram correlation
//                          measure and added histogram peak count algorithm
//        21 Jul 2006 djs/hls - Added exponential measure autoalg
//                    djs/hls - Changed exponential measure to histogram correlation
//                              measure and added histogram peak count algorithm
//                    mko     - Added protocol checking to procinfo system
//  3.0 - 15 May 2006 hls - Merged the Manual & Auto Sort protocols
//  2.5 - 18 Nov 2005 hls - Added cbPKT_SS_STATUS
//  2.4 - 10 Nov 2005 kfk - Added cbPKT_SET_DOUT
//  2.3 - 02 Nov 2005 kfk - Added cbPKT_SS_RESET
//  2.2 - 27 Oct 2005 kfk - Updated cbPKT_SS_STATISTICS to include params to affect
//                          spike sorting rates
//
//  2.1 - 22 Jun 2005 kfk - added all packets associated with spike sorting options
//                          cbPKTDLEN_SS_DETECT
//                          cbPKT_SS_ARTIF_REJECT
//                          cbPKT_SS_NOISE_LINE
//                          cbPKT_SS_STATISTICS
//
//  2.0 - 11 Apr 2005 kfk - Redifined the Spike packet to include classificaiton data
//
//  1.8 - 27 Mar 2006 ab  - Added cbPKT_SS_STATUS
//  1.7 -  7 Feb 2006 hls - Added anagain to cbSCALING structure
//                          to support different external gain
//  1.6 - 25 Feb 2005 kfk - Added cbPKTTYPE_ADAPTFILTSET and
//                          cbGetAdaptFilter() and cbSGetAdaptFilter()
//  1.5 - 30 Dec 2003 kfk - Added cbPKTTYPE_REPCONFIGALL and cbPKTTYPE_PREVREP
//                          redefined cbPKTTYPE_REQCONFIGALL
//  1.4 - 15 Dec 2003 kfk - Added "last_valid_index" to the send buffer
//                          Added cbDOUT_TRACK
//


///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Fixed storage size definitions for delcared variables
// (includes conditional testing so that there is no clash with win32 headers)
//
///////////////////////////////////////////////////////////////////////////////////////////////////

typedef signed char     INT8;
typedef unsigned char   UINT8;

typedef signed short    INT16;
typedef unsigned short  UINT16;

typedef signed int      INT32;
typedef unsigned int    UINT32;

#ifndef WIN32
# if __WORDSIZE == 64
typedef long int                 INT64;
typedef unsigned long int       UINT64;
# else
typedef long long int            INT64;
typedef unsigned long long int  UINT64;
# endif
typedef const char * LPCSTR;
typedef void * HANDLE;
typedef UINT32 COLORREF;
typedef INT32 LONG;
typedef LONG * LPLONG;
typedef LONG * PLONG;
typedef UINT8 BYTE;
typedef UINT32 DWORD;
typedef unsigned int UINT;
#define RGB(r,g,b) ((UINT8)r + ((UINT8)g << 8) + ((UINT8)b << 16))
#define MAKELONG(a, b)      ((a & 0xffff) | ((b & 0xffff) << 16))
#ifndef FALSE
#define FALSE 0
#define TRUE 1
#endif
typedef UINT32 BOOL;
#define MAX_PATH 1024
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Default Cerebus networking connection parameters
//
//  All connections should be defined here
//
///////////////////////////////////////////////////////////////////////////////////////////////////
#define cbNET_UDP_ADDR_INST         "192.168.137.1"   // Cerebus default address
#define cbNET_UDP_ADDR_CNT          "192.168.137.128" // NSP default control address
#define cbNET_UDP_ADDR_BCAST        "192.168.137.255" // NSP default broadcast address
#define cbNET_UDP_PORT_BCAST        1002              // Neuroflow Data Port
#define cbNET_UDP_PORT_CNT          1001              // Neuroflow Control Port
// maximum udp datagram size used to transport cerebus packets, taken from MTU size
#define cbCER_UDP_SIZE_MAX          1024 // Note that multiple packets may reside in one udp datagram

///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Maximum entity ranges for static declarations using this version of the library
//
///////////////////////////////////////////////////////////////////////////////////////////////////
#define cbNSP1      1

///////////////////////////////////////////////////////////
#define cbRAWGROUP  6   // group number for raw data feed
///////////////////////////////////////////////////////////

// Front End Channels   (128)   ->  (256)
// Analog Input         (16)    ->  (32)
// Analog Output        (4)     ->  (8)
// Audio Output         (2)     ->  (4)
// Digital Output       (4)     ->  (8)
// Digital Input        (1)     ->  (2)
// Serial Input         (1)     ->  (2)
//---------------------------------------
// Total (actually 156) (160)   ->  (320)
//
#define cbMAXOPEN   4                               // Maximum number of open cbhwlib's (nsp's)
#define cbMAXPROCS  1                               // Number of NSP's
#define cbMAXBANKS  16
#define cbMAXGROUPS 8                               // number of sample rate groups
#define cbMAXFILTS  32
#define cbMAXVIDEOSOURCE 1                          // maximum number of video sources
#define cbMAXTRACKOBJ 20                            // maximum number of trackable objects
#define cbMAXCHANS  160
#define cbNUM_ANALOG_CHANS    144
#define cbMAXUNITS  5
#define cbMAXHOOPS  4
#define cbMAX_AOUT_TRIGGER 5                        // maximum number of per-channel analog output triggers

// N-Trode definitions
#define cbMAXSITES  4                               //*** maximum number of electrodes that can be included in an n-trode group  -- eventually want to support octrodes
#define cbMAXSITEPLOTS ((cbMAXSITES - 1) * cbMAXSITES / 2)  // combination of 2 out of n is n!/((n-2)!2!) -- the only issue is the display

// Channel Definitions
#define cbNUM_FE_CHANS        128                                       // #Front end channels
#define cbNUM_ANAIN_CHANS     16                                        // #Analog Input channels
#define cbNUM_ANAOUT_CHANS    4                                         // #Analog Output channels
#define cbNUM_AUDOUT_CHANS    2                                         // #Audio Output channels
#define cbNUM_ANALOGOUT_CHANS (cbNUM_ANAOUT_CHANS + cbNUM_AUDOUT_CHANS) // Total Analog Output
#define cbNUM_DIGIN_CHANS     1                                         // #Digital Input channels
#define cbNUM_SERIAL_CHANS    1                                         // #Serial Input channels
#define cbNUM_DIGOUT_CHANS    4                                         // #Digital Output channels

// Total of all channels = 156
#define cbMAXCHANS            160
#define cbFIRST_FE_CHAN       0                                          // 0   First Front end channel
#define cbFIRST_ANAIN_CHAN    cbNUM_FE_CHANS                             // 256 First Analog Input channel
#define cbFIRST_ANAOUT_CHAN   (cbFIRST_ANAIN_CHAN + cbNUM_ANAIN_CHANS)   // 288 First Analog Output channel
#define cbFIRST_AUDOUT_CHAN   (cbFIRST_ANAOUT_CHAN + cbNUM_ANAOUT_CHANS) // 296 First Audio Output channel
#define cbFIRST_DIGIN_CHAN    (cbFIRST_AUDOUT_CHAN + cbNUM_AUDOUT_CHANS) // 300 First Digital Input channel
#define cbFIRST_SERIAL_CHAN   (cbFIRST_DIGIN_CHAN + cbNUM_DIGIN_CHANS)   // 302 First Serial Input channel
#define cbFIRST_DIGOUT_CHAN   (cbFIRST_SERIAL_CHAN + cbNUM_SERIAL_CHANS) // 304 First Digital Output channel

// Bank definitions - NOTE: If any of the channel types have more than cbCHAN_PER_BANK channels, the banks must be increased accordingly
#define cbCHAN_PER_BANK       32                                         // number of 32 channel banks == 1024
#define cbNUM_FE_BANKS        (cbNUM_FE_CHANS / cbCHAN_PER_BANK)         // number of Front end banks
#define cbNUM_ANAIN_BANKS     1                                          // number of Analog Input banks
#define cbNUM_ANAOUT_BANKS    1                                          // number of Analog Output banks
#define cbNUM_AUDOUT_BANKS    1                                          // number of Audio Output banks
#define cbNUM_DIGIN_BANKS     1                                          // number of Digital Input banks
#define cbNUM_SERIAL_BANKS    1                                          // number of Serial Input banks
#define cbNUM_DIGOUT_BANKS    1                                          // number of Digital Output banks

// Custom digital filters
#define cbFIRST_DIGITAL_FILTER  13  // (0-based) filter number, must be less than cbMAXFILTS
#define cbNUM_DIGITAL_FILTERS   4

// This is the number of aout chans with gain. Conveniently, the
// 4 Analog Outputs and the 2 Audio Outputs are right next to each other
// in the channel numbering sequence.
#define AOUT_NUM_GAIN_CHANS             (cbNUM_ANAOUT_CHANS + cbNUM_AUDOUT_CHANS)

#define cbMAXUNITS            5                                         // hard coded to 5 in some places
#define cbMAXNTRODES          (cbNUM_ANALOG_CHANS / 2)                  // minimum is stereotrode so max n-trodes is max chans / 2

// These defines moved from cbHwlibHi to have a central place
#define MAX_CHANS_FRONT_END    cbNUM_FE_CHANS
#define MIN_CHANS_ANALOG_IN    (MAX_CHANS_FRONT_END + 1)
#define MAX_CHANS_ANALOG_IN    (MAX_CHANS_FRONT_END + cbNUM_ANAIN_CHANS)
#define MIN_CHANS_ANALOG_OUT   (MAX_CHANS_ANALOG_IN + 1)
#define MAX_CHANS_ANALOG_OUT   (MAX_CHANS_ANALOG_IN + cbNUM_ANAOUT_CHANS)
#define MIN_CHANS_AUDIO        (MAX_CHANS_ANALOG_OUT + 1)
#define MAX_CHANS_AUDIO        (MAX_CHANS_ANALOG_OUT + cbNUM_AUDOUT_CHANS)
#define MIN_CHANS_DIGITAL_IN   (MAX_CHANS_AUDIO + 1)
#define MAX_CHANS_DIGITAL_IN   (MAX_CHANS_AUDIO + cbNUM_DIGIN_CHANS)
#define MIN_CHANS_SERIAL       (MAX_CHANS_DIGITAL_IN + 1)
#define MAX_CHANS_SERIAL       (MAX_CHANS_DIGITAL_IN + cbNUM_SERIAL_CHANS)
#define MIN_CHANS_DIGITAL_OUT  (MAX_CHANS_SERIAL + 1)
#define MAX_CHANS_DIGITAL_OUT  (MAX_CHANS_SERIAL + cbNUM_DIGOUT_CHANS)
#define SCALE_LNC_COUNT        17
#define SCALE_CONTINUOUS_COUNT 17
#define SCALE_SPIKE_COUNT      23

// Special unit classification values
enum UnitClassification
{
    UC_UNIT_UNCLASSIFIED    = 0,        // This unit is not classified
    UC_UNIT_ANY             = 254,      // Any unit including noise
    UC_UNIT_NOISE           = 255,      // This unit is really noise
};

///////////////////////////////////////////////////////////////////////////////////////////////////
//  Some of the string length constants
#define cbLEN_STR_UNIT          8
#define cbLEN_STR_LABEL         16
#define cbLEN_STR_FILT_LABEL    16
#define cbLEN_STR_IDENT         64
///////////////////////////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Library Result defintions
//
///////////////////////////////////////////////////////////////////////////////////////////////////

typedef unsigned int cbRESULT;

#define cbRESULT_OK                 0   // Function executed normally
#define cbRESULT_NOLIBRARY          1   // The library was not properly initialized
#define cbRESULT_NOCENTRALAPP       2   // Unable to access the central application
#define cbRESULT_LIBINITERROR       3   // Error attempting to initialize library error
#define cbRESULT_MEMORYUNAVAIL      4   // Not enough memory available to complete the operation
#define cbRESULT_INVALIDADDRESS     5   // Invalid Processor or Bank address
#define cbRESULT_INVALIDCHANNEL     6   // Invalid channel ID passed to function
#define cbRESULT_INVALIDFUNCTION    7   // Channel exists, but requested function is not available
#define cbRESULT_NOINTERNALCHAN     8   // No internal channels available to connect hardware stream
#define cbRESULT_HARDWAREOFFLINE    9   // Hardware is offline or unavailable
#define cbRESULT_DATASTREAMING     10   // Hardware is streaming data and cannot be configured
#define cbRESULT_NONEWDATA         11   // There is no new data to be read in
#define cbRESULT_DATALOST          12   // The Central App incoming data buffer has wrapped
#define cbRESULT_INVALIDNTRODE     13   // Invalid NTrode number passed to function
#define cbRESULT_BUFRECALLOCERR    14   // Receive buffer could not be allocated
#define cbRESULT_BUFGXMTALLOCERR   15   // Global transmit buffer could not be allocated
#define cbRESULT_BUFLXMTALLOCERR   16   // Local transmit buffer could not be allocated
#define cbRESULT_BUFCFGALLOCERR    17   // Configuration buffer could not be allocated
#define cbRESULT_BUFPCSTATALLOCERR 18   // PC status buffer could not be allocated
#define cbRESULT_BUFSPKALLOCERR    19   // Spike cache buffer could not be allocated
#define cbRESULT_EVSIGERR          20   // Couldn't create shared event signal
#define cbRESULT_SOCKERR           21   // Generic socket creation error
#define cbRESULT_SOCKOPTERR        22   // Socket option error (possibly permission issue)
#define cbRESULT_SOCKMEMERR        23   // Socket memory assignment error
#define cbRESULT_INSTINVALID       24   // Invalid range or instrument address
#define cbRESULT_SOCKBIND          25   // Cannot bind to any address (possibly no Instrument network)
#define cbRESULT_SYSLOCK           26   // Cannot (un)lock the system resources (possiblly resource busy)

///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Library Initialization Functions
//
// The standard procedure for intializing and using this library is to:
//   1) Intialize the library with cbOpen().
//   2) Obtain system and channel configuration info with cbGet* commands.
//   3) Configure the system channels with appropriate cbSet* commands.
//   4) Receive data through the callback function
//   5) Repeat steps 2/3/4 as needed until the application closes.
//   6) call cbClose() to de-allocate and free the library
//
///////////////////////////////////////////////////////////////////////////////////////////////////


UINT32 cbVersion(void);
// Returns the major/minor revision of the current library in the upper/lower UINT16 fields.

///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Systemwide Inquiry and Configuration Functions
//
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
// Open multiple instances of library as stand-alone or under Central application
cbRESULT cbOpen(BOOL bStandAlone = FALSE, UINT32 nInstance = 0);
// Initializes the Neuromatic library (and establishes a link to the Central Control Application if bStandAlone is FALSE).
// This function must be called before any other functions are called from this library.
// Returns OK, NOCENTRALAPP, LIBINITERROR, MEMORYUNVAIL, or HARDWAREOFFLINE

cbRESULT cbClose(BOOL bStandAlone = FALSE, UINT32 nInstance = 0);
// Close the library (must match how library is openned)

cbRESULT cbCheckApp(const char * lpName);
// Check if an application is running using its mutex

cbRESULT cbAquireSystemLock(const char * lpName, HANDLE & hLock);
cbRESULT cbReleaseSystemLock(const char * lpName, HANDLE & hLock);
// Aquire or release application system lock

#define  cbINSTINFO_READY      0x0001     // Instrument is connected
#define  cbINSTINFO_LOCAL      0x0002     // Instrument runs on the localhost
#define  cbINSTINFO_NPLAY      0x0004     // Instrument is nPlay
#define  cbINSTINFO_CEREPLEX   0x0008     // Instrument is Cereplex
#define  cbINSTINFO_EMULATOR   0x0010     // Instrument is Emulator
cbRESULT cbGetInstInfo(UINT32 *instInfo, UINT32 nInstance = 0);
// Purpose: get instrument information.

cbRESULT cbGetLatency(UINT32 *nLatency, UINT32 nInstance = 0);
// Purpose: get instrument latency.

// Returns instrument information
// Returns cbRESULT_OK if successful, cbRESULT_NOLIBRARY if library was never initialized.
cbRESULT cbGetSystemClockFreq(UINT32 *freq, UINT32 nInstance = 0);
// Retrieves the system timestamp/sample clock frequency (in Hz) from the Central App cache.
//
// Returns: cbRESULT_OK if data successfully retrieved.
//          cbRESULT_NOLIBRARY if the library was not properly initialized


cbRESULT cbGetSystemClockTime(UINT32 *time, UINT32 nInstance = 0);
// Retrieves the last 32-bit timestamp from the Central App cache.
//
// Returns: cbRESULT_OK if data successfully retrieved.
//          cbRESULT_NOLIBRARY if the library was not properly initialized

#endif

// .nev packet data structure (1024 bytes total)
typedef struct {
    UINT32 time;        // system clock timestamp
    UINT16 chid;        // channel identifier
    UINT8  type;        // packet type
    UINT8  dlen;        // length of data field in 32-bit chunks
    UINT32 data[254];   // data buffer (up to 1016 bytes)
} nevPKT_GENERIC;


#define nevPKT_HEADER_SIZE 8  // define the size of the packet header in bytes
#define cbPKT_MAX_SIZE        1024                     // the maximum size of the packet in bytes for 128 channels

#define cbPKT_HEADER_SIZE     sizeof(cbPKT_HEADER)     // define the size of the packet header in bytes
#define cbPKT_HEADER_32SIZE   (cbPKT_HEADER_SIZE / 4)  // define the size of the packet header in UINT32's

// Cerebus packet header data structure (must be a multiple of UINT32)
typedef struct {
    UINT32 time;        // system clock timestamp
    UINT16 chid;        // channel identifier
    UINT8 type;      // packet type
    UINT8 dlen;      // length of data field in 32-bit chunks
    
} cbPKT_HEADER;


// Generic Cerebus packet data structure (1024 bytes total)
typedef struct {
    UINT32 time;        // system clock timestamp
    UINT16 chid;        // channel identifier
    UINT8 type;      // packet type
    UINT8 dlen;      // length of data field in 32-bit chunks
    
    UINT32 data[(cbPKT_MAX_SIZE - cbPKT_HEADER_SIZE) / 4];   // data buffer (up to 1016 bytes)
} cbPKT_GENERIC;

#ifdef __cplusplus

// Shuts down the programming library and frees any resources linked in cbOpen()
// Returns cbRESULT_OK if successful, cbRESULT_NOLIBRARY if library was never initialized.
// Updates the read pointers in the memory area so that
// all of the un-read packets are ignored. In other words, it
// initializes all of the pointers so that the begging of read time is NOW.
cbRESULT cbMakePacketReadingBeginNow(UINT32 nInstance = 0);
///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Data checking and processing functions
//
// To get data from the shared memory buffers used in the Central App, the user can:
// 1) periodically poll for new data using a multimedia or windows timer
// 2) create a thread that uses a Win32 Event synchronization object to que the data polling
//
///////////////////////////////////////////////////////////////////////////////////////////////////

enum cbLevelOfConcern
{
    LOC_LOW,                // Time for sippen lemonaide
    LOC_MEDIUM,             // Step up to the plate
    LOC_HIGH,               // Put yer glass down
    LOC_CRITICAL,           // Get yer but in gear
    LOC_COUNT               // How many level of concerns are there
};

cbRESULT cbCheckforData(cbLevelOfConcern & nLevelOfConcern, UINT32 *pktstogo = NULL, UINT32 nInstance = 0);
// The pktstogo and timetogo are optional fields (NULL if not used) that returns the number of new
// packets and timestamps that need to be read to catch up to the buffer.
//
// Returns: cbRESULT_OK    if there is new data in the buffer
//          cbRESULT_NONEWDATA if there is no new data available
//          cbRESULT_DATALOST if the Central App incoming data buffer has wrapped the read buffer


cbRESULT cbWaitforData(UINT32 nInstance = 0);
// Executes a WaitForSingleObject command to wait for the Central App event signal
//
// Returns: cbRESULT_OK    if there is new data in the buffer
//          cbRESULT_NONEWDATA if the function timed out after 250ms
//          cbRESULT_DATALOST if the Central App incoming data buffer has wrapped the read buffer
///////////////////////////////////////////////////////////////////////////////////////////////////
//
// NEV file definitions
//
///////////////////////////////////////////////////////////////////////////////////////////////////

cbPKT_GENERIC *cbGetNextPacketPtr(UINT32 nInstance = 0);
// Returns pointer to next packet in the shared memory space.  If no packet available, returns NULL

// Cerebus Library function to send packets via the Central Application Queue
cbRESULT cbSendPacket(void * pPacket, UINT32 nInstance = 0);
cbRESULT cbSendLoopbackPacket(void * pPacket, UINT32 nInstance = 0);

#endif
///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Configuration/Report Packet Definitions (chid = 0x8000)
//
///////////////////////////////////////////////////////////////////////////////////////////////////

#define cbPKTCHAN_CONFIGURATION         0x8000          // Channel # to mean configuration


///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Detailed Processor and Bank Inquiry Functions
//
// Instrumentation equipment is organized into three levels within this library:
//   1) Signal Processors - Signal processing and data distribution layer
//   2) Signal Banks      - Groups of channels with similar properties and physical locations
//   3) Signal Channels   - Individual physical channels with one or more functions
//
//   Computer --+-- Signal Processor ----- Signal Bank --+-- Channel
//              |                      |                 +-- Channel
//              |                      |                 +-- Channel
//              |                      |
//              |                      +-- Signal Bank --+-- Channel
//              |                                        +-- Channel
//              |                                        +-- Channel
//              |
//              +-- Signal Processor ----- Signal Bank --+-- Channel
//                                     |                 +-- Channel
//                                     |                 +-- Channel
//                                     |
//                                     +-- Signal Bank --+-- Channel
//                                                       +-- Channel
//                                                       +-- Channel
//
// In this implementation, Signal Channels are numbered from 1-32767 across the entire system and
// are associated to Signal Banks and Signal Processors by the hardware.
//
// Signal Processors are numbered 1-8 and Signal Banks are numbered from 1-16 within a specific
// Signal Processor.  Processor and Bank numbers are NOT required to be continuous and
// are a function of the hardware configuration.  For example, an instrumentation set-up could
// include Processors at addresses 1, 2, and 7.  Configuration packets given to the computer to
// describe the hardware also report the channel enumeration.
//
///////////////////////////////////////////////////////////////////////////////////////////////////


#define cbSORTMETHOD_MANUAL     0
#define cbSORTMETHOD_AUTO       1

// Signal Processor Configuration Structure
typedef struct {
    UINT32 idcode;      // manufacturer part and rom ID code of the Signal Processor
    char   ident[cbLEN_STR_IDENT];   // ID string with the equipment name of the Signal Processor
    UINT32 chanbase;    // lowest channel identifier claimed by this processor
    UINT32 chancount;   // number of channel identifiers claimed by this processor
    UINT32 bankcount;   // number of signal banks supported by the processor
    UINT32 groupcount;  // number of sample groups supported by the processor
    UINT32 filtcount;   // number of digital filters supported by the processor
    UINT32 sortcount;   // number of channels supported for spike sorting (reserved for future)
    UINT32 unitcount;   // number of supported units for spike sorting    (reserved for future)
    UINT32 hoopcount;   // number of supported hoops for spike sorting    (reserved for future)
    UINT32 sortmethod;  // sort method  (0=manual, 1=automatic spike sorting)
    UINT32 version;     // current version of libraries
} cbPROCINFO;

// Signal Bank Configuration Structure
typedef struct {
    UINT32 idcode;     // manufacturer part and rom ID code of the module addressed to this bank
    char   ident[cbLEN_STR_IDENT];  // ID string with the equipment name of the Signal Bank hardware module
    char   label[cbLEN_STR_LABEL];  // Label on the instrument for the signal bank, eg "Analog In"
    UINT32 chanbase;   // lowest channel identifier claimed by this bank
    UINT32 chancount;  // number of channel identifiers claimed by this bank
} cbBANKINFO;

typedef struct {
    char    name[cbLEN_STR_LABEL];
    float   fps;        // nominal record fps
} cbVIDEOSOURCE;

#ifdef __cplusplus
cbRESULT cbGetVideoSource(char *name, float *fps, UINT32 id, UINT32 nInstance = 0);
cbRESULT cbSetVideoSource(const char *name, float fps, UINT32 id, UINT32 nInstance = 0);
// Get/Set the video source parameters.
//
// Returns: cbRESULT_OK if data successfully retrieved.
//          cbRESULT_NOLIBRARY if the library was not properly initialized.
#endif

#define cbTRACKOBJ_TYPE_UNDEFINED  0
#define cbTRACKOBJ_TYPE_2DMARKERS  1
#define cbTRACKOBJ_TYPE_2DBLOB     2
#define cbTRACKOBJ_TYPE_3DMARKERS  3
#define cbTRACKOBJ_TYPE_2DBOUNDARY 4
#define cbTRACKOBJ_TYPE_1DSIZE     5
typedef struct {
    char    name[cbLEN_STR_LABEL];
    UINT16  type;        // trackable type (cbTRACKOBJ_TYPE_*)
    UINT16  pointCount;  // maximum number of points
} cbTRACKOBJ;

#ifdef __cplusplus
cbRESULT cbGetTrackObj(char *name, UINT16 *type, UINT16 *pointCount, UINT32 id, UINT32 nInstance = 0);
cbRESULT cbSetTrackObj(const char *name, UINT16 type, UINT16 pointCount, UINT32 id, UINT32 nInstance = 0);
// Get/Set the trackable object parameters.
//
// Returns: cbRESULT_OK if data successfully retrieved.
//          cbRESULT_NOLIBRARY if the library was not properly initialized.
#endif

#define  cbFILTTYPE_PHYSICAL      0x0001
#define  cbFILTTYPE_DIGITAL       0x0002
#define  cbFILTTYPE_ADAPTIVE      0x0004
#define  cbFILTTYPE_NONLINEAR     0x0008
#define  cbFILTTYPE_BUTTERWORTH   0x0100
#define  cbFILTTYPE_CHEBYCHEV     0x0200
#define  cbFILTTYPE_BESSEL        0x0400
#define  cbFILTTYPE_ELLIPTICAL    0x0800
typedef struct {
    char    label[cbLEN_STR_FILT_LABEL];
    UINT32  hpfreq;     // high-pass corner frequency in milliHertz
    UINT32  hporder;    // high-pass filter order
    UINT32  hptype;     // high-pass filter type
    UINT32  lpfreq;     // low-pass frequency in milliHertz
    UINT32  lporder;    // low-pass filter order
    UINT32  lptype;     // low-pass filter type
} cbFILTDESC;

typedef struct {
    UINT32 bEnabled;    // BOOL implemented as UINT32 - for structure alignment at paragraph boundary
    INT16  nAmplPos;
    INT16  nAmplNeg;
} cbAMPLITUDEREJECT;

typedef struct {
    INT16       nOverride;
    INT16       afOrigin[3];
    INT16       afShape[3][3];
    INT16       aPhi;
    UINT32      bValid; // is this unit in use at this time?
                        // BOOL implemented as UINT32 - for structure alignment at paragraph boundary
} cbMANUALUNITMAPPING;

#define cbCHAN_EXISTS       0x00000001  // Channel id is allocated
#define cbCHAN_CONNECTED    0x00000002  // Channel is connected and mapped and ready to use
#define cbCHAN_ISOLATED     0x00000004  // Channel is electrically isolated
#define cbCHAN_AINP         0x00000100  // Channel has analog input capabilities
#define cbCHAN_AOUT         0x00000200  // Channel has analog output capabilities
#define cbCHAN_DINP         0x00000400  // Channel has digital input capabilities
#define cbCHAN_DOUT         0x00000800  // Channel has digital output capabilities

#ifdef __cplusplus
cbRESULT cbGetChanCaps(UINT32 chan, UINT32 *chancaps, UINT32 nInstance = 0);
// Retreives the channel capabilities from the Central App Cache.
//
// Returns: cbRESULT_OK if data successfully retreived or packet successfully queued to be sent.
//          cbRESULT_INVALIDCHANNEL if the specified channel is not mapped or does not exist.
//          cbRESULT_NOLIBRARY if the library was not properly initialized.
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Digital Input Inquiry and Configuration Functions
//
///////////////////////////////////////////////////////////////////////////////////////////////////

#define  cbDINP_SERIALMASK   0x000000FF  // Bit mask used to detect RS232 Serial Baud Rates
#define  cbDINP_BAUD2400     0x00000001  // RS232 Serial Port operates at 2400   (n-8-1)
#define  cbDINP_BAUD9600     0x00000002  // RS232 Serial Port operates at 9600   (n-8-1)
#define  cbDINP_BAUD19200    0x00000004  // RS232 Serial Port operates at 19200  (n-8-1)
#define  cbDINP_BAUD38400    0x00000008  // RS232 Serial Port operates at 38400  (n-8-1)
#define  cbDINP_BAUD57600    0x00000010  // RS232 Serial Port operates at 57600  (n-8-1)
#define  cbDINP_BAUD115200   0x00000020  // RS232 Serial Port operates at 115200 (n-8-1)
#define  cbDINP_1BIT         0x00000100  // Port has a single input bit (eg single BNC input)
#define  cbDINP_8BIT         0x00000200  // Port has 8 input bits
#define  cbDINP_16BIT        0x00000400  // Port has 16 input bits
#define  cbDINP_32BIT        0x00000800  // Port has 32 input bits
#define  cbDINP_ANYBIT       0x00001000  // Capture the port value when any bit changes.
#define  cbDINP_WRDSTRB      0x00002000  // Capture the port when a word-write line is strobed
#define  cbDINP_PKTCHAR      0x00004000  // Capture packets using an End of Packet Character
#define  cbDINP_PKTSTRB      0x00008000  // Capture packets using an End of Packet Logic Input
#define  cbDINP_MONITOR      0x00010000  // Port controls other ports or system events
#define  cbDINP_REDGE        0x00020000  // Capture the port value when any bit changes lo-2-hi (rising edge)
#define  cbDINP_FEDGE        0x00040000  // Capture the port value when any bit changes hi-2-lo (falling edge)
#define  cbDINP_STRBANY      0x00080000  // Capture packets using 8-bit strobe/8-bit any Input
#define  cbDINP_STRBRIS      0x00100000  // Capture packets using 8-bit strobe/8-bit rising edge Input
#define  cbDINP_STRBFAL      0x00200000  // Capture packets using 8-bit strobe/8-bit falling edge Input
#define  cbDINP_MASK         (cbDINP_ANYBIT | cbDINP_WRDSTRB | cbDINP_PKTCHAR | cbDINP_PKTSTRB | cbDINP_MONITOR | cbDINP_REDGE | cbDINP_FEDGE | cbDINP_STRBANY | cbDINP_STRBRIS | cbDINP_STRBFAL)

#ifdef __cplusplus

cbRESULT cbGetDinpCaps(UINT32 chan, UINT32 *dinpcaps, UINT32 nInstance = 0);
// Retreives the channel's digital input capabilities from the Central App Cache.
// Port Capabilities are reported as compbined cbDINPOPT_* flags.  Zero = no DINP capabilities.
//
// Returns: cbRESULT_OK if data successfully retreived.
//          cbRESULT_INVALIDCHANNEL if the specified channel is not mapped or does not exist.
//          cbRESULT_NOLIBRARY if the library was not properly initialized.


cbRESULT cbGetDinpOptions(UINT32 chan, UINT32 *options, UINT32 *eopchar, UINT32 nInstance = 0);
cbRESULT cbSetDinpOptions(UINT32 chan, UINT32 options, UINT32 eopchar, UINT32 nInstance = 0);
// Get/Set the Digital Input Port options for the specified channel.
//
// Port options are expressed as a combined set of cbDINP_* option flags, for example:
// a) cbDINP_SERIAL + cbDINP_BAUDxx = capture single 8-bit RS232 serial values.
// b) cbDINP_SERIAL + cbDINP_BAUDxx + cbDINP_PKTCHAR = capture serial packets that are terminated
//      with an end of packet character (only the lower 8 bits are used).
// c) cbDINP_1BIT + cbDINP_ANYBIT = capture the changes of a single digital input line.
// d) cbDINP_xxBIT + cbDINP_ANYBIT = capture the xx-bit input word when any bit changes.
// e) cbDINP_xxBIT + cbDINP_WRDSTRB = capture the xx-bit input based on a word-strobe line.
// f) cbDINP_xxBIT + cbDINP_WRDSTRB + cbDINP_PKTCHAR = capture packets composed of xx-bit words
//      in which the packet is terminated with the specified end-of-packet character.
// g) cbDINP_xxBIT + cbDINP_WRDSTRB + cbDINP_PKTLINE = capture packets composed of xx-bit words
//      in which the last character of a packet is accompanyied with an end-of-pkt logic signal.
// h) cbDINP_xxBIT + cbDINP_REDGE = capture the xx-bit input word when any bit goes from low to hi.
// i) cbDINP_xxBIT + dbDINP_FEDGE = capture the xx-bit input word when any bit goes from hi to low.
//
// NOTE: If the end-of-packet character value (eopchar) is not used in the options, it is ignored.
//
// Add cbDINP_PREVIEW to the option set to get preview updates (cbPKT_PREVDINP) at each word,
// not only when complete packets are sent.
//
// The Get function returns values from the Central Control App cache.  The Set function validates
// that the specified options are available and then queues a cbPKT_SETDINPOPT packet.  The system
// acknowledges this change with a cbPKT_ACKDINPOPT packet.
//
// Returns: cbRESULT_OK if data successfully retreived or packet successfully queued to be sent.
//          cbRESULT_INVALIDFUNCTION a requested option is not available on that channel.
//          cbRESULT_INVALIDCHANNEL if the specified channel is not mapped or does not exist.
//          cbRESULT_NOLIBRARY if the library was not properly initialized.
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Digital Output Inquiry and Configuration Functions
//
///////////////////////////////////////////////////////////////////////////////////////////////////


#define  cbDOUT_SERIALMASK          0x000000FF  // Port operates as an RS232 Serial Connection
#define  cbDOUT_BAUD2400            0x00000001  // Serial Port operates at 2400   (n-8-1)
#define  cbDOUT_BAUD9600            0x00000002  // Serial Port operates at 9600   (n-8-1)
#define  cbDOUT_BAUD19200           0x00000004  // Serial Port operates at 19200  (n-8-1)
#define  cbDOUT_BAUD38400           0x00000008  // Serial Port operates at 38400  (n-8-1)
#define  cbDOUT_BAUD57600           0x00000010  // Serial Port operates at 57600  (n-8-1)
#define  cbDOUT_BAUD115200          0x00000020  // Serial Port operates at 115200 (n-8-1)
#define  cbDOUT_1BIT                0x00000100  // Port has a single output bit (eg single BNC output)
#define  cbDOUT_8BIT                0x00000200  // Port has 8 output bits
#define  cbDOUT_16BIT               0x00000400  // Port has 16 output bits
#define  cbDOUT_32BIT               0x00000800  // Port has 32 output bits
#define  cbDOUT_VALUE               0x00010000  // Port can be manually configured
#define  cbDOUT_TRACK               0x00020000  // Port should track the most recently selected channel
#define  cbDOUT_FREQUENCY           0x00040000  // Port can output a frequency
#define  cbDOUT_MONITOR_UNIT0       0x01000000  // Can monitor unit 0 = UNCLASSIFIED
#define  cbDOUT_MONITOR_UNIT1       0x02000000  // Can monitor unit 1
#define  cbDOUT_MONITOR_UNIT2       0x04000000  // Can monitor unit 2
#define  cbDOUT_MONITOR_UNIT3       0x08000000  // Can monitor unit 3
#define  cbDOUT_MONITOR_UNIT4       0x10000000  // Can monitor unit 4
#define  cbDOUT_MONITOR_UNIT5       0x20000000  // Can monitor unit 5
#define  cbDOUT_MONITOR_UNIT_ALL    0x3F000000  // Can monitor ALL units
#define  cbDOUT_MONITOR_SHIFT_TO_FIRST_UNIT 24  // This tells us how many bit places to get to unit 1

#ifdef __cplusplus

cbRESULT cbGetDoutCaps(UINT32 chan, UINT32 *doutcaps, UINT32 nInstance = 0);
// Retreives the channel's digital output capabilities from the Central Control App Cache.
// Port Capabilities are reported as compbined cbDOUTOPT_* flags.  Zero = no DINP capabilities.
//
// Returns: cbRESULT_OK if data successfully retreived.
//          cbRESULT_INVALIDCHANNEL if the specified channel is not mapped or does not exist.
//          cbRESULT_NOLIBRARY if the library was not properly initialized.


cbRESULT cbGetDoutOptions(UINT32 chan, UINT32 *options, UINT32 *monchan, UINT32 *doutval, UINT32 nInstance = 0);
cbRESULT cbSetDoutOptions(UINT32 chan, UINT32 options, UINT32 monchan, UINT32 doutval, UINT32 nInstance = 0);
// Get/Set the Digital Output Port options for the specified channel.
//
// The only changable DOUT options in this version of the interface libraries are baud rates for
// serial output ports.  These are set with the cbDOUTOPT_BAUDxx options.
//
// The Get function returns values from the Central Control App cache.  The Set function validates
// that the specified options are available and then queues a cbPKT_SETDOUTOPT packet.  The system
// acknowledges this change with a cbPKT_REPDOUTOPT packet.
//
// Returns: cbRESULT_OK if data successfully retreived or packet successfully queued to be sent.
//          cbRESULT_INVALIDCHANNEL if the specified channel is not mapped or does not exist.
//          cbRESULT_NOINTERNALCHAN if there is no internal channel for mapping the in->out chan
//          cbRESULT_NOLIBRARY if the library was not properly initialized.
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Analog Input Inquiry and Configuration Functions
//
// The analog input processing in this library assumes the following signal flow structure:
//
//   Input with  --+-- Adaptive LNC filter -- Adaptive Filter --+-- Sampling Stream Filter -- Sample Group
//    physical     |                                            |
//     filter      +-- Raw Preview                              |
//                                                              +-- Adaptive Filter -- Spike Stream Filter --+-- Spike Processing
//                                                              |                         |
//                                                              |                         +-- Spike Preview
//                                                              +-- LNC Preview
//
// Adaptive Filter (above) is one or the other depending on settings, never both!
//
// This system forks the signal into 2 separate streams: a continuous stream and a spike stream.
// All simpler systems are derived from this structure and unincluded elements are bypassed or
// ommitted, for example the structure of the NSAS neural channels would be:
//
//   Input with --------+-- Spike Processing       ( NOTE: the physical filter is tuned )
//    physical          |                          (   for spikes and spike processing  )
//     filter           +-- Spike Preview
//
///////////////////////////////////////////////////////////////////////////////////////////////////


// In this system, analog values are represented by 16-bit signed integers.  The cbSCALING
// structure gives the mapping between the signal's analog space and the converted digital values.
//
//                (anamax) ---
//                          |
//                          |
//                          |                      \      --- (digmax)
//                        analog   === Analog to ===\      |
//                        range    ===  Digital  ===/   digital
//                          |                      /     range
//                          |                              |
//                          |                             --- (digmin)
//                (anamin) ---
//
// The analog range extent values are reported in 32-bit integers, along with a unit description.
// Units should be given with traditional metric scales such as P, M, K, m, u(for micro), n, p,
// etc and they are limited to 8 ASCII characters for description.
//
// The anamin and anamax represent the min and max values of the analog signal.  The digmin and
// digmax values are their cooresponding converted digital values.  If the signal is inverted in
// the scaling conversion, the digmin value will be greater than the digmax value.
//
// For example if a +/-5V signal is mapped into a +/-1024 digital value, the preferred unit
// would be "mV", anamin/max = +/- 5000, and digmin/max= +/-1024.
typedef struct {
    INT16   digmin;     // digital value that cooresponds with the anamin value
    INT16   digmax;     // digital value that cooresponds with the anamax value
    INT32   anamin;     // the minimum analog value present in the signal
    INT32   anamax;     // the maximum analog value present in the signal
    INT32   anagain;    // the gain applied to the default analog values to get the analog values
    char    anaunit[cbLEN_STR_UNIT]; // the unit for the analog signal (eg, "uV" or "MPa")
} cbSCALING;


#define  cbAINP_RAWPREVIEW          0x00000001      // Generate scrolling preview data for the raw channel
#define  cbAINP_LNC                 0x00000002      // Line Noise Cancellation
#define  cbAINP_LNCPREVIEW          0x00000004      // Retrieve the LNC correction waveform
#define  cbAINP_SMPSTREAM           0x00000010      // stream the analog input stream directly to disk
#define  cbAINP_SMPFILTER           0x00000020      // Digitally filter the analog input stream
#define  cbAINP_RAWSTREAM           0x00000040      // Raw data stream available
#define  cbAINP_SPKSTREAM           0x00000100      // Spike Stream is available
#define  cbAINP_SPKFILTER           0x00000200      // Selectable Filters
#define  cbAINP_SPKPREVIEW          0x00000400      // Generate scrolling preview of the spike channel
#define  cbAINP_SPKPROC             0x00000800      // Channel is able to do online spike processing
#define  cbAINP_OFFSET_CORRECT_CAP  0x00001000      // Offset correction mode (0-disabled 1-enabled)

#ifdef __cplusplus
cbRESULT cbGetAinpCaps(UINT32 chan, UINT32 *ainpcaps, cbSCALING *physcalin, cbFILTDESC *phyfiltin, UINT32 nInstance = 0);
// Retreives the channel's analog input capabilities from the Central Control App Cache.
// Capabilities are reported as combined cbAINP_* flags.  Zero = no AINP capabilities.
//
// Returns: cbRESULT_OK if data successfully retreived.
//          cbRESULT_INVALIDCHANNEL if the specified channel is not mapped or does not exist.
//          cbRESULT_NOLIBRARY if the library was not properly initialized.
#endif

#define  cbAINP_LNC_OFF             0x00000000      // Line Noise Cancellation disabled
#define  cbAINP_LNC_RUN_HARD        0x00000001      // Hardware-based LNC running and adapting according to the adaptation const
#define  cbAINP_LNC_RUN_SOFT        0x00000002      // Software-based LNC running and adapting according to the adaptation const
#define  cbAINP_LNC_HOLD            0x00000004      // LNC running, but not adapting
#define  cbAINP_LNC_MASK            0x00000007      // Mask for LNC Flags
#define  cbAINP_REFELEC_LFPSPK      0x00000010      // Apply reference electrode to LFP & Spike
#define  cbAINP_REFELEC_SPK         0x00000020      // Apply reference electrode to Spikes only
#define  cbAINP_REFELEC_MASK        0x00000030      // Mask for Reference Electrode flags
#define  cbAINP_RAWSTREAM_ENABLED   0x00000040      // Raw data stream enabled
#define  cbAINP_OFFSET_CORRECT      0x00000100      // Offset correction mode (0-disabled 1-enabled)


#ifdef __cplusplus
cbRESULT cbGetAinpOpts(UINT32 chan, UINT32 *ainpopts, UINT32 *LNCrate, UINT32 *refElecChan, UINT32 nInstance = 0);
cbRESULT cbSetAinpOpts(UINT32 chan, const UINT32 ainpopts,  UINT32 LNCrate, const UINT32 refElecChan, UINT32 nInstance = 0);
// Get and Set the user-assigned amplitude reject values.
//
// Returns: cbRESULT_OK if data successfully retreived or packet successfully queued to be sent.
//          cbRESULT_INVALIDCHANNEL if the specified channel is not mapped or does not exist.
//          cbRESULT_NOLIBRARY if the library was not properly initialized.
//
// The LNC configuration is composed of an adaptation rate and a mode variable.  The rate sets the
// first order decay of the filter according to:
//
//    newLNCvalue = (LNCrate/65536)*(oldLNCvalue) + ((65536-LNCrate)/65536)*LNCsample
//
// The relationships between the adaptation time constant in sec, line frequency in Hz and the
// the LNCrate variable are given below:
//
//         time_constant = 1 / ln[ (LNCrate/65536)^(-line_freq) ]
//
//         LNCrate = 65536 * e^[-1/(time_constant*line_freq)]
//
// The LNCmode sets whether the channel LNC block is disabled, running, or on hold.
//
// To set multiple channels on hold or run, pass channel=0.  In this case, the LNCrate is ignored
// and the run or hold value passed to the LNCmode variable is applied to all LNC enabled channels.


cbRESULT cbGetAinpScaling(UINT32 chan, cbSCALING *scaling, UINT32 nInstance = 0);
cbRESULT cbSetAinpScaling(UINT32 chan, cbSCALING *scaling, UINT32 nInstance = 0);
// Get/Set the user-specified scaling for the channel.  The digmin and digmax values of the user
// specified scaling must be within the digmin and digmax values for the physical channel mapping.
//
// Returns: cbRESULT_OK if data successfully retreived.
//          cbRESULT_INVALIDCHANNEL if the specified channel is not mapped or does not exist.
//          cbRESULT_NOLIBRARY if the library was not properly initialized.

cbRESULT cbGetAinpDisplay(UINT32 chan, INT32 *smpdispmin, INT32 *smpdispmax, INT32 *spkdispmax, INT32 *lncdispmax, UINT32 nInstance = 0);
cbRESULT cbSetAinpDisplay(UINT32 chan, INT32  smpdispmin, INT32  smpdispmax, INT32  spkdispmax, INT32  lncdispmax, UINT32 nInstance = 0);
// Get and Set the display ranges used by User applications.  smpdispmin/max set the digital value
// range that should be displayed for the sampled analog stream.  Spike streams are assumed to be
// symmetric about zero so that spikes should be plotted from -spkdispmax to +spkdispmax.  Passing
// zero as a scale instructs the Central app to send the cached value.  Fields with NULL pointers
// are ignored by the library.
//
// Returns: cbRESULT_OK if data successfully retreived or packet successfully queued to be sent.
//          cbRESULT_INVALIDCHANNEL if the specified channel is not mapped or does not exist.
//          cbRESULT_NOLIBRARY if the library was not properly initialized.
#endif

// <<NOTE: these constants are based on the preview request packet identifier>>
#define cbAINPPREV_LNC    0x81
#define cbAINPPREV_STREAM 0x82
#define cbAINPPREV_ALL    0x83

#ifdef __cplusplus

cbRESULT cbSetAinpPreview(UINT32 chan, UINT32 prevopts, UINT32 nInstance = 0);
// Requests preview packets for a specific channel.
// Setting the AINPPREV_LNC option gets a single LNC update waveform.
// Setting the AINPPREV_STREAMS enables compressed preview information.
//
// A channel ID of zero requests the specified preview packets from all active ainp channels.
//
// Returns: cbRESULT_OK if data successfully retreived.
//          cbRESULT_INVALIDCHANNEL if the specified channel is not mapped or does not exist.
//          cbRESULT_NOLIBRARY if the library was not properly initialized.


//////////////////////////////////////////////////////////////
// AINP Continuous Stream Functions


cbRESULT cbGetAinpSampling(UINT32 chan, UINT32 *filter, UINT32 *group, UINT32 nInstance = 0);
cbRESULT cbSetAinpSampling(UINT32 chan, UINT32 filter,  UINT32 group, UINT32 nInstance = 0);
// Get/Set the periodic sample group for the channel.  Continuous sampling is performed in
// groups with each Neural Signal Processor.  There are up to 4 groups for each processor.
// A group number of zero signifies that the channel is not part of a continuous sample group.
// filter = 1 to cbNFILTS, 0 is reserved for the null filter case
//
// Returns: cbRESULT_OK if data successfully retreived.
//          cbRESULT_INVALIDCHANNEL if the specified channel is not mapped or does not exist.
//          cbRESULT_INVALIDFUNCTION if the group number is not valid.
//          cbRESULT_NOLIBRARY if the library was not properly initialized.
#endif

//////////////////////////////////////////////////////////////
// AINP Spike Stream Functions


#define  cbAINPSPK_EXTRACT      0x00000001  // Time-stamp and packet to first superthreshold peak
#define  cbAINPSPK_REJART       0x00000002  // Reject around clipped signals on multiple channels
#define  cbAINPSPK_REJCLIP      0x00000004  // Reject clipped signals on the channel
#define  cbAINPSPK_ALIGNPK      0x00000008  //
#define  cbAINPSPK_REJAMPL      0x00000010  // Reject based on amplitude
#define  cbAINPSPK_THRLEVEL     0x00000100  // Analog level threshold detection
#define  cbAINPSPK_THRENERGY    0x00000200  // Energy threshold detection
#define  cbAINPSPK_THRAUTO      0x00000400  // Auto threshold detection
#define  cbAINPSPK_SPREADSORT   0x00001000  // Enable Auto spread Sorting
#define  cbAINPSPK_CORRSORT     0x00002000  // Enable Auto Histogram Correlation Sorting
#define  cbAINPSPK_PEAKMAJSORT  0x00004000  // Enable Auto Histogram Peak Major Sorting
#define  cbAINPSPK_PEAKFISHSORT 0x00008000  // Enable Auto Histogram Peak Fisher Sorting
#define  cbAINPSPK_HOOPSORT     0x00010000  // Enable Manual Hoop Sorting
#define  cbAINPSPK_PCAMANSORT   0x00020000  // Enable Manual PCA Sorting
#define  cbAINPSPK_PCAKMEANSORT 0x00040000  // Enable K-means PCA Sorting
#define  cbAINPSPK_PCAEMSORT    0x00080000  // Enable EM-clustering PCA Sorting
#define  cbAINPSPK_PCADBSORT    0x00100000  // Enable DBSCAN PCA Sorting
#define  cbAINPSPK_AUTOSORT     (cbAINPSPK_SPREADSORT | cbAINPSPK_CORRSORT | cbAINPSPK_PEAKMAJSORT | cbAINPSPK_PEAKFISHSORT) // old auto sorting methods
#define  cbAINPSPK_NOSORT       0x00000000  // No sorting
#define  cbAINPSPK_PCAAUTOSORT  (cbAINPSPK_PCAKMEANSORT | cbAINPSPK_PCAEMSORT | cbAINPSPK_PCADBSORT)  // All PCA sorting auto algorithms
#define  cbAINPSPK_PCASORT      (cbAINPSPK_PCAMANSORT | cbAINPSPK_PCAKMEANSORT | cbAINPSPK_PCAEMSORT | cbAINPSPK_PCADBSORT)  // All PCA sorting algorithms
#define  cbAINPSPK_ALLSORT      (cbAINPSPK_AUTOSORT | cbAINPSPK_HOOPSORT | cbAINPSPK_PCASORT)                                // All sorting algorithms

#ifdef __cplusplus

cbRESULT cbGetAinpSpikeCaps(UINT32 chan, UINT32 *flags, UINT32 nInstance = 0);
cbRESULT cbGetAinpSpikeOptions(UINT32 chan, UINT32 *flags, UINT32 *filter, UINT32 nInstance = 0);
cbRESULT cbSetAinpSpikeOptions(UINT32 chan, UINT32 flags,  UINT32 filter, UINT32 nInstance = 0);
// Get/Set spike capabilities and options.  The EXTRACT flag must be set for a channel to perform
// spike extraction and processing.  The HOOPS and TEMPLATE flags are exclusive, only one can be
// used at a time.
// the THRSHOVR flag will turn auto thresholding off for that channel
// filter = 1 to cbNFILTS, 0 is reserved for the null filter case
//
// Returns: cbRESULT_OK if data successfully retreived.
//          cbRESULT_INVALIDCHANNEL if the specified channel is not mapped or does not exist.
//          cbRESULT_INVALIDFUNCTION if invalid flag combinations are passed.
//          cbRESULT_NOLIBRARY if the library was not properly initialized.
//


cbRESULT cbGetAinpSpikeThreshold(UINT32 chan, INT32 *level, UINT32 nInstance = 0);
cbRESULT cbSetAinpSpikeThreshold(UINT32 chan, INT32 level, UINT32 nInstance = 0);
// Get/Set the spike detection threshold and threshold detection mode.
//
// Returns: cbRESULT_OK if data successfully retreived.
//          cbRESULT_INVALIDCHANNEL if the specified channel is not mapped or does not exist.
//          cbRESULT_INVALIDFUNCTION if invalid flag combinations are passed.
//          cbRESULT_NOLIBRARY if the library was not properly initialized.

#endif

typedef struct {
    UINT16 valid; // 0=undefined, 1 for valid
    INT16  time;  // time offset into spike window
    INT16  min;   // minimum value for the hoop window
    INT16  max;   // maximum value for the hoop window
} cbHOOP;

#ifdef __cplusplus

cbRESULT cbGetAinpSpikeHoops(UINT32 chan, cbHOOP *hoops, UINT32 nInstance = 0);
cbRESULT cbSetAinpSpikeHoops(UINT32 chan, cbHOOP *hoops, UINT32 nInstance = 0);
// Get/Set the spike hoop set.  The hoops parameter points to an array of hoops declared as
// cbHOOP hoops[cbMAXUNITS][cbMAXHOOPS].
//
// Empty hoop definitions have zeros for the cbHOOP structure members.  Hoop definitions can be
// cleared by passing a NULL cbHOOP pointer to the Set function or by calling the Set function
// with an all-zero cbHOOP structure.
//
// Returns: cbRESULT_OK if data successfully retreived.
//          cbRESULT_INVALIDCHANNEL if the specified channel is not mapped or does not exist.
//          cbRESULT_INVALIDFUNCTION if an invalid unit or hoop number is passed.
//          cbRESULT_NOLIBRARY if the library was not properly initialized.
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Analog Output Inquiry and Configuration Functions
//
///////////////////////////////////////////////////////////////////////////////////////////////////


#define  cbAOUT_AUDIO        0x00000001  // Channel is physically optimized for audio output
#define  cbAOUT_SCALE        0x00000002  // Output a static value
#define  cbAOUT_TRACK        0x00000004  // Output a static value
#define  cbAOUT_STATIC       0x00000008  // Output a static value
#define  cbAOUT_MONITORRAW   0x00000010  // Monitor an analog signal line - RAW data
#define  cbAOUT_MONITORLNC   0x00000020  // Monitor an analog signal line - Line Noise Cancelation
#define  cbAOUT_MONITORSMP   0x00000040  // Monitor an analog signal line - Continuous
#define  cbAOUT_MONITORSPK   0x00000080  // Monitor an analog signal line - spike
#define  cbAOUT_STIMULATE    0x00000100  // Stimulation waveform functions are available.
#define  cbAOUT_WAVEFORM     0x00000200  // Custom Waveform

#ifdef __cplusplus
cbRESULT cbGetAoutCaps(UINT32 chan, UINT32 *aoutcaps, cbSCALING *physcalout, cbFILTDESC *phyfiltout, UINT32 nInstance = 0);
// Get/Set the spike template capabilities and options.  The nunits and nhoops values detail the
// number of units that the channel supports.
//
// Empty template definitions have zeros for the cbSPIKETEMPLATE structure members.  Spike
// Template definitions can be cleared by passing a NULL cbSPIKETEMPLATE pointer to the Set
// function or by calling the Set function with an all-zero cbSPIKETEMPLATE structure.
//
// Returns: cbRESULT_OK if data successfully retreived.
//          cbRESULT_INVALIDCHANNEL if the specified channel is not mapped or does not exist.
//          cbRESULT_INVALIDFUNCTION if an invalid unit number is passed.
//          cbRESULT_NOLIBRARY if the library was not properly initialized.


cbRESULT cbGetAoutScaling(UINT32 chan, cbSCALING *scaling, UINT32 nInstance = 0);
cbRESULT cbSetAoutScaling(UINT32 chan, cbSCALING *scaling, UINT32 nInstance = 0);
// Get/Set the user-specified scaling for the channel.  The digmin and digmax values of the user
// specified scaling must be within the digmin and digmax values for the physical channel mapping.
//
// Returns: cbRESULT_OK if data successfully retreived.
//          cbRESULT_INVALIDCHANNEL if the specified channel is not mapped or does not exist.
//          cbRESULT_NOLIBRARY if the library was not properly initialized.


cbRESULT cbGetAoutOptions(UINT32 chan, UINT32 *options, UINT32 *monchan, UINT32 *value, UINT32 nInstance = 0);
cbRESULT cbSetAoutOptions(UINT32 chan, UINT32 options, UINT32 monchan, UINT32 value, UINT32 nInstance = 0);
// Get/Set the Monitored channel for a Analog Output Port.  Setting zero for the monitored channel
// stops the monitoring and frees any instrument monitor resources.  The factor ranges
//
// Returns: cbRESULT_OK if data successfully retreived or packet successfully queued to be sent.
//          cbRESULT_NOINTERNALCHAN if there is no internal channel for mapping the in->out chan
//          cbRESULT_INVALIDCHANNEL if the specified channel is not mapped or does not exist.
//          cbRESULT_NOLIBRARY if the library was not properly initialized.


// Request that the sorting model be updated
cbRESULT cbGetSortingModel(UINT32 nInstance = 0);
cbRESULT cbGetFeatureSpaceDomain(UINT32 nInstance = 0);


// Getting and setting the noise boundary
cbRESULT cbSSGetNoiseBoundary(UINT32 chanIdx, float afCentroid[3], float afMajor[3], float afMinor_1[3], float afMinor_2[3], UINT32 nInstance = 0);
cbRESULT cbSSSetNoiseBoundary(UINT32 chanIdx, float afCentroid[3], float afMajor[3], float afMinor_1[3], float afMinor_2[3], UINT32 nInstance = 0);

cbRESULT cbSSGetNoiseBoundaryByTheta(UINT32 chanIdx, float afCentroid[3], float afAxisLen[3], float afTheta[3], UINT32 nInstance = 0);
cbRESULT cbSSSetNoiseBoundaryByTheta(UINT32 chanIdx, const float afCentroid[3], const float afAxisLen[3], const float afTheta[3], UINT32 nInstance = 0);

// Getting and settings statistics
cbRESULT cbSSGetStatistics(UINT32 * pnUpdateSpikes, UINT32 * pnAutoalg, UINT32 * nMode,
                           float * pfMinClusterPairSpreadFactor,
                           float * pfMaxSubclusterSpreadFactor,
                           float * pfMinClusterHistCorrMajMeasure,
                           float * pfMaxClusterPairHistCorrMajMeasure,
                           float * pfClusterHistValleyPercentage,
                           float * pfClusterHistClosePeakPercentage,
                           float * pfClusterHistMinPeakPercentage,
                           UINT32 * pnWaveBasisSize,
                           UINT32 * pnWaveSampleSize,
                           UINT32 nInstance = 0);

cbRESULT cbSSSetStatistics(UINT32 nUpdateSpikes, UINT32 nAutoalg, UINT32 nMode,
                           float fMinClusterPairSpreadFactor,
                           float fMaxSubclusterSpreadFactor,
                           float fMinClusterHistCorrMajMeasure,
                           float fMaxClusterPairHistCorrMajMeasure,
                           float fClusterHistValleyPercentage,
                           float fClusterHistClosePeakPercentage,
                           float fClusterHistMinPeakPercentage,
                           UINT32 nWaveBasisSize,
                           UINT32 nWaveSampleSize,
                           UINT32 nInstance = 0);


// Spike sorting artifact rejecting
cbRESULT cbSSGetArtifactReject(UINT32 * pnMaxChans, UINT32 * pnRefractorySamples, UINT32 nInstance = 0);
cbRESULT cbSSSetArtifactReject(UINT32 nMaxChans, UINT32 nRefractorySamples, UINT32 nInstance = 0);

// Spike detection parameters
cbRESULT cbSSGetDetect(float * pfThreshold, float * pfScaling, UINT32 nInstance = 0);
cbRESULT cbSSSetDetect(float fThreshold, float fScaling, UINT32 nInstance = 0);

#endif

// To control and keep track of how long an element of spike sorting has been adapting.
//
enum ADAPT_TYPE { ADAPT_NEVER, ADAPT_ALWAYS, ADAPT_TIMED };
typedef struct {
    UINT32 nMode;           // 0-do not adapt at all, 1-always adapt, 2-adapt if timer not timed out
    float fTimeOutMinutes;  // how many minutes until time out
    float fElapsedMinutes;  // the amount of time that has elapsed

#ifdef __cplusplus
    void set(ADAPT_TYPE nMode, float fTimeOutMinutes)
    {
        this->nMode = static_cast<UINT32>(nMode);
        this->fTimeOutMinutes = fTimeOutMinutes;
    }
#endif

} cbAdaptControl;

#ifdef __cplusplus

// Getting and setting spike sorting status parameters
cbRESULT cbSSGetStatus(cbAdaptControl * pcntlUnitStats, cbAdaptControl * pcntlNumUnits, UINT32 nInstance = 0);
cbRESULT cbSSSetStatus(cbAdaptControl cntlUnitStats, cbAdaptControl cntlNumUnits, UINT32 nInstance = 0);

#endif

///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Data Packet Structures (chid<0x8000)
//
///////////////////////////////////////////////////////////////////////////////////////////////////

// Sample Group data packet
typedef struct {
    UINT32  time;       // system clock timestamp
    UINT16  chid;       // 0x0000
    UINT8   type;       // sample group ID (1-127)
    UINT8   dlen;       // packet length equal
    INT16   data[252];  // variable length address list
} cbPKT_GROUP;


// DINP digital value data
typedef struct {
    UINT32 time;        // system clock timestamp
    UINT16 chid;        // channel identifier
    UINT8  unit;        // reserved
    UINT8  dlen;        // length of waveform in 32-bit chunks
    UINT32 data[254];   // data buffer (up to 1016 bytes)
} cbPKT_DINP;


// AINP spike waveform data
// cbMAX_PNTS must be an even number
#define cbMAX_PNTS  128 // make large enough to track longest possible - spike width in samples

#define cbPKTDLEN_SPK   ((sizeof(cbPKT_SPK)/4)-2)
#define cbPKTDLEN_SPKSHORT (cbPKTDLEN_SPK - ((sizeof(INT16)*cbMAX_PNTS)/4))
typedef struct {
    UINT32 time;                // system clock timestamp
    UINT16 chid;                // channel identifier
    UINT8  unit;                // unit identification (0=unclassified, 31=artifact, 30=background)
    UINT8  dlen;                // length of what follows ... always  cbPKTDLEN_SPK
    float  fPattern[3];         // values of the pattern space (Normal uses only 2, PCA uses third)
    INT16  nPeak;
    INT16  nValley;
    // wave must be the last item in the structure because it can be variable length to a max of cbMAX_PNTS
    INT16  wave[cbMAX_PNTS];    // Room for all possible points collected
} cbPKT_SPK;

// System Heartbeat Packet (sent every 10ms)
#define cbPKTTYPE_SYSHEARTBEAT    0x00
#define cbPKTDLEN_SYSHEARTBEAT    ((sizeof(cbPKT_SYSHEARTBEAT)/4)-2)
typedef struct {
    UINT32 time;        // system clock timestamp
    UINT16 chid;        // 0x8000
    UINT8  type;        // 0
    UINT8  dlen;        // cbPKTDLEN_SYSHEARTBEAT
} cbPKT_SYSHEARTBEAT;

// Audio commands "val"
#define cbAUDIO_CMD_NONE             0   // PC->NPLAY query audio status
// nPlay file version (first byte NSx version, second byte NEV version)
#define cbNPLAY_FILE_NS21            1   // NSX 2.1 file
#define cbNPLAY_FILE_NS22            2   // NSX 2.2 file
#define cbNPLAY_FILE_NEV21           (1 << 8)   // Nev 2.1 file
#define cbNPLAY_FILE_NEV22           (2 << 8)   // Nev 2.2 file
#define cbNPLAY_FILE_NEV23           (3 << 8)   // Nev 2.3 file
// nPlay commands and status changes (cbPKT_NPLAY.mode)
#define cbNPLAY_FNAME_LEN            992  // length of the file name (with terminating null)
#define cbNPLAY_MODE_NONE            0    // no command (parameters)
#define cbNPLAY_MODE_PAUSE           1    // PC->NPLAY pause if "val" is non-zero, un-pause otherwise
#define cbNPLAY_MODE_SEEK            2    // PC->NPLAY seek to time "val"
#define cbNPLAY_MODE_CONFIG          3    // PC<->NPLAY request full config
#define cbNPLAY_MODE_OPEN            4    // PC->NPLAY open new file in "val" for playback
#define cbNPLAY_MODE_PATH            5    // PC->NPLAY use the directory path in fname
#define cbNPLAY_MODE_CONFIGMAIN      6    // PC<->NPLAY request main config packet
#define cbNPLAY_MODE_STEP            7    // PC<->NPLAY run "val" procTime steps and pause, then send cbNPLAY_FLAG_STEPPED
#define cbNPLAY_MODE_SINGLE          8    // PC->NPLAY single mode if "val" is non-zero, wrap otherwise
#define cbNPLAY_MODE_RESET           9    // PC->NPLAY reset nPlay
#define cbNPLAY_MODE_NEVRESORT       10   // PC->NPLAY resort NEV if "val" is non-zero, do not if otherwise
#define cbNPLAY_MODE_AUDIO_CMD       11   // PC->NPLAY perform audio command in "val" (cbAUDIO_CMD_*), with option "opt"
#define cbNPLAY_FLAG_NONE            0x00  // no flag
#define cbNPLAY_FLAG_CONF            0x01  // NPLAY->PC config packet ("val" is "fname" file index)
#define cbNPLAY_FLAG_MAIN           (0x02 | cbNPLAY_FLAG_CONF)  // NPLAY->PC main config packet ("val" is file version)
#define cbNPLAY_FLAG_DONE            0x02  // NPLAY->PC step command done

// nPlay configuration packet(sent on restart together with config packet)
#define cbPKTTYPE_NPLAYREP   0x5C /* NPLAY->PC response */
#define cbPKTTYPE_NPLAYSET   0xDC /* PC->NPLAY request */
#define cbPKTDLEN_NPLAY    ((sizeof(cbPKT_NPLAY)/4)-2)
typedef struct {
    UINT32 time;        // system clock timestamp
    UINT16 chid;        // 0x8000
    UINT8 type;      // cbPKTTYPE_NPLAY*
    UINT8 dlen;      // cbPKTDLEN_NPLAY
    
    union {
        UINT32 ftime;   // the total time of the file.
        UINT32 opt;     // optional value
    };
    UINT32 stime;       // start time
    UINT32 etime;       // stime < end time < ftime
    UINT32 val; 		// Used for current time to traverse, file index, file version, ...
    UINT16 mode;	    // cbNPLAY_MODE_* command to nPlay
    UINT16 flags; 		// cbNPLAY_FLAG_* status of nPlay
    float speed;		// positive means fast forward, negative means rewind, 0 means go as fast as you can.
    char  fname[cbNPLAY_FNAME_LEN];   // This is a String with the file name.
} cbPKT_NPLAY;

#ifdef __cplusplus
cbRESULT cbGetNplay(char *fname, float *speed, UINT32 *flags, UINT32 *ftime, UINT32 *stime, UINT32 *etime, UINT32 * filever, UINT32 nInstance = 0);
cbRESULT cbSetNplay(const char *fname, float speed, UINT32 mode, UINT32 val, UINT32 stime, UINT32 etime, UINT32 nInstance = 0);
// Get/Set the nPlay parameters.
//
// Returns: cbRESULT_OK if data successfully retrieved.
//          cbRESULT_NOLIBRARY if the library was not properly initialized.

#endif

#define cbTRIGGER_MODE_UNDEFINED        0
#define cbTRIGGER_MODE_BUTTONPRESS      1 // Patient button press event
#define cbTRIGGER_MODE_EVENTRESET       2 // event reset

// Custom trigger event packet
#define cbPKTTYPE_TRIGGERREP   0x5E /* NPLAY->PC response */
#define cbPKTTYPE_TRIGGERSET   0xDE /* PC->NPLAY request */
#define cbPKTDLEN_TRIGGER    ((sizeof(cbPKT_TRIGGER)/4)-2)
typedef struct {
    UINT32 time;        // system clock timestamp
    UINT16 chid;        // 0x8000
    UINT8  type;        // cbPKTTYPE_TRIGGER*
    UINT8  dlen;        // cbPKTDLEN_TRIGGER
    UINT32 mode;        // cbTRIGGER_MODE_*
} cbPKT_TRIGGER;

// Video tracking event packet
#define cbMAX_TRACKCOORDS (128) // Maximum nimber of coordinates (must be an even number)
#define cbPKTTYPE_VIDEOTRACKREP   0x5F /* NPLAY->PC response */
#define cbPKTTYPE_VIDEOTRACKSET   0xDF /* PC->NPLAY request */
#define cbPKTDLEN_VIDEOTRACK    ((sizeof(cbPKT_VIDEOTRACK)/4)-2)
#define cbPKTDLEN_VIDEOTRACKSHORT (cbPKTDLEN_VIDEOTRACK - ((sizeof(UINT16)*cbMAX_TRACKCOORDS)/4))
typedef struct {
    UINT32 time;        // system clock timestamp
    UINT16 chid;        // 0x8000
    UINT8  type;        // cbPKTTYPE_VIDEOTRACK*
    UINT8  dlen;        // cbPKTDLEN_VIDEOTRACK
    UINT16 parentID;    // parent ID
    UINT16 nodeID;      // node ID (cross-referenced in the TrackObj header)
    UINT16 nodeCount;   // Children count
    UINT16 pointCount;  // number of points at this node
    // this must be the last item in the structure because it can be variable length to a max of cbMAX_TRACKCOORDS
    union {
        UINT16 coords[cbMAX_TRACKCOORDS];
        UINT32 sizes[cbMAX_TRACKCOORDS / 2];
    };
} cbPKT_VIDEOTRACK;

// Reconfiguration log event
#define cbMAX_LOG          128  // Maximum log description
#define cbPKTTYPE_LOGREP   0x63 /* NPLAY->PC response */
#define cbPKTTYPE_LOGSET   0xE3 /* PC->NPLAY request */
#define cbPKTDLEN_LOG    ((sizeof(cbPKT_LOG)/4)-2)
#define cbPKTDLEN_LOGSHORT    (cbPKTDLEN_LOG - ((sizeof(char)*cbMAX_LOG)/4)) // All but description
typedef struct {
    UINT32 time;        // system clock timestamp
    UINT16 chid;        // 0x8000
    UINT8  type;        // cbPKTTYPE_LOG*
    UINT8  dlen;        // cbPKTDLEN_LOG
    UINT16 mode;        // log mode (0-normal 1-critical)
    char   name[cbLEN_STR_LABEL];    // Computer name
    char   desc[cbMAX_LOG];     // description of the change (will fill the rest of the packet)
} cbPKT_LOG;

// Protocol Monitoring packet (sent periodically about every second)
#define cbPKTTYPE_SYSPROTOCOLMONITOR    0x01
#define cbPKTDLEN_SYSPROTOCOLMONITOR    ((sizeof(cbPKT_SYSPROTOCOLMONITOR)/4)-2)
typedef struct {
    UINT32 time;        // system clock timestamp
    UINT16 chid;        // 0x8000
    UINT8  type;        // 1
    UINT8  dlen;        // cbPKTDLEN_SYSPROTOCOLMONITOR
    UINT32 sentpkts;    // Packets sent since last cbPKT_SYSPROTOCOLMONITOR (or 0 if timestamp=0);
                        //  the cbPKT_SYSPROTOCOLMONITOR packets are counted as well so this must
                        //  be equal to at least 1
} cbPKT_SYSPROTOCOLMONITOR;


#define cbPKTTYPE_REQCONFIGALL  0x88            // request for ALL configuration information
#define cbPKTTYPE_REPCONFIGALL  0x08            // response that NSP got your request


// System Condition Report Packet
#define cbPKTTYPE_SYSREP        0x10
#define cbPKTTYPE_SYSREPSPKLEN  0x11
#define cbPKTTYPE_SYSREPRUNLEV  0x12
#define cbPKTTYPE_SYSSET        0x90
#define cbPKTTYPE_SYSSETSPKLEN  0x91
#define cbPKTTYPE_SYSSETRUNLEV  0x92
#define cbPKTDLEN_SYSINFO       ((sizeof(cbPKT_SYSINFO)/4)-2)
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
} cbPKT_SYSINFO;

#ifdef __cplusplus
cbRESULT cbGetSpikeLength(UINT32 *length, UINT32 *pretrig, UINT32 * pSysfreq, UINT32 nInstance = 0);
cbRESULT cbSetSpikeLength(UINT32 length, UINT32 pretrig, UINT32 nInstance = 0);
// Get/Set the system-wide spike length.  Lengths should be specified in multiples of 2 and
// within the range of 16 to 128 samples long.
//
// Returns: cbRESULT_OK if data successfully retrieved.
//          cbRESULT_INVALIDCHANNEL if the specified channel is not mapped or does not exist.
//          cbRESULT_INVALIDFUNCTION if invalid flag combinations are passed.
//          cbRESULT_NOLIBRARY if the library was not properly initialized.
#endif

#define cbRUNLEVEL_STARTUP      10
#define cbRUNLEVEL_HARDRESET    20
#define cbRUNLEVEL_STANDBY      30
#define cbRUNLEVEL_RESET        40
#define cbRUNLEVEL_RUNNING      50
#define cbRUNLEVEL_STRESSED     60
#define cbRUNLEVEL_ERROR        70
#define cbRUNLEVEL_SHUTDOWN     80

#define cbRUNFLAGS_NONE          0
#define cbRUNFLAGS_LOCK          1 // Lock recording after reset

#ifdef __cplusplus
cbRESULT cbGetSystemRunLevel(UINT32 *runlevel, UINT32 *locked, UINT32 *resetque, UINT32 nInstance = 0);
cbRESULT cbSetSystemRunLevel(UINT32 runlevel, UINT32 locked, UINT32 resetque, UINT32 nInstance = 0);
// Get Set the System Condition
// Returns: cbRESULT_OK if data successfully retrieved.
//          cbRESULT_NOLIBRARY if the library was not properly initialized
#endif

// Video/external synchronization packet.
// This packet comes from NeuroMotive through network or RS232
// then is transmitted to the Central after spike or LFP packets to stamp them.
#define cbPKTTYPE_VIDEOSYNCHREP   0x29  /* NSP->PC response */
#define cbPKTTYPE_VIDEOSYNCHSET   0xA9  /* PC->NSP request */
#define cbPKTDLEN_VIDEOSYNCH  ((sizeof(cbPKT_VIDEOSYNCH)/4)-2)
typedef struct {
    UINT32  time;		// System clock timestamp
    UINT16  chid;		// 0x8000
    UINT8   type;       // cbPKTTYPE_VIDEOSYNCH*
    UINT8   dlen;       // cbPKTDLEN_VIDEOSYNCH
    UINT16  split;      // file split number of the video file
    UINT32  frame;      // frame number in last video (0-based)
    UINT32  etime;      // capture elapsed time (in milliseconds)
    UINT16  id;         // video source id
} cbPKT_VIDEOSYNCH;


// Comment flags
#define cbCOMMENT_FLAG_RGBA            0x00  // data is color in rgba format
#define cbCOMMENT_FLAG_TIMESTAMP       0x01  //data is the timestamp of when the comment was started
// Comment annotation packet.
#define cbMAX_COMMENT  128     // cbMAX_COMMENT must be a multiple of four
#define cbPKTTYPE_COMMENTREP   0x31  /* NSP->PC response */
#define cbPKTTYPE_COMMENTSET   0xB1  /* PC->NSP request */
#define cbPKTDLEN_COMMENT  ((sizeof(cbPKT_COMMENT)/4)-2)
#define cbPKTDLEN_COMMENTSHORT (cbPKTDLEN_COMMENT - ((sizeof(UINT8)*cbMAX_COMMENT)/4))
typedef struct {
    UINT32  time;		// System clock timestamp
    UINT16  chid;		// 0x8000
    UINT8   type;       // cbPKTTYPE_COMMENT*
    UINT8   dlen;       // cbPKTDLEN_COMMENT
    struct {
        UINT8   charset;        // Character set (0 - ANSI, 1 - UTF16)
        UINT8   flags;          // Can be any of cbCOMMENT_FLAG_*
        UINT8   reserved[2];    // Reserved (must be 0)
    } info;
    UINT32  data;       // depends on flags (see flags above)
    char   comment[cbMAX_COMMENT]; // Comment
} cbPKT_COMMENT;


#ifdef __cplusplus
cbRESULT cbSetComment(UINT8 charset, UINT8 flags, UINT32 data, const char * comment, UINT32 nInstance = 0);
// Set one comment event.
//
// Returns: cbRESULT_OK if data successfully retrieved.
//          cbRESULT_NOLIBRARY if the library was not properly initialized.
#endif

// NeuroMotive status
#define cbNM_STATUS_IDLE                0  // NeuroMotive is idle
#define cbNM_STATUS_EXIT                1  // NeuroMotive is exiting
#define cbNM_STATUS_REC                 2  // NeuroMotive is recording
#define cbNM_STATUS_PLAY                3  // NeuroMotive is playing video file
#define cbNM_STATUS_CAP                 4  // NeuroMotive is capturing from camera
#define cbNM_STATUS_STOP                5  // NeuroMotive is stopping
#define cbNM_STATUS_PAUSED              6  // NeuroMotive is paused
#define cbNM_STATUS_COUNT               7  // This is the count of status options

// NeuroMotive commands and status changes (cbPKT_NM.mode)
#define cbNM_MODE_NONE                  0
#define cbNM_MODE_CONFIG                1  // Ask NeuroMotive for configuration
#define cbNM_MODE_SETVIDEOSOURCE        2  // Configure video source
#define cbNM_MODE_SETTRACKABLE          3  // Configure trackable
#define cbNM_MODE_STATUS                4  // NeuroMotive status reporting (cbNM_STATUS_*)
#define cbNM_FLAG_NONE                  0

#define cbPKTTYPE_NMREP   0x32  /* NSP->PC response */
#define cbPKTTYPE_NMSET   0xB2  /* PC->NSP request */
#define cbPKTDLEN_NM  ((sizeof(cbPKT_NM)/4)-2)
typedef struct {
    UINT32  time;		// System clock timestamp
    UINT16  chid;		// 0x8000
    UINT8   type;       // cbPKTTYPE_NM*
    UINT8   dlen;       // cbPKTDLEN_NM
    UINT32  mode;	    // cbNM_MODE_* command to NeuroMotive or Central
    UINT32  flags; 		// cbNM_FLAG_* status of NeuroMotive
    UINT32  value;      // value assigned to this mode
    char    name[cbLEN_STR_LABEL];   // name associated with this mode
} cbPKT_NM;


// Report Processor Information (duplicates the cbPROCINFO structure)
#define cbPKTTYPE_PROCREP   0x21
#define cbPKTDLEN_PROCINFO  ((sizeof(cbPKT_PROCINFO)/4)-2)
typedef struct {
    UINT32 time;        // system clock timestamp
    UINT16 chid;        // 0x8000
    UINT8  type;        // cbPKTTYPE_PROC*
    UINT8  dlen;        // cbPKT_PROCINFODLEN
    UINT32 proc;        // index of the bank
    UINT32 idcode;      // manufacturer part and rom ID code of the Signal Processor
    char   ident[cbLEN_STR_IDENT];   // ID string with the equipment name of the Signal Processor
    UINT32 chanbase;    // lowest channel number of channel id range claimed by this processor
    UINT32 chancount;   // number of channel identifiers claimed by this processor
    UINT32 bankcount;   // number of signal banks supported by the processor
    UINT32 groupcount;  // number of sample groups supported by the processor
    UINT32 filtcount;   // number of digital filters supported by the processor
    UINT32 sortcount;   // number of channels supported for spike sorting (reserved for future)
    UINT32 unitcount;   // number of supported units for spike sorting    (reserved for future)
    UINT32 hoopcount;   // number of supported units for spike sorting    (reserved for future)
    UINT32 sortmethod;  // sort method  (0=manual, 1=automatic spike sorting)
    UINT32 version;     // current version of libraries
} cbPKT_PROCINFO;

#ifdef __cplusplus
cbRESULT cbGetProcInfo(UINT32 proc, cbPROCINFO *procinfo, UINT32 nInstance = 0);
// Retreives information for a the Signal Processor module located at procid
// The function requires an allocated but uninitialized cbPROCINFO structure.
//
// Returns: cbRESULT_OK if data successfully retreived.
//          cbRESULT_INVALIDADDRESS if no hardware at the specified Proc and Bank address
//          cbRESULT_NOLIBRARY if the library was not properly initialized.

cbRESULT cbGetChanCount(UINT32 *count, UINT32 nInstance = 0);
// Retreives the total number of channels in the system
//
// Returns: cbRESULT_OK if data successfully retreived.
//          cbRESULT_NOLIBRARY if the library was not properly initialized.
#endif

// Report Bank Information (duplicates the cbBANKINFO structure)
#define cbPKTTYPE_BANKREP   0x22
#define cbPKTDLEN_BANKINFO  ((sizeof(cbPKT_BANKINFO)/4)-2)
typedef struct {
    UINT32 time;        // system clock timestamp
    UINT16 chid;        // 0x8000
    UINT8  type;        // cbPKTTYPE_BANK*
    UINT8  dlen;        // cbPKT_BANKINFODLEN
    UINT32 proc;        // the address of the processor on which the bank resides
    UINT32 bank;        // the address of the bank reported by the packet
    UINT32 idcode;      // manufacturer part and rom ID code of the module addressed to this bank
    char   ident[cbLEN_STR_IDENT];   // ID string with the equipment name of the Signal Bank hardware module
    char   label[cbLEN_STR_LABEL];   // Label on the instrument for the signal bank, eg "Analog In"
    UINT32 chanbase;    // lowest channel number of channel id range claimed by this bank
    UINT32 chancount;   // number of channel identifiers claimed by this bank
} cbPKT_BANKINFO;

#ifdef __cplusplus
cbRESULT cbGetBankInfo(UINT32 proc, UINT32 bank, cbBANKINFO *bankinfo, UINT32 nInstance = 0);
// Retreives information for the Signal bank located at bankaddr on Proc procaddr.
// The function requires an allocated but uninitialized cbBANKINFO structure.
//
// Returns: cbRESULT_OK if data successfully retreived.
//          cbRESULT_INVALIDADDRESS if no hardware at the specified Proc and Bank address
//          cbRESULT_NOLIBRARY if the library was not properly initialized.
#endif

// Filter (FILT) Information Packets
#define cbPKTTYPE_FILTREP   0x23
#define cbPKTTYPE_FILTSET   0xA3
#define cbPKTDLEN_FILTINFO  ((sizeof(cbPKT_FILTINFO)/4)-2)
typedef struct {
    UINT32  time;       // system clock timestamp
    UINT16  chid;       // 0x8000
    UINT8   type;       // cbPKTTYPE_GROUP*
    UINT8   dlen;       // packet length equal to length of list + 6 quadlets
    UINT32  proc;       //
    UINT32  filt;       //
    char    label[cbLEN_STR_FILT_LABEL];  //
    UINT32  hpfreq;     // high-pass corner frequency in milliHertz
    UINT32  hporder;    // high-pass filter order
    UINT32  hptype;     // high-pass filter type
    UINT32  lpfreq;     // low-pass frequency in milliHertz
    UINT32  lporder;    // low-pass filter order
    UINT32  lptype;     // low-pass filter type
    // These are for sending the NSP filter info, otherwise the NSP has this stuff in NSPDefaults.c
    double gain;        // filter gain
    double sos1a1;      // filter coefficient
    double sos1a2;      // filter coefficient
    double sos1b1;      // filter coefficient
    double sos1b2;      // filter coefficient
    double sos2a1;      // filter coefficient
    double sos2a2;      // filter coefficient
    double sos2b1;      // filter coefficient
    double sos2b2;      // filter coefficient
} cbPKT_FILTINFO;

#ifdef __cplusplus
cbRESULT cbGetFilterDesc(UINT32 proc, UINT32 filt, cbFILTDESC *filtdesc, UINT32 nInstance = 0);
// Retreives the user filter definitions from a specific processor
// filter = 1 to cbNFILTS, 0 is reserved for the null filter case
// Returns: cbRESULT_OK if data successfully retreived.
//          cbRESULT_INVALIDADDRESS if no hardware at the specified Proc and Bank address
//          cbRESULT_NOLIBRARY if the library was not properly initialized.
#endif


// Factory Default settings request packet
#define cbPKTTYPE_CHANRESETREP  0x24        /* NSP->PC response...ignore all values */
#define cbPKTTYPE_CHANRESET     0xA4        /* PC->NSP request */
#define cbPKTDLEN_CHANRESET ((sizeof(cbPKT_CHANRESET) / 4) - 2)
typedef struct {
    UINT32  time;           // system clock timestamp
    UINT16  chid;           // 0x8000
    UINT8   type;           // cbPKTTYPE_AINP*
    UINT8   dlen;           // cbPKT_DLENCHANINFO
    UINT32  chan;           // actual channel id of the channel being configured

    // For all of the values that follow
    // 0 = NOT change value; nonzero = reset to factory defaults

    UINT8   label;          // Channel label
    UINT8   userflags;      // User flags for the channel state
    UINT8   position;       // reserved for future position information
    UINT8   scalin;         // user-defined scaling information
    UINT8   scalout;        // user-defined scaling information
    UINT8   doutopts;       // digital output options (composed of cbDOUT_* flags)
    UINT8   dinpopts;       // digital input options (composed of cbDINP_* flags)
    UINT8   aoutopts;       // analog output options
    UINT8   eopchar;        // the end of packet character
    UINT8   monsource;      // address of channel to monitor
    UINT8   outvalue;       // output value
    UINT8   ainpopts;       // analog input options (composed of cbAINP_* flags)
    UINT8   lncrate;        // line noise cancellation filter adaptation rate
    UINT8   smpfilter;      // continuous-time pathway filter id
    UINT8   smpgroup;       // continuous-time pathway sample group
    UINT8   smpdispmin;     // continuous-time pathway display factor
    UINT8   smpdispmax;     // continuous-time pathway display factor
    UINT8   spkfilter;      // spike pathway filter id
    UINT8   spkdispmax;     // spike pathway display factor
    UINT8   lncdispmax;     // Line Noise pathway display factor
    UINT8   spkopts;        // spike processing options
    UINT8   spkthrlevel;    // spike threshold level
    UINT8   spkthrlimit;    //
    UINT8   spkgroup;       // NTrodeGroup this electrode belongs to - 0 is single unit, non-0 indicates a multi-trode grouping
    UINT8   spkhoops;       // spike hoop sorting set
} cbPKT_CHANRESET;

///////////////////////////////////////////////////////////////////////////////////////////////////
// Adaptive filtering
#define cbPKTTYPE_ADAPTFILTREP  0x25        /* NSP->PC response...*/
#define cbPKTTYPE_ADAPTFILTSET  0xA5        /* PC->NSP request */
#define cbPKTDLEN_ADAPTFILTINFO ((sizeof(cbPKT_ADAPTFILTINFO) / 4) - 2)
#define ADAPT_FILT_DISABLED		0
#define ADAPT_FILT_ALL			1
#define ADAPT_FILT_SPIKES		2
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

} cbPKT_ADAPTFILTINFO;  // The packet....look below vvvvvvvv

#ifdef __cplusplus
// Tell me about the current adaptive filter settings
cbRESULT cbGetAdaptFilter(UINT32  proc,             // which NSP processor?
                          UINT32  * pnMode,         // 0=disabled, 1=filter continuous & spikes, 2=filter spikes
                          float   * pdLearningRate, // speed at which adaptation happens. Very small. e.g. 5e-12
                          UINT32  * pnRefChan1,     // The first reference channel (1 based).
                          UINT32  * pnRefChan2,     // The second reference channel (1 based).
                          UINT32 nInstance = 0);


// Update the adaptive filter settings
cbRESULT cbSetAdaptFilter(UINT32  proc,             // which NSP processor?
                          UINT32  * pnMode,         // 0=disabled, 1=filter continuous & spikes, 2=filter spikes
                          float   * pdLearningRate, // speed at which adaptation happens. Very small. e.g. 5e-12
                          UINT32  * pnRefChan1,     // The first reference channel (1 based).
                          UINT32  * pnRefChan2,     // The second reference channel (1 based).
                          UINT32 nInstance = 0);

// Useful for creating cbPKT_ADAPTFILTINFO packets
struct PktAdaptFiltInfo : public cbPKT_ADAPTFILTINFO
{
    PktAdaptFiltInfo(UINT32 nMode, float dLearningRate, UINT32 nRefChan1, UINT32 nRefChan2)
    {
        this->chid = 0x8000;
        this->type = cbPKTTYPE_ADAPTFILTSET;
        this->dlen = cbPKTDLEN_ADAPTFILTINFO;

        this->nMode = nMode;
        this->dLearningRate = dLearningRate;
        this->nRefChan1 = nRefChan1;
        this->nRefChan2 = nRefChan2;
    };
};
#endif


///////////////////////////////////////////////////////////////////////////////////////////////////
// Reference Electrode filtering
#define cbPKTTYPE_REFELECFILTREP  0x26        /* NSP->PC response...*/
#define cbPKTTYPE_REFELECFILTSET  0xA6        /* PC->NSP request */
#define cbPKTDLEN_REFELECFILTINFO ((sizeof(cbPKT_REFELECFILTINFO) / 4) - 2)
#define REFELEC_FILT_DISABLED	0
#define REFELEC_FILT_ALL		1
#define REFELEC_FILT_SPIKES		2
typedef struct {
    UINT32  time;           // system clock timestamp
    UINT16  chid;           // 0x8000
    UINT8   type;           // cbPKTTYPE_REFELECFILTSET or cbPKTTYPE_REFELECFILTREP
    UINT8   dlen;           // cbPKTDLEN_REFELECFILTINFO
    UINT32  chan;           // Ignored

    UINT32  nMode;          // 0=disabled, 1=filter continuous & spikes, 2=filter spikes
    UINT32  nRefChan;       // The reference channel (1 based).

#ifdef __cplusplus
    void set(UINT32 nMode, UINT32 nRefChan)
    {
        chid = cbPKTCHAN_CONFIGURATION;
        type = cbPKTTYPE_REFELECFILTSET;
        dlen = ((sizeof(*this) / 4) - 2);
        this->nMode = nMode;
        this->nRefChan = nRefChan;
    }
#endif

} cbPKT_REFELECFILTINFO;  // The packet

#ifdef __cplusplus
// Tell me about the current reference electrode filter settings
cbRESULT cbGetRefElecFilter(UINT32  proc,           // which NSP processor?
                            UINT32  * pnMode,       // 0=disabled, 1=filter continuous & spikes, 2=filter spikes
                            UINT32  * pnRefChan,    // The reference channel (1 based).
                            UINT32 nInstance = 0);


// Update the reference electrode filter settings
cbRESULT cbSetRefElecFilter(UINT32  proc,           // which NSP processor?
                            UINT32  * pnMode,       // 0=disabled, 1=filter continuous & spikes, 2=filter spikes
                            UINT32  * pnRefChan,    // The reference channel (1 based).
                            UINT32 nInstance = 0);
#endif

// NTrode Information Packets
#define cbNTRODEINFO_FS_PEAK           0         // Ntrode peak feature space
#define cbNTRODEINFO_FS_VALLEY         1         // Ntrode valley feature space
#define cbPKTTYPE_REPNTRODEINFO      0x27        /* NSP->PC response...*/
#define cbPKTTYPE_SETNTRODEINFO      0xA7        /* PC->NSP request */
#define cbPKTDLEN_NTRODEINFO         ((sizeof(cbPKT_NTRODEINFO) / 4) - 2)
typedef struct {
    UINT32  time;           // system clock timestamp
    UINT16  chid;           // 0x8000
    UINT8   type;           // cbPKTTYPE_REPNTRODEINFO or cbPKTTYPE_SETNTRODEINFO
    UINT8   dlen;           // cbPKTDLEN_NTRODEGRPINFO
    UINT32  ntrode;         // ntrode with which we are working (1-based)
    char    label[cbLEN_STR_LABEL];   // Label of the Ntrode (null terminated if < 16 characters)
    cbMANUALUNITMAPPING ellipses[cbMAXSITEPLOTS][cbMAXUNITS];  // unit mapping
    UINT16  nSite;          // number channels in this NTrode ( 0 <= nSite <= cbMAXSITES)
    UINT16  fs;             // NTrode feature space cbNTRODEINFO_FS_*
    UINT16  nChan[cbMAXSITES];  // group of channels in this NTrode

#ifdef __cplusplus
    void set(UINT32 ntrode, char label[cbLEN_STR_LABEL])
    {
        chid = cbPKTCHAN_CONFIGURATION;
        type = cbPKTTYPE_SETNTRODEINFO;
        dlen = ((sizeof(*this) / 4) - 2);
        this->ntrode = ntrode;
        memcpy(this->label, label, sizeof(this->label));
        memset(this->ellipses, 0, sizeof(ellipses));
    }
#endif

} cbPKT_NTRODEINFO;  // ntrode information packet

#ifdef __cplusplus
cbRESULT cbGetNTrodeInfo( const UINT32 ntrode, char *label, cbMANUALUNITMAPPING ellipses[][cbMAXUNITS], UINT16 * nSite, UINT16 * chans, UINT16 * fs, UINT32 nInstance = 0);
cbRESULT cbSetNTrodeInfo( const UINT32 ntrode, const char *label, cbMANUALUNITMAPPING ellipses[][cbMAXUNITS], UINT16 fs, UINT32 nInstance = 0);
#endif

// Sample Group (GROUP) Information Packets
#define cbPKTTYPE_GROUPREP      0x30    // (lower 7bits=ppppggg)
#define cbPKTTYPE_GROUPSET      0xB0
#define cbPKTDLEN_GROUPINFO  ((sizeof(cbPKT_GROUPINFO)/4) - cbPKT_HEADER_32SIZE)
#define cbPKTDLEN_GROUPINFOSHORT  8       // basic length without list
typedef struct {
    UINT32  time;       // system clock timestamp
    UINT16  chid;       // 0x8000
    UINT8   type;       // cbPKTTYPE_GROUP*
    UINT8   dlen;       // packet length equal to length of list + 6 quadlets
    UINT32  proc;       //
    UINT32  group;      //
    char    label[cbLEN_STR_LABEL];  // sampling group label
    UINT32  period;     // sampling period for the group
    UINT32  length;     //
    UINT32  list[cbNUM_ANALOG_CHANS];   // variable length list. The max size is
                                        // the total number of analog channels
} cbPKT_GROUPINFO;

#ifdef __cplusplus
cbRESULT cbGetSampleGroupInfo(UINT32 proc, UINT32 group, char *label, UINT32 *period, UINT32 *length, UINT32 nInstance = 0);
cbRESULT cbGetSampleGroupList(UINT32 proc, UINT32 group, UINT32 *length, UINT32 *list, UINT32 nInstance = 0);
cbRESULT cbSetSampleGroupOptions(UINT32 proc, UINT32 group, UINT32 period, char *label, UINT32 nInstance = 0);
// Retreives the Sample Group information in a processor and their definitions
// Labels are 16-characters maximum.
//
// Returns: cbRESULT_OK if data successfully retreived.
//          cbRESULT_INVALIDADDRESS if no hardware at the specified Proc and Bank address
//          cbRESULT_NOLIBRARY if the library was not properly initialized.


cbRESULT cbGetRawGroupInfo( UINT32 proc, UINT32 group, char *label, UINT32 *period, UINT32* length, UINT32 nInstance = 0);
cbRESULT cbGetRawGroupList( UINT32 proc, UINT32 group, UINT32 *length, UINT32 *list, UINT32 nInstance = 0);
// Retreives the Raw Group information in a processor and their definitions
// Labels are 16-characters maximum.
//
// Returns: cbRESULT_OK if data successfully retreived.
//          cbRESULT_INVALIDADDRESS if no hardware at the specified Proc and Bank address
//          cbRESULT_NOLIBRARY if the library was not properly initialized.
#endif

// Analog Input (AINP) Information Packets
#define cbPKTTYPE_CHANREP                   0x40
#define cbPKTTYPE_CHANREPLABEL              0x41
#define cbPKTTYPE_CHANREPSCALE              0x42
#define cbPKTTYPE_CHANREPDOUT               0x43
#define cbPKTTYPE_CHANREPDINP               0x44
#define cbPKTTYPE_CHANREPAOUT               0x45
#define cbPKTTYPE_CHANREPDISP               0x46
#define cbPKTTYPE_CHANREPAINP               0x47
#define cbPKTTYPE_CHANREPSMP                0x48
#define cbPKTTYPE_CHANREPSPK                0x49
#define cbPKTTYPE_CHANREPSPKTHR             0x4A
#define cbPKTTYPE_CHANREPSPKHPS             0x4B
#define cbPKTTYPE_CHANREPUNITOVERRIDES      0x4C
#define cbPKTTYPE_CHANREPNTRODEGROUP        0x4D
#define cbPKTTYPE_CHANREPREJECTAMPLITUDE    0x4E
#define cbPKTTYPE_CHANREPAUTOTHRESHOLD      0x4F
#define cbPKTTYPE_CHANSET                   0xC0
#define cbPKTTYPE_CHANSETLABEL              0xC1
#define cbPKTTYPE_CHANSETSCALE              0xC2
#define cbPKTTYPE_CHANSETDOUT               0xC3
#define cbPKTTYPE_CHANSETDINP               0xC4
#define cbPKTTYPE_CHANSETAOUT               0xC5
#define cbPKTTYPE_CHANSETDISP               0xC6
#define cbPKTTYPE_CHANSETAINP               0xC7
#define cbPKTTYPE_CHANSETSMP                0xC8
#define cbPKTTYPE_CHANSETSPK                0xC9
#define cbPKTTYPE_CHANSETSPKTHR             0xCA
#define cbPKTTYPE_CHANSETSPKHPS             0xCB
#define cbPKTTYPE_CHANSETUNITOVERRIDES      0xCC
#define cbPKTTYPE_CHANSETNTRODEGROUP        0xCD
#define cbPKTTYPE_CHANSETREJECTAMPLITUDE    0xCE
#define cbPKTTYPE_CHANSETAUTOTHRESHOLD  0xCF
#define cbPKTDLEN_CHANINFO      ((sizeof(cbPKT_CHANINFO)/4)-2)
#define cbPKTDLEN_CHANINFOSHORT (cbPKTDLEN_CHANINFO - ((sizeof(cbHOOP)*cbMAXUNITS*cbMAXHOOPS)/4))
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
    cbSCALING           physcalin;      // physical channel scaling information
    cbFILTDESC          phyfiltin;      // physical channel filter definition
    cbSCALING           physcalout;     // physical channel scaling information
    cbFILTDESC          phyfiltout;     // physical channel filter definition
    char                label[cbLEN_STR_LABEL];   // Label of the channel (null terminated if <16 characters)
    UINT32              userflags;      // User flags for the channel state
    INT32               position[4];    // reserved for future position information
    cbSCALING           scalin;         // user-defined scaling information for AINP
    cbSCALING           scalout;        // user-defined scaling information for AOUT
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
    cbMANUALUNITMAPPING unitmapping[cbMAXUNITS];            // manual unit mapping
    cbHOOP              spkhoops[cbMAXUNITS][cbMAXHOOPS];   // spike hoop sorting set
} cbPKT_CHANINFO;

#ifdef __cplusplus
cbRESULT cbGetChanInfo(UINT32 chan, cbPKT_CHANINFO *pChanInfo, UINT32 nInstance = 0);
// Get the full channel config.
//
// Returns: cbRESULT_OK if data successfully retreived or packet successfully queued to be sent.
//          cbRESULT_INVALIDCHANNEL if the specified channel is not mapped or does not exist.
//          cbRESULT_NOLIBRARY if the library was not properly initialized.
cbRESULT cbGetChanAmplitudeReject(UINT32 chan, cbAMPLITUDEREJECT *AmplitudeReject, UINT32 nInstance = 0);
cbRESULT cbSetChanAmplitudeReject(UINT32 chan, const cbAMPLITUDEREJECT AmplitudeReject, UINT32 nInstance = 0);
// Get and Set the user-assigned amplitude reject values.
//
// Returns: cbRESULT_OK if data successfully retreived or packet successfully queued to be sent.
//          cbRESULT_INVALIDCHANNEL if the specified channel is not mapped or does not exist.
//          cbRESULT_NOLIBRARY if the library was not properly initialized.

cbRESULT cbGetChanAutoThreshold(UINT32 chan, UINT32 *bEnabled, UINT32 nInstance = 0);
cbRESULT cbSetChanAutoThreshold(UINT32 chan, const UINT32 bEnabled, UINT32 nInstance = 0);
// Get and Set the user-assigned auto threshold option
//
// Returns: cbRESULT_OK if data successfully retreived or packet successfully queued to be sent.
//          cbRESULT_INVALIDCHANNEL if the specified channel is not mapped or does not exist.
//          cbRESULT_NOLIBRARY if the library was not properly initialized.

cbRESULT cbGetChanUnitMapping( UINT32 chan, cbMANUALUNITMAPPING *unitmapping, UINT32 nInstance = 0);
cbRESULT cbSetChanUnitMapping( UINT32 chan, cbMANUALUNITMAPPING *unitmapping, UINT32 nInstance = 0);
// Get and Set the user-assigned unit override for the channel.
//
// Returns: cbRESULT_OK if data successfully retreived or packet successfully queued to be sent.
//          cbRESULT_INVALIDCHANNEL if the specified channel is not mapped or does not exist.
//          cbRESULT_NOLIBRARY if the library was not properly initialized.

cbRESULT cbGetChanLoc(UINT32 chan, UINT32 *proc, UINT32 *bank, char *banklabel, UINT32 *term, UINT32 nInstance = 0);
// Gives the physical processor number, bank label, and terminal number of the specified channel
// by reading the configuration data in the Central App Cache.  Bank Labels are the name of the
// bank that is written on the instrument and they are null-terminated, up to 16 char long.
//
// Returns: cbRESULT_OK if all is ok
//          cbRESULT_INVALIDCHANNEL if the specified channel is not mapped or does not exist.
//          cbRESULT_NOLIBRARY if the library was not properly initialized.


// flags for user flags...no effect on the cerebus
//#define cbUSER_DISABLED     0x00000001  // Channel should be electrically disabled
//#define cbUSER_EXPERIMENT   0x00000100  // Channel used for experiment environment information
//#define cbUSER_NEURAL       0x00000200  // Channel connected to neural electrode or signal

cbRESULT cbGetChanLabel(UINT32 chan, char *label, UINT32 *userflags, INT32 *position, UINT32 nInstance = 0);
cbRESULT cbSetChanLabel(UINT32 chan, const char *label, UINT32 userflags,  INT32 *position, UINT32 nInstance = 0);
// Get and Set the user-assigned label for the channel.  Channel Names may be up to 16 chars long
// and should be null terminated if shorter.
//
// Returns: cbRESULT_OK if data successfully retreived or packet successfully queued to be sent.
//          cbRESULT_INVALIDCHANNEL if the specified channel is not mapped or does not exist.
//          cbRESULT_NOLIBRARY if the library was not properly initialized.

cbRESULT cbGetChanNTrodeGroup(UINT32 chan, UINT32 *NTrodeGroup, UINT32 nInstance = 0);
cbRESULT cbSetChanNTrodeGroup(UINT32 chan, const UINT32 NTrodeGroup, UINT32 nInstance = 0);
// Get and Set the user-assigned label for the N-Trode.  N-Trode Names may be up to 16 chars long
// and should be null terminated if shorter.
//
// Returns: cbRESULT_OK if data successfully retreived or packet successfully queued to be sent.
//          cbRESULT_INVALIDCHANNEL if the specified channel is not mapped or does not exist.
//          cbRESULT_NOLIBRARY if the library was not properly initialized.
#endif

/////////////////////////////////////////////////////////////////////////////////
// These are part of the "reflected" mechanism. They go out as type 0xE? and come
// Back in as type 0x6?

#define cbPKTTYPE_MASKED_REFLECTED              0xE0
#define cbPKTTYPE_COMPARE_MASK_REFLECTED        0xF0
#define cbPKTTYPE_REFLECTED_CONVERSION_MASK     0x7F

// Packet which says that these channels are now selected
typedef struct _cbPKT_UNIT_SELECTION
{
    UINT32 time;            // system clock timestamp
    UINT16 chid;            // 0x8000
    UINT8  type;            //
    UINT8  dlen;            // How many dwords follow......end of standard header
    INT32 lastchan;         // Which channel was clicked last.
    UINT16   abyUnitSelections[cbMAXCHANS];     // one for each channel, channels are 0 based here

#ifdef __cplusplus
    // Inputs:
    //  ulastchan = 0 based channel that was most recently selected
    _cbPKT_UNIT_SELECTION(INT16 iLastchan = 1) :
        chid(0x8000),
        type(TYPE_OUTGOING),
        lastchan(iLastchan)
    {
        // Can't use 'this' in a member-initialization
        dlen = sizeof(*this) / 4 - cbPKT_HEADER_32SIZE;
        memset(abyUnitSelections, 0, sizeof(abyUnitSelections));
    }

    // These are the packet type constants
    enum { TYPE_OUTGOING = 0xE2 };          // Goes out like this
    enum { TYPE_INCOMING = 0x62 };          // Comes back in like this after "reflection"

    // These are the masks for use with   abyUnitSelections
    enum
    {
        UNIT_UNCLASS_MASK = 0x01,       // mask to use to say unclassified units are selected
        UNIT_1_MASK       = 0x02,       // mask to use to say unit 1 is selected
        UNIT_2_MASK       = 0x04,       // mask to use to say unit 2 is selected
        UNIT_3_MASK       = 0x08,       // mask to use to say unit 3 is selected
        UNIT_4_MASK       = 0x10,       // mask to use to say unit 4 is selected
        UNIT_5_MASK       = 0x20,       // mask to use to say unit 5 is selected
        CONTINUOUS_MASK   = 0x40,       // mask to use to say the continuous signal is selected

        UNIT_ALL_MASK = UNIT_UNCLASS_MASK |
                        UNIT_1_MASK |   // This means the channel is completely selected
                        UNIT_2_MASK |
                        UNIT_3_MASK |
                        UNIT_4_MASK |
                        UNIT_5_MASK |
                        CONTINUOUS_MASK,
    };

    static int UnitToUnitmask(int nUnit) { return 1 << nUnit; }
#endif
} cbPKT_UNIT_SELECTION;

#ifdef __cplusplus
cbRESULT cbGetChannelSelection(cbPKT_UNIT_SELECTION * pPktUnitSel, UINT32 nInstance = 0);
// Retreives the channel unit selection status
// Returns: cbRESULT_OK if data successfully retreived.
//          cbRESULT_NOLIBRARY if the library was not properly initialized.
#endif


// file config options
#define cbFILECFG_OPT_NONE         0x00000000  // Launch File dialog, set filew info, start or stop recording
#define cbFILECFG_OPT_KEEPALIVE    0x00000001  // Keep-alive message
#define cbFILECFG_OPT_REC          0x00000002  // Recording is in progress
#define cbFILECFG_OPT_STOP         0x00000003  // Recording stopped
#define cbFILECFG_OPT_NMREC        0x00000004  // NeuroMotive recording status
#define cbFILECFG_OPT_CLOSE        0x00000005  // Close file application
#define cbFILECFG_OPT_SYNCH        0x00000006  // Recording datetime
#define cbFILECFG_OPT_OPEN         0x00000007  // Launch File dialog, do not set or do anything

// file save configuration packet
#define cbPKTTYPE_REPFILECFG 0x61
#define cbPKTTYPE_SETFILECFG 0xE1
#define cbPKTDLEN_FILECFG ((sizeof(cbPKT_FILECFG)/4)-2)
#define cbPKTDLEN_FILECFGSHORT (cbPKTDLEN_FILECFG - ((sizeof(char)*3*256)/4)) // used for keep-alive messages
typedef struct {
    UINT32 time;            // system clock timestamp
    UINT16 chid;            // 0x8000
    UINT8  type;
    UINT8  dlen;
    UINT32 options;         // cbFILECFG_OPT_*
    UINT32 duration;
    UINT32 recording;
    UINT32 extctrl;
    char   username[256];
    union {
        char   filename[256];
        char   datetime[256];
    };
    char   comment[256];
} cbPKT_FILECFG;

#ifdef __cplusplus
cbRESULT cbGetFileInfo(cbPKT_FILECFG * filecfg, UINT32 nInstance = 0);
// Retreives the file recordign status
// Returns: cbRESULT_OK if data successfully retreived.
//          cbRESULT_NOLIBRARY if the library was not properly initialized.
#endif

// Patient information for recording file
#define cbMAX_PATIENTSTRING  128     //

#define cbPKTTYPE_REPPATIENTINFO 0x64
#define cbPKTTYPE_SETPATIENTINFO 0xE4
#define cbPKTDLEN_PATIENTINFO ((sizeof(cbPKT_PATIENTINFO)/4)-2)
typedef struct {
    UINT32 time;            // system clock timestamp
    UINT16 chid;            // 0x8000
    UINT8  type;			// cbPKTTYPE_SETPATIENTINFO
    UINT8  dlen;
    char   ID[cbMAX_PATIENTSTRING];
    char   firstname[cbMAX_PATIENTSTRING];
    char   lastname[cbMAX_PATIENTSTRING];
    UINT32 DOBMonth;
    UINT32 DOBDay;
    UINT32 DOBYear;
} cbPKT_PATIENTINFO;

// Calculated Impedance data packet
#define cbPKTTYPE_REPIMPEDANCE   0x65
#define cbPKTTYPE_SETIMPEDANCE   0xE5
#define cbPKTDLEN_IMPEDANCE ((sizeof(cbPKT_IMPEDANCE)/4)-2)
typedef struct {
    UINT32  time;       // system clock timestamp
    UINT16  chid;       // 0x8000
    UINT8   type;       // E5
    UINT8   dlen;       // packet length equal
    float   data[140];  // variable length address list
} cbPKT_IMPEDANCE;

// Poll packet command
#define cbPOLL_MODE_NONE            0   // no command (parameters)
#define cbPOLL_MODE_APPSTATUS       1   // Poll or response to poll about the status of an application
// Poll packet status flags
#define cbPOLL_FLAG_NONE            0   // no flag (parameters)
#define cbPOLL_FLAG_RESPONSE        1   // Response to the query
// Extra information
#define cbPOLL_EXT_NONE             0   // No extra information
#define cbPOLL_EXT_EXISTS           1   // App exists
#define cbPOLL_EXT_RUNNING          2   // App is running
// poll applications to determine different states
// Central will return if an application exists (and is running)
#define cbPKTTYPE_REPPOLL   0x67
#define cbPKTTYPE_SETPOLL   0xE7
#define cbPKTDLEN_POLL ((sizeof(cbPKT_POLL)/4)-2)
typedef struct {
    UINT32  time;       // system clock timestamp
    UINT16  chid;       // 0x8000
    UINT8   type;       // E7
    UINT8   dlen;       // packet length equal
    UINT32  mode;       // any of cbPOLL_MODE_* commands
    UINT32  flags;      // any of the cbPOLL_FLAG_* status
    UINT32  extra;      // Extra parameters depending on flags and mode
    char    appname[32]; // name of program to apply command specified by mode
    char    username[256]; // return your computername
    UINT32  reserved[32]; // reserved for the future
} cbPKT_POLL;


// initiate impedance check
#define cbPKTTYPE_REPINITIMPEDANCE   0x66
#define cbPKTTYPE_SETINITIMPEDANCE   0xE6
#define cbPKTDLEN_INITIMPEDANCE ((sizeof(cbPKT_INITIMPEDANCE)/4)-2)
typedef struct {
    UINT32  time;       // system clock timestamp
    UINT16  chid;       // 0x8000
    UINT8   type;       // E6
    UINT8   dlen;       // packet length equal
    UINT32	initiate;	// on set call -> 1 to start autoimpedance
                        // on response -> 1 initiated
} cbPKT_INITIMPEDANCE;

// file save configuration packet
#define cbPKTTYPE_REPMAPFILE 0x68
#define cbPKTTYPE_SETMAPFILE 0xE8
#define cbPKTDLEN_MAPFILE ((sizeof(cbPKT_MAPFILE)/4)-2)
typedef struct {
    UINT32 time;            // system clock timestamp
    UINT16 chid;            // 0x8000
    UINT8  type;
    UINT8  dlen;
    char   filename[512];

#ifdef __cplusplus
    void set(const char *szMapFilename)
    {
        chid = cbPKTCHAN_CONFIGURATION;
        type = cbPKTTYPE_SETMAPFILE;
        dlen = ((sizeof(*this) / 4) - 2);
        memset(filename, 0, sizeof(filename));
        strncpy(filename, szMapFilename, sizeof(filename));
    }
#endif

} cbPKT_MAPFILE;


//-----------------------------------------------------
///// Packets to tell me about the spike sorting model

// This packet says, "Give me all of the model". In response, you will get a series of cbPKTTYPE_MODELREP
//
#define cbPKTTYPE_SS_MODELALLREP   0x50        /* NSP->PC response */
#define cbPKTTYPE_SS_MODELALLSET   0xD0        /* PC->NSP request  */
#define cbPKTDLEN_SS_MODELALLSET ((sizeof(cbPKT_SS_MODELALLSET) / 4) - 2)
typedef struct {
    UINT32  time;           // system clock timestamp
    UINT16  chid;           // 0x8000
    UINT8   type;           // cbPKTTYPE_MODELALLSET or cbPKTTYPE_MODELALLREP depending on the direction
    UINT8   dlen;           // 0
} cbPKT_SS_MODELALLSET;


//
#define cbPKTTYPE_SS_MODELREP      0x51        /* NSP->PC response */
#define cbPKTTYPE_SS_MODELSET      0xD1        /* PC->NSP request  */
#define cbPKTDLEN_SS_MODELSET ((sizeof(cbPKT_SS_MODELSET) / 4) - 2)
#define MAX_REPEL_POINTS 3
typedef struct {
    UINT32  time;           // system clock timestamp
    UINT16  chid;           // 0x8000
    UINT8   type;           // cbPKTTYPE_SS_MODELREP or cbPKTTYPE_SS_MODELSET depending on the direction
    UINT8   dlen;           // cbPKTDLEN_SS_MODELSET

    UINT32  chan;           // actual channel id of the channel being configured (0 based)
    UINT32  unit_number;    // unit label (0 based, 0 is noise cluster)
    UINT32  valid;          // 1 = valid unit, 0 = not a unit, in other words just deleted when NSP -> PC
    UINT32  inverted;       // 0 = not inverted, 1 = inverted

    // Block statistics (change from block to block)
    INT32   num_samples;    // non-zero value means that the block stats are valid
    float   mu_x[2];
    float   Sigma_x[2][2];
    float   determinant_Sigma_x;
    ///// Only needed if we are using a Bayesian classification model
    float   Sigma_x_inv[2][2];
    float   log_determinant_Sigma_x;
    /////
    float   subcluster_spread_factor_numerator;
    float   subcluster_spread_factor_denominator;
    float   mu_e;
    float   sigma_e_squared;
} cbPKT_SS_MODELSET;


// This packet contains the options for the automatic spike sorting.
//
#define cbPKTTYPE_SS_DETECTREP  0x52        /* NSP->PC response */
#define cbPKTTYPE_SS_DETECTSET  0xD2        /* PC->NSP request  */
#define cbPKTDLEN_SS_DETECT ((sizeof(cbPKT_SS_DETECT) / 4) - 2)
typedef struct {
    UINT32  time;           // system clock timestamp
    UINT16  chid;           // 0x8000
    UINT8   type;           // cbPKTTYPE_SS_DETECTREP or cbPKTTYPE_SS_DETECTSET depending on the direction
    UINT8   dlen;           // cbPKTDLEN_SS_DETECT

    float   fThreshold;     // current detection threshold
    float   fMultiplier;    // multiplier

#ifdef __cplusplus
    void set(float fThreshold, float fMultiplier)
    {
        chid = cbPKTCHAN_CONFIGURATION;
        type = cbPKTTYPE_SS_DETECTSET;
        dlen = ((sizeof(*this) / 4) - 2);
        this->fThreshold = fThreshold;
        this->fMultiplier = fMultiplier;
    }
#endif

} cbPKT_SS_DETECT;

// Options for artifact rejecting
//
#define cbPKTTYPE_SS_ARTIF_REJECTREP  0x53        /* NSP->PC response */
#define cbPKTTYPE_SS_ARTIF_REJECTSET  0xD3        /* PC->NSP request  */
#define cbPKTDLEN_SS_ARTIF_REJECT ((sizeof(cbPKT_SS_ARTIF_REJECT) / 4) - 2)
typedef struct {
    UINT32  time;               // system clock timestamp
    UINT16  chid;               // 0x8000
    UINT8   type;               // cbPKTTYPE_SS_ARTIF_REJECTREP or cbPKTTYPE_SS_ARTIF_REJECTSET depending on the direction
    UINT8   dlen;               // cbPKTDLEN_SS_ARTIF_REJECT

    UINT32  nMaxSimulChans;     // how many channels can fire exactly at the same time???
    UINT32  nRefractoryCount;   // for how many samples (30 kHz) is a neuron refractory, so can't re-trigger

#ifdef __cplusplus
    void set(UINT32 nMaxSimulChans, UINT32 nRefractoryCount)
    {
        chid = cbPKTCHAN_CONFIGURATION;
        type = cbPKTTYPE_SS_ARTIF_REJECTSET;
        dlen = ((sizeof(*this) / 4) - 2);
        this->nMaxSimulChans = nMaxSimulChans;
        this->nRefractoryCount = nRefractoryCount;
    }
#endif

} cbPKT_SS_ARTIF_REJECT;

// Options for noise boundary
//
#define cbPKTTYPE_SS_NOISE_BOUNDARYREP  0x54        /* NSP->PC response */
#define cbPKTTYPE_SS_NOISE_BOUNDARYSET  0xD4        /* PC->NSP request  */
#define cbPKTDLEN_SS_NOISE_BOUNDARY ((sizeof(cbPKT_SS_NOISE_BOUNDARY) / 4) - 2)
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

} cbPKT_SS_NOISE_BOUNDARY;

// All sorting algorithms for which we can change settings
// (in cbPKT_SS_STATISTICS.nAutoalg)
#define cbAUTOALG_NONE                 0   // No sorting
#define cbAUTOALG_SPREAD               1   // Auto spread
#define cbAUTOALG_HIST_CORR_MAJ        2   // Auto Hist Correlation
#define cbAUTOALG_HIST_PEAK_COUNT_MAJ  3   // Auto Hist Peak Maj
#define cbAUTOALG_HIST_PEAK_COUNT_FISH 4   // Auto Hist Peak Fish
#define cbAUTOALG_PCA                  5   // Manual PCA
#define cbAUTOALG_HOOPS                6   // Manual Hoops
#define cbAUTOALG_PCA_KMEANS           7   // K-means PCA
#define cbAUTOALG_PCA_EM               8   // EM-clustering PCA
#define cbAUTOALG_PCA_DBSCAN           9   // DBSCAN PCA
// The commands to change sorting parameters and state
// (in cbPKT_SS_STATISTICS.nMode)
#define cbAUTOALG_MODE_SETTING         0   // Change the settings and leave sorting the same (PC->NSP request)
#define cbAUTOALG_MODE_APPLY           1   // Change settings and apply this sorting to all channels (PC->NSP request)
// This packet contains the settings for the spike sorting algorithms
//
#define cbPKTTYPE_SS_STATISTICSREP  0x55        /* NSP->PC response */
#define cbPKTTYPE_SS_STATISTICSSET  0xD5        /* PC->NSP request  */
#define cbPKTDLEN_SS_STATISTICS ((sizeof(cbPKT_SS_STATISTICS) / 4) - 2)
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

} cbPKT_SS_STATISTICS;

// Send this packet to the NSP to tell it to reset all spike sorting to default values
#define cbPKTTYPE_SS_RESETREP       0x56        /* NSP->PC response */
#define cbPKTTYPE_SS_RESETSET       0xD6        /* PC->NSP request  */
#define cbPKTDLEN_SS_RESET ((sizeof(cbPKT_SS_RESET) / 4) - 2)
typedef struct
{
    UINT32  time;           // system clock timestamp
    UINT16  chid;           // 0x8000
    UINT8   type;           // cbPKTTYPE_SS_RESETREP or cbPKTTYPE_SS_RESETSET depending on the direction
    UINT8   dlen;           // cbPKTDLEN_SS_RESET

    #ifdef __cplusplus
    void set()
    {
        chid = cbPKTCHAN_CONFIGURATION;
        type = cbPKTTYPE_SS_RESETSET;
        dlen = ((sizeof(*this) / 4) - 2);
    }
    #endif
} cbPKT_SS_RESET;


// This packet contains the status of the automatic spike sorting.
//
#define cbPKTTYPE_SS_STATUSREP  0x57        /* NSP->PC response */
#define cbPKTTYPE_SS_STATUSSET  0xD7        /* PC->NSP request  */
#define cbPKTDLEN_SS_STATUS ((sizeof(cbPKT_SS_STATUS) / 4) - 2)
typedef struct {
    UINT32  time;           // system clock timestamp
    UINT16  chid;           // 0x8000
    UINT8   type;           // cbPKTTYPE_SS_STATUSREP or cbPKTTYPE_SS_STATUSSET depending on the direction
    UINT8   dlen;           // cbPKTDLEN_SS_STATUS

    cbAdaptControl cntlUnitStats;
    cbAdaptControl cntlNumUnits;

#ifdef __cplusplus
    void set(cbAdaptControl cntlUnitStats, cbAdaptControl cntlNumUnits)
    {
        chid = cbPKTCHAN_CONFIGURATION;     //0x8000
        type = cbPKTTYPE_SS_STATUSSET;
        dlen = ((sizeof(*this) / 4) - 2);

        this->cntlUnitStats = cntlUnitStats;
        this->cntlNumUnits = cntlNumUnits;
    }
#endif

} cbPKT_SS_STATUS;

// Send this packet to the NSP to tell it to reset all spike sorting models
#define cbPKTTYPE_SS_RESET_MODEL_REP    0x58        /* NSP->PC response */
#define cbPKTTYPE_SS_RESET_MODEL_SET    0xD8        /* PC->NSP request  */
#define cbPKTDLEN_SS_RESET_MODEL ((sizeof(cbPKT_SS_RESET_MODEL) / 4) - 2)
typedef struct
{
    UINT32  time;           // system clock timestamp
    UINT16  chid;           // 0x8000
    UINT8   type;           // cbPKTTYPE_SS_RESET_MODEL_REP or cbPKTTYPE_SS_RESET_MODEL_SET depending on the direction
    UINT8   dlen;           // cbPKTDLEN_SS_RESET_MODEL

    #ifdef __cplusplus
    void set()
    {
        chid = cbPKTCHAN_CONFIGURATION;
        type = cbPKTTYPE_SS_RESET_MODEL_SET;
        dlen = ((sizeof(*this) / 4) - 2);
    }
    #endif
} cbPKT_SS_RESET_MODEL;

// Feature space commands and status changes (cbPKT_SS_RECALC.mode)
#define cbPCA_RECALC_START             0	// PC ->NSP start recalculation
#define cbPCA_RECALC_STOPPED           1    // NSP->PC  finished recalculation
#define cbPCA_COLLECTION_STARTED       2    // NSP->PC  waveform collection started
#define cbBASIS_CHANGE                 3    // Change the basis of feature space
#define cbUNDO_BASIS_CHANGE            4
#define cbREDO_BASIS_CHANGE            5
#define cbINVALIDATE_BASIS             6

// Send this packet to the NSP to tell it to re calculate all PCA Basis Vectors and Values
#define cbPKTTYPE_SS_RECALCREP       0x59        /* NSP->PC response */
#define cbPKTTYPE_SS_RECALCSET       0xD9        /* PC->NSP request  */
#define cbPKTDLEN_SS_RECALC ((sizeof(cbPKT_SS_RECALC) / 4) - 2)
typedef struct
{
    UINT32  time;           // system clock timestamp
    UINT16  chid;           // 0x8000
    UINT8   type;           // cbPKTTYPE_SS_RECALCREP or cbPKTTYPE_SS_RECALCSET depending on the direction
    UINT8   dlen;           // cbPKTDLEN_SS_RECALC

    UINT32  chan;           // 1 based channel we want to recalc (0 = All channels)
    UINT32  mode;           // cbPCA_RECALC_START -> Start PCa basis, cbPCA_RECALC_STOPPED-> PCA basis stopped, cbPCA_COLLECTION_STARTED -> PCA waveform collection started
    #ifdef __cplusplus
    void set(UINT32 chan, UINT32 mode)
    {
        chid = cbPKTCHAN_CONFIGURATION;
        type = cbPKTTYPE_SS_RECALCSET;
        dlen = ((sizeof(*this) / 4) - 2);
        this->chan = chan;
        this->mode = mode;
    }
    #endif
} cbPKT_SS_RECALC;

// This packet holds the calculated basis of the feature space from NSP to Central
// Or it has the previous basis retrieved and transmitted by central to NSP
#define cbPKTTYPE_FS_BASISREP       0x5B        /* NSP->PC response */
#define cbPKTTYPE_FS_BASISSET       0xDB        /* PC->NSP request  */
#define cbPKTDLEN_FS_BASIS ((sizeof(cbPKT_FS_BASIS) / 4) - 2)
#define cbPKTDLEN_FS_BASISSHORT (cbPKTDLEN_FS_BASIS - ((sizeof(float)* cbMAX_PNTS * 3)/4))
typedef struct
{
    UINT32  time;           // system clock timestamp
    UINT16  chid;           // 0x8000
    UINT8   type;           // cbPKTTYPE_FS_BASISREP or cbPKTTYPE_FS_BASISSET depending on the direction
    UINT8   dlen;           // cbPKTDLEN_FS_BASIS

    UINT32  chan;           // 1-based channel number
    UINT32  mode;           // cbBASIS_CHANGE, cbUNDO_BASIS_CHANGE, cbREDO_BASIS_CHANGE, cbINVALIDATE_BASIS ...
    UINT32  fs;             // Feature space: cbAUTOALG_PCA
    // basis must be the last item in the structure because it can be variable length to a max of cbMAX_PNTS
    float  basis[cbMAX_PNTS][3];    // Room for all possible points collected

    #ifdef __cplusplus
    void set(UINT32 chan, UINT32 mode, UINT32 fs, float * basis[], int spikeLen)
    {
        int i = 0, j = 0;
        chid = cbPKTCHAN_CONFIGURATION;
        type = cbPKTTYPE_FS_BASISSET;
        UINT8 dlenShort = UINT8(((sizeof(*this) / 4) - 2) - ((sizeof(float)* cbMAX_PNTS * 3)/4));
        dlen = UINT8(dlenShort + sizeof(float) * spikeLen / 4 * 3);
        this->chan = chan;
        this->mode = mode;
        this->fs   = fs;
        for (i = 0; i < spikeLen; ++i){
            for (j = 0; j < 3; ++j) {
                this->basis[i][j] = basis[i][j];
            }
        }
    }
    #endif
} cbPKT_FS_BASIS;

// This packet holds the Line Noise Cancellation parameters
#define cbPKTTYPE_LNCREP       0x28        /* NSP->PC response */
#define cbPKTTYPE_LNCSET       0xA8        /* PC->NSP request  */
#define cbPKTDLEN_LNC ((sizeof(cbPKT_LNC) / 4) - 2)
typedef struct
{
    UINT32  time;           // system clock timestamp
    UINT16  chid;           // 0x8000
    UINT8   type;           // cbPKTTYPE_LNCSET or cbPKTTYPE_LNCREP depending on the direction
    UINT8   dlen;           // cbPKTDLEN_LNC

    UINT32 lncFreq;        // Nominal line noise frequency to be canceled  (in Hz)
    UINT32 lncRefChan;     // Reference channel for lnc synch (1-based)
    UINT32 lncGlobalMode;  // reserved

    #ifdef __cplusplus
    void set(UINT32 lncFreq, UINT32 lncRefChan, UINT32 lncGlobalMode)
    {
        chid = cbPKTCHAN_CONFIGURATION;
        type = cbPKTTYPE_LNCSET;
        dlen = ((sizeof(*this) / 4) - 2);
        this->lncFreq = lncFreq;
        this->lncRefChan = lncRefChan;
        this->lncGlobalMode   = lncGlobalMode;
    }
    #endif
} cbPKT_LNC;

#ifdef __cplusplus
cbRESULT cbGetLncParameters(UINT32 *nLncFreq, UINT32 *nLncRefChan, UINT32 *nLncGMode, UINT32 nInstance = 0);
cbRESULT cbSetLncParameters(UINT32 nLncFreq, UINT32 nLncRefChan, UINT32 nLncGMode, UINT32 nInstance = 0);
// Get/Set the system-wide LNC parameters.
//
// Returns: cbRESULT_OK if data successfully retrieved.
//          cbRESULT_NOLIBRARY if the library was not properly initialized.
#endif

// Send this packet to force the digital output to this value
#define cbPKTTYPE_SET_DOUTREP       0x5D        /* NSP->PC response */
#define cbPKTTYPE_SET_DOUTSET       0xDD        /* PC->NSP request  */
#define cbPKTDLEN_SET_DOUT ((sizeof(cbPKT_SET_DOUT) / 4) - 2)
typedef struct {
    UINT32 time;        // system clock timestamp
    UINT16 chid;        // 0x8000
    UINT8  type;        // cbPKTTYPE_SET_DOUTREP or cbPKTTYPE_SET_DOUTSET depending on direction
    UINT8  dlen;        // length of waveform in 32-bit chunks
    UINT16 chan;        // which digital output channel (1 based, will equal chan from GetDoutCaps)
    UINT16 value;       // Which value to set? zero = 0; non-zero = 1 (output is 1 bit)

    #ifdef __cplusplus
    // Inputs:
    //  nChan - 1 based channel to set
    //  bSet - TRUE means to assert the port (1); FALSE, unassert (0)
    void set(UINT32 nChan, bool bSet)
    {
        chid = cbPKTCHAN_CONFIGURATION;
        type = cbPKTTYPE_SET_DOUTSET;
        dlen = sizeof(*this) / 4 - 2;

        chan = nChan;
        value = bSet ? 1 : 0;
    }
    #endif
} cbPKT_SET_DOUT;

// signal generator waveform type
#define    cbWaveType_NONE          0   // the waveform is disabled
#define    cbWaveType_PARAMETERS    1   // the waveform is defined by amplitude, frequency, duration and number
#define    cbWaveType_SINE          2   // the waveform is a sinusoid
#define    cbWaveType_FILE          3   // the waveform is defined in a file
#define	   cbWaveType_EQUATION      4   // the waveform is defined by an equation

// AOUT signal generator waveform data
// Send this packet of analog out data when the signal generator is generating the output for a channel
#define cbPKTTYPE_WAVEFORMREP       0x33        /* NSP->PC response */
#define cbPKTTYPE_WAVEFORMSET       0xB3        /* PC->NSP request  */
#define cbPKTDLEN_WAVEFORM   ((sizeof(cbPKT_AOUT_WAVEFORM)/4)-2)
#define cbPKTDLEN_WAVEFORMSHORT (cbPKT_AOUT_WAVEFORM - ((sizeof(INT16)*cbLEN_AOUT_WAVEFORM)/4))
typedef struct {
    UINT32 time;				// system clock timestamp
    UINT16 chid;				// cbPKTCHAN_CONFIGURATION
    UINT8  type;				// cbPKTTYPE_WAVEFORMREP or cbPKTTYPE_WAVEFORMSET depending on direction
    UINT8  dlen;				// packet size
    UINT16 chan;				// which analog output/audio output channel (1 based, will equal chan from GetDoutCaps)

    // Waveform parameter information
    INT32  NPulses;
    INT32  Offset;
    INT32  Phase1Duration;		// samples
    INT32  Phase2Duration;		// samples
    INT32  Phase1Amplitude;		// mV - This is used for both square wave, and sine wave
    INT32  Phase2Amplitude;		// mV
    INT32  InterPhaseDelay;		// samples
    INT32  InterPulseDelay;		// samples
    INT32  nFrequency;   		// sine wave Hz
    INT32  nWaveType;		    // Can be any of cbWaveType_*

    // Waveform buffer information
    UINT32 saved;						// TRUE/FALSE = the packet was saved / the packet should be resent

    #ifdef __cplusplus
    // Inputs:
    //  nChan - 1 based channel
    //  pData - pointer to waveform data to output on the channel
    void set(UINT32 nChan, INT32 nNPulses, INT32 nOffset, INT32 nPhase1Duration, INT32 nPhase2Duration, INT32 nPhase1Amplitude, INT32 nPhase2Amplitude, INT32 nInterPhaseDelay, INT32 nInterPulseDelay)
    {
        chid = cbPKTCHAN_CONFIGURATION;
        type = cbPKTTYPE_WAVEFORMSET;
        dlen = ((sizeof(*this) / 4) - 2);

        chan = nChan;
        saved = 0;

        NPulses			= nNPulses;
        Offset			= nOffset;
        Phase1Duration	= nPhase1Duration;
        Phase2Duration	= nPhase2Duration;
        Phase1Amplitude	= nPhase1Amplitude;
        Phase2Amplitude	= nPhase2Amplitude;
        InterPhaseDelay	= nInterPhaseDelay;
        InterPulseDelay	= nInterPulseDelay;
        nFrequency       = 0;
        nWaveType        = cbWaveType_PARAMETERS;
    }
    // Inputs:
    //  nChan - 1 based channel
    //  pData - pointer to waveform data to output on the channel
    void set(UINT32 nChan, INT32 nPhase1Amplitude, INT32 Frequency)
    {
        chid = cbPKTCHAN_CONFIGURATION;
        type = cbPKTTYPE_WAVEFORMSET;
        dlen = (UINT8)((sizeof(*this) / 4) - 2);

        chan = nChan;
        saved = 0;

        NPulses			= 0;
        Offset			= 0;
        Phase1Duration	= 1;
        Phase2Duration	= 1;
        Phase1Amplitude	= nPhase1Amplitude;
        Phase2Amplitude	= 1;
        InterPhaseDelay	= 1;
        InterPulseDelay	= 1;
        nFrequency       = Frequency;
        nWaveType        = cbWaveType_SINE;
    }
    #endif
} cbPKT_AOUT_WAVEFORM;

///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Preview Data Packet Definitions (chid = 0x8000 + channel)
//
///////////////////////////////////////////////////////////////////////////////////////////////////


// preview information requests
#define cbPKTTYPE_PREVSETLNC    0x81
#define cbPKTTYPE_PREVSETSTREAM 0x82
#define cbPKTTYPE_PREVSET       0x83

#define cbPKTTYPE_PREVREP       0x03        // Acknowledged response from the packet above


// line noise cancellation (LNC) waveform preview packet
#define cbPKTTYPE_PREVREPLNC    0x01
#define cbPKTDLEN_PREVREPLNC    ((sizeof(cbPKT_LNCPREV)/4)-2)
typedef struct {
    UINT32 time;        // system clock timestamp
    UINT16 chid;        // 0x8000 + channel identifier
    UINT8  type;        // cbPKT_LNCPREVLEN
    UINT8  dlen;        // length of the waveform/2
    UINT32 freq;        // Estimated line noise frequency * 1000 (zero means not valid)
    INT16  wave[300];   // lnc cancellation waveform (downsampled by 2)
} cbPKT_LNCPREV;

// These values are taken by nSampleRows if PCA
#define cbPCA_START_COLLECTION            0  // start collecting samples
#define cbPCA_START_BASIS                 1  // start basis calculation
#define cbPCA_MANUAL_LAST_SAMPLE          2  // the manual-only PCA, samples at zero, calculates PCA basis at 1 and stops at 2
// the first time a basis is calculated it can be used, even for the waveforms collected for the next basis
#define cbSTREAMPREV_NONE               0x00000000
#define cbSTREAMPREV_PCABASIS_NONEMPTY  0x00000001
// Streams preview packet
#define cbPKTTYPE_PREVREPSTREAM  0x02
#define cbPKTDLEN_PREVREPSTREAM  ((sizeof(cbPKT_STREAMPREV)/4)-2)
typedef struct {
    UINT32 time;        // system clock timestamp
    UINT16 chid;        // 0x8000 + channel identifier
    UINT8  type;        // cbPKTTYPE_PREVREPSTREAM
    UINT8  dlen;        // cbPKTDLEN_PREVREPSTREAM
    INT16  rawmin;      // minimum raw channel value over last preview period
    INT16  rawmax;      // maximum raw channel value over last preview period
    INT16  smpmin;      // minimum sample channel value over last preview period
    INT16  smpmax;      // maximum sample channel value over last preview period
    INT16  spkmin;      // minimum spike channel value over last preview period
    INT16  spkmax;      // maximum spike channel value over last preview period
    UINT32 spkmos;      // mean of squares
    UINT32 eventflag;   // flag to detail the units that happend in the last sample period
    INT16  envmin;      // minimum envelope channel value over the last preview period
    INT16  envmax;      // maximum envelope channel value over the last preview period
    INT32  spkthrlevel; // preview of spike threshold level
    UINT32 nWaveNum;    // this tracks the number of waveforms collected in the WCM for each channel
    UINT32 nSampleRows; // tracks number of sample vectors of waves
    UINT32 nFlags;      // cbSTREAMPREV_*
} cbPKT_STREAMPREV;


///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Shared Memory Definitions used by Central App and Cerebus library functions
//
///////////////////////////////////////////////////////////////////////////////////////////////////

// TODO: separate out these definitions so that there are no conditional compiles
#ifdef __cplusplus

typedef struct {
    COLORREF winrsvd[48];
    COLORREF dispback;
    COLORREF dispgridmaj;
    COLORREF dispgridmin;
    COLORREF disptext;
    COLORREF dispwave;
    COLORREF dispwavewarn;
    COLORREF dispwaveclip;
    COLORREF dispthresh;
    COLORREF dispmultunit;
    COLORREF dispunit[16];  // 0 = unclassified
    COLORREF dispnoise;
    COLORREF dispchansel[3];
    COLORREF disptemp[5];
    COLORREF disprsvd[14];
} cbCOLORTABLE;

cbRESULT cbGetColorTable(cbCOLORTABLE **colortable, UINT32 nInstance = 0);


typedef struct {
    float fRMSAutoThresholdDistance;    // multiplier to use for autothresholding when using
                                        // RMS to guess noise
    UINT32 reserved[31];
} cbOPTIONTABLE;


// Get/Set the multiplier to use for autothresholdine when using RMS to guess noise
// This will adjust fAutoThresholdDistance above, but use the API instead
float cbGetRMSAutoThresholdDistance(UINT32 nInstance = 0);
void cbSetRMSAutoThresholdDistance(float fRMSAutoThresholdDistance, UINT32 nInstance = 0);

//////////////////////////////////////////////////////////////////////////////////////////////////


#define cbPKT_SPKCACHEPKTCNT  400
#define cbPKT_SPKCACHELINECNT cbNUM_ANALOG_CHANS

typedef struct {
    UINT32 chid;            // ID of the Channel
    UINT32 pktcnt;          // # of packets which can be saved
    UINT32 pktsize;         // Size of an individual packet
    UINT32 head;            // Where (0 based index) in the circular buffer to place the NEXT packet.
    UINT32 valid;           // How many packets have come in since the last configuration
    cbPKT_SPK spkpkt[cbPKT_SPKCACHEPKTCNT];     // Circular buffer of the cached spikes
} cbSPKCACHE;

typedef struct {
    UINT32 flags;
    UINT32 chidmax;
    UINT32 linesize;
    UINT32 spkcount;
    cbSPKCACHE cache[cbPKT_SPKCACHELINECNT];
} cbSPKBUFF;

cbRESULT cbGetSpkCache(UINT32 chid, cbSPKCACHE **cache, UINT32 nInstance = 0);

#ifdef WIN32
enum WM_USER_GLOBAL
{
    WM_USER_WAITEVENT = WM_USER,                // mmtimer says it is OK to continue
    WM_USER_CRITICAL_DATA_CATCHUP,              // We have reached a critical data point and we have skipped
};
#endif


#define cbRECBUFFLEN 2097118
typedef struct {
    UINT32 received;
    UINT32 lasttime;
    UINT32 headwrap;
    UINT32 headindex;
    UINT32 buffer[cbRECBUFFLEN];
} cbRECBUFF;

#ifdef _MSC_VER
// The following structure is used to hold Cerebus packets queued for transmission to the NSP.
// The length of the structure is set during initialization of the buffer in the Central App.
// The pragmas allow a zero-length data field entry in the structure for referencing the data.
#pragma warning(push)
#pragma warning(disable:4200)
#endif

typedef struct {
    UINT32 transmitted;     // How many packets have we sent out?

    UINT32 headindex;       // 1st empty position
                            // (moves on filling)

    UINT32 tailindex;       // 1 past last emptied position (empty when head = tail)
                            // Moves on emptying

    UINT32 last_valid_index;// index number of greatest valid starting index for a head (or tail)
    UINT32 bufferlen;       // number of indexes in buffer (units of UINT32) <------+
    UINT32 buffer[0];       // big buffer of data...there are actually "bufferlen"--+ indices
} cbXMTBUFF;

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#define WM_USER_SET_THOLD_SIGMA     (WM_USER + 100)
#define WM_USER_SET_THOLD_TIME      (WM_USER + 101)

typedef struct {
    // ***** THESE MUST BE 1ST IN THE STRUCTURE WITH MODELSET LAST OF THESE ***
    // ***** SEE WriteCCFNoPrompt() ***
    cbPKT_FS_BASIS          asBasis[cbMAXCHANS];    // All of the PCA basis values
    cbPKT_SS_MODELSET       asSortModel[cbMAXCHANS][cbMAXUNITS + 2];    // All of the model (rules) for spike sorting

    //////// These are spike sorting options
    cbPKT_SS_DETECT         pktDetect;        // parameters dealing with actual detection
    cbPKT_SS_ARTIF_REJECT   pktArtifReject;   // artifact rejection
    cbPKT_SS_NOISE_BOUNDARY pktNoiseBoundary[cbNUM_ANALOG_CHANS]; // where o'where are the noise boundaries
    cbPKT_SS_STATISTICS     pktStatistics;    // information about statistics
    cbPKT_SS_STATUS         pktStatus;        // Spike sorting status

} cbSPIKE_SORTING;

// This is the number of aout chans with gain. Conveniently, the
// 4 Analog Outputs and the 2 Audio Outputs are right next to each other
// in the channel numbering sequence.

typedef struct {
    char  m_sFileName[MAX_PATH];
    INT32 m_nNPulses;
    INT32 m_nOffset;
    INT32 m_nPhase1Duration;	// samples
    INT32 m_nPhase2Duration;	// samples
    INT32 m_nPhase1Amplitude;	// mV
    INT32 m_nPhase2Amplitude;	// mV
    INT32 m_nInterPhaseDelay;	// samples
    INT32 m_nInterPulseDelay;	// samples
    INT32 m_nFrequency;   		// HZ
    INT32 m_nWaveType;		    // Can be any of cbWaveType_*
} cbWaveformData;

class cbPcStatus
{
private:
    INT32 m_bRecording;
    INT32 m_iBlockRecording;
    cbPKT_UNIT_SELECTION m_pktSelection;
    cbWaveformData m_WaveformData[AOUT_NUM_GAIN_CHANS];

public:
    cbPcStatus() :
        m_bRecording(false),
        m_iBlockRecording(0), 
        m_pktSelection(1)
        {
        }
    bool IsRecording() { return (m_bRecording != 0); }
    void SetRecording(bool bRecording) { m_bRecording = bRecording; }
    bool IsRecordingBlocked() { return m_iBlockRecording != 0; }
    void SetBlockRecording(bool bBlockRecording) { m_iBlockRecording += bBlockRecording ? 1 : -1; }
    const cbPKT_UNIT_SELECTION * GetChannelSelections() const { return &m_pktSelection; }
    void SetChannelSelections(const cbPKT_UNIT_SELECTION & rPkt) { m_pktSelection = rPkt; }
    void InitWaveform()
    {
        for (int i = 0; i < AOUT_NUM_GAIN_CHANS; i++)
        {
            m_WaveformData[i].m_nWaveType = cbWaveType_NONE;
            m_WaveformData[i].m_nNPulses = 0;
            m_WaveformData[i].m_nOffset = 0;
            m_WaveformData[i].m_nPhase1Duration = 30;
            m_WaveformData[i].m_nPhase2Duration = 30;
            m_WaveformData[i].m_nPhase1Amplitude = 1000;
            m_WaveformData[i].m_nPhase2Amplitude = -1000;
            m_WaveformData[i].m_nInterPhaseDelay = 30;
            m_WaveformData[i].m_nInterPulseDelay = 30;
            m_WaveformData[i].m_nFrequency = 4;
        }
    }
    BOOL SetWaveformFile(int nChannel, LPCSTR filename, INT16 nLength)
    {
        if ((nLength + 1 ) < MAX_PATH)
        {
            m_WaveformData[nChannel].m_nWaveType = cbWaveType_FILE;
            memset(m_WaveformData[nChannel].m_sFileName, 0, sizeof(m_WaveformData[nChannel].m_sFileName));
            memcpy(m_WaveformData[nChannel].m_sFileName, filename, nLength);
            return TRUE;
        }
        return FALSE;
    }
    void SetWaveformParameter(int nChannel, INT32 nNPulses, INT32 nOffset, INT32 nPhase1Duration, INT32 nPhase2Duration, INT32 nPhase1Amplitude, INT32 nPhase2Amplitude, INT32 nInterPhaseDelay, INT32 nInterPulseDelay)
    {
        if ((nChannel >=0) && (nChannel < AOUT_NUM_GAIN_CHANS))
        {
            m_WaveformData[nChannel].m_nWaveType = cbWaveType_PARAMETERS;
            m_WaveformData[nChannel].m_nNPulses = nNPulses;
            m_WaveformData[nChannel].m_nOffset = nOffset;
            m_WaveformData[nChannel].m_nPhase1Duration = nPhase1Duration;
            m_WaveformData[nChannel].m_nPhase2Duration = nPhase2Duration;
            m_WaveformData[nChannel].m_nPhase1Amplitude = nPhase1Amplitude;
            m_WaveformData[nChannel].m_nPhase2Amplitude = nPhase2Amplitude;
            m_WaveformData[nChannel].m_nInterPhaseDelay = nInterPhaseDelay;
            m_WaveformData[nChannel].m_nInterPulseDelay = nInterPulseDelay;
        }
    }
    void SetWaveformSine(int nChannel, INT32 nPhase1Amplitude, INT32 nFrequency)
    {
        if ((nChannel >=0) && (nChannel < AOUT_NUM_GAIN_CHANS))
        {
            m_WaveformData[nChannel].m_nWaveType = cbWaveType_SINE;
            m_WaveformData[nChannel].m_nPhase1Amplitude = nPhase1Amplitude;
            m_WaveformData[nChannel].m_nFrequency = nFrequency;
        }
    }
    int GetWaveformType(int nChannel) { return m_WaveformData[nChannel].m_nWaveType; }
    char *GetWaveformFile(int nChannel) { return m_WaveformData[nChannel].m_sFileName; }
    INT32 GetWaveformPulses(int nChannel) { return m_WaveformData[nChannel].m_nNPulses; }
    INT32 GetWaveformOffset(int nChannel) { return m_WaveformData[nChannel].m_nOffset; }
    INT32 GetWaveformPhase1Duration(int nChannel) { return m_WaveformData[nChannel].m_nPhase1Duration; }
    INT32 GetWaveformPhase2Duration(int nChannel) { return m_WaveformData[nChannel].m_nPhase2Duration; }
    INT32 GetWaveformPhase1Amplitude(int nChannel) { return m_WaveformData[nChannel].m_nPhase1Amplitude; }
    INT32 GetWaveformPhase2Amplitude(int nChannel) { return m_WaveformData[nChannel].m_nPhase2Amplitude; }
    INT32 GetWaveformInterPhaseDelay(int nChannel) { return m_WaveformData[nChannel].m_nInterPhaseDelay; }
    INT32 GetWaveformInterPulseDelay(int nChannel) { return m_WaveformData[nChannel].m_nInterPulseDelay; }
    INT32 GetWaveformFrequency(int nChannel) { return m_WaveformData[nChannel].m_nFrequency; }
};

typedef struct {
    UINT32          version;
    UINT32          sysflags;
    cbOPTIONTABLE   optiontable;    // Should be 32 32-bit values
    cbCOLORTABLE    colortable;     // Should be 96 32-bit values
    cbPKT_SYSINFO   sysinfo;
    cbPKT_PROCINFO  procinfo[cbMAXPROCS];
    cbPKT_BANKINFO  bankinfo[cbMAXPROCS][cbMAXBANKS];
    cbPKT_GROUPINFO groupinfo[cbMAXPROCS][cbMAXGROUPS]; // sample group ID (1-4=proc1, 5-8=proc2, etc)
    cbPKT_GROUPINFO rawgroupinfo; // sample group ID (1-4=proc1, 5-8=proc2, etc)
    cbPKT_FILTINFO  filtinfo[cbMAXPROCS][cbMAXFILTS];
    cbPKT_ADAPTFILTINFO adaptinfo;          //  Settings about adapting
    cbPKT_REFELECFILTINFO refelecinfo;          //  Settings about reference electrode filtering
    cbPKT_CHANINFO  chaninfo[cbMAXCHANS];
    cbSPIKE_SORTING isSortingOptions;   // parameters dealing with spike sorting
    cbPKT_NTRODEINFO isNTrodeInfo[cbMAXNTRODES];  // allow for the max number of ntrodes (if all are stereo-trodes)
    cbPKT_LNC       isLnc; //LNC parameters
    cbPKT_NPLAY     isNPlay; // nPlay Info
    cbVIDEOSOURCE   isVideoSource[cbMAXVIDEOSOURCE]; // Video source
    cbTRACKOBJ      isTrackObj[cbMAXTRACKOBJ];       // Trackable objects
    // This must be at the bottom of this structure because it is variable size 32-bit or 64-bit
    // depending on the compile settings e.g. 64-bit cbmex communicating with 32-bit Central
    HANDLE          hwndCentral;    // Handle to the Window in Central

} cbCFGBUFF;

// Latest CCF structure
typedef struct {
    cbPKT_CHANINFO isChan[cbMAXCHANS];
    cbPKT_ADAPTFILTINFO isAdaptInfo;
    cbPKT_SS_DETECT isSS_Detect;
    cbPKT_SS_ARTIF_REJECT isSS_ArtifactReject;
    cbPKT_SS_NOISE_BOUNDARY isSS_NoiseBoundary[cbNUM_ANALOG_CHANS];
    cbPKT_SS_STATISTICS isSS_Statistics;
    cbPKT_SS_STATUS isSS_Status;
    cbPKT_SYSINFO isSysInfo;
    cbPKT_NTRODEINFO isNTrodeInfo[cbMAXNTRODES];
    cbPKT_AOUT_WAVEFORM isWaveform[AOUT_NUM_GAIN_CHANS][cbMAX_AOUT_TRIGGER];
    cbPKT_FILTINFO filtinfo[cbNUM_DIGITAL_FILTERS];
    cbPKT_LNC       isLnc;
} cbCCF;

// CCF processing state
typedef enum _cbStateCCF
{
    CCFSTATE_READ = 0,     // Reading in progress
    CCFSTATE_WRITE,        // Writing in progress
    CCFSTATE_SEND ,        // Sendign in progress
    CCFSTATE_CONVERT,      // Conversion in progress
    CCFSTATE_THREADREAD,   // Total threaded read progress
    CCFSTATE_THREADWRITE,  // Total threaded write progress
    CCFSTATE_UNKNOWN, // (Always the last) unknown state
} cbStateCCF;

// External Global Variables

extern HANDLE      cb_xmt_global_buffer_hnd[cbMAXOPEN];       // Transmit queues to send out of this PC
extern cbXMTBUFF*  cb_xmt_global_buffer_ptr[cbMAXOPEN];

extern HANDLE      cb_xmt_local_buffer_hnd[cbMAXOPEN];        // Transmit queues only for local (this PC) use
extern cbXMTBUFF*  cb_xmt_local_buffer_ptr[cbMAXOPEN];

extern HANDLE       cb_rec_buffer_hnd[cbMAXOPEN];
extern cbRECBUFF*   cb_rec_buffer_ptr[cbMAXOPEN];
extern HANDLE       cb_cfg_buffer_hnd[cbMAXOPEN];
extern cbCFGBUFF*   cb_cfg_buffer_ptr[cbMAXOPEN];
extern HANDLE       cb_pc_status_buffer_hnd[cbMAXOPEN];
extern cbPcStatus*  cb_pc_status_buffer_ptr[cbMAXOPEN];        // parameters dealing with local pc status
extern HANDLE       cb_spk_buffer_hnd[cbMAXOPEN];
extern cbSPKBUFF*   cb_spk_buffer_ptr[cbMAXOPEN];
extern HANDLE       cb_sig_event_hnd[cbMAXOPEN];

extern UINT32       cb_library_initialized[cbMAXOPEN];
extern UINT32       cb_library_index[cbMAXOPEN];

#endif

#pragma pack(pop)

#endif      // end of include guard

