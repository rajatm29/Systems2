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
//#define LOG_FILE "/tmp/yashd.log"
#define LOG_FILE "/Users/rajatmonga/Desktop/pp-rm58873/systems-hw-2/yashd.log"
#define MAX_JOBS 20
#define MAX_INPUT_SIZE 20

typedef struct {
    pid_t pid;
    int job_id;
    char status[10];
    char command[MAX_INPUT_SIZE];
} Job;

pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;
pid_t current_pid = -1; // Global variable to track the child process ID
pid_t fg_pid = 0;

Job jobs[MAX_JOBS];
int job_count = 0;
int next_job_id = 1;

void add_job(pid_t pid, char *command, char *status) {
    if (job_count < MAX_JOBS) {
        jobs[job_count].pid = pid;
        jobs[job_count].job_id = next_job_id++;
        strncpy(jobs[job_count].status, status, sizeof(jobs[job_count].status) - 1);
        strncpy(jobs[job_count].command, command, sizeof(jobs[job_count].command) - 1);
        job_count++;
    }
}

void remove_job(int job_id) {
    for (int i = 0; i < job_count; i++) {
        if (jobs[i].job_id == job_id) {
            memmove(&jobs[i], &jobs[i+1], (job_count - i - 1) * sizeof(Job));
            job_count--;
            break;
        }
    }
}

void print_jobs(int client_sock) {
    char buffer[2048];
    int offset = 0;

    // Build the job list output
    for (int i = 0; i < job_count; i++) {
        offset += snprintf(buffer + offset, sizeof(buffer) - offset,
                           "[%d]%c %s %s\n", 
                           jobs[i].job_id, 
                           (i == job_count - 1) ? '+' : '-', 
                           jobs[i].status, 
                           jobs[i].command);
    }

    // If no jobs, add a message indicating that
    if (job_count == 0) {
        snprintf(buffer, sizeof(buffer), "No jobs\n");
    }

    // Send the job list to the client
    send(client_sock, buffer, strlen(buffer), 0);

    // Send the prompt to indicate readiness for the next command
    send(client_sock, "\n#", 2, 0);
}

void handle_file_redirection(const char *command, int client_sock) {
    char filename[1024];
    int append_mode = 0;

    // Parse the filename and determine if append mode is used
    if (sscanf(command, "cat > %1023s", filename) == 1) {
        append_mode = 0; // Write mode
    } else if (sscanf(command, "cat >> %1023s", filename) == 1) {
        append_mode = 1; // Append mode
    } else {
        send(client_sock, "Invalid redirection command\n#", 29, 0);
        return;
    }

    // Open the file for writing or appending
    FILE *file = fopen(filename, append_mode ? "a" : "w");
    if (file == NULL) {
        send(client_sock, "Failed to open file\n#", 21, 0);
        return;
    }

    // Read content from the client until "#EOF\n" is received
    char buffer[1024];
    char recv_buffer[4096] = {0};
    ssize_t n;

    while ((n = recv(client_sock, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[n] = '\0';
        // Append received data to recv_buffer
        strncat(recv_buffer, buffer, n);

        char *eof_marker;
        while ((eof_marker = strstr(recv_buffer, "#EOF\n")) != NULL) {
            *eof_marker = '\0'; // Null-terminate at EOF marker
            fputs(recv_buffer, file); // Write up to EOF marker
            fclose(file);
            send(client_sock, "File written successfully\n#", 26, 0);
            send(client_sock, "\n#", 2, 0);
            return;
        }

        // If no EOF marker, write the whole buffer and continue
        fputs(recv_buffer, file);
        recv_buffer[0] = '\0'; // Clear recv_buffer
    }

    // If we exit the loop without finding EOF, there was an error
    fclose(file);
    send(client_sock, "Error: Unexpected end of input\n#", 32, 0);
    send(client_sock, "\n#", 2, 0);
}

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
    //if (chdir("./") < 0) exit(EXIT_FAILURE);

    /* Close all file descriptors that are open */
    int k;
    for (k = getdtablesize()-1; k>0; k--)
        close(k);

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

void execute_command(const char *command, int client_sock) {
    // Handle file redirection for cat > or cat >>
    if (strncmp(command, "cat >", 5) == 0 || strncmp(command, "cat >>", 6) == 0) {
        handle_file_redirection(command, client_sock);
        return;
    }

    int pipe_fd[2];
    pipe(pipe_fd);
    pid_t pid = fork();

    int is_background = 0;
    char trimmed_command[1024];
    strncpy(trimmed_command, command, sizeof(trimmed_command) - 1);
    trimmed_command[sizeof(trimmed_command) - 1] = '\0'; // Ensure null termination

    size_t len = strlen(trimmed_command);
    if (len > 0 && trimmed_command[len - 1] == '&') {
        is_background = 1;
        trimmed_command[len - 1] = '\0'; // Remove the '&' character
        while (len > 1 && trimmed_command[len - 2] == ' ') {
            trimmed_command[--len - 1] = '\0'; // Trim trailing spaces
        }
    }

    if (pid == 0) { // Child process
        dup2(pipe_fd[1], STDOUT_FILENO);  //redirect output
        dup2(pipe_fd[1], STDERR_FILENO);  //redirect err
        close(pipe_fd[0]);
        close(pipe_fd[1]);
        execlp("/bin/sh", "sh", "-c", trimmed_command, NULL);
        exit(EXIT_FAILURE);
    } else { // Parent process
        close(pipe_fd[1]);

        // Add job to job list
        add_job(pid, trimmed_command, is_background == 1 ? "Running" : "Foreground");

        if (is_background == 0) {
            // Wait for foreground job to finish
            fg_pid = pid;
            current_pid = pid; // Set current_pid to the child PID
            char output[1024];
            ssize_t n;
            while ((n = read(pipe_fd[0], output, sizeof(output))) > 0) {
                send(client_sock, output, n, 0);
            }
            close(pipe_fd[0]);
            int status;
            waitpid(pid, &status, WUNTRACED);
            fg_pid = 0;
            current_pid = -1; // Reset current_pid

            // Update job status based on child's exit status
            if (WIFSTOPPED(status)) {
                // Update job status to "Stopped"
                strcpy(jobs[job_count - 1].status, "Stopped");
            } else {
                // Remove completed job from list
                remove_job(jobs[job_count - 1].job_id);
            }
        } else {
            // Background job, do not wait for it to finish
            close(pipe_fd[0]);
            send(client_sock, "Running in background\n#", 23, 0);
        }

        send(client_sock, "\n#", 2, 0);
    }
}

void handle_ctl_command(const char *ctl_command, int client_sock) {
    char action = ctl_command[0];

    if (current_pid == -1) {
        // No current command is running
        send(client_sock, "No command is currently running\n#", 33, 0);
        return;
    }

    switch (action) {
        case 'c':
            // Send SIGINT to the current running process
            if (kill(current_pid, SIGINT) == 0) {
                send(client_sock, "Command interrupted\n#", 22, 0);
            } else {
                send(client_sock, "Failed to interrupt the command\n#", 34, 0);
            }
            send(client_sock, "\n#", 2, 0);
            break;
        case 'z':
            // Send SIGTSTP to the current running process
            printf("PID %d", current_pid);
            if (kill(current_pid, SIGTSTP) == 0) {
                send(client_sock, "Command suspended\n#", 20, 0);
            } else {
                send(client_sock, "Failed to suspend the command\n#", 32, 0);
            }
            send(client_sock, "\n#", 2, 0);
            break;
        case 'd':
            // Handle 'd' as an optional cleanup or termination command
            send(client_sock, "Disconnecting...\n#", 19, 0);
            close(client_sock); // Close the socket to terminate the connection
            pthread_exit(NULL); // End the thread
            break;
        default:
            // Unknown control command
            send(client_sock, "Unknown control command\n#", 25, 0);
            break;
    }
}

// Handle fg command
void handle_fg(int client_sock) {
    if (job_count > 0) {
        Job *job = &jobs[job_count - 1];
        printf("%s\n", job->command);
        kill(job->pid, SIGCONT);
        fg_pid = job->pid;
        strcpy(job->status, "Stopped");
        tcsetpgrp(STDIN_FILENO, fg_pid); //terminal control to fg process
        int status;
        waitpid(job->pid, &status, WUNTRACED);
        fg_pid = 0;
        if (!WIFSTOPPED(status)) {
            remove_job(job->job_id);
        }
        send(client_sock, "Job resumed in foreground\n#", 26, 0);
    } else {
        send(client_sock, "No jobs to bring to foreground\n#", 31, 0);
    }
}

// Handle bg command
void handle_bg(int client_sock) {
    if (job_count > 0) {
        Job *job = &jobs[job_count - 1];
        kill(job->pid, SIGCONT);
        strcpy(job->status, "Running");
        send(client_sock, "Job resumed in background\n#", 26, 0);
    } else {
        send(client_sock, "No jobs to bring to background\n#", 31, 0);
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
    char recv_buffer[4096] = {0};

    send(sock, "\n#", 2, 0);

    while ((read_size = recv(sock, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[read_size] = '\0';
        // Append received data to recv_buffer
        strncat(recv_buffer, buffer, read_size);

        // Process complete lines
        char *line_end;
        while ((line_end = strchr(recv_buffer, '\n')) != NULL) {
            *line_end = '\0'; // Null-terminate the line
            char *line = recv_buffer;

            // Process the line
            if (strncmp(line, "CMD jobs", 8) == 0) {
                print_jobs(sock);
            } else if (strncmp(line, "CMD fg", 6) == 0) {
                handle_fg(sock);
            } else if (strncmp(line, "CMD bg", 6) == 0) {
                handle_bg(sock);
            } else if (strncmp(line, "CMD ", 4) == 0) {
                const char *command = line + 4;
                log_command(client_ip, client_port, command);
                execute_command(command, sock);
            } else if (strncmp(line, "CTL ", 4) == 0) {
                handle_ctl_command(line + 4, sock);
            } else {
                send(sock, "Invalid command\n#", 17, 0);
            }

            // Move remaining data to the start of recv_buffer
            size_t remaining_len = strlen(line_end + 1);
            memmove(recv_buffer, line_end + 1, remaining_len + 1);
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
            perror("Accept failed");
            continue;
        }

        // Create a thread for each client
        pthread_t client_thread;
        int *new_sock = malloc(1);
        *new_sock = new_socket;
        if (pthread_create(&client_thread, NULL, client_handler, (void*)new_sock) < 0) {
            perror("Could not create thread");
            free(new_sock);
        }

        // Detach the thread to handle its own cleanup
        pthread_detach(client_thread);
    }

    close(server_fd);
    pthread_mutex_destroy(&log_mutex);
    return 0;
}
