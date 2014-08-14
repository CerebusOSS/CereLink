// =STS=> cbHwlibHi.cpp[1688].aa22   submit   SMID:23 
//////////////////////////////////////////////////////////////////////////////////////////////////
//
// (c) Copyright 2003 - 2008 Cyberkinetics, Inc.
// (c) Copyright 2008 - 2012 Blackrock Microsystems
//
// $Workfile: cbHwlibHi.cpp $
// $Archive: /Cerebus/WindowsApps/cbhwlib/cbHwlibHi.cpp $
// $Revision: 13 $
// $Date: 4/05/04 11:29a $
// $Author: Kkorver $
//
// $NoKeywords: $
//
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"         // MFC core and standard components
#include "debugmacs.h"
#include "cbHwlibHi.h"
#include "math.h"
#include "cki_common.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#endif


///////////////////////////////////////////////////////////////////////////////////////////////////

// Author & Date:   Kirk Korver     30 Jan 2006
// Purpose: tell me if CentralSim is running, or is it regular central
// Outputs:
//  TRUE means CentralSIM is running; FALSE, Central
bool IsSimRunning(UINT32 nInstance)
{
#ifdef WIN32
    UINT32 nIdx = cb_library_index[nInstance];
    char szTitle[255] = "";
    ::GetWindowText(static_cast<HWND>(cb_cfg_buffer_ptr[nIdx]->hwndCentral), szTitle, sizeof(szTitle));
    if (strstr(szTitle, "SIM"))
        return true;
    else
        return false;
#else
    return (cbCheckApp("SIM") == cbRESULT_OK);
#endif
}


// TRUE means yes; FALSE, no
bool IsSpikeProcessingEnabled(UINT32 nChan, UINT32 nInstance)
{
    UINT32 dwFlags;
    if (cbRESULT_OK != ::cbGetAinpSpikeOptions(nChan, &dwFlags, NULL, nInstance))
        return false;

    return (dwFlags & cbAINPSPK_EXTRACT) ? true : false;
}



// TRUE means yes; FALSE, no
bool IsContinuousProcessingEnabled(UINT32 nChan, UINT32 nInstance)
{
    // In the case where the call fails, we will still return FALSE.
    UINT32 nGroup = 0;
    ::cbGetAinpSampling(nChan, NULL, &nGroup, nInstance);

    return nGroup == 0 ? false : true;
}

// TRUE means yes; FALSE, no
bool IsRawProcessingEnabled(UINT32 nChan, UINT32 nInstance)
{
    // In the case where the call fails, we will still return FALSE.
    UINT32 nainpopts = 0;
    ::cbGetAinpOpts( nChan, &nainpopts, NULL, NULL, nInstance);

    return ((nainpopts & cbAINP_RAWSTREAM_ENABLED) == 0 ? false : true);
}


// TRUE means yes; FALSE, no
bool IsChanSerial(UINT32 dwChan)
{
    if ((dwChan >= MIN_CHANS_SERIAL) && (dwChan <= MAX_CHANS_SERIAL))
        return true;

    return false;
}


// TRUE means yes; FALSE, no
bool IsChanDigin(UINT32 dwChan)
{
    if ((dwChan >= MIN_CHANS_DIGITAL_IN) && (dwChan <= MAX_CHANS_DIGITAL_IN))
        return true;

    return false;
}

bool IsChanDigout(UINT32 dwChan)
{
    if ((dwChan >= MIN_CHANS_DIGITAL_OUT) && (dwChan <= MAX_CHANS_DIGITAL_OUT))
        return true;

    return false;
}


// Author & Date:   Almut Branner   15 Jan 2004
// Purpose: Find out whether an analog in channel
// Input:   nChannel - the channel ID that we want to check
bool IsChanAnalogIn(UINT32 dwChan)
{
    if ((dwChan >= MIN_CHANS) && (dwChan <= MAX_CHANS_ANALOG_IN))
        return true;

    return false;
}


// Author & Date:   Almut Branner   15 Jan 2004
// Purpose: Find out whether a channel is a Front-End analog in channel
// Input:   nChannel - the channel ID that we want to check
bool IsChanFEAnalogIn(UINT32 dwChan)
{
    if ((dwChan >= MIN_CHANS) && (dwChan <= MAX_CHANS_FRONT_END))
        return true;

    return false;
}


// Author & Date:   Almut Branner   15 Jan 2004
// Purpose: Find out whether a channel is a Analog-In analog in channel
// Input:   nChannel - the channel ID that we want to check
bool IsChanAIAnalogIn(UINT32 dwChan)
{
    if ((dwChan >= MIN_CHANS_ANALOG_IN) && (dwChan <= MAX_CHANS_ANALOG_IN))
        return true;

    return false;
}


// Author & Date:   Almut Branner   21 Nov 2003
// Purpose: Find out whether a channel is continuously sampled
// Input:   nChannel - the channel ID that we want to check
bool IsChanCont(UINT32 dwChan, UINT32 nInstance)
{
    UINT32 nGroup = 0;

    ::cbGetAinpSampling(dwChan, NULL, &nGroup, nInstance);

    return (nGroup != 0);
}


// Author & Date:   Kirk Korver     29 Oct 2003
// Purpose: Tell me if this channel is enabled
// Inputs:
//  nChannel - the channel of interest (1 based)
// Outputs:
//  TRUE if the channel is enabled; FALSE, otherwise
bool IsChannelEnabled(UINT32 nChannel, UINT32 nInstance)
{
    ASSERT(nChannel >=1);
    ASSERT(nChannel <= MAX_CHANS_DIGITAL_OUT);

    if (nChannel <= MAX_CHANS_ANALOG_IN)
        return IsAnalogInEnabled(nChannel);

    if (nChannel <= MAX_CHANS_ANALOG_OUT)
        return IsAnalogOutEnabled(nChannel);

    if (nChannel <= MAX_CHANS_AUDIO)
        return IsAudioEnabled(nChannel);

    if (nChannel <= MAX_CHANS_DIGITAL_IN)
        return IsDigitalInEnabled(nChannel);

    if (nChannel <= MAX_CHANS_SERIAL)
        return IsSerialEnabled(nChannel, nInstance);

    if (nChannel <= MAX_CHANS_DIGITAL_OUT)
        return IsDigitalOutEnabled(nChannel, nInstance);

    return false;
}


// Author & Date:   Kirk Korver     29 Oct 2003
// Purpose: Tell me if this channel is enabled
// Inputs:
//  nChannel - the channel of interest (1 based)
// Outputs:
//  TRUE if the channel is enabled; FALSE, otherwise
bool IsAnalogInEnabled(UINT32 nChannel)
{
    TRACE("*** ::IsAnalogInEnabled *** not functional\n");
    return true;
}

// Author & Date:   Kirk Korver     29 Oct 2003
// Purpose: Tell me if this channel is enabled
// Inputs:
//  nChannel - the channel of interest (1 based)
// Outputs:
//  TRUE if the channel is enabled; FALSE, otherwise
bool IsAnalogOutEnabled(UINT32 nChannel)
{
    TRACE("*** ::IsAnalogOutEnabled *** not functional\n");
    return true;
}

// Author & Date:   Kirk Korver     29 Oct 2003
// Purpose: Tell me if this channel is enabled
// Inputs:
//  nChannel - the channel of interest (1 based)
// Outputs:
//  TRUE if the channel is enabled; FALSE, otherwise
bool IsAudioEnabled(UINT32 nChannel)
{
    TRACE("*** ::IsAudioEnabled *** not functional\n");
    return true;
}


// Author & Date:   Kirk Korver     29 Oct 2003
// Purpose: Tell me if this channel is enabled
// Inputs:
//  nChannel - the channel of interest (1 based)
// Outputs:
//  TRUE if the channel is enabled; FALSE, otherwise
bool IsDigitalInEnabled(UINT32 nChannel)
{
    TRACE("*** ::IsDigitalInEnabled *** not functional\n");
    return true;
}

// Author & Date:   Kirk Korver     29 Oct 2003
// Purpose: Tell me if this channel is enabled
// Inputs:
//  nChannel - the channel of interest (1 based)
// Outputs:
//  TRUE if the channel is enabled; FALSE, otherwise
bool IsSerialEnabled(UINT32 nChannel, UINT32 nInstance)
{
    UINT32 nOptions;
    ::cbGetDinpOptions(nChannel, &nOptions, NULL, nInstance);//get EOPchar

    if (nOptions & cbDINP_SERIALMASK)
        return true;
    else
        return false;
}

// Author & Date:   Kirk Korver     29 Oct 2003
// Purpose: Tell me if this channel is enabled
// Inputs:
//  nChannel - the channel of interest (1 based)
// Outputs:
//  TRUE if the channel is enabled; FALSE, otherwise
bool IsDigitalOutEnabled(UINT32 nChannel, UINT32 nInstance)
{
    UINT32 nOptions;
    cbGetDoutOptions(nChannel, &nOptions, NULL, NULL, nInstance);

    return (nOptions & cbDOUT_1BIT) ? true : false;
}

// Author & Date:   Almut Branner   6 Nov 2003
// Purpose: Tell me whether hoops are defined for a channel
// Inputs:  nChannel - the channel of interest
// Outputs: TRUE if hoops are defined, FALSE, otherwise
bool AreHoopsDefined(UINT32 nChannel, UINT32 nInstance)
{
    cbHOOP hoops[cbMAXUNITS][cbMAXHOOPS];
    cbGetAinpSpikeHoops(nChannel, &hoops[0][0], nInstance);

    for (UINT32 nUnit = 0; nUnit < cbMAXUNITS; ++nUnit)
        if (hoops[nUnit][0].valid)
            return true;

    return false;
}

// Author & Date:   Kirk Korver     24 Mar 2004
// Purpose: tell me if there are hoops for this channel and unit
// Inputs:
//  nChannel - the 1 based channel number
//  nUnit - the 0 based unit number
bool AreHoopsDefined(UINT32 nChannel, UINT32 nUnit, UINT32 nInstance)
{
    cbHOOP hoops[cbMAXUNITS][cbMAXHOOPS];
    cbGetAinpSpikeHoops(nChannel, &hoops[0][0], nInstance);

    if (hoops[nUnit][0].valid)
        return true;
    else
        return false;
}

// Author & Date:   Ehsan Azar   June 3, 2009
// Purpose: determine if a channel has  valid sorting unit
// Input:   nChannel = channel to track   1 - based
//          dwUnit the unit number (1=< dwUnit <=cbMAXUNITS)
bool cbHasValidUnit(UINT32 dwChan, UINT32 dwUnit, UINT32 nInstance)
{
    UINT32 nIdx = cb_library_index[nInstance];

    cbSPIKE_SORTING *pSortingOptions = &cb_cfg_buffer_ptr[nIdx]->isSortingOptions;
    if (dwUnit > cbMAXUNITS)
        return false;
    return (pSortingOptions->asSortModel[dwChan - 1][dwUnit].valid == TRUE);
}

// Author & Date:   Ehsan Azar   June 2, 2009
// Purpose: Return if given channel has a sorting algorithm
//       If cbAINPSPK_ALLSORT is passed it returns if there is any sorting
//       If cbAINPSPK_NOSORT is passed it returns if there is no sorting
// Input:   spkSrtOpt = Sorting methods:
//                      cbAINPSPK_HOOPSORT, cbAINPSPK_SPREADSORT,
//                      cbAINPSPK_CORRSORT, cbAINPSPK_PEAKMAJSORT,
//                      cbAINPSPK_PEAKFISHSORT, cbAINPSPK_PCAMANSORT,
//                      cbAINPSPK_NOSORT, cbAINPSPK_OLDAUTOSORT,
//                      cbAINPSPK_ALLSORT
bool cbHasSpikeSorting(UINT32 dwChan, UINT32 spkSrtOpt, UINT32 nInstance)
{
    UINT32 dwFlags;
    ::cbGetAinpSpikeOptions(dwChan, &dwFlags, NULL, nInstance);
    if (spkSrtOpt == cbAINPSPK_NOSORT)
        return ((dwFlags & cbAINPSPK_ALLSORT) == cbAINPSPK_NOSORT);
    else if (spkSrtOpt == cbAINPSPK_ALLSORT)
        return ((dwFlags & cbAINPSPK_ALLSORT) != cbAINPSPK_NOSORT);
    else
        return ((dwFlags & spkSrtOpt) != cbAINPSPK_NOSORT);
}

// Author & Date:   Ehsan Azar   June 2, 2009
// Purpose: Set a sorting algorithm for a channel
// Input:   spkSrtOpt = Sorting methods:
//                      cbAINPSPK_HOOPSORT, cbAINPSPK_SPREADSORT,
//                      cbAINPSPK_CORRSORT, cbAINPSPK_PEAKMAJSORT,
//                      cbAINPSPK_PEAKFISHSORT, cbAINPSPK_PCAMANSORT,
//                      cbAINPSPK_NOSORT
cbRESULT cbSetSpikeSorting(UINT32 dwChan, UINT32 spkSrtOpt, UINT32 nInstance)
{
    UINT32 dwFlags, dwFilter;
    ::cbGetAinpSpikeOptions(dwChan, &dwFlags, &dwFilter, nInstance);

    dwFlags &= ~(cbAINPSPK_ALLSORT) ; // Delete all sortings first
    dwFlags |= spkSrtOpt; // Add requested sorting only
    cbRESULT nRet = ::cbSetAinpSpikeOptions(dwChan, dwFlags, dwFilter, nInstance);
    return nRet;
}


// Author & Date:   Almut Branner   Dec 9, 2003
// Purpose: Set all output channels to track a particular channel
// Input:   nChannel = channel to track   1 - based
void TrackChannel(UINT32 nChannel, UINT32 nInstance)
{
    // Only do this if the channel is a analog input channel
    if ( (nChannel >= MIN_CHANS) && (nChannel <= MAX_CHANS_ANALOG_IN) )
    {
        // scan through the analog output channels
        // if channel exists and is set to track, update its source channel
        {for (int aoutch = MIN_CHANS_ANALOG_OUT; aoutch <= MAX_CHANS_AUDIO; ++aoutch)
        {
            UINT32 options, monchan, value;
            if (cbGetAoutOptions(aoutch, &options, &monchan, &value, nInstance)==cbRESULT_OK)
            {
                if (options & cbAOUT_TRACK)
                {
                    cbSetAoutOptions(aoutch, options, nChannel, value, nInstance);
                }
            }
        }}

        // Now scan through the Digital Out channels and set them
        for (int nChan = MIN_CHANS_DIGITAL_OUT; nChan <= MAX_CHANS_DIGITAL_OUT; ++nChan)
        {
            UINT32 nOptions, nVal;
            if (cbRESULT_OK == ::cbGetDoutOptions(nChan, &nOptions, NULL, &nVal, nInstance))
            {
                // Now if we are set to track, then change the monitored channel
                if (nOptions & cbDOUT_TRACK)
                {
                    ::cbSetDoutOptions(nChan, nOptions, nChannel, nVal, nInstance);
                }
            }
        }
    }
}


// Author & Date:   Kirk Korver     10 Feb 2004
// Purpose: update the analog input scaling. If there is an Analog Output
//  channel which is monitoring this channel, then update the output
//  scaling to match the displayed scaling
// Inputs:
//  chan - the analog input channel being altered (1 based)
//  smpdispmax - the maximum digital value of the continuous display
//  spkdispmax - the maximum digital value of the spike display
//  lncdispmax - the maximum digital value of the LNC display
cbRESULT cbSetAnaInOutDisplay(UINT32 chan, INT32 smpdispmax, INT32 spkdispmax, INT32 lncdispmax, UINT32 nInstance)
{
    cbRESULT retVal;

    // Start off by sending the new Analog In scaling
    retVal = ::cbSetAinpDisplay(chan, -smpdispmax, smpdispmax, spkdispmax, lncdispmax, nInstance);
    if (retVal != cbRESULT_OK)
        return retVal;


    ///// Now adjust any of the analog output monitoring /////

    // There are 6 analog channels of interest
    // They happen to be numbered so that AnalogOut and AudioOut are continuous
    for (UINT32 nAnaChan = MIN_CHANS_ANALOG_OUT; nAnaChan <= MAX_CHANS_AUDIO; ++nAnaChan)
    {
        UINT32 dwOptions = 0;
        UINT32 dwMonChan = 0;
        ::cbGetAoutOptions(nAnaChan, &dwOptions, &dwMonChan, NULL, nInstance);

        if (dwMonChan == chan)
        {
            // Get the input scaling
            cbSCALING isInScaling;
            retVal = ::cbGetAinpScaling(chan, &isInScaling, nInstance);
            if (retVal != cbRESULT_OK)
                return retVal;

            if (dwOptions & cbAOUT_MONITORSMP)      // Monitoring the continuous signal
            {
                INT32 nAnaMax = smpdispmax * isInScaling.anamax / isInScaling.digmax;
                cbSCALING isOutScaling;

                isOutScaling.digmin = -smpdispmax;
                isOutScaling.digmax =  smpdispmax;
                isOutScaling.anamin = -nAnaMax;
                isOutScaling.anamax =  nAnaMax;
                strncpy(isOutScaling.anaunit, isInScaling.anaunit, sizeof(isOutScaling.anaunit));
                retVal = ::cbSetAoutScaling(nAnaChan, &isOutScaling, nInstance);
                if (retVal != cbRESULT_OK)
                    return retVal;
            }
            else if (dwOptions & cbAOUT_MONITORSPK) // Monitoring the spikes
            {
                INT32 nAnaMax = spkdispmax * isInScaling.anamax / isInScaling.digmax;
                cbSCALING isOutScaling;

                isOutScaling.digmin = -spkdispmax;
                isOutScaling.digmax =  spkdispmax;
                isOutScaling.anamin = -nAnaMax;
                isOutScaling.anamax =  nAnaMax;
                strncpy(isOutScaling.anaunit, isInScaling.anaunit, sizeof(isOutScaling.anaunit));
                retVal = ::cbSetAoutScaling(nAnaChan, &isOutScaling, nInstance);
                if (retVal != cbRESULT_OK)
                    return retVal;
            }
        }
    }
    return retVal;
}

// Author & Date:   Hyrum L. Sessions   25 Jan 2006
// Purpose: Get the scaling of spike and analog channels
// Inputs:  nChan - the 1 based channel of interest
//          anSpikeScale - pointer to array to store spike scaling
//          anContScale  - pointer to array to store continuous scaling
//          anLncScale   - pointer to array to store LNC scaling
cbRESULT cbGetScaling(UINT32 nChan,
                      UINT32 *anSpikeScale,
                      UINT32 *anContScale,
                      UINT32 *anLncScale, 
                      UINT32 nInstance)
{
    cbRESULT retVal;
    cbSCALING isScaling;
    INT32 nAnaMax;
    int i;

    if (nChan < MIN_CHANS || nChan > MAX_CHANS_ANALOG_IN)
        return cbRESULT_INVALIDCHANNEL;

    retVal = ::cbGetAinpScaling(nChan, &isScaling, nInstance);
    if (retVal != cbRESULT_OK)
        return retVal;

    nAnaMax = isScaling.anamax / isScaling.anagain;

    if (nChan <= MAX_CHANS_FRONT_END)
    {

        if (anContScale)
        {
            for (i = 0; i < SCALE_CONTINUOUS_COUNT; ++i)
                anContScale[SCALE_CONTINUOUS_COUNT - i - 1] = UINT32(nAnaMax * exp(-0.3467 * i));
        }

        if (anLncScale)
        {
            for (i = 0; i < SCALE_LNC_COUNT; ++i)
                anLncScale[SCALE_LNC_COUNT - i - 1] = UINT32(nAnaMax * exp(-0.3467 * i));
        }

        if (anSpikeScale)
        {
            for (i = 0; i < SCALE_SPIKE_COUNT; ++i)
                anSpikeScale[SCALE_SPIKE_COUNT - i - 1] = UINT32(0.25 * nAnaMax * exp(-0.3467 / 2.0 * i));

            anSpikeScale[SCALE_SPIKE_COUNT - 1] = nAnaMax;
            anSpikeScale[SCALE_SPIKE_COUNT - 2] = nAnaMax / 2;
            anSpikeScale[SCALE_SPIKE_COUNT - 3] = nAnaMax / 4;
        }
    }
    else
    {
        if (anContScale)
            for (i = 0; i < SCALE_CONTINUOUS_COUNT; ++i)
                anContScale[SCALE_CONTINUOUS_COUNT - i - 1] = UINT32(nAnaMax * exp(-0.3467 * i));

        if (anLncScale)
            for (i = 0; i < SCALE_LNC_COUNT; ++i)
                anLncScale[SCALE_LNC_COUNT - i - 1] = UINT32(nAnaMax * exp(-0.3467 * i));

        if (anSpikeScale)
            for (i = 0; i < SCALE_SPIKE_COUNT; ++i)
                anSpikeScale[SCALE_SPIKE_COUNT - i - 1] = UINT32(nAnaMax * exp(-0.3467 * i / 2.0));

    }
    return cbRESULT_OK;
}


////////////////////////////// acquisition groups ////////////////////////////////////////////////

// These will be ordered by group so I don't need that
struct cbAcqSettings
{
    UINT32 nFilter;         // Which filter
    UINT32 nSampleGroup;    // Which sample group
};
const cbAcqSettings isAcqData[ACQ_GROUP_COUNT] =
{
    // Filter (Low pass)                           Sample Group
    {6,  /* don't care (was 1)*/                    0 /* not sampling */},  // match "startup"
    {8,  /* 150 Hz Low (was 5)*/                    2 /* 1 kS/sec */    },
    {6,  /* 250 Hz Low (was 1)*/                    2 /* 1 kS/sec */    },
    {9,  /* 10-250 Hz Band (was 4)*/                2 /* 1 kS/sec */    },
    {7,  /* 500 Hz Low (was 2)*/                    3 /* 2 kS/sec */    },
    {10, /* 2.5 kHz Low (was 6)*/                   4 /* 10 kS/sec */   },
    {0,  /* NONE - HW only (7.5 kHz) */             5 /* 30 kS/sec */   },
    {2,  /* 250 Hz High (was 3)*/                   5 /* 30 kS/sec */   },
    {8,                                             5                   },
};



// Author & Date:   Ehsan Azar     28 May 2009
// Purpose: tell which sampling group this channel is part of
// Inputs:
//  nChan - the 1 based channel of interest
// Outpus:
//  0 means SMPGRP_NONE
UINT32 cbGetSmpGroup(UINT32 nChan, UINT32 nInstance)
{
        UINT32 dwGroup;
        ::cbGetAinpSampling(nChan, NULL, &dwGroup, nInstance);
        return dwGroup;
}

// Author & Date:   Ehsan Azar     28 May 2009
// Purpose: tell me how many sampling groups exist
//  this number will always be larger than 1 because group 0 (empty)
//  will always exist
UINT32 cbGetSmpGroupCount()
{
    return SMP_GROUP_COUNT;
}


// Author & Date:   Kirk Korver     02 Nov 2005
// Purpose: tell which acquisition group this channel is part of
//  See SetAcqGroup
// Inputs:
//  nChan - the 1 based channel of interest
// Outpus:
//  0 means not part of any acquisition group; 1+ means part of that group
// Note: Do not use this function to find the sampling rate,
// use cbGetSmpGroup instead.
UINT32 cbGetAcqGroup(UINT32 nChan, UINT32 nInstance)
{
    UINT32 nFilter = 0;
    UINT32 nSG = 0;
    ::cbGetAinpSampling(nChan, &nFilter, &nSG, nInstance);

    for (const cbAcqSettings * pTest = isAcqData; pTest != ARRAY_END(isAcqData); ++pTest)
    {
        if (pTest->nFilter == nFilter &&
            pTest->nSampleGroup == nSG)
        {
            return UINT32(pTest - &isAcqData[0]);
        }
    }

    // If I got here then then was no match. I had better make it "not sample"
    // and return that.
    cbSetAcqGroup(nChan, 0, nInstance);
    return 0;
}

// Author & Date:   Kirk Korver     02 Nov 2005
// Purpose: tell which acquisition group this channel is part of
//  See SetAcqGroup
// Inputs:
//  nChan - the 1 based channel of interest
//  nGroup - the group to now belong to: 0 = not in a grup; 1+, a member of that group
// Outpus:
//  0 means not part of any acquisition group; 1+ means part of that group
cbRESULT cbSetAcqGroup(UINT32 nChan, UINT32 nGroup, UINT32 nInstance)
{
    if (nGroup >= ACQ_GROUP_COUNT)
        return cbRESULT_INVALIDFUNCTION;

    UINT32 nFilter = isAcqData[nGroup].nFilter;
    UINT32 nSG = isAcqData[nGroup].nSampleGroup;
    cbRESULT nRet = ::cbSetAinpSampling(nChan, nFilter, nSG, nInstance);
    return nRet;
}


// Purpose: tell me how many acquisition groups exist
//  this number will always be larger than 1 because group 0 (empty)
//  will always exist
UINT32 cbGetAcqGroupCount()
{
    return ACQ_GROUP_COUNT;
}

const char * asContFilter[ACQ_GROUP_COUNT] =
{
    "",
    "0.3 Hz - 150 Hz   -   1 kS/s",
    "0.3 Hz - 250 Hz   -   1 kS/s",
    "10 Hz - 250 Hz    -   1 kS/s",
    "0.3 Hz - 500 Hz   -   2 kS/s",
    "0.3 Hz - 2.5 kHz  -  10 kS/s",
    "0.3 Hz - 7.5 kHz  -  30 kS/s",
    "250 Hz - 7.5 kHz  -  30 kS/s",
    "0.3 Hz - 150 Hz   -  30 kS/s",
};


// Author & Date:   Hyrum L. Sessions   25 May 2007
// Purpose: get a textual description of the acquisition group
// Inputs:  nGroup - group in question
// Outputs: pointer to the description of the group
const char * cbGetAcqGroupDesc(UINT32 nGroup)
{
    return asContFilter[nGroup < ACQ_GROUP_COUNT ? nGroup : 0];
}

// Author & Date:   Hyrum L. Sessions   29 May 2007
// Purpose: get the filter used by an acquision group
// Inputs:  nGroup - group in question
// Outputs: filter value
UINT32 cbGetAcqGroupFilter(UINT32 nGroup)
{
    return isAcqData[nGroup < ACQ_GROUP_COUNT ? nGroup : 0].nFilter;
}

// Author & Date:   Hyrum L. Sessions   29 May 2007
// Purpose: get the sample group used by an acquision group
// Inputs:  nGroup - group in question
// Outputs: sample group value
UINT32 cbGetAcqGroupSampling(UINT32 nGroup)
{
    return isAcqData[nGroup < ACQ_GROUP_COUNT ? nGroup : 0].nSampleGroup;
}

// Purpose: get the number of units for this channel
// Inputs:  nChan - 1 based channel of interest
// Returns: number of valid units for this channel
UINT32 cbGetChanUnits(UINT32 nChan, UINT32 nInstance)
{
    UINT32 nIdx = cb_library_index[nInstance];

    ASSERT(nChan >= MIN_CHANS);
    ASSERT(nChan <= cbMAXCHANS);

    UINT32 nValidUnits = 0;

    // let's start at 1 so we don't get the noise unit which is 0
    for (UINT32 i = 1; i < ARRAY_SIZE(cb_cfg_buffer_ptr[nIdx]->isSortingOptions.asSortModel[nChan]); ++i)
    {
        if (cb_cfg_buffer_ptr[nIdx]->isSortingOptions.asSortModel[nChan - 1][i].valid)
            ++nValidUnits;
    }
    return nValidUnits;
}

// Purpose: tells if the unit in the channel is valid
// Inputs:  nChan - 1 based channel of interest
//          nUnit - 1 based unit of interest (0 is noise unit)
// Returns: 1 if the unit in the channel is valid, 0 is otherwise
UINT32 cbIsChanUnitValid(UINT32 nChan, INT32 nUnit, UINT32 nInstance)
{
    UINT32 nIdx = cb_library_index[nInstance];

    BOOL bValid = false;
    cbMANUALUNITMAPPING isUnitMapping[cbMAXUNITS];

    ASSERT(nChan >= MIN_CHANS);
    ASSERT(nChan <= cbMAXCHANS);
    ASSERT(nUnit <= cbMAXUNITS);

    // get the override array for the requested channel
    cbGetChanUnitMapping(nChan, &isUnitMapping[0], nInstance);

    // find a translated unit that points to this unit
    for (int i = 0; i < cbMAXUNITS; ++i)
    {
        if (nUnit == isUnitMapping[i].nOverride + 1)     // zero based
            if ((cb_cfg_buffer_ptr[nIdx]->isSortingOptions.asSortModel[nChan - 1][isUnitMapping[i].nOverride + 1].valid) ||
                (isUnitMapping[i].bValid))
            {
                bValid = true;
                break;
            }
    }
    return bValid;
}

// Author & Date:   Ehsan Azar     18 Nov 2010
// Purpose: Set the noise boundary parameters (compatibility for INT16)
// Inputs:
//  chanIdx  - channel number (1-based)
//  anCentroid - the center of an ellipsoid
//  anMajor - major axis of the ellipsoid
//  anMinor_1 - first minor axis of the ellipsoid
//  anMinor_2 - second minor axis of the ellipsoid
// Outputs:
//  cbRESULT_OK if life is good
cbRESULT cbSSSetNoiseBoundary(UINT32 chanIdx, INT16 anCentroid[3], INT16 anMajor[3], INT16 anMinor_1[3], INT16 anMinor_2[3], UINT32 nInstance)
{
    float afCentroid[3], afMajor[3], afMinor_1[3], afMinor_2[3];
    for (int i = 0; i < 3; ++i) {
        afCentroid[i] = anCentroid[i];
        afMajor[i] = anMajor[i];
        afMinor_1[i] = anMinor_1[i];
        afMinor_2[i] = anMinor_2[i];
    }
    return cbSSSetNoiseBoundary(chanIdx, afCentroid, afMajor, afMinor_1, afMinor_2, nInstance);
}

// Author & Date:   Ehsan Azar     19 Nov 2010
// Purpose: Get NTrode unitmapping for a particular site
// Inputs:
//  ntrode  - NTrode number (1-based)
//  nSite   - the site within NTrode (1 to cbMAXSITEPLOTS)
// Outputs:
//  unitmapping - the unitmapping for the given site in given NTrode
//  cbRESULT_OK if life is good
cbRESULT cbGetNTrodeUnitMapping(UINT32 ntrode, UINT16 nSite, cbMANUALUNITMAPPING *unitmapping, UINT32 nInstance)
{
    UINT32 nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Test that the NTrode number is valid and initialized
    if ((ntrode - 1) >= cbMAXNTRODES) return cbRESULT_INVALIDNTRODE;
    if (nSite >= cbMAXSITEPLOTS) return cbRESULT_INVALIDFUNCTION;
    if (cb_cfg_buffer_ptr[nIdx]->isNTrodeInfo[ntrode - 1].chid==0) return cbRESULT_INVALIDNTRODE;

    // Return the requested data from the rec buffer
    if (unitmapping) memcpy(unitmapping,&cb_cfg_buffer_ptr[nIdx]->isNTrodeInfo[ntrode - 1].ellipses[nSite][0],cbMAXUNITS*sizeof(cbMANUALUNITMAPPING));
    return cbRESULT_OK;
}

// Author & Date:   Ehsan Azar     19 Nov 2010
// Purpose: Set NTrode unitmapping of a particular site
// Inputs:
//  ntrode  - NTrode number (1-based)
//  nSite   - the site within NTrode (1 to cbMAXSITEPLOTS)
//  unitmapping - the unitmapping for the given site in given NTrode
//  fs      - if valid, set as new NTrode feature space otherwise leave unchanged
// Outputs:
//  cbRESULT_OK if life is good
cbRESULT cbSetNTrodeUnitMapping(UINT32 ntrode, UINT16 nSite, cbMANUALUNITMAPPING *unitmapping, INT16 fs, UINT32 nInstance)
{
    UINT32 nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Test that the NTrode number is valid and initialized
    if ((ntrode - 1) >= cbMAXNTRODES) return cbRESULT_INVALIDNTRODE;
    if (cb_cfg_buffer_ptr[nIdx]->isNTrodeInfo[ntrode - 1].chid == 0) return cbRESULT_INVALIDNTRODE;
    if (nSite >= cbMAXSITEPLOTS) return cbRESULT_INVALIDFUNCTION;
    if (fs < 0)
        fs = cb_cfg_buffer_ptr[nIdx]->isNTrodeInfo[ntrode - 1].fs; // previous feature space
    cbMANUALUNITMAPPING ellipses[cbMAXSITEPLOTS][cbMAXUNITS]; // for the mapping structure called in from the NSP
    char label[cbLEN_STR_LABEL];
    // Get previous ellipses
    cbGetNTrodeInfo(ntrode, label, ellipses, NULL, NULL, NULL, nInstance);

    if (unitmapping) memcpy(&ellipses[nSite][0], unitmapping, cbMAXUNITS * sizeof(cbMANUALUNITMAPPING));
    cbSetNTrodeInfo(ntrode, label, ellipses, fs, nInstance);
    return cbRESULT_OK;
}

// Author & Date:   Ehsan Azar     19 Nov 2010
// Purpose: Set NTrode feature space if changed, keeping the rest of NTrode information intact
// Inputs:
//  ntrode  - NTrode number (1-based)
//  fs      - set as new NTrode feature space
// Outputs:
//  cbRESULT_OK if life is good
cbRESULT cbSetNTrodeFeatureSpace(UINT32 ntrode, UINT16 fs, UINT32 nInstance)
{
    UINT32 nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Test that the NTrode number is valid and initialized
    if ((ntrode - 1) >= cbMAXNTRODES) return cbRESULT_INVALIDNTRODE;
    if (cb_cfg_buffer_ptr[nIdx]->isNTrodeInfo[ntrode - 1].chid == 0) return cbRESULT_INVALIDNTRODE;
    // If feature space has changed
    if (fs != cb_cfg_buffer_ptr[nIdx]->isNTrodeInfo[ntrode - 1].fs)
        return cbSetNTrodeUnitMapping(ntrode, 0, NULL, fs, nInstance);
    return cbRESULT_OK;
}

// Author & Date:   Ehsan Azar     6 Nov 2012
// Purpose: Find if file recording is active
// Outputs:
//  returns if file is being recorded
bool IsFileRecording(UINT32 nInstance)
{
    cbPKT_FILECFG filecfg;
    if (cbGetFileInfo(&filecfg, nInstance) == cbRESULT_OK)
    {
        if (filecfg.options == cbFILECFG_OPT_REC)
            return true;
    }
    return false;
}
