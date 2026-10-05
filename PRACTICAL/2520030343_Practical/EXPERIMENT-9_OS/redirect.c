#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

int main()
{
    int fd;

    fd = open("output.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);

    if (fd == -1)
    {
        perror("open");
        return 1;
    }

    dup2(fd, STDOUT_FILENO);

    close(fd);

    printf("This output is redirected to output.txt\n");
    printf("dup2() successfully changed standard output.\n");

    return 0;
}
