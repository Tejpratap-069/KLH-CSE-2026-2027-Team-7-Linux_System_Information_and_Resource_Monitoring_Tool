#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

int main()
{
    int fd;
    char buffer[200];

    fd = open("input.txt", O_RDONLY);

    if (fd < 0)
    {
        perror("open");
        return 1;
    }

    int n = read(fd, buffer, sizeof(buffer) - 1);

    buffer[n] = '\0';

    printf("%s", buffer);

    close(fd);

    return 0;
}
