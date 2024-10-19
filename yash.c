#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <signal.h>

#define PORT 3820

int sock;
volatile sig_atomic_t running = 1;

// Signal handler for Ctrl-C (SIGINT)
void handle_sigint(int sig) {
    const char *ctl_stop = "CTL c\n";
    send(sock, ctl_stop, strlen(ctl_stop), 0);
    printf("\n[Interrupted]\n# ");
    fflush(stdout);
}

// Signal handler for Ctrl-Z (SIGTSTP)
void handle_sigtstp(int sig) {
    const char *ctl_suspend = "CTL z\n";
    send(sock, ctl_suspend, strlen(ctl_suspend), 0);
    printf("\n[Stopped]\n# ");
    fflush(stdout);
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

    // Read initial prompt from server
    valread = read(sock, buffer, sizeof(buffer) - 1);
    if (valread > 0) {
        buffer[valread] = '\0';
        printf("%s", buffer);
    } else {
        printf("Failed to receive initial prompt from server.\n");
        close(sock);
        exit(EXIT_FAILURE);
    }

    // Main loop for user input
    while (running) {
        fflush(stdout);

        // Get user input
        if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
            // EOF (Ctrl-D) detected, send "CTL d" to server
            const char *ctl_disconnect = "CTL d\n";
            send(sock, ctl_disconnect, strlen(ctl_disconnect), 0);
            printf("\nExiting...\n");
            break;
        }

        // Send the command to the server in the format "CMD <Command_String>\n"
        char command[1024];
        snprintf(command, sizeof(command), "CMD %s", buffer);
        send(sock, command, strlen(command), 0);

        // Read the response from the server
        while ((valread = read(sock, buffer, sizeof(buffer) - 1)) > 0) {
            buffer[valread] = '\0';
            printf("%s", buffer);

            // Check if the prompt (#) is received, indicating the end of the response
            if (strstr(buffer, "\n#") != NULL) {
                break;
            }
        }
    }

    // Close the socket
    close(sock);
    return 0;
}
