#include <stdio.h>

int main() {
    char *name = "Akshay";
    char *str1 = '$name';
    char *str2 = 'Hello $name';

    printf("Single quoted content:\n");
    printf("%s\n", str1);
    printf("%s\n", str2);

    return 0;
}
