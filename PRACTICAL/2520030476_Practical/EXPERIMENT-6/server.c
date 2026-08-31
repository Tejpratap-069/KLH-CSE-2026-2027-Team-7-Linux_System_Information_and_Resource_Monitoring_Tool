#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#define SERVER_FIFO "server_fifo"
#define CLIENT_FIFO "client_fifo"
#define SIZE 256

int main()
{
    int server_fd, client_fd;
    char message[SIZE];
    char response[SIZE];

    // Create named pipes
    mkfifo(SERVER_FIFO, 0666);
    mkfifo(CLIENT_FIFO, 0666);

    printf("====================================\n");
    printf("       FIFO SERVER PROGRAM\n");
    printf("====================================\n");
    printf("Server started...\n");
    printf("Waiting for client messages...\n");

    while (1)
    {
        // Open server FIFO for reading
        server_fd = open(SERVER_FIFO, O_RDONLY);

        if (server_fd == -1)
        {
            perror("Error opening server FIFO");
            exit(EXIT_FAILURE);
        }

        memset(message, 0, SIZE);

        // Read message
        ssize_t bytes_read = read(server_fd, message, SIZE - 1);

        close(server_fd);

        if (bytes_read > 0)
        {
            message[bytes_read] = '\0';

            // Remove newline
            message[strcspn(message, "\n")] = '\0';

            printf("\nClient Message: %s\n", message);

            // Exit condition
            if (strcmp(message, "exit") == 0)
            {
                printf("Server shutting down...\n");
                break;
            }

            // Safely create response
            snprintf(response, SIZE,
                     "Server processed: %.220s", message);

            // Open client FIFO for writing
            client_fd = open(CLIENT_FIFO, O_WRONLY);

            if (client_fd == -1)
            {
                perror("Error opening client FIFO");
                continue;
            }

            write(client_fd, response, strlen(response) + 1);

            printf("Response sent: %s\n", response);

            close(client_fd);
        }
    }

    // Remove FIFOs
    unlink(SERVER_FIFO);
    unlink(CLIENT_FIFO);

    printf("FIFOs removed.\n");
    printf("Server terminated.\n");

    return 0;
}
