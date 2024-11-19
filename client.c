#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>

#define SERV_TCP_PORT 7777
#define BUFFER_SIZE 2048
#define USERNAME_SIZE 50
#define MESSAGE_SIZE (BUFFER_SIZE - USERNAME_SIZE - 64) // Buffer size minus username size and additional characters

void *receive_message(void *sockfd);
void show_menu();
void show_chat_interface();

char username[USERNAME_SIZE];  // Make username global

int main(int argc, char *argv[]) {
    int server_fd;
    char *serv_host = "localhost";
    struct sockaddr_in serv_addr;
    struct hostent *host_ptr;
    int port;
    int con;
    pthread_t recv_thread;

    // Check command line arguments for host and port
    if (argc >= 2) {
        serv_host = argv[1];
    }
    if (argc == 3) {
        sscanf(argv[2], "%d", &port);
    } else {
        port = SERV_TCP_PORT;
    }

    // Get host information
    host_ptr = gethostbyname(serv_host);
    // Handler for host name error
    if (host_ptr == NULL) {
        perror("gethostbyname error");
        exit(1);
    }

    // Handler for address type error
    if (host_ptr->h_addrtype != AF_INET) {
        perror("unknown address type");
        exit(1);
    }

    // Initialize server address structure
    bzero((char *)&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = ((struct in_addr *)host_ptr->h_addr_list[0])->s_addr;
    serv_addr.sin_port = htons(port);

    // Create socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("Can't open stream socket");
        exit(1);
    }

    // Connect to the server
    con = connect(server_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    if (con < 0) {
        perror("Can't connect to server");
        exit(1);
    }

    // Show menu
    show_menu();

    char command[10];
    char buffer[BUFFER_SIZE];
    while (1) {
        printf("Enter command: ");
        fgets(command, 10, stdin);
        command[strcspn(command, "\n")] = 0; // Remove newline character

        if (strcmp(command, "register") == 0) {
            printf("\nInput a username: ");
            fgets(username, USERNAME_SIZE, stdin);
            username[strcspn(username, "\n")] = 0; // Remove newline character

            char password[USERNAME_SIZE];
            printf("Input a password: ");
            fgets(password, USERNAME_SIZE, stdin);
            password[strcspn(password, "\n")] = 0; // Remove newline character

            snprintf(buffer, BUFFER_SIZE, "/register %s %s\n", username, password);
            write(server_fd, buffer, strlen(buffer));
            // Wait for server response
            read(server_fd, buffer, BUFFER_SIZE);
            printf("%s", buffer);
        } else if (strcmp(command, "signin") == 0) {
            printf("\nInput a username: ");
            fgets(username, USERNAME_SIZE, stdin);
            username[strcspn(username, "\n")] = 0; // Remove newline character

            char password[USERNAME_SIZE];
            printf("Input a password: ");
            fgets(password, USERNAME_SIZE, stdin);
            password[strcspn(password, "\n")] = 0; // Remove newline character

            snprintf(buffer, BUFFER_SIZE, "/signin %s %s\n", username, password);
            write(server_fd, buffer, strlen(buffer));
            // Wait for server response
            read(server_fd, buffer, BUFFER_SIZE);
            printf("%s", buffer);

            // Assume successful sign-in for now
            if (strstr(buffer, "Sign-in successful") != NULL) {
                break;
            }
        } else {
            printf("Unknown command. Please enter 'register' or 'signin'.\n");
        }
    }

    // Create thread for receiving messages
    if (pthread_create(&recv_thread, NULL, receive_message, (void *)&server_fd) != 0) {
        perror("Can't create thread");
        exit(1);
    }

    // Enter chat interface
    show_chat_interface();

    // Loop for sending messages to server
    while (1) {
        bzero(buffer, MESSAGE_SIZE);
        fgets(buffer, MESSAGE_SIZE - 1, stdin);

        // Check if the user wants to exit
        if (strncmp(buffer, "exit", 4) == 0) {
            snprintf(buffer, BUFFER_SIZE, "%s has left the chat\n", username);
            write(server_fd, buffer, strlen(buffer));
            break;
        }

        // Send raw message to server
        write(server_fd, buffer, strlen(buffer));
    }

    close(server_fd);
    return 0;
}

// Function to display the menu
void show_menu() {
    printf("|=================================================================|\n");
    printf("|                    Welcome to the Chat                          |\n");
    printf("|         -------------------------------------------             |\n");
    printf("|             Available commands:                                 |\n");
    printf("|             register - Register a new user                      |\n");
    printf("|             signin   - Sign in with existing user               |\n");
    printf("|=================================================================|\n");
}

// Function to display the chat interface
void show_chat_interface() {
    printf("\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n");
    printf("                 =================================\n");
    printf("                       Entering Chat Room\n");
    printf("                 =================================\n");
    printf("Manual:\n1. Type message and press enter to send a message to everyone\n");
    printf("2. Type /dm <username> before your message to send a private message.\n");
    printf("3. Type 'exit' to leave the chat\n");
    printf("_________________________________________________________________________\n");

}

// Function for a user to receive a message from another user
void *receive_message(void *sockfd) {
    int sock = *(int *)sockfd;
    char buffer[BUFFER_SIZE];
    int len;

    // Load previous chat history
    FILE *file = fopen("chat_history.txt", "r");
    if (file != NULL) {
        while (fgets(buffer, BUFFER_SIZE, file) != NULL) {
            printf("%s", buffer);
        }
        fclose(file);
        printf("_________________________________________________________________________\n");
        printf("                       ^^Past messages^^\n");
        printf("_________________________________________________________________________\n");
    }

    while ((len = read(sock, buffer, BUFFER_SIZE - 1)) > 0) {
        buffer[len] = '\0';
        // Check if the message is from the client itself
        if (strncmp(buffer, username, strlen(username)) == 0) {
            printf("You: %s", buffer + strlen(username) + 2); // Adjust the display
        } else {
            printf("%s", buffer);
        }
    }

    pthread_exit(NULL);
}
