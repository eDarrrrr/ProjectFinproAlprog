#ifndef COMMON_H
#define COMMON_H

#include <string>
#include <vector>
#include <chrono>
#include <ctime> 
#include <iomanip> 
#include <sstream> 

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#define SOCKET int
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define closesocket close
#endif

struct ScanLog {
    std::string studentID;
    time_t timestamp;
    bool operator<(const ScanLog& other) const {
        return timestamp < other.timestamp;
    }
};

#endif 