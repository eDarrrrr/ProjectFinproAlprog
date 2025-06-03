#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <chrono>
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
    #define SOCKET_ERROR   -1
    #define closesocket close
#endif


#ifdef IN
#  undef IN
#endif
#ifdef OUT
#  undef OUT
#endif

#include "common.h"

std::mutex cout_mtx;


void handle_client(SOCKET client_sock) {
    
    while (true) {
        LogEntry entry;
        int received = recv(client_sock,
                            reinterpret_cast<char*>(&entry),
                            sizeof(LogEntry),
                            0);
        if (received == 0) {
            
            break;
        }
        if (received == SOCKET_ERROR) {
            #ifdef _WIN32
                std::lock_guard<std::mutex> lk(cout_mtx);
                std::cerr << "[SERVER] recv gagal: " << WSAGetLastError() << "\n";
            #else
                std::lock_guard<std::mutex> lk(cout_mtx);
                perror("[SERVER] recv gagal");
            #endif
            break;
        }
        if (received < sizeof(LogEntry)) {
            std::lock_guard<std::mutex> lk(cout_mtx);
            std::cerr << "[SERVER] Data kurang lengkap: diterima "
                      << received << " dari " << sizeof(LogEntry) << "\n";
            break;
        }

        
        std::tm* ptm = std::localtime(&entry.timestamp);
        char timebuf[32];
        std::strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", ptm);
        {
            std::lock_guard<std::mutex> lk(cout_mtx);
            std::cout << "[SERVER] Diterima dari ID:  " << entry.studentId
                      << " | Waktu: " << timebuf
                      << " | Tipe: " << entryTypeToString(entry.type)
                      << "\n";
        }
    }

    closesocket(client_sock);
    {
        std::lock_guard<std::mutex> lk(cout_mtx);
        std::cout << "[SERVER] Koneksi klien ditutup.\n";
    }
}

int main() {
    
    #ifdef _WIN32
        WSADATA wsaData;
        int wsaRes = WSAStartup(MAKEWORD(2,2), &wsaData);
        if (wsaRes != 0) {
            std::cerr << "[SERVER] WSAStartup gagal: " << wsaRes << "\n";
            return 1;
        }
    #endif

    SOCKET listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_sock == INVALID_SOCKET) {
        #ifdef _WIN32
            std::cerr << "[SERVER] Gagal membuat socket: " << WSAGetLastError() << "\n";
            WSACleanup();
        #else
            perror("[SERVER] Gagal membuat socket");
        #endif
        return 1;
    }

    
    sockaddr_in serv_addr{};
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(SERVER_PORT);

    
    if (bind(listen_sock,
             reinterpret_cast<sockaddr*>(&serv_addr),
             sizeof(serv_addr)) == SOCKET_ERROR) {
        #ifdef _WIN32
            std::cerr << "[SERVER] Bind gagal: " << WSAGetLastError() << "\n";
            closesocket(listen_sock);
            WSACleanup();
        #else
            perror("[SERVER] Bind gagal");
        #endif
        return 1;
    }

    
    if (listen(listen_sock, SOMAXCONN) == SOCKET_ERROR) {
        #ifdef _WIN32
            std::cerr << "[SERVER] Listen gagal: " << WSAGetLastError() << "\n";
            closesocket(listen_sock);
            WSACleanup();
        #else
            perror("[SERVER] Listen gagal");
        #endif
        return 1;
    }

    {
        std::lock_guard<std::mutex> lk(cout_mtx);
        std::cout << "[SERVER] Menunggu koneksi di port " << SERVER_PORT << "...\n";
    }

    
    while (true) {
        sockaddr_in client_addr{};
        socklen_t addr_len = sizeof(client_addr);
        SOCKET client_sock = accept(listen_sock,
                                    reinterpret_cast<sockaddr*>(&client_addr),
                                    &addr_len);
        if (client_sock == INVALID_SOCKET) {
            #ifdef _WIN32
                std::lock_guard<std::mutex> lk(cout_mtx);
                std::cerr << "[SERVER] Accept gagal: " << WSAGetLastError() << "\n";
                continue;
            #else
                perror("[SERVER] Accept gagal");
                continue;
            #endif
        }

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET,
                  &client_addr.sin_addr,
                  client_ip,
                  sizeof(client_ip));
        {
            std::lock_guard<std::mutex> lk(cout_mtx);
            std::cout << "[SERVER] Klien terhubung: " << client_ip
                      << ":" << ntohs(client_addr.sin_port) << "\n";
        }

        
        std::thread t(handle_client, client_sock);
        t.detach();
    }

    
    closesocket(listen_sock);
    #ifdef _WIN32
        WSACleanup();
    #endif
    return 0;
}
