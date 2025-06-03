// Common.h
#pragma once

#include <ctime>
#include <cstdint>

// Panjang maksimum NPM (termasuk null‐terminator)
static const size_t MAX_NPM_LEN = 32;

// Struktur data log attendance yang dikirimkan oleh client
#pragma pack(push, 1)
struct LogEntry {
    char npm[MAX_NPM_LEN];   // NPM mahasiswa (terminated dengan '\0')
    std::int64_t timestamp;  // UNIX timestamp (int64_t agar konsisten di Windows)
};
#pragma pack(pop)
