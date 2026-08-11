#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

int main() {
    pid_t pid = fork();

    if(pid == 0) {
        printf("Child Process PID: %d\n", getpid());
        exit(0);
    }
    else {
        printf("Parent PID: %d\n", getpid());
        sleep(20);

        printf("Check zombie process using:\n");
        printf("ps -el | grep Z\n");
    }

    return 0;
}
