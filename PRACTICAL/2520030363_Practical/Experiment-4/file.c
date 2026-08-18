#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

int main() {
    int source_fd, dest_fd;
    char source[100], destination[100];
    char buffer[1024];
    ssize_t bytesRead;

    printf("Enter source file name: ");
    scanf("%s", source);

    printf("Enter destination file name: ");
    scanf("%s", destination);

    source_fd = open(source, O_RDONLY);

    if (source_fd < 0) {
        perror("Error opening source file");
        exit(1);
    }

    dest_fd = open(destination,
                   O_WRONLY | O_CREAT | O_TRUNC,
                   0644);

    if (dest_fd < 0) {
        perror("Error creating destination file");
        close(source_fd);
        exit(1);
    }

    while ((bytesRead = read(source_fd,
                             buffer,
                             sizeof(buffer))) > 0) {
        write(dest_fd, buffer, bytesRead);
    }

    if (bytesRead < 0) {
        perror("Read Error");
    }

    close(source_fd);
    close(dest_fd);

    printf("File copied successfully.\n");

    return 0;
}
