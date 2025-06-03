#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <algorithm>
#include <thread>
#include <mutex>
#include <sstream>
#include <cstring>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "Ws2_32.lib")
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

#include "json.hpp"
#include "common.h"

using json = nlohmann::json;

namespace nlohmann {
    template <>
    struct adl_serializer<LogEntry> {
        static void to_json(json& j, const LogEntry& entry) {
            j = json{{"studentId", std::string(entry.studentId)}, {"timestamp", entry.timestamp}, {"type", entry.type}};
        }
    };
}

std::string formatTimestamp(std::time_t t) {
    std::stringstream ss;
    ss << std::put_time(std::localtime(&t), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

class LogManager {
private:
    std::vector<LogEntry> logs_in_memory;
    std::string binaryLogFile;
    std::string jsonLogFile;
    mutable std::mutex logMutex;

public:
    LogManager(const std::string& binFile = "server_attendance_log.bin",
               const std::string& jsonFile = "server_attendance_report.json")
        : binaryLogFile(binFile), jsonLogFile(jsonFile) {
        loadFromBinary();
    }

    void addLog(const LogEntry& entry) {
        std::lock_guard<std::mutex> lock(logMutex);
        logs_in_memory.push_back(entry);
        std::cout << "[SERVER LOG] Diterima: ID: " << entry.studentId
                  << ", Tipe: " << entryTypeToString(entry.type)
                  << ", Waktu: " << formatTimestamp(entry.timestamp) << std::endl;
        appendToBinary(entry);
    }

    void appendToBinary(const LogEntry& entry) {
        std::ofstream outFile(binaryLogFile, std::ios::binary | std::ios::app);
        if (!outFile) {
            std::cerr << "Error: Tidak bisa membuka file biner untuk append: " << binaryLogFile << std::endl;
            return;
        }
        outFile.write(reinterpret_cast<const char*>(&entry), sizeof(LogEntry));
        outFile.close();
    }

    void loadFromBinary() {
        std::lock_guard<std::mutex> lock(logMutex);
        std::ifstream inFile(binaryLogFile, std::ios::binary);
        if (!inFile) {
            std::cout << "Info: File log biner (" << binaryLogFile << ") tidak ditemukan. Memulai dengan log baru." << std::endl;
            return;
        }

        logs_in_memory.clear();
        LogEntry entry;
        while (inFile.read(reinterpret_cast<char*>(&entry), sizeof(LogEntry))) {
            logs_in_memory.push_back(entry);
        }
        inFile.close();
        std::cout << "Info: " << logs_in_memory.size() << " entri log dimuat dari " << binaryLogFile << std::endl;
    }

    void exportToJson() {
        std::lock_guard<std::mutex> lock(logMutex);
        if (logs_in_memory.empty()) {
            std::cout << "Tidak ada log untuk diekspor." << std::endl;
            return;
        }
        json jsonData = logs_in_memory;

        std::ofstream outFile(jsonLogFile);
        if (!outFile) {
            std::cerr << "Error: Tidak bisa membuka file JSON untuk tulis: " << jsonLogFile << std::endl;
            return;
        }
        outFile << jsonData.dump(4);
        outFile.close();
        std::cout << "Info: Log diekspor ke " << jsonLogFile << std::endl;
    }

    std::vector<LogEntry> searchById(const std::string& studentId_str) {
        std::lock_guard<std::mutex> lock(logMutex);
        std::vector<LogEntry> results;
        for (const auto& entry : logs_in_memory) {
            if (std::string(entry.studentId) == studentId_str) {
                results.push_back(entry);
            }
        }
        std::sort(results.begin(), results.end(), [](const LogEntry& a, const LogEntry& b) {
            return a.timestamp < b.timestamp;
        });
        return results;
    }

    void displayLogs(const std::vector<LogEntry>& entriesToDisplay) const {
        if (entriesToDisplay.empty()) {
            std::cout << "Tidak ada log untuk ditampilkan." << std::endl;
            return;
        }
        std::cout << "\n--- Daftar Log ---" << std::endl;
        std::cout << std::left << std::setw(15) << "ID"
                  << std::setw(25) << "Timestamp"
                  << std::setw(10) << "Tipe" << std::endl;
        std::cout << std::string(50, '-') << std::endl;
        for (const auto& entry : entriesToDisplay) {
            std::cout << std::left << std::setw(15) << entry.studentId
                      << std::setw(25) << formatTimestamp(entry.timestamp)
                      << std::setw(10) << entryTypeToString(entry.type) << std::endl;
        }
        std::cout << "------------------" << std::endl;
    }
     std::vector<LogEntry> getAllLogsSortedByTime() {
        std::lock_guard<std::mutex> lock(logMutex);
        std::vector<LogEntry> sortedLogs = logs_in_memory;
        std::sort(sortedLogs.begin(), sortedLogs.end(), [](const LogEntry& a, const LogEntry& b) {
            return a.timestamp < b.timestamp;
        });
        return sortedLogs;
    }
};

LogManager logManager;

void handle_client_connection(SOCKET client_socket) {
    std::cout << "Klien terhubung. Menunggu data..." << std::endl;
    char buffer[sizeof(LogEntry)];
    int bytes_received;

    while ((bytes_received = recv(client_socket, buffer, sizeof(LogEntry), 0)) > 0) {
        if (bytes_received == sizeof(LogEntry)) {
            LogEntry received_entry;
            memcpy(&received_entry, buffer, sizeof(LogEntry));
            logManager.addLog(received_entry);
        } else if (bytes_received < sizeof(LogEntry) && bytes_received > 0) {
            std::cerr << "Menerima paket data yang tidak lengkap. Ukuran: " << bytes_received << std::endl;
        } else {
            std::cerr << "Error saat recv atau data tidak valid." << std::endl;
        }
    }

    if (bytes_received == 0) {
        std::cout << "Klien menutup koneksi." << std::endl;
    } else if (bytes_received == SOCKET_ERROR) {
        #ifdef _WIN32
            std::cerr << "recv failed dengan error: " << WSAGetLastError() << std::endl;
        #else
            perror("recv failed");
        #endif
    }
    closesocket(client_socket);
    std::cout << "Koneksi klien ditutup." << std::endl;
}

void server_admin_interface() {
    std::string command;
    while(true) {
        std::cout << "\n[ADMIN SERVER] Masukkan perintah ('export', 'search [ID]', 'displayall', 'exit'): ";
        std::getline(std::cin, command);

        if (command == "export") {
            logManager.exportToJson();
        } else if (command.rfind("search ", 0) == 0) {
            std::string id_to_search = command.substr(7);
            if (!id_to_search.empty()) {
                std::vector<LogEntry> results = logManager.searchById(id_to_search);
                logManager.displayLogs(results);
            } else {
                std::cout << "ID tidak boleh kosong untuk pencarian." << std::endl;
            }
        } else if (command == "displayall") {
            std::vector<LogEntry> allLogs = logManager.getAllLogsSortedByTime();
            logManager.displayLogs(allLogs);
        }
        else if (command == "exit") {
            std::cout << "Server admin interface ditutup. Server utama mungkin masih berjalan." << std::endl;
            break;
        } else {
            std::cout << "Perintah tidak dikenal." << std::endl;
        }
    }
}

int main() {
#ifdef _WIN32
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        std::cerr << "WSAStartup failed: " << result << std::endl;
        return 1;
    }
#endif

    SOCKET listen_socket = INVALID_SOCKET;
    SOCKET client_socket = INVALID_SOCKET;

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(SERVER_PORT);

    listen_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_socket == INVALID_SOCKET) {
        std::cerr << "Error membuat socket: "
                  #ifdef _WIN32
                  << WSAGetLastError()
                  #else
                  << strerror(errno)
                  #endif
                  << std::endl;
        #ifdef _WIN32
            WSACleanup();
        #endif
        return 1;
    }

    if (bind(listen_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        std::cerr << "Bind failed: "
                  #ifdef _WIN32
                  << WSAGetLastError()
                  #else
                  << strerror(errno)
                  #endif
                  << std::endl;
        closesocket(listen_socket);
        #ifdef _WIN32
            WSACleanup();
        #endif
        return 1;
    }

    if (listen(listen_socket, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "Listen failed: "
                  #ifdef _WIN32
                  << WSAGetLastError()
                  #else
                  << strerror(errno)
                  #endif
                  << std::endl;
        closesocket(listen_socket);
        #ifdef _WIN32
            WSACleanup();
        #endif
        return 1;
    }

    std::cout << "Server mendengarkan di port " << SERVER_PORT << "..." << std::endl;

    std::thread admin_thread(server_admin_interface);
    admin_thread.detach();

    while (true) {
        client_socket = accept(listen_socket, NULL, NULL);
        if (client_socket == INVALID_SOCKET) {
            std::cerr << "Accept failed: "
                      #ifdef _WIN32
                      << WSAGetLastError()
                      #else
                      << strerror(errno)
                      #endif
                      << std::endl;
            continue;
        }
        std::thread client_handler_thread(handle_client_connection, client_socket);
        client_handler_thread.detach();
    }

    closesocket(listen_socket);
#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}