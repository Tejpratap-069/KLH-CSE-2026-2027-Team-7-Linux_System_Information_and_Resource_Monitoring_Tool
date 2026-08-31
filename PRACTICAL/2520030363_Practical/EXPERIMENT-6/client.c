#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#define SERVER_FIFO "server_fifo"
#define CLIENT_FIFO "client_fifo"
#define SIZE 256

int main()
{
    int server_fd, client_fd;
    char message[SIZE];
    char response[SIZE];

    printf("====================================\n");
    printf("       FIFO CLIENT PROGRAM\n");
    printf("====================================\n");

    while (1)
    {
        printf("\nEnter message: ");
        fflush(stdout);

        if (fgets(message, SIZE, stdin) == NULL)
        {
            break;
        }

        // Remove newline
        message[strcspn(message, "\n")] = '\0';

        // Open server FIFO
        server_fd = open(SERVER_FIFO, O_WRONLY);

        if (server_fd == -1)
        {
            perror("Error opening server FIFO");
            exit(EXIT_FAILURE);
        }

        // Send message
        write(server_fd, message, strlen(message) + 1);

        close(server_fd);

        // Exit
        if (strcmp(message, "exit") == 0)
        {
            printf("Client exiting...\n");
            break;
        }

        // Open client FIFO
        client_fd = open(CLIENT_FIFO, O_RDONLY);

        if (client_fd == -1)
        {
            perror("Error opening client FIFO");
            exit(EXIT_FAILURE);
        }

        memset(response, 0, SIZE);

        // Read server response
        read(client_fd, response, SIZE - 1);

        printf("Server Response: %s\n", response);

        close(client_fd);
    }

    return 0;
}
