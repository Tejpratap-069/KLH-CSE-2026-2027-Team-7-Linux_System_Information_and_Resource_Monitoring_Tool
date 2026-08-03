#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
int main()
{
char cmd[50];
printf("Enter Linux Command: ");
scanf("%s", cmd);
pid_t pid = fork();
if(pid < 0)
{
printf("Fork Failed\n");
return 1;
}
else if(pid == 0)
{
printf("\nChild Process\n");
printf("Child PID : %d\n", getpid());
printf("Parent PID: %d\n", getppid());
execlp(cmd, cmd, NULL);
perror("Command Execution Failed");
exit(1);
}
else
{
printf("\nParent Process\n");
printf("Parent PID : %d\n", getpid());
printf("Child PID : %d\n", pid);
wait(NULL);
printf("\nChild Process Completed\n");
}
return 0;
}
