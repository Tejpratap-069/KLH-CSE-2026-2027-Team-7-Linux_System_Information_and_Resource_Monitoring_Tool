#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

int main()
{
    int fd;

    fd = open("combined.txt",
              O_WRONLY | O_CREAT | O_TRUNC,
              0644);

    dup2(fd, STDOUT_FILENO);
    dup2(fd, STDERR_FILENO);

    close(fd);

    printf("Normal output\n");
    fprintf(stderr, "Error output\n");

    return 0;
}
