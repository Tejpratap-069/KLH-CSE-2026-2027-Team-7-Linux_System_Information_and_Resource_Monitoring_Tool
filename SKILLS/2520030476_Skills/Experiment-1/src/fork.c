#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>

int main()
{
    char command[100];

    while (1)
    {
        printf("myshell> ");

        if (fgets(command, sizeof(command), stdin) == NULL)
        {
            break;
        }

        // Remove newline
        command[strcspn(command, "\n")] = '\0';

        // Exit command
        if (strcmp(command, "exit") == 0)
        {
            break;
        }

        pid_t pid = fork();

        if (pid == 0)
        {
            // Child process
            execlp(command, command, NULL);

            printf("Command not found!\n");
            exit(1);
        }
        else if (pid > 0)
        {
            // Parent process
            wait(NULL);
        }
        else
        {
            printf("Fork failed!\n");
        }
    }

    return 0;
}
