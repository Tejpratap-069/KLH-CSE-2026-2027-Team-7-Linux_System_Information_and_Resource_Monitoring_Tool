#define _GNU_SOURCE
#include "monitor.h"
#include "utils.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static const ProcessInfo *find_prev(const ProcessInfo *prev, size_t prev_count, pid_t pid, uint64_t start_ticks) {
    for (size_t i=0;i<prev_count;i++) if (prev[i].pid==pid && prev[i].start_ticks==start_ticks) return &prev[i];
    return NULL;
}

static int parse_proc_stat(pid_t pid, ProcessInfo *p) {
    char path[128], buf[4096]; size_t n=0;
    snprintf(path,sizeof(path),"/proc/%d/stat",pid);
    if (read_text_file(path,buf,sizeof(buf),&n)!=0) return -1;
    char *lparen=strchr(buf,'('), *rparen=strrchr(buf,')');
    if (!lparen || !rparen || rparen<=lparen) return -1;
    size_t namelen=(size_t)(rparen-lparen-1);
    if (namelen>=sizeof(p->name)) namelen=sizeof(p->name)-1;
    memcpy(p->name,lparen+1,namelen); p->name[namelen]='\0';
    char *tail=rparen+2;
    char *copy=strdup(tail); if(!copy) return -1;
    char *save=NULL; int idx=0;
    uint64_t utime=0,stime=0,start=0; long long rss_pages=0;
    for(char *tok=strtok_r(copy," ", &save);tok;tok=strtok_r(NULL," ",&save),idx++) {
        if(idx==0) p->state=tok[0];
        else if(idx==1) p->ppid=(pid_t)strtol(tok,NULL,10);
        else if(idx==11) utime=strtoull(tok,NULL,10);
        else if(idx==12) stime=strtoull(tok,NULL,10);
        else if(idx==19) start=strtoull(tok,NULL,10);
        else if(idx==21) { rss_pages=strtoll(tok,NULL,10); break; }
    }
    free(copy);
    p->cpu_ticks=utime+stime;
    p->start_ticks=start;
    long page_kb=sysconf(_SC_PAGESIZE)/1024; if(page_kb<=0) page_kb=4;
    p->rss_kb=rss_pages>0?(uint64_t)rss_pages*(uint64_t)page_kb:0;
    p->ram_mb=(double)p->rss_kb/1024.0;
    return start?0:-1;
}

static void parse_status(pid_t pid, ProcessInfo *p) {
    char path[128]; snprintf(path,sizeof(path),"/proc/%d/status",pid);
    char *buf=NULL; size_t len=0;
    if(read_text_file_alloc(path,262144,&buf,&len)!=0) return;
    char *save=NULL;
    for(char *line=strtok_r(buf,"\n",&save);line;line=strtok_r(NULL,"\n",&save)) {
        unsigned long v;
        if(sscanf(line,"Uid:\t%lu",&v)==1) p->uid=(uid_t)v;
        else if(sscanf(line,"Threads:\t%ld",&p->threads)==1) {}
    }
    free(buf);
    struct passwd pwd,*res=NULL; char pwbuf[16384];
    if(getpwuid_r(p->uid,&pwd,pwbuf,sizeof(pwbuf),&res)==0 && res) snprintf(p->user,sizeof(p->user),"%s",pwd.pw_name);
    else snprintf(p->user,sizeof(p->user),"%u",(unsigned)p->uid);
}

static void read_cmdline(pid_t pid, ProcessInfo *p) {
    char path[128]; snprintf(path,sizeof(path),"/proc/%d/cmdline",pid);
    int fd=open(path,O_RDONLY|O_CLOEXEC);
    if(fd<0){ snprintf(p->cmdline,sizeof(p->cmdline),"[%s]",p->name); return; }
    ssize_t n=read(fd,p->cmdline,sizeof(p->cmdline)-1); close(fd);
    if(n<=0){ snprintf(p->cmdline,sizeof(p->cmdline),"[%s]",p->name); return; }
    p->cmdline[n]='\0';
    for(ssize_t i=0;i<n-1;i++) if(p->cmdline[i]=='\0') p->cmdline[i]=' ';
}

static void format_start_time(ProcessInfo *p, double uptime) {
    long hz=sysconf(_SC_CLK_TCK); if(hz<=0) hz=100;
    time_t now=time(NULL);
    time_t boot=now-(time_t)uptime;
    time_t st=boot+(time_t)(p->start_ticks/(uint64_t)hz);
    struct tm tmv; localtime_r(&st,&tmv);
    strftime(p->start_time,sizeof(p->start_time),"%Y-%m-%d %H:%M:%S",&tmv);
}

int collect_processes(ProcessInfo *out, size_t max_count, size_t *count,
                      uint64_t mem_total_kb, uint64_t cpu_total_ticks, uint64_t prev_cpu_total_ticks,
                      const ProcessInfo *prev, size_t prev_count, int cpu_cores,
                      int *running, int *sleeping, long *thread_total,
                      char *err, size_t errcap) {
    (void)err; (void)errcap;
    if(!out||!count) return -1;
    DIR *d=opendir("/proc"); if(!d) return -1;
    size_t nproc=0; int run=0,sleep=0; long threads=0;
    double uptime=0.0; char ubuf[128]; size_t un=0;
    if(read_text_file("/proc/uptime",ubuf,sizeof(ubuf),&un)==0) sscanf(ubuf,"%lf",&uptime);
    uint64_t total_delta=(prev_cpu_total_ticks>0 && cpu_total_ticks>prev_cpu_total_ticks)
                         ? cpu_total_ticks-prev_cpu_total_ticks : 0;
    struct dirent *de;
    while((de=readdir(d))!=NULL && nproc<max_count) {
        if(!is_digits(de->d_name)) continue;
        long lp=strtol(de->d_name,NULL,10); if(lp<=0||lp>2147483647L) continue;
        ProcessInfo p; memset(&p,0,sizeof(p)); p.pid=(pid_t)lp; p.threads=0;
        if(parse_proc_stat(p.pid,&p)!=0) continue; /* PID may disappear here */
        parse_status(p.pid,&p);
        read_cmdline(p.pid,&p);
        format_start_time(&p,uptime);
        p.ram_percent=mem_total_kb?100.0*(double)p.rss_kb/(double)mem_total_kb:0.0;
        const ProcessInfo *old=find_prev(prev,prev_count,p.pid,p.start_ticks);
        if(old && total_delta>0) {
            uint64_t dp=p.cpu_ticks>=old->cpu_ticks?p.cpu_ticks-old->cpu_ticks:0;
            p.cpu_percent=100.0*(double)dp*(double)(cpu_cores>0?cpu_cores:1)/(double)total_delta;
            if(p.cpu_percent<0) p.cpu_percent=0;
        }
        if(p.state=='R') run++;
        if(p.state=='S'||p.state=='I'||p.state=='D') sleep++;
        threads+=p.threads;
        out[nproc++]=p;
    }
    closedir(d);
    *count=nproc; if(running)*running=run; if(sleeping)*sleeping=sleep; if(thread_total)*thread_total=threads;
    return 0;
}

static void json_key_string(char **b,size_t *l,size_t *c,const char *key,const char *val,int comma){
    appendf(b,l,c,"\"%s\":",key); json_escape_append(b,l,c,val?val:""); if(comma) appendf(b,l,c,",");
}

static void parse_status_detail(pid_t pid, uint64_t *vmsize,uint64_t *vmrss,uint64_t *rssanon,uint64_t *rssfile,
                                uint64_t *vmdata,uint64_t *vmstk,uint64_t *vmexe,uint64_t *vmlib,uint64_t *vmswap) {
    char path[128]; snprintf(path,sizeof(path),"/proc/%d/status",pid);
    char *buf=NULL; size_t len=0; if(read_text_file_alloc(path,262144,&buf,&len)!=0) return;
    char *save=NULL; unsigned long long v=0;
    for(char *line=strtok_r(buf,"\n",&save);line;line=strtok_r(NULL,"\n",&save)) {
        if(sscanf(line,"VmSize: %llu kB",&v)==1)*vmsize=v;
        else if(sscanf(line,"VmRSS: %llu kB",&v)==1)*vmrss=v;
        else if(sscanf(line,"RssAnon: %llu kB",&v)==1)*rssanon=v;
        else if(sscanf(line,"RssFile: %llu kB",&v)==1)*rssfile=v;
        else if(sscanf(line,"VmData: %llu kB",&v)==1)*vmdata=v;
        else if(sscanf(line,"VmStk: %llu kB",&v)==1)*vmstk=v;
        else if(sscanf(line,"VmExe: %llu kB",&v)==1)*vmexe=v;
        else if(sscanf(line,"VmLib: %llu kB",&v)==1)*vmlib=v;
        else if(sscanf(line,"VmSwap: %llu kB",&v)==1)*vmswap=v;
    }
    free(buf);
}

static void parse_io(pid_t pid, unsigned long long *read_bytes,unsigned long long *write_bytes,unsigned long long *rchar,unsigned long long *wchar,int *perm) {
    char path[128]; snprintf(path,sizeof(path),"/proc/%d/io",pid);
    char *buf=NULL; size_t len=0; if(read_text_file_alloc(path,131072,&buf,&len)!=0){ if(errno==EACCES)*perm=0; return; }
    *perm=1; char *save=NULL; unsigned long long v=0;
    for(char *line=strtok_r(buf,"\n",&save);line;line=strtok_r(NULL,"\n",&save)) {
        if(sscanf(line,"read_bytes: %llu",&v)==1)*read_bytes=v;
        else if(sscanf(line,"write_bytes: %llu",&v)==1)*write_bytes=v;
        else if(sscanf(line,"rchar: %llu",&v)==1)*rchar=v;
        else if(sscanf(line,"wchar: %llu",&v)==1)*wchar=v;
    }
    free(buf);
}

int inspect_process_json(pid_t pid, const MonitorSnapshot *snap, char **json_out) {
    if(!json_out) return -1;
    *json_out=NULL;
    ProcessInfo p; memset(&p,0,sizeof(p));
    const ProcessInfo *found=snap?monitor_find_process(snap,pid):NULL;
    if(found){
        ProcessInfo current;memset(&current,0,sizeof(current));current.pid=pid;
        if(parse_proc_stat(pid,&current)!=0 || current.start_ticks!=found->start_ticks) return -2;
        p=*found;
    } else { p.pid=pid; if(parse_proc_stat(pid,&p)!=0) return -2; parse_status(pid,&p); read_cmdline(pid,&p); }
    char exe[MAX_PATH_TEXT]="Permission unavailable",cwd[MAX_PATH_TEXT]="Permission unavailable",path[128];
    snprintf(path,sizeof(path),"/proc/%d/exe",pid); if(path_readlink(path,exe,sizeof(exe))!=0 && errno==ENOENT) snprintf(exe,sizeof(exe),"Process exited");
    snprintf(path,sizeof(path),"/proc/%d/cwd",pid); if(path_readlink(path,cwd,sizeof(cwd))!=0 && errno==ENOENT) snprintf(cwd,sizeof(cwd),"Process exited");
    uint64_t vmsize=0,vmrss=0,rssanon=0,rssfile=0,vmdata=0,vmstk=0,vmexe=0,vmlib=0,vmswap=0;
    parse_status_detail(pid,&vmsize,&vmrss,&rssanon,&rssfile,&vmdata,&vmstk,&vmexe,&vmlib,&vmswap);
    unsigned long long rb=0,wb=0,rc=0,wc=0; int io_perm=0; parse_io(pid,&rb,&wb,&rc,&wc,&io_perm);
    int fd_count=0,fd_perm=1; snprintf(path,sizeof(path),"/proc/%d/fd",pid); DIR *d=opendir(path);
    if(!d){fd_perm=0;} else {struct dirent *de; while((de=readdir(d))) if(de->d_name[0]!='.')fd_count++; closedir(d);}
    char *b=NULL; size_t l=0,c=0; appendf(&b,&l,&c,"{");
    appendf(&b,&l,&c,"\"pid\":%d,\"ppid\":%d,",p.pid,p.ppid);
    json_key_string(&b,&l,&c,"name",p.name,1); appendf(&b,&l,&c,"\"state\":\"%c\",",p.state?p.state:'?');
    json_key_string(&b,&l,&c,"user",p.user,1); appendf(&b,&l,&c,"\"uid\":%u,\"cpu_percent\":%.3f,\"ram_mb\":%.3f,\"ram_percent\":%.3f,\"threads\":%ld,",(unsigned)p.uid,p.cpu_percent,p.ram_mb,p.ram_percent,p.threads);
    json_key_string(&b,&l,&c,"cmdline",p.cmdline,1); json_key_string(&b,&l,&c,"start_time",p.start_time,1); json_key_string(&b,&l,&c,"executable",exe,1); json_key_string(&b,&l,&c,"cwd",cwd,1);
    appendf(&b,&l,&c,"\"memory\":{\"VmSize_kB\":%llu,\"VmRSS_kB\":%llu,\"RssAnon_kB\":%llu,\"RssFile_kB\":%llu,\"VmData_kB\":%llu,\"VmStk_kB\":%llu,\"VmExe_kB\":%llu,\"VmLib_kB\":%llu,\"VmSwap_kB\":%llu},",
            (unsigned long long)vmsize,(unsigned long long)vmrss,(unsigned long long)rssanon,(unsigned long long)rssfile,(unsigned long long)vmdata,(unsigned long long)vmstk,(unsigned long long)vmexe,(unsigned long long)vmlib,(unsigned long long)vmswap);
    if(io_perm) appendf(&b,&l,&c,"\"io\":{\"read_bytes\":%llu,\"write_bytes\":%llu,\"rchar\":%llu,\"wchar\":%llu},",rb,wb,rc,wc);
    else appendf(&b,&l,&c,"\"io\":{\"status\":\"Permission unavailable\"},");
    if(fd_perm) appendf(&b,&l,&c,"\"open_fd_count\":%d,",fd_count); else appendf(&b,&l,&c,"\"open_fd_count\":null,\"fd_status\":\"Permission unavailable\",");
    appendf(&b,&l,&c,"\"maps_endpoint\":\"/api/process/%d/maps\"}",pid);
    *json_out=b; return 0;
}

int process_maps_json(pid_t pid, char **json_out) {
    if(!json_out) return -1;
    *json_out=NULL;
    char path[128]; snprintf(path,sizeof(path),"/proc/%d/maps",pid);
    char *maps=NULL; size_t len=0;
    if(read_text_file_alloc(path,256*1024,&maps,&len)!=0) {
        char *b=NULL; size_t l=0,c=0; appendf(&b,&l,&c,"{\"pid\":%d,\"status\":",pid); json_escape_append(&b,&l,&c,errno==EACCES?"Permission unavailable":"Process unavailable"); appendf(&b,&l,&c,"}"); *json_out=b; return -2;
    }
    char *b=NULL; size_t l=0,c=0; appendf(&b,&l,&c,"{\"pid\":%d,\"truncated\":%s,\"maps\":",pid,len>=256*1024?"true":"false"); json_escape_append(&b,&l,&c,maps); appendf(&b,&l,&c,"}"); free(maps); *json_out=b; return 0;
}
