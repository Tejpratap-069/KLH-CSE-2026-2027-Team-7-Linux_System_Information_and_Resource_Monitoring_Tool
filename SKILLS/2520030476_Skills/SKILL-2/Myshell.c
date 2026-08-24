#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define MAX_INPUT 1024
#define MAX_ARGS 64

int main()
{
    char input[MAX_INPUT];
    char *args[MAX_ARGS];

    while (1)
    {
        printf("myshell> ");
        fflush(stdout);

        // Read command
        if (fgets(input, sizeof(input), stdin) == NULL)
        {
            printf("\nExiting shell...\n");
            break;
        }

        // Remove newline
        input[strcspn(input, "\n")] = '\0';

        // Ignore empty command
        if (strlen(input) == 0)
            continue;

        // Exit command
        if (strcmp(input, "exit") == 0)
        {
            printf("Exiting shell...\n");
            break;
        }

        // Tokenize command
        int count = 0;
        char *token = strtok(input, " ");

        while (token != NULL && count < MAX_ARGS - 1)
        {
            args[count++] = token;
            token = strtok(NULL, " ");
        }

        args[count] = NULL;

        // Built-in cd command
        if (strcmp(args[0], "cd") == 0)
        {
            if (args[1] == NULL)
            {
                printf("cd: missing directory\n");
            }
            else if (chdir(args[1]) != 0)
            {
                perror("cd");
            }

            continue;
        }

        // Create child process
        pid_t pid = fork();

        if (pid < 0)
        {
            perror("fork");
        }
        else if (pid == 0)
        {
            // Child process executes command
            execvp(args[0], args);

            // If execvp fails
            perror("Command not found");
            exit(EXIT_FAILURE);
        }
        else
        {
            // Parent waits for child
            wait(NULL);
        }
    }

    return 0;
}
