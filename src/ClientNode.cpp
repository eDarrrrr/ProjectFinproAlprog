#include <iostream>
#include <string>
#include <winsock2.h>
#pragma comment(lib,"ws2_32.lib")

#define SERVER "127.0.0.1"
#define PORT 8888

using namespace std;

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

    string input;
    while (true) {
        cout << "[CLIENT] > ";
        getline(cin, input);
        if (input == "/exit") break;
        send(client_socket, input.c_str(), input.length(), 0);
    }

    closesocket(client_socket);
    WSACleanup();
    cout << "Client shutdown.\n";
    return 0;
}
