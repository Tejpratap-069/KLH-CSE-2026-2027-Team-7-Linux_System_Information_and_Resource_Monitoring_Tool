#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

int main()
{
    int fd;

    char message[] =
        "Hello! This message is written using write() system call.\n";

    fd = open("output.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);

    if (fd == -1)
    {
        perror("Error opening file");
        return 1;
    }

    if (write(fd, message, sizeof(message) - 1) == -1)
    {
        perror("Error writing to file");
        close(fd);
        return 1;
    }

    printf("Data written successfully.\n");

    close(fd);

    return 0;
}
