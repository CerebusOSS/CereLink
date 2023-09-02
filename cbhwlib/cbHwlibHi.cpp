// =STS=> cbHwlibHi.cpp[1688].aa22   submit   SMID:23 
//////////////////////////////////////////////////////////////////////////////////////////////////
//
// (c) Copyright 2003 - 2008 Cyberkinetics, Inc.
// (c) Copyright 2008 - 2021 Blackrock Microsystems
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
bool IsSimRunning(uint32_t nInstance)
{
#ifdef WIN32
    uint32_t nIdx = cb_library_index[nInstance];
    char szTitle[255] = "";
    ::GetWindowTextA(static_cast<HWND>(cb_cfg_buffer_ptr[nIdx]->hwndCentral), szTitle, sizeof(szTitle));
    if (strstr(szTitle, "SIM"))
        return true;
    else
        return false;
#else
    return (cbCheckApp("SIM") == cbRESULT_OK);
#endif
}


// TRUE means yes; FALSE, no
bool IsSpikeProcessingEnabled(uint32_t nChan, uint32_t nInstance)
{
    uint32_t dwFlags;
    if (cbRESULT_OK != ::cbGetAinpSpikeOptions(nChan, &dwFlags, NULL, nInstance))
        return false;

    return (dwFlags & cbAINPSPK_EXTRACT) ? true : false;
}



// TRUE means yes; FALSE, no
bool IsContinuousProcessingEnabled(uint32_t nChan, uint32_t nInstance)
{
    // In the case where the call fails, we will still return FALSE.
    uint32_t nGroup = 0;
    ::cbGetAinpSampling(nChan, NULL, &nGroup, nInstance);

    return nGroup == 0 ? false : true;
}

// TRUE means yes; FALSE, no
bool IsRawProcessingEnabled(uint32_t nChan, uint32_t nInstance)
{
    // In the case where the call fails, we will still return FALSE.
    uint32_t nainpopts = 0;
    ::cbGetAinpOpts( nChan, &nainpopts, NULL, NULL, nInstance);

    return ((nainpopts & cbAINP_RAWSTREAM_ENABLED) == 0 ? false : true);
}


// TRUE means yes; FALSE, no
bool IsChanAnyDigIn(uint32_t dwChan, uint32_t nInstance)
{
    uint32_t dwChanCaps;
    bool result;

    result = dwChan >= MIN_CHANS;
    result &= cbGetChanCaps(dwChan, &dwChanCaps, nInstance) == cbRESULT_OK;
    result &= (dwChanCaps & cbCHAN_DINP) == cbCHAN_DINP;
    return result;
}


// TRUE means yes; FALSE, no
bool IsChanSerial(uint32_t dwChan, uint32_t nInstance)
{
    uint32_t dwDiginCaps;
    bool result = IsChanAnyDigIn(dwChan, nInstance);
    result &= cbGetDinpCaps(dwChan, &dwDiginCaps, nInstance) == cbRESULT_OK;
    result &= (dwDiginCaps & cbDINP_SERIALMASK) == cbDINP_SERIALMASK;
    return result;
}


// TRUE means yes; FALSE, no
bool IsChanDigin(uint32_t dwChan, uint32_t nInstance)
{
    uint32_t dwDiginCaps;
    bool result = IsChanAnyDigIn(dwChan, nInstance);
    result &= cbGetDinpCaps(dwChan, &dwDiginCaps, nInstance) == cbRESULT_OK;
    result &= (dwDiginCaps & cbDINP_SERIALMASK) != cbDINP_SERIALMASK;
    return result;
}

bool IsChanDigout(uint32_t dwChan, uint32_t nInstance)
{
    uint32_t dwChanCaps;
    bool result;

    result = dwChan >= MIN_CHANS;
    result &= cbGetChanCaps(dwChan, &dwChanCaps, nInstance) == cbRESULT_OK;
    result &= (dwChanCaps & cbCHAN_DOUT) == cbCHAN_DOUT;
    return result;
}


// Author & Date:   Almut Branner   15 Jan 2004
// Purpose: Find out whether an analog in channel
// Input:   nChannel - the channel ID that we want to check
bool IsChanAnalogIn(uint32_t dwChan, uint32_t nInstance)
{
    uint32_t dwChanCaps;
    bool result;

    result = dwChan >= MIN_CHANS;
    result &= cbGetChanCaps(dwChan, &dwChanCaps, nInstance) == cbRESULT_OK;
    result &= (dwChanCaps & cbCHAN_AINP) == cbCHAN_AINP;
    return result;
}


// Author & Date:   Almut Branner   15 Jan 2004
// Purpose: Find out whether a channel is a Front-End analog in channel
// Input:   nChannel - the channel ID that we want to check
bool IsChanFEAnalogIn(uint32_t dwChan, uint32_t nInstance)
{
    uint32_t dwChanCaps;

    return ((dwChan >= MIN_CHANS) &&
        (cbGetChanCaps(dwChan, &dwChanCaps, nInstance) == cbRESULT_OK) &&
        ((cbCHAN_AINP | cbCHAN_ISOLATED) == (dwChanCaps & (cbCHAN_AINP | cbCHAN_ISOLATED))));
}


// Author & Date:   Almut Branner   15 Jan 2004
// Purpose: Find out whether a channel is a Analog-In analog in channel
// Input:   nChannel - the channel ID that we want to check
bool IsChanAIAnalogIn(uint32_t dwChan, uint32_t nInstance)
{
    uint32_t dwChanCaps;

    return ((dwChan >= MIN_CHANS) &&
        (cbGetChanCaps(dwChan, &dwChanCaps, nInstance) == cbRESULT_OK) &&
        (dwChanCaps & cbCHAN_AINP) &&
        !(dwChanCaps & cbCHAN_ISOLATED));
}


/// @author Hyrum L. Sessions
/// @date   14 Feb 2017
/// @brief  Find out whether a channel is an Analog Out channel, don't include audio out
// Input:   nChannel - the channel ID that we want to check
bool IsChanAnalogOut(uint32_t dwChan, uint32_t nInstance)
{
    uint32_t dwChanCaps;
    uint32_t dwAnaOutCaps;

    return ((dwChan >= MIN_CHANS) &&
        (cbGetChanCaps(dwChan, &dwChanCaps, nInstance) == cbRESULT_OK) &&
        (cbGetAoutCaps(dwChan, &dwAnaOutCaps, NULL, NULL, nInstance) == cbRESULT_OK) &&
        (dwChanCaps & cbCHAN_AOUT) &&
        ((dwAnaOutCaps & cbAOUT_AUDIO) == 0));
}


/// @author Hyrum L. Sessions
/// @date   20 Feb 2017
/// @brief  Find out whether a channel is an Audio Out channel
// Input:   nChannel - the channel ID that we want to check
bool IsChanAudioOut(uint32_t dwChan, uint32_t nInstance)
{
    uint32_t dwChanCaps;
    uint32_t dwAnaOutCaps;

    return ((dwChan >= MIN_CHANS) &&
        (cbGetChanCaps(dwChan, &dwChanCaps, nInstance) == cbRESULT_OK) &&
        (cbGetAoutCaps(dwChan, &dwAnaOutCaps, NULL, NULL, nInstance) == cbRESULT_OK) &&
        (dwChanCaps & cbCHAN_AOUT) &&
        ((dwAnaOutCaps & cbAOUT_AUDIO) == cbAOUT_AUDIO));
}


// Author & Date:   Almut Branner   21 Nov 2003
// Purpose: Find out whether a channel is continuously sampled
// Input:   nChannel - the channel ID that we want to check
bool IsChanCont(uint32_t dwChan, uint32_t nInstance)
{
    uint32_t nGroup = 0;

    ::cbGetAinpSampling(dwChan, NULL, &nGroup, nInstance);

    return (nGroup != 0);
}


// Author & Date:   Kirk Korver     29 Oct 2003
// Purpose: Tell me if this channel is enabled
// Inputs:
//  nChannel - the channel of interest (1 based)
// Outputs:
//  TRUE if the channel is enabled; FALSE, otherwise
bool IsChannelEnabled(uint32_t nChannel, uint32_t nInstance)
{
    ASSERT(nChannel >=1);
    ASSERT(nChannel <= MAX_CHANS_DIGITAL_OUT);

    if (IsChanAnalogIn(nChannel))
        return IsAnalogInEnabled(nChannel);

    if (IsChanAnalogOut(nChannel))
        return IsAnalogOutEnabled(nChannel);

    if (!IsChanDigout(nChannel))
        return IsAudioEnabled(nChannel);

    if (IsChanDigin(nChannel))
        return IsDigitalInEnabled(nChannel);

    if (IsChanSerial(nChannel))
        return IsSerialEnabled(nChannel, nInstance);

    if (IsChanDigout(nChannel))
        return IsDigitalOutEnabled(nChannel, nInstance);

    return false;
}


// Author & Date:   Kirk Korver     29 Oct 2003
// Purpose: Tell me if this channel is enabled
// Inputs:
//  nChannel - the channel of interest (1 based)
// Outputs:
//  TRUE if the channel is enabled; FALSE, otherwise
bool IsAnalogInEnabled(uint32_t nChannel)
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
bool IsAnalogOutEnabled(uint32_t nChannel)
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
bool IsAudioEnabled(uint32_t nChannel)
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
bool IsDigitalInEnabled(uint32_t nChannel)
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
bool IsSerialEnabled(uint32_t nChannel, uint32_t nInstance)
{
    uint32_t nOptions;
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
bool IsDigitalOutEnabled(uint32_t nChannel, uint32_t nInstance)
{
    uint32_t nOptions;
    cbGetDoutOptions(nChannel, &nOptions, NULL, NULL, NULL, NULL, NULL, nInstance);

    return (nOptions & cbDOUT_1BIT) ? true : false;
}

// Author & Date:   Almut Branner   6 Nov 2003
// Purpose: Tell me whether hoops are defined for a channel
// Inputs:  nChannel - the channel of interest
// Outputs: TRUE if hoops are defined, FALSE, otherwise
bool AreHoopsDefined(uint32_t nChannel, uint32_t nInstance)
{
    cbHOOP hoops[cbMAXUNITS][cbMAXHOOPS];
    cbGetAinpSpikeHoops(nChannel, &hoops[0][0], nInstance);

    for (uint32_t nUnit = 0; nUnit < cbMAXUNITS; ++nUnit)
        if (hoops[nUnit][0].valid)
            return true;

    return false;
}

// Author & Date:   Kirk Korver     24 Mar 2004
// Purpose: tell me if there are hoops for this channel and unit
// Inputs:
//  nChannel - the 1 based channel number
//  nUnit - the 0 based unit number
bool AreHoopsDefined(uint32_t nChannel, uint32_t nUnit, uint32_t nInstance)
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
bool cbHasValidUnit(uint32_t dwChan, uint32_t dwUnit, uint32_t nInstance)
{
    uint32_t nIdx = cb_library_index[nInstance];

    cbSPIKE_SORTING *pSortingOptions = &cb_cfg_buffer_ptr[nIdx]->isSortingOptions;
    if (dwUnit > cbMAXUNITS)
        return false;
    return (pSortingOptions->asSortModel[dwChan - 1][dwUnit].valid != 0);
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
bool cbHasSpikeSorting(uint32_t dwChan, uint32_t spkSrtOpt, uint32_t nInstance)
{
    uint32_t dwFlags;
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
cbRESULT cbSetSpikeSorting(uint32_t dwChan, uint32_t spkSrtOpt, uint32_t nInstance)
{
    uint32_t dwFlags, dwFilter;
    ::cbGetAinpSpikeOptions(dwChan, &dwFlags, &dwFilter, nInstance);

    dwFlags &= ~(cbAINPSPK_ALLSORT) ; // Delete all sortings first
    dwFlags |= spkSrtOpt; // Add requested sorting only
    cbRESULT nRet = ::cbSetAinpSpikeOptions(dwChan, dwFlags, dwFilter, nInstance);
    return nRet;
}


// Author & Date:   Almut Branner   Dec 9, 2003
// Purpose: Set all output channels to track a particular channel
// Input:   nChannel = channel to track   1 - based
void TrackChannel(uint32_t nChannel, uint32_t nInstance)
{
    int32_t smpdispmax = 0;
    int32_t spkdispmax = 0;
    uint32_t nChanProcStart = 0;
    uint32_t nChanProcMax = 0;
    cbPROCINFO isProcInfo;

    // Only do this if the channel is an analog input channel
    if (IsChanAnalogIn(nChannel))
    {
        // Find channel range for the NSP with the channel to be monitored
        for (uint32_t nProc = cbNSP1; nProc <= cbMAXPROCS; ++nProc)
        {
            if (NSP_FOUND == cbGetNspStatus(nProc))
            {
                ::cbGetProcInfo(nProc, &isProcInfo);
                nChanProcMax += isProcInfo.chancount;
                if (cbGetChanInstrument(nChannel) == nProc)
                    break;
                nChanProcStart = nChanProcMax;
            }
        }

        // scan through the analog output channels
        // if channel exists and is set to track, update its source channel
        for (uint32_t nOutCh = nChanProcStart + 1; nOutCh <= nChanProcMax; ++nOutCh)
        {
            if (IsChanAnalogOut(nOutCh) || IsChanAudioOut(nOutCh))
            {
                uint32_t options, monchan, value;
                if (cbGetAoutOptions(nOutCh, &options, &monchan, &value, nInstance) == cbRESULT_OK)
                {
                    if (options & cbAOUT_TRACK)
                    {
                        cbSetAoutOptions(nOutCh, options, nChannel, value, nInstance);

                        ///// Now adjust any of the analog output monitoring /////
                        // Get the input scaling
                        cbSCALING isInScaling;
                        ::cbGetAinpDisplay(nChannel, NULL, &smpdispmax, &spkdispmax, NULL);
                        ::cbGetAinpScaling(nChannel, &isInScaling, nInstance);

                        if (options & cbAOUT_MONITORSMP)      // Monitoring the continuous signal
                        {
                            int32_t nAnaMax = smpdispmax * isInScaling.anamax / isInScaling.digmax;
                            cbSCALING isOutScaling;

                            isOutScaling.digmin = -smpdispmax;
                            isOutScaling.digmax = smpdispmax;
                            isOutScaling.anamin = -nAnaMax;
                            isOutScaling.anamax = nAnaMax;
                            strncpy(isOutScaling.anaunit, isInScaling.anaunit, sizeof(isOutScaling.anaunit));
                            ::cbSetAoutScaling(nOutCh, &isOutScaling, nInstance);
                        }
                        if (options & cbAOUT_MONITORSPK) // Monitoring the spikes
                        {
                            int32_t nAnaMax = spkdispmax * isInScaling.anamax / isInScaling.digmax;
                            cbSCALING isOutScaling;

                            isOutScaling.digmin = -spkdispmax;
                            isOutScaling.digmax = spkdispmax;
                            isOutScaling.anamin = -nAnaMax;
                            isOutScaling.anamax = nAnaMax;
                            strncpy(isOutScaling.anaunit, isInScaling.anaunit, sizeof(isOutScaling.anaunit));
                            ::cbSetAoutScaling(nOutCh, &isOutScaling, nInstance);
                        }
                    }
                }
            }
            // Now scan through the Digital Out channels and set them
            if (IsChanDigout(nOutCh))
            {
                uint32_t nOptions, nVal;
                if (cbRESULT_OK == ::cbGetDoutOptions(nOutCh, &nOptions, NULL, &nVal, NULL, NULL, NULL, nInstance))
                {
                    // Now if we are set to track, then change the monitored channel
                    if (nOptions & cbDOUT_TRACK)
                    {
                        ::cbSetDoutOptions(nOutCh, nOptions, nChannel, nVal, cbDOUT_TRIGGER_NONE, 0, 0, nInstance);
                    }
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
cbRESULT cbSetAnaInOutDisplay(uint32_t chan, int32_t smpdispmax, int32_t spkdispmax, int32_t lncdispmax, uint32_t nInstance)
{
    cbRESULT retVal;

    // Start off by sending the new Analog In scaling
    retVal = ::cbSetAinpDisplay(chan, -smpdispmax, smpdispmax, spkdispmax, lncdispmax, nInstance);
    if (retVal != cbRESULT_OK)
        return retVal;


    ///// Now adjust any of the analog output monitoring /////

    // There are 6 analog channels of interest
    // They happen to be numbered so that AnalogOut and AudioOut are continuous
    for (uint32_t nAnaChan = MIN_CHANS; nAnaChan <= cb_pc_status_buffer_ptr[0]->cbGetNumTotalChans(); ++nAnaChan) //1-based ch numbering, 0 is reserved
    {
        if (IsChanAnalogOut(nAnaChan) || IsChanAudioOut(nAnaChan))
        {
            uint32_t dwOptions = 0;
            uint32_t dwMonChan = 0;
            ::cbGetAoutOptions(nAnaChan, &dwOptions, &dwMonChan, NULL, nInstance);
            dwMonChan = cbGetExpandedChannelNumber(cbGetChanInstrument(dwMonChan), dwMonChan);

            if (dwMonChan == chan)
            {
                // Get the input scaling
                cbSCALING isInScaling;
                retVal = ::cbGetAinpScaling(chan, &isInScaling, nInstance);
                if (retVal != cbRESULT_OK)
                    return retVal;

                if (dwOptions & cbAOUT_MONITORSMP)      // Monitoring the continuous signal
                {
                    int32_t nAnaMax = smpdispmax * isInScaling.anamax / isInScaling.digmax;
                    cbSCALING isOutScaling;

                    isOutScaling.digmin = -smpdispmax;
                    isOutScaling.digmax = smpdispmax;
                    isOutScaling.anamin = -nAnaMax;
                    isOutScaling.anamax = nAnaMax;
                    strncpy(isOutScaling.anaunit, isInScaling.anaunit, sizeof(isOutScaling.anaunit));
                    retVal = ::cbSetAoutScaling(nAnaChan, &isOutScaling, nInstance);
                    if (retVal != cbRESULT_OK)
                        return retVal;
                }
                else if (dwOptions & cbAOUT_MONITORSPK) // Monitoring the spikes
                {
                    int32_t nAnaMax = spkdispmax * isInScaling.anamax / isInScaling.digmax;
                    cbSCALING isOutScaling;

                    isOutScaling.digmin = -spkdispmax;
                    isOutScaling.digmax = spkdispmax;
                    isOutScaling.anamin = -nAnaMax;
                    isOutScaling.anamax = nAnaMax;
                    strncpy(isOutScaling.anaunit, isInScaling.anaunit, sizeof(isOutScaling.anaunit));
                    retVal = ::cbSetAoutScaling(nAnaChan, &isOutScaling, nInstance);
                    if (retVal != cbRESULT_OK)
                        return retVal;
                }
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
cbRESULT cbGetScaling(uint32_t nChan,
                      uint32_t *anSpikeScale,
                      uint32_t *anContScale,
                      uint32_t *anLncScale, 
                      uint32_t nInstance)
{
    cbRESULT retVal;
    cbSCALING isScaling;
    int32_t nAnaMax;
    int i;

    if (!IsChanAnalogIn(nChan))
        return cbRESULT_INVALIDCHANNEL;

    retVal = ::cbGetAinpScaling(nChan, &isScaling, nInstance);
    if (retVal != cbRESULT_OK)
        return retVal;

    nAnaMax = isScaling.anamax / isScaling.anagain;

    if (IsChanFEAnalogIn(nChan))
    {
        if (anContScale)
        {
            for (i = 0; i < SCALE_CONTINUOUS_COUNT; ++i)
                anContScale[SCALE_CONTINUOUS_COUNT - i - 1] = uint32_t(nAnaMax * exp(-0.3467 * i));
        }

        if (anLncScale)
        {
            for (i = 0; i < SCALE_LNC_COUNT; ++i)
                anLncScale[SCALE_LNC_COUNT - i - 1] = uint32_t(nAnaMax * exp(-0.3467 * i));
        }

        if (anSpikeScale)
        {
            for (i = 0; i < SCALE_SPIKE_COUNT; ++i)
                anSpikeScale[SCALE_SPIKE_COUNT - i - 1] = uint32_t(0.25 * nAnaMax * exp(-0.3467 / 2.0 * i));

            anSpikeScale[SCALE_SPIKE_COUNT - 1] = nAnaMax;
            anSpikeScale[SCALE_SPIKE_COUNT - 2] = nAnaMax / 2;
            anSpikeScale[SCALE_SPIKE_COUNT - 3] = nAnaMax / 4;
        }
    }
    else
    {
        if (anContScale)
            for (i = 0; i < SCALE_CONTINUOUS_COUNT; ++i)
                anContScale[SCALE_CONTINUOUS_COUNT - i - 1] = uint32_t(nAnaMax * exp(-0.3467 * i));

        if (anLncScale)
            for (i = 0; i < SCALE_LNC_COUNT; ++i)
                anLncScale[SCALE_LNC_COUNT - i - 1] = uint32_t(nAnaMax * exp(-0.3467 * i));

        if (anSpikeScale)
            for (i = 0; i < SCALE_SPIKE_COUNT; ++i)
                anSpikeScale[SCALE_SPIKE_COUNT - i - 1] = uint32_t(nAnaMax * exp(-0.3467 * i / 2.0));

    }
    return cbRESULT_OK;
}


////////////////////////////// acquisition groups ////////////////////////////////////////////////

// These will be ordered by group so I don't need that
struct cbAcqSettings
{
    uint32_t nFilter;         // Which filter
    uint32_t nSampleGroup;    // Which sample group
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
uint32_t cbGetSmpGroup(uint32_t nChan, uint32_t nInstance)
{
        uint32_t dwGroup;
        ::cbGetAinpSampling(nChan, NULL, &dwGroup, nInstance);
        return dwGroup;
}

// Author & Date:   Ehsan Azar     28 May 2009
// Purpose: tell me how many sampling groups exist
//  this number will always be larger than 1 because group 0 (empty)
//  will always exist
uint32_t cbGetSmpGroupCount()
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
uint32_t cbGetAcqGroup(uint32_t nChan, uint32_t nInstance)
{
    uint32_t nFilter = 0;
    uint32_t nSG = 0;
    ::cbGetAinpSampling(nChan, &nFilter, &nSG, nInstance);

    for (const cbAcqSettings * pTest = isAcqData; pTest != ARRAY_END(isAcqData); ++pTest)
    {
        if (pTest->nFilter == nFilter &&
            pTest->nSampleGroup == nSG)
        {
            return uint32_t(pTest - &isAcqData[0]);
        }
    }

    // If I got here then there was no match. I had better make it "not sample"
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
cbRESULT cbSetAcqGroup(uint32_t nChan, uint32_t nGroup, uint32_t nInstance)
{
    if (nGroup >= ACQ_GROUP_COUNT)
        return cbRESULT_INVALIDFUNCTION;

    uint32_t nFilter = isAcqData[nGroup].nFilter;
    uint32_t nSG = isAcqData[nGroup].nSampleGroup;
    cbRESULT nRet = ::cbSetAinpSampling(nChan, nFilter, nSG, nInstance);
    return nRet;
}


// Purpose: tell me how many acquisition groups exist
//  this number will always be larger than 1 because group 0 (empty)
//  will always exist
uint32_t cbGetAcqGroupCount()
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
const char * cbGetAcqGroupDesc(uint32_t nGroup)
{
    return asContFilter[nGroup < ACQ_GROUP_COUNT ? nGroup : 0];
}

// Author & Date:   Hyrum L. Sessions   29 May 2007
// Purpose: get the filter used by an acquision group
// Inputs:  nGroup - group in question
// Outputs: filter value
uint32_t cbGetAcqGroupFilter(uint32_t nGroup)
{
    return isAcqData[nGroup < ACQ_GROUP_COUNT ? nGroup : 0].nFilter;
}

// Author & Date:   Hyrum L. Sessions   29 May 2007
// Purpose: get the sample group used by an acquision group
// Inputs:  nGroup - group in question
// Outputs: sample group value
uint32_t cbGetAcqGroupSampling(uint32_t nGroup)
{
    return isAcqData[nGroup < ACQ_GROUP_COUNT ? nGroup : 0].nSampleGroup;
}

// Purpose: get the number of units for this channel
// Inputs:  nChan - 1 based channel of interest
// Returns: number of valid units for this channel
uint32_t cbGetChanUnits(uint32_t nChan, uint32_t nInstance)
{
    uint32_t nIdx = cb_library_index[nInstance];

    ASSERT(nChan >= MIN_CHANS);
    ASSERT(nChan <= cbMAXCHANS);

    uint32_t nValidUnits = 0;

    // let's start at 1 so we don't get the noise unit which is 0
    for (uint32_t nUnit = 1; nUnit < cb_pc_status_buffer_ptr[0]->cbGetNumTotalChans(); ++nUnit)
    {
        if (cb_cfg_buffer_ptr[nIdx]->isSortingOptions.asSortModel[nChan - 1][nUnit].valid)
            ++nValidUnits;
    }
    return nValidUnits;
}

// Purpose: tells if the unit in the channel is valid
// Inputs:  nChan - 1 based channel of interest
//          nUnit - 1 based unit of interest (0 is noise unit)
// Returns: 1 if the unit in the channel is valid, 0 is otherwise
uint32_t cbIsChanUnitValid(uint32_t nChan, int32_t nUnit, uint32_t nInstance)
{
    uint32_t nIdx = cb_library_index[nInstance];

    bool bValid = false;
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
// Purpose: Set the noise boundary parameters (compatibility for int16_t)
// Inputs:
//  chanIdx  - channel number (1-based)
//  anCentroid - the center of an ellipsoid
//  anMajor - major axis of the ellipsoid
//  anMinor_1 - first minor axis of the ellipsoid
//  anMinor_2 - second minor axis of the ellipsoid
// Outputs:
//  cbRESULT_OK if life is good
cbRESULT cbSSSetNoiseBoundary(uint32_t chanIdx, int16_t anCentroid[3], int16_t anMajor[3], int16_t anMinor_1[3], int16_t anMinor_2[3], uint32_t nInstance)
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
cbRESULT cbGetNTrodeUnitMapping(uint32_t ntrode, uint16_t nSite, cbMANUALUNITMAPPING *unitmapping, uint32_t nInstance)
{
    uint32_t nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Test that the NTrode number is valid and initialized
    if ((ntrode - 1) >= cbMAXNTRODES) return cbRESULT_INVALIDNTRODE;
    if (nSite >= cbMAXSITEPLOTS) return cbRESULT_INVALIDFUNCTION;
    if (cb_cfg_buffer_ptr[nIdx]->isNTrodeInfo[ntrode - 1].cbpkt_header.chid==0) return cbRESULT_INVALIDNTRODE;

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
cbRESULT cbSetNTrodeUnitMapping(uint32_t ntrode, uint16_t nSite, cbMANUALUNITMAPPING *unitmapping, int16_t fs, uint32_t nInstance)
{
    uint32_t nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Test that the NTrode number is valid and initialized
    if ((ntrode - 1) >= cbMAXNTRODES) return cbRESULT_INVALIDNTRODE;
    if (cb_cfg_buffer_ptr[nIdx]->isNTrodeInfo[ntrode - 1].cbpkt_header.chid == 0) return cbRESULT_INVALIDNTRODE;
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
cbRESULT cbSetNTrodeFeatureSpace(uint32_t ntrode, uint16_t fs, uint32_t nInstance)
{
    uint32_t nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Test that the NTrode number is valid and initialized
    if ((ntrode - 1) >= cbMAXNTRODES) return cbRESULT_INVALIDNTRODE;
    if (cb_cfg_buffer_ptr[nIdx]->isNTrodeInfo[ntrode - 1].cbpkt_header.chid == 0) return cbRESULT_INVALIDNTRODE;
    // If feature space has changed
    if (fs != cb_cfg_buffer_ptr[nIdx]->isNTrodeInfo[ntrode - 1].fs)
        return cbSetNTrodeUnitMapping(ntrode, 0, NULL, fs, nInstance);
    return cbRESULT_OK;
}

uint32_t cbGetNTrodeInstrument(uint32_t nNTrode, uint32_t nInstance)
{
    uint32_t nInstrument = cbNSP1;    // add to chan for instruments > 0
    uint32_t nIdx = cb_library_index[nInstance];
#ifndef CBPROTO_311
    if (cb_cfg_buffer_ptr[nIdx]->isNTrodeInfo[nNTrode - 1].cbpkt_header.instrument < cbMAXPROCS)
        nInstrument = cb_cfg_buffer_ptr[nIdx]->isNTrodeInfo[nNTrode - 1].cbpkt_header.instrument + 1;
#endif
    ASSERT(nInstrument - 1 < cbMAXPROCS);

    return nInstrument;
}

// Author & Date:   Ehsan Azar     6 Nov 2012
// Purpose: Find if file recording is active
// Outputs:
//  returns if file is being recorded
bool IsFileRecording(uint32_t nInstance)
{
    cbPKT_FILECFG filecfg;
    if (cbGetFileInfo(&filecfg, nInstance) == cbRESULT_OK)
    {
        if (filecfg.options == cbFILECFG_OPT_REC)
            return true;
    }
    return false;
}

/// @author     Hyrum L. Sessions
/// @date       24 Feb 2017
/// @brief      Get the channel number of the specified ordinal analog input channel
///             e.g. on a 128 channel system, the 1st digin channel is 151
///
/// @param [in] nOrdinal 1 based ordinal digin to find
/// @return     Channel number of the ordinal digin, 0 if none exist or invalid ordinal
uint32_t GetAIAnalogInChanNumber(uint32_t nOrdinal, uint32_t nInstance)
{
    uint32_t nReturn = 0;
    uint32_t nChan;
    uint32_t nMaxChan = cb_pc_status_buffer_ptr[0]->cbGetNumTotalChans();

    if (1 <= nOrdinal)
    {
        for (nChan = MIN_CHANS; nChan <= nMaxChan; ++nChan)
        {
            if (IsChanAIAnalogIn(nChan, nInstance) && (0 == --nOrdinal))
            {
                nReturn = nChan;
                break;
            }
        }
    }
    return nReturn;
}

/// @author     Hyrum L. Sessions
/// @date       24 Feb 2017
/// @brief      Get the channel number of the specified ordinal digin channel
///             e.g. on a 128 channel system, the 1st digin channel is 151
///
/// @param [in] nOrdinal 1 based ordinal digin to find
/// @return     Channel number of the ordinal digin, 0 if none exist or invalid ordinal
uint32_t GetAnalogOutChanNumber(uint32_t nOrdinal, uint32_t nInstance)
{
    uint32_t nReturn = 0;
    uint32_t nChan;
    uint32_t nMaxChan = cb_pc_status_buffer_ptr[0]->cbGetNumTotalChans();

    if (1 <= nOrdinal)
    {
        for (nChan = MIN_CHANS; nChan <= nMaxChan; ++nChan)
        {
            if (IsChanAnalogOut(nChan, nInstance) && (0 == --nOrdinal))
            {
                nReturn = nChan;
                break;
            }
        }
    }
    return nReturn;
}

/// @author     Hyrum L. Sessions
/// @date       24 Feb 2017
/// @brief      Get the channel number of the specified ordinal digin channel
///             e.g. on a 128 channel system, the 1st digin channel is 151
///
/// @param [in] nOrdinal 1 based ordinal digin to find
/// @return     Channel number of the ordinal digin, 0 if none exist or invalid ordinal
uint32_t GetAudioOutChanNumber(uint32_t nOrdinal, uint32_t nInstance)
{
    uint32_t nReturn = 0;
    uint32_t nChan;
    uint32_t nMaxChan = cb_pc_status_buffer_ptr[0]->cbGetNumTotalChans();

    if (1 <= nOrdinal)
    {
        for (nChan = MIN_CHANS; nChan <= nMaxChan; ++nChan)
        {
            if (IsChanAudioOut(nChan, nInstance) && (0 == --nOrdinal))
            {
                nReturn = nChan;
                break;
            }
        }
    }
    return nReturn;
}

/// @author     Hyrum L. Sessions
/// @date       27 Feb 2017
/// @brief      Get the channel number of the specified ordinal analog or audio channel
///             e.g. on a 128 channel system, the 1st digin channel is 145
///
/// @param [in] nOrdinal 1 based ordinal digin to find
/// @return     Channel number of the ordinal digin, 0 if none exist or invalid ordinal
uint32_t GetAnalogOrAudioOutChanNumber(uint32_t nOrdinal, uint32_t nInstance)
{
    uint32_t nReturn = 0;
    uint32_t nChan;
    uint32_t nMaxChan = cb_pc_status_buffer_ptr[0]->cbGetNumTotalChans();

    if (1 <= nOrdinal)
    {
        for (nChan = MIN_CHANS; nChan <= nMaxChan; ++nChan)
        {
            if ((IsChanAnalogOut(nChan, nInstance) || IsChanAudioOut(nChan, nInstance)) &&
                (0 == --nOrdinal))
            {
                nReturn = nChan;
                break;
            }
        }
    }
    return nReturn;
}

/// @author     Hyrum L. Sessions
/// @date       24 Feb 2017
/// @brief      Get the channel number of the specified ordinal digin channel
///             e.g. on a 128 channel system, the 1st digin channel is 151
///
/// @param [in] nOrdinal 1 based ordinal digin to find
/// @return     Channel number of the ordinal digin, 0 if none exist or invalid ordinal
uint32_t GetDiginChanNumber(uint32_t nOrdinal, uint32_t nInstance)
{
    uint32_t nReturn = 0;
    uint32_t nChan;
    uint32_t nMaxChan = cb_pc_status_buffer_ptr[0]->cbGetNumTotalChans();

    if (1 <= nOrdinal)
    {
        for (nChan = MIN_CHANS; nChan <= nMaxChan; ++nChan)
        {
            if (IsChanDigin(nChan, nInstance) && (0 == --nOrdinal))
            {
                nReturn = nChan;
                break;
            }
        }
    }
    return nReturn;
}

/// @author     Hyrum L. Sessions
/// @date       24 Feb 2017
/// @brief      Get the channel number of the specified ordinal digin channel
///             e.g. on a 128 channel system, the 1st digin channel is 151
///
/// @param [in] nOrdinal 1 based ordinal digin to find
/// @return     Channel number of the ordinal digin, 0 if none exist or invalid ordinal
uint32_t GetSerialChanNumber(uint32_t nOrdinal, uint32_t nInstance)
{
    uint32_t nReturn = 0;
    uint32_t nChan;
    uint32_t nMaxChan = cb_pc_status_buffer_ptr[0]->cbGetNumTotalChans();

    if (1 <= nOrdinal)
    {
        for (nChan = MIN_CHANS; nChan <= nMaxChan; ++nChan)
        {
            if (IsChanSerial(nChan, nInstance) && (0 == --nOrdinal))
            {
                nReturn = nChan;
                break;
            }
        }
    }
    return nReturn;
}

/// @author     Hyrum L. Sessions
/// @date       24 Feb 2017
/// @brief      Get the channel number of the specified ordinal digin channel
///             e.g. on a 128 channel system, the 1st digin channel is 151
///
/// @param [in] nOrdinal 1 based ordinal digin to find
/// @return     Channel number of the ordinal digin, 0 if none exist or invalid ordinal
uint32_t GetDigoutChanNumber(uint32_t nOrdinal, uint32_t nInstance)
{
    uint32_t nReturn = 0;
    uint32_t nChan;
    uint32_t nMaxChan = cb_pc_status_buffer_ptr[0]->cbGetNumTotalChans();

    if (1 <= nOrdinal)
    {
        for (nChan = MIN_CHANS; nChan <= nMaxChan; ++nChan)
        {
            if (IsChanDigout(nChan, nInstance) && (0 == --nOrdinal))
            {
                nReturn = nChan;
                break;
            }
        }
    }
    return nReturn;
}

uint32_t cbGetNumActiveInstruments()
{
    uint32_t nNumActiveInstruments = 0;

    for (int nProc = 0; nProc < cbMAXPROCS; ++nProc)
        if (NSP_FOUND == cbGetNspStatus(nProc + 1))
            ++nNumActiveInstruments;

    return nNumActiveInstruments;
}


NSP_STATUS cbGetNspStatus(uint32_t nInstrument)
{
    if (nInstrument > cbMAXPROCS)
        return NSP_INVALID;
    return cb_pc_status_buffer_ptr[0]->cbGetNspStatus(nInstrument - 1);
}

#ifndef CBPROTO_311
void cbSetNspStatus(uint32_t nInstrument, NSP_STATUS nStatus)
{
    ASSERT(nInstrument - 1 < cbMAXPROCS);
    if (nInstrument - 1 < cbMAXPROCS)
        cb_pc_status_buffer_ptr[0]->cbSetNspStatus(nInstrument - 1, nStatus);
}
#endif

uint32_t cbGetExpandedChannelNumber(uint32_t nInstrument, uint32_t nChannel)
{
    uint32_t nChanAdd = 0;    // add to chan for instruments > 0
    cbPROCINFO isProcInfo;

    if ((nChannel - 1) >= cbMAXCHANS)
        return 0;

    ASSERT(nInstrument - 1 < cbMAXPROCS);
    if (nInstrument > cbMAXPROCS)
        return nChannel;

    for (UINT nProc = cbNSP1; nProc <= cbMAXPROCS; ++nProc)
    {
        if (nInstrument == nProc)
            return nChannel + nChanAdd;

        memset(&isProcInfo, 0, sizeof(cbPROCINFO));
        ::cbGetProcInfo(nProc, &isProcInfo);
        nChanAdd += isProcInfo.chancount;
    }
    return nChannel;
}


uint32_t cbGetChanInstrument(uint32_t nChannel, uint32_t nInstance)
{
    uint32_t nInstrument = cbNSP1;    // add to chan for instruments > 0
    uint32_t nIdx = cb_library_index[nInstance];

    if ((nChannel - 1) >= cbMAXCHANS)
        return 0;

#ifndef CBPROTO_311
    if (cb_cfg_buffer_ptr[nIdx]->chaninfo[nChannel - 1].cbpkt_header.instrument < cbMAXPROCS)
        nInstrument = cb_cfg_buffer_ptr[nIdx]->chaninfo[nChannel - 1].cbpkt_header.instrument + 1;
#endif
    ASSERT(nInstrument - 1 < cbMAXPROCS);

    return nInstrument;
}

/// @author Hyrum L. Sessions
/// @date   14 April 2021
/// @brief  Get the channel number that is local to the instrument
///         e.g. channel 285 is the first channel on instrument two so return 1
uint32_t cbGetInstrumentLocalChannelNumber(uint32_t nChan)
{
    // Test that the channel address is valid and initialized
    if ((nChan - 1) >= cbMAXCHANS) return cbRESULT_INVALIDCHANNEL;
    if (cb_cfg_buffer_ptr[0]->chaninfo[nChan - 1].cbpkt_header.chid == 0) return cbRESULT_INVALIDCHANNEL;

    // Return the requested data from the rec buffer
    return cb_cfg_buffer_ptr[0]->chaninfo[nChan - 1].chan;
}
