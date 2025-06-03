
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include "Common.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <string>
#include <ctime>
#include <cstring>     

#pragma comment(lib, "Ws2_32.lib")

static const char *SERVER_ADDR = "127.0.0.1";
static const unsigned short SERVER_PORT = 54000;

int main() {
    WSADATA wsaData;
    int wsResult = WSAStartup(MAKEWORD(2,2), &wsaData);
    if (wsResult != 0) {
        std::cerr << "[Error] WSAStartup gagal: " << wsResult << "\n";
        return 1;
    }

    
    std::string studentNPM;
    std::cout << "Masukkan NPM Mahasiswa (max " << (MAX_NPM_LEN - 1) << " karakter): ";
    std::getline(std::cin, studentNPM);
    if (studentNPM.empty()) {
        std::cerr << "[Error] NPM tidak boleh kosong.\n";
        WSACleanup();
        return 1;
    }
    if (studentNPM.size() >= MAX_NPM_LEN) {
        std::cerr << "[Error] Panjang NPM melebihi " << (MAX_NPM_LEN - 1) << " karakter.\n";
        WSACleanup();
        return 1;
    }

    
    LogEntry entry{};
    std::memset(entry.npm, 0, sizeof(entry.npm));
    std::memcpy(entry.npm, studentNPM.c_str(), studentNPM.size());
    std::time_t now = std::time(nullptr);
    entry.timestamp = static_cast<std::int64_t>(now);

    
    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        std::cerr << "[Error] Gagal membuat socket: " << WSAGetLastError() << "\n";
        WSACleanup();
        return 1;
    }

    
    sockaddr_in serverHint{};
    serverHint.sin_family = AF_INET;
    inet_pton(AF_INET, SERVER_ADDR, &serverHint.sin_addr);
    serverHint.sin_port = htons(SERVER_PORT);

    
    if (connect(sock, reinterpret_cast<sockaddr*>(&serverHint), sizeof(serverHint)) == SOCKET_ERROR) {
        std::cerr << "[Error] Gagal connect ke server: " << WSAGetLastError() << "\n";
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    
    int sendResult = send(sock, reinterpret_cast<const char*>(&entry), static_cast<int>(sizeof(LogEntry)), 0);
    if (sendResult == SOCKET_ERROR) {
        std::cerr << "[Error] Gagal mengirim data: " << WSAGetLastError() << "\n";
        closesocket(sock);
        WSACleanup();
        return 1;
    }
    std::cout << "[Client] Mengirim scan: NPM=" << entry.npm << ", timestamp=" << entry.timestamp << "\n";

    
    char buf[16];
    int bytesIn = recv(sock, buf, 16, 0);
    if (bytesIn > 0) {
        buf[bytesIn] = '\0';
        std::cout << "[Client] Menerima dari server: " << buf << "\n";
    }

    closesocket(sock);
    WSACleanup();
    return 0;
}
