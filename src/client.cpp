#include "client.h"
#include <iostream>
#include <string>    
#include <cstring>   
#include <cstdlib>   
#include <thread>    
#include <chrono>    

#ifdef _WIN32
#pragma comment(lib, "ws2_32.lib") 
#endif

const int SERVER_PORT = 8080;
const char* SERVER_IP = "127.0.0.1";

int main() {
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed." << std::endl;
        return 1;
    }
#endif

    while (true) {
        std::cout << "Enter Student ID (or type 'exit' to quit): ";
        std::string student_id;
        if (!std::getline(std::cin, student_id)) {
            if(std::cin.eof()) { 
                std::cout << "EOF detected, exiting client." << std::endl;
                break;
            }
            std::cin.clear(); 
            std::cout << "Error reading input." << std::endl;
            continue;
        }


        if (student_id == "exit") {
            break;
        }
        if (student_id.empty()) {
            std::cout << "Student ID cannot be empty." << std::endl;
            continue;
        }

        SOCKET sock = INVALID_SOCKET;
        struct sockaddr_in serv_addr;

        if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
            std::cerr << "Socket creation error" << std::endl;
            continue;
        }

        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(SERVER_PORT);
        
        if(inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr) <= 0) {
            std::cerr << "Invalid address/ Address not supported: " << SERVER_IP << std::endl;
            closesocket(sock);
            continue;
        }

        if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
            std::cerr << "Connection Failed to server " << SERVER_IP << ":" << SERVER_PORT << std::endl;
            #ifdef _WIN32
            std::cerr << "Winsock error: " << WSAGetLastError() << std::endl;
            #else
            perror("Connection error");
            #endif
            closesocket(sock);
            std::cout << "Ensure the server is running and accessible." << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(2)); 
            continue;
        }

        time_t current_time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        std::string message = student_id + ":" + std::to_string(current_time);
        
        int bytes_sent = send(sock, message.c_str(), static_cast<int>(message.length()), 0);
        if (bytes_sent == SOCKET_ERROR) {
            std::cerr << "Send failed." << std::endl;
            closesocket(sock);
            continue;
        }
        std::cout << "Scan sent: ID=" << student_id << ", Timestamp=" << current_time << std::endl;
        
        char buffer[1024] = {0};
        int valread = recv(sock, buffer, sizeof(buffer) - 1, 0);
        if (valread > 0) {
            buffer[valread] = '\0';
            std::cout << "Server response: " << buffer << std::endl;
        } else if (valread == 0) {
             std::cout << "Server closed connection." << std::endl;
        } else {
            #ifdef _WIN32
            std::cerr << "recv failed with error: " << WSAGetLastError() << std::endl;
            #else
            perror("recv failed");
            #endif
        }
        closesocket(sock);
        std::cout << std::endl;
    }

#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}