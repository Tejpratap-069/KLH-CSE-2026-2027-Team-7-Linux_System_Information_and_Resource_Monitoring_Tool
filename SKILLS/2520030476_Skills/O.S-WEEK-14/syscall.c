#include <stdio.h>
#include <unistd.h>

int main()
{
    printf("Process ID = %d\n", getpid());
    printf("Parent ID  = %d\n", getppid());

    return 0;
}
