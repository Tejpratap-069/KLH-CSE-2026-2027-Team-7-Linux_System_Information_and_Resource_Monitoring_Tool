#define _GNU_SOURCE
#include "remote_ssh.h"
#include "application_monitor.h"
#include "utils.h"
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define SSH_OUTPUT_LIMIT (6*1024*1024)

static const char *REMOTE_SNAPSHOT_SCRIPT =
"set -u\n"
"[ -r /proc/stat ] && [ -r /proc/meminfo ] || { printf '__ERR_PROC__\\n'; exit 20; }\n"
"safe(){ printf '%s' \"$1\" | tr '|\\r\\n' '   '; }\n"
"hn=$(hostname 2>/dev/null || printf unknown); printf 'HOST|'; safe \"$hn\"; printf '\\n'\n"
"kr=$(uname -r 2>/dev/null || printf unknown); printf 'KERNEL|'; safe \"$kr\"; printf '\\n'\n"
"dist=$(awk -F= '/^PRETTY_NAME=/{v=$2; gsub(/^\"|\"$/,"",v); print v; exit}' /etc/os-release 2>/dev/null); printf 'DISTRO|'; safe \"${dist:-Linux}\"; printf '\\n'\n"
"model=$(awk -F: '/^model name/{sub(/^[ \\t]+/,\"\",$2);print $2;exit}' /proc/cpuinfo 2>/dev/null); printf 'MODEL|'; safe \"${model:-Unknown CPU}\"; printf '\\n'\n"
"cores=$(grep -c '^processor' /proc/cpuinfo 2>/dev/null || printf 1); printf 'CORES|%s\\n' \"${cores:-1}\"\n"
"hz=$(getconf CLK_TCK 2>/dev/null || printf 100); printf 'HZ|%s\\n' \"$hz\"\n"
"pg=$(getconf PAGESIZE 2>/dev/null || printf 4096); printf 'PAGESIZE|%s\\n' \"$pg\"\n"
"awk '/^cpu /{t=0;for(i=2;i<=9&&i<=NF;i++)t+=$i; idle=$5+$6; printf \"CPU|%.0f|%.0f\\n\",t,idle;exit}' /proc/stat\n"
"awk '{printf \"UP|%s\\n\",$1}' /proc/uptime 2>/dev/null\n"
"awk '{printf \"LOAD|%s|%s|%s\\n\",$1,$2,$3}' /proc/loadavg 2>/dev/null\n"
"awk '/^MemTotal:/{mt=$2}/^MemAvailable:/{ma=$2}/^SwapTotal:/{st=$2}/^SwapFree:/{sf=$2}END{printf \"MEM|%.0f|%.0f|%.0f|%.0f\\n\",mt,ma,st,sf}' /proc/meminfo\n"
"df -Pk / 2>/dev/null | awk 'NR==2{printf \"DISK|%.0f|%.0f|%.0f\\n\",$2*1024,$3*1024,$4*1024}'\n"
"for d in /sys/class/net/*; do [ -e \"$d\" ] || continue; n=${d##*/}; st=$(cat \"$d/operstate\" 2>/dev/null || printf unknown); rx=$(cat \"$d/statistics/rx_bytes\" 2>/dev/null || printf 0); tx=$(cat \"$d/statistics/tx_bytes\" 2>/dev/null || printf 0); ip4=$(ip -o -4 addr show dev \"$n\" 2>/dev/null | awk 'NR==1{split($4,a,\"/\");print a[1]}'); printf 'NET|'; safe \"$n\"; printf '|'; safe \"$st\"; printf '|'; safe \"$ip4\"; printf '|%s|%s\\n' \"$rx\" \"$tx\"; done\n"
"for d in /proc/[0-9]*; do [ -r \"$d/stat\" ] || continue; pid=${d##*/}; stat=$(cat \"$d/stat\" 2>/dev/null) || continue; stat=$(printf '%s' \"$stat\" | tr '|' '/'); uid=$(awk '/^Uid:/{print $2;exit}' \"$d/status\" 2>/dev/null); th=$(awk '/^Threads:/{print $2;exit}' \"$d/status\" 2>/dev/null); cmd=$(head -c 512 \"$d/cmdline\" 2>/dev/null | tr '\\000' ' ' | tr '|\\r\\n' '   '); printf 'PROC|%s|%s|%s|%s|' \"$pid\" \"$uid\" \"$th\" \"$stat\"; safe \"$cmd\"; printf '\\n'; done\n";

static int run_ssh_script(const MachineConfig *m,const char *identity,const char *script,char **out,char *err,size_t errcap,int timeout_ms){
    if(!m||!script||!out) return -1;
    *out=NULL;
    int inpipe[2],outpipe[2];
    if(pipe(inpipe)!=0){if(err&&errcap)snprintf(err,errcap,"pipe() failed: %s",strerror(errno));return-1;}
    if(pipe(outpipe)!=0){int saved=errno;close(inpipe[0]);close(inpipe[1]);if(err&&errcap)snprintf(err,errcap,"pipe() failed: %s",strerror(saved));return-1;}
    pid_t pid=fork();if(pid<0){close(inpipe[0]);close(inpipe[1]);close(outpipe[0]);close(outpipe[1]);if(err&&errcap)snprintf(err,errcap,"fork() failed: %s",strerror(errno));return-1;}
    if(pid==0){
        dup2(inpipe[0],STDIN_FILENO);dup2(outpipe[1],STDOUT_FILENO);dup2(outpipe[1],STDERR_FILENO);
        close(inpipe[0]);close(inpipe[1]);close(outpipe[0]);close(outpipe[1]);
        char port[16],target[384];snprintf(port,sizeof(port),"%d",m->port);
        if(strcmp(m->username,"__ssh_config__")==0)snprintf(target,sizeof(target),"%s",m->host);
        else snprintf(target,sizeof(target),"%s@%s",m->username,m->host);
        const char *argv[24];int a=0;argv[a++]="ssh";argv[a++]="-o";argv[a++]="BatchMode=yes";argv[a++]="-o";argv[a++]="ConnectTimeout=4";argv[a++]="-o";argv[a++]="ConnectionAttempts=1";argv[a++]="-o";argv[a++]="ServerAliveInterval=2";argv[a++]="-o";argv[a++]="ServerAliveCountMax=2";argv[a++]="-o";argv[a++]="StrictHostKeyChecking=accept-new";argv[a++]="-p";argv[a++]=port;
        if(identity&&*identity){argv[a++]="-i";argv[a++]=identity;}
        argv[a++]=target;argv[a++]="sh";argv[a++]="-s";argv[a]=NULL;
        execvp("ssh",(char *const*)argv);_exit(127);
    }
    close(inpipe[0]);close(outpipe[1]);
    safe_write_all(inpipe[1],script,strlen(script));close(inpipe[1]);
    int flags=fcntl(outpipe[0],F_GETFL,0);fcntl(outpipe[0],F_SETFL,flags|O_NONBLOCK);
    size_t cap=8192,len=0;char *buf=malloc(cap);if(!buf){kill(pid,SIGKILL);waitpid(pid,NULL,0);close(outpipe[0]);return-1;}buf[0]='\0';
    uint64_t start=now_ms();int status=0,done=0,eof=0,timed_out=0;
    while(!done||!eof){
        struct pollfd pfd={outpipe[0],POLLIN|POLLHUP,0};poll(&pfd,1,100);
        for(;;){char tmp[4096];ssize_t n=read(outpipe[0],tmp,sizeof(tmp));if(n>0){if(len+(size_t)n+1>SSH_OUTPUT_LIMIT){kill(pid,SIGKILL);if(err&&errcap)snprintf(err,errcap,"SSH output exceeded safety limit");free(buf);close(outpipe[0]);waitpid(pid,NULL,0);return-1;}if(len+(size_t)n+1>cap){size_t nc=cap*2;while(nc<len+(size_t)n+1)nc*=2;char *nb=realloc(buf,nc);if(!nb){kill(pid,SIGKILL);free(buf);close(outpipe[0]);waitpid(pid,NULL,0);return-1;}buf=nb;cap=nc;}memcpy(buf+len,tmp,(size_t)n);len+=(size_t)n;buf[len]='\0';continue;}if(n==0)eof=1;break;}
        if(!done){pid_t w=waitpid(pid,&status,WNOHANG);if(w==pid)done=1;}
        if((int)(now_ms()-start)>timeout_ms){kill(pid,SIGKILL);waitpid(pid,&status,0);done=1;timed_out=1;}
        if(done&&eof)break;
    }
    close(outpipe[0]);
    if(!WIFEXITED(status)||WEXITSTATUS(status)!=0){if(err&&errcap){if(timed_out)snprintf(err,errcap,"Connection timed out");else if(strstr(buf,"__ERR_PROC__"))snprintf(err,errcap,"Remote Linux /proc unavailable");else if(strstr(buf,"Permission denied"))snprintf(err,errcap,"SSH authentication required");else if(strstr(buf,"Could not resolve hostname"))snprintf(err,errcap,"Remote host unreachable");else if(strstr(buf,"Connection timed out"))snprintf(err,errcap,"Connection timed out");else if(strstr(buf,"Connection refused"))snprintf(err,errcap,"SSH connection refused");else if(WIFEXITED(status)&&WEXITSTATUS(status)==127)snprintf(err,errcap,"SSH client is not installed");else snprintf(err,errcap,"SSH failed: %.180s",buf);}free(buf);return-1;}
    *out=buf;return 0;
}

int ssh_available(void){return access("/usr/bin/ssh",X_OK)==0||access("/bin/ssh",X_OK)==0;}

static int parse_remote_stat_line(const char *statline,ProcessInfo *p,long page_size,long hz,double uptime){
    char *copy=strdup(statline);if(!copy)return-1;char *lp=strchr(copy,'('),*rp=strrchr(copy,')');if(!lp||!rp||rp<=lp){free(copy);return-1;}p->pid=(pid_t)strtol(copy,NULL,10);size_t nl=(size_t)(rp-lp-1);if(nl>=sizeof(p->name))nl=sizeof(p->name)-1;memcpy(p->name,lp+1,nl);p->name[nl]='\0';char *tail=rp+2,*save=NULL;int idx=0;uint64_t u=0,s=0,st=0;long long rss=0;for(char *tok=strtok_r(tail," ",&save);tok;tok=strtok_r(NULL," ",&save),idx++){if(idx==0)p->state=tok[0];else if(idx==1)p->ppid=(pid_t)strtol(tok,NULL,10);else if(idx==11)u=strtoull(tok,NULL,10);else if(idx==12)s=strtoull(tok,NULL,10);else if(idx==19)st=strtoull(tok,NULL,10);else if(idx==21){rss=strtoll(tok,NULL,10);break;}}p->cpu_ticks=u+s;p->start_ticks=st;p->rss_kb=rss>0?(uint64_t)rss*(uint64_t)(page_size/1024):0;p->ram_mb=(double)p->rss_kb/1024.0;time_t now=time(NULL),boot=now-(time_t)uptime,ts=boot+(time_t)(hz>0?st/(uint64_t)hz:st/100);struct tm tv;localtime_r(&ts,&tv);strftime(p->start_time,sizeof(p->start_time),"%Y-%m-%d %H:%M:%S",&tv);free(copy);return st?0:-1;
}

static const ProcessInfo *find_old(const MonitorSnapshot *s,pid_t pid,uint64_t start){if(!s)return NULL;for(size_t i=0;i<s->process_count;i++)if(s->processes[i].pid==pid&&s->processes[i].start_ticks==start)return&s->processes[i];return NULL;}

int remote_collect_snapshot(RemoteCollector *rc,MonitorSnapshot *out,char *err,size_t errcap){
    if(!rc||!out) return -1;
    pthread_mutex_lock(&rc->lock);
    char *raw=NULL;if(run_ssh_script(&rc->machine,rc->identity_file,REMOTE_SNAPSHOT_SCRIPT,&raw,err,errcap,12000)!=0){pthread_mutex_unlock(&rc->lock);return-1;}
    MonitorSnapshot *s=out;memset(s,0,sizeof(*s));s->source_type=SOURCE_REMOTE;snprintf(s->machine_id,sizeof(s->machine_id),"%s",rc->machine.id);snprintf(s->machine_name,sizeof(s->machine_name),"%s",rc->machine.display_name);snprintf(s->source_status,sizeof(s->source_status),"REMOTE — LIVE");
    long hz=100,page_size=4096;char *save=NULL;char *lines=raw;uint64_t rxsum=0,txsum=0;
    /* First pass parses system values and process rows-> */
    for(char *line=strtok_r(lines,"\n",&save);line;line=strtok_r(NULL,"\n",&save)){
        if(strncmp(line,"HOST|",5)==0)snprintf(s->system.hostname,sizeof(s->system.hostname),"%s",line+5);
        else if(strncmp(line,"KERNEL|",7)==0)snprintf(s->system.kernel,sizeof(s->system.kernel),"%s",line+7);
        else if(strncmp(line,"DISTRO|",7)==0)snprintf(s->system.distro,sizeof(s->system.distro),"%s",line+7);
        else if(strncmp(line,"MODEL|",6)==0)snprintf(s->system.cpu_model,sizeof(s->system.cpu_model),"%s",line+6);
        else if(strncmp(line,"CORES|",6)==0)s->system.cpu_cores=atoi(line+6);
        else if(strncmp(line,"HZ|",3)==0)hz=strtol(line+3,NULL,10);
        else if(strncmp(line,"PAGESIZE|",9)==0)page_size=strtol(line+9,NULL,10);
        else if(strncmp(line,"CPU|",4)==0){char *p=line+4,*q=strchr(p,'|');if(q){*q='\0';s->system.cpu_total_ticks=strtoull(p,NULL,10);s->system.cpu_idle_ticks=strtoull(q+1,NULL,10);}}
        else if(strncmp(line,"UP|",3)==0)s->system.uptime_seconds=strtod(line+3,NULL);
        else if(strncmp(line,"LOAD|",5)==0)sscanf(line+5,"%lf|%lf|%lf",&s->system.load1,&s->system.load5,&s->system.load15);
        else if(strncmp(line,"MEM|",4)==0){unsigned long long mt=0,ma=0,st=0,sf=0;sscanf(line+4,"%llu|%llu|%llu|%llu",&mt,&ma,&st,&sf);s->system.mem_total_kb=mt;s->system.mem_available_kb=ma;s->system.mem_used_kb=mt>=ma?mt-ma:0;s->system.mem_percent=mt?100.0*(double)s->system.mem_used_kb/(double)mt:0;s->system.swap_total_kb=st;s->system.swap_free_kb=sf;s->system.swap_used_kb=st>=sf?st-sf:0;s->system.swap_percent=st?100.0*(double)s->system.swap_used_kb/(double)st:0;}
        else if(strncmp(line,"DISK|",5)==0){unsigned long long t=0,u=0,f=0;sscanf(line+5,"%llu|%llu|%llu",&t,&u,&f);s->system.disk_total_bytes=t;s->system.disk_used_bytes=u;s->system.disk_free_bytes=f;s->system.disk_percent=t?100.0*(double)u/(double)t:0;}
        else if(strncmp(line,"NET|",4)==0&&s->system.interface_count<MAX_INTERFACES){char *p=line+4;char *parts[5]={0};for(int i=0;i<5;i++){parts[i]=p;char *q=strchr(p,'|');if(i<4){if(!q)break;*q='\0';p=q+1;}}NetInterfaceInfo *ni=&s->system.interfaces[s->system.interface_count++];snprintf(ni->name,sizeof(ni->name),"%s",parts[0]?parts[0]:"");snprintf(ni->operstate,sizeof(ni->operstate),"%s",parts[1]?parts[1]:"");snprintf(ni->ipv4,sizeof(ni->ipv4),"%s",parts[2]?parts[2]:"");ni->rx_bytes=parts[3]?strtoull(parts[3],NULL,10):0;ni->tx_bytes=parts[4]?strtoull(parts[4],NULL,10):0;rxsum+=ni->rx_bytes;txsum+=ni->tx_bytes;}
        else if(strncmp(line,"PROC|",5)==0&&s->process_count<MAX_PROCESSES){
            char *p=line+5;char *a=strchr(p,'|');if(!a)continue;*a='\0';pid_t pid=(pid_t)strtol(p,NULL,10);p=a+1;a=strchr(p,'|');if(!a)continue;*a='\0';uid_t uid=(uid_t)strtoul(p,NULL,10);p=a+1;a=strchr(p,'|');if(!a)continue;*a='\0';long th=strtol(p,NULL,10);char *stat=a+1;char *cmd=strrchr(stat,'|');if(!cmd)continue;*cmd++='\0';ProcessInfo pi;memset(&pi,0,sizeof(pi));if(parse_remote_stat_line(stat,&pi,page_size,hz,s->system.uptime_seconds)!=0)continue;pi.pid=pid;pi.uid=uid;pi.threads=th;snprintf(pi.user,sizeof(pi.user),"%u",(unsigned)uid);snprintf(pi.cmdline,sizeof(pi.cmdline),"%s",*cmd?cmd:pi.name);pi.ram_percent=s->system.mem_total_kb?100.0*(double)pi.rss_kb/(double)s->system.mem_total_kb:0;s->processes[s->process_count++]=pi;
        }
    }
    free(raw);
    s->system.net_rx_bytes=rxsum;s->system.net_tx_bytes=txsum;s->system.process_total=(int)s->process_count;s->system.process_running=0;s->system.process_sleeping=0;s->system.thread_total=0;
    uint64_t dticks=0;double sec=1.0;if(rc->have_previous&&s->system.cpu_total_ticks>rc->previous.system.cpu_total_ticks){dticks=s->system.cpu_total_ticks-rc->previous.system.cpu_total_ticks;if(rc->previous.timestamp_ms){uint64_t now=now_ms();sec=(double)(now-rc->previous.timestamp_ms)/1000.0;if(sec<=0)sec=1.0;}uint64_t idle=s->system.cpu_idle_ticks>=rc->previous.system.cpu_idle_ticks?s->system.cpu_idle_ticks-rc->previous.system.cpu_idle_ticks:0;s->system.cpu_percent=dticks?100.0*(double)(dticks-idle)/(double)dticks:0;s->system.net_rx_rate_bps=s->system.net_rx_bytes>=rc->previous.system.net_rx_bytes?(double)(s->system.net_rx_bytes-rc->previous.system.net_rx_bytes)/sec:0;s->system.net_tx_rate_bps=s->system.net_tx_bytes>=rc->previous.system.net_tx_bytes?(double)(s->system.net_tx_bytes-rc->previous.system.net_tx_bytes)/sec:0;}
    for(size_t i=0;i<s->process_count;i++){ProcessInfo *p=&s->processes[i];if(p->state=='R')s->system.process_running++;if(p->state=='S'||p->state=='I'||p->state=='D')s->system.process_sleeping++;s->system.thread_total+=p->threads;const ProcessInfo *old=rc->have_previous?find_old(&rc->previous,p->pid,p->start_ticks):NULL;if(old&&dticks){uint64_t dp=p->cpu_ticks>=old->cpu_ticks?p->cpu_ticks-old->cpu_ticks:0;p->cpu_percent=100.0*(double)dp*(double)(s->system.cpu_cores>0?s->system.cpu_cores:1)/(double)dticks;}}
    s->timestamp_ms=now_ms();s->data_age_ms=0;rc->previous=*s;rc->have_previous=1;pthread_mutex_unlock(&rc->lock);return 0;
}

int remote_test_connection(const MachineConfig *m,const char *identity,char **json_out){
    if(!json_out) return -1;
    *json_out=NULL;
    const char *script="printf 'HOST|'; hostname 2>/dev/null || printf unknown; printf '\\nKERNEL|'; uname -r 2>/dev/null || printf unknown; printf '\\nDISTRO|'; awk -F= '/^PRETTY_NAME=/{v=$2;gsub(/^\"|\"$/,\"\",v);print v;exit}' /etc/os-release 2>/dev/null; printf '\\nPROC|'; test -r /proc/stat && printf yes || printf no; printf '\\n'\n";char *raw=NULL,err[256]="";if(run_ssh_script(m,identity,script,&raw,err,sizeof(err),8000)!=0){char *b=NULL;size_t l=0,c=0;appendf(&b,&l,&c,"{\"connected\":false,\"error\":");json_escape_append(&b,&l,&c,err);appendf(&b,&l,&c,"}");*json_out=b;return-1;}char host[256]="",kernel[128]="",distro[256]="",proc[16]="";char *save=NULL;for(char *line=strtok_r(raw,"\n",&save);line;line=strtok_r(NULL,"\n",&save)){if(strncmp(line,"HOST|",5)==0)snprintf(host,sizeof(host),"%s",line+5);else if(strncmp(line,"KERNEL|",7)==0)snprintf(kernel,sizeof(kernel),"%s",line+7);else if(strncmp(line,"DISTRO|",7)==0)snprintf(distro,sizeof(distro),"%s",line+7);else if(strncmp(line,"PROC|",5)==0)snprintf(proc,sizeof(proc),"%s",line+5);}free(raw);char *b=NULL;size_t l=0,c=0;appendf(&b,&l,&c,"{\"connected\":%s,\"hostname\":",strcmp(proc,"yes")==0?"true":"false");json_escape_append(&b,&l,&c,host);appendf(&b,&l,&c,",\"kernel\":");json_escape_append(&b,&l,&c,kernel);appendf(&b,&l,&c,",\"distribution\":");json_escape_append(&b,&l,&c,distro);appendf(&b,&l,&c,",\"proc_available\":%s}",strcmp(proc,"yes")==0?"true":"false");*json_out=b;return strcmp(proc,"yes")==0?0:-1;
}

static char *build_pid_script(pid_t pid,int maps_only){
    char *b=NULL;size_t l=0,c=0;
    if(maps_only){appendf(&b,&l,&c,"head -c 262144 /proc/%d/maps 2>/dev/null || { printf '__ERROR__'; exit 3; }\n",pid);return b;}
    appendf(&b,&l,&c,"p=/proc/%d\n[ -d \"$p\" ] || { echo 'ERROR|Process unavailable'; exit 3; }\nprintf 'META|'; getconf CLK_TCK 2>/dev/null || printf 100; printf '|'; getconf PAGESIZE 2>/dev/null || printf 4096; printf '|'; awk '{print $1}' /proc/uptime 2>/dev/null || printf 0; printf '\\n'\nprintf 'STAT|'; cat \"$p/stat\" 2>/dev/null | tr '|' '/'; printf '\\n'\nprintf 'STATUS_BEGIN\\n'; cat \"$p/status\" 2>/dev/null; printf 'STATUS_END\\n'\nprintf 'CMD|'; tr '\\000' ' ' < \"$p/cmdline\" 2>/dev/null | tr '|\\r\\n' '   '; printf '\\n'\nprintf 'EXE|'; readlink \"$p/exe\" 2>/dev/null | tr '|\\r\\n' '   ' || printf 'Permission unavailable'; printf '\\n'\nprintf 'CWD|'; readlink \"$p/cwd\" 2>/dev/null | tr '|\\r\\n' '   ' || printf 'Permission unavailable'; printf '\\n'\nprintf 'FD|'; ls -1 \"$p/fd\" 2>/dev/null | wc -l || printf 0; printf '\\n'\nprintf 'IO_BEGIN\\n'; cat \"$p/io\" 2>/dev/null || printf 'Permission unavailable\\n'; printf 'IO_END\\n'\n");return b;
}

int remote_inspect_process_json(const MachineConfig *m,const char *identity,pid_t pid,const MonitorSnapshot *snap,char **json_out){
    if(!m||pid<=0||!json_out) return -1;
    char *script=build_pid_script(pid,0),*raw=NULL,err[256]="";int rc=run_ssh_script(m,identity,script,&raw,err,sizeof(err),8000);free(script);if(rc!=0){char *b=NULL;size_t l=0,c=0;appendf(&b,&l,&c,"{\"pid\":%d,\"error\":",pid);json_escape_append(&b,&l,&c,err);appendf(&b,&l,&c,"}");*json_out=b;return-1;}
    char statline[4096]="",cmd[MAX_CMDLINE]="",exe[MAX_PATH_TEXT]="Permission unavailable",cwd[MAX_PATH_TEXT]="Permission unavailable";long hz=100,page_size=4096;double uptime=0;long fdcount=0;uint64_t vms=0,vmrss=0,ra=0,rf=0,vd=0,vs=0,ve=0,vl=0,vw=0;unsigned uid=0;long threads=0;unsigned long long rb=0,wb=0,rchar=0,wchar=0;int io_perm=1,in_status=0,in_io=0;char *save=NULL;for(char *line=strtok_r(raw,"\n",&save);line;line=strtok_r(NULL,"\n",&save)){if(strcmp(line,"STATUS_BEGIN")==0){in_status=1;continue;}if(strcmp(line,"STATUS_END")==0){in_status=0;continue;}if(strcmp(line,"IO_BEGIN")==0){in_io=1;continue;}if(strcmp(line,"IO_END")==0){in_io=0;continue;}if(in_status){unsigned long long v=0;if(sscanf(line,"Uid: %u",&uid)==1){}else if(sscanf(line,"Threads: %ld",&threads)==1){}else if(sscanf(line,"VmSize: %llu kB",&v)==1)vms=v;else if(sscanf(line,"VmRSS: %llu kB",&v)==1)vmrss=v;else if(sscanf(line,"RssAnon: %llu kB",&v)==1)ra=v;else if(sscanf(line,"RssFile: %llu kB",&v)==1)rf=v;else if(sscanf(line,"VmData: %llu kB",&v)==1)vd=v;else if(sscanf(line,"VmStk: %llu kB",&v)==1)vs=v;else if(sscanf(line,"VmExe: %llu kB",&v)==1)ve=v;else if(sscanf(line,"VmLib: %llu kB",&v)==1)vl=v;else if(sscanf(line,"VmSwap: %llu kB",&v)==1)vw=v;continue;}if(in_io){unsigned long long v=0;if(strstr(line,"Permission unavailable"))io_perm=0;else if(sscanf(line,"read_bytes: %llu",&v)==1)rb=v;else if(sscanf(line,"write_bytes: %llu",&v)==1)wb=v;else if(sscanf(line,"rchar: %llu",&v)==1)rchar=v;else if(sscanf(line,"wchar: %llu",&v)==1)wchar=v;continue;}if(strncmp(line,"META|",5)==0)sscanf(line+5,"%ld|%ld|%lf",&hz,&page_size,&uptime);else if(strncmp(line,"STAT|",5)==0)snprintf(statline,sizeof(statline),"%s",line+5);else if(strncmp(line,"CMD|",4)==0)snprintf(cmd,sizeof(cmd),"%s",line+4);else if(strncmp(line,"EXE|",4)==0)snprintf(exe,sizeof(exe),"%s",line+4);else if(strncmp(line,"CWD|",4)==0)snprintf(cwd,sizeof(cwd),"%s",line+4);else if(strncmp(line,"FD|",3)==0)fdcount=strtol(line+3,NULL,10);}
    ProcessInfo p;memset(&p,0,sizeof(p));if(parse_remote_stat_line(statline,&p,page_size,hz,uptime)!=0){free(raw);char *eb=NULL;size_t el=0,ec=0;appendf(&eb,&el,&ec,"{\"pid\":%d,\"error\":\"Process data unavailable\"}",pid);*json_out=eb;return-1;}p.uid=uid;p.threads=threads;snprintf(p.user,sizeof(p.user),"%u",uid);snprintf(p.cmdline,sizeof(p.cmdline),"%s",cmd);const ProcessInfo *live=snap?monitor_find_process(snap,pid):NULL;if(live&&live->start_ticks==p.start_ticks){p.cpu_percent=live->cpu_percent;p.ram_mb=live->ram_mb;p.ram_percent=live->ram_percent;snprintf(p.start_time,sizeof(p.start_time),"%s",live->start_time);}free(raw);char *b=NULL;size_t l=0,c=0;appendf(&b,&l,&c,"{\"pid\":%d,\"ppid\":%d,\"name\":",p.pid,p.ppid);json_escape_append(&b,&l,&c,p.name);appendf(&b,&l,&c,",\"state\":\"%c\",\"uid\":%u,\"user\":",p.state?p.state:'?',uid);json_escape_append(&b,&l,&c,p.user);appendf(&b,&l,&c,",\"cpu_percent\":%.3f,\"ram_mb\":%.3f,\"ram_percent\":%.3f,\"threads\":%ld,\"start_time\":",p.cpu_percent,p.ram_mb,p.ram_percent,threads);json_escape_append(&b,&l,&c,p.start_time);appendf(&b,&l,&c,",\"cmdline\":");json_escape_append(&b,&l,&c,cmd);appendf(&b,&l,&c,",\"executable\":");json_escape_append(&b,&l,&c,exe);appendf(&b,&l,&c,",\"cwd\":");json_escape_append(&b,&l,&c,cwd);appendf(&b,&l,&c,",\"memory\":{\"VmSize_kB\":%llu,\"VmRSS_kB\":%llu,\"RssAnon_kB\":%llu,\"RssFile_kB\":%llu,\"VmData_kB\":%llu,\"VmStk_kB\":%llu,\"VmExe_kB\":%llu,\"VmLib_kB\":%llu,\"VmSwap_kB\":%llu},",(unsigned long long)vms,(unsigned long long)vmrss,(unsigned long long)ra,(unsigned long long)rf,(unsigned long long)vd,(unsigned long long)vs,(unsigned long long)ve,(unsigned long long)vl,(unsigned long long)vw);if(io_perm)appendf(&b,&l,&c,"\"io\":{\"read_bytes\":%llu,\"write_bytes\":%llu,\"rchar\":%llu,\"wchar\":%llu},",rb,wb,rchar,wchar);else appendf(&b,&l,&c,"\"io\":{\"status\":\"Permission unavailable\"},");appendf(&b,&l,&c,"\"open_fd_count\":%ld,\"maps_endpoint\":\"/api/process/%d/maps\"}",fdcount,pid);*json_out=b;return 0;
}

int remote_process_maps_json(const MachineConfig *m,const char *identity,pid_t pid,char **json_out){if(!m||pid<=0||!json_out)return-1;char *script=build_pid_script(pid,1),*raw=NULL,err[256]="";int rc=run_ssh_script(m,identity,script,&raw,err,sizeof(err),8000);free(script);char *b=NULL;size_t l=0,c=0;appendf(&b,&l,&c,"{\"pid\":%d,",pid);if(rc!=0){appendf(&b,&l,&c,"\"status\":");json_escape_append(&b,&l,&c,err);appendf(&b,&l,&c,"}");*json_out=b;return-1;}appendf(&b,&l,&c,"\"maps\":");json_escape_append(&b,&l,&c,raw);appendf(&b,&l,&c,"}");free(raw);*json_out=b;return 0;}


int remote_open_applications_json(const MachineConfig *m, const char *identity, const MonitorSnapshot *snap, char **json_out) {
    if (!m || !snap || !json_out) return -1;
    static const char *script =
        "set -u\n"
        "[ -d /proc ] || { printf '__ERR_PROC__\\n'; exit 20; }\n"
        "for d in /proc/[0-9]*; do [ -r \"$d/environ\" ] || continue; "
        "tr '\\000' '\\n' < \"$d/environ\" 2>/dev/null | grep -a -Eq '^(DISPLAY|WAYLAND_DISPLAY)=' || continue; "
        "grep -a -Eq 'libX11\\.so|libxcb\\.so|libwayland-client\\.so|libgtk-[34]\\.so|libQt[56]Gui\\.so|libgdk-[34]\\.so' \"$d/maps\" 2>/dev/null || continue; "
        "pid=${d##*/}; name=$(cat \"$d/comm\" 2>/dev/null | tr '|\\r\\n' '   '); "
        "[ -n \"$name\" ] || continue; printf 'APP|%s|%s\\n' \"$pid\" \"$name\"; done\n";
    char *raw = NULL, err[256] = "";
    int rc = run_ssh_script(m, identity, script, &raw, err, sizeof(err), 8000);
    if (rc != 0) {
        char *b=NULL; size_t l=0,c=0;
        appendf(&b,&l,&c,"{\"available\":false,\"source\":\"remote\",\"platform\":\"remote-linux\",\"error\":");
        json_escape_append(&b,&l,&c,err); appendf(&b,&l,&c,",\"applications\":[]}");
        *json_out=b; return -1;
    }
    typedef struct { pid_t pid; char name[128]; char friendly[160]; char kind[24]; double cpu; double ram; int n; } RApp;
    RApp apps[256]; size_t count=0; memset(apps,0,sizeof(apps));
    char *save=NULL;
    for(char *line=strtok_r(raw,"\n",&save);line&&count<256;line=strtok_r(NULL,"\n",&save)){
        trim_newline(line); if(strncmp(line,"APP|",4)!=0) continue;
        char *p=line+4,*bar=strchr(p,'|'); if(!bar)continue; *bar='\0';
        pid_t pid=(pid_t)strtol(p,NULL,10); const char *name=bar+1; if(pid<=0||application_name_is_noise(name))continue;
        char friendly[160]; application_friendly_name(name,friendly,sizeof(friendly));
        int idx=-1; for(size_t i=0;i<count;i++)if(strcmp(apps[i].friendly,friendly)==0){idx=(int)i;break;}
        if(idx<0){idx=(int)count++;apps[idx].pid=pid;snprintf(apps[idx].name,sizeof(apps[idx].name),"%s",name);snprintf(apps[idx].friendly,sizeof(apps[idx].friendly),"%s",friendly);snprintf(apps[idx].kind,sizeof(apps[idx].kind),"%s",application_kind_for_name(name));}
        apps[idx].n++;
        const ProcessInfo *pi=monitor_find_process(snap,pid); if(pi){apps[idx].cpu+=pi->cpu_percent;apps[idx].ram+=pi->ram_mb;}
        if(pid<apps[idx].pid)apps[idx].pid=pid;
    }
    free(raw);
    char *b=NULL;size_t l=0,c=0;
    appendf(&b,&l,&c,"{\"available\":true,\"source\":\"remote\",\"platform\":\"remote-linux\",\"detection_mode\":\"remote-proc-gui-session\",\"note\":");
    json_escape_append(&b,&l,&c,"Remote GUI applications are inferred from authorized SSH reads of /proc/<PID>/environ for DISPLAY/WAYLAND_DISPLAY and correlated with the remote process snapshot. Exact desktop window titles are not exposed by /proc.");
    appendf(&b,&l,&c,",\"applications\":[");
    for(size_t i=0;i<count;i++){
        if(i)appendf(&b,&l,&c,",");
        appendf(&b,&l,&c,"{\"pid\":%d,\"name\":",apps[i].pid);json_escape_append(&b,&l,&c,apps[i].name);
        appendf(&b,&l,&c,",\"application\":");json_escape_append(&b,&l,&c,apps[i].friendly);
        appendf(&b,&l,&c,",\"kind\":");json_escape_append(&b,&l,&c,apps[i].kind);
        appendf(&b,&l,&c,",\"title\":\"\",\"cpu_percent\":%.3f,\"ram_mb\":%.3f,\"process_count\":%d}",apps[i].cpu,apps[i].ram,apps[i].n);
    }
    appendf(&b,&l,&c,"]}"); *json_out=b; return 0;
}
