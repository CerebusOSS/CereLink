///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   central_version.cpp
/// @author Caden Shmookler
/// @date   2026-06-25
///
/// @brief  Protocol version detection for Central
///
///////////////////////////////////////////////////////////////////////////////////////////////////

// Platform headers MUST be included first (before cbproto)
#include "platform_first.h"
#ifdef _WIN32
    #include <winver.h>
    #include <tlhelp32.h>

    #include <vector>
    #include <charconv>
#endif

#include <cbshm/central_version.h>

namespace cbshm {

cbutil::Result<CentralVersion> detectCentralVersion() {
#ifdef _WIN32
    // The 'version' field in Central's shared memory configuration buffer
    // is set to a magic number, 96, instead of a meaningful value that
    // indicates the application or protocol version.  The only other field
    // in Central's shared memory with version information is procinfo[].version,
    // but it's byte offset changes between protocol versions so it is unusable.
    // This function infers Central's protocol version by inspecting
    // VersionInfo.ProductVersion in Central.exe and converting from application
    // version to protocol version.
    //
    // This process for inferring the protocol version is indirect and brittle.
    // If a future version of Central were to have a different executable name,
    // version field name, or version format, then this detection method would
    // fail to deduce the protocol version.

    // Get the path to Central's executable file from the running processes list.

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return cbutil::Result<CentralVersion>::error("Failed to get a snapshot of the running processes from the Tool Help library");
    }

    PROCESSENTRY32 process_entry{};
    process_entry.dwSize = sizeof(process_entry);
    if (! Process32First(snapshot, &process_entry)) {
        return cbutil::Result<CentralVersion>::error("Failed to get the first process from the running processes snapshot");
    }

    // Enumerate the running processes and identify Central by it's process name.
    DWORD central_pid = 0;
    do {
        if (std::string(process_entry.szExeFile) == "Central.exe") {
            central_pid = process_entry.th32ProcessID;
            break;
        }
    } while(Process32Next(snapshot, &process_entry));
    if (central_pid == 0) {
        return cbutil::Result<CentralVersion>::error("Failed to find Central among the currently running processes. Is Central running?");
    }

    HANDLE central = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, central_pid);
    if (! central) {
        return cbutil::Result<CentralVersion>::error("Failed to inspect Central's process");
    }

    char central_path[MAX_PATH] = {0};
    DWORD central_path_size = MAX_PATH;
    if (! QueryFullProcessImageNameA(central, NULL, central_path, &central_path_size)) {
        CloseHandle(central);
        return cbutil::Result<CentralVersion>::error("Failed to get the path to Central's executable");
    }
    CloseHandle(central);

    // Get Central's application version by inspecting the properties of the
    // Central.exe binary

    DWORD handle = 0;
    DWORD block_size = GetFileVersionInfoSizeA(central_path, &handle);
    if (block_size == 0) {
        return cbutil::Result<CentralVersion>::error("Failed to get the length of the resource block containing Central's application version");
    }

    std::vector<BYTE> block(block_size);
    if (!GetFileVersionInfoA(central_path, handle, block_size, block.data())) {
        return cbutil::Result<CentralVersion>::error("Failed to extract the version resource block from Central");
    }

    struct LangCodePage {
        WORD lang;
        WORD codepage;
    } *translate = nullptr;
    UINT translate_size = 0;
    if (!VerQueryValueA(block.data(), "\\VarFileInfo\\Translation", (LPVOID*)&translate, &translate_size) || translate_size == 0) {
        return cbutil::Result<CentralVersion>::error("Failed to lookup the available version translations for Central");
    }

    char sub_block[64];
    sprintf_s(sub_block, sizeof(sub_block), "\\StringFileInfo\\%04x%04x\\ProductVersion", translate[0].lang, translate[0].codepage);

    char* value = nullptr;
    UINT value_size = 0;
    if (!VerQueryValueA(block.data(), sub_block, (LPVOID*)&value, &value_size) || value_size == 0) {
        return cbutil::Result<CentralVersion>::error("Failed to extract Central's version from the resource block");
    }
    std::string app_version = std::string(value, value_size - 1); // strip trailing null byte

    // Get the indicies of both dots in the version string
    size_t ldot_idx = app_version.find("."); 
    size_t rdot_idx = app_version.rfind(".");
    if (ldot_idx == std::string::npos || rdot_idx == std::string::npos) {
        return cbutil::Result<CentralVersion>::error("Failed to find both dots separating the major, minor, and patch version values in '" + app_version + "'");
    }

    // Extract the major version number
    char* major_version_begin = app_version.data();
    char* major_version_end = app_version.data() + ldot_idx;
    uint32_t major_version;
    std::from_chars_result result{};
    result.ptr = nullptr;
    result.ec = std::errc(0);
    result = std::from_chars(major_version_begin, major_version_end, major_version);
    if (result.ec != std::errc(0) || result.ptr != major_version_end) {
        return cbutil::Result<CentralVersion>::error("Failed to isolate the major version value from '" + app_version + "'");
    }

    // Extract the minor version number
    char* minor_version_begin = app_version.data() + ldot_idx + 1;
    char* minor_version_end = app_version.data() + rdot_idx;
    uint32_t minor_version;
    result.ptr = nullptr;
    result.ec = std::errc(0);
    result = std::from_chars(minor_version_begin, minor_version_end, minor_version);
    if (result.ec != std::errc(0) || result.ptr != minor_version_end) {
        return cbutil::Result<CentralVersion>::error("Failed to isolate the minor version value from '" + app_version + "'");
    }

    // Convert application version to protocol version
    switch(major_version) {
        case 7:
            switch (minor_version) {
                default:
                    return cbutil::Result<CentralVersion>::ok(CentralVersion::CURRENT);
                    // return cbutil::Result<CentralVersion>::error("Unrecognized minor version number in version '" + app_version + "'");
                case 8:
                    return cbutil::Result<CentralVersion>::ok(CentralVersion::CURRENT);
                case 7:
                    return cbutil::Result<CentralVersion>::ok(CentralVersion::V7_7);
                case 6:
                    return cbutil::Result<CentralVersion>::ok(CentralVersion::V7_6);
                case 5:
                    return cbutil::Result<CentralVersion>::ok(CentralVersion::V7_5);
                case 0:
                    return cbutil::Result<CentralVersion>::ok(CentralVersion::V7_0);
            }
            break;
        case 6:
            /* fallthrough */
        case 5:
            /* fallthrough */
        case 4:
            /* fallthrough */
        case 3:
            /* fallthrough */
        case 2:
            /* fallthrough */
        case 1:
            return cbutil::Result<CentralVersion>::error("Unsupported major version number in version '" + app_version + "'. Please update Central to a newer version");
        default:
            return cbutil::Result<CentralVersion>::ok(CentralVersion::UNKNOWN);
            // return cbutil::Result<CentralVersion>::error("Unrecognized major version number in version '" + app_version + "'" );
    }
#else
    return cbutil::Result<CentralVersion>::error("Compatibility with Central requires Windows");
#endif
}

cbproto_protocol_version_t getProtocolVersion(CentralVersion version) {
    switch (version) {
        default:
            /* fallthrough */
        case CentralVersion::UNKNOWN:
            return CBPROTO_PROTOCOL_UNKNOWN;
        case CentralVersion::V7_0:
            return CBPROTO_PROTOCOL_311;
        case CentralVersion::V7_5:
            return CBPROTO_PROTOCOL_400;
        case CentralVersion::V7_6:
            /* fallthrough */
        case CentralVersion::V7_7:
            return CBPROTO_PROTOCOL_410;
        case CentralVersion::CURRENT:
            return CBPROTO_PROTOCOL_CURRENT;
    }
}

} // namespace cbshm
