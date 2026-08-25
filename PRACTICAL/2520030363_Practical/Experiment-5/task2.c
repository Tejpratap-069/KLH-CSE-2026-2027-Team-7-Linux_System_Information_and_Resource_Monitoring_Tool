#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>

int main()
{
    int fd[2];
    pid_t pid;
    char message[] = "Hello from Parent Process!";
    char buffer[100];

    // Create pipe
    if (pipe(fd) == -1)
    {
        perror("pipe");
        return 1;
    }

    // Create child process
    pid = fork();

    if (pid == -1)
    {
        perror("fork");
        return 1;
    }

    if (pid > 0)
    {
        // Parent process

        close(fd[0]);

        // Send message to child
        write(fd[1], message, strlen(message) + 1);

        printf("Parent: Message sent to child.\n");

        close(fd[1]);

        wait(NULL);
    }
    else
    {
        // Child process

        close(fd[1]);

        // Read message from parent
        read(fd[0], buffer, sizeof(buffer));

        printf("Child: Message received: %s\n", buffer);

        close(fd[0]);
    }

    return 0;
}
