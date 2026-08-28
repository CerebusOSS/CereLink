/*
 * test_cbsdk_c_compile.c
 *
 * cbsdk.h is a C API, so it and any files it includes must be compilable with
 * a C compiler.
 */

#include <cbsdk/cbsdk.h>

int main(void) {
    // Nothing to do here.  Just include cbsdk...

    /* Types that carry data across the C boundary are named here so a change
     * to their shape or spelling breaks the C build rather than only C++. */
    cbsdk_channel_conversion_t conversion;
    cbsdk_scaling_source_t source = CBSDK_SCALING_PHYSICAL;
    conversion.scale = 1.0;
    conversion.offset = 0.0;
    conversion.unit[0] = '\0';
    (void) source;
    (void) conversion;

    return 0;
}
