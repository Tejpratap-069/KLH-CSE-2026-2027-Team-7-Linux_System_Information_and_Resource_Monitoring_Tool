#include <stdio.h>
#include <signal.h>
#include <unistd.h>

void handler(int sig)
{
    printf("\nSignal received: %d\n", sig);
}

int main()
{
    signal(SIGUSR1, handler);

    printf("PID = %d\n", getpid());
    printf("Use another terminal:\n");
    printf("kill -USR1 PID\n");

    while (1)
        pause();

    return 0;
}
