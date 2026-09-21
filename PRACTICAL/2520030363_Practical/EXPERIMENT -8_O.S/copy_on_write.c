#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

int main() {

    int *data = malloc(100000 * sizeof(int));

    if (data == NULL) {
        printf("Memory allocation failed\n");
        return 1;
    }

    for (int i = 0; i < 100000; i++) {
        data[i] = i;
    }

    printf("Before fork()\n");
    printf("data[0] = %d\n", data[0]);

    pid_t pid = fork();

    if (pid < 0) {
        perror("fork failed");
        free(data);
        return 1;
    }

    if (pid == 0) {

        printf("\nChild process\n");

        printf("Before modification: data[0] = %d\n",
               data[0]);

        data[0] = 999;

        printf("After modification: data[0] = %d\n",
               data[0]);

        printf("Child PID: %d\n", getpid());
    }
    else {

        wait(NULL);

        printf("\nParent process\n");
        printf("data[0] = %d\n", data[0]);
    }

    free(data);

    return 0;
}
