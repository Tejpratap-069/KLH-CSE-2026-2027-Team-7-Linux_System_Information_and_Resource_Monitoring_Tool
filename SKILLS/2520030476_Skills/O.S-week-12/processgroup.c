#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>

int main()
{
    pid_t pid = fork();

    if (pid == 0)
    {
        setpgid(0, 0);

        printf("Child PID  = %d\n", getpid());
        printf("Child PGID = %d\n", getpgrp());

        sleep(10);
    }
    else
    {
        sleep(1);

        printf("Parent PID  = %d\n", getpid());
        printf("Parent PGID = %d\n", getpgrp());

        kill(-pid, SIGTERM);

        wait(NULL);

        printf("Child group terminated\n");
    }

    return 0;
}
