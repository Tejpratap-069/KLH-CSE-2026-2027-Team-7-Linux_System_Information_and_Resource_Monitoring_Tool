#include <stdio.h>
#include <string.h>

int main() {
    char input[100];

    while (1) {
        // Display prompt
        printf("myshell> ");
        fflush(stdout);

        // Read user input
        if (fgets(input, sizeof(input), stdin) == NULL) {
            printf("\nExiting...\n");
            break;
        }

        // Remove newline character
        input[strcspn(input, "\n")] = '\0';

        // Handle exit condition
        if (strcmp(input, "exit") == 0) {
            printf("Exiting shell...\n");
            break;
        }

        // Display entered command
        printf("You entered: %s\n", input);
    }

    return 0;
}
