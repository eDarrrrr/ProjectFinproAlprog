#include "server.h"
#include <iostream>
#include <fstream>
#include <thread>
#include <algorithm>
#include <cstring> 
#include <cstdlib> 
#include <cstdio>  

#ifdef _WIN32
#pragma comment(lib, "ws2_32.lib") 
#endif

const int PORT = 8080;
const char* BINARY_FILE = "attendance_log.bin";
const char* JSON_FILE = "attendance_log.json";

std::vector<ScanLog> g_scan_logs;
std::mutex g_logs_mutex;

std::string format_timestamp(time_t rawtime) {
    std::tm* timeinfo = std::localtime(&rawtime);
    std::ostringstream oss;
    oss << std::put_time(timeinfo, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

void save_to_binary() {
    std::lock_guard<std::mutex> lock(g_logs_mutex);
    std::ofstream outfile(BINARY_FILE, std::ios::binary | std::ios::trunc);
    if (!outfile) {
        std::cerr << "Error: Cannot open binary file for writing: " << BINARY_FILE << std::endl;
        return;
    }
    for (const auto& log : g_scan_logs) {
        uint32_t id_len = static_cast<uint32_t>(log.studentID.length());
        outfile.write(reinterpret_cast<const char*>(&id_len), sizeof(id_len));
        outfile.write(log.studentID.c_str(), id_len);
        outfile.write(reinterpret_cast<const char*>(&log.timestamp), sizeof(log.timestamp));
    }
    outfile.close();
    std::cout << "Logs saved to binary file: " << BINARY_FILE << std::endl;
}

void load_from_binary() {
    std::lock_guard<std::mutex> lock(g_logs_mutex);
    std::ifstream infile(BINARY_FILE, std::ios::binary);
    if (!infile) {
        std::cout << "Info: No existing binary log file found or cannot open: " << BINARY_FILE << ". Starting fresh." << std::endl;
        return;
    }
    g_scan_logs.clear();
    while (infile.peek() != EOF) {
        uint32_t id_len = 0; 
        infile.read(reinterpret_cast<char*>(&id_len), sizeof(id_len));
        if (infile.gcount() != sizeof(id_len)) break;

        if (id_len > 1024*10) { 
             std::cerr << "Error: Unusually large ID length in binary file. Aborting load." << std::endl;
             g_scan_logs.clear(); 
             break;
        }
        std::string studentID(id_len, '\0');
        infile.read(&studentID[0], id_len);
        if (infile.gcount() != id_len) break;
        
        time_t timestamp = 0; 
        infile.read(reinterpret_cast<char*>(&timestamp), sizeof(timestamp));
        if (infile.gcount() != sizeof(timestamp)) break;

        g_scan_logs.push_back({studentID, timestamp});
    }
    infile.close();
    if (!g_scan_logs.empty()) {
        std::cout << "Logs loaded from binary file: " << BINARY_FILE << std::endl;
    }
}

void export_to_json() {
    std::lock_guard<std::mutex> lock(g_logs_mutex);
    std::ofstream outfile(JSON_FILE);
    if (!outfile) {
        std::cerr << "Error: Cannot open JSON file for writing: " << JSON_FILE << std::endl;
        return;
    }
    outfile << "[" << std::endl;
    for (size_t i = 0; i < g_scan_logs.size(); ++i) {
        outfile << "  {" << std::endl;
        outfile << "    \"studentID\": \"" << g_scan_logs[i].studentID << "\"," << std::endl;
        outfile << "    \"timestamp\": \"" << format_timestamp(g_scan_logs[i].timestamp) << "\"" << std::endl;
        outfile << "  }";
        if (i < g_scan_logs.size() - 1) {
            outfile << ",";
        }
        outfile << std::endl;
    }
    outfile << "]" << std::endl;
    outfile.close();
    std::cout << "Logs exported to JSON file: " << JSON_FILE << std::endl;
}

void handle_client(SOCKET client_socket) {
    char buffer[1024] = {0};
    int valread;

    valread = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
    if (valread > 0) {
        buffer[valread] = '\0';
        std::string data(buffer);
        size_t delimiter_pos = data.find(':');
        if (delimiter_pos != std::string::npos) {
            std::string student_id = data.substr(0, delimiter_pos);
            try {
                time_t ts = std::stoll(data.substr(delimiter_pos + 1));
                ScanLog log_entry = {student_id, ts};
                {
                    std::lock_guard<std::mutex> lock(g_logs_mutex);
                    g_scan_logs.push_back(log_entry);
                }
                std::cout << "Received log: ID=" << student_id << ", Timestamp=" << format_timestamp(ts) << std::endl;
                const char* ack = "Data received";
                send(client_socket, ack, static_cast<int>(strlen(ack)), 0);
            } catch (const std::invalid_argument& ia) {
                std::cerr << "Invalid timestamp format from client: " << data.substr(delimiter_pos + 1) << std::endl;
                const char* err_msg = "Invalid timestamp format";
                send(client_socket, err_msg, static_cast<int>(strlen(err_msg)), 0);
            } catch (const std::out_of_range& oor) {
                 std::cerr << "Timestamp out of range from client: " << data.substr(delimiter_pos + 1) << std::endl;
                 const char* err_msg = "Timestamp out of range";
                 send(client_socket, err_msg, static_cast<int>(strlen(err_msg)), 0);
            }
        } else {
            const char* err_msg = "Invalid data format. Expected ID:Timestamp";
            send(client_socket, err_msg, static_cast<int>(strlen(err_msg)), 0);
            std::cerr << "Invalid data format from client." << std::endl;
        }
    } else if (valread == 0) {
        std::cout << "Client disconnected." << std::endl;
    } else {
        #ifdef _WIN32
        int error_code = WSAGetLastError();
        std::cerr << "recv failed with error: " << error_code << std::endl;
        #else
        perror("recv failed");
        #endif
    }
    closesocket(client_socket);
}

void search_logs_by_id(const std::string& student_id) {
    std::lock_guard<std::mutex> lock(g_logs_mutex);
    std::cout << "Search results for ID: " << student_id << std::endl;
    bool found = false;
    for (const auto& log : g_scan_logs) {
        if (log.studentID == student_id) {
            std::cout << "  ID: " << log.studentID << ", Timestamp: " << format_timestamp(log.timestamp) << std::endl;
            found = true;
        }
    }
    if (!found) {
        std::cout << "  No logs found for this ID." << std::endl;
    }
}

void display_sorted_logs() {
    std::vector<ScanLog> sorted_logs;
    {
        std::lock_guard<std::mutex> lock(g_logs_mutex);
        sorted_logs = g_scan_logs;
    }
    std::sort(sorted_logs.begin(), sorted_logs.end());
    std::cout << "All logs sorted by timestamp:" << std::endl;
    for (const auto& log : sorted_logs) {
        std::cout << "  ID: " << log.studentID << ", Timestamp: " << format_timestamp(log.timestamp) << std::endl;
    }
     if (sorted_logs.empty()) {
        std::cout << "  No logs available." << std::endl;
    }
}

volatile bool g_server_running = true;

int main() {
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed." << std::endl;
        return 1;
    }
#endif

    load_from_binary();

    SOCKET server_fd;
    struct sockaddr_in address;
    
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
        perror("socket failed");
        return 1;
    }
    
    int opt = 1;
#ifndef _WIN32
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt SO_REUSEADDR | SO_REUSEPORT failed");
    }
#else
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<char*>(&opt), sizeof(opt))) {
        perror("setsockopt SO_REUSEADDR failed");
    }
#endif

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);
    
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) == SOCKET_ERROR) {
        perror("bind failed");
        closesocket(server_fd);
        return 1;
    }
    if (listen(server_fd, SOMAXCONN) == SOCKET_ERROR) { 
        perror("listen failed");
        closesocket(server_fd);
        return 1;
    }
    
    std::cout << "Server listening on port " << PORT << std::endl;
    std::cout << "Enter command (save_binary, export_json, search <ID>, show_sorted, exit):" << std::endl;

    std::thread server_listener_thread([&]() {
        while (g_server_running) {
            SOCKET new_socket;
            struct sockaddr_in client_address;
            socklen_t client_addrlen = sizeof(client_address);

            
            fd_set readfds;
            FD_ZERO(&readfds);
            FD_SET(server_fd, &readfds);
            struct timeval tv;
            tv.tv_sec = 1; 
            tv.tv_usec = 0;

            int activity = select(server_fd + 1, &readfds, nullptr, nullptr, &tv);

            if (activity < 0 && errno != EINTR) { 
                if (!g_server_running) break; 
                perror("select error");
                continue;
            }

            if (activity > 0 && FD_ISSET(server_fd, &readfds)) {
                 new_socket = accept(server_fd, (struct sockaddr *)&client_address, &client_addrlen);
                 if (new_socket == INVALID_SOCKET) {
                    if (!g_server_running) break;
                    #ifdef _WIN32
                    if (WSAGetLastError() != WSAEINTR && WSAGetLastError() != WSAECONNABORTED && WSAGetLastError() != WSAEWOULDBLOCK) {
                         std::cerr << "accept failed with error: " << WSAGetLastError() << std::endl;
                    }
                    #else
                    if (errno != EINTR && errno != ECONNABORTED && errno != EWOULDBLOCK) {
                        perror("accept failed");
                    }
                    #endif
                    continue; 
                }
                char client_ip[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &client_address.sin_addr, client_ip, INET_ADDRSTRLEN);
                std::cout << "Connection accepted from " << client_ip << ":" << ntohs(client_address.sin_port) << std::endl;
                std::thread(handle_client, new_socket).detach();
            }
        }
        std::cout << "Server listener thread stopped." << std::endl;
    });


    std::string command_line;
    while (g_server_running) {
        std::cout << "> ";
        if (!std::getline(std::cin, command_line)) {
             if (std::cin.eof()){ 
                std::cout << "EOF detected, shutting down..." << std::endl;
                command_line = "exit"; 
             } else {
                std::cin.clear(); 
                std::cout << "Error reading command." << std::endl;
                continue;
             }
        }
        std::istringstream iss(command_line);
        std::string command;
        iss >> command;

        if (command == "save_binary") {
            save_to_binary();
        } else if (command == "export_json") {
            export_to_json();
        } else if (command == "search") {
            std::string student_id;
            if (iss >> student_id) {
                search_logs_by_id(student_id);
            } else {
                std::cout << "Usage: search <student_id>" << std::endl;
            }
        } else if (command == "show_sorted") {
            display_sorted_logs();
        } else if (command == "exit") {
            std::cout << "Shutting down server..." << std::endl;
            g_server_running = false; 
            closesocket(server_fd); 
            break; 
        } else if (!command.empty()) {
            std::cout << "Unknown command: " << command << std::endl;
        }
    }

    if (server_listener_thread.joinable()) {
        server_listener_thread.join();
    }
    
    save_to_binary(); 
    std::cout << "Server shut down gracefully." << std::endl;

#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}