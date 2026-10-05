#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

int main()
{
    int fd;

    fd = open("error.txt",
              O_WRONLY | O_CREAT | O_TRUNC,
              0644);

    dup2(fd, STDERR_FILENO);

    close(fd);

    fprintf(stderr, "This is an error message\n");

    return 0;
}
