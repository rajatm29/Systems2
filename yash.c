#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sys/select.h>
#include <errno.h>
#include <fcntl.h>

#define PORT 3820

int sock;
volatile sig_atomic_t running = 1;

// Signal handler for Ctrl-C (SIGINT)
void handle_sigint(int sig) {
    const char *ctl_stop = "CTL c\n";
    send(sock, ctl_stop, strlen(ctl_stop), 0);
    printf("\n[Interrupted]\n");
    fflush(stdout);
}

// Signal handler for Ctrl-Z (SIGTSTP)
void handle_sigtstp(int sig) {
    const char *ctl_suspend = "CTL z\n";
    send(sock, ctl_suspend, strlen(ctl_suspend), 0);
    printf("\n[Stopped]\n");
    fflush(stdout);
}

// Function to send file content to server for cat > or cat >>
void send_file_content(const char *command) {
    // Notify the server to start file redirection
    char start_cmd[1024];
    snprintf(start_cmd, sizeof(start_cmd), "CMD %s\n", command);
    send(sock, start_cmd, strlen(start_cmd), 0);

    printf("Enter content. Press Ctrl-D to finish:\n");

    // Send user input to the server
    char buffer[1024];
    while (fgets(buffer, sizeof(buffer), stdin)) {
        send(sock, buffer, strlen(buffer), 0);
    }

    // Indicate the end of file content
    send(sock, "#EOF\n", 5, 0);
    printf("[End of input]\n");

    clearerr(stdin); 
}

// Main function
int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <IP_Address_of_Server>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in server_addr;
    char buffer[1024];
    ssize_t valread;

    // Create socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);

    // Convert IP address
    if (inet_pton(AF_INET, argv[1], &server_addr.sin_addr) <= 0) {
        perror("Invalid address or Address not supported");
        close(sock);
        exit(EXIT_FAILURE);
    }

    // Connect to the server
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        close(sock);
        exit(EXIT_FAILURE);
    }

    printf("Connected to server at %s:%d\n", argv[1], PORT);

    // Set up signal handling
    signal(SIGINT, handle_sigint);
    signal(SIGTSTP, handle_sigtstp);

    fd_set readfds;
    int maxfd = sock > STDIN_FILENO ? sock : STDIN_FILENO;

    // Main loop for user input and server responses
    while (running) {
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);
        FD_SET(sock, &readfds);

        printf("# ");
        fflush(stdout);

        int activity = select(maxfd + 1, &readfds, NULL, NULL, NULL);

        if (activity < 0 && errno != EINTR) {
            perror("select error");
            break;
        }

        if (FD_ISSET(sock, &readfds)) {
            // Data available from server
            char recv_buffer[8192];
            valread = recv(sock, recv_buffer, sizeof(recv_buffer) - 1, 0);
            if (valread <= 0) {
                // Connection closed
                printf("\nServer disconnected.\n");
                break;
            }
            recv_buffer[valread] = '\0';
            printf("%s", recv_buffer);
            fflush(stdout);
        }

        if (FD_ISSET(STDIN_FILENO, &readfds)) {
            // Input from user
            if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
                // EOF (Ctrl-D) detected, send "CTL d\n" to server
                const char *ctl_disconnect = "CTL d\n";
                send(sock, ctl_disconnect, strlen(ctl_disconnect), 0);
                printf("\nExiting...\n");
                break;
            }

            // Remove newline character from the input
            buffer[strcspn(buffer, "\n")] = 0;

            // Check if the command is a redirection (cat > or cat >>)
            if (strncmp(buffer, "cat >", 5) == 0 || strncmp(buffer, "cat >>", 6) == 0) {
                send_file_content(buffer); // Handle file content redirection
            } else {
                // Regular command: send it to the server
                char command[1024];
                snprintf(command, sizeof(command), "CMD %s\n", buffer);
                send(sock, command, strlen(command), 0);
            }
        }
    }

    // Close the socket
    close(sock);
    return 0;
}
