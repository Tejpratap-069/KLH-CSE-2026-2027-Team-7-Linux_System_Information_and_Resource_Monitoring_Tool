#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <string.h>

#define SIZE 1000000

int main()
{
    int pipefd[2];
    pid_t pid;

    char *data = malloc(SIZE);
    char *buffer = malloc(SIZE);

    if (data == NULL || buffer == NULL)
    {
        perror("Memory allocation failed");
        return 1;
    }

    // Generate data
    for (int i = 0; i < SIZE; i++)
    {
        data[i] = 'A' + (i % 26);
    }

    // Create anonymous pipe
    if (pipe(pipefd) == -1)
    {
        perror("Pipe creation failed");
        return 1;
    }

    // Create child process
    pid = fork();

    if (pid < 0)
    {
        perror("Fork failed");
        return 1;
    }

    if (pid > 0)
    {
        // =========================
        // PARENT - PRODUCER
        // =========================

        struct timeval start, end;

        close(pipefd[0]);   // Close unused read end

        gettimeofday(&start, NULL);

        // Write data to pipe
        int total_written = 0;

        while (total_written < SIZE)
        {
            int n = write(pipefd[1],
                          data + total_written,
                          SIZE - total_written);

            if (n <= 0)
            {
                perror("Write failed");
                break;
            }

            total_written += n;
        }

        close(pipefd[1]);   // Signal end of data

        wait(NULL);         // Wait for child

        gettimeofday(&end, NULL);

        double time_taken =
            (end.tv_sec - start.tv_sec) +
            (end.tv_usec - start.tv_usec) / 1000000.0;

        double throughput =
            (total_written / (1024.0 * 1024.0)) / time_taken;

        printf("\n========== Communication Efficiency ==========\n");
        printf("Data Transferred : %d bytes\n", total_written);
        printf("Time Taken       : %.6f seconds\n", time_taken);
        printf("Throughput       : %.2f MB/s\n", throughput);
        printf("==============================================\n");
    }
    else
    {
        // =========================
        // CHILD - CONSUMER
        // =========================

        close(pipefd[1]);   // Close unused write end

        int total_read = 0;

        while (total_read < SIZE)
        {
            int n = read(pipefd[0],
                         buffer + total_read,
                         SIZE - total_read);

            if (n < 0)
            {
                perror("Read failed");
                break;
            }

            if (n == 0)
                break;

            total_read += n;
        }

        close(pipefd[0]);

        printf("\nChild (Consumer): Received %d bytes\n",
               total_read);

        // Verify received data
        if (memcmp(data, buffer, total_read) == 0)
        {
            printf("Data Verification: SUCCESS\n");
        }
        else
        {
            printf("Data Verification: FAILED\n");
        }

        exit(0);
    }

    free(data);
    free(buffer);

    return 0;
}
