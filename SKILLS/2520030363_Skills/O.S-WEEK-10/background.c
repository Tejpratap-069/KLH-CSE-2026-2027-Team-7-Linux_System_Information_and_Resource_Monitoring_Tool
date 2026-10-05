#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

int main()
{
    pid_t pid = fork();

    if (pid == 0)
    {
        printf("Child process running\n");
        sleep(5);
        printf("Child completed\n");
    }
    else
    {
        printf("Parent continues\n");
        printf("Child PID = %d\n", pid);

        wait(NULL);
    }

    return 0;
}
