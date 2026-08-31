#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

void signal_handler(int sig)
{
    if (sig == SIGINT)
    {
        printf("\nReceived SIGINT (Ctrl+C)\n");
        printf("SIGINT handler executed.\n");
    }
    else if (sig == SIGTERM)
    {
        printf("\nReceived SIGTERM\n");
        printf("SIGTERM handler executed.\n");
        printf("Program terminating...\n");
        exit(0);
    }
    else if (sig == SIGUSR1)
    {
        printf("\nReceived SIGUSR1\n");
        printf("SIGUSR1 handler executed.\n");
    }
}

int main()
{
    printf("POSIX Signal Handling Program\n");
    printf("Process ID (PID): %d\n", getpid());

    // Register signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGUSR1, signal_handler);

    printf("\nWaiting for signals...\n");
    printf("Press Ctrl+C to send SIGINT.\n");
    printf("Use another terminal to send SIGUSR1 or SIGTERM.\n");

    while (1)
    {
        printf("Program is running...\n");
        sleep(3);
    }

    return 0;
}
