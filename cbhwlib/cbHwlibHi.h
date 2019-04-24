/* =STS=> cbHwlibHi.h[1689].aa11   submit   SMID:13 */
//////////////////////////////////////////////////////////////////////////////////////////////////
//
// (c) Copyright 2003-2008 Cyberkinetics, Inc.
// (c) Copyright 2008-2012 Blackrock Microsystems
//
// $Workfile: cbHwlibHi.h $
// $Archive: /Cerebus/WindowsApps/cbhwlib/cbHwlibHi.h $
// $Revision: 10 $
// $Date: 2/11/04 1:49p $
// $Author: Kkorver $
//
// $NoKeywords: $
//
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef CBHWLIBHI_H_INCLUDED
#define CBHWLIBHI_H_INCLUDED

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "cbhwlib.h"

#define MIN_CHANS              1
#define MAX_CHANS_ARRAY        96       // In neuroport there are only 96 channels

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

// Currently we have 8 acquisition groups:
//                                  Sampling        Filter
enum { ACQGRP_NONE,             //  not sampled
       ACQGRP_1KS_EEG,          //  1  kS/s         EEG/ECG/EOG/ERG
       ACQGRP_1KS,              //  1  kS/s         LFP Wide
       ACQGRP_1KS_SPIKE_MED,    //  1  kS/s         Spike Medium
       ACQGRP_2KS,              //  2  kS/s         LFP XWide
       ACQGRP_10KS,             //  10 kS/s         Activity
       ACQGRP_30KS,             //  30 kS/s         Unfiltered
       ACQGRP_30KS_EMG,         //  30 kS/s         EMG
       ACQGRP_30KS_EEG,         //  30 kS/s         EEG
       ACQ_GROUP_COUNT
};

// Currently we have 5 sampling rates:
//                                  Sampling
enum { SMPGRP_NONE,             //  not sampled
       SMPGRP_500S,             //  500  S/s
       SMPGRP_1KS,              //  1  kS/s
       SMPGRP_2KS,              //  2  kS/s
       SMPGRP_10KS,             //  10 kS/s
       SMPGRP_30KS,             //  30 kS/s
       SMP_GROUP_COUNT
};


///////////////////////////////////////////////////////////////////////////////////////////////////

bool IsSimRunning(uint32_t nInstance = 0);    // TRUE means that CentralSim is being used; FALSE, not

bool IsSpikeProcessingEnabled(uint32_t nChan, uint32_t nInstance = 0);    // TRUE means yes; FALSE, no
bool IsContinuousProcessingEnabled(uint32_t nChan, uint32_t nInstance = 0); // TRUE means yes; FALSE, no
bool IsRawProcessingEnabled(uint32_t nChan, uint32_t nInstance = 0);		// true means yes
// Is this channel enabled?
// This will figure out what kind of channel, and then find out if it is enabled
bool IsChannelEnabled(uint32_t nChannel, uint32_t nInstance = 0);

// Use these with care. make sure you know what kind of channel you have
bool IsAnalogInEnabled(uint32_t nChannel);
bool IsAudioEnabled(uint32_t nChannel);
bool IsAnalogOutEnabled(uint32_t nChannel);
bool IsDigitalInEnabled(uint32_t nChannel);
bool IsSerialEnabled(uint32_t nChannel, uint32_t nInstance = 0);
bool IsDigitalOutEnabled(uint32_t nChannel, uint32_t nInstance = 0);

// Is it "this" kind of channel? (very fast)
bool IsChanAnalogIn(uint32_t dwChan, uint32_t nInstance = 0);             // TRUE means yes; FALSE, no
bool IsChanFEAnalogIn(uint32_t dwChan, uint32_t nInstance = 0);           // TRUE means yes; FALSE, no
bool IsChanAIAnalogIn(uint32_t dwChan, uint32_t nInstance = 0);           // TRUE means yes; FALSE, no
bool IsChanAnyDigIn(uint32_t dwChan, uint32_t nInstance = 0);  // TRUE means yes; FALSE, no
bool IsChanSerial(uint32_t dwChan, uint32_t nInstance = 0); // TRUE means yes; FALSE, no
bool IsChanDigin(uint32_t dwChan, uint32_t nInstance = 0);  // TRUE means yes; FALSE, no
bool IsChanDigout(uint32_t dwChan, uint32_t nInstance = 0);               // TRUE means yes; FALSE, no
bool IsChanCont(uint32_t dwChan, uint32_t nInstance = 0);                 // TRUE means yes; FALSE, no

bool AreHoopsDefined(uint32_t nChannel, uint32_t nInstance = 0);
bool AreHoopsDefined(uint32_t nChannel, uint32_t nUnit, uint32_t nInstance = 0);


// Author & Date:   Ehsan Azar   June 3, 2009
// Purpose: determine if a channel has  valid sorting unit
// Input:   nChannel = channel to track   1 - based
//          dwUnit the unit number (1=< dwUnit <=cbMAXUNITS)
bool cbHasValidUnit(uint32_t dwChan, uint32_t dwUnit, uint32_t nInstance = 0);                 // TRUE means yes; FALSE, no

// Author & Date:   Ehsan Azar   June 2, 2009
// Purpose: Return TRUE if given channel has a sorting algorithm
//       If cbAINPSPK_ALLSORT is passed it returns TRUE if there is any sorting
//       If cbAINPSPK_NOSORT is passed it returns TRUE if there is no sorting
// Input:   spkSrtOpt = Sorting methods:
//                      cbAINPSPK_HOOPSORT, cbAINPSPK_SPREADSORT,
//                      cbAINPSPK_CORRSORT, cbAINPSPK_PEAKMAJSORT,
//                      cbAINPSPK_PEAKFISHSORT, cbAINPSPK_PCAMANSORT,
//                      cbAINPSPK_PCAKMEANSORT, cbAINPSPK_PCAEMSORT,
//                      cbAINPSPK_PCADBSORT, cbAINPSPK_NOSORT,
//                      cbAINPSPK_OLDAUTOSORT, cbAINPSPK_ALLSORT
bool cbHasSpikeSorting(uint32_t dwChan, uint32_t spkSrtOpt, uint32_t nInstance = 0);

// Author & Date:   Ehsan Azar   June 2, 2009
// Purpose: Set a sorting algorithm for a channel
// Input:   spkSrtOpt = Sorting methods:
//                      cbAINPSPK_HOOPSORT, cbAINPSPK_SPREADSORT,
//                      cbAINPSPK_CORRSORT, cbAINPSPK_PEAKMAJSORT,
//                      cbAINPSPK_PEAKFISHSORT, cbAINPSPK_PCAMANSORT,
//                      cbAINPSPK_PCAKMEANSORT, cbAINPSPK_PCAEMSORT,
//                      cbAINPSPK_PCADBSORT, cbAINPSPK_NOSORT
cbRESULT cbSetSpikeSorting(uint32_t dwChan, uint32_t spkSrtOpt, uint32_t nInstance = 0);


// Other functions
void TrackChannel(uint32_t nChannel, uint32_t nInstance = 0);


// Purpose: update the analog input scaling. If there is an Analog Output
//  channel which is monitoring this channel, then update the output
//  scaling to match the displayed scaling
// Inputs:
//  chan - the analog input channel being altered (1 based)
//  smpdispmax - the maximum digital value of the continuous display
//  spkdispmax - the maximum digital value of the spike display
//  lncdispmax - the maximum digital value of the LNC display
cbRESULT cbSetAnaInOutDisplay(uint32_t chan, int32_t smpdispmax, int32_t spkdispmax, int32_t lncdispmax, uint32_t nInstance = 0);

// Purpose: transfrom a digital value to an analog value
// Inputs:
//  nChan - the 1 based channel of interest
//  nDigital - the digital value
// Outputs:
//  the corresponding analog value
inline int32_t cbXfmDigToAna(uint32_t nChan, int32_t nDigital, uint32_t nInstance = 0)
{
    int32_t nVal = nDigital;
    cbSCALING isScaling;
    ::cbGetAinpScaling(nChan, &isScaling, nInstance);

    if (0 != isScaling.anagain)
        nVal = nDigital * (isScaling.anamax / isScaling.anagain) / isScaling.digmax;
    return nVal;
}

// Purpose: transform an analog value to a digital value
// Inputs:
//  nChan - the 1 based channel of interest
//  nAnalog - the analog value
// Outputs:
//  the corresponding digital value
inline int32_t cbXfmAnaToDig(uint32_t nChan, int32_t nAnalog, uint32_t nInstance = 0)
{
    cbSCALING isScaling;
    ::cbGetAinpScaling(nChan, &isScaling, nInstance);

    int32_t nVal = nAnalog * isScaling.digmax / (isScaling.anamax / isScaling.anagain);
    return nVal;
}

// Author & Date:   Hyrum L. Sessions   25 Jan 2006
// Purpose: Get the scaling of spike and analog channels
// Inputs:  nChan - the 1 based channel of interest
//          anSpikeScale - pointer to array to store spike scaling
//          anContScale - pointer to array to store continuous scaling
//          anLncScale   - pointer to array to store LNC scaling
cbRESULT cbGetScaling(uint32_t nChan,
                      uint32_t *anSpikeScale,
                      uint32_t *anContScale,
                      uint32_t *anLncScale,
                      uint32_t nInstance = 0);

// Purpose: tell which acquisition group this channel is part of
// Inputs:
//  nChan - the 1 based channel of interest
// Outpus:
//  0 means not part of any acquisition group; 1+ means part of that group
uint32_t cbGetAcqGroup(uint32_t nChan, uint32_t nInstance = 0);
cbRESULT cbSetAcqGroup(uint32_t nChan, uint32_t nGroup, uint32_t nInstance = 0);

// Author & Date:   Ehsan Azar     28 May 2009
// Purpose: tell which sampling group this channel is part of
// Inputs:
//  nChan - the 1 based channel of interest
// Outpus:
//  0 means SMPGRP_NONEuint32_t
uint32_t cbGetSmpGroup(uint32_t nChan, uint32_t nInstance = 0);

// Author & Date:   Ehsan Azar     28 May 2009
// Purpose: tell me how many sampling groups exist
//  this number will always be larger than 1 because group 0 (empty)
//  will always exist
uint32_t cbGetSmpGroupCount();

// Purpose: tell me how many acquisition groups exist
//  this number will always be larger than 1 because group 0 (empty)
//  will always exist
uint32_t cbGetAcqGroupCount();

// Purpose: get a textual description of the acquisition group
// Inputs:  nGroup - group in question
// Outputs: pointer to the description of the group
const char * cbGetAcqGroupDesc(uint32_t nGroup);

// Purpose: get the individual components (filter & sample group)
// Inputs:  nGroup - group in question
// Outputs: component value
uint32_t cbGetAcqGroupFilter(uint32_t nGroup);
uint32_t cbGetAcqGroupSampling(uint32_t nGroup);

// Purpose: get the number of units for this channel
// Inputs:  nChan - 1 based channel of interest
// Returns: number of valid units for this channel
uint32_t cbGetChanUnits(uint32_t nChan, uint32_t nInstance = 0);

// Purpose: tells if the unit in the channel is valid
// Inputs:  nChan - 1 based channel of interest
//          nUnit - 1 based unit of interest (0 is noise unit)
// Returns: 1 if the unit in the channel is valid, 0 is otherwise
uint32_t cbIsChanUnitValid(uint32_t nChan, int32_t nUnit, uint32_t nInstance = 0);

// Purpose: Set the noise boundary parameters (compatibility for int16_t)
cbRESULT cbSSSetNoiseBoundary(uint32_t chanIdx, int16_t anCentroid[3], int16_t anMajor[3], int16_t anMinor_1[3], int16_t anMinor_2[3], uint32_t nInstance = 0);

// Purpose: Get NTrode unitmapping for a particular site
cbRESULT cbGetNTrodeUnitMapping(uint32_t ntrode, uint16_t nSite, cbMANUALUNITMAPPING *unitmapping, uint32_t nInstance = 0);

// Purpose: Set NTrode unitmapping of a particular site
cbRESULT cbSetNTrodeUnitMapping(uint32_t ntrode, uint16_t nSite, cbMANUALUNITMAPPING *unitmapping, int16_t fs = -1, uint32_t nInstance = 0);

// Purpose: Set NTrode feature space, keeping the rest of NTrode information intact
cbRESULT cbSetNTrodeFeatureSpace(uint32_t ntrode, uint16_t fs, uint32_t nInstance = 0);

// Returns: returns if file is being recorded
bool IsFileRecording(uint32_t nInstance = 0);

#endif // include guards
