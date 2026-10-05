#include <stdio.h>

#define BUFFER_SIZE 1024

int main(int argc, char *argv[])
{
    FILE *source, *destination;
    char buffer[BUFFER_SIZE];
    size_t bytesRead;

    if (argc != 3)
    {
        printf("Usage: %s source destination\n", argv[0]);
        return 1;
    }

    source = fopen(argv[1], "rb");

    if (source == NULL)
    {
        perror("Error opening source file");
        return 1;
    }

    destination = fopen(argv[2], "wb");

    if (destination == NULL)
    {
        perror("Error opening destination file");
        fclose(source);
        return 1;
    }

    while ((bytesRead = fread(buffer, 1, BUFFER_SIZE, source)) > 0)
    {
        fwrite(buffer, 1, bytesRead, destination);
    }

    fclose(source);
    fclose(destination);

    printf("File copied using standard library functions.\n");

    return 0;
}
