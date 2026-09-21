#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

int main() {
    pid_t pid;
    int status;

    pid = fork();

    if (pid < 0) {
        perror("fork failed");
        return 1;
    }

    if (pid == 0) {
        printf("Child process running...\n");
        sleep(2);
        printf("Child process finished.\n");
        return 10;
    }
    else {
        printf("Parent waiting for child...\n");

        waitpid(pid, &status, 0);

        if (WIFEXITED(status)) {
            printf("Child exited with status: %d\n",
                   WEXITSTATUS(status));
        }

        printf("Parent process finished.\n");
    }

    return 0;
}
