#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include "communication.h"

extern client_t *clients[MAX_CLIENTS];
extern pthread_mutex_t clients_mutex;

// Function to send message to all clients except the sender
void send_message_to_all(client_t *sender, char *message) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (clients[i] != NULL && clients[i]->socket != sender->socket) {
            write(clients[i]->socket, message, strlen(message));
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

// Function to list all users
void list_all_users(client_t *client) {
    pthread_mutex_lock(&clients_mutex);
    char message[BUFFER_SIZE];
    strcpy(message, "Active users:\n");
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (clients[i] != NULL && clients[i]->registered) {
            strncat(message, clients[i]->username, USERNAME_SIZE - 1);
            strcat(message, "\n");
        }
    }
    pthread_mutex_unlock(&clients_mutex);
    write(client->socket, message, strlen(message));
}

// Function to send a direct message to a specific user
void send_direct_message(client_t *sender, char *recipient_username, char *message) {
    pthread_mutex_lock(&clients_mutex);
    char buffer[BUFFER_SIZE + USERNAME_SIZE + 3]; // +3 for "(DM from ...): " and null terminator
    snprintf(buffer, sizeof(buffer), "(DM from %s): %s", sender->username, message);
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (clients[i] != NULL && strcmp(clients[i]->username, recipient_username) == 0) {
            write(clients[i]->socket, buffer, strlen(buffer));
            pthread_mutex_unlock(&clients_mutex);
            return;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
    char error_message[BUFFER_SIZE];
    snprintf(error_message, sizeof(error_message), "User %s not found.\n", recipient_username);
    write(sender->socket, error_message, strlen(error_message));
}
