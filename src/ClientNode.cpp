#include <iostream>
#include <thread>
#include <string>
#include <winsock2.h>

#define SERVER "127.0.0.1"
#define PORT 8888

using namespace std;

void receiveThread(SOCKET sock) {
    char buffer[1024];
    while (true) {
        int recv_size = recv(sock, buffer, sizeof(buffer) - 1, 0);
        if (recv_size <= 0) {
            cout << "\n[INFO] Server disconnected or error.\n";
            break;
        }
        buffer[recv_size] = '\0';
        cout << "\n[Server]: " << buffer << endl;
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
    SOCKET client_socket;
    struct sockaddr_in server;

    cout << "Initialising Winsock...\n";
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        cerr << "WSAStartup failed. Error code: " << WSAGetLastError() << "\n";
        return 1;
    }

    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == INVALID_SOCKET) {
        cerr << "Could not create socket. Error code: " << WSAGetLastError() << "\n";
        WSACleanup();
        return 1;
    }

    server.sin_addr.s_addr = inet_addr(SERVER);
    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);

    if (connect(client_socket, (struct sockaddr*)&server, sizeof(server)) < 0) {
        cerr << "Connection failed. Error code: " << WSAGetLastError() << "\n";
        closesocket(client_socket);
        WSACleanup();
        return 1;
    }
    cout << "Connected to server.\n";

    // Mulai dua thread: menerima dan mengirim
    thread t_recv(receiveThread, client_socket);
    thread t_send(sendThread, client_socket);

    t_send.join();
    closesocket(client_socket);
    t_recv.join();

    WSACleanup();
    cout << "Client shut down.\n";
    return 0;
}
