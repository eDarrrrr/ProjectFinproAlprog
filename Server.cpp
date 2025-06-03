// Server.cpp
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include "Common.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>       // untuk SetConsoleOutputCP, SetConsoleCP
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

// Mutex untuk proteksi akses ke allLogs, file biner, lastStatus, dan validUsers
std::mutex g_logsMutex;

// Vector in‐memory untuk menyimpan semua log hari ini (LogEntry hanya npm+timestamp)
std::vector<LogEntry> allLogs;

// Map untuk menyimpan status terakhir (masuk/keluar) tiap NPM
//   false = “di luar” (atau belum pernah),  true = “di dalam”
std::unordered_map<std::string, bool> lastStatus;

// Map untuk menyimpan daftar pengguna valid (npm -> nama), dibaca sekali di startup
std::unordered_map<std::string, std::string> validUsers;

// Utility untuk mendapatkan tanggal hari ini format YYYYMMDD
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

// Nama file biner hari ini
std::string TodayBinaryFilename() {
    return "attendance_" + TodayDateString() + ".bin";
}

// Fungsi untuk menulis satu LogEntry ke file biner (append)
// Pastikan dipanggil dalam kondisi mutex sudah ter‐lock
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

// Fungsi untuk mengekspor semua log hari ini ke JSON (dengan field "npm","name","timestamp","action")
void ExportTodayJson() {
    std::lock_guard<std::mutex> lock(g_logsMutex);

    std::string binName = TodayBinaryFilename();
    if (!std::filesystem::exists(binName)) {
        std::cerr << "[Warning] File biner " << binName << " tidak ditemukan.\n";
        return;
    }

    // Baca ulang semua LogEntry dari file biner
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

    // Kita pakai map temporer untuk menentukan action (Masuk/Keluar) per NPM
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
        // Ambil nama dari validUsers
        std::string nameStr = validUsers.count(npmStr) ? validUsers[npmStr] : "";

        // Tentukan action berdasarkan tempStatus[npmStr]
        bool last = false;
        auto it = tempStatus.find(npmStr);
        if (it != tempStatus.end()) last = it->second;

        std::string action;
        if (!last) {
            action = "Masuk";
            tempStatus[npmStr] = true;
        } else {
            action = "Keluar";
            tempStatus[npmStr] = false;
        }

        // Format timestamp ke string "YYYY-MM-DD HH:MM:SS"
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

        // Tulis object JSON: npm, name, timestamp, action
        ofs << "  {\n";
        ofs << "    \"npm\": \"" << npmStr << "\",\n";
        ofs << "    \"name\": \"" << nameStr << "\",\n";
        ofs << "    \"timestamp\": \"" << timeStr.str() << "\",\n";
        ofs << "    \"action\": \"" << action << "\"\n";
        if (i + 1 == logsFromFile.size()) ofs << "  }\n";
        else                             ofs << "  },\n";
    }
    ofs << "]\n";
    ofs.close();

    std::cout << "[Info] Ekspor JSON selesai: " << jsonName << "\n";
}

// Fungsi untuk mencari data berdasarkan NPM (tampilkan nama, waktu, status)
void SearchByNPM(const std::string &searchNpm) {
    std::lock_guard<std::mutex> lock(g_logsMutex);

    // 1) Filter semua LogEntry yang NPM-nya sama dengan searchNpm
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

    // 2) Urutkan hasil berdasarkan timestamp (ascending)
    std::sort(results.begin(), results.end(), [](const LogEntry &a, const LogEntry &b) {
        return a.timestamp < b.timestamp;
    });

    // 3) Iterasi dan flip status (false=“di luar”, true=“di dalam”) untuk menentukan action
    bool localStatus = false;
    std::string nameStr = validUsers.count(searchNpm) ? validUsers[searchNpm] : "";
    std::cout << "[Search] Hasil pencarian untuk NPM = " << searchNpm
              << " ── Nama: " << nameStr << "\n";
    for (auto &e : results) {
        std::string action;
        if (!localStatus) {
            action = "Masuk";
            localStatus = true;
        } else {
            action = "Keluar";
            localStatus = false;
        }

        // Format timestamp
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

        // Tampilkan NPM, Waktu, dan Status
        std::cout << "  NPM: " << e.npm
                  << " ── Waktu: " << oss.str()
                  << " ── " << action << "\n";
    }
}

// Fungsi untuk memuat daftar pengguna valid dari file users.txt
void LoadValidUsers(const std::string &userFile) {
    std::ifstream ifs(userFile);
    if (!ifs) {
        std::cerr << "[Warning] File pengguna '" << userFile << "' tidak ditemukan. Semua absen akan ditolak.\n";
        return;
    }
    std::string line;
    while (std::getline(ifs, line)) {
        // Format tiap baris: npm,nama
        std::istringstream iss(line);
        std::string npm, nama;
        if (!std::getline(iss, npm, ',')) continue;
        if (!std::getline(iss, nama)) continue;
        // Trim spasi
        while (!npm.empty() && isspace(npm.back())) npm.pop_back();
        while (!npm.empty() && isspace(npm.front())) npm.erase(npm.begin());
        while (!nama.empty() && isspace(nama.back())) nama.pop_back();
        while (!nama.empty() && isspace(nama.front())) nama.erase(nama.begin());

        if (!npm.empty() && !nama.empty()) {
            validUsers[npm] = nama;
        }
    }
    ifs.close();
    std::cout << "[Info] Daftar pengguna valid dimuat (" << validUsers.size() << " entri).\n";
}

// Fungsi yang dijalankan tiap kali terjadi koneksi dari client
void HandleClient(SOCKET clientSocket) {
    // Terima satu LogEntry (hanya npm & timestamp)
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
    // Pastikan string terminate
    entry.npm[MAX_NPM_LEN - 1] = '\0';
    std::string npmStr(entry.npm);

    // Validasi NPM: harus ada di validUsers
    {
        std::lock_guard<std::mutex> lock(g_logsMutex);
        auto it = validUsers.find(npmStr);
        if (it == validUsers.end()) {
            const char *msg = "ERROR: NPM tidak terdaftar.\n";
            send(clientSocket, msg, static_cast<int>(strlen(msg)), 0);
            closesocket(clientSocket);
            return;
        }
    }

    // Jika lolos validasi, lanjutkan pencatatan (tanpa input nama dari client)
    std::string action;
    std::string nameStr = validUsers[npmStr];

    {
        std::lock_guard<std::mutex> lock(g_logsMutex);
        // Cek status terakhir untuk npm ini
        bool last = false;
        auto it2 = lastStatus.find(npmStr);
        if (it2 != lastStatus.end()) {
            last = it2->second;
        }
        if (!last) {
            action = "Masuk";
            lastStatus[npmStr] = true;
        } else {
            action = "Keluar";
            lastStatus[npmStr] = false;
        }

        // Tambahkan ke vector in‐memory dan ke file biner
        allLogs.push_back(entry);
        AppendLogToBinary(entry);

        // Cetak ke console: sertakan NPM, Nama (dari validUsers), timestamp, action
        std::cout << "[Log] NPM=" << entry.npm
                  << ", Nama=" << nameStr
                  << ", timestamp=" << entry.timestamp
                  << " ── " << action << "\n";
    }

    // Kirim ACK “OK” ke client
    const char *ack = "OK";
    send(clientSocket, ack, static_cast<int>(strlen(ack)), 0);

    closesocket(clientSocket);
}

int main() {
    // 0) Load daftar pengguna valid dari file “users.txt”
    LoadValidUsers("users.txt");

    // 1) Atur console ke UTF-8 agar Unicode bisa tampil (opsional)
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    // 2) Inisialisasi Winsock
    WSADATA wsaData;
    int wsResult = WSAStartup(MAKEWORD(2,2), &wsaData);
    if (wsResult != 0) {
        std::cerr << "[Error] WSAStartup gagal: " << wsResult << "\n";
        return 1;
    }

    // 3) Buat socket listening
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
    std::cout << "— Ketik \"search <NPM>\" untuk mencari, \"export\" untuk ekspor JSON, \"exit\" untuk berhenti.\n\n";

    // 4) Thread terpisah untuk menerima koneksi client
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

    // 5) Loop perintah admin di console (search, export, exit)
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

    // 6) Bersihkan
    closesocket(listenSocket);
    WSACleanup();
    return 0;
}
