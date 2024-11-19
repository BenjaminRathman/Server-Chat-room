#ifndef COMMUNICATION_H
#define COMMUNICATION_H

#include <arpa/inet.h>  // For struct sockaddr_in

#define MAX_CLIENTS 100
#define BUFFER_SIZE 1024
#define USERNAME_SIZE 50
#define PASSWORD_SIZE 50  // Define PASSWORD_SIZE
#define SERV_TCP_PORT 7777  // Define default server port

typedef struct {
    int socket;
    struct sockaddr_in address;
    socklen_t addr_len;
    char username[USERNAME_SIZE];
    char password[PASSWORD_SIZE];  // Store plain-text password
    int registered;  // 0 = not registered, 1 = registered
} client_t;

extern client_t *clients[MAX_CLIENTS];
extern pthread_mutex_t clients_mutex;

void send_message_to_all(client_t *sender, char *message);
void list_all_users(client_t *client);
void send_direct_message(client_t *sender, char *recipient_username, char *message);

#endif // COMMUNICATION_H
