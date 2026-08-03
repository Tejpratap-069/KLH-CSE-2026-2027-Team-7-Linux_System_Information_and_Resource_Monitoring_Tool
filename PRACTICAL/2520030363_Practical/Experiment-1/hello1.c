#include<stdio.h>
#include<unistd.h>
int main() {
pid_t pid;
pid = fork();
if(pid==0)
printf("I am Child\n");
else
 printf("I am parent\n");
return 0;
}
