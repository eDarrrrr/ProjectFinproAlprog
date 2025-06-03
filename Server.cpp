
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include "Common.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <thread>
#include <vector>
#include <mutex>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <filesystem>
#include <unordered_map>
#include <string>


#pragma comment(lib, "Ws2_32.lib")

static const unsigned short SERVER_PORT = 54000;


std::mutex g_logsMutex;


std::vector<LogEntry> allLogs;



std::unordered_map<std::string, bool> lastStatus;


std::string TodayDateString() {
    std::time_t t = std::time(nullptr);
    std::tm tm{};
#if defined(_MSC_VER)
    localtime_s(&tm, &t);
#else
    tm = *std::localtime(&t);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y%m%d");
    return oss.str();
}


std::string TodayBinaryFilename() {
    return "attendance_" + TodayDateString() + ".bin";
}



void AppendLogToBinary(const LogEntry &entry) {
    std::string fname = TodayBinaryFilename();
    std::ofstream ofs(fname, std::ios::binary | std::ios::app);
    if (!ofs) {
        std::cerr << "[Error] Gagal membuka file biner: " << fname << std::endl;
        return;
    }
    ofs.write(reinterpret_cast<const char*>(&entry), sizeof(LogEntry));
    ofs.close();
}


void ExportTodayJson() {
    std::lock_guard<std::mutex> lock(g_logsMutex);

    
    std::string binName = TodayBinaryFilename();
    if (!std::filesystem::exists(binName)) {
        std::cerr << "[Warning] File biner " << binName << " tidak ditemukan.\n";
        return;
    }

    
    std::ifstream ifs(binName, std::ios::binary);
    if (!ifs) {
        std::cerr << "[Error] Gagal membuka file biner untuk ekspor JSON: " << binName << std::endl;
        return;
    }
    std::vector<LogEntry> logsFromFile;
    LogEntry tmp;
    while (ifs.read(reinterpret_cast<char*>(&tmp), sizeof(LogEntry))) {
        
        tmp.npm[MAX_NPM_LEN - 1] = '\0';
        logsFromFile.push_back(tmp);
    }
    ifs.close();

    
    std::unordered_map<std::string, bool> tempStatus;
    
    std::string jsonName = "attendance_" + TodayDateString() + ".json";
    std::ofstream ofs(jsonName);
    if (!ofs) {
        std::cerr << "[Error] Gagal menulis file JSON: " << jsonName << std::endl;
        return;
    }

    ofs << "[\n";
    for (size_t i = 0; i < logsFromFile.size(); ++i) {
        const auto &e = logsFromFile[i];
        std::string npmStr = std::string(e.npm);

        
        bool last = false;
        auto it = tempStatus.find(npmStr);
        if (it != tempStatus.end()) {
            last = it->second;
        }
        std::string action;
        if (!last) {
            action = "Masuk";
            tempStatus[npmStr] = true;
        } else {
            action = "Keluar";
            tempStatus[npmStr] = false;
        }

        
        std::time_t ts = static_cast<time_t>(e.timestamp);
#if defined(_MSC_VER)
        std::tm tmBuf{};
        localtime_s(&tmBuf, &ts);
        std::ostringstream timeStr;
        timeStr << std::put_time(&tmBuf, "%Y-%m-%d %H:%M:%S");
#else
        std::tm * tmBuf = std::localtime(&ts);
        std::ostringstream timeStr;
        timeStr << std::put_time(tmBuf, "%Y-%m-%d %H:%M:%S");
#endif

        
        ofs << "  {\n";
        ofs << "    \"npm\": \"" << e.npm << "\",\n";
        ofs << "    \"timestamp\": \"" << timeStr.str() << "\",\n";
        ofs << "    \"action\": \"" << action << "\"\n";
        if (i + 1 == logsFromFile.size()) ofs << "  }\n";
        else                             ofs << "  },\n";
    }
    ofs << "]\n";
    ofs.close();

    std::cout << "[Info] Ekspor JSON selesai: " << jsonName << "\n";
}



void SearchByNPM(const std::string &searchNpm) {
    std::lock_guard<std::mutex> lock(g_logsMutex);

       
    std::vector<LogEntry> results;
    for (auto &e : allLogs) {
        if (searchNpm == std::string(e.npm)) {
            results.push_back(e);
        }
    }

    if (results.empty()) {
        std::cout << "[Search] Tidak ditemukan log dengan NPM: " << searchNpm << "\n";
        return;
    }

       
    std::sort(results.begin(), results.end(), [](const LogEntry &a, const LogEntry &b) {
        return a.timestamp < b.timestamp;
    });

       
    bool localStatus = false;    
    std::cout << "[Search] Hasil pencarian untuk NPM = " << searchNpm << ":\n";
    for (auto &e : results) {
           
        std::string action;
        if (!localStatus) {
            action = "Masuk";
            localStatus = true;
        } else {
            action = "Keluar";
            localStatus = false;
        }

           
        std::time_t ts = static_cast<time_t>(e.timestamp);
    #if defined(_MSC_VER)
        std::tm tmBuf{};
        localtime_s(&tmBuf, &ts);
        std::ostringstream oss;
        oss << std::put_time(&tmBuf, "%Y-%m-%d %H:%M:%S");
    #else
        std::tm *tmBuf = std::localtime(&ts);
        std::ostringstream oss;
        oss << std::put_time(tmBuf, "%Y-%m-%d %H:%M:%S");
    #endif

           
        std::cout << " -- NPM: " << e.npm
                  << " -- Waktu: " << oss.str()
                  << " -- " << action << "\n";
    }
}


void HandleClient(SOCKET clientSocket) {
    
    LogEntry entry;
    int bytesReceived = recv(clientSocket,
                             reinterpret_cast<char*>(&entry),
                             static_cast<int>(sizeof(LogEntry)),
                             0);
    if (bytesReceived == SOCKET_ERROR || bytesReceived == 0) {
        std::cerr << "[Warning] Gagal menerima data atau koneksi tertutup.\n";
        closesocket(clientSocket);
        return;
    }
    
    entry.npm[MAX_NPM_LEN - 1] = '\0';

    
    std::string npmStr = std::string(entry.npm);

    
    std::string action;
    {
        std::lock_guard<std::mutex> lock(g_logsMutex);

        
        bool last = false;
        auto it = lastStatus.find(npmStr);
        if (it != lastStatus.end()) {
            last = it->second;
        }
        
        if (!last) {
            action = "Masuk";
            lastStatus[npmStr] = true;
        }
        
        else {
            action = "Keluar";
            lastStatus[npmStr] = false;
        }

        
        allLogs.push_back(entry);
        AppendLogToBinary(entry);

        
        std::cout << "[Log] NPM=" << entry.npm
                  << ", timestamp=" << entry.timestamp
                  << " -- " << action << "\n";
    }

    
    const char *ack = "OK";
    send(clientSocket, ack, static_cast<int>(strlen(ack)), 0);

    closesocket(clientSocket);
}

int main() {
    WSADATA wsaData;
    int wsResult = WSAStartup(MAKEWORD(2,2), &wsaData);
    if (wsResult != 0) {
        std::cerr << "[Error] WSAStartup gagal: " << wsResult << "\n";
        return 1;
    }

    
    SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSocket == INVALID_SOCKET) {
        std::cerr << "[Error] Gagal membuat socket: " << WSAGetLastError() << "\n";
        WSACleanup();
        return 1;
    }

    
    sockaddr_in serverHint{};
    serverHint.sin_family = AF_INET;
    serverHint.sin_port = htons(SERVER_PORT);
    serverHint.sin_addr.s_addr = INADDR_ANY; 

    if (bind(listenSocket, reinterpret_cast<sockaddr*>(&serverHint), sizeof(serverHint)) == SOCKET_ERROR) {
        std::cerr << "[Error] Gagal bind: " << WSAGetLastError() << "\n";
        closesocket(listenSocket);
        WSACleanup();
        return 1;
    }

    
    if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "[Error] Listen gagal: " << WSAGetLastError() << "\n";
        closesocket(listenSocket);
        WSACleanup();
        return 1;
    }

    std::cout << "[Info] Server berjalan di port " << SERVER_PORT << "\n";
    std::cout << "Ketik \"search <NPM>\" untuk mencari, \"export\" untuk ekspor JSON, \"exit\" untuk berhenti.\n\n";

    
    std::thread listenerThread([&]() {
        while (true) {
            sockaddr_in clientAddr{};
            int clientSize = sizeof(clientAddr);
            SOCKET clientSocket = accept(listenSocket, reinterpret_cast<sockaddr*>(&clientAddr), &clientSize);
            if (clientSocket == INVALID_SOCKET) {
                std::cerr << "[Warning] accept() error: " << WSAGetLastError() << "\n";
                continue;
            }
            
            std::thread t(HandleClient, clientSocket);
            t.detach();
        }
    });

    
    std::string command;
    while (true) {
        std::getline(std::cin, command);
        if (command.rfind("search ", 0) == 0) {
            std::string npm = command.substr(7);
            if (!npm.empty()) {
                SearchByNPM(npm);
            } else {
                std::cout << "[Usage] search <NPM>\n";
            }
        } else if (command == "export") {
            ExportTodayJson();
        } else if (command == "exit") {
            std::cout << "[Info] Server akan berhenti...\n";
            break;
        } else if (!command.empty()) {
            std::cout << "[Info] Perintah tidak dikenal. Gunakan: search, export, exit.\n";
        }
    }

    
    closesocket(listenSocket);
    WSACleanup();
    return 0;
}
