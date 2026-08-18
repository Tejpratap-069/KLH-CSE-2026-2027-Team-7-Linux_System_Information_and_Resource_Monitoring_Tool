#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

int main()
{
    int fd;
    char buffer[100];
    ssize_t bytes_read;

    fd = open("input.txt", O_RDONLY);

    if (fd == -1)
    {
        perror("Error opening file");
        return 1;
    }

    bytes_read = read(fd, buffer, sizeof(buffer) - 1);

    if (bytes_read == -1)
    {
        perror("Error reading file");
        close(fd);
        return 1;
    }

    buffer[bytes_read] = '\0';

    printf("Contents of the file:\n");
    printf("%s\n", buffer);

    close(fd);

    return 0;
}
