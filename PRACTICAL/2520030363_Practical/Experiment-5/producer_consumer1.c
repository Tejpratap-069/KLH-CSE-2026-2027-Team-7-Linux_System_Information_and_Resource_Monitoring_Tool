#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>
#include <string.h>

#define BUFFER_SIZE 100
#define NUM_MESSAGES 100000

int main()
{
    int fd[2];
    pid_t pid;

    char buffer[BUFFER_SIZE];

    struct timespec start, end;

    long long total_bytes = 0;

    // Create anonymous pipe
    if (pipe(fd) == -1)
    {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    // Create child process
    pid = fork();

    if (pid < 0)
    {
        perror("fork");
        exit(EXIT_FAILURE);
    }

    // =========================
    // CHILD PROCESS - CONSUMER
    // =========================
    if (pid == 0)
    {
        close(fd[1]);   // Close unused write end

        while (1)
        {
            int bytes_read = read(fd[0], buffer, BUFFER_SIZE);

            if (bytes_read == 0)
                break;

            if (bytes_read < 0)
            {
                perror("read");
                exit(EXIT_FAILURE);
            }

            total_bytes += bytes_read;
        }

        close(fd[0]);

        printf("\nConsumer received %lld bytes.\n", total_bytes);

        exit(EXIT_SUCCESS);
    }

    // =========================
    // PARENT PROCESS - PRODUCER
    // =========================
    else
    {
        close(fd[0]);   // Close unused read end

        strcpy(buffer, "Producer generated data.");

        // Start timer
        clock_gettime(CLOCK_MONOTONIC, &start);

        for (int i = 0; i < NUM_MESSAGES; i++)
        {
            int bytes_written = write(fd[1], buffer, strlen(buffer));

            if (bytes_written < 0)
            {
                perror("write");
                exit(EXIT_FAILURE);
            }

            total_bytes += bytes_written;
        }

        close(fd[1]);

        // Wait for child to finish
        wait(NULL);

        // Stop timer
        clock_gettime(CLOCK_MONOTONIC, &end);

        double time_taken =
            (end.tv_sec - start.tv_sec) +
            (end.tv_nsec - start.tv_nsec) / 1e9;

        double throughput =
            (total_bytes / (1024.0 * 1024.0)) / time_taken;

        printf("\nProducer generated %lld bytes.\n", total_bytes);
        printf("Number of messages : %d\n", NUM_MESSAGES);
        printf("Time taken         : %.6f seconds\n", time_taken);
        printf("Communication rate : %.2f MB/s\n", throughput);
    }

    return 0;
}
