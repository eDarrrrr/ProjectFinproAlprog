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
#include <map>
#include <sstream>
#include <regex>
#include <algorithm>
#include <vector>
#include <fstream>

#pragma comment(lib,"ws2_32.lib")
#define PORT 8888

using namespace std;

vector<SOCKET> client_sockets; // Simpan semua client
mutex clients_mutex;
bool server_running = true;
map<string, string> npm_to_nama;

string getTimestamp() {
    auto now = chrono::system_clock::now();
    time_t t_now = chrono::system_clock::to_time_t(now);
    tm* local_tm = localtime(&t_now);
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", local_tm);
    return string(buf);
}

struct LogRecord {
    char npm[32];
    char nama[64];
    char waktu[32];
    char hasil[16]; // "BERHASIL" / "GAGAL"
};

void saveLogBiner(const LogRecord& rec, const std::string& filename="log_harian.bin") {
    std::ofstream out(filename, std::ios::binary | std::ios::app);
    out.write(reinterpret_cast<const char*>(&rec), sizeof(LogRecord));
    out.close();
}

// Fungsi ekspor seluruh log biner ke JSON
void exportLogToJson(const std::string& binFile = "log_harian.bin", const std::string& jsonFile = "log_harian.json") {
    std::ifstream in(binFile, std::ios::binary);
    if (!in) {
        std::cout << "[Server] Tidak ada log biner.\n";
        return;
    }
    std::vector<LogRecord> logs;
    LogRecord temp;
    while (in.read(reinterpret_cast<char*>(&temp), sizeof(LogRecord))) {
        logs.push_back(temp);
    }
    in.close();

    std::ofstream out(jsonFile);
    out << "[\n";
    for (size_t i = 0; i < logs.size(); ++i) {
        out << "  {\n";
        out << "    \"npm\": \"" << logs[i].npm << "\",\n";
        out << "    \"nama\": \"" << logs[i].nama << "\",\n";
        out << "    \"waktu\": \"" << logs[i].waktu << "\",\n";
        out << "    \"hasil\": \"" << logs[i].hasil << "\"\n";
        out << "  }";
        if (i != logs.size() - 1) out << ",";
        out << "\n";
    }
    out << "]";
    out.close();

    std::cout << "[Server] Log harian diekspor ke " << jsonFile << std::endl;
}
void findLogByNPM(const string& npm) {
    ifstream logfile("log.txt");
    if (!logfile) {
        cout << "[Server] Log file belum ada.\n";
        return;
    }
    cout << "\n=== LOG untuk NPM: " << npm << " ===" << endl;
    string line;
    bool found = false;
    regex pattern("." + npm + ".");
    while (getline(logfile, line)) {
        if (regex_search(line, pattern)) {
            cout << line << endl;
            found = true;
        }
    }
    if (!found) cout << "[Server] Tidak ditemukan log untuk NPM tersebut.\n";
    cout << "=== END LOG ===\n" << endl;
}

void loadMahasiswaDB(const string& filename) {
    ifstream dbfile(filename);
    if (!dbfile) {
        cout << "[Server] Database mahasiswa tidak ditemukan.\n";
        return;
    }
    string line;
    getline(dbfile, line); // skip header
    while (getline(dbfile, line)) {
        stringstream ss(line);
        string npm, nama;
        getline(ss, npm, ',');
        getline(ss, nama, ',');
        npm_to_nama[npm] = nama;
    }
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
        string npm = buffer;
        string balasan;
        string loginfo;
        string nama;
        string timestamp = getTimestamp();
        string hasil;

        auto it = npm_to_nama.find(npm);
        if (it != npm_to_nama.end()) {
            nama = it->second;
            hasil = "BERHASIL";
            balasan = "Berhasil absen " + nama + " (" + npm + ") pada " + timestamp;
            loginfo = "[ABSEN] [" + timestamp + "] " + npm + " - " + nama + " BERHASIL absen.";
        } else {
            nama = "";
            hasil = "GAGAL";
            balasan = "NPM tidak terdaftar";
            loginfo = "[ABSEN] [" + timestamp + "] " + npm + " GAGAL absen (tidak terdaftar).";
        }
        send(client_socket, balasan.c_str(), balasan.length(), 0);
        logMessage(client_id, loginfo);

        LogRecord rec;
        strncpy(rec.npm, npm.c_str(), sizeof(rec.npm));
        strncpy(rec.nama, nama.c_str(), sizeof(rec.nama));
        strncpy(rec.waktu, timestamp.c_str(), sizeof(rec.waktu));
        strncpy(rec.hasil, hasil.c_str(), sizeof(rec.hasil));
        saveLogBiner(rec);

        cout << "[Client " << client_id << "]: " << npm << " --> " << balasan << endl;
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
    while (server_running) {
        cout << "\n[Server Command] > ";
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
            showLog();
        } else if (cmd.rfind("find ", 0) == 0) {  // starts with "find "
            string npm = cmd.substr(5);
            findLogByNPM(npm);
        }else if (cmd == "exportjson") {
            exportLogToJson();
        } else {
            cout << "[Server] Command tidak dikenali. Gunakan: list, log, find <npm>, exit/quit, exportjson" << endl;
        }
    }
}


int main() {
    WSADATA wsa;
    SOCKET listen_socket;
    struct sockaddr_in server, client;
    int c = sizeof(struct sockaddr_in);

    loadMahasiswaDB("mahasiswa.csv");

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

