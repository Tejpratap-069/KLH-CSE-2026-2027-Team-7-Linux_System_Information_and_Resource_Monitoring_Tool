#include <stdio.h>
#include <signal.h>
#include <unistd.h>

void handler(int sig)
{
    printf("\nSIGINT received\n");
}

int main()
{
    signal(SIGINT, handler);

    printf("Press Ctrl+C\n");

    while (1)
        sleep(1);

    return 0;
}
