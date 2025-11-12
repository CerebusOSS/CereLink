#include <iostream>
#include <cstddef>
#include "cbhwlib.h"

int main() {
    std::cout << "=== UPSTREAM Detailed cbCFGBUFF Analysis ===" << std::endl;
    std::cout << "cbMAXPROCS = " << cbMAXPROCS << std::endl;
    std::cout << "cbMAXBANKS = " << cbMAXBANKS << std::endl;
    std::cout << "cbMAXGROUPS = " << cbMAXGROUPS << std::endl;
    std::cout << "cbMAXFILTS = " << cbMAXFILTS << std::endl;
    std::cout << std::endl;

    std::cout << "sizeof(cbPKT_BANKINFO) = " << sizeof(cbPKT_BANKINFO) << std::endl;
    std::cout << "sizeof(cbPKT_GROUPINFO) = " << sizeof(cbPKT_GROUPINFO) << std::endl;
    std::cout << "sizeof(cbPKT_FILTINFO) = " << sizeof(cbPKT_FILTINFO) << std::endl;
    std::cout << "sizeof(cbPKT_ADAPTFILTINFO) = " << sizeof(cbPKT_ADAPTFILTINFO) << std::endl;
    std::cout << "sizeof(cbPKT_REFELECFILTINFO) = " << sizeof(cbPKT_REFELECFILTINFO) << std::endl;
    std::cout << std::endl;

    std::cout << "Array sizes:" << std::endl;
    std::cout << "bankinfo[" << cbMAXPROCS << "][" << cbMAXBANKS << "] = " << (cbMAXPROCS * cbMAXBANKS * sizeof(cbPKT_BANKINFO)) << " bytes" << std::endl;
    std::cout << "groupinfo[" << cbMAXPROCS << "][" << cbMAXGROUPS << "] = " << (cbMAXPROCS * cbMAXGROUPS * sizeof(cbPKT_GROUPINFO)) << " bytes" << std::endl;
    std::cout << "filtinfo[" << cbMAXPROCS << "][" << cbMAXFILTS << "] = " << (cbMAXPROCS * cbMAXFILTS * sizeof(cbPKT_FILTINFO)) << " bytes" << std::endl;
    std::cout << "adaptinfo[" << cbMAXPROCS << "] = " << (cbMAXPROCS * sizeof(cbPKT_ADAPTFILTINFO)) << " bytes" << std::endl;
    std::cout << "refelecinfo[" << cbMAXPROCS << "] = " << (cbMAXPROCS * sizeof(cbPKT_REFELECFILTINFO)) << " bytes" << std::endl;

    UINT32 total = cbMAXPROCS * cbMAXBANKS * sizeof(cbPKT_BANKINFO) +
                   cbMAXPROCS * cbMAXGROUPS * sizeof(cbPKT_GROUPINFO) +
                   cbMAXPROCS * cbMAXFILTS * sizeof(cbPKT_FILTINFO) +
                   cbMAXPROCS * sizeof(cbPKT_ADAPTFILTINFO) +
                   cbMAXPROCS * sizeof(cbPKT_REFELECFILTINFO);
    std::cout << "\nTotal between procinfo and chaninfo: " << total << " bytes" << std::endl;

    return 0;
}
