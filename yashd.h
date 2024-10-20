#ifndef YASHD_H
#define YASHD_H

#include <sys/types.h>
#include <pthread.h>

/* Macro Definitions */
#define PORT 3820
#define LOG_FILE "/Users/rajatmonga/Desktop/pp-rm58873/systems-hw-2/yashd.log"
#define MAX_JOBS 20
#define MAX_INPUT_SIZE 20

/* Type Definitions */
typedef struct {
    pid_t pid;
    int job_id;
    char status[10];
    char command[MAX_INPUT_SIZE];
} Job;

/* Global Variable Declarations */
extern pthread_mutex_t log_mutex;
extern pid_t current_pid; // Global variable to track the child process ID
extern pid_t fg_pid;

extern Job jobs[MAX_JOBS];
extern int job_count;
extern int next_job_id;

/* Function Prototypes */

/**
 * Adds a new job to the job list.
 * @param pid Process ID of the job.
 * @param command Command associated with the job.
 * @param status Status of the job ("Running", "Stopped", etc.).
 */
void add_job(pid_t pid, char *command, char *status);

/**
 * Removes a job from the job list by job ID.
 * @param job_id The ID of the job to remove.
 */
void remove_job(int job_id);

/**
 * Sends the list of jobs to the client socket.
 * @param client_sock The client socket descriptor.
 */
void print_jobs(int client_sock);

/**
 * Handles file redirection commands like 'cat > filename' or 'cat >> filename'.
 * @param command The command string containing the redirection.
 * @param client_sock The client socket descriptor.
 */
void handle_file_redirection(const char *command, int client_sock);

/**
 * Sets socket options to allow port reuse.
 * @param sockfd The socket file descriptor.
 */
void reusePort(int sockfd);

/**
 * Daemonizes the process to run in the background.
 */
void daemonize();

/**
 * Logs the executed command along with client IP and port.
 * @param client_ip The client's IP address.
 * @param port The client's port number.
 * @param command The command executed.
 */
void log_command(const char *client_ip, int port, const char *command);

/**
 * Executes a shell command and sends the output to the client.
 * @param command The command to execute.
 * @param client_sock The client socket descriptor.
 */
void execute_command(const char *command, int client_sock);

/**
 * Handles control commands from the client (e.g., Ctrl-C, Ctrl-Z).
 * @param ctl_command The control command string.
 * @param client_sock The client socket descriptor.
 */
void handle_ctl_command(const char *ctl_command, int client_sock);

/**
 * Brings the most recent stopped job to the foreground.
 * @param client_sock The client socket descriptor.
 */
void handle_fg(int client_sock);

/**
 * Resumes a stopped job in the background.
 * @param client_sock The client socket descriptor.
 */
void handle_bg(int client_sock);

/**
 * Handles client connections in a separate thread.
 * @param socket_desc Pointer to the client socket descriptor.
 * @return NULL
 */
void *client_handler(void *socket_desc);

#endif /* YASHD_H */
