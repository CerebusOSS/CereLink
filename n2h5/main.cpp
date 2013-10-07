//////////////////////////////////////////////////////////////////////////////
//
// (c) Copyright 2012 Blackrock Microsystems
//
// $Workfile: main.c $
// $Archive: /n2h5/main.c $
// $Revision: 1 $
// $Date: 11/1/12 1:00p $
// $Author: Ehsan $
//
// $NoKeywords: $
//
//////////////////////////////////////////////////////////////////////////////
//
// PURPOSE:
//
// Convert nsx and nev files to hdf5 format
//
// Multiple data groups can be merged in one file
//  For example to combine different experiments.
// Main data group populates the root itself, 
//  the next roots at "/group00001" and so forth
// The field GroupCount in the root attributes contains total data groups count
// While first level group names are part of the spec (/channel, /comment, /group00001, ...)
//  the sub-group names should not be relied upon, instead all children should be iterated
//  and attributes consulted to find the information about each sub-group
// To merge channels of the same experiment, it should be added to the same group
//  for example the raw recording of /channel/channel00001 can go to /channel/channel00001_1
//
//////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"

#include "n2h5.h"
#include "NevNsx.h"

#ifndef WIN32
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#endif


// Keep this after all headers
#include "compat.h"

#ifdef WIN32                          // Windows needs the different spelling
#define ftello _ftelli64
#define fseeko _fseeki64
#endif

 // TODO: optimize these numbers
#define CHUNK_SIZE_CONTINUOUS   (1024)
#define CHUNK_SIZE_EVENT        (1024)

UINT16 g_nCombine = 0; // subchannel combine level
bool g_bAppend = false;
bool g_bNoSpikes = false;
bool g_bSkipEmpty = false;

// Author & Date:   Ehsan Azar   Jan 16, 2013
// Purpose: Add created header to the hdf file
// Inputs:
//  file  - the destination file
//  header - Root header
// Outputs:
//   Returns 0 on success, error code otherwise
int AddRoot(hid_t file, BmiRootAttr_t & header)
{
    herr_t ret;

    // Add file header as attribute of the root group
    {
        std::string strAttr = "BmiRoot";
        hsize_t     dims[1] = {1};
        hid_t space = H5Screate_simple(1, dims, NULL);
        hid_t gid_root = H5Gopen(file, "/", H5P_DEFAULT);
        if(!H5Aexists(gid_root, strAttr.c_str()))
        {
            hid_t tid_root_attr = CreateRootAttrType(file);
            hid_t aid_root = H5Acreate(gid_root, strAttr.c_str(), tid_root_attr, space, H5P_DEFAULT, H5P_DEFAULT);
            ret = H5Awrite(aid_root, tid_root_attr, &header);
            ret = H5Aclose(aid_root);
            H5Tclose(tid_root_attr);
        }

        ret = H5Gclose(gid_root);
        ret = H5Sclose(space);
    }

    return 0;
}

// Author & Date:   Ehsan Azar   Nov 17, 2012
// Purpose: Create and add root
// Inputs:
//  pFile - the source file
//  file  - the destination file
//  isHdr - NEV header
// Outputs:
//   Returns 0 on success, error code otherwise
int AddRoot(FILE * pFile, hid_t file, NevHdr & isHdr)
{
    BmiRootAttr_t header;
    memset(&header, 0, sizeof(header));
    header.nMajorVersion = 1;
    header.szApplication = isHdr.szApplication;
    header.szComment = isHdr.szComment;
    header.nGroupCount = 1;
    {
        TIMSTM ts;
        memset(&ts, 0, sizeof(ts));
        SYSTEMTIME & st = isHdr.isAcqTime;
        sprintf((char *)&ts, "%04hd-%02hd-%02hd %02hd:%02hd:%02hd.%06d",
                st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute,
                st.wSecond, st.wMilliseconds * 1000); 
        ts.chNull = '\0';
        header.szDate = (char *)&ts;
    }

    return AddRoot(file, header);
}

// Author & Date:   Ehsan Azar   Nov 17, 2012
// Purpose: Create and add root
// Inputs:
//  szSrcFile - source file name
//  pFile     - the source file
//  file      - the destination file
//  isHdr     - Nsx 2.1 header
// Outputs:
//   Returns 0 on success, error code otherwise
int AddRoot(const char * szSrcFile, FILE * pFile, hid_t file, Nsx21Hdr & isHdr)
{
    BmiRootAttr_t header;
    memset(&header, 0, sizeof(header));
    header.nMajorVersion = 1;
    header.szApplication = isHdr.szGroup;
    char szComment[] = ""; // Old format does not have a comment
    header.szComment = szComment;
    header.nGroupCount = 1;
    TIMSTM ts;
    memset(&ts, 0, sizeof(ts));
#ifdef WIN32
    WIN32_FILE_ATTRIBUTE_DATA fattr;
    if (!GetFileAttributesEx(szSrcFile, GetFileExInfoStandard, &fattr))
    {
        printf("Cannot get file attributes\n");
        return 1;
    }
    SYSTEMTIME st;
    FileTimeToSystemTime(&fattr.ftCreationTime, &st);
    sprintf((char *)&ts, "%04hd-%02hd-%02hd %02hd:%02hd:%02hd.%06d",
            st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute,
            st.wSecond, st.wMilliseconds * 1000); 
    ts.chNull = '\0';
    header.szDate = (char *)&ts;
#else
    struct stat st;
    if (stat(szSrcFile, &st))
    {
        printf("Cannot get file attributes\n");
        return 1;
    }
    time_t t = st.st_mtime;
    // TODO: use strftime to convert to st
#endif

    return AddRoot(file, header);
}

// Author & Date:   Ehsan Azar   Nov 17, 2012
// Purpose: Create and add root
// Inputs:
//  pFile - the source file
//  file  - the destination file
//  isHdr - NSx 2.2 header
// Outputs:
//   Returns 0 on success, error code otherwise
int AddRoot(FILE * pFile, hid_t file, Nsx22Hdr & isHdr)
{
    BmiRootAttr_t header;
    memset(&header, 0, sizeof(header));
    header.nMajorVersion = 1;
    header.szApplication = isHdr.szGroup;
    header.szComment = isHdr.szComment;
    header.nGroupCount = 1;
    {
        TIMSTM ts;
        memset(&ts, 0, sizeof(ts));
        SYSTEMTIME & st = isHdr.isAcqTime;
        sprintf((char *)&ts, "%04hd-%02hd-%02hd %02hd:%02hd:%02hd.%06d",
                st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute,
                st.wSecond, st.wMilliseconds * 1000); 
        ts.chNull = '\0';
        header.szDate = (char *)&ts;
    }

    return AddRoot(file, header);
}

// Author & Date:   Ehsan Azar   Nov 13, 2012
// Purpose: Convert NEV 
// Inputs:
//  pFile - the source file
//  file  - the destination file
// Outputs:
//   Returns 0 on success, error code otherwise
int ConvertNev(FILE * pFile, hid_t file)
{
    herr_t ret;
    
    NevHdr isHdr;
    fseeko(pFile, 0, SEEK_SET);    // read header from beginning of file
    if (fread(&isHdr, sizeof(isHdr), 1, pFile) != 1)
    {
        printf("Cannot read source file header\n");
        return 1;
    }

    // Add root attribute
    if (AddRoot(pFile, file, isHdr))
        return 1;

    UINT16 nSpikeLength = 48;
    hid_t tid_spike = -1;
    hid_t tid_dig = -1;
    hid_t tid_comment = -1;
    hid_t tid_tracking[cbMAXTRACKOBJ];
    int   size_tracking[cbMAXTRACKOBJ] = {0};
    hid_t tid_synch = -1;
    hid_t tid_sampling_attr = -1;
    hid_t tid_filt_attr = -1;

    // 21, 22, 23 flat file verion number
    UINT32 nVer = isHdr.byFileRevMajor * 10 + isHdr.byFileRevMinor;

    nSpikeLength = (isHdr.dwBytesPerPacket - 8) / 2;

    char * szMapFile = NULL;
    BmiTrackingAttr_t trackingAttr[cbMAXTRACKOBJ];
    memset(trackingAttr, 0, sizeof(trackingAttr));
    BmiSynchAttr_t synchAttr;
    memset(&synchAttr, 0, sizeof(synchAttr));
    BmiSamplingAttr_t samplingAttr[cbNUM_ANALOG_CHANS];
    memset(samplingAttr, 0, sizeof(samplingAttr));
    BmiChanAttr_t chanAttr[cbMAXCHANS];
    memset(chanAttr, 0, sizeof(chanAttr));
    BmiChanExtAttr_t chanExtAttr[cbNUM_ANALOG_CHANS];
    memset(chanExtAttr, 0, sizeof(chanExtAttr));
    BmiFiltAttr_t filtAttr[cbNUM_ANALOG_CHANS];
    memset(filtAttr, 0, sizeof(filtAttr));
    // NEV provides Ext1 additional header
    BmiChanExt1Attr_t chanExt1Attr[cbNUM_ANALOG_CHANS];
    memset(chanExt1Attr, 0, sizeof(chanExt1Attr));
    // Read the header to fill channel attributes
    for (UINT32 i = 0; i < isHdr.dwNumOfExtendedHeaders; ++i)
    {
        NevExtHdr isExtHdr;
        if (fread(&isExtHdr, sizeof(isExtHdr), 1, pFile) != 1)
        {
            printf("Invalid source file header\n");
            return 1;
        }
        if (0 == strncmp(isExtHdr.achPacketID, "NEUEVWAV", sizeof(isExtHdr.achPacketID)))
        {
            if (isExtHdr.neuwav.wave_samples != 0)
                nSpikeLength = isExtHdr.neuwav.wave_samples;
            int id = isExtHdr.id;
            if (id == 0 || id > cbNUM_ANALOG_CHANS)
            {
                printf("Invalid channel ID in source file header\n");
                return 1;
            }
            id--; // make it zero-based
            chanAttr[id].id = isExtHdr.id;
            // Currently all spikes are sampled at clock rate
            samplingAttr[id].fClock = float(isHdr.dwTimeStampResolutionHz);
            samplingAttr[id].fSampleRate = float(isHdr.dwSampleResolutionHz);
            samplingAttr[id].nSampleBits = isExtHdr.neuwav.wave_bytes * 8;

            chanExtAttr[id].phys_connector = isExtHdr.neuwav.phys_connector;
            chanExtAttr[id].connector_pin = isExtHdr.neuwav.connector_pin;
            chanExtAttr[id].dFactor = isExtHdr.neuwav.digital_factor;

            chanExt1Attr[id].energy_thresh = isExtHdr.neuwav.energy_thresh;
            chanExt1Attr[id].high_thresh = isExtHdr.neuwav.high_thresh;
            chanExt1Attr[id].low_thresh = isExtHdr.neuwav.low_thresh;
            chanExt1Attr[id].sortCount = isExtHdr.neuwav.sorted_count;

        }
        else if (0 == strncmp(isExtHdr.achPacketID, "NEUEVLBL", sizeof(isExtHdr.achPacketID)))
        {
            int id = isExtHdr.id;
            if (id == 0 || id > cbNUM_ANALOG_CHANS)
            {
                printf("Invalid channel ID in source file header\n");
                return 1;
            }
            id--;
            chanAttr[id].szLabel = _strdup(isExtHdr.neulabel.label);
        }
        else if (0 == strncmp(isExtHdr.achPacketID, "NEUEVFLT", sizeof(isExtHdr.achPacketID)))
        {
            int id = isExtHdr.id;
            if (id == 0 || id > cbNUM_ANALOG_CHANS)
            {
                printf("Invalid channel ID in source file header\n");
                return 1;
            }
            id--;
            filtAttr[id].hpfreq = isExtHdr.neuflt.hpfreq;
            filtAttr[id].hporder = isExtHdr.neuflt.hporder;
            filtAttr[id].hptype = isExtHdr.neuflt.hptype;
            filtAttr[id].lpfreq = isExtHdr.neuflt.lpfreq;
            filtAttr[id].lporder = isExtHdr.neuflt.lporder;
            filtAttr[id].lptype = isExtHdr.neuflt.lptype;
        }
        else if (0 == strncmp(isExtHdr.achPacketID, "VIDEOSYN", sizeof(isExtHdr.achPacketID)))
        {
            synchAttr.id = isExtHdr.id;
            synchAttr.fFps = isExtHdr.videosyn.fFps;
            synchAttr.szLabel = _strdup(isExtHdr.videosyn.label);
        }
        else if (0 == strncmp(isExtHdr.achPacketID, "TRACKOBJ", sizeof(isExtHdr.achPacketID)))
        {
            int id = isExtHdr.trackobj.trackID; // 1-based
            if (id == 0 || id > cbMAXTRACKOBJ)
            {
                printf("Invalid trackable ID in source file header\n");
                return 1;
            }
            id--;
            trackingAttr[id].type = isExtHdr.id; // 0-based type
            trackingAttr[id].trackID = isExtHdr.trackobj.trackID;
            trackingAttr[id].maxPoints = isExtHdr.trackobj.maxPoints;
            trackingAttr[id].szLabel = _strdup(isExtHdr.trackobj.label);
        }
        else if (0 == strncmp(isExtHdr.achPacketID, "MAPFILE", sizeof(isExtHdr.achPacketID)))
        {
            szMapFile = _strdup(isExtHdr.mapfile.label);
        }
        else if (0 == strncmp(isExtHdr.achPacketID, "DIGLABEL", sizeof(isExtHdr.achPacketID)))
        {
            int id = 0;
            if (isExtHdr.diglabel.mode == 0)
            {
                id = cbFIRST_SERIAL_CHAN; // Serial
                chanAttr[id].id = cbFIRST_SERIAL_CHAN + 1;
            }
            else if (isExtHdr.diglabel.mode == 1)
            {
                id = cbFIRST_DIGIN_CHAN; // Digital
                chanAttr[id].id = cbFIRST_DIGIN_CHAN + 1;
            } else {
                printf("Invalid digital input mode in source file header\n");
                return 1;
            }
            chanAttr[id].szLabel = _strdup(isExtHdr.diglabel.label);
        } else {
            printf("Unknown header (%7s) in the source file\n", isExtHdr.achPacketID);
        }
    } // end for (UINT32 i = 0

    UINT32 nChannelOffset = 0;
    UINT32 nDigChannelOffset = 0;
    UINT32 nSerChannelOffset = 0;

    hsize_t     dims[1] = {1};
    hid_t space_attr = H5Screate_simple(1, dims, NULL);

    // Add channel group
    {
        hid_t gid_channel = -1;
        if (H5Lexists(file, "channel", H5P_DEFAULT))
            gid_channel = H5Gopen(file, "channel", H5P_DEFAULT);
        else
            gid_channel = H5Gcreate(file, "channel", H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
        if (szMapFile != NULL)
        {
            hid_t tid_attr_map_str = H5Tcopy(H5T_C_S1);
            ret = H5Tset_size(tid_attr_map_str, 1024);
            hid_t aid = H5Acreate(gid_channel, "MapFile", tid_attr_map_str, space_attr, H5P_DEFAULT, H5P_DEFAULT);
            ret = H5Awrite(aid, tid_attr_map_str, szMapFile);
            ret = H5Aclose(aid);
            H5Tclose(tid_attr_map_str);
        }
        if (g_bAppend)
        {
            bool bExists = false;
            // Find the last place to append to
            do {
                nChannelOffset++;
                std::string strLabel = "channel";
                char szNum[7];
                sprintf(szNum, "%05u", nChannelOffset);
                strLabel += szNum;
                bExists = (H5Lexists(gid_channel, strLabel.c_str(), H5P_DEFAULT) != 0);
            } while(bExists);
            nChannelOffset--;
            do {
                nDigChannelOffset++;
                std::string strLabel = "digital";
                char szNum[7];
                sprintf(szNum, "%05u", nDigChannelOffset);
                strLabel += szNum;
                bExists = (H5Lexists(gid_channel, strLabel.c_str(), H5P_DEFAULT) != 0);
            } while(bExists);
            nDigChannelOffset--;
            do {
                nSerChannelOffset++;
                std::string strLabel = "serial";
                char szNum[7];
                sprintf(szNum, "%05u", nSerChannelOffset);
                strLabel += szNum;
                bExists = (H5Lexists(gid_channel, strLabel.c_str(), H5P_DEFAULT) != 0);
            } while(bExists);
            nSerChannelOffset--;
        }
        tid_spike = CreateSpike16Type(gid_channel, nSpikeLength);
        hid_t tid_chan_attr = CreateChanAttrType(gid_channel);
        hid_t tid_chanext_attr = CreateChanExtAttrType(gid_channel);
        hid_t tid_chanext1_attr = CreateChanExt1AttrType(gid_channel);
        tid_sampling_attr = CreateSamplingAttrType(gid_channel);
        tid_filt_attr = CreateFiltAttrType(gid_channel);
        for (int i = 0; i < cbNUM_ANALOG_CHANS; ++i)
        {
            if (chanAttr[i].id == 0)
                continue;
            char szNum[7];
            std::string strLabel = "channel";
            sprintf(szNum, "%05u", chanAttr[i].id + nChannelOffset);
            strLabel += szNum;
            hid_t gid = -1;
            if (H5Lexists(gid_channel, strLabel.c_str(), H5P_DEFAULT))
                gid = H5Gopen(gid_channel, strLabel.c_str(), H5P_DEFAULT);
            else
                gid = H5Gcreate(gid_channel, strLabel.c_str(), H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);

            // Basic channel attributes
            if (!H5Aexists(gid, "BmiChan"))
            {
                hid_t aid = H5Acreate(gid, "BmiChan", tid_chan_attr, space_attr, H5P_DEFAULT, H5P_DEFAULT);
                ret = H5Awrite(aid, tid_chan_attr, &chanAttr[i]);
                ret = H5Aclose(aid);
            }

            // Extra channel attributes
            if (!H5Aexists(gid, "BmiChanExt"))
            {
                hid_t aid = H5Acreate(gid, "BmiChanExt", tid_chanext_attr, space_attr, H5P_DEFAULT, H5P_DEFAULT);
                ret = H5Awrite(aid, tid_chanext_attr, &chanExtAttr[i]);
                ret = H5Aclose(aid);
            }

            // Additional extra channel attributes
            if (!H5Aexists(gid, "BmiChanExt1"))
            {
                hid_t aid = H5Acreate(gid, "BmiChanExt1", tid_chanext1_attr, space_attr, H5P_DEFAULT, H5P_DEFAULT);
                ret = H5Awrite(aid, tid_chanext1_attr, &chanExt1Attr[i]);
                ret = H5Aclose(aid);
            }

            ret = H5Gclose(gid);
        }
        ret = H5Tclose(tid_chanext1_attr);
        ret = H5Tclose(tid_chanext_attr);

        // Add digital and serial channel and their attributes
        {
            UINT16 id = 1 + 0;
            char szNum[7];
            std::string strLabel = "digital";
            sprintf(szNum, "%05u", id + nDigChannelOffset);
            strLabel += szNum;
            tid_dig = CreateDig16Type(gid_channel);
            hid_t gid = -1;
            if (H5Lexists(gid_channel, strLabel.c_str(), H5P_DEFAULT))
                gid = H5Gopen(gid_channel, strLabel.c_str(), H5P_DEFAULT);
            else
                gid = H5Gcreate(gid_channel, strLabel.c_str(), H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
            if (chanAttr[cbFIRST_DIGIN_CHAN].id)
            {
                if (!H5Aexists(gid, "BmiChan"))
                {
                    hid_t aid = H5Acreate(gid, "BmiChan", tid_chan_attr, space_attr, H5P_DEFAULT, H5P_DEFAULT);
                    ret = H5Awrite(aid, tid_chan_attr, &chanAttr[cbFIRST_DIGIN_CHAN]);
                    ret = H5Aclose(aid);
                }
            }
            ret = H5Gclose(gid);
        }

        // Add digital and serial channel and their attributes
        {
            UINT16 id = 1 + 0;
            char szNum[7];
            std::string strLabel = "serial";
            sprintf(szNum, "%05u", id + nSerChannelOffset);
            strLabel += szNum;
            hid_t gid = -1;
            if (H5Lexists(gid_channel, strLabel.c_str(), H5P_DEFAULT))
                gid = H5Gopen(gid_channel, strLabel.c_str(), H5P_DEFAULT);
            else
                gid = H5Gcreate(gid_channel, strLabel.c_str(), H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
            if (chanAttr[cbFIRST_SERIAL_CHAN].id)
            {
                if (!H5Aexists(gid, "BmiChan"))
                {
                    hid_t aid = H5Acreate(gid, "BmiChan", tid_chan_attr, space_attr, H5P_DEFAULT, H5P_DEFAULT);
                    ret = H5Awrite(aid, tid_chan_attr, &chanAttr[cbFIRST_SERIAL_CHAN]);
                    ret = H5Aclose(aid);
                }
            }
            ret = H5Gclose(gid);
        }
        ret = H5Tclose(tid_chan_attr);

        ret = H5Gclose(gid_channel);
    }

    bool bHasVideo = false;
    // Add video group
    if (nVer >= 23)
    {
        hid_t gid_video = -1;
        if (H5Lexists(file, "video", H5P_DEFAULT))
            gid_video = H5Gopen(file, "video", H5P_DEFAULT);
        else
            gid_video = H5Gcreate(file, "video", H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
        hid_t tid_tracking_attr = CreateTrackingAttrType(gid_video);
        hid_t tid_synch_attr = CreateSynchAttrType(gid_video);
        tid_synch = CreateSynchType(gid_video);
        // Add synchronization groups
        if (synchAttr.szLabel != NULL)
        {
            bHasVideo = true;
            char szNum[7];
            std::string strLabel = "synch";
            sprintf(szNum, "%05u", synchAttr.id);
            strLabel += szNum;
            hid_t gid = -1;
            if (H5Lexists(gid_video, strLabel.c_str(), H5P_DEFAULT))
                gid = H5Gopen(gid_video, strLabel.c_str(), H5P_DEFAULT);
            else
                gid = H5Gcreate(gid_video, strLabel.c_str(), H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
            if (!H5Aexists(gid, "BmiSynch"))
            {
                hid_t aid = H5Acreate(gid, "BmiSynch", tid_synch_attr, space_attr, H5P_DEFAULT, H5P_DEFAULT);
                ret = H5Awrite(aid, tid_synch_attr, &synchAttr);
                ret = H5Aclose(aid);
                ret = H5Gclose(gid);
            }
        }
        // Add tracking groups
        for (int i = 0; i < cbMAXTRACKOBJ; ++i)
        {
            tid_tracking[i] = -1;
            if (trackingAttr[i].szLabel != NULL)
            {
                bHasVideo = true;
                int dim = 2, width = 2;
                switch (trackingAttr[i].type)
                {
                case cbTRACKOBJ_TYPE_2DMARKERS:
                case cbTRACKOBJ_TYPE_2DBLOB:
                case cbTRACKOBJ_TYPE_2DBOUNDARY:
                    dim = 2;
                    width = 2;
                    break;
                case cbTRACKOBJ_TYPE_3DMARKERS:
                    dim = 3;
                    width = 2;
                    break;
                case cbTRACKOBJ_TYPE_1DSIZE:
                    dim = 1;
                    width = 4;
                    break;
                default:
                    // The defualt is already set
                    break;
                }
                // The only fixed length now is the case for single point tracking
                tid_tracking[i] = CreateTrackingType(gid_video, dim, width);
                size_tracking[i] = dim * width; // Size of each tracking element in bytes
                char szNum[7];
                std::string strLabel = "tracking";
                sprintf(szNum, "%05u", trackingAttr[i].trackID);
                strLabel += szNum;

                hid_t gid = -1;
                if (H5Lexists(gid_video, strLabel.c_str(), H5P_DEFAULT))
                    gid = H5Gopen(gid_video, strLabel.c_str(), H5P_DEFAULT);
                else
                    gid = H5Gcreate(gid_video, strLabel.c_str(), H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);

                if (!H5Aexists(gid, "BmiTracking"))
                {
                    hid_t aid = H5Acreate(gid, "BmiTracking", tid_tracking_attr, space_attr, H5P_DEFAULT, H5P_DEFAULT);
                    ret = H5Awrite(aid, tid_tracking_attr, &trackingAttr[i]);
                    ret = H5Aclose(aid);
                    ret = H5Gclose(gid);
                }
            } // end if (trackingAttr[i].szLabel
        } // end for (int i = 0
        ret = H5Tclose(tid_tracking_attr);
        ret = H5Tclose(tid_synch_attr);
        ret = H5Gclose(gid_video);
    }
    // Comment group
    if (nVer >= 23)
    {
        hid_t gid_comment = H5Gcreate(file, "comment", H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
        tid_comment = CreateCommentType(gid_comment);
        {
            // NeuroMotive charset is fixed
            hid_t aid = H5Acreate(gid_comment, "NeuroMotiveCharset", H5T_NATIVE_UINT8, space_attr, H5P_DEFAULT, H5P_DEFAULT);
            UINT8 charset = 255;
            ret = H5Awrite(aid, H5T_NATIVE_UINT8, &charset);
            ret = H5Aclose(aid);

            hid_t gid;
            if (bHasVideo)
            {
                gid = H5Gcreate(gid_comment, "comment00256", H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
                aid = H5Acreate(gid, "Charset", H5T_NATIVE_UINT8, space_attr, H5P_DEFAULT, H5P_DEFAULT);
                ret = H5Awrite(aid, H5T_NATIVE_UINT8, &charset);
                ret = H5Aclose(aid);
                ret = H5Gclose(gid);
            }
            charset = 0; // Add group for normal comments right here
            gid = H5Gcreate(gid_comment, "comment00001", H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
            aid = H5Acreate(gid, "Charset", H5T_NATIVE_UINT8, space_attr, H5P_DEFAULT, H5P_DEFAULT);
            ret = H5Awrite(aid, H5T_NATIVE_UINT8, &charset);
            ret = H5Aclose(aid);
            ret = H5Gclose(gid);
        }
        ret = H5Gclose(gid_comment);
    }
    ret = H5Sclose(space_attr);

    // ---------------------------------------------------------------------------------------------
    // Done with reading headers
    // Add packet tables
    {
        size_t chunk_size = CHUNK_SIZE_EVENT;
        int compression = -1; // TODO: use options to add compression
        hid_t ptid_spike[cbNUM_ANALOG_CHANS];
        for (int i = 0; i < cbNUM_ANALOG_CHANS; ++i)
            ptid_spike[i] = -1;
        hid_t ptid_serial = -1, ptid_digital = -1, ptid_synch = -1;
        hid_t ptid_comment[256];
        for (int i = 0; i < 256; ++i)
            ptid_comment[i] = -1;
        hid_t ptid_tracking[cbMAXTRACKOBJ];
        for (int i = 0; i < cbMAXTRACKOBJ; ++i)
            ptid_tracking[i] = -1;
        NevData nevData;
        fseeko(pFile, isHdr.dwStartOfData, SEEK_SET);
        size_t nGot = fread(&nevData, isHdr.dwBytesPerPacket, 1, pFile);
        if (nGot != 1)
        {
            perror("Source file is empty or invalid\n");
            return 1;
        }
        do {

            if (nevData.wPacketID >= 1 && nevData.wPacketID <= cbNUM_ANALOG_CHANS) // found spike data
            {
                if (!g_bNoSpikes)
                {
                    int id = nevData.wPacketID; // 1-based
                    if (ptid_spike[id - 1] < 0)
                    {
                        char szNum[7];
                        std::string strLabel = "/channel/channel";
                        sprintf(szNum, "%05u", id + nChannelOffset);
                        strLabel += szNum;
                        hid_t gid;
                        if(H5Lexists(file, strLabel.c_str(), H5P_DEFAULT))
                        {
                            gid = H5Gopen(file, strLabel.c_str(), H5P_DEFAULT);
                        } else {
                            printf("Creating %s without attributes\n", strLabel.c_str());
                            gid = H5Gcreate(file, strLabel.c_str(), H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
                        }
                        ptid_spike[id - 1] = H5PTcreate_fl(gid, "spike_set", tid_spike, chunk_size, compression);
                        hid_t dsid = H5Dopen(gid, "spike_set", H5P_DEFAULT);
                        ret = H5Gclose(gid);
                        hid_t space_attr = H5Screate_simple(1, dims, NULL);

                        // Add data sampling attribute
                        hid_t aid = H5Acreate(dsid, "Sampling", tid_sampling_attr, space_attr, H5P_DEFAULT, H5P_DEFAULT);
                        ret = H5Awrite(aid, tid_sampling_attr, &samplingAttr[id - 1]);
                        ret = H5Aclose(aid);
                        // Add data filtering attribute
                        aid = H5Acreate(dsid, "Filter", tid_filt_attr, space_attr, H5P_DEFAULT, H5P_DEFAULT);
                        ret = H5Awrite(aid, tid_filt_attr, &filtAttr[id - 1]);
                        ret = H5Aclose(aid);

                        ret = H5Sclose(space_attr);
                    }
                    BmiSpike16_t spk;
                    spk.dwTimestamp = nevData.dwTimestamp;
                    spk.res = nevData.spike.res;
                    spk.unit = nevData.spike.unit;
                    for (int i = 0; i < nSpikeLength; ++i)
                        spk.wave[i] = nevData.spike.wave[i];
                    ret = H5PTappend(ptid_spike[id - 1], 1, &spk);
                } // end if (!g_bNoSpikes
            } else {
                switch (nevData.wPacketID)
                {
                case 0:      // found a digital or serial event
                    if (!(nevData.digital.byInsertionReason & 1))
                    {
                        // Other digital events are not implemented in NSP yet
                        printf("Unkown digital event (%u) dropped\n", nevData.digital.byInsertionReason);
                        break;
                    }
                    if (nevData.digital.byInsertionReason & 128) // If bit 7 is set it is serial
                    {
                        if (ptid_serial < 0)
                        {
                            UINT16 id = 1 + 0;
                            char szNum[7];
                            std::string strLabel = "/channel/serial";
                            sprintf(szNum, "%05u", id + nSerChannelOffset);
                            strLabel += szNum;
                            hid_t gid = H5Gopen(file, strLabel.c_str(), H5P_DEFAULT);
                            ptid_serial = H5PTcreate_fl(gid, "serial_set", tid_dig, chunk_size, compression);
                            H5Gclose(gid);
                        }
                        BmiDig16_t dig;
                        dig.dwTimestamp = nevData.dwTimestamp;
                        dig.value = nevData.digital.wDigitalValue;
                        ret = H5PTappend(ptid_serial, 1, &dig);
                    } else {
                        if (ptid_digital < 0)
                        {
                            UINT16 id = 1 + 0;
                            char szNum[7];
                            std::string strLabel = "/channel/digital";
                            sprintf(szNum, "%05u", id + nDigChannelOffset);
                            strLabel += szNum;
                            hid_t gid = H5Gopen(file, strLabel.c_str(), H5P_DEFAULT);
                            ptid_digital = H5PTcreate_fl(gid, "digital_set", tid_dig, chunk_size, compression);
                            H5Gclose(gid);
                        }
                        BmiDig16_t dig;
                        dig.dwTimestamp = nevData.dwTimestamp;
                        dig.value = nevData.digital.wDigitalValue;
                        ret = H5PTappend(ptid_digital, 1, &dig);
                    }
                    break;
                case 0xFFFF: // found a comment event
                    {
                        int id = nevData.comment.charset; // 0-based
                        if (ptid_comment[id] < 0)
                        {
                            char szNum[7];
                            std::string strLabel = "/comment/comment";
                            sprintf(szNum, "%05u", id + 1);
                            strLabel += szNum;
                            hid_t gid;
                            if(H5Lexists(file, strLabel.c_str(), H5P_DEFAULT))
                            {
                                gid = H5Gopen(file, strLabel.c_str(), H5P_DEFAULT);
                            } else {
                                gid = H5Gcreate(file, strLabel.c_str(), H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
                                {
                                    hsize_t     dims[1] = {1};
                                    hid_t space_attr = H5Screate_simple(1, dims, NULL);
                                    hid_t aid = H5Acreate(gid, "Charset", H5T_NATIVE_UINT8, space_attr, H5P_DEFAULT, H5P_DEFAULT);
                                    UINT8 charset = nevData.comment.charset;
                                    ret = H5Awrite(aid, H5T_NATIVE_UINT8, &charset);
                                    ret = H5Aclose(aid);
                                    ret = H5Sclose(space_attr);
                                }
                            }

                            ptid_comment[id] = H5PTcreate_fl(gid, "comment_set", tid_comment, chunk_size, compression);
                            H5Gclose(gid);
                        }
                        BmiComment_t cmt;
                        cmt.dwTimestamp = nevData.dwTimestamp;
                        cmt.data = nevData.comment.data;
                        cmt.flags = nevData.comment.flags;
                        strncpy(cmt.szComment, nevData.comment.comment, min(BMI_COMMENT_LEN, sizeof(nevData.comment.comment)));
                        ret = H5PTappend(ptid_comment[id], 1, &cmt);
                    }                    
                    break;
                case 0xFFFE: // found a synchronization event
                    {
                        int id = nevData.synch.id; // 0-based
                        if (id != 0)
                        {
                            printf("Unsupported synchronization source dropped\n");
                            break;
                        }
                        if (ptid_synch < 0)
                        {
                            char szNum[7];
                            std::string strLabel = "/video/synch";
                            sprintf(szNum, "%05u", id + 1);
                            strLabel += szNum;
                            hid_t gid;
                            if(H5Lexists(file, strLabel.c_str(), H5P_DEFAULT))
                            {
                                gid = H5Gopen(file, strLabel.c_str(), H5P_DEFAULT);
                            } else {
                                printf("Creating %s without attributes\n", strLabel.c_str());
                                gid = H5Gcreate(file, strLabel.c_str(), H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
                            }
                            ptid_synch = H5PTcreate_fl(gid, "synch_set", tid_synch, chunk_size, compression);
                            H5Gclose(gid);
                        }
                        BmiSynch_t synch;
                        synch.dwTimestamp = nevData.dwTimestamp;
                        synch.etime = nevData.synch.etime;
                        synch.frame = nevData.synch.frame;
                        synch.split = nevData.synch.split;
                        ret = H5PTappend(ptid_synch, 1, &synch);
                    }
                    break;
                case 0xFFFD: // found a video tracking event
                    {
                        int id = nevData.track.nodeID; // 0-based
                        if (id >= cbMAXTRACKOBJ)
                        {
                            printf("Invalid tracking packet dropped\n");
                            break;
                        }
                        if (ptid_tracking[id] < 0)
                        {
                            char szNum[7];
                            std::string strLabel = "/video/tracking";
                            sprintf(szNum, "%05u", id + 1);
                            strLabel += szNum;
                            hid_t gid;
                            if(H5Lexists(file, strLabel.c_str(), H5P_DEFAULT))
                            {
                                gid = H5Gopen(file, strLabel.c_str(), H5P_DEFAULT);
                            } else {
                                printf("Creating %s without attributes\n", strLabel.c_str());
                                gid = H5Gcreate(file, strLabel.c_str(), H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
                            }
                            if (tid_tracking[id] < 0)
                            {
                                printf("Creating tracking set with undefined type\n");
                                hid_t gid_video = H5Gopen(file, "/video", H5P_DEFAULT);
                                tid_tracking[id] = CreateTrackingType(gid_video, 2, 2);
                                ret = H5Gclose(gid_video);
                            }
                            ptid_tracking[id] = H5PTcreate_fl(gid, "tracking_set", tid_tracking[id], chunk_size, compression);
                            H5Gclose(gid);
                        }

                        // Flatten tracking array
                        BmiTracking_fl_t tr;
                        tr.dwTimestamp = nevData.dwTimestamp;
                        tr.nodeCount = nevData.track.nodeCount;
                        tr.parentID = nevData.track.parentID;
                        if (size_tracking[id] > 0)
                        {
                            for (int i = 0; i < nevData.track.coordsLength; ++i)
                            {
                                memcpy(tr.coords, (char *)&nevData.track.coords[0] + i * size_tracking[id], size_tracking[id]);
                                ret = H5PTappend(ptid_tracking[id], 1, &tr);
                            }
                        }
                    }
                    break;
                default:
                    if (nevData.wPacketID <= 2048)
                        printf("Unexpected spike channel (%u) dropped\n", nevData.wPacketID);
                    else
                        printf("Unknown packet type (%u) dropped\n", nevData.wPacketID);
                    break;
                }
            }
            // Read more packets
            nGot = fread(&nevData, isHdr.dwBytesPerPacket, 1, pFile);
        } while (nGot == 1);
    }

    //
    // We are going to call H5Close so no need to close what is open at this stage
    //
    return 0;
}

// Author & Date:   Ehsan Azar   Nov 17, 2012
// Purpose: Convert NSx2.1
// Inputs:
//  szSrcFile - source file name
//  pFile     - the source file
//  file      - the destination file
// Outputs:
//   Returns 0 on success, error code otherwise
int ConvertNSx21(const char * szSrcFile, FILE * pFile, hid_t file)
{
    herr_t ret;
    Nsx21Hdr isHdr;
    // Read the header
    fseeko(pFile, 0, SEEK_SET);    // read header from beginning of file
    fread(&isHdr, sizeof(isHdr), 1, pFile);
    
    if (isHdr.cnChannels > cbNUM_ANALOG_CHANS)
    {
        printf("Invalid number of channels in source file header\n");
        return 1;
    }

    BmiChanAttr_t chanAttr[cbNUM_ANALOG_CHANS];
    memset(chanAttr, 0, sizeof(chanAttr));
    BmiSamplingAttr_t samplingAttr[cbNUM_ANALOG_CHANS];
    memset(samplingAttr, 0, sizeof(samplingAttr));

    // Add root attribute
    if (AddRoot(szSrcFile, pFile, file, isHdr))
        return 1;

    hid_t ptid_chan[cbNUM_ANALOG_CHANS];
    {
        size_t chunk_size = CHUNK_SIZE_CONTINUOUS;
        int compression = -1; // TODO: use options to add compression

        hsize_t     dims[1] = {1};
        hid_t space_attr = H5Screate_simple(1, dims, NULL);

        hid_t gid_channel = -1;
        if (H5Lexists(file, "channel", H5P_DEFAULT))
            gid_channel = H5Gopen(file, "channel", H5P_DEFAULT);
        else
            gid_channel = H5Gcreate(file, "channel", H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
        hid_t tid_chan_attr = CreateChanAttrType(gid_channel);
        hid_t tid_sampling_attr = CreateSamplingAttrType(gid_channel);
        for (UINT32 i = 0; i < isHdr.cnChannels; ++i)
        {
            char szNum[7];
            UINT32 id; // 1-based
            if (fread(&id, sizeof(UINT32), 1, pFile) != 1)
            {
                printf("Invalid header in source file\n");
                return 1;
            }
            chanAttr[i].id = id;
            std::string strLabel = "chan";
            sprintf(szNum, "%u", id);
            strLabel += szNum;
            chanAttr[i].szLabel = _strdup(strLabel.c_str());
            samplingAttr[i].fClock = 30000;
            // FIXME: This might be incorrect for really old file recordings
            // TODO: search the file to see if 14 is more accurate
            samplingAttr[i].nSampleBits = 16;
            samplingAttr[i].fSampleRate = float(30000.0) / isHdr.nPeriod;

            UINT32 nChannelOffset = 0;
            if (g_bAppend)
            {
                bool bExists = false;
                // Find the last place to append to
                do {
                    nChannelOffset++;
                    std::string strLabel = "channel";
                    char szNum[7];
                    sprintf(szNum, "%05u", nChannelOffset);
                    strLabel += szNum;
                    bExists = (H5Lexists(gid_channel, strLabel.c_str(), H5P_DEFAULT) != 0);
                } while(bExists);
                nChannelOffset--;
            }

            strLabel = "channel";
            sprintf(szNum, "%05u", id + nChannelOffset);
            strLabel += szNum;
            hid_t gid = -1;
            if (H5Lexists(gid_channel, strLabel.c_str(), H5P_DEFAULT))
                gid = H5Gopen(gid_channel, strLabel.c_str(), H5P_DEFAULT);
            else
                gid = H5Gcreate(gid_channel, strLabel.c_str(), H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);

            // Basic channel attributes
            hid_t aid = H5Acreate(gid, "BmiChan", tid_chan_attr, space_attr, H5P_DEFAULT, H5P_DEFAULT);
            ret = H5Awrite(aid, tid_chan_attr, &chanAttr[i]);
            ret = H5Aclose(aid);

            // If need to go one level deeper
            if (g_nCombine > 0)
            {
                hid_t gidParent = gid;
                sprintf(szNum, "%05u", g_nCombine);
                strLabel = szNum;
                if (H5Lexists(gidParent, strLabel.c_str(), H5P_DEFAULT))
                    gid = H5Gopen(gidParent, strLabel.c_str(), H5P_DEFAULT);
                else
                    gid = H5Gcreate(gidParent, strLabel.c_str(), H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
                ret = H5Gclose(gidParent);
            }

            ptid_chan[i] = H5PTcreate_fl(gid, "continuous_set", H5T_NATIVE_INT16, chunk_size, compression);

            hid_t dsid = H5Dopen(gid, "continuous_set", H5P_DEFAULT);
            ret = H5Gclose(gid);
            // Add data start clock attribute
            aid = H5Acreate(dsid, "StartClock", H5T_NATIVE_UINT32, space_attr, H5P_DEFAULT, H5P_DEFAULT);
            UINT32 nStartTime = 0; // 2.1 does not have paused headers
            ret = H5Awrite(aid, H5T_NATIVE_UINT32, &nStartTime);
            ret = H5Aclose(aid);
            // Add data sampling attribute
            aid = H5Acreate(dsid, "Sampling", tid_sampling_attr, space_attr, H5P_DEFAULT, H5P_DEFAULT);
            ret = H5Awrite(aid, tid_sampling_attr, &samplingAttr[i]);
            ret = H5Aclose(aid);

            ret = H5Dclose(dsid);
        } // end for (UINT32 i = 0
        ret = H5Tclose(tid_sampling_attr);
        ret = H5Tclose(tid_chan_attr);
        ret = H5Gclose(gid_channel);
        ret = H5Sclose(space_attr);
    }

    int count = 0;
    INT16 anDataBufferCache[cbNUM_ANALOG_CHANS][CHUNK_SIZE_CONTINUOUS];
    INT16 anDataBuffer[cbNUM_ANALOG_CHANS];
    size_t nGot = fread(anDataBuffer, sizeof(INT16), isHdr.cnChannels, pFile);
    if (nGot != isHdr.cnChannels)
    {
        perror("Source file is empty or invalid\n");
        return 1;
    }
    do {
        for (UINT32 i = 0; i < isHdr.cnChannels; ++i)
        {
            anDataBufferCache[i][count] = anDataBuffer[i];
        }
        count++;
        if (count == CHUNK_SIZE_CONTINUOUS)
        {
            for (UINT32 i = 0; i < isHdr.cnChannels; ++i)
            {
                ret = H5PTappend(ptid_chan[i], count, &anDataBufferCache[i][0]);
            }
            count = 0;
        }
        nGot = fread(anDataBuffer, sizeof(INT16), isHdr.cnChannels, pFile);
    } while (nGot == isHdr.cnChannels);

    // Write out the remaining chunk
    if (count > 0)
    {
        for (UINT32 i = 0; i < isHdr.cnChannels; ++i)
        {
            ret = H5PTappend(ptid_chan[i], count, &anDataBufferCache[i][0]);
        }
    }

    //
    // We are going to call H5Close so no need to close what is open at this stage
    //
    return 0;
}

// Author & Date:   Ehsan Azar   Nov 17, 2012
// Purpose: Convert NSx2.2
// Inputs:
//  pFile - the source file
//  file  - the destination file
// Outputs:
//   Returns 0 on success, error code otherwise
int ConvertNSx22(FILE * pFile, hid_t file)
{
    herr_t ret;
    Nsx22Hdr isHdr;
    // Read the header
    fseeko(pFile, 0, SEEK_SET);    // read header from beginning of file
    fread(&isHdr, sizeof(isHdr), 1, pFile);
    UINT64 dataStart = isHdr.nBytesInHdrs + sizeof(Nsx22DataHdr);

    if (isHdr.cnChannels > cbNUM_ANALOG_CHANS)
    {
        printf("Invalid number of channels in source file header\n");
        return 1;
    }

    BmiFiltAttr_t filtAttr[cbNUM_ANALOG_CHANS];
    memset(filtAttr, 0, sizeof(filtAttr));
    BmiSamplingAttr_t samplingAttr[cbNUM_ANALOG_CHANS];
    memset(samplingAttr, 0, sizeof(samplingAttr));
    BmiChanAttr_t chanAttr[cbNUM_ANALOG_CHANS];
    memset(chanAttr, 0, sizeof(chanAttr));
    BmiChanExtAttr_t chanExtAttr[cbNUM_ANALOG_CHANS];
    memset(chanExtAttr, 0, sizeof(chanExtAttr));
    BmiChanExt2Attr_t chanExt2Attr[cbNUM_ANALOG_CHANS];
    memset(chanExt2Attr, 0, sizeof(chanExt2Attr));

    // Add root attribute
    if (AddRoot(pFile, file, isHdr))
        return 1;

    // Read extra headers
    for (UINT32 i = 0; i < isHdr.cnChannels; ++i)
    {
        Nsx22ExtHdr isExtHdr;
        if (fread(&isExtHdr, sizeof(isExtHdr), 1, pFile) != 1)
        {
            printf("Invalid source file header\n");
            return 1;
        }
        if (0 != strncmp(isExtHdr.achExtHdrID, "CC", sizeof(isExtHdr.achExtHdrID)))
        {
            printf("Invalid source file extended header\n");
            return 1;
        }
        int id = isExtHdr.id;
        if (id == 0 || id > cbNUM_ANALOG_CHANS)
        {
            printf("Invalid channel ID in source file header\n");
            return 1;
        }
        chanAttr[i].id = isExtHdr.id;
        samplingAttr[i].fClock = float(isHdr.nResolution);
        samplingAttr[i].fSampleRate = float(isHdr.nResolution) / float(isHdr.nPeriod);
        samplingAttr[i].nSampleBits = 16;

        chanExtAttr[i].phys_connector = isExtHdr.phys_connector;
        chanExtAttr[i].connector_pin = isExtHdr.connector_pin;
        UINT64 anarange = INT64(isExtHdr.anamax) - INT64(isExtHdr.anamin);
        UINT64 digrange = INT64(isExtHdr.digmax) - INT64(isExtHdr.digmin);
        if (strncmp(isExtHdr.anaunit, "uV", 2) == 0)
        {
            chanExtAttr[i].dFactor = UINT32((anarange * INT64(1E3)) / digrange);
        }
        else if (strncmp(isExtHdr.anaunit, "mV", 2) == 0)
        {
            chanExtAttr[i].dFactor = UINT32((anarange * INT64(1E6)) / digrange);
        }
        else if (strncmp(isExtHdr.anaunit, "V", 2) == 0)
        {
            chanExtAttr[i].dFactor = UINT32((anarange * INT64(1E9)) / digrange);
        } else {
            printf("Unknown analog unit for channel %u, uV used\n", isExtHdr.id);
            chanExtAttr[i].dFactor = UINT32((anarange * INT64(1E3)) / digrange);
        }
        filtAttr[i].hpfreq = isExtHdr.hpfreq;
        filtAttr[i].hporder = isExtHdr.hporder;
        filtAttr[i].hptype = isExtHdr.hptype;
        filtAttr[i].lpfreq = isExtHdr.lpfreq;
        filtAttr[i].lporder = isExtHdr.lporder;
        filtAttr[i].lptype = isExtHdr.lptype;
        chanAttr[i].szLabel = _strdup(isExtHdr.label);

        chanExt2Attr[i].anamax = isExtHdr.anamax;
        chanExt2Attr[i].anamin = isExtHdr.anamin;
        chanExt2Attr[i].digmax = isExtHdr.digmax;
        chanExt2Attr[i].digmin = isExtHdr.digmin;
        strncpy(chanExt2Attr[i].anaunit, isExtHdr.anaunit, 16);
    }

    hsize_t     dims[1] = {1};
    hid_t space_attr = H5Screate_simple(1, dims, NULL);
    hid_t tid_sampling_attr = -1;
    hid_t tid_filt_attr = -1;

    UINT32 nChannelOffset = 0;

    hid_t ptid_chan[cbNUM_ANALOG_CHANS];
    {
        hid_t gid_channel = -1;
        if (H5Lexists(file, "channel", H5P_DEFAULT))
            gid_channel = H5Gopen(file, "channel", H5P_DEFAULT);
        else
            gid_channel = H5Gcreate(file, "channel", H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
        
        if (g_bAppend)
        {
            bool bExists = false;
            // Find the last place to append to
            do {
                nChannelOffset++;
                std::string strLabel = "channel";
                char szNum[7];
                sprintf(szNum, "%05u", nChannelOffset);
                strLabel += szNum;
                bExists = (H5Lexists(gid_channel, strLabel.c_str(), H5P_DEFAULT) != 0);
            } while(bExists);
            nChannelOffset--;
        }

        tid_sampling_attr = CreateSamplingAttrType(gid_channel);
        tid_filt_attr = CreateFiltAttrType(gid_channel);
        hid_t tid_chan_attr = CreateChanAttrType(gid_channel);
        hid_t tid_chanext_attr = CreateChanExtAttrType(gid_channel);
        hid_t tid_chanext2_attr = CreateChanExt2AttrType(gid_channel);
        for (UINT32 i = 0; i < isHdr.cnChannels; ++i)
        {
            std::string strLabel = "channel";
            {
                UINT32 id = chanAttr[i].id + nChannelOffset;
                char szNum[7];
                sprintf(szNum, "%05u", id);
                strLabel += szNum;
            }
            hid_t gid = -1;
            if (H5Lexists(gid_channel, strLabel.c_str(), H5P_DEFAULT))
                gid = H5Gopen(gid_channel, strLabel.c_str(), H5P_DEFAULT);
            else
                gid = H5Gcreate(gid_channel, strLabel.c_str(), H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);

            // Basic channel attributes
            if (!H5Aexists(gid, "BmiChan"))
            {
                hid_t aid = H5Acreate(gid, "BmiChan", tid_chan_attr, space_attr, H5P_DEFAULT, H5P_DEFAULT);
                ret = H5Awrite(aid, tid_chan_attr, &chanAttr[i]);
                ret = H5Aclose(aid);
            }

            // Extra header attribute
            if (!H5Aexists(gid, "BmiChanExt"))
            {
                hid_t aid = H5Acreate(gid, "BmiChanExt", tid_chanext_attr, space_attr, H5P_DEFAULT, H5P_DEFAULT);
                ret = H5Awrite(aid, tid_chanext_attr, &chanExtAttr[i]);
                ret = H5Aclose(aid);
            }

            // Additional extra channel attributes
            if (!H5Aexists(gid, "BmiChanExt2"))
            {
                hid_t aid = H5Acreate(gid, "BmiChanExt2", tid_chanext2_attr, space_attr, H5P_DEFAULT, H5P_DEFAULT);
                ret = H5Awrite(aid, tid_chanext2_attr, &chanExt2Attr[i]);
                ret = H5Aclose(aid);
            }

            ret = H5Gclose(gid);
        } // end for (UINT32 i = 0
        ret = H5Tclose(tid_chanext2_attr);
        ret = H5Tclose(tid_chanext_attr);
        ret = H5Tclose(tid_chan_attr);
        ret = H5Gclose(gid_channel);
    }

    // Now read data
    fseeko(pFile, isHdr.nBytesInHdrs, SEEK_SET);
    Nsx22DataHdr isDataHdr;
    size_t nGot = fread(&isDataHdr, sizeof(Nsx22DataHdr), 1, pFile);
    int setCount = 0;
    if (nGot != 1)
    {
        printf("Invalid source file (cannot read data header)\n");
        return 1;
    }
    do {
        if (isDataHdr.nHdr != 1)
        {
            printf("Invalid data header in source file\n");
            break;
        }
        if (isDataHdr.nNumDatapoints == 0)
        {
            printf("Data section %d with zero points detected!\n", setCount);
            if (g_bSkipEmpty)
            {
                printf(" Skip this section and assume next in file is new data header\n");
                nGot = fread(&isDataHdr, sizeof(Nsx22DataHdr), 1, pFile);
                if (nGot != 1)
                    break;
                continue;
            } else {
                printf(" Retrieve the rest of the file as one chunk\n"
                        " Last section may have unaligned trailing data points\n"
                        " Use --skipempty if instead you want to skip empty headers\n");
            }
        }
        for (UINT32 i = 0; i < isHdr.cnChannels; ++i)
        {
            size_t chunk_size = CHUNK_SIZE_CONTINUOUS;
            int compression = -1; // TODO: use options to add compression
            char szNum[7];
            std::string strLabel = "/channel/channel";
            sprintf(szNum, "%05u", chanAttr[i].id + nChannelOffset);
            strLabel += szNum;
            hid_t gid = H5Gopen(file, strLabel.c_str(), H5P_DEFAULT);
            // If need to go one level deeper
            if (g_nCombine > 0)
            {
                hid_t gidParent = gid;
                sprintf(szNum, "%05u", g_nCombine);
                strLabel = szNum;
                if (H5Lexists(gidParent, strLabel.c_str(), H5P_DEFAULT))
                    gid = H5Gopen(gidParent, strLabel.c_str(), H5P_DEFAULT);
                else
                    gid = H5Gcreate(gidParent, strLabel.c_str(), H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
                ret = H5Gclose(gidParent);
            }
            strLabel = "continuous_set";
            if (setCount > 0)
            {
                sprintf(szNum, "%05u", setCount);
                strLabel += szNum;
            }
            // We want to keep all data sets
            while (H5Lexists(gid, strLabel.c_str(), H5P_DEFAULT))
            {
                setCount++;
                strLabel = "continuous_set";
                sprintf(szNum, "%05u", setCount);
                strLabel += szNum;
            }
            ptid_chan[i] = H5PTcreate_fl(gid, strLabel.c_str(), H5T_NATIVE_INT16, chunk_size, compression);

            hid_t dsid = H5Dopen(gid, strLabel.c_str(), H5P_DEFAULT);
            ret = H5Gclose(gid);
            // Add data start clock attribute
            hid_t aid = H5Acreate(dsid, "StartClock", H5T_NATIVE_UINT32, space_attr, H5P_DEFAULT, H5P_DEFAULT);
            UINT32 nStartTime = isDataHdr.nTimestamp;
            ret = H5Awrite(aid, H5T_NATIVE_UINT32, &nStartTime);
            ret = H5Aclose(aid);
            // Add data sampling attribute
            aid = H5Acreate(dsid, "Sampling", tid_sampling_attr, space_attr, H5P_DEFAULT, H5P_DEFAULT);
            ret = H5Awrite(aid, tid_sampling_attr, &samplingAttr[i]);
            ret = H5Aclose(aid);
            // Add data filtering attribute
            aid = H5Acreate(dsid, "Filter", tid_filt_attr, space_attr, H5P_DEFAULT, H5P_DEFAULT);
            ret = H5Awrite(aid, tid_filt_attr, &filtAttr[i]);
            ret = H5Aclose(aid);

            ret = H5Dclose(dsid);
        }
        int count = 0;
        INT16 anDataBufferCache[cbNUM_ANALOG_CHANS][CHUNK_SIZE_CONTINUOUS];
        for (UINT32 i = 0; i < isDataHdr.nNumDatapoints || isDataHdr.nNumDatapoints == 0; ++i)
        {
            INT16 anDataBuffer[cbNUM_ANALOG_CHANS];
            size_t nGot = fread(anDataBuffer, sizeof(INT16), isHdr.cnChannels, pFile);
            if (nGot != isHdr.cnChannels)
            {
                if (isDataHdr.nNumDatapoints == 0)
                    printf("Data section %d may be unaligned\n", setCount);
                else
                    printf("Fewer data points (%u) than specified in data header (%u) at the source file!\n", i + 1, isDataHdr.nNumDatapoints);
                break;
            }
            for (UINT32 j = 0; j < isHdr.cnChannels; ++j)
            {
                anDataBufferCache[j][count] = anDataBuffer[j];
            }
            count++;
            if (count == CHUNK_SIZE_CONTINUOUS)
            {
                for (UINT32 j = 0; j < isHdr.cnChannels; ++j)
                {
                    ret = H5PTappend(ptid_chan[j], count, &anDataBufferCache[j][0]);
                }
                count = 0;
            }
        } // end for (UINT32 i = 0

        // Write out the remaining chunk
        if (count > 0)
        {
            for (UINT32 i = 0; i < isHdr.cnChannels; ++i)
            {
                ret = H5PTappend(ptid_chan[i], count, &anDataBufferCache[i][0]);
            }
        }
        // Close packet tables as we may open them again for paused files
        for (UINT32 i = 0; i < isHdr.cnChannels; ++i)
        {
            ret = H5PTclose(ptid_chan[i]);
        }        
        // Read possiblly more data streams
        nGot = fread(&isDataHdr, sizeof(Nsx22DataHdr), 1, pFile);
        setCount++;
    } while (nGot == 1);

    //
    // We are going to call H5Close so no need to close what is open at this stage
    //
    return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////////
// Function main()
/////////////////////////////////////////////////////////////////////////////////////////////////
int main(int argc, char * const argv[])
{
    herr_t ret;
    int idxSrcFile = 1;
    bool bForce = false;
    bool bCache = true;
    bool bCombine = false;
    for (int i = 1; i < argc; ++i)
    {
        if (strcmp(argv[i], "--force") == 0)
        {
            bForce = true;
            idxSrcFile++;
        }
        else if (strcmp(argv[i], "--nocache") == 0)
        {
            bCache = false;
            idxSrcFile++;
        }
        else if (strcmp(argv[i], "--nospikes") == 0)
        {
            g_bNoSpikes = true;
            idxSrcFile++;
        }
        else if (strcmp(argv[i], "--skipempty") == 0)
        {
            g_bSkipEmpty = true;
            idxSrcFile++;
        }
        else if (strcmp(argv[i], "--append") == 0)
        {
            g_bAppend = true;
            idxSrcFile++;
        }
        else if (strcmp(argv[i], "--combine") == 0)
        {
            if (i + 1 >= argc || !isdigit(argv[i + 1][0]))
            {
                printf("Combine level not specified or is invalid\n");
                idxSrcFile = argc; // Just to show the usage
                break;
            }
            g_nCombine = atoi(argv[i + 1]);
            bCombine = true;
            idxSrcFile += 2;
            i++;
        }
        else if (strcmp(argv[i], "--group") == 0)
        {
            // TODO: implement
            printf("Group addition is not implemented in this version!\n");
            return 0;
        }
    }
    if (idxSrcFile >= argc)
    {
        printf("Blackrock file conversion utility (version 1.0)\n"
               "Usage: n2h5 [options] <srcfile> [<destfile>]\n"
               "Purpose: Converts srcfile to destfile\n"
               "Inputs:\n"
               "<srcfile>  - the file to convert from (nev or nsx format)\n"
               "<destfile> - the converted file to create (hdf5 format)\n"
               "             default is <srcfile>.bh5\n"
               "Options:\n"
               " --force    : overwrites the destination if it exists, create if not\n"
               " --nocache  : slower but results in smaller file size\n"
               " --nospikes : ignore spikes\n"
               " --skipempty: skip 0-sized headers (instead of ignoring them)\n"
               " --combine <level> : combine to the existing channels at given subchannel level (level 0 means no subchannel)\n"
               "    same experiment, same channels, different data sets (e.g. different sampling rates or filters)\n"
               " --append   : append channels to the end of current channels\n"
               "    same experiment, different channel (e.g. sync systems recording)\n"
               " --group    : add as new group\n"
               "    different experiments\n");
        return 0;
    }

    // TODO: implement --append for video and comment data
    // TODO: implement --group

    const char * szSrcFile = argv[idxSrcFile];
    std::string strDest;
    if ((idxSrcFile + 1) >= argc)
    {
        strDest = szSrcFile;
        strDest += ".bh5";
    } else {
        strDest = argv[idxSrcFile + 1];
    }
    const char * szDstFile = strDest.c_str();

    char achFileID[8];

    FILE * pFile = fopen(szSrcFile, "rb");
    if (pFile == NULL)
    {
        perror("Unable to open source file for reading");
        return 0;
    }

    if (H5open())
    {
        fclose(pFile);
        printf("cannot open hdf5 library\n");
        return 0;
    }

    hid_t file;
    hid_t facpl = H5P_DEFAULT;
    
    if (g_bAppend || bCombine)
    {
        // Open read-only just to validate destination file
        file = H5Fopen(szDstFile, H5F_ACC_RDONLY, H5P_DEFAULT);
        if (file < 0 && !bForce)
        {
            printf("Cannot append to the destination file or destiantion file does not exist\n"
                   "Use --force to to ignore this error\n");
            goto ErrHandle;
        }
        H5Fclose(file);
    }

    if (bCache)
    {
        double rdcc_w0 = 1; // We only write so this should work
        facpl = H5Pcreate(H5P_FILE_ACCESS);
        // Useful primes: 401 4049 404819
        ret = H5Pset_cache(facpl, 0, 404819, 4 * 1024 * CHUNK_SIZE_CONTINUOUS, rdcc_w0);
    }
    if (g_bAppend || bCombine)
    {
        file = H5Fopen(szDstFile, H5F_ACC_RDWR, H5P_DEFAULT);
    } else {
        file = H5Fcreate(szDstFile, bForce ? H5F_ACC_TRUNC : H5F_ACC_EXCL, H5P_DEFAULT, facpl);
    }
    if (facpl != H5P_DEFAULT)
        ret = H5Pclose(facpl);

    if (file < 0)
    {
        if (g_bAppend || bCombine)
            printf("Cannot open the destination file or destination file does not exist\n"
                   "Use --force to create new file\n");
        else
            printf("Cannot create destination file or destiantion file exists\n"
                   "Use --force to overwite the file\n");
        goto ErrHandle;
    }
    fread(&achFileID, sizeof(achFileID), 1, pFile);
    // NEV file
    if (0 == strncmp(achFileID, "NEURALEV", sizeof(achFileID)))
    {
        if (ConvertNev(pFile, file))
        {
            printf("Error in ConvertNev()\n");
            goto ErrHandle;
        }
    }
    // 2.1 filespec
    else if (0 == strncmp(achFileID, "NEURALSG", sizeof(achFileID)))
    {
        if (ConvertNSx21(szSrcFile, pFile, file))
        {
            printf("Error in ConvertNSx21()\n");
            goto ErrHandle;
        }
    }
    // 2.2 filespec
    else if (0 == strncmp(achFileID, "NEURALCD", sizeof(achFileID)))
    {
        if (ConvertNSx22(pFile, file))
        {
            printf("Error in ConvertNSx22()\n");
            goto ErrHandle;
        }
    } else {
        printf("Invalid source file format\n");
    }
ErrHandle:
    if (pFile)
        fclose(pFile);
    if (file > 0)
        H5Fclose(file);
    H5close();
    return 0;
}
