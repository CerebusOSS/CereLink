#include <iostream>
#include <cstddef>
#include "cbhwlib.h"

int main() {
    std::cout << "=== UPSTREAM cbCFGBUFF Structure Size Analysis ===" << std::endl;
    std::cout << "sizeof(cbCFGBUFF) = " << sizeof(cbCFGBUFF) << " bytes" << std::endl;
    std::cout << "sizeof(HANDLE) = " << sizeof(HANDLE) << " bytes" << std::endl;
    std::cout << std::endl;

    std::cout << "=== Field Offsets ===" << std::endl;
    std::cout << "offsetof(cbCFGBUFF, version) = " << offsetof(cbCFGBUFF, version) << std::endl;
    std::cout << "offsetof(cbCFGBUFF, sysinfo) = " << offsetof(cbCFGBUFF, sysinfo) << std::endl;
    std::cout << "offsetof(cbCFGBUFF, procinfo) = " << offsetof(cbCFGBUFF, procinfo) << std::endl;
    std::cout << "offsetof(cbCFGBUFF, chaninfo) = " << offsetof(cbCFGBUFF, chaninfo) << std::endl;
    std::cout << "offsetof(cbCFGBUFF, isSortingOptions) = " << offsetof(cbCFGBUFF, isSortingOptions) << std::endl;
    std::cout << "offsetof(cbCFGBUFF, isNTrodeInfo) = " << offsetof(cbCFGBUFF, isNTrodeInfo) << std::endl;
    std::cout << "offsetof(cbCFGBUFF, isNPlay) = " << offsetof(cbCFGBUFF, isNPlay) << std::endl;
    std::cout << "offsetof(cbCFGBUFF, isVideoSource) = " << offsetof(cbCFGBUFF, isVideoSource) << std::endl;
    std::cout << "offsetof(cbCFGBUFF, isTrackObj) = " << offsetof(cbCFGBUFF, isTrackObj) << std::endl;
    std::cout << "offsetof(cbCFGBUFF, fileinfo) = " << offsetof(cbCFGBUFF, fileinfo) << std::endl;
    std::cout << "offsetof(cbCFGBUFF, hwndCentral) = " << offsetof(cbCFGBUFF, hwndCentral) << std::endl;
    std::cout << std::endl;

    std::cout << "=== Component Sizes ===" << std::endl;
    std::cout << "sizeof(cbOPTIONTABLE) = " << sizeof(cbOPTIONTABLE) << std::endl;
    std::cout << "sizeof(cbCOLORTABLE) = " << sizeof(cbCOLORTABLE) << std::endl;
    std::cout << "sizeof(cbPKT_SYSINFO) = " << sizeof(cbPKT_SYSINFO) << std::endl;
    std::cout << "sizeof(cbPKT_PROCINFO) = " << sizeof(cbPKT_PROCINFO) << std::endl;
    std::cout << "sizeof(cbPKT_CHANINFO) = " << sizeof(cbPKT_CHANINFO) << std::endl;
    std::cout << "sizeof(cbSPIKE_SORTING) = " << sizeof(cbSPIKE_SORTING) << std::endl;
    std::cout << "sizeof(cbPKT_NTRODEINFO) = " << sizeof(cbPKT_NTRODEINFO) << std::endl;
    std::cout << "sizeof(cbPKT_NPLAY) = " << sizeof(cbPKT_NPLAY) << std::endl;

    return 0;
}
