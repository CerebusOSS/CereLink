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

bool IsSimRunning(UINT32 nInstance = 0);    // TRUE means that CentralSim is being used; FALSE, not

bool IsSpikeProcessingEnabled(UINT32 nChan, UINT32 nInstance = 0);    // TRUE means yes; FALSE, no
bool IsContinuousProcessingEnabled(UINT32 nChan, UINT32 nInstance = 0); // TRUE means yes; FALSE, no
bool IsRawProcessingEnabled(UINT32 nChan, UINT32 nInstance = 0);		// true means yes
// Is this channel enabled?
// This will figure out what kind of channel, and then find out if it is enabled
bool IsChannelEnabled(UINT32 nChannel, UINT32 nInstance = 0);

// Use these with care. make sure you know what kind of channel you have
bool IsAnalogInEnabled(UINT32 nChannel);
bool IsAudioEnabled(UINT32 nChannel);
bool IsAnalogOutEnabled(UINT32 nChannel);
bool IsDigitalInEnabled(UINT32 nChannel);
bool IsSerialEnabled(UINT32 nChannel, UINT32 nInstance = 0);
bool IsDigitalOutEnabled(UINT32 nChannel, UINT32 nInstance = 0);

// Is it "this" kind of channel? (very fast)
bool IsChanAnalogIn(UINT32 dwChan);             // TRUE means yes; FALSE, no
bool IsChanFEAnalogIn(UINT32 dwChan);           // TRUE means yes; FALSE, no
bool IsChanAIAnalogIn(UINT32 dwChan);           // TRUE means yes; FALSE, no
bool IsChanSerial(UINT32 dwChan);               // TRUE means yes; FALSE, no
bool IsChanDigin(UINT32 dwChan);                // TRUE means yes; FALSE, no
bool IsChanDigout(UINT32 dwChan);               // TRUE means yes; FALSE, no
bool IsChanCont(UINT32 dwChan, UINT32 nInstance = 0);                 // TRUE means yes; FALSE, no

bool AreHoopsDefined(UINT32 nChannel, UINT32 nInstance = 0);
bool AreHoopsDefined(UINT32 nChannel, UINT32 nUnit, UINT32 nInstance = 0);


// Author & Date:   Ehsan Azar   June 3, 2009
// Purpose: determine if a channel has  valid sorting unit
// Input:   nChannel = channel to track   1 - based
//          dwUnit the unit number (1=< dwUnit <=cbMAXUNITS)
bool cbHasValidUnit(UINT32 dwChan, UINT32 dwUnit, UINT32 nInstance = 0);                 // TRUE means yes; FALSE, no

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
bool cbHasSpikeSorting(UINT32 dwChan, UINT32 spkSrtOpt, UINT32 nInstance = 0);

// Author & Date:   Ehsan Azar   June 2, 2009
// Purpose: Set a sorting algorithm for a channel
// Input:   spkSrtOpt = Sorting methods:
//                      cbAINPSPK_HOOPSORT, cbAINPSPK_SPREADSORT,
//                      cbAINPSPK_CORRSORT, cbAINPSPK_PEAKMAJSORT,
//                      cbAINPSPK_PEAKFISHSORT, cbAINPSPK_PCAMANSORT,
//                      cbAINPSPK_PCAKMEANSORT, cbAINPSPK_PCAEMSORT,
//                      cbAINPSPK_PCADBSORT, cbAINPSPK_NOSORT
cbRESULT cbSetSpikeSorting(UINT32 dwChan, UINT32 spkSrtOpt, UINT32 nInstance = 0);


// Other functions
void TrackChannel(UINT32 nChannel, UINT32 nInstance = 0);


// Purpose: update the analog input scaling. If there is an Analog Output
//  channel which is monitoring this channel, then update the output
//  scaling to match the displayed scaling
// Inputs:
//  chan - the analog input channel being altered (1 based)
//  smpdispmax - the maximum digital value of the continuous display
//  spkdispmax - the maximum digital value of the spike display
//  lncdispmax - the maximum digital value of the LNC display
cbRESULT cbSetAnaInOutDisplay(UINT32 chan, INT32 smpdispmax, INT32 spkdispmax, INT32 lncdispmax, UINT32 nInstance = 0);

// Purpose: transfrom a digital value to an analog value
// Inputs:
//  nChan - the 1 based channel of interest
//  nDigital - the digital value
// Outputs:
//  the corresponding analog value
inline INT32 cbXfmDigToAna(UINT32 nChan, INT32 nDigital, UINT32 nInstance = 0)
{
    INT32 nVal = nDigital;
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
inline INT32 cbXfmAnaToDig(UINT32 nChan, INT32 nAnalog, UINT32 nInstance = 0)
{
    cbSCALING isScaling;
    ::cbGetAinpScaling(nChan, &isScaling, nInstance);

    INT32 nVal = nAnalog * isScaling.digmax / (isScaling.anamax / isScaling.anagain);
    return nVal;
}

// Author & Date:   Hyrum L. Sessions   25 Jan 2006
// Purpose: Get the scaling of spike and analog channels
// Inputs:  nChan - the 1 based channel of interest
//          anSpikeScale - pointer to array to store spike scaling
//          anContScale - pointer to array to store continuous scaling
//          anLncScale   - pointer to array to store LNC scaling
cbRESULT cbGetScaling(UINT32 nChan,
                      UINT32 *anSpikeScale,
                      UINT32 *anContScale,
                      UINT32 *anLncScale,
                      UINT32 nInstance = 0);

// Purpose: tell which acquisition group this channel is part of
// Inputs:
//  nChan - the 1 based channel of interest
// Outpus:
//  0 means not part of any acquisition group; 1+ means part of that group
UINT32 cbGetAcqGroup(UINT32 nChan, UINT32 nInstance = 0);
cbRESULT cbSetAcqGroup(UINT32 nChan, UINT32 nGroup, UINT32 nInstance = 0);

// Author & Date:   Ehsan Azar     28 May 2009
// Purpose: tell which sampling group this channel is part of
// Inputs:
//  nChan - the 1 based channel of interest
// Outpus:
//  0 means SMPGRP_NONEUINT32
UINT32 cbGetSmpGroup(UINT32 nChan, UINT32 nInstance = 0);

// Author & Date:   Ehsan Azar     28 May 2009
// Purpose: tell me how many sampling groups exist
//  this number will always be larger than 1 because group 0 (empty)
//  will always exist
UINT32 cbGetSmpGroupCount();

// Purpose: tell me how many acquisition groups exist
//  this number will always be larger than 1 because group 0 (empty)
//  will always exist
UINT32 cbGetAcqGroupCount();

// Purpose: get a textual description of the acquisition group
// Inputs:  nGroup - group in question
// Outputs: pointer to the description of the group
const char * cbGetAcqGroupDesc(UINT32 nGroup);

// Purpose: get the individual components (filter & sample group)
// Inputs:  nGroup - group in question
// Outputs: component value
UINT32 cbGetAcqGroupFilter(UINT32 nGroup);
UINT32 cbGetAcqGroupSampling(UINT32 nGroup);

// Purpose: get the number of units for this channel
// Inputs:  nChan - 1 based channel of interest
// Returns: number of valid units for this channel
UINT32 cbGetChanUnits(UINT32 nChan, UINT32 nInstance = 0);

// Purpose: tells if the unit in the channel is valid
// Inputs:  nChan - 1 based channel of interest
//          nUnit - 1 based unit of interest (0 is noise unit)
// Returns: 1 if the unit in the channel is valid, 0 is otherwise
UINT32 cbIsChanUnitValid(UINT32 nChan, INT32 nUnit, UINT32 nInstance = 0);

// Purpose: Set the noise boundary parameters (compatibility for INT16)
cbRESULT cbSSSetNoiseBoundary(UINT32 chanIdx, INT16 anCentroid[3], INT16 anMajor[3], INT16 anMinor_1[3], INT16 anMinor_2[3], UINT32 nInstance = 0);

// Purpose: Get NTrode unitmapping for a particular site
cbRESULT cbGetNTrodeUnitMapping(UINT32 ntrode, UINT16 nSite, cbMANUALUNITMAPPING *unitmapping, UINT32 nInstance = 0);

// Purpose: Set NTrode unitmapping of a particular site
cbRESULT cbSetNTrodeUnitMapping(UINT32 ntrode, UINT16 nSite, cbMANUALUNITMAPPING *unitmapping, INT16 fs = -1, UINT32 nInstance = 0);

// Purpose: Set NTrode feature space, keeping the rest of NTrode information intact
cbRESULT cbSetNTrodeFeatureSpace(UINT32 ntrode, UINT16 fs, UINT32 nInstance = 0);

// Returns: returns if file is being recorded
bool IsFileRecording(UINT32 nInstance = 0);

#endif // include guards
