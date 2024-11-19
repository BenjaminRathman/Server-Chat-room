#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "communication.h"

#define SERV_TCP_PORT 7777

client_t *clients[MAX_CLIENTS];
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

char *users_file = "users.txt";
char *chat_history_file = "chat_history.txt";

void *handle_client(void *arg);
int register_user(client_t *client, char *username, char *password);
int sign_in_user(client_t *client, char *username, char *password);
void load_users();
void save_user(char *username, char *password);
void append_chat_history(const char *message);

int main(int argc, char *argv[]) {
    int server_fd, client_fd, port;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len;
    pthread_t tid;

    // Determine port number
    if (argc == 2) {
        sscanf(argv[1], "%d", &port);
    } else {
        port = SERV_TCP_PORT;
    }

    // Load users from file
    load_users();

    // Create socket and handle socket error
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket error");
        exit(1);
    }

    // Set the SO_REUSEADDR option
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt error");
        exit(1);
    }

    // Initialize server address structure
    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(port);

    // Bind the socket to the address
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind error");
        exit(1);
    }

    // Listen for incoming connections
    listen(server_fd, MAX_CLIENTS);
    printf("Server listening on port %d\n", port);

    while (1) {
        client_len = sizeof(client_addr);
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0) {
            perror("accept error");
            continue;
        }

        // Allocate memory for the new client and initialize it
        client_t *client = (client_t *)malloc(sizeof(client_t));
        client->socket = client_fd;
        client->address = client_addr;
        client->addr_len = client_len;
        client->registered = 0; // Initialize as not registered

        // Lock the clients mutex and add the client to the clients array
        pthread_mutex_lock(&clients_mutex);
        for (int i = 0; i < MAX_CLIENTS; ++i) {
            if (clients[i] == NULL) {
                clients[i] = client;
                pthread_create(&tid, NULL, handle_client, (void *)client);
                break;
            }
        }
        pthread_mutex_unlock(&clients_mutex);
    }

    // Close the server socket
    close(server_fd);
    return 0;
}

// Function to handle connection with clients
void *handle_client(void *arg) {
    char buffer[BUFFER_SIZE];
    int len;
    client_t *client = (client_t *)arg;

    while ((len = read(client->socket, buffer, sizeof(buffer) - 1)) > 0) {
        buffer[len] = '\0';

        if (strncmp(buffer, "/register ", 10) == 0) {
            char *username = strtok(buffer + 10, " ");
            char *password = strtok(NULL, " ");
            if (username && password) {
                if (register_user(client, username, password) == 0) {
                    write(client->socket, "\nRegistration successful, Please sign in to continue\n", 55);
                } else {
                    write(client->socket, "Username already taken\n", 24);
                }
            } else {
                write(client->socket, "Invalid registration format\n", 28);
            }
        } else if (strncmp(buffer, "/signin ", 8) == 0) {
            char *username = strtok(buffer + 8, " ");
            char *password = strtok(NULL, " ");
            if (username && password) {
                if (sign_in_user(client, username, password) == 0) {
                    write(client->socket, "Sign-in successful\n", 20);
                } else {
                    write(client->socket, "Invalid username or password\n", 29);
                }
            } else {
                write(client->socket, "Invalid sign-in format\n", 23);
            }
        } else if (strncmp(buffer, "/list", 5) == 0) {
            list_all_users(client);
        } else if (strncmp(buffer, "/dm ", 4) == 0) {
            char *recipient_username = strtok(buffer + 4, " ");
            char *message = strtok(NULL, "\0");
            if (recipient_username && message) {
                send_direct_message(client, recipient_username, message);
            } else {
                write(client->socket, "Invalid direct message format\n", 30);
            }
        } else if (client->registered) {
            // Format message with username
            char message[BUFFER_SIZE];
            snprintf(message, BUFFER_SIZE - 1, "%s: %s", client->username, buffer);  // Adjusted to BUFFER_SIZE - 1
            send_message_to_all(client, message);
            append_chat_history(message);
        } else {
            write(client->socket, "You must register or sign-in first\n", 35);
        }
    }

    close(client->socket);
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (clients[i] == client) {
            clients[i] = NULL;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
    free(client);
    pthread_exit(NULL);
}

// Function to register a user
int register_user(client_t *client, char *username, char *password) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (clients[i] != NULL && strcmp(clients[i]->username, username) == 0) {
            pthread_mutex_unlock(&clients_mutex);
            return -1;  // Username already taken
        }
    }

    strncpy(client->username, username, USERNAME_SIZE - 1);
    client->username[USERNAME_SIZE - 1] = '\0';
    strncpy(client->password, password, PASSWORD_SIZE - 1);
    client->password[PASSWORD_SIZE - 1] = '\0';
    client->registered = 1;
    save_user(username, password);
    pthread_mutex_unlock(&clients_mutex);
    return 0;  // Registration successful
}

// Function to sign in a user
int sign_in_user(client_t *client, char *username, char *password) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (clients[i] != NULL && strcmp(clients[i]->username, username) == 0) {
            if (strcmp(clients[i]->password, password) == 0) {
                client->registered = 1;
                strncpy(client->username, username, USERNAME_SIZE - 1);
                client->username[USERNAME_SIZE - 1] = '\0';
                pthread_mutex_unlock(&clients_mutex);
                return 0;  // Sign-in successful
            } else {
                pthread_mutex_unlock(&clients_mutex);
                return -1;  // Incorrect password
            }
        }
    }
    pthread_mutex_unlock(&clients_mutex);
    return -1;  // Username not found
}

// Function to load users from file
void load_users() {
    FILE *file = fopen(users_file, "r");
    if (file != NULL) {
        char username[USERNAME_SIZE];
        char password[PASSWORD_SIZE];
        while (fscanf(file, "%s %s\n", username, password) != EOF) {
            // Allocate memory for the new client and initialize it
            client_t *client = (client_t *)malloc(sizeof(client_t));
            client->socket = -1; // Invalid socket since this is just for storage
            strncpy(client->username, username, USERNAME_SIZE - 1);
            client->username[USERNAME_SIZE - 1] = '\0';
            strncpy(client->password, password, PASSWORD_SIZE - 1);
            client->password[PASSWORD_SIZE - 1] = '\0';
            client->registered = 1; // Mark as registered

            // Add client to the clients array
            for (int i = 0; i < MAX_CLIENTS; ++i) {
                if (clients[i] == NULL) {
                    clients[i] = client;
                    break;
                }
            }
        }
        fclose(file);
    }
}

// Function to save a user to file
void save_user(char *username, char *password) {
    FILE *file = fopen(users_file, "a");
    if (file != NULL) {
        fprintf(file, "%s %s\n", username, password);
        fclose(file);
    }
}

// Function to append a message to the chat history file
void append_chat_history(const char *message) {
    FILE *file = fopen(chat_history_file, "a");
    if (file != NULL) {
        fprintf(file, "%s\n", message);
        fclose(file);
    }
}
