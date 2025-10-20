#ifndef CBHWLIB_H_INCLUDED
#define CBHWLIB_H_INCLUDED

#include "cbproto.h"

#if defined(WIN32)
#ifndef _CRT_SECURE_NO_DEPRECATE
#define _CRT_SECURE_NO_DEPRECATE
#endif
// It has to be in this order for right version of sockets
#ifdef NO_AFX
#include <winsock2.h>
#include <windows.h>
#endif  // NO_AFX
#endif  // defined(WIN32)
#if defined(WIN32)
#include "pstdint.h"
#else
#include "stdint.h"
#include <stddef.h>  // For NULL on some compilers
#endif

#pragma pack(push, 1)

///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Systemwide Inquiry and Configuration Functions
//
///////////////////////////////////////////////////////////////////////////////////////////////////

// Open multiple instances of library as stand-alone or under Central application
cbRESULT cbOpen(bool bStandAlone = false, uint32_t nInstance = 0);
// Initializes the Neuromatic library (and establishes a link to the Central Control Application if bStandAlone is FALSE).
// This function must be called before any other functions are called from this library.
// Returns OK, NOCENTRALAPP, LIBINITERROR, MEMORYUNVAIL, HARDWAREOFFLINE, INSTOUTDATED or LIBOUTDATED

cbRESULT cbClose(bool bStandAlone = false, uint32_t nInstance = 0);
// Close the library (must match how library is openned)

cbRESULT cbCheckApp(const char * lpName);
// Check if an application is running using its mutex

cbRESULT cbAcquireSystemLock(const char * lpName, HANDLE & hLock);
cbRESULT cbReleaseSystemLock(const char * lpName, HANDLE & hLock);
// Acquire or release application system lock

uint32_t GetInstrumentLocalChan(uint32_t nChan, uint32_t nInstance = 0);
// Get the instrument local channel number

#define  cbINSTINFO_READY      0x0001     // Instrument is connected
#define  cbINSTINFO_LOCAL      0x0002     // Instrument runs on the localhost
#define  cbINSTINFO_NPLAY      0x0004     // Instrument is nPlay
#define  cbINSTINFO_CEREPLEX   0x0008     // Instrument is Cereplex
#define  cbINSTINFO_EMULATOR   0x0010     // Instrument is Emulator
#define  cbINSTINFO_NSP1       0x0020     // Instrument is NSP1
#define  cbINSTINFO_WNSP       0x0040     // Instrument is WNSP
cbRESULT cbGetInstInfo(uint32_t *instInfo, uint32_t nInstance = 0);
// Purpose: get instrument information.

cbRESULT cbGetLatency(uint32_t *nLatency, uint32_t nInstance = 0);
// Purpose: get instrument latency.

// Returns instrument information
// Returns cbRESULT_OK if successful, cbRESULT_NOLIBRARY if library was never initialized.
cbRESULT cbGetSystemClockFreq(uint32_t *freq, uint32_t nInstance = 0);
// Retrieves the system timestamp/sample clock frequency (in Hz) from the Central App cache.
//
// Returns: cbRESULT_OK if data successfully retrieved.
//          cbRESULT_NOLIBRARY if the library was not properly initialized

cbRESULT cbGetSystemClockTime(PROCTIME *time, uint32_t nInstance = 0);
// Retrieves the last 32-bit timestamp from the Central App cache.
//
// Returns: cbRESULT_OK if data successfully retrieved.
//          cbRESULT_NOLIBRARY if the library was not properly initialized


// Shuts down the programming library and frees any resources linked in cbOpen()
// Returns cbRESULT_OK if successful, cbRESULT_NOLIBRARY if library was never initialized.
// Updates the read pointers in the memory area so that
// all the un-read packets are ignored. In other words, it
// initializes all the pointers so that the begging of read time is NOW.
cbRESULT cbMakePacketReadingBeginNow(uint32_t nInstance = 0);
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

cbRESULT cbCheckforData(cbLevelOfConcern & nLevelOfConcern, uint32_t *pktstogo = nullptr, uint32_t nInstance = 0);
// The pktstogo and timetogo are optional fields (NULL if not used) that returns the number of new
// packets and timestamps that need to be read to catch up to the buffer.
//
// Returns: cbRESULT_OK    if there is new data in the buffer
//          cbRESULT_NONEWDATA if there is no new data available
//          cbRESULT_DATALOST if the Central App incoming data buffer has wrapped the read buffer

cbRESULT cbWaitforData(uint32_t nInstance = 0);
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

cbPKT_GENERIC *cbGetNextPacketPtr(uint32_t nInstance = 0);
// Returns pointer to next packet in the shared memory space.  If no packet available, returns NULL

// Cerebus Library function to send packets via the Central Application Queue
cbRESULT cbSendPacket(void * pPacket, uint32_t nInstance = 0);
cbRESULT cbSendPacketToInstrument(void * pPacket, uint32_t nInstance = 0, uint32_t nInstrument = cbNSP1 - 1);
cbRESULT cbSendLoopbackPacket(void * pPacket, uint32_t nInstance = 0);

cbRESULT cbGetVideoSource(char *name, float *fps, uint32_t id, uint32_t nInstance = 0);
cbRESULT cbSetVideoSource(const char *name, float fps, uint32_t id, uint32_t nInstance = 0);
// Get/Set the video source parameters.
//
// Returns: cbRESULT_OK if data successfully retrieved.
//          cbRESULT_NOLIBRARY if the library was not properly initialized.

cbRESULT cbGetTrackObj(char *name, uint16_t *type, uint16_t *pointCount, uint32_t id, uint32_t nInstance = 0);
cbRESULT cbSetTrackObj(const char *name, uint16_t type, uint16_t pointCount, uint32_t id, uint32_t nInstance = 0);
// Get/Set the trackable object parameters.
//
// Returns: cbRESULT_OK if data successfully retrieved.
//          cbRESULT_NOLIBRARY if the library was not properly initialized.

cbRESULT cbGetChanCaps(uint32_t chan, uint32_t *chancaps, uint32_t nInstance = 0);
// Retreives the channel capabilities from the Central App Cache.
//
// Returns: cbRESULT_OK if data successfully retreived or packet successfully queued to be sent.
//          cbRESULT_INVALIDCHANNEL if the specified channel is not mapped or does not exist.
//          cbRESULT_NOLIBRARY if the library was not properly initialized.

///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Digital Input Inquiry and Configuration Functions
//
///////////////////////////////////////////////////////////////////////////////////////////////////

cbRESULT cbGetDinpCaps(uint32_t chan, uint32_t *dinpcaps, uint32_t nInstance = 0);
// Retreives the channel's digital input capabilities from the Central App Cache.
// Port Capabilities are reported as compbined cbDINPOPT_* flags.  Zero = no DINP capabilities.
//
// Returns: cbRESULT_OK if data successfully retreived.
//          cbRESULT_INVALIDCHANNEL if the specified channel is not mapped or does not exist.
//          cbRESULT_NOLIBRARY if the library was not properly initialized.


cbRESULT cbGetDinpOptions(uint32_t chan, uint32_t *options, uint32_t *eopchar, uint32_t nInstance = 0);
cbRESULT cbSetDinpOptions(uint32_t chan, uint32_t options, uint32_t eopchar, uint32_t nInstance = 0);
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

///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Digital Output Inquiry and Configuration Functions
//
///////////////////////////////////////////////////////////////////////////////////////////////////

cbRESULT cbGetDoutCaps(uint32_t chan, uint32_t *doutcaps, uint32_t nInstance = 0);
// Retreives the channel's digital output capabilities from the Central Control App Cache.
// Port Capabilities are reported as compbined cbDOUTOPT_* flags.  Zero = no DINP capabilities.
//
// Returns: cbRESULT_OK if data successfully retreived.
//          cbRESULT_INVALIDCHANNEL if the specified channel is not mapped or does not exist.
//          cbRESULT_NOLIBRARY if the library was not properly initialized.


cbRESULT cbGetDoutOptions(uint32_t chan, uint32_t *options, uint32_t *monchan, int32_t *doutval,
                          uint8_t *triggertype = nullptr, uint16_t *trigchan = nullptr, uint16_t *trigval = nullptr, uint32_t nInstance = 0);
cbRESULT cbSetDoutOptions(uint32_t chan, uint32_t options, uint32_t monchan, int32_t doutval,
                          uint8_t triggertype = cbDOUT_TRIGGER_NONE, uint16_t trigchan = 0, uint16_t trigval = 0, uint32_t nInstance = 0);
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

///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Analog Input Inquiry and Configuration Functions
//
///////////////////////////////////////////////////////////////////////////////////////////////////

cbRESULT cbGetAinpCaps(uint32_t chan, uint32_t *ainpcaps, cbSCALING *physcalin, cbFILTDESC *phyfiltin, uint32_t nInstance = 0);
// Retreives the channel's analog input capabilities from the Central Control App Cache.
// Capabilities are reported as combined cbAINP_* flags.  Zero = no AINP capabilities.
//
// Returns: cbRESULT_OK if data successfully retreived.
//          cbRESULT_INVALIDCHANNEL if the specified channel is not mapped or does not exist.
//          cbRESULT_NOLIBRARY if the library was not properly initialized.

cbRESULT cbGetAinpOpts(uint32_t chan, uint32_t *ainpopts, uint32_t *LNCrate, uint32_t *refElecChan, uint32_t nInstance = 0);
cbRESULT cbSetAinpOpts(uint32_t chan, uint32_t ainpopts,  uint32_t LNCrate, uint32_t refElecChan, uint32_t nInstance = 0);
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
// The relationships between the adaptation time constant in sec, line frequency in Hz and
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


cbRESULT cbGetAinpScaling(uint32_t chan, cbSCALING *scaling, uint32_t nInstance = 0);
cbRESULT cbSetAinpScaling(uint32_t chan, const cbSCALING *scaling, uint32_t nInstance = 0);
// Get/Set the user-specified scaling for the channel.  The digmin and digmax values of the user
// specified scaling must be within the digmin and digmax values for the physical channel mapping.
//
// Returns: cbRESULT_OK if data successfully retreived.
//          cbRESULT_INVALIDCHANNEL if the specified channel is not mapped or does not exist.
//          cbRESULT_NOLIBRARY if the library was not properly initialized.

cbRESULT cbGetAinpDisplay(uint32_t chan, int32_t *smpdispmin, int32_t *smpdispmax, int32_t *spkdispmax, int32_t *lncdispmax, uint32_t nInstance = 0);
cbRESULT cbSetAinpDisplay(uint32_t chan, int32_t  smpdispmin, int32_t  smpdispmax, int32_t  spkdispmax, int32_t  lncdispmax, uint32_t nInstance = 0);
// Get and Set the display ranges used by User applications.  smpdispmin/max set the digital value
// range that should be displayed for the sampled analog stream.  Spike streams are assumed to be
// symmetric about zero so that spikes should be plotted from -spkdispmax to +spkdispmax.  Passing
// zero as a scale instructs the Central app to send the cached value.  Fields with NULL pointers
// are ignored by the library.
//
// Returns: cbRESULT_OK if data successfully retreived or packet successfully queued to be sent.
//          cbRESULT_INVALIDCHANNEL if the specified channel is not mapped or does not exist.
//          cbRESULT_NOLIBRARY if the library was not properly initialized.

cbRESULT cbSetAinpPreview(uint32_t chan, uint32_t prevopts, uint32_t nInstance = 0);
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

cbRESULT cbGetAinpSampling(uint32_t chan, uint32_t *filter, uint32_t *group, uint32_t nInstance = 0);
cbRESULT cbSetAinpSampling(uint32_t chan, uint32_t filter,  uint32_t group, uint32_t nInstance = 0);
// Get/Set the periodic sample group for the channel.  Continuous sampling is performed in
// groups with each Neural Signal Processor.  There are up to 4 groups for each processor.
// A group number of zero signifies that the channel is not part of a continuous sample group.
// filter = 1 to cbNFILTS, 0 is reserved for the null filter case
//
// Returns: cbRESULT_OK if data successfully retreived.
//          cbRESULT_INVALIDCHANNEL if the specified channel is not mapped or does not exist.
//          cbRESULT_INVALIDFUNCTION if the group number is not valid.
//          cbRESULT_NOLIBRARY if the library was not properly initialized.

//////////////////////////////////////////////////////////////
// AINP Spike Stream Functions

cbRESULT cbGetAinpSpikeCaps(uint32_t chan, uint32_t *flags, uint32_t nInstance = 0);
cbRESULT cbGetAinpSpikeOptions(uint32_t chan, uint32_t *flags, uint32_t *filter, uint32_t nInstance = 0);
cbRESULT cbSetAinpSpikeOptions(uint32_t chan, uint32_t flags,  uint32_t filter, uint32_t nInstance = 0);
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


cbRESULT cbGetAinpSpikeThreshold(uint32_t chan, int32_t *level, uint32_t nInstance = 0);
cbRESULT cbSetAinpSpikeThreshold(uint32_t chan, int32_t level, uint32_t nInstance = 0);
// Get/Set the spike detection threshold and threshold detection mode.
//
// Returns: cbRESULT_OK if data successfully retreived.
//          cbRESULT_INVALIDCHANNEL if the specified channel is not mapped or does not exist.
//          cbRESULT_INVALIDFUNCTION if invalid flag combinations are passed.
//          cbRESULT_NOLIBRARY if the library was not properly initialized.


cbRESULT cbGetAinpSpikeHoops(uint32_t chan, cbHOOP *hoops, uint32_t nInstance = 0);
cbRESULT cbSetAinpSpikeHoops(uint32_t chan, const cbHOOP *hoops, uint32_t nInstance = 0);
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

///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Analog Output Inquiry and Configuration Functions
//
///////////////////////////////////////////////////////////////////////////////////////////////////

cbRESULT cbGetAoutCaps(uint32_t chan, uint32_t *aoutcaps, cbSCALING *physcalout, cbFILTDESC *phyfiltout, uint32_t nInstance = 0);
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


cbRESULT cbGetAoutScaling(uint32_t chan, cbSCALING *scaling, uint32_t nInstance = 0);
cbRESULT cbSetAoutScaling(uint32_t chan, const cbSCALING *scaling, uint32_t nInstance = 0);
// Get/Set the user-specified scaling for the channel.  The digmin and digmax values of the user
// specified scaling must be within the digmin and digmax values for the physical channel mapping.
//
// Returns: cbRESULT_OK if data successfully retreived.
//          cbRESULT_INVALIDCHANNEL if the specified channel is not mapped or does not exist.
//          cbRESULT_NOLIBRARY if the library was not properly initialized.


cbRESULT cbGetAoutOptions(uint32_t chan, uint32_t *options, uint32_t *monchan, int32_t *value, uint32_t nInstance = 0);
cbRESULT cbSetAoutOptions(uint32_t chan, uint32_t options, uint32_t monchan, int32_t value, uint32_t nInstance = 0);
// Get/Set the Monitored channel for an Analog Output Port.  Setting zero for the monitored channel
// stops the monitoring and frees any instrument monitor resources.  The factor ranges
//
// Returns: cbRESULT_OK if data successfully retreived or packet successfully queued to be sent.
//          cbRESULT_NOINTERNALCHAN if there is no internal channel for mapping the in->out chan
//          cbRESULT_INVALIDCHANNEL if the specified channel is not mapped or does not exist.
//          cbRESULT_NOLIBRARY if the library was not properly initialized.


// Request that the sorting model be updated
cbRESULT cbGetSortingModel(uint32_t nInstance = 0);
cbRESULT cbGetFeatureSpaceDomain(uint32_t nInstance = 0);


// Getting and setting the noise boundary
cbRESULT cbSSGetNoiseBoundary(uint32_t chanIdx, float afCentroid[3], float afMajor[3], float afMinor_1[3], float afMinor_2[3], uint32_t nInstance = 0);
cbRESULT cbSSSetNoiseBoundary(uint32_t chanIdx, float afCentroid[3], float afMajor[3], float afMinor_1[3], float afMinor_2[3], uint32_t nInstance = 0);

cbRESULT cbSSGetNoiseBoundaryByTheta(uint32_t chanIdx, float afCentroid[3], float afAxisLen[3], float afTheta[3], uint32_t nInstance = 0);
cbRESULT cbSSSetNoiseBoundaryByTheta(uint32_t chanIdx, const float afCentroid[3], const float afAxisLen[3], const float afTheta[3], uint32_t nInstance = 0);

// Getting and settings statistics
cbRESULT cbSSGetStatistics(uint32_t * pnUpdateSpikes, uint32_t * pnAutoalg, uint32_t * nMode,
                           float * pfMinClusterPairSpreadFactor,
                           float * pfMaxSubclusterSpreadFactor,
                           float * pfMinClusterHistCorrMajMeasure,
                           float * pfMaxClusterPairHistCorrMajMeasure,
                           float * pfClusterHistValleyPercentage,
                           float * pfClusterHistClosePeakPercentage,
                           float * pfClusterHistMinPeakPercentage,
                           uint32_t * pnWaveBasisSize,
                           uint32_t * pnWaveSampleSize,
                           uint32_t nInstance = 0);

cbRESULT cbSSSetStatistics(uint32_t nUpdateSpikes, uint32_t nAutoalg, uint32_t nMode,
                           float fMinClusterPairSpreadFactor,
                           float fMaxSubclusterSpreadFactor,
                           float fMinClusterHistCorrMajMeasure,
                           float fMaxClusterPairHistCorrMajMeasure,
                           float fClusterHistValleyPercentage,
                           float fClusterHistClosePeakPercentage,
                           float fClusterHistMinPeakPercentage,
                           uint32_t nWaveBasisSize,
                           uint32_t nWaveSampleSize,
                           uint32_t nInstance = 0);


// Spike sorting artifact rejecting
cbRESULT cbSSGetArtifactReject(uint32_t * pnMaxChans, uint32_t * pnRefractorySamples, uint32_t nInstance = 0);
cbRESULT cbSSSetArtifactReject(uint32_t nMaxChans, uint32_t nRefractorySamples, uint32_t nInstance = 0);

// Spike detection parameters
cbRESULT cbSSGetDetect(float * pfThreshold, float * pfScaling, uint32_t nInstance = 0);
cbRESULT cbSSSetDetect(float fThreshold, float fScaling, uint32_t nInstance = 0);

// Getting and setting spike sorting status parameters
cbRESULT cbSSGetStatus(cbAdaptControl * pcntlUnitStats, cbAdaptControl * pcntlNumUnits, uint32_t nInstance = 0);
cbRESULT cbSSSetStatus(cbAdaptControl cntlUnitStats, cbAdaptControl cntlNumUnits, uint32_t nInstance = 0);


///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Data Packet Structures (chid<0x8000)
//
///////////////////////////////////////////////////////////////////////////////////////////////////

cbRESULT cbGetNplay(char *fname, float *speed, uint32_t *flags, PROCTIME *ftime, PROCTIME *stime, PROCTIME *etime, PROCTIME * filever, uint32_t nInstance = 0);
cbRESULT cbSetNplay(const char *fname, float speed, uint32_t mode, PROCTIME val, PROCTIME stime, PROCTIME etime, uint32_t nInstance = 0);
// Get/Set the nPlay parameters.
//
// Returns: cbRESULT_OK if data successfully retrieved.
//          cbRESULT_NOLIBRARY if the library was not properly initialized.


cbRESULT cbGetSpikeLength(uint32_t *length, uint32_t *pretrig, uint32_t * pSysfreq, uint32_t nInstance = 0);
cbRESULT cbSetSpikeLength(uint32_t length, uint32_t pretrig, uint32_t nInstance = 0);
// Get/Set the system-wide spike length.  Lengths should be specified in multiples of 2 and
// within the range of 16 to 128 samples long.
//
// Returns: cbRESULT_OK if data successfully retrieved.
//          cbRESULT_INVALIDCHANNEL if the specified channel is not mapped or does not exist.
//          cbRESULT_INVALIDFUNCTION if invalid flag combinations are passed.
//          cbRESULT_NOLIBRARY if the library was not properly initialized.

cbRESULT cbGetSystemRunLevel(uint32_t *runlevel, uint32_t *runflags, uint32_t *resetque, uint32_t nInstance = 0);
cbRESULT cbSetSystemRunLevel(uint32_t runlevel, uint32_t runflags, uint32_t resetque, uint8_t nInstrument = 0, uint32_t nInstance = 0);
// Get Set the System Condition
// Returns: cbRESULT_OK if data successfully retrieved.
//          cbRESULT_NOLIBRARY if the library was not properly initialized

cbRESULT cbSetComment(uint8_t charset, uint32_t rgba, PROCTIME time, const char* comment, uint32_t nInstance = 0);
// Set one comment event.
//
// Returns: cbRESULT_OK if data successfully retrieved.
//          cbRESULT_NOLIBRARY if the library was not properly initialized.


cbRESULT cbGetProcInfo(uint32_t proc, cbPROCINFO *procinfo, uint32_t nInstance = 0);
// Retreives information for the Signal Processor module located at procid
// The function requires an allocated but uninitialized cbPROCINFO structure.
//
// Returns: cbRESULT_OK if data successfully retreived.
//          cbRESULT_INVALIDADDRESS if no hardware at the specified Proc and Bank address
//          cbRESULT_NOLIBRARY if the library was not properly initialized.


cbRESULT cbGetChanCount(uint32_t *count, uint32_t nInstance = 0);
// Retreives the total number of channels in the system
//
// Returns: cbRESULT_OK if data successfully retreived.
//          cbRESULT_NOLIBRARY if the library was not properly initialized.


cbRESULT cbGetBankInfo(uint32_t proc, uint32_t bank, cbBANKINFO *bankinfo, uint32_t nInstance = 0);
// Retreives information for the Signal bank located at bankaddr on Proc procaddr.
// The function requires an allocated but uninitialized cbBANKINFO structure.
//
// Returns: cbRESULT_OK if data successfully retreived.
//          cbRESULT_INVALIDADDRESS if no hardware at the specified Proc and Bank address
//          cbRESULT_NOLIBRARY if the library was not properly initialized.

cbRESULT cbGetFilterDesc(uint32_t proc, uint32_t filt, cbFILTDESC *filtdesc, uint32_t nInstance = 0);
// Retreives the user filter definitions from a specific processor
// filter = 1 to cbNFILTS, 0 is reserved for the null filter case
// Returns: cbRESULT_OK if data successfully retreived.
//          cbRESULT_INVALIDADDRESS if no hardware at the specified Proc and Bank address
//          cbRESULT_NOLIBRARY if the library was not properly initialized.

// Tell me about the current adaptive filter settings
cbRESULT cbGetAdaptFilter(uint32_t  proc,             // which NSP processor?
                          uint32_t  * pnMode,         // 0=disabled, 1=filter continuous & spikes, 2=filter spikes
                          float   * pdLearningRate, // speed at which adaptation happens. Very small. e.g. 5e-12
                          uint32_t  * pnRefChan1,     // The first reference channel (1 based).
                          uint32_t  * pnRefChan2,     // The second reference channel (1 based).
                          uint32_t nInstance = 0);


// Update the adaptive filter settings
cbRESULT cbSetAdaptFilter(uint32_t  proc,             // which NSP processor?
                          const uint32_t  * pnMode,         // 0=disabled, 1=filter continuous & spikes, 2=filter spikes
                          const float   * pdLearningRate, // speed at which adaptation happens. Very small. e.g. 5e-12
                          const uint32_t  * pnRefChan1,     // The first reference channel (1 based).
                          const uint32_t  * pnRefChan2,     // The second reference channel (1 based).
                          uint32_t nInstance = 0);

// Useful for creating cbPKT_ADAPTFILTINFO packets
struct PktAdaptFiltInfo : public cbPKT_ADAPTFILTINFO
{
    PktAdaptFiltInfo(const uint32_t nMode, const float dLearningRate, const uint32_t nRefChan1, const uint32_t nRefChan2) : cbPKT_ADAPTFILTINFO()
    {
        this->cbpkt_header.chid = 0x8000;
        this->cbpkt_header.type = cbPKTTYPE_ADAPTFILTSET;
        this->cbpkt_header.dlen = cbPKTDLEN_ADAPTFILTINFO;

        this->nMode = nMode;
        this->dLearningRate = dLearningRate;
        this->nRefChan1 = nRefChan1;
        this->nRefChan2 = nRefChan2;
    };
};


///////////////////////////////////////////////////////////////////////////////////////////////////
// Reference Electrode filtering
// Tell me about the current reference electrode filter settings
cbRESULT cbGetRefElecFilter(uint32_t  proc,           // which NSP processor?
                            uint32_t  * pnMode,       // 0=disabled, 1=filter continuous & spikes, 2=filter spikes
                            uint32_t  * pnRefChan,    // The reference channel (1 based).
                            uint32_t nInstance = 0);


// Update the reference electrode filter settings
cbRESULT cbSetRefElecFilter(uint32_t  proc,           // which NSP processor?
                            const uint32_t  * pnMode,       // 0=disabled, 1=filter continuous & spikes, 2=filter spikes
                            const uint32_t  * pnRefChan,    // The reference channel (1 based).
                            uint32_t nInstance = 0);

// NTrode Information Packets
cbRESULT cbGetNTrodeInfo( uint32_t ntrode, char *label, cbMANUALUNITMAPPING ellipses[][cbMAXUNITS], uint16_t * nSite, uint16_t * chans, uint16_t * fs, uint32_t nInstance = 0);
cbRESULT cbSetNTrodeInfo( uint32_t ntrode, const char *label, cbMANUALUNITMAPPING ellipses[][cbMAXUNITS], uint16_t fs, uint32_t nInstance = 0);

// Sample Group (GROUP) Information Packets
cbRESULT cbGetSampleGroupInfo(uint32_t proc, uint32_t group, char *label, uint32_t *period, uint32_t *length, uint32_t nInstance = 0);
cbRESULT cbGetSampleGroupList(uint32_t proc, uint32_t group, uint32_t *length, uint16_t *list, uint32_t nInstance = 0);
cbRESULT cbSetSampleGroupOptions(uint32_t proc, uint32_t group, uint32_t period, char *label, uint32_t nInstance = 0);
// Retreives the Sample Group information in a processor and their definitions
// Labels are 16-characters maximum.
//
// Returns: cbRESULT_OK if data successfully retreived.
//          cbRESULT_INVALIDADDRESS if no hardware at the specified Proc and Bank address
//          cbRESULT_NOLIBRARY if the library was not properly initialized.


cbRESULT cbGetChanInfo(uint32_t chan, cbPKT_CHANINFO *pChanInfo, uint32_t nInstance = 0);
// Get the full channel config.
//
// Returns: cbRESULT_OK if data successfully retreived or packet successfully queued to be sent.
//          cbRESULT_INVALIDCHANNEL if the specified channel is not mapped or does not exist.
//          cbRESULT_NOLIBRARY if the library was not properly initialized.
cbRESULT cbGetChanAmplitudeReject(uint32_t chan, cbAMPLITUDEREJECT *AmplitudeReject, uint32_t nInstance = 0);
cbRESULT cbSetChanAmplitudeReject(uint32_t chan, cbAMPLITUDEREJECT AmplitudeReject, uint32_t nInstance = 0);
// Get and Set the user-assigned amplitude reject values.
//
// Returns: cbRESULT_OK if data successfully retreived or packet successfully queued to be sent.
//          cbRESULT_INVALIDCHANNEL if the specified channel is not mapped or does not exist.
//          cbRESULT_NOLIBRARY if the library was not properly initialized.

cbRESULT cbGetChanAutoThreshold(uint32_t chan, uint32_t *bEnabled, uint32_t nInstance = 0);
cbRESULT cbSetChanAutoThreshold(uint32_t chan, uint32_t bEnabled, uint32_t nInstance = 0);
// Get and Set the user-assigned auto threshold option
//
// Returns: cbRESULT_OK if data successfully retreived or packet successfully queued to be sent.
//          cbRESULT_INVALIDCHANNEL if the specified channel is not mapped or does not exist.
//          cbRESULT_NOLIBRARY if the library was not properly initialized.

cbRESULT cbGetChanUnitMapping( uint32_t chan, cbMANUALUNITMAPPING *unitmapping, uint32_t nInstance = 0);
cbRESULT cbSetChanUnitMapping( uint32_t chan, const cbMANUALUNITMAPPING *unitmapping, uint32_t nInstance = 0);
// Get and Set the user-assigned unit override for the channel.
//
// Returns: cbRESULT_OK if data successfully retreived or packet successfully queued to be sent.
//          cbRESULT_INVALIDCHANNEL if the specified channel is not mapped or does not exist.
//          cbRESULT_NOLIBRARY if the library was not properly initialized.

cbRESULT cbGetChanLoc(uint32_t chan, uint32_t *proc, uint32_t *bank, char *banklabel, uint32_t *term, uint32_t nInstance = 0);
// Gives the physical processor number, bank label, and terminal number of the specified channel
// by reading the configuration data in the Central App Cache.  Bank Labels are the name of the
// bank that is written on the instrument, and they are null-terminated, up to 16 char long.
//
// Returns: cbRESULT_OK if all is ok
//          cbRESULT_INVALIDCHANNEL if the specified channel is not mapped or does not exist.
//          cbRESULT_NOLIBRARY if the library was not properly initialized.


// flags for user flags...no effect on the cerebus
//#define cbUSER_DISABLED     0x00000001  // Channel should be electrically disabled
//#define cbUSER_EXPERIMENT   0x00000100  // Channel used for experiment environment information
//#define cbUSER_NEURAL       0x00000200  // Channel connected to neural electrode or signal

cbRESULT cbGetChanLabel(uint32_t chan, char *label, uint32_t *userflags, int32_t *position, uint32_t nInstance = 0);
cbRESULT cbSetChanLabel(uint32_t chan, const char *label, uint32_t userflags, const int32_t *position, uint32_t nInstance = 0);
// Get and Set the user-assigned label for the channel.  Channel Names may be up to 16 chars long
// and should be null terminated if shorter.
//
// Returns: cbRESULT_OK if data successfully retreived or packet successfully queued to be sent.
//          cbRESULT_INVALIDCHANNEL if the specified channel is not mapped or does not exist.
//          cbRESULT_NOLIBRARY if the library was not properly initialized.

cbRESULT cbGetChanNTrodeGroup(uint32_t chan, uint32_t *NTrodeGroup, uint32_t nInstance = 0);
cbRESULT cbSetChanNTrodeGroup(uint32_t chan, uint32_t NTrodeGroup, uint32_t nInstance = 0);
// Get and Set the user-assigned label for the N-Trode.  N-Trode Names may be up to 16 chars long
// and should be null terminated if shorter.
//
// Returns: cbRESULT_OK if data successfully retreived or packet successfully queued to be sent.
//          cbRESULT_INVALIDCHANNEL if the specified channel is not mapped or does not exist.
//          cbRESULT_NOLIBRARY if the library was not properly initialized.

/////////////////////////////////////////////////////////////////////////////////
// These are part of the "reflected" mechanism. They go out as type 0xE? and come
// Back in as type 0x6?

#define cbPKTTYPE_MASKED_REFLECTED              0xE0
#define cbPKTTYPE_COMPARE_MASK_REFLECTED        0xF0
#define cbPKTTYPE_REFLECTED_CONVERSION_MASK     0x7F


cbRESULT cbGetChannelSelection(cbPKT_UNIT_SELECTION* pPktUnitSel, uint32_t nProc, uint32_t nInstance = 0);
// Retreives the channel unit selection status
// Returns: cbRESULT_OK if data successfully retreived.
//          cbRESULT_NOLIBRARY if the library was not properly initialized.

cbRESULT cbGetFileInfo(cbPKT_FILECFG * filecfg, uint32_t nInstance = 0);
// Retreives the file recordign status
// Returns: cbRESULT_OK if data successfully retreived.
//          cbRESULT_NOLIBRARY if the library was not properly initialized.

//-----------------------------------------------------
///// Packets to tell me about the spike sorting model
/// Sets the noise boundary parameters
// The information obtained by these functions is implicit within the structure data,
// but they are provided for convenience
void GetAxisLengths(const cbPKT_SS_NOISE_BOUNDARY *pPkt, float afAxisLen[3]);
void GetRotationAngles(const cbPKT_SS_NOISE_BOUNDARY *pPkt, float afTheta[3]);
void InitPktSSNoiseBoundary(cbPKT_SS_NOISE_BOUNDARY* pPkt, uint32_t chan, float cen1, float cen2, float cen3, float maj1, float maj2, float maj3,
                            float min11, float min12, float min13, float min21, float min22, float min23);

/// Send this packet to the NSP to tell it to reset all spike sorting to default values
cbRESULT cbSetSSReset(uint32_t nInstance = 0);
// Restart spike sorting (applies to histogram peak count).
//
// Returns: cbRESULT_OK if data successfully retrieved.
//          cbRESULT_NOLIBRARY if the library was not properly initialized.

// This packet contains the status of the automatic spike sorting.
//
/// Send this packet to the NSP to tell it to re calculate all PCA Basis Vectors and Values
cbRESULT cbSetSSRecalc(uint8_t proc, uint32_t chan, uint32_t mode, uint32_t nInstance = 0);
// Recalc spike sorting (applies to PCA based algorithms).
//
// Returns: cbRESULT_OK if data successfully retrieved.
//          cbRESULT_NOLIBRARY if the library was not properly initialized.


cbRESULT cbGetLncParameters(uint32_t nProc, uint32_t* nLncFreq, uint32_t* nLncRefChan, uint32_t* nLncGMode, uint32_t nInstance = 0);
cbRESULT cbSetLncParameters(uint32_t nProc, uint32_t nLncFreq, uint32_t nLncRefChan, uint32_t nLncGMode, uint32_t nInstance = 0);
// Get/Set the system-wide LNC parameters.
//
// Returns: cbRESULT_OK if data successfully retrieved.
//          cbRESULT_NOLIBRARY if the library was not properly initialized.


cbRESULT cbGetAoutWaveform(uint32_t channel, uint8_t  trigNum, uint16_t  * mode, uint32_t  * repeats, uint16_t  * trig,
                           uint16_t  * trigChan, uint16_t  * trigValue, cbWaveformData * wave, uint32_t nInstance = 0);
// Returns anallog output waveform information
// Returns cbRESULT_OK if successful, cbRESULT_NOLIBRARY if library was never initialized.

/// @author     Hyrum L. Sessions
/// @date       1 Aug 2019
/// @brief      Get the waveform number for a specific aout channel
///
/// since channels are not contiguous, we can't just subtract the number of analog channels to
/// get the number.
///
/// @param [in] channel  - 1-based channel number
/// @param [out] wavenum  -
/// @param [in] nInstance - Instance number of the library
cbRESULT cbGetAoutWaveformNumber(uint32_t channel, uint32_t* wavenum, uint32_t nInstance = 0);


///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Shared Memory Definitions used by Central App and Cerebus library functions
//
///////////////////////////////////////////////////////////////////////////////////////////////////

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

cbRESULT cbGetColorTable(cbCOLORTABLE **colortable, uint32_t nInstance = 0);


typedef struct {
    float fRMSAutoThresholdDistance;    // multiplier to use for autothresholding when using
    // RMS to guess noise
    uint32_t reserved[31];
} cbOPTIONTABLE;


// Get/Set the multiplier to use for autothresholdine when using RMS to guess noise
// This will adjust fAutoThresholdDistance above, but use the API instead
float cbGetRMSAutoThresholdDistance(uint32_t nInstance = 0);
void cbSetRMSAutoThresholdDistance(float fRMSAutoThresholdDistance, uint32_t nInstance = 0);

//////////////////////////////////////////////////////////////////////////////////////////////////


#define cbPKT_SPKCACHEPKTCNT  400
#define cbPKT_SPKCACHELINECNT cbNUM_ANALOG_CHANS

typedef struct {
    uint32_t chid;            // ID of the Channel
    uint32_t pktcnt;          // # of packets which can be saved
    uint32_t pktsize;         // Size of an individual packet
    uint32_t head;            // Where (0 based index) in the circular buffer to place the NEXT packet.
    uint32_t valid;           // How many packets have come in since the last configuration
    cbPKT_SPK spkpkt[cbPKT_SPKCACHEPKTCNT];     // Circular buffer of the cached spikes
} cbSPKCACHE;

typedef struct {
    uint32_t flags;
    uint32_t chidmax;
    uint32_t linesize;
    uint32_t spkcount;
    cbSPKCACHE cache[cbPKT_SPKCACHELINECNT];
} cbSPKBUFF;

cbRESULT cbGetSpkCache(uint32_t chid, cbSPKCACHE **cache, uint32_t nInstance = 0);

#ifdef WIN32
enum WM_USER_GLOBAL
{
    WM_USER_WAITEVENT = WM_USER,                // mmtimer says it is OK to continue
    WM_USER_CRITICAL_DATA_CATCHUP,              // We have reached a critical data point and we have skipped
};
#endif


#define cbRECBUFFLEN (cbNUM_FE_CHANS * 32768 * 4)
typedef struct {
    uint32_t received;
    PROCTIME lasttime;
    uint32_t headwrap;
    uint32_t headindex;
    uint32_t buffer[cbRECBUFFLEN];
} cbRECBUFF;

#ifdef _MSC_VER
// The following structure is used to hold Cerebus packets queued for transmission to the NSP.
// The length of the structure is set during initialization of the buffer in the Central App.
// The pragmas allow a zero-length data field entry in the structure for referencing the data.
#pragma warning(push)
#pragma warning(disable:4200)
#endif

typedef struct {
    uint32_t transmitted;     // How many packets have we sent out?

    uint32_t headindex;       // 1st empty position
    // (moves on filling)

    uint32_t tailindex;       // 1 past last emptied position (empty when head = tail)
    // Moves on emptying

    uint32_t last_valid_index;// index number of greatest valid starting index for a head (or tail)
    uint32_t bufferlen;       // number of indexes in buffer (units of uint32_t) <------+
    uint32_t buffer[0];       // big buffer of data...there are actually "bufferlen"--+ indices
} cbXMTBUFF;

#define cbXMT_GLOBAL_BUFFLEN    ((cbCER_UDP_SIZE_MAX / 4) * 5000 + 2)     // room for 500 packets
#define cbXMT_LOCAL_BUFFLEN     ((cbCER_UDP_SIZE_MAX / 4) * 2000 + 2)     // room for 200 packets


#ifdef _MSC_VER
#pragma warning(pop)
#endif

#define WM_USER_SET_THOLD_SIGMA     (WM_USER + 100)
#define WM_USER_SET_THOLD_TIME      (WM_USER + 101)

typedef struct {
    // ***** THESE MUST BE 1ST IN THE STRUCTURE WITH MODELSET LAST OF THESE ***
    // ***** SEE WriteCCFNoPrompt() ***
    cbPKT_FS_BASIS          asBasis[cbMAXCHANS];    // All the PCA basis values
    cbPKT_SS_MODELSET       asSortModel[cbMAXCHANS][cbMAXUNITS + 2];    // All the model (rules) for spike sorting

    //////// These are spike sorting options
    cbPKT_SS_DETECT         pktDetect;        // parameters dealing with actual detection
    cbPKT_SS_ARTIF_REJECT   pktArtifReject;   // artifact rejection
    cbPKT_SS_NOISE_BOUNDARY pktNoiseBoundary[cbMAXCHANS]; // where o'where are the noise boundaries
    cbPKT_SS_STATISTICS     pktStatistics;    // information about statistics
    cbPKT_SS_STATUS         pktStatus;        // Spike sorting status

} cbSPIKE_SORTING;

#define PCSTAT_TYPE_CERVELLO        0x00000001      // Cervello type system
#define PCSTAT_DISABLE_RAW          0x00000002      // Disable recording of raw data

enum NSP_STATUS { NSP_INIT, NSP_NOIPADDR, NSP_NOREPLY, NSP_FOUND, NSP_INVALID };

class cbPcStatus
{
public:
    cbPKT_UNIT_SELECTION isSelection[cbMAXPROCS]{};

private:
    int32_t m_iBlockRecording;
    uint32_t m_nPCStatusFlags;
    uint32_t m_nNumFEChans;     // number of each type of channels received from the instrument
    uint32_t m_nNumAnainChans;
    uint32_t m_nNumAnalogChans;
    uint32_t m_nNumAoutChans;
    uint32_t m_nNumAudioChans;
    uint32_t m_nNumAnalogoutChans;
    uint32_t m_nNumDiginChans;
    uint32_t m_nNumSerialChans;
    uint32_t m_nNumDigoutChans;
    uint32_t m_nNumTotalChans;
#ifndef CBPROTO_311
    NSP_STATUS  m_nNspStatus[cbMAXPROCS]{};       // true if the nsp has received a sysinfo from each NSP
    uint32_t m_nNumNTrodesPerInstrument[cbMAXPROCS]{};
    uint32_t m_nGeminiSystem;   // Used as boolean true if connected to a gemini system
#endif

public:
    cbPcStatus() :
            m_iBlockRecording(0),
            m_nPCStatusFlags(0),
            m_nNumFEChans(0),
            m_nNumAnainChans(0),
            m_nNumAnalogChans(0),
            m_nNumAoutChans(0),
            m_nNumAudioChans(0),
            m_nNumAnalogoutChans(0),
            m_nNumDiginChans(0),
            m_nNumSerialChans(0),
            m_nNumDigoutChans(0),
            m_nNumTotalChans(0)
#ifndef CBPROTO_311
    , m_nGeminiSystem(0)
#endif
    {
        for (uint32_t nProc = 0; nProc < cbMAXPROCS; ++nProc)
        {
            isSelection[nProc].lastchan = 1;
#ifndef CBPROTO_311
            m_nNspStatus[nProc] = NSP_INIT;
            m_nNumNTrodesPerInstrument[nProc] = cbMAXNTRODES;
#endif
        }
    }
    [[nodiscard]] bool IsRecordingBlocked() const           { return m_iBlockRecording != 0; }
    [[nodiscard]] uint32_t cbGetPCStatusFlags() const       { return m_nPCStatusFlags; }
    [[nodiscard]] uint32_t cbGetNumFEChans() const          { return m_nNumFEChans; }
    [[nodiscard]] uint32_t cbGetNumAnainChans() const       { return m_nNumAnainChans; }
    [[nodiscard]] uint32_t cbGetNumAnalogChans() const      { return m_nNumAnalogChans; }
    [[nodiscard]] uint32_t cbGetNumAoutChans() const        { return m_nNumAoutChans; }
    [[nodiscard]] uint32_t cbGetNumAudioChans() const       { return m_nNumAudioChans; }
    [[nodiscard]] uint32_t cbGetNumAnalogoutChans() const   { return m_nNumAnalogoutChans; }
    [[nodiscard]] uint32_t cbGetNumDiginChans() const       { return m_nNumDiginChans; }
    [[nodiscard]] uint32_t cbGetNumSerialChans() const      { return m_nNumSerialChans; }
    [[nodiscard]] uint32_t cbGetNumDigoutChans() const      { return m_nNumDigoutChans; }
    [[nodiscard]] uint32_t cbGetNumTotalChans() const       { return m_nNumTotalChans; }

#ifndef CBPROTO_311
    [[nodiscard]] NSP_STATUS cbGetNspStatus(const uint32_t nProc) const { return m_nNspStatus[nProc]; }
    [[nodiscard]] uint32_t cbGetNumNTrodesPerInstrument(const uint32_t nProc) const { return m_nNumNTrodesPerInstrument[nProc - 1]; }
    void cbSetNspStatus(const uint32_t nInstrument, const NSP_STATUS nStatus) { m_nNspStatus[nInstrument] = nStatus; }
    void cbSetNumNTrodesPerInstrument(const uint32_t nInstrument, const uint32_t nNumNTrodesPerInstrument) { m_nNumNTrodesPerInstrument[nInstrument - 1] = nNumNTrodesPerInstrument; }
#else
    // m_NspStatus not avail in 3.11 so set to always found to pass logic checks.
    NSP_STATUS cbGetNspStatus(uint32_t nProc) { return NSP_FOUND; }
#endif
    void SetBlockRecording(const bool bBlockRecording)                { m_iBlockRecording += bBlockRecording ? 1 : -1; }
    void cbSetPCStatusFlags(const uint32_t nPCStatusFlags)            { m_nPCStatusFlags = nPCStatusFlags; }
    void cbSetNumFEChans(const uint32_t nNumFEChans)                  { m_nNumFEChans = nNumFEChans; }
    void cbSetNumAnainChans(const uint32_t nNumAnainChans)            { m_nNumAnainChans = nNumAnainChans; }
    void cbSetNumAnalogChans(const uint32_t nNumAnalogChans)          { m_nNumAnalogChans = nNumAnalogChans; }
    void cbSetNumAoutChans(const uint32_t nNumAoutChans)              { m_nNumAoutChans = nNumAoutChans; }
    void cbSetNumAudioChans(const uint32_t nNumAudioChans)            { m_nNumAudioChans = nNumAudioChans; }
    void cbSetNumAnalogoutChans(const uint32_t nNumAnalogoutChans)    { m_nNumAnalogoutChans = nNumAnalogoutChans; }
    void cbSetNumDiginChans(const uint32_t nNumDiginChans)            { m_nNumDiginChans = nNumDiginChans; }
    void cbSetNumSerialChans(const uint32_t nNumSerialChans)          { m_nNumSerialChans = nNumSerialChans; }
    void cbSetNumDigoutChans(const uint32_t nNumDigoutChans)          { m_nNumDigoutChans = nNumDigoutChans; }
    void cbSetNumTotalChans(const uint32_t nNumTotalChans)            { m_nNumTotalChans = nNumTotalChans; }
};

typedef struct {
    uint32_t          version;
    uint32_t          sysflags;
    cbOPTIONTABLE   optiontable;    // Should be 32 32-bit values
    cbCOLORTABLE    colortable;     // Should be 96 32-bit values
    cbPKT_SYSINFO   sysinfo;
    cbPKT_PROCINFO  procinfo[cbMAXPROCS];
    cbPKT_BANKINFO  bankinfo[cbMAXPROCS][cbMAXBANKS];
    cbPKT_GROUPINFO groupinfo[cbMAXPROCS][cbMAXGROUPS]; // sample group ID (1-4=proc1, 5-8=proc2, etc)
    cbPKT_FILTINFO  filtinfo[cbMAXPROCS][cbMAXFILTS];
    cbPKT_ADAPTFILTINFO adaptinfo[cbMAXPROCS];          //  Settings about adapting
    cbPKT_REFELECFILTINFO refelecinfo[cbMAXPROCS];      //  Settings about reference electrode filtering
    cbPKT_CHANINFO  chaninfo[cbMAXCHANS];
    cbSPIKE_SORTING isSortingOptions;   // parameters dealing with spike sorting
    cbPKT_NTRODEINFO isNTrodeInfo[cbMAXNTRODES];  // allow for the max number of ntrodes (if all are stereo-trodes)
    cbPKT_AOUT_WAVEFORM isWaveform[AOUT_NUM_GAIN_CHANS][cbMAX_AOUT_TRIGGER]; // Waveform parameters
    cbPKT_LNC       isLnc[cbMAXPROCS]; //LNC parameters
    cbPKT_NPLAY     isNPlay; // nPlay Info
    cbVIDEOSOURCE   isVideoSource[cbMAXVIDEOSOURCE]; // Video source
    cbTRACKOBJ      isTrackObj[cbMAXTRACKOBJ];       // Trackable objects
    cbPKT_FILECFG   fileinfo; // File recording status
    // This must be at the bottom of this structure because it is variable size 32-bit or 64-bit
    // depending on the compile settings e.g. 64-bit cbmex communicating with 32-bit Central
    HANDLE          hwndCentral;    // Handle to the Window in Central

} cbCFGBUFF;



// External Global Variables

typedef struct cbSharedMemHandle {
    HANDLE hnd = nullptr;
    char name[64] = {0};
    int fd = -1;
    uint32_t size = 0;
} cbSharedMemHandle;

extern cbSharedMemHandle  cb_xmt_global_buffer_hnd[cbMAXOPEN];       // Transmit queues to send out of this PC
extern cbXMTBUFF*  cb_xmt_global_buffer_ptr[cbMAXOPEN];

extern cbSharedMemHandle  cb_xmt_local_buffer_hnd[cbMAXOPEN];        // Transmit queues only for local (this PC) use
extern cbXMTBUFF*  cb_xmt_local_buffer_ptr[cbMAXOPEN];

extern cbSharedMemHandle  cb_rec_buffer_hnd[cbMAXOPEN];
extern cbRECBUFF*   cb_rec_buffer_ptr[cbMAXOPEN];
extern cbSharedMemHandle  cb_cfg_buffer_hnd[cbMAXOPEN];
extern cbCFGBUFF*   cb_cfg_buffer_ptr[cbMAXOPEN];
extern cbSharedMemHandle  cb_pc_status_buffer_hnd[cbMAXOPEN];
extern cbPcStatus*  cb_pc_status_buffer_ptr[cbMAXOPEN];        // parameters dealing with local pc status
extern cbSharedMemHandle  cb_spk_buffer_hnd[cbMAXOPEN];
extern cbSPKBUFF*   cb_spk_buffer_ptr[cbMAXOPEN];
extern HANDLE       cb_sig_event_hnd[cbMAXOPEN];

extern uint32_t       cb_library_initialized[cbMAXOPEN];
extern uint32_t       cb_library_index[cbMAXOPEN];

#pragma pack(pop)

#endif      // end of include guard