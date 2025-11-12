#include <iostream>
#include <cstddef>
#include "cbhwlib.h"

int main() {
    std::cout << "=== UPSTREAM Central 7.8.0 RC cbCFGBUFF Structure Size Analysis ===" << std::endl;
    std::cout << "cbMAXPROCS = " << cbMAXPROCS << std::endl;
    std::cout << "cbNUM_FE_CHANS = " << cbNUM_FE_CHANS << std::endl;
    std::cout << "cbMAXCHANS = " << cbMAXCHANS << std::endl;
    std::cout << "cbMAXNTRODES = " << cbMAXNTRODES << std::endl;
    std::cout << std::endl;

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
    std::cout << "offsetof(cbCFGBUFF, fileinfo) = " << offsetof(cbCFGBUFF, fileinfo) << std::endl;
    std::cout << "offsetof(cbCFGBUFF, hwndCentral) = " << offsetof(cbCFGBUFF, hwndCentral) << std::endl;

    return 0;
}
