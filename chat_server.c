#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <windows.h>

#pragma comment(lib, "ws2_32.lib")

#define MAX_CLIENTS 10
#define BUFFER_SIZE 1024
#define PORT 8081

typedef struct {
    SOCKET client_socket;
    char user_id[32];
    int is_authenticated;
} client_info_t;

client_info_t *active_clients[MAX_CLIENTS];
CRITICAL_SECTION client_lock;
FILE *chat_log;

void record_message(const char *from_user, const char *to_user, const char *text) {
    if (!chat_log) return;
    SYSTEMTIME current_time;
    GetLocalTime(&current_time);
    fprintf(chat_log, "[%d-%02d-%02d %02d:%02d:%02d] %s -> %s: %s\n",
        current_time.wYear, current_time.wMonth, current_time.wDay,
        current_time.wHour, current_time.wMinute, current_time.wSecond,
        from_user, to_user, text);
    fflush(chat_log);
}

void send_to_all(char *text, SOCKET sender_socket, const char *from_user) {
    EnterCriticalSection(&client_lock);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (active_clients[i] && active_clients[i]->client_socket != sender_socket) {
            send(active_clients[i]->client_socket, text, strlen(text), 0);
            record_message(from_user, "GROUP", text);
        }
    }
    LeaveCriticalSection(&client_lock);
}

void send_to_one(char *text, char *target_user, const char *from_user, SOCKET sender_socket) {
    EnterCriticalSection(&client_lock);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (active_clients[i] && strcmp(active_clients[i]->user_id, target_user) == 0) {
            send(active_clients[i]->client_socket, text, strlen(text), 0);
            record_message(from_user, target_user, text);
            LeaveCriticalSection(&client_lock);
            return;
        }
    }
    LeaveCriticalSection(&client_lock);
    char error_text[BUFFER_SIZE];
    snprintf(error_text, BUFFER_SIZE, "User %s not found.\n", target_user);
    send(sender_socket, error_text, strlen(error_text), 0);
}

DWORD WINAPI process_client(LPVOID param) {
    SOCKET client_socket = *(SOCKET *)param;
    char data_buffer[BUFFER_SIZE];
    client_info_t *client_data = NULL;
    int bytes_read;

    send(client_socket, "Enter USERNAME: ", 16, 0);
    bytes_read = recv(client_socket, data_buffer, BUFFER_SIZE - 1, 0);
    if (bytes_read <= 0) {
        closesocket(client_socket);
        free(param);
        return 0;
    }
    data_buffer[bytes_read] = '\0';
    data_buffer[strcspn(data_buffer, "\r\n")] = '\0';
    char user_id[32];
    strncpy(user_id, data_buffer, 32);
    user_id[31] = '\0';

    send(client_socket, "Enter PASSWORD: ", 16, 0);
    bytes_read = recv(client_socket, data_buffer, BUFFER_SIZE - 1, 0);
    if (bytes_read <= 0) {
        closesocket(client_socket);
        free(param);
        return 0;
    }
    data_buffer[bytes_read] = '\0';
    data_buffer[strcspn(data_buffer, "\r\n")] = '\0';

    if (strcmp(data_buffer, "Shivam@123") != 0) {
        send(client_socket, "Authentication failed.\n", 23, 0);
        closesocket(client_socket);
        free(param);
        return 0;
    }

    client_data = (client_info_t *)malloc(sizeof(client_info_t));
    if (!client_data) {
        closesocket(client_socket);
        free(param);
        return 0;
    }
    client_data->client_socket = client_socket;
    strcpy(client_data->user_id, user_id);
    client_data->is_authenticated = 1;

    EnterCriticalSection(&client_lock);
    int slot = -1;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (!active_clients[i]) {
            slot = i;
            active_clients[i] = client_data;
            break;
        }
    }
    LeaveCriticalSection(&client_lock);

    if (slot == -1) {
        send(client_socket, "Server full. Try again later.\n", 30, 0);
        free(client_data);
        closesocket(client_socket);
        free(param);
        return 0;
    }

    char greet_message[BUFFER_SIZE];
    snprintf(greet_message, BUFFER_SIZE, "Password is correct. Welcome %s! You are connected.\n", user_id);
    send(client_socket, greet_message, strlen(greet_message), 0);
    snprintf(greet_message, BUFFER_SIZE, "[%s has joined the chat]\n", user_id);
    send_to_all(greet_message, client_socket, user_id);

    while ((bytes_read = recv(client_socket, data_buffer, BUFFER_SIZE - 1, 0)) > 0) {
        data_buffer[bytes_read] = '\0';
        data_buffer[strcspn(data_buffer, "\r\n")] = '\0';
        printf("Received from %s: '%s'\n", user_id, data_buffer);

        if (strncmp(data_buffer, "@", 1) == 0) {
            char *target = strtok(data_buffer + 1, " ");
            char *msg_content = strtok(NULL, "");
            if (target && msg_content) {
                char full_text[BUFFER_SIZE];
                snprintf(full_text, BUFFER_SIZE, "[Private from %s] %s\n", user_id, msg_content);
                send_to_one(full_text, target, user_id, client_socket);
            } else {
                char error_text[BUFFER_SIZE];
                snprintf(error_text, BUFFER_SIZE, "Invalid format. Use: @username message\n");
                send(client_socket, error_text, strlen(error_text), 0);
            }
        } else {
            char full_text[BUFFER_SIZE];
            snprintf(full_text, BUFFER_SIZE, "[%s] %s\n", user_id, data_buffer);
            send_to_all(full_text, client_socket, user_id);
        }
    }

    snprintf(data_buffer, BUFFER_SIZE, "[%s has left the chat]\n", user_id);
    send_to_all(data_buffer, client_socket, user_id);
    printf("Client %s disconnected.\n", user_id);

    EnterCriticalSection(&client_lock);
    active_clients[slot] = NULL;
    free(client_data);
    LeaveCriticalSection(&client_lock);

    closesocket(client_socket);
    free(param);
    return 0;
}

int main() {
    WSADATA wsa_data;
    SOCKET main_socket, new_client_socket;
    struct sockaddr_in server_config, client_config;
    int client_size = sizeof(client_config);

    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        printf("WSAStartup failed: %d\n", WSAGetLastError());
        return 1;
    }

    chat_log = fopen("chat_log.txt", "a");
    if (!chat_log) {
        printf("Failed to open chat_log.txt - proceeding without logging.\n");
    }

    for (int i = 0; i < MAX_CLIENTS; i++) {
        active_clients[i] = NULL;
    }

    main_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (main_socket == INVALID_SOCKET) {
        printf("Socket creation failed: %d\n", WSAGetLastError());
        if (chat_log) fclose(chat_log);
        WSACleanup();
        return 1;
    }

    server_config.sin_family = AF_INET;
    server_config.sin_addr.s_addr = INADDR_ANY;
    server_config.sin_port = htons(PORT);

    if (bind(main_socket, (struct sockaddr *)&server_config, sizeof(server_config)) == SOCKET_ERROR) {
        printf("Bind failed: %d\n", WSAGetLastError());
        if (chat_log) fclose(chat_log);
        closesocket(main_socket);
        WSACleanup();
        return 1;
    }

    if (listen(main_socket, MAX_CLIENTS) == SOCKET_ERROR) {
        printf("Listen failed: %d\n", WSAGetLastError());
        if (chat_log) fclose(chat_log);
        closesocket(main_socket);
        WSACleanup();
        return 1;
    }

    printf("Server is listening on port %d...\n", PORT);
    InitializeCriticalSection(&client_lock);

    while (1) {
        new_client_socket = accept(main_socket, (struct sockaddr *)&client_config, &client_size);
        if (new_client_socket == INVALID_SOCKET) {
            printf("Accept failed: %d\n", WSAGetLastError());
            continue;
        }

        SOCKET *client_ptr = (SOCKET *)malloc(sizeof(SOCKET));
        *client_ptr = new_client_socket;

        HANDLE client_thread = CreateThread(NULL, 0, process_client, client_ptr, 0, NULL);
        if (!client_thread) {
            printf("Thread creation failed: %d\n", GetLastError());
            free(client_ptr);
            closesocket(new_client_socket);
        } else {
            CloseHandle(client_thread);
        }
    }

    if (chat_log) fclose(chat_log);
    closesocket(main_socket);
    DeleteCriticalSection(&client_lock);
    WSACleanup();
    return 0;
}
