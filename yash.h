#ifndef YASH_H
#define YASH_H

#include <signal.h>

/* Macro Definitions */
#define PORT 3820

/* Global Variable Declarations */
extern int sock;
extern volatile sig_atomic_t running;

/* Function Prototypes */

/**
 * Signal handler for Ctrl-C (SIGINT).
 * Sends a control command to the server to interrupt the current command.
 * @param sig The signal number.
 */
void handle_sigint(int sig);

/**
 * Signal handler for Ctrl-Z (SIGTSTP).
 * Sends a control command to the server to suspend the current command.
 * @param sig The signal number.
 */
void handle_sigtstp(int sig);

/**
 * Sends file content to the server for 'cat > filename' or 'cat >> filename' commands.
 * @param command The command string containing the redirection.
 */
void send_file_content(const char *command);

#endif /* YASH_H */
