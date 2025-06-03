#include <iostream>
#include <thread>
#include <string>
#include <winsock2.h>

#define PORT 8888

using namespace std;

void receiveThread(SOCKET sock) {
    char buffer[1024];
    while (true) {
        int recv_size = recv(sock, buffer, sizeof(buffer) - 1, 0);
        if (recv_size <= 0) {
            cout << "\n[INFO] Client disconnected or error.\n";
            break;
        }
        buffer[recv_size] = '\0';
        cout << "\n[Client]: " << buffer << endl;
        cout << ">> ";
        cout.flush();
    }
}

void sendThread(SOCKET sock) {
    string input;
    while (true) {
        cout << ">> ";
        getline(cin, input);
        if (input == "/exit") break;
        send(sock, input.c_str(), input.length(), 0);
    }
}

int main() {
    WSADATA wsa;
    SOCKET listen_socket, client_socket;
    struct sockaddr_in server, client;
    int c;

    cout << "Initialising Winsock...\n";
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        cerr << "WSAStartup failed. Error code: " << WSAGetLastError() << "\n";
        return 1;
    }

    listen_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_socket == INVALID_SOCKET) {
        cerr << "Could not create socket. Error code: " << WSAGetLastError() << "\n";
        WSACleanup();
        return 1;
    }

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(PORT);

    if (bind(listen_socket, (struct sockaddr*)&server, sizeof(server)) == SOCKET_ERROR) {
        cerr << "Bind failed. Error code: " << WSAGetLastError() << "\n";
        closesocket(listen_socket);
        WSACleanup();
        return 1;
    }

    listen(listen_socket, 1);
    cout << "Waiting for a client...\n";
    c = sizeof(struct sockaddr_in);
    client_socket = accept(listen_socket, (struct sockaddr*)&client, &c);
    if (client_socket == INVALID_SOCKET) {
        cerr << "Accept failed. Error code: " << WSAGetLastError() << "\n";
        closesocket(listen_socket);
        WSACleanup();
        return 1;
    }
    cout << "Client connected.\n";

    // Mulai dua thread: menerima dan mengirim
    thread t_recv(receiveThread, client_socket);
    thread t_send(sendThread, client_socket);

    t_send.join();          // Tunggu input user selesai (/exit)
    closesocket(client_socket); // Socket ditutup supaya thread recv juga selesai
    t_recv.join();

    closesocket(listen_socket);
    WSACleanup();
    cout << "Server shut down.\n";
    return 0;
}
