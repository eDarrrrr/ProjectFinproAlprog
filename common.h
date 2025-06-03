#ifndef COMMON_H
#define COMMON_H

#include <string>
#include <vector>
#include <ctime>
#include <cstring> // For strncpy

#define SERVER_PORT 8080
#define BUFFER_SIZE 1024

enum class EntryType : char {
    IN = 'I',
    OUT = 'O'
};

inline std::string entryTypeToString(EntryType type) {
    return type == EntryType::IN ? "MASUK" : "KELUAR";
}

struct LogEntry {
    char studentId[50];
    std::time_t timestamp;
    EntryType type;

    LogEntry() : timestamp(0), type(EntryType::IN) {
        studentId[0] = '\0';
    }

    LogEntry(const std::string& id, std::time_t ts, EntryType et) : timestamp(ts), type(et) {
        strncpy(studentId, id.c_str(), sizeof(studentId) - 1);
        studentId[sizeof(studentId) - 1] = '\0';
    }
};

#endif 