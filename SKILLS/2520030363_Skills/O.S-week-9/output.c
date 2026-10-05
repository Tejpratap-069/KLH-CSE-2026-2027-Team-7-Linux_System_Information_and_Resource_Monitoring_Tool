#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

int main()
{
    int fd;

    fd = open("output.txt",
              O_WRONLY | O_CREAT | O_TRUNC,
              0644);

    if (fd < 0)
    {
        perror("open");
        return 1;
    }

    dup2(fd, STDOUT_FILENO);

    close(fd);

    printf("Hello from output redirection\n");

    return 0;
}
