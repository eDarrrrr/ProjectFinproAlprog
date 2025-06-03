
#pragma once

#include <ctime>
#include <cstdint>


static const size_t MAX_NPM_LEN = 32;


#pragma pack(push, 1)
struct LogEntry {
    char npm[MAX_NPM_LEN];   
    std::int64_t timestamp;  
};
#pragma pack(pop)
