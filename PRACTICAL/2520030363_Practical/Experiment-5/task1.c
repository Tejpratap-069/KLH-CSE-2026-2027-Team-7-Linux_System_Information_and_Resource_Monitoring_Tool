#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

int main()
{
    int fd[2];
    pid_t p1, p2;

    // Create pipe
    if (pipe(fd) == -1)
    {
        perror("pipe");
        return 1;
    }

    // Create first child
    p1 = fork();

    if (p1 == -1)
    {
        perror("fork");
        return 1;
    }

    if (p1 == 0)
    {
        // Child 1: execute ls -l

        close(fd[0]);

        // Redirect stdout to pipe
        dup2(fd[1], STDOUT_FILENO);

        close(fd[1]);

        execlp("ls", "ls", "-l", NULL);

        perror("execlp");
        exit(1);
    }

    // Create second child
    p2 = fork();

    if (p2 == -1)
    {
        perror("fork");
        return 1;
    }

    if (p2 == 0)
    {
        // Child 2: execute grep ".c"

        close(fd[1]);

        // Redirect stdin from pipe
        dup2(fd[0], STDIN_FILENO);

        close(fd[0]);

        execlp("grep", "grep", ".c", NULL);

        perror("execlp");
        exit(1);
    }

    // Parent closes pipe
    close(fd[0]);
    close(fd[1]);

    // Wait for children
    waitpid(p1, NULL, 0);
    waitpid(p2, NULL, 0);

    return 0;
}
