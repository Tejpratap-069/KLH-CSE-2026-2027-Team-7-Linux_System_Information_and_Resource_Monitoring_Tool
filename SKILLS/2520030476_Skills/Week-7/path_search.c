#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main() {
    char *path = getenv("PATH");

    if (path == NULL) {
        printf("PATH variable not found.\n");
        return 1;
    }

    printf("PATH = %s\n\n", path);

    char *copy = strdup(path);
    char *dir = strtok(copy, ":");

    while (dir != NULL) {
        printf("Directory: %s\n", dir);
        dir = strtok(NULL, ":");
    }

    free(copy);

    return 0;
}
