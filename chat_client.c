#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <windows.h>
#pragma comment(lib, "ws2_32.lib")

#define BUFFER_SIZE 1024
#define USERNAME_LEN 32
#define PORT 8081

DWORD WINAPI message_receiver(LPVOID socket_ptr) {
    SOCKET client_socket = *(SOCKET *)socket_ptr;
    char data_buffer[BUFFER_SIZE];
    int data_len;
    while ((data_len = recv(client_socket, data_buffer, BUFFER_SIZE - 1, 0)) > 0) {
        data_buffer[data_len] = '\0';
        printf("%s", data_buffer);
        fflush(stdout);
    }
    printf("Disconnected from server.\n");
    return 0;
}
int main() {
    WSADATA wsa_data;
    SOCKET client_socket;
    struct sockaddr_in server_info;
    char data_buffer[BUFFER_SIZE];
    char user_id[USERNAME_LEN];
    char pass_key[64];

    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        printf("WSAStartup failed: %d\n", WSAGetLastError());
        return 1;
    }

    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == INVALID_SOCKET) {
        printf("Socket creation failed: %d\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }

    server_info.sin_family = AF_INET;
    server_info.sin_port = htons(PORT);
    server_info.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(client_socket, (struct sockaddr *)&server_info, sizeof(server_info)) == SOCKET_ERROR) {
        printf("Connection failed: %d\n", WSAGetLastError());
        closesocket(client_socket);
        WSACleanup();
        return 1;
    }
    printf("Server is connected.\n");

    recv(client_socket, data_buffer, BUFFER_SIZE - 1, 0);
    printf("%s", data_buffer);

    printf("Enter username: ");
    fgets(user_id, USERNAME_LEN, stdin);
    user_id[strcspn(user_id, "\n")] = '\0';
    send(client_socket, user_id, strlen(user_id), 0);

    recv(client_socket, data_buffer, BUFFER_SIZE - 1, 0);
    printf("%s", data_buffer);

    printf("Enter password: ");
    fgets(pass_key, sizeof(pass_key), stdin);
    pass_key[strcspn(pass_key, "\n")] = '\0';
    send(client_socket, pass_key, strlen(pass_key), 0);

    int bytes_received = recv(client_socket, data_buffer, BUFFER_SIZE - 1, 0);
    if (bytes_received <= 0) {
        printf("Failed to receive welcome message.\n");
        closesocket(client_socket);
        WSACleanup();
        return 1;
    }

    data_buffer[bytes_received] = '\0';
    printf("%s", data_buffer);
    if (strstr(data_buffer, "failed")) {
        closesocket(client_socket);
        WSACleanup();
        return 0;
    }

    HANDLE recv_thread = CreateThread(NULL, 0, message_receiver, &client_socket, 0, NULL);
    if (!recv_thread) {
        printf("Thread creation failed.\n");
        closesocket(client_socket);
        WSACleanup();
        return 1;
    }

    printf("Chat started. Use '@username message' for private messages or 'exit' to quit.\n");
    while (1) {
        fgets(data_buffer, BUFFER_SIZE, stdin);
        data_buffer[strcspn(data_buffer, "\n")] = '\0';
        if (strcmp(data_buffer, "exit") == 0)
            break;
        send(client_socket, data_buffer, strlen(data_buffer), 0);
    }

    closesocket(client_socket);
    CloseHandle(recv_thread);
    WSACleanup();
    return 0;
}
