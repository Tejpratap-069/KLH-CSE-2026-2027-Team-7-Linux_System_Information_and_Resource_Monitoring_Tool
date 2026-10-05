#include <stdio.h>
#include <stdlib.h>

int main()
{
    int *p;

    p = malloc(5 * sizeof(int));

    if (p == NULL)
        return 1;

    for (int i = 0; i < 5; i++)
        p[i] = i * 10;

    for (int i = 0; i < 5; i++)
        printf("%d ", p[i]);

    free(p);

    return 0;
}
