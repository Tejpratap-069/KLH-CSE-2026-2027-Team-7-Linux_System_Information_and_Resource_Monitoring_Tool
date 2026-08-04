#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

int main() {
    char command[100];

    printf("Enter command: ");
    scanf("%99s", command);

    pid_t pid = fork();

    if (pid < 0) {
        printf("Fork failed!\n");
    }
    else if (pid == 0) {
        execlp(command, command, NULL);
        printf("Command execution failed!\n");
    }
    else {
        wait(NULL);
        printf("Child process completed.\n");
    }

    return 0;
}
