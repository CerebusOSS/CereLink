/* =STS=> cki_common.h[1693].aa07   open     SMID:7 */
///////////////////////////////////////////////////////////////////////////////
//
// (c) Copyright 2002 - 2008 Cyberkinetics, Inc.
// (c) Copyright 2008 - 2019 Blackrock Microsystems, LLC
//
// $Workfile: cki_common.h $
// $Archive: /Cerebus/Human/WindowsApps/cbhwlib/cki_common.h $
// $Revision: 7 $
// $Date: 6/02/05 3:52p $
// $Author: Kkorver $
//
// $NoKeywords: $
//
///////////////////////////////////////////////////////////////////////////////

#ifndef CKI_COMMON_H_INCLUDED
#define CKI_COMMON_H_INCLUDED

#ifdef _MSC_VER
#include <vector>
#include<algorithm>

typedef std::vector<int> INTVECTOR;
#endif

enum CKI_ERROR
{
    CKI_OK = 0,
    CKI_UNSPEC_ERROR,       // Generic unclassified error
    CKI_INVALID_CHANNEL,    // Invalid channel used
    CKI_INVALID_UNIT,       // Invalid unit used
    CKI_FILE_READ_ERR,      // File read error
    CKI_FILE_WRITE_ERR,     // File write error
};

typedef CKI_ERROR CKIRETURN;  // Common return type, non-zero values indicate err (type)

enum STARTUP_OPTIONS
{
    OPT_NONE        = 0x00000000,
    OPT_ANY_IP      = 0x00000001,       // set if we want to bind to whatever ip address is available
    OPT_LOOPBACK    = 0x00000002,       // set if we want to try the loopback ip address
	OPT_LOCAL       = 0x00000003,       // set if we want to connect to lacalhost
    OPT_REUSE       = 0X00000004        // set if we want to allow other connections to the port
};

// The two cart types. Research gives advanced options to the researchers
#define NEUROPORT_CART "Neuroport"
#define RESEARCH_CART  "Research"

// CodeMeter firm code, product code, and feature codes
#define TEST_CM_FIRMCODE 10 // This is the test firm code
#define BMI_CM_FIRMCODE 101966
#define BMI_CM_APP_NM   1                    // NeuroMotive without any tracking capabilities
// Features are per-bits and thus power-of-two; we can have up to 32 features currently
#define BMI_CM_APP_NM_FEATURE_TRACK 1        // NeuroMotive with all tracking capabilities

// Find out the size of an array
#define ARRAY_SIZE(x) (sizeof(x)/sizeof(x[0]))

#define TIMER_PERIOD_CENTRAL		10	// milliseconds period of the main timer of Central

//	I think that in this one case, the preprocessor is a better choice
//
//template<class T>
//inline size_t ARRAY_SIZE(const T array)
//{ return sizeof(array) / sizeof(*array); }
//
//

// Use this for STL array functions..ONLY IF A STATIC ARRAY
// e.g.  std::max_element(x, ARRAY_END(x));
#define ARRAY_END(x) (x + ARRAY_SIZE(x))

#ifdef _MSC_VER
// Author & Date:   Kirk Korver     23 May 2003
// Purpose: Ensure that a number is between 2 other numbers (inclusive)
// Inputs:
//  nVal - the value we need to clip
//  nMaxValue - the maximum possible value (aka biggest positive number)
//  nMinValue - the minimum possible value (aka smallest negative number)
// Outputs:
//  the new value of nVal adjusted so it fits within the range
template <class T>
inline T clip(T nValToClip, T nMinValue, T nMaxValue)
{
    if (nValToClip > nMaxValue)
        return nMaxValue;

    if (nValToClip < nMinValue)
        return nMinValue;

    return nValToClip;
}


namespace std
{
    // Author & Date:   Kirk Korver     03 Jun 2004
    // Purpose: wrap the std::for_each function to apply the functor to ALL entries
    // Usage:
    //  vector<int> v(10);
    //  int Double(int x) { return x+x; }
    //  for_all(v, Double);
    // Inputs:
    //  c - the container we care about
    //  f - the function or functor we want to apply
    template <class Container, typename Functor> inline
    Functor for_all(Container  & c, Functor & f)
    {
        return std::for_each(c.begin(), c.end(), f);
    }
};
#endif


#endif // include guard
