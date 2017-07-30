//////////////////////////////////////////////////////////////////////////////
//
// (c) Copyright 2012 Blackrock Microsystems
//
// $Workfile: n2h5.c $
// $Archive: /n2h5/n2h5.c $
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
// Implementation of our types in terms of hdf5 types
//
// Note: all the types are committed to the most immediate parent group
//       while we try to keep the types in different version the same, 
//       one should try to base reading on the field names rather than types
//
//////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"

#include "n2h5.h"

// Author & Date:   Ehsan Azar   Nov 13, 2012
// Purpose: Create type for BmiFiltAttr_t and commit
// Inputs:
//  loc  - where to add the type
// Outputs:
//   Returns the type id
hid_t CreateFiltAttrType(hid_t loc)
{
    herr_t ret;
    hid_t tid = -1;
    std::string strLink = "BmiFiltAttr_t";
    // If already there return it
    if(H5Lexists(loc, strLink.c_str(), H5P_DEFAULT))
    {
        tid = H5Topen(loc, strLink.c_str(), H5P_DEFAULT);
        return tid;
    }
    tid = H5Tcreate(H5T_COMPOUND, sizeof(BmiFiltAttr_t));
    ret = H5Tinsert(tid, "HighPassFreq", offsetof(BmiFiltAttr_t, hpfreq), H5T_NATIVE_UINT32);
    ret = H5Tinsert(tid, "HighPassOrder", offsetof(BmiFiltAttr_t, hporder), H5T_NATIVE_UINT32);
    ret = H5Tinsert(tid, "HighPassType", offsetof(BmiFiltAttr_t, hptype), H5T_NATIVE_UINT16);
    ret = H5Tinsert(tid, "LowPassFreq", offsetof(BmiFiltAttr_t, lpfreq), H5T_NATIVE_UINT32);
    ret = H5Tinsert(tid, "LowPassOrder", offsetof(BmiFiltAttr_t, lporder), H5T_NATIVE_UINT32);
    ret = H5Tinsert(tid, "LowPassType", offsetof(BmiFiltAttr_t, lptype), H5T_NATIVE_UINT16);

    ret = H5Tcommit(loc, strLink.c_str(), tid, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);

    return tid;
}

// Author & Date:   Ehsan Azar   Nov 13, 2012
// Purpose: Create type for BmiChanExtAttr_t and commit
// Inputs:
//  loc  - where to add the type
// Outputs:
//   Returns the type id
hid_t CreateChanExtAttrType(hid_t loc)
{
    herr_t ret;
    hid_t tid = -1;
    std::string strLink = "BmiChanExtAttr_t";
    // If already there return it
    if(H5Lexists(loc, strLink.c_str(), H5P_DEFAULT))
    {
        tid = H5Topen(loc, strLink.c_str(), H5P_DEFAULT);
        return tid;
    }

    tid = H5Tcreate(H5T_COMPOUND, sizeof(BmiChanExtAttr_t));
    ret = H5Tinsert(tid, "NanoVoltsPerLSB", offsetof(BmiChanExtAttr_t, dFactor), H5T_NATIVE_DOUBLE);
    ret = H5Tinsert(tid, "PhysicalConnector", offsetof(BmiChanExtAttr_t, phys_connector), H5T_NATIVE_UINT8);
    ret = H5Tinsert(tid, "ConnectorPin", offsetof(BmiChanExtAttr_t, connector_pin), H5T_NATIVE_UINT8);

    ret = H5Tcommit(loc, strLink.c_str(), tid, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);

    return tid;
}

// Author & Date:   Ehsan Azar   Nov 13, 2012
// Purpose: Create type for BmiChanExt1Attr_t and commit
// Inputs:
//  loc  - where to add the type
// Outputs:
//   Returns the type id
hid_t CreateChanExt1AttrType(hid_t loc)
{
    herr_t ret;
    hid_t tid = -1;
    std::string strLink = "BmiChanExt1Attr_t";
    // If already there return it
    if(H5Lexists(loc, strLink.c_str(), H5P_DEFAULT))
    {
        tid = H5Topen(loc, strLink.c_str(), H5P_DEFAULT);
        return tid;
    }
    tid = H5Tcreate(H5T_COMPOUND, sizeof(BmiChanExt1Attr_t));
    ret = H5Tinsert(tid, "SortCount", offsetof(BmiChanExt1Attr_t, sortCount), H5T_NATIVE_UINT8);
    ret = H5Tinsert(tid, "EnergyThreshold", offsetof(BmiChanExt1Attr_t, energy_thresh), H5T_NATIVE_UINT32);
    ret = H5Tinsert(tid, "HighThreshold", offsetof(BmiChanExt1Attr_t, high_thresh), H5T_NATIVE_INT32);
    ret = H5Tinsert(tid, "LowThreshold", offsetof(BmiChanExt1Attr_t, low_thresh), H5T_NATIVE_INT32);

    ret = H5Tcommit(loc, strLink.c_str(), tid, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);

    return tid;
}

// Author & Date:   Ehsan Azar   Nov 13, 2012
// Purpose: Create type for BmiChanExt2Attr_t and commit
// Inputs:
//  loc  - where to add the type
// Outputs:
//   Returns the type id
hid_t CreateChanExt2AttrType(hid_t loc)
{
    herr_t ret;
    hid_t tid = -1;
    std::string strLink = "BmiChanExt2Attr_t";
    // If already there return it
    if(H5Lexists(loc, strLink.c_str(), H5P_DEFAULT))
    {
        tid = H5Topen(loc, strLink.c_str(), H5P_DEFAULT);
        return tid;
    }
    hid_t tid_attr_unit_str = H5Tcopy(H5T_C_S1);
    ret = H5Tset_size(tid_attr_unit_str, 16);

    tid = H5Tcreate(H5T_COMPOUND, sizeof(BmiChanExt2Attr_t));
    ret = H5Tinsert(tid, "DigitalMin", offsetof(BmiChanExt2Attr_t, digmin), H5T_NATIVE_INT32);
    ret = H5Tinsert(tid, "DigitalMax", offsetof(BmiChanExt2Attr_t, digmax), H5T_NATIVE_INT32);
    ret = H5Tinsert(tid, "AnalogMin", offsetof(BmiChanExt2Attr_t, anamin), H5T_NATIVE_INT32);
    ret = H5Tinsert(tid, "AnalogMax", offsetof(BmiChanExt2Attr_t, anamax), H5T_NATIVE_INT32);
    ret = H5Tinsert(tid, "AnalogUnit", offsetof(BmiChanExt2Attr_t, anaunit), tid_attr_unit_str);

    ret = H5Tcommit(loc, strLink.c_str(), tid, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);

    H5Tclose(tid_attr_unit_str);

    return tid;
}

// Author & Date:   Ehsan Azar   Nov 13, 2012
// Purpose: Create type for BmiChanAttr_t and commit
// Inputs:
//  loc  - where to add the type
// Outputs:
//   Returns the type id
hid_t CreateChanAttrType(hid_t loc)
{
    herr_t ret;
    hid_t tid = -1;
    std::string strLink = "BmiChanAttr_t";
    // If already there return it
    if(H5Lexists(loc, strLink.c_str(), H5P_DEFAULT))
    {
        tid = H5Topen(loc, strLink.c_str(), H5P_DEFAULT);
        return tid;
    }
    hid_t tid_attr_label_str = H5Tcopy(H5T_C_S1);
    ret = H5Tset_size(tid_attr_label_str, 64);

    tid = H5Tcreate(H5T_COMPOUND, sizeof(BmiChanAttr_t));
    ret = H5Tinsert(tid, "ID", offsetof(BmiChanAttr_t, id), H5T_NATIVE_UINT16);
    ret = H5Tinsert(tid, "Label", offsetof(BmiChanAttr_t, szLabel), tid_attr_label_str);

    ret = H5Tcommit(loc, strLink.c_str(), tid, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);

    H5Tclose(tid_attr_label_str);
    return tid;
}

// Author & Date:   Ehsan Azar   Nov 13, 2012
// Purpose: Create type for BmiSamplingAttr_t and commit
// Inputs:
//  loc  - where to add the type
// Outputs:
//   Returns the type id
hid_t CreateSamplingAttrType(hid_t loc)
{
    herr_t ret;
    hid_t tid = -1;
    std::string strLink = "BmiSamplingAttr_t";
    // If already there return it
    if(H5Lexists(loc, strLink.c_str(), H5P_DEFAULT))
    {
        tid = H5Topen(loc, strLink.c_str(), H5P_DEFAULT);
        return tid;
    }
    tid = H5Tcreate(H5T_COMPOUND, sizeof(BmiSamplingAttr_t));
    ret = H5Tinsert(tid, "Clock", offsetof(BmiSamplingAttr_t, fClock), H5T_NATIVE_FLOAT);
    ret = H5Tinsert(tid, "SampleRate", offsetof(BmiSamplingAttr_t, fSampleRate), H5T_NATIVE_FLOAT);
    ret = H5Tinsert(tid, "SampleBits", offsetof(BmiSamplingAttr_t, nSampleBits), H5T_NATIVE_UINT8);

    ret = H5Tcommit(loc, strLink.c_str(), tid, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);

    return tid;
}

// Author & Date:   Ehsan Azar   Nov 13, 2012
// Purpose: Create type for BmiRootAttr_t and commit
// Inputs:
//  loc  - where to add the type
// Outputs:
//   Returns the type id
hid_t CreateRootAttrType(hid_t loc)
{
    herr_t ret;
    hid_t tid = -1;
    std::string strLink = "BmiRootAttr_t";
    // If already there return it
    if(H5Lexists(loc, strLink.c_str(), H5P_DEFAULT))
    {
        tid = H5Topen(loc, strLink.c_str(), H5P_DEFAULT);
        return tid;
    }
    hid_t tid_attr_str = H5Tcopy(H5T_C_S1);
    ret = H5Tset_size(tid_attr_str, 64);
    hid_t tid_attr_comment_str = H5Tcopy(H5T_C_S1);
    ret = H5Tset_size(tid_attr_comment_str, 1024);

    tid = H5Tcreate(H5T_COMPOUND, sizeof(BmiRootAttr_t));
    ret = H5Tinsert(tid, "MajorVersion", offsetof(BmiRootAttr_t, nMajorVersion), H5T_NATIVE_UINT32);
    ret = H5Tinsert(tid, "MinorVersion", offsetof(BmiRootAttr_t, nMinorVersion), H5T_NATIVE_UINT32);
    ret = H5Tinsert(tid, "Flags", offsetof(BmiRootAttr_t, nFlags), H5T_NATIVE_UINT32);
    ret = H5Tinsert(tid, "GroupCount", offsetof(BmiRootAttr_t, nGroupCount), H5T_NATIVE_UINT32);
    ret = H5Tinsert(tid, "Date", offsetof(BmiRootAttr_t, szDate), tid_attr_str); // date of the file creation
    ret = H5Tinsert(tid, "Application", offsetof(BmiRootAttr_t, szApplication), tid_attr_str); // application that created the file
    ret = H5Tinsert(tid, "Comment", offsetof(BmiRootAttr_t, szComment), tid_attr_comment_str); // file comments

    ret = H5Tcommit(loc, strLink.c_str(), tid, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);

    H5Tclose(tid_attr_str);
    H5Tclose(tid_attr_comment_str);
    return tid;
}

// Author & Date:   Ehsan Azar   Nov 17, 2012
// Purpose: Create type for BmiSynchAttr_t and commit
// Inputs:
//  loc  - where to add the type
// Outputs:
//   Returns the type id
hid_t CreateSynchAttrType(hid_t loc)
{
    herr_t ret;
    hid_t tid = -1;
    std::string strLink = "BmiSynchAttr_t";
    // If already there return it
    if(H5Lexists(loc, strLink.c_str(), H5P_DEFAULT))
    {
        tid = H5Topen(loc, strLink.c_str(), H5P_DEFAULT);
        return tid;
    }
    hid_t tid_attr_label_str = H5Tcopy(H5T_C_S1);
    ret = H5Tset_size(tid_attr_label_str, 64);

    tid = H5Tcreate(H5T_COMPOUND, sizeof(BmiSynchAttr_t));
    ret = H5Tinsert(tid, "ID", offsetof(BmiSynchAttr_t, id), H5T_NATIVE_UINT16);
    ret = H5Tinsert(tid, "Label", offsetof(BmiSynchAttr_t, szLabel), tid_attr_label_str);
    ret = H5Tinsert(tid, "FPS", offsetof(BmiSynchAttr_t, fFps), H5T_NATIVE_FLOAT);

    ret = H5Tcommit(loc, strLink.c_str(), tid, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);

    H5Tclose(tid_attr_label_str);
    return tid;
}

// Author & Date:   Ehsan Azar   Nov 13, 2012
// Purpose: Create type for BmiTrackingAttr_t and commit
// Inputs:
//  loc  - where to add the type
// Outputs:
//   Returns the type id
hid_t CreateTrackingAttrType(hid_t loc)
{
    herr_t ret;
    hid_t tid = -1;
    std::string strLink = "BmiTrackingAttr_t";
    // If already there return it
    if(H5Lexists(loc, strLink.c_str(), H5P_DEFAULT))
    {
        tid = H5Topen(loc, strLink.c_str(), H5P_DEFAULT);
        return tid;
    }
    hid_t tid_attr_label_str = H5Tcopy(H5T_C_S1);
    ret = H5Tset_size(tid_attr_label_str, 128);

    tid = H5Tcreate(H5T_COMPOUND, sizeof(BmiTrackingAttr_t));
    ret = H5Tinsert(tid, "Label", offsetof(BmiTrackingAttr_t, szLabel), tid_attr_label_str);
    ret = H5Tinsert(tid, "Type", offsetof(BmiTrackingAttr_t, type), H5T_NATIVE_UINT16);
    ret = H5Tinsert(tid, "TrackID", offsetof(BmiTrackingAttr_t, trackID), H5T_NATIVE_UINT16);
    ret = H5Tinsert(tid, "MaxPoints", offsetof(BmiTrackingAttr_t, maxPoints), H5T_NATIVE_UINT16);

    ret = H5Tcommit(loc, strLink.c_str(), tid, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);

    H5Tclose(tid_attr_label_str);
    return tid;
}

// Author & Date:   Ehsan Azar   Nov 13, 2012
// Purpose: Create type for BmiSpike16_t and commit
//          Note: For performance reasons and because spike length is constant during
//           and experiment we use fixed-length array here instead of varible-length
// Inputs:
//  loc         - where to add the type
//  spikeLength - the spike length to use
// Outputs:
//   Returns the type id
hid_t CreateSpike16Type(hid_t loc, uint16_t spikeLength)
{
    herr_t ret;
    hid_t tid = -1;

    // e.g. for spike length of 48 type name will be "BmiSpike16_48_t"
    char szNum[4] = {'\0'};
    sprintf(szNum, "%u", spikeLength);
    std::string strLink = "BmiSpike16_";
    strLink += szNum;
    strLink += "_t";
    // If already there return it
    if(H5Lexists(loc, strLink.c_str(), H5P_DEFAULT))
    {
        tid = H5Topen(loc, strLink.c_str(), H5P_DEFAULT);
        return tid;
    }

    hsize_t     dims[1] = {spikeLength};
    hid_t tid_arr_wave = H5Tarray_create(H5T_NATIVE_INT16, 1, dims);

    tid = H5Tcreate(H5T_COMPOUND, offsetof(BmiSpike16_t, wave) + sizeof(int16_t) * spikeLength);
    ret = H5Tinsert(tid, "TimeStamp", offsetof(BmiSpike16_t, dwTimestamp), H5T_NATIVE_UINT32);
    ret = H5Tinsert(tid, "Unit", offsetof(BmiSpike16_t, unit), H5T_NATIVE_UINT8);
    ret = H5Tinsert(tid, "Wave", offsetof(BmiSpike16_t, wave), tid_arr_wave);
    ret = H5Tcommit(loc, strLink.c_str(), tid, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);

    H5Tclose(tid_arr_wave);
    return tid;
}

// Author & Date:   Ehsan Azar   Nov 23, 2012
// Purpose: Create type for BmiDig16_t and commit
// Inputs:
//  loc         - where to add the type
// Outputs:
//   Returns the type id
hid_t CreateDig16Type(hid_t loc)
{
    herr_t ret;
    hid_t tid = -1;
    std::string strLink = "BmiDig16_t";
    // If already there return it
    if(H5Lexists(loc, strLink.c_str(), H5P_DEFAULT))
    {
        tid = H5Topen(loc, strLink.c_str(), H5P_DEFAULT);
        return tid;
    }

    tid = H5Tcreate(H5T_COMPOUND, sizeof(BmiDig16_t));
    ret = H5Tinsert(tid, "TimeStamp", offsetof(BmiDig16_t, dwTimestamp), H5T_NATIVE_UINT32);
    ret = H5Tinsert(tid, "Value", offsetof(BmiDig16_t, value), H5T_NATIVE_UINT16);
    ret = H5Tcommit(loc, strLink.c_str(), tid, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);

    return tid;
}

// Author & Date:   Ehsan Azar   Nov 13, 2012
// Purpose: Create type for BmiSynch_t and commit
// Inputs:
//  loc  - where to add the type
// Outputs:
//   Returns the type id
hid_t CreateSynchType(hid_t loc)
{
    herr_t ret;

    hid_t tid = H5Tcreate(H5T_COMPOUND, sizeof(BmiSynch_t));
    ret = H5Tinsert(tid, "TimeStamp", offsetof(BmiSynch_t, dwTimestamp), H5T_NATIVE_UINT32);
    ret = H5Tinsert(tid, "Split", offsetof(BmiSynch_t, split), H5T_NATIVE_UINT16);
    ret = H5Tinsert(tid, "Frame", offsetof(BmiSynch_t, frame), H5T_NATIVE_UINT32);
    ret = H5Tinsert(tid, "ElapsedTime", offsetof(BmiSynch_t, etime), H5T_NATIVE_UINT32);

    ret = H5Tcommit(loc, "BmiSynch_t", tid, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);

    return tid;
}

// Author & Date:   Ehsan Azar   Nov 20, 2012
// Purpose: Create type for BmiTracking_t and commit
// Inputs:
//  loc    - where to add the type
//  dim    - dimension (1D, 2D or 3D)
//  width  - datapoint width in bytes
// Outputs:
//   Returns the type id
hid_t CreateTrackingType(hid_t loc, int dim, int width)
{
    herr_t ret;
    hid_t tid_coords;
    std::string strLabel = "BmiTracking";
    // e.g. for 1D (32-bit) fixed-length, type name will be "BmiTracking32_1D_t"
    //      for 2D (16-bit) fixed-length, type name will be "BmiTracking16_2D_t"

    switch (width)
    {
    case 1:
        strLabel += "8";
        tid_coords = H5Tcopy(H5T_NATIVE_UINT8);
        break;
    case 2:
        strLabel += "16";
        tid_coords = H5Tcopy(H5T_NATIVE_UINT16);
        break;
    case 4:
        strLabel += "32";
        tid_coords = H5Tcopy(H5T_NATIVE_UINT32);
        break;
    default:
        return 0;
        // should not happen
        break;
    }
    switch (dim)
    {
    case 1:
        strLabel += "_1D";
        break;
    case 2:
        strLabel += "_2D";
        break;
    case 3:
        strLabel += "_3D";
        break;
    default:
        return 0;
        // should not happen
        break;
    }
    strLabel += "_t";

    hid_t tid = -1;
    if(H5Lexists(loc, strLabel.c_str(), H5P_DEFAULT))
    {
        tid = H5Topen(loc, strLabel.c_str(), H5P_DEFAULT);
    } else {
        tid = H5Tcreate(H5T_COMPOUND, offsetof(BmiTracking_fl_t, coords) + dim * width * 1);
        ret = H5Tinsert(tid, "TimeStamp", offsetof(BmiTracking_fl_t, dwTimestamp), H5T_NATIVE_UINT32);
        ret = H5Tinsert(tid, "ParentID", offsetof(BmiTracking_fl_t, parentID), H5T_NATIVE_UINT16);
        ret = H5Tinsert(tid, "NodeCount", offsetof(BmiTracking_fl_t, nodeCount), H5T_NATIVE_UINT16);
        ret = H5Tinsert(tid, "ElapsedTime", offsetof(BmiTracking_fl_t, etime), H5T_NATIVE_UINT32);
        char corrd_labels[3][2] = {"x", "y", "z"};
        for (int i = 0; i < dim; ++i)
            ret = H5Tinsert(tid, corrd_labels[i], offsetof(BmiTracking_fl_t, coords) + i * width, tid_coords);
        ret = H5Tcommit(loc, strLabel.c_str(), tid, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
    }

    H5Tclose(tid_coords);

    return tid;
}

// Author & Date:   Ehsan Azar   Nov 19, 2012
// Purpose: Create type for BmiComment_t and commit
// Inputs:
//  loc  - where to add the type
// Outputs:
//   Returns the type id
hid_t CreateCommentType(hid_t loc)
{
    herr_t ret;
    // Use fixed-size comments because 
    //  it is faster, more tools support it, and also allows compression
    hid_t tid_str_array = H5Tcopy(H5T_C_S1);
    ret = H5Tset_size(tid_str_array, BMI_COMMENT_LEN);

    hid_t tid = H5Tcreate(H5T_COMPOUND, sizeof(BmiComment_t));
    ret = H5Tinsert(tid, "TimeStamp", offsetof(BmiComment_t, dwTimestamp), H5T_NATIVE_UINT32);
    ret = H5Tinsert(tid, "Data", offsetof(BmiComment_t, data), H5T_NATIVE_UINT32);
    ret = H5Tinsert(tid, "Flags", offsetof(BmiComment_t, flags), H5T_NATIVE_UINT8);
    ret = H5Tinsert(tid, "Comment", offsetof(BmiComment_t, szComment), tid_str_array);

    ret = H5Tcommit(loc, "BmiComment_t", tid, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);

    H5Tclose(tid_str_array);

    return tid;
}
