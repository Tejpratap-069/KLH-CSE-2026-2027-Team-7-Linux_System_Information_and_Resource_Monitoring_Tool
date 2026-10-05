#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>

int main()
{
    int p1[2], p2[2];

    pipe(p1);
    pipe(p2);

    if (fork() == 0)
    {
        dup2(p1[1], STDOUT_FILENO);

        close(p1[0]);
        close(p1[1]);
        close(p2[0]);
        close(p2[1]);

        execlp("ls", "ls", NULL);
        exit(1);
    }

    if (fork() == 0)
    {
        dup2(p1[0], STDIN_FILENO);
        dup2(p2[1], STDOUT_FILENO);

        close(p1[0]);
        close(p1[1]);
        close(p2[0]);
        close(p2[1]);

        execlp("grep", "grep", ".c", NULL);
        exit(1);
    }

    if (fork() == 0)
    {
        dup2(p2[0], STDIN_FILENO);

        close(p1[0]);
        close(p1[1]);
        close(p2[0]);
        close(p2[1]);

        execlp("wc", "wc", "-l", NULL);
        exit(1);
    }

    close(p1[0]);
    close(p1[1]);
    close(p2[0]);
    close(p2[1]);

    wait(NULL);
    wait(NULL);
    wait(NULL);

    return 0;
}
