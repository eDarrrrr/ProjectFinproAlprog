#include <iostream>
#include <thread>
#include <vector>
#include <string>
#include <mutex>
#include <winsock2.h>
#include <fstream>
#include <chrono>
#include <ctime>
#include <iomanip>

#pragma comment(lib,"ws2_32.lib")
#define PORT 8888

using namespace std;

vector<SOCKET> client_sockets; // Simpan semua client
mutex clients_mutex;
bool server_running = true;

string getTimestamp() {
    auto now = chrono::system_clock::now();
    time_t t_now = chrono::system_clock::to_time_t(now);
    tm* local_tm = localtime(&t_now);
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", local_tm);
    return string(buf);
}

void logMessage(int client_id, const string& msg) {
    ofstream logfile("log.txt", ios::app); // append mode
    logfile << "[" << getTimestamp() << "] Client " << client_id << ": " << msg << endl;
}

void clientHandler(SOCKET client_socket, int client_id) {
    char buffer[1024];
    while (server_running) {
        int recv_size = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
        if (recv_size <= 0) {
            cout << "[Client " << client_id << "] disconnected." << endl;
            break;
        }
        buffer[recv_size] = '\0';
        cout << "[Client " << client_id << "]: " << buffer << endl;
        logMessage(client_id, buffer);  // <--- LOG di sini
    }
    closesocket(client_socket);

    // Hapus client dari list
    lock_guard<mutex> lock(clients_mutex);
    for (auto it = client_sockets.begin(); it != client_sockets.end(); ++it) {
        if (*it == client_socket) {
            client_sockets.erase(it);
            break;
        }
    }
}

void showLog() {
    ifstream logfile("log.txt");
    if (!logfile) {
        cout << "[Server] Log file belum ada.\n";
        return;
    }
    cout << "\n=== LOG PESAN ===" << endl;
    string line;
    while (getline(logfile, line)) {
        cout << line << endl;
    }
    cout << "=== END LOG ===\n" << endl;
}

void broadcastShutdown() {
    string shutdownMsg = "Server shutting down, you will be disconnected";
    lock_guard<mutex> lock(clients_mutex);
    for (SOCKET s : client_sockets) {
        send(s, shutdownMsg.c_str(), shutdownMsg.length(), 0);
    }
}

void serverCommandPrompt() {
    string cmd;
    cout << "\n[Server Command] > ";
    while (server_running) {
        getline(cin, cmd);
        if (cmd == "exit" || cmd == "quit") {
            cout << "Shutting down server..." << endl;
            server_running = false;
            broadcastShutdown();
            lock_guard<mutex> lock(clients_mutex);
            for (SOCKET s : client_sockets) closesocket(s);
            break;
        } else if (cmd == "list") {
            lock_guard<mutex> lock(clients_mutex);
            cout << "[Server] Clients connected: " << client_sockets.size() << endl;
            int i = 0;
            for (SOCKET s : client_sockets) cout << "  Client ID: " << i++ << " SOCKET: " << s << endl;
        } else if (cmd == "log") {
            showLog(); // <--- COMMAND LOG
        } else {
            cout << "[Server] Command tidak dikenali. Gunakan: list, log, exit/quit" << endl;
        }
    }
}

int main() {
    WSADATA wsa;
    SOCKET listen_socket;
    struct sockaddr_in server, client;
    int c = sizeof(struct sockaddr_in);

    cout << "Initialising Winsock...\n";
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        cerr << "WSAStartup failed. Error code: " << WSAGetLastError() << endl;
        return 1;
    }

    listen_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_socket == INVALID_SOCKET) {
        cerr << "Could not create socket. Error code: " << WSAGetLastError() << endl;
        WSACleanup();
        return 1;
    }

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(PORT);

    if (bind(listen_socket, (struct sockaddr*)&server, sizeof(server)) == SOCKET_ERROR) {
        cerr << "Bind failed. Error code: " << WSAGetLastError() << endl;
        closesocket(listen_socket);
        WSACleanup();
        return 1;
    }

    listen(listen_socket, 10);
    cout << "Server running, waiting for clients on port " << PORT << "...\n";

    thread serverCmd(serverCommandPrompt);

    int client_id = 0;
    while (server_running) {
        SOCKET client_socket = accept(listen_socket, (struct sockaddr*)&client, &c);
        if (client_socket == INVALID_SOCKET) {
            if (!server_running) break;
            cerr << "Accept failed. Error code: " << WSAGetLastError() << endl;
            continue;
        }
        // Tambah ke list client
        {
            lock_guard<mutex> lock(clients_mutex);
            client_sockets.push_back(client_socket);
        }
        cout << "Client " << client_id << " connected. SOCKET: " << client_socket << endl;
        thread(clientHandler, client_socket, client_id).detach();
        client_id++;
    }

    closesocket(listen_socket);
    serverCmd.join();
    WSACleanup();
    cout << "Server shut down." << endl;
    return 0;
}
