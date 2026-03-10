// Quick diagnostic: dump XmtGlobal and XmtLocal shared memory state
// Build: g++ -o xmt_diag xmt_diag.cpp -lkernel32
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

void dumpXmt(const char* name) {
    HANDLE h = OpenFileMappingA(FILE_MAP_READ, FALSE, name);
    if (!h) {
        fprintf(stderr, "  %s: OpenFileMapping FAILED (err=%lu) - segment doesn't exist\n", name, GetLastError());
        return;
    }
    void* ptr = MapViewOfFile(h, FILE_MAP_READ, 0, 0, 0);
    if (!ptr) {
        fprintf(stderr, "  %s: MapViewOfFile FAILED (err=%lu)\n", name, GetLastError());
        CloseHandle(h);
        return;
    }
    auto* xmt = static_cast<XmtHeader*>(ptr);
    fprintf(stderr, "  %s:\n", name);
    fprintf(stderr, "    transmitted=%u  headindex=%u  tailindex=%u\n",
            xmt->transmitted, xmt->headindex, xmt->tailindex);
    fprintf(stderr, "    last_valid_index=%u  bufferlen=%u\n",
            xmt->last_valid_index, xmt->bufferlen);

    // Dump first 20 words of buffer (after header)
    uint32_t* buf = reinterpret_cast<uint32_t*>(static_cast<char*>(ptr) + sizeof(XmtHeader));
    fprintf(stderr, "    buffer[0..19]:");
    for (int i = 0; i < 20; i++) {
        fprintf(stderr, " %08X", buf[i]);
        if (i == 9) fprintf(stderr, "\n                 ");
    }
    fprintf(stderr, "\n");

    // If head != tail, dump words at tailindex
    if (xmt->headindex != xmt->tailindex) {
        uint32_t t = xmt->tailindex;
        fprintf(stderr, "    AT TAIL[%u]:", t);
        for (uint32_t i = 0; i < 10 && (t+i) < xmt->bufferlen; i++) {
            fprintf(stderr, " %08X", buf[t+i]);
        }
        fprintf(stderr, "\n");
    }

    UnmapViewOfFile(ptr);
    CloseHandle(h);
}

int main() {
    fprintf(stderr, "=== XMT Buffer Diagnostics ===\n\n");
    dumpXmt("XmtGlobal");
    fprintf(stderr, "\n");
    dumpXmt("XmtLocal");
    fprintf(stderr, "\n");
    // Also check numbered variants
    for (int i = 1; i <= 3; i++) {
        char name[32];
        snprintf(name, sizeof(name), "XmtGlobal%d", i);
        HANDLE h = OpenFileMappingA(FILE_MAP_READ, FALSE, name);
        if (h) {
            CloseHandle(h);
            dumpXmt(name);
        }
    }
    return 0;
}
