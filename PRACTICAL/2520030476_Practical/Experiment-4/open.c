#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

int main()
{
    int fd;

    fd = open("input.txt", O_RDONLY);

    if (fd == -1)
    {
        perror("Error opening file");
        return 1;
    }

    printf("File opened successfully.\n");
    printf("File descriptor: %d\n", fd);

    close(fd);

    return 0;
}
