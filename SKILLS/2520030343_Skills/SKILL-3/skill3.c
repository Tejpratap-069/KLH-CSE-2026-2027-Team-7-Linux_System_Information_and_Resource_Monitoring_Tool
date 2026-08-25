#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_HISTORY 10
#define BUFFER_SIZE 100

char history[MAX_HISTORY][BUFFER_SIZE];
int history_count = 0;

/* Add command to history */
void add_history(const char *command)
{
    if (strlen(command) == 0)
        return;

    if (history_count < MAX_HISTORY)
    {
        strcpy(history[history_count], command);
        history_count++;
    }
    else
    {
        for (int i = 0; i < MAX_HISTORY - 1; i++)
            strcpy(history[i], history[i + 1]);

        strcpy(history[MAX_HISTORY - 1], command);
    }
}

/* Display command history */
void show_history()
{
    printf("\nCommand History:\n");

    if (history_count == 0)
    {
        printf("No commands in history.\n");
        return;
    }

    for (int i = 0; i < history_count; i++)
        printf("%d  %s\n", i + 1, history[i]);
}

/* Recall previous command */
void previous_command(int *position)
{
    if (history_count == 0)
    {
        printf("History is empty.\n");
        return;
    }

    if (*position > 0)
        (*position)--;

    printf("Previous Command: %s\n", history[*position]);
}

/* Recall next command */
void next_command(int *position)
{
    if (history_count == 0)
    {
        printf("History is empty.\n");
        return;
    }

    if (*position < history_count - 1)
        (*position)++;

    printf("Next Command: %s\n", history[*position]);
}

int main()
{
    char buffer[BUFFER_SIZE];
    int choice;
    int position = 0;

    printf("====================================\n");
    printf("       SKILL 3 - COMMAND HISTORY\n");
    printf("====================================\n");

    while (1)
    {
        printf("\n1. Enter Command");
        printf("\n2. Show History");
        printf("\n3. Previous Command");
        printf("\n4. Next Command");
        printf("\n5. Clear Input Buffer");
        printf("\n6. Exit");

        printf("\n\nEnter choice: ");
        scanf("%d", &choice);
        getchar();

        switch (choice)
        {
            case 1:
                printf("Enter command: ");
                fgets(buffer, BUFFER_SIZE, stdin);

                buffer[strcspn(buffer, "\n")] = '\0';

                add_history(buffer);

                position = history_count - 1;

                printf("Command stored successfully.\n");
                break;

            case 2:
                show_history();
                break;

            case 3:
                previous_command(&position);
                break;

            case 4:
                next_command(&position);
                break;

            case 5:
                memset(buffer, 0, BUFFER_SIZE);
                printf("Input buffer cleared successfully.\n");
                break;

            case 6:
                printf("Exiting program...\n");
                return 0;

            default:
                printf("Invalid choice!\n");
        }
    }

    return 0;
}
