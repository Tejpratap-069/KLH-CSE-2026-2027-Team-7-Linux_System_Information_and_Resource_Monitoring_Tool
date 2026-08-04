#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>

int main()
{
    char command[100];

    while(1)
    {
        printf("myshell> ");
        fgets(command, sizeof(command), stdin);

        command[strcspn(command, "\n")] = 0;

        if(strcmp(command, "exit") == 0)
        {
            break;
        }

        pid_t pid = fork();

        if(pid == 0)
        {
            execlp(command, command, NULL);

            printf("Command not found!\n");
            exit(1);
        }
        else if(pid > 0)
        {
            wait(NULL);
        }
        else
        {
            printf("Fork Failed!\n");
        }
    }

    return 0;
}
