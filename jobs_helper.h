#ifndef JOBS_HELPER_H
#define JOBS_HELPER_H

#define MAX_INPUT_SIZE 20
#define MAX_JOBS 50

typedef struct {
    pid_t pid;
    int job_id;
    char status[10];
    char command[MAX_INPUT_SIZE];
} Job;

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

Job* find_job(pid_t pid) {
    for (int i = 0; i < job_count; i++) {
        if (jobs[i].pid == pid) {
            return &jobs[i];
        }
    }
    return NULL;
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




#endif