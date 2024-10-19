#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <time.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/stat.h>


#define PORT 3820
#define LOG_FILE "/Users/rajatmonga/Desktop/pp-rm58873/systems-hw-2/yashd.log"

// Mutex for logging to ensure thread safety
pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

// Function to reuse port
void reusePort(int sockfd) {
    int opt = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));
}

// Function to daemonize the process
void daemonize() {
    pid_t pid = fork();
    if (pid < 0) exit(EXIT_FAILURE);
    if (pid > 0) exit(EXIT_SUCCESS);

    umask(0);
    if (setsid() < 0) exit(EXIT_FAILURE);
    if (chdir("/") < 0) exit(EXIT_FAILURE);

    freopen("/dev/null", "r", stdin);
    freopen("/dev/null", "w", stdout);
    freopen("/dev/null", "w", stderr);
}

// Function to log executed commands
void log_command(const char *client_ip, int port, const char *command) {
    pthread_mutex_lock(&log_mutex);
    FILE *log_file = fopen(LOG_FILE, "a");
    if (log_file) {
        time_t now = time(NULL);
        char time_str[64];
        strftime(time_str, sizeof(time_str), "%b %d %H:%M:%S", localtime(&now));
        fprintf(log_file, "%s yashd[%s:%d]: %s\n", time_str, client_ip, port, command);
        fclose(log_file);
    }
    pthread_mutex_unlock(&log_mutex);
}

// Function to execute a command and send output to the client
void execute_command(const char *command, int client_sock) {
    int pipe_fd[2];
    pipe(pipe_fd);
    pid_t pid = fork();

    if (pid == 0) {
        // Child process
        dup2(pipe_fd[1], STDOUT_FILENO); // Redirect stdout to pipe
        close(pipe_fd[0]);
        close(pipe_fd[1]);
        execlp("/bin/sh", "sh", "-c", command, NULL);
        exit(EXIT_FAILURE);
    } else {
        // Parent process
        close(pipe_fd[1]);
        char output[1024];
        ssize_t n;
        while ((n = read(pipe_fd[0], output, sizeof(output))) > 0) {
            send(client_sock, output, n, 0); // Send output to client
        }
        close(pipe_fd[0]);
        waitpid(pid, NULL, 0); // Wait for child process to finish

        // Send prompt to indicate readiness for next command
        send(client_sock, "\n#", 2, 0);
    }
}

// Function to handle each client connection
void *client_handler(void *socket_desc) {
    int sock = *(int*)socket_desc;
    free(socket_desc);

    char client_ip[INET_ADDRSTRLEN];
    struct sockaddr_in addr;
    socklen_t addr_size = sizeof(struct sockaddr_in);
    getpeername(sock, (struct sockaddr*)&addr, &addr_size);
    inet_ntop(AF_INET, &addr.sin_addr, client_ip, sizeof(client_ip));
    int client_port = ntohs(addr.sin_port);

    char buffer[1024];
    ssize_t read_size;

    // Send initial prompt to client
    send(sock, "\n#", 2, 0);

    // Receive commands and handle them
    while ((read_size = recv(sock, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[read_size] = '\0';

        // Process command
        if (strncmp(buffer, "CMD ", 4) == 0) {
            const char *command = buffer + 4;
            log_command(client_ip, client_port, command);
            execute_command(command, sock);
        } else if (strncmp(buffer, "CTL ", 4) == 0) {
            // Handle control commands (e.g., c, z, d)
            // Currently, this example does not implement specific CTL actions
            send(sock, "Control commands are not implemented\n#", 35, 0);
        } else {
            // Treat as plain text
            send(sock, "Invalid command\n#", 17, 0);
        }
    }

    close(sock);
    pthread_exit(NULL);
}

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    // Daemonize the process
    daemonize();

    // Create server socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    reusePort(server_fd);

    // Bind
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);
    bind(server_fd, (struct sockaddr *)&address, sizeof(address));

    // Listen
    listen(server_fd, 3);

    // Handle incoming connections
    while (1) {
        new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
        if (new_socket < 0) {
            perror("accept");
            continue;
        }

        // Create a thread for each client
        pthread_t client_thread;
        int *new_sock = malloc(1);
        *new_sock = new_socket;
        if (pthread_create(&client_thread, NULL, client_handler, (void*)new_sock) < 0) {
            perror("could not create thread");
            free(new_sock);
        }

        // Detach the thread to handle its own cleanup
        pthread_detach(client_thread);
    }

    close(server_fd);
    pthread_mutex_destroy(&log_mutex);
    return 0;
}
