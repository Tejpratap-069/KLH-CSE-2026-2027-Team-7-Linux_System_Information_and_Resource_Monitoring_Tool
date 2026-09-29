#define _GNU_SOURCE
#include "process_demo.h"
#include "utils.h"
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

int run_process_ipc_demo_json(char **json_out) {
    if(!json_out) return -1;
    *json_out=NULL;
    int fds[2];
    if(pipe(fds)!=0) return -1;
    uint64_t start=now_ms();
    pid_t parent=getpid();
    pid_t child=fork();
    if(child<0){close(fds[0]);close(fds[1]);return -1;}
    if(child==0){
        close(fds[0]);
        char up[128]="unavailable"; size_t n=0;
        if(read_text_file("/proc/uptime",up,sizeof(up),&n)==0) trim_newline(up);
        char msg[256];
        int m=snprintf(msg,sizeof(msg),"Child %d read /proc/uptime: %s",getpid(),up);
        if(m>0) safe_write_all(fds[1],msg,(size_t)m);
        close(fds[1]);
        _exit(0);
    }
    close(fds[1]);
    char msg[512]; size_t off=0;
    for(;;){ssize_t n=read(fds[0],msg+off,sizeof(msg)-1-off);if(n<0){if(errno==EINTR)continue;break;}if(n==0)break;off+=(size_t)n;if(off>=sizeof(msg)-1)break;}
    msg[off]='\0'; close(fds[0]);
    int status=0; pid_t w=waitpid(child,&status,0); uint64_t end=now_ms();
    char *b=NULL; size_t l=0,c=0;
    appendf(&b,&l,&c,"{\"parent_pid\":%d,\"child_pid\":%d,\"message\":",parent,child);json_escape_append(&b,&l,&c,msg);
    appendf(&b,&l,&c,",\"waitpid_return\":%d,\"child_exit_status\":%d,\"started_ms\":%llu,\"finished_ms\":%llu,\"duration_ms\":%llu,\"mechanisms\":[\"pipe()\",\"fork()\",\"read()\",\"write()\",\"waitpid()\"]}",(int)w,WIFEXITED(status)?WEXITSTATUS(status):-1,(unsigned long long)start,(unsigned long long)end,(unsigned long long)(end-start));
    *json_out=b; return 0;
}
