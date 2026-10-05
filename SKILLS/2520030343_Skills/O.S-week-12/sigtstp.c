#include <stdio.h>
#include <signal.h>
#include <unistd.h>

void handler(int sig)
{
    printf("\nSIGTSTP received\n");
}

int main()
{
    signal(SIGTSTP, handler);

    printf("Press Ctrl+Z\n");

    while (1)
        sleep(1);

    return 0;
}
