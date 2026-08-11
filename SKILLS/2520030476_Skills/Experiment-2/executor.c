#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
int main() {
    char command[100];
    // Accept Linux command from the user
    printf("Enter a Linux command: ");
    scanf("%99s", command);
    // Create a child process
    pid_t pid = fork();
    if (pid < 0) {
        // Fork failed
        perror("fork failed");
        return 1;
    }
    else if (pid == 0) {
        // Child process
        printf("\nChild Process\n");
        printf("Child PID  : %d\n", getpid());
        printf("Parent PID : %d\n", getppid());
        printf("Executing command: %s\n\n", command);
        // Execute the command
        execlp(command, command, NULL);

        // Executes only if exec fails
        perror("Execution failed");
        exit(1);
    }
    else {
        // Parent process
        printf("\nParent Process\n");
        printf("Parent PID : %d\n", getpid());
        printf("Child PID  : %d\n", pid);
        // Wait for child to finish
        wait(NULL);
        printf("\nChild process completed.\n");
    }
    return 0;
}
