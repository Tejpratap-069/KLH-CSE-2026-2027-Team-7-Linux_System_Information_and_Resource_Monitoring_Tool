#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

int main() {
    pid_t pid1, pid2;

    pid1 = fork();

    if(pid1 == 0) {
        printf("Child 1 PID: %d\n", getpid());
        sleep(2);
        return 0;
    }

    pid2 = fork();

    if(pid2 == 0) {
        printf("Child 2 PID: %d\n", getpid());
        sleep(4);
        return 0;
    }

    wait(NULL);
    printf("One child completed using wait()\n");

    waitpid(pid2, NULL, 0);
    printf("Child 2 completed using waitpid()\n");

    return 0;
}
