#ifndef COMMON_H
#define COMMON_H

#include <string>
#include <ctime>
#include <cstring>

#define SERVER_PORT 8080
#define BUFFER_SIZE 1024

enum class EntryType : char {
    ENTRY_IN  = 'I',
    ENTRY_OUT = 'O'
};

inline std::string entryTypeToString(EntryType type) {
    if (type == EntryType::ENTRY_IN)  return "MASUK";
    else                               return "KELUAR";
}

struct LogEntry {
    char studentId[50];
    std::time_t timestamp;
    EntryType type;

    LogEntry() : timestamp(0), type(EntryType::ENTRY_IN) {
        studentId[0] = '\0';
    }

    LogEntry(const std::string& id, std::time_t ts, EntryType et) : timestamp(ts), type(et) {
        std::strncpy(studentId, id.c_str(), sizeof(studentId) - 1);
        studentId[sizeof(studentId) - 1] = '\0';
    }
};

#endif  
