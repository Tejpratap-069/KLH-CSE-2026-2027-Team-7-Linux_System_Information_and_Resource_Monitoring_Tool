#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

int main()
{
    int fd;

    fd = open("output.txt",
              O_WRONLY | O_CREAT | O_APPEND,
              0644);

    dup2(fd, STDOUT_FILENO);

    close(fd);

    printf("New line added\n");

    return 0;
}
