// Reset XmtGlobal: advance tailindex to headindex to skip stale packets
#include <windows.h>
#include <cstdio>
#include <cstdint>

struct XmtHeader {
    uint32_t transmitted;
    uint32_t headindex;
    uint32_t tailindex;
    uint32_t last_valid_index;
    uint32_t bufferlen;
};

int main() {
    HANDLE h = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, "XmtGlobal");
    if (!h) {
        fprintf(stderr, "Cannot open XmtGlobal (err=%lu)\n", GetLastError());
        return 1;
    }
    void* ptr = MapViewOfFile(h, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    if (!ptr) {
        fprintf(stderr, "Cannot map XmtGlobal (err=%lu)\n", GetLastError());
        CloseHandle(h);
        return 1;
    }
    auto* xmt = static_cast<XmtHeader*>(ptr);

    fprintf(stderr, "BEFORE: transmitted=%u  head=%u  tail=%u  last_valid=%u  buflen=%u\n",
            xmt->transmitted, xmt->headindex, xmt->tailindex,
            xmt->last_valid_index, xmt->bufferlen);

    // Reset: move tail to head (skip all stale packets)
    xmt->tailindex = xmt->headindex;

    fprintf(stderr, "AFTER:  transmitted=%u  head=%u  tail=%u  (stale packets skipped)\n",
            xmt->transmitted, xmt->headindex, xmt->tailindex);

    UnmapViewOfFile(ptr);
    CloseHandle(h);
    return 0;
}
