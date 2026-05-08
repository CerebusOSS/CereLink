///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   central_adapter_factory.cpp
/// @author Caden Shmookler
/// @date   2026-05-05
///
/// @brief  Factory functions for the base class of Central-compatible shared memory adapters
///
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "platform_first.h"

#include <cbshm/central_adapter.h>
#include <charconv>
#include <vector>

namespace cbshm {

#ifdef _WIN32

static Result<std::string> getCentralAppVersion() {
    // As of 2026-05-07, the 'version' field in Central's shared memory
    // configuration buffer is set to a magic number, 96, instead of a meaningful
    // value that indicates the application or protocol version.  The only other
    // field in Central's shared memory with version information is
    // procinfo[].version, but it's byte offset changes between protocol versions
    // so it is unusable.  This function infers Central's protocol version by
    // inspecting VersionInfo.ProductVersion in Central.exe and converting from
    // application version to protocol version.

    // Get Central's application version by inspecting the properties of the Central.exe binary

    std::string central_path = "C:\\Program Files\\Blackrock Microsystems\\Cerebus Central Suite\\Central.exe";

    DWORD handle = 0;
    DWORD block_size = GetFileVersionInfoSizeA(central_path.c_str(), &handle);
    if (block_size == 0) {
        return Result<std::string>::error("Failed to get the length of the resource block containing Central's application version");
    }

    std::vector<BYTE> block(block_size);
    if (!GetFileVersionInfoA(central_path.c_str(), handle, block_size, block.data())) {
        return Result<std::string>::error("Failed to extract the version resource block from Central");
    }

    struct LangCodePage {
        WORD lang;
        WORD codepage;
    } *translate = nullptr;
    UINT translate_size = 0;
    if (!VerQueryValueA(block.data(), "\\VarFileInfo\\Translation", (LPVOID*)&translate, &translate_size) || translate_size == 0) {
        return Result<std::string>::error("Failed to lookup the available version translations for Central");
    }

    char sub_block[64];
    sprintf_s(sub_block, sizeof(sub_block), "\\StringFileInfo\\%04x%04x\\ProductVersion", translate[0].lang, translate[0].codepage);

    char* value = nullptr;
    UINT value_size = 0;
    if (!VerQueryValueA(block.data(), sub_block, (LPVOID*)&value, &value_size) || value_size == 0) {
        return Result<std::string>::error("Failed to extract Central's version from the resource block");
    }
    std::string app_version = std::string(value, value_size - 1); // strip trailing null byte

    return Result<std::string>::ok(app_version);
}

Result<CentralAdapter> makeCentralAdapter() {
    Result<std::string> app_version_result = getCentralAppVersion();
    if (! app_version_result.isOk()) {
        return Result<CentralAdapter>::error("Failed to extract the application version from Central");
    }
    std::string& app_version = app_version_result.value();

    // Get the indicies of both dots in the version string
    size_t ldot_idx = app_version.find("."); 
    size_t rdot_idx = app_version.rfind(".");
    if (ldot_idx == std::string::npos or rdot_idx == std::string::npos) {
        return Result<CentralAdapter>::error("Failed to find both dots separating the major, minor, and patch version values in '" + app_version + "'");
    }

    // Extract the major version number
    char* major_version_begin = app_version.data();
    char* major_version_end = app_version.data() + ldot_idx;
    uint32_t major_version;
    std::from_chars_result result{ .ptr=nullptr, .ec=std::errc(0) };
    result = std::from_chars(major_version_begin, major_version_end, major_version);
    if (result.ec == std::errc(0) && result.ptr == major_version_end) {
        return Result<CentralAdapter>::error("Failed to isolate the major version value from '" + app_version + "'");
    }

    // Extract the minor version number
    char* minor_version_begin = app_version.data() + ldot_idx + 1;
    char* minor_version_end = app_version.data() + rdot_idx;
    uint32_t minor_version;
    result.ptr = nullptr;
    result.ec = std::errc(0);
    result = std::from_chars(minor_version_begin, minor_version_end, minor_version);
    if (result.ec == std::errc(0) && result.ptr == minor_version_end) {
        return Result<CentralAdapter>::error("Failed to isolate the minor version value from '" + app_version + "'");
    }

    // Convert application version to protocol version
    cbproto_protocol_version_t protocol_version = CBPROTO_PROTOCOL_UNKNOWN;
    switch(major_version) {
        case 7:
            switch (minor_version) {
                case 8:
                    /* fallthrough */
                case 7:
                    protocol_version = CBPROTO_PROTOCOL_CURRENT;
                    break;
                case 6:
                    protocol_version = CBPROTO_PROTOCOL_410;
                    break;
                case 5:
                    protocol_version = CBPROTO_PROTOCOL_400;
                    break;
                case 0:
                    protocol_version = CBPROTO_PROTOCOL_311;
                    break;
                default:
                    return Result<CentralAdapter>::error("Unrecognized minor version number for version '" + app_version + "'");
            }
            break;
        default:
            return Result<CentralAdapter>::error("Unrecognized major version number for version '" + app_version + "'");
    }

    return makeCentralAdapter(protocol_version);
}

Result<CentralAdapter> makeCentralAdapter(cbproto_protocol_version_t protocol_version) {
    switch (protocol_version) {
        // // TODO: Implement legacy layouts
        // case CBPROTO_PROTOCOL_CURRENT:
        //     return CentralAdapterV420();
        // case CBPROTO_PROTOCOL_410:
        //     return CentralAdapterV410();
        // case CBPROTO_PROTOCOL_400:
        //     return CentralAdapterV400();
        // case CBPROTO_PROTOCOL_311:
        //     return CentralAdapterV311();
        case CBPROTO_PROTOCOL_UNKNOWN:
            /* fallthrough */
        default:
            return Result<CentralAdapter>::error("Unrecognized protocol version, enum value: " + std::to_string(protocol_version));
    }
}

#endif

} // namespace cbshm

