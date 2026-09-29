#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
int main(void){size_t n=32u*1024u*1024u;unsigned char *p=malloc(n);if(!p){perror("malloc");return 1;}memset(p,0xA5,n);printf("demo-memory PID=%d allocated=%zu MiB for 6 seconds\n",getpid(),n/(1024u*1024u));fflush(stdout);sleep(6);free(p);return 0;}
