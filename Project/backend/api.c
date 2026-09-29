#define _GNU_SOURCE
#include "api.h"
#include "application_monitor.h"
#include "process_demo.h"
#include "utils.h"
#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

static char *dupstr(const char *s){size_t n=strlen(s)+1;char *p=malloc(n);if(p)memcpy(p,s,n);return p;}
static ApiResponse resp(int status,const char *body){ApiResponse r;memset(&r,0,sizeof(r));r.status=status;snprintf(r.content_type,sizeof(r.content_type),"application/json; charset=utf-8");r.body=dupstr(body?body:"{}");return r;}
void api_response_free(ApiResponse *r){if(r&&r->body){free(r->body);r->body=NULL;}}

static int json_get_string(const char *json,const char *key,char *out,size_t cap){
    if(!json||!key||!out||cap<2) return -1;
    char needle[128];snprintf(needle,sizeof(needle),"\"%s\"",key);const char *p=strstr(json,needle);if(!p)return-1;p+=strlen(needle);while(isspace((unsigned char)*p))p++;if(*p!=':')return-1;p++;while(isspace((unsigned char)*p))p++;if(*p!='"')return-1;p++;size_t j=0;while(*p&&*p!='"'&&j+1<cap){if(*p=='\\'){p++;if(!*p)break;char c=*p;if(c=='n')c='\n';else if(c=='r')c='\r';else if(c=='t')c='\t';out[j++]=c;p++;}else out[j++]=*p++;}if(*p!='"')return-1;out[j]='\0';return 0;
}
static int json_get_int(const char *json,const char *key,int *out){char needle[128];snprintf(needle,sizeof(needle),"\"%s\"",key);const char *p=json?strstr(json,needle):NULL;if(!p)return-1;p+=strlen(needle);while(isspace((unsigned char)*p))p++;if(*p!=':')return-1;p++;while(isspace((unsigned char)*p))p++;char *e=NULL;long v=strtol(p,&e,10);if(e==p||v<0||v>65535)return-1;*out=(int)v;return 0;}

static int remote_cb(void *opaque,MonitorSnapshot *out,char *err,size_t errcap){return remote_collect_snapshot((RemoteCollector*)opaque,out,err,errcap);}

void api_context_init(ApiContext *ctx,MonitorContext *monitor,MachineStore *machines){memset(ctx,0,sizeof(*ctx));ctx->monitor=monitor;ctx->machines=machines;pthread_mutex_init(&ctx->remote_collector.lock,NULL);const char *id=getenv("MONITOR_SSH_IDENTITY");if(id&&*id)snprintf(ctx->identity_file,sizeof(ctx->identity_file),"%s",id);}
void api_context_destroy(ApiContext *ctx){if(ctx)pthread_mutex_destroy(&ctx->remote_collector.lock);}

static void add_string(char **b,size_t *l,size_t *c,const char *key,const char *v,int comma){appendf(b,l,c,"\"%s\":",key);json_escape_append(b,l,c,v?v:"");if(comma)appendf(b,l,c,",");}

static char *snapshot_system_json(const MonitorSnapshot *s,const char *last_error){
    char *b=NULL;size_t l=0,c=0;appendf(&b,&l,&c,"{");add_string(&b,&l,&c,"source",s->source_type==SOURCE_REMOTE?"remote":"local",1);add_string(&b,&l,&c,"machine_id",s->machine_id,1);add_string(&b,&l,&c,"machine_name",s->machine_name,1);add_string(&b,&l,&c,"status",s->source_status,1);appendf(&b,&l,&c,"\"timestamp_ms\":%llu,\"measurement_freshness_ms\":%llu,",(unsigned long long)s->timestamp_ms,(unsigned long long)s->data_age_ms);add_string(&b,&l,&c,"hostname",s->system.hostname,1);add_string(&b,&l,&c,"distribution",s->system.distro,1);add_string(&b,&l,&c,"kernel",s->system.kernel,1);add_string(&b,&l,&c,"cpu_model",s->system.cpu_model,1);
    appendf(&b,&l,&c,"\"cpu_cores\":%d,\"cpu_percent\":%.3f,\"load\":[%.3f,%.3f,%.3f],\"uptime_seconds\":%.3f,",s->system.cpu_cores,s->system.cpu_percent,s->system.load1,s->system.load5,s->system.load15,s->system.uptime_seconds);
    appendf(&b,&l,&c,"\"memory\":{\"total_kb\":%llu,\"available_kb\":%llu,\"used_kb\":%llu,\"percent\":%.3f},",(unsigned long long)s->system.mem_total_kb,(unsigned long long)s->system.mem_available_kb,(unsigned long long)s->system.mem_used_kb,s->system.mem_percent);
    appendf(&b,&l,&c,"\"swap\":{\"total_kb\":%llu,\"free_kb\":%llu,\"used_kb\":%llu,\"percent\":%.3f},",(unsigned long long)s->system.swap_total_kb,(unsigned long long)s->system.swap_free_kb,(unsigned long long)s->system.swap_used_kb,s->system.swap_percent);
    appendf(&b,&l,&c,"\"disk\":{\"total_bytes\":%llu,\"used_bytes\":%llu,\"free_bytes\":%llu,\"percent\":%.3f},",(unsigned long long)s->system.disk_total_bytes,(unsigned long long)s->system.disk_used_bytes,(unsigned long long)s->system.disk_free_bytes,s->system.disk_percent);
    appendf(&b,&l,&c,"\"processes\":{\"total\":%d,\"running\":%d,\"sleeping\":%d,\"threads\":%ld},",s->system.process_total,s->system.process_running,s->system.process_sleeping,s->system.thread_total);
    appendf(&b,&l,&c,"\"network\":{\"rx_bytes\":%llu,\"tx_bytes\":%llu,\"rx_rate_bps\":%.3f,\"tx_rate_bps\":%.3f,\"interfaces\":[",(unsigned long long)s->system.net_rx_bytes,(unsigned long long)s->system.net_tx_bytes,s->system.net_rx_rate_bps,s->system.net_tx_rate_bps);
    for(int i=0;i<s->system.interface_count;i++){const NetInterfaceInfo *n=&s->system.interfaces[i];if(i)appendf(&b,&l,&c,",");appendf(&b,&l,&c,"{");add_string(&b,&l,&c,"name",n->name,1);add_string(&b,&l,&c,"state",n->operstate,1);add_string(&b,&l,&c,"ipv4",n->ipv4,1);appendf(&b,&l,&c,"\"rx_bytes\":%llu,\"tx_bytes\":%llu}",(unsigned long long)n->rx_bytes,(unsigned long long)n->tx_bytes);}
    appendf(&b,&l,&c,"]}");if(last_error&&*last_error){appendf(&b,&l,&c,",\"last_error\":");json_escape_append(&b,&l,&c,last_error);}appendf(&b,&l,&c,"}");return b;
}

static char *processes_json(const MonitorSnapshot *s){char *b=NULL;size_t l=0,c=0;appendf(&b,&l,&c,"{\"timestamp_ms\":%llu,\"source\":\"%s\",\"machine_id\":",(unsigned long long)s->timestamp_ms,s->source_type==SOURCE_REMOTE?"remote":"local");json_escape_append(&b,&l,&c,s->machine_id);appendf(&b,&l,&c,",\"processes\":[");for(size_t i=0;i<s->process_count;i++){const ProcessInfo *p=&s->processes[i];if(i)appendf(&b,&l,&c,",");appendf(&b,&l,&c,"{\"pid\":%d,\"ppid\":%d,\"uid\":%u,\"user\":",p->pid,p->ppid,(unsigned)p->uid);json_escape_append(&b,&l,&c,p->user);appendf(&b,&l,&c,",\"state\":\"%c\",\"name\":",p->state?p->state:'?');json_escape_append(&b,&l,&c,p->name);appendf(&b,&l,&c,",\"cpu_percent\":%.3f,\"ram_mb\":%.3f,\"ram_percent\":%.3f,\"threads\":%ld,\"cmdline\":",p->cpu_percent,p->ram_mb,p->ram_percent,p->threads);json_escape_append(&b,&l,&c,p->cmdline);appendf(&b,&l,&c,",\"start_time\":");json_escape_append(&b,&l,&c,p->start_time);appendf(&b,&l,&c,"}");}appendf(&b,&l,&c,"]}");return b;}

static char *lifecycle_json(MonitorContext *m){LifecycleEvent ev[MAX_LIFECYCLE_EVENTS];size_t n=monitor_get_lifecycle(m,ev,MAX_LIFECYCLE_EVENTS);char *b=NULL;size_t l=0,c=0;appendf(&b,&l,&c,"{\"events\":[");for(size_t k=0;k<n;k++){size_t i=n-1-k;LifecycleEvent *e=&ev[i];if(k)appendf(&b,&l,&c,",");appendf(&b,&l,&c,"{\"event\":");json_escape_append(&b,&l,&c,e->event);appendf(&b,&l,&c,",\"timestamp_ms\":%llu,\"pid\":%d,\"ppid\":%d,\"name\":",(unsigned long long)e->timestamp_ms,e->pid,e->ppid);json_escape_append(&b,&l,&c,e->name);appendf(&b,&l,&c,",\"cpu_percent\":%.3f,\"ram_mb\":%.3f,\"peak_cpu_percent\":%.3f,\"peak_ram_mb\":%.3f,\"first_seen_ms\":%llu,\"last_seen_ms\":%llu,\"lifetime_ms\":%llu,\"final_state\":",e->cpu_percent,e->ram_mb,e->peak_cpu_percent,e->peak_ram_mb,(unsigned long long)e->first_seen_ms,(unsigned long long)e->last_seen_ms,(unsigned long long)e->lifetime_ms);json_escape_append(&b,&l,&c,e->final_state);appendf(&b,&l,&c,"}");}appendf(&b,&l,&c,"]}");return b;}

static char *machines_json(MachineStore *store){MachineConfig ms[MAX_MACHINES];size_t n=machine_store_list(store,ms,MAX_MACHINES);char selected[64];pthread_mutex_lock(&store->lock);snprintf(selected,sizeof(selected),"%s",store->selected_id);pthread_mutex_unlock(&store->lock);char *b=NULL;size_t l=0,c=0;appendf(&b,&l,&c,"{\"selected_id\":");json_escape_append(&b,&l,&c,selected);appendf(&b,&l,&c,",\"machines\":[");for(size_t i=0;i<n;i++){if(i)appendf(&b,&l,&c,",");appendf(&b,&l,&c,"{\"id\":");json_escape_append(&b,&l,&c,ms[i].id);appendf(&b,&l,&c,",\"display_name\":");json_escape_append(&b,&l,&c,ms[i].display_name);appendf(&b,&l,&c,",\"host\":");json_escape_append(&b,&l,&c,ms[i].host);appendf(&b,&l,&c,",\"username\":");json_escape_append(&b,&l,&c,ms[i].username);appendf(&b,&l,&c,",\"port\":%d,\"is_local\":%s}",ms[i].port,ms[i].is_local?"true":"false");}appendf(&b,&l,&c,"]}");return b;}

static int parse_machine_body(const char *body,MachineConfig *m,char *err,size_t errcap){memset(m,0,sizeof(*m));if(json_get_string(body,"display_name",m->display_name,sizeof(m->display_name))!=0||json_get_string(body,"host",m->host,sizeof(m->host))!=0||json_get_string(body,"username",m->username,sizeof(m->username))!=0){snprintf(err,errcap,"display_name, host and username are required");return-1;}if(json_get_int(body,"port",&m->port)!=0)m->port=22;machine_make_id(m->display_name,m->host,m->id,sizeof(m->id));return validate_machine_config(m,err,errcap);}

static int selected_machine(ApiContext *ctx,MachineConfig *m){char id[64];pthread_mutex_lock(&ctx->monitor->lock);snprintf(id,sizeof(id),"%s",ctx->monitor->selected_machine_id);pthread_mutex_unlock(&ctx->monitor->lock);return machine_store_get(ctx->machines,id,m);}


static int activate_machine(ApiContext *ctx,const MachineConfig *m,char *err,size_t errcap){
    if(!ctx||!m)return-1;
    if(m->is_local){
        ctx->remote_active=0;
        monitor_set_local(ctx->monitor);
    }else{
        pthread_mutex_lock(&ctx->remote_collector.lock);
        ctx->remote_collector.machine=*m;
        snprintf(ctx->remote_collector.identity_file,sizeof(ctx->remote_collector.identity_file),"%s",ctx->identity_file);
        memset(&ctx->remote_collector.previous,0,sizeof(ctx->remote_collector.previous));
        ctx->remote_collector.have_previous=0;
        pthread_mutex_unlock(&ctx->remote_collector.lock);
        ctx->remote_active=1;
        monitor_set_remote(ctx->monitor,m->id,m->display_name,remote_cb,&ctx->remote_collector);
    }
    if(machine_store_select(ctx->machines,m->id,err,errcap)!=0){
        monitor_set_local(ctx->monitor);
        ctx->remote_active=0;
        return -1;
    }
    return 0;
}

static void quick_connect_defaults(char *username,size_t usercap,int *port){
    const char *u=getenv("MONITOR_SSH_USER");
    if(u&&*u)snprintf(username,usercap,"%s",u);
    else snprintf(username,usercap,"__ssh_config__");
    *port=22;
    const char *p=getenv("MONITOR_SSH_PORT");
    if(p&&*p){char *e=NULL;long v=strtol(p,&e,10);if(e!=p&&*e=='\0'&&v>=1&&v<=65535)*port=(int)v;}
}

static int parse_quick_connect_body(const char *body,MachineConfig *m,char *err,size_t errcap){
    memset(m,0,sizeof(*m));
    if(json_get_string(body,"host",m->host,sizeof(m->host))!=0){snprintf(err,errcap,"IPv4 address is required");return-1;}
    struct in_addr addr;
    if(inet_pton(AF_INET,m->host,&addr)!=1){snprintf(err,errcap,"Enter a valid IPv4 address, for example 192.168.1.42");return-1;}
    quick_connect_defaults(m->username,sizeof(m->username),&m->port);
    snprintf(m->display_name,sizeof(m->display_name),"Remote %.48s",m->host);
    machine_make_id(m->display_name,m->host,m->id,sizeof(m->id));
    return validate_machine_config(m,err,errcap);
}

static int find_remote_by_endpoint(MachineStore *store,const char *host,const char *username,int port,MachineConfig *out){
    MachineConfig ms[MAX_MACHINES];size_t n=machine_store_list(store,ms,MAX_MACHINES);
    for(size_t i=0;i<n;i++)if(!ms[i].is_local&&strcmp(ms[i].host,host)==0&&strcmp(ms[i].username,username)==0&&ms[i].port==port){if(out)*out=ms[i];return 0;}
    return -1;
}

int api_handle(ApiContext *ctx,const char *method,const char *path,const char *body,ApiResponse *out){
    if(!ctx||!method||!path||!out)return-1;
    if(strcmp(path,"/api/health")==0&&strcmp(method,"GET")==0){*out=resp(200,"{\"ok\":true,\"service\":\"monitor_web\"}");return 1;}
    if(strcmp(path,"/api/machines")==0&&strcmp(method,"GET")==0){char *j=machines_json(ctx->machines);*out=resp(200,j);free(j);return 1;}
    if(strcmp(path,"/api/quick-connect")==0&&strcmp(method,"POST")==0){
        MachineConfig m;char err[256]="";
        if(parse_quick_connect_body(body,&m,err,sizeof(err))!=0){char *b=NULL;size_t l=0,c=0;appendf(&b,&l,&c,"{\"connected\":false,\"error\":");json_escape_append(&b,&l,&c,err);appendf(&b,&l,&c,"}");*out=resp(400,b);free(b);return 1;}
        char *test_json=NULL;int trc=remote_test_connection(&m,ctx->identity_file,&test_json);
        if(trc!=0){*out=resp(502,test_json?test_json:"{\"connected\":false,\"error\":\"SSH connection failed\"}");free(test_json);return 1;}
        char remote_hostname[64]="";if(test_json)json_get_string(test_json,"hostname",remote_hostname,sizeof(remote_hostname));
        MachineConfig chosen;
        if(find_remote_by_endpoint(ctx->machines,m.host,m.username,m.port,&chosen)!=0){
            if(remote_hostname[0]&&strcmp(remote_hostname,"unknown")!=0)snprintf(m.display_name,sizeof(m.display_name),"%s",remote_hostname);
            else snprintf(m.display_name,sizeof(m.display_name),"Remote %.48s",m.host);
            machine_make_id(m.display_name,m.host,m.id,sizeof(m.id));
            if(machine_store_add(ctx->machines,&m,err,sizeof(err))!=0){free(test_json);char *b=NULL;size_t l=0,c=0;appendf(&b,&l,&c,"{\"connected\":false,\"error\":");json_escape_append(&b,&l,&c,err);appendf(&b,&l,&c,"}");*out=resp(400,b);free(b);return 1;}
            if(find_remote_by_endpoint(ctx->machines,m.host,m.username,m.port,&chosen)!=0){free(test_json);*out=resp(500,"{\"connected\":false,\"error\":\"Saved machine could not be reloaded\"}");return 1;}
        }
        if(activate_machine(ctx,&chosen,err,sizeof(err))!=0){free(test_json);*out=resp(500,"{\"connected\":false,\"error\":\"Could not activate remote machine\"}");return 1;}
        char *b=NULL;size_t l=0,c=0;appendf(&b,&l,&c,"{\"connected\":true,\"selected\":true,\"id\":");json_escape_append(&b,&l,&c,chosen.id);appendf(&b,&l,&c,",\"display_name\":");json_escape_append(&b,&l,&c,chosen.display_name);appendf(&b,&l,&c,",\"host\":");json_escape_append(&b,&l,&c,chosen.host);appendf(&b,&l,&c,",\"ssh_user\":");json_escape_append(&b,&l,&c,chosen.username);appendf(&b,&l,&c,",\"ssh_port\":%d,\"connection\":",chosen.port);json_escape_append(&b,&l,&c,"authorized SSH");appendf(&b,&l,&c,"}");
        free(test_json);*out=resp(200,b);free(b);return 1;
    }
    if(strcmp(path,"/api/machines/test")==0&&strcmp(method,"POST")==0){MachineConfig m;char err[256];if(parse_machine_body(body,&m,err,sizeof(err))!=0){char *b=NULL;size_t l=0,c=0;appendf(&b,&l,&c,"{\"connected\":false,\"error\":");json_escape_append(&b,&l,&c,err);appendf(&b,&l,&c,"}");*out=resp(400,b);free(b);return 1;}char *j=NULL;int rc=remote_test_connection(&m,ctx->identity_file,&j);*out=resp(rc==0?200:502,j?j:"{\"connected\":false}");free(j);return 1;}
    if(strcmp(path,"/api/machines")==0&&strcmp(method,"POST")==0){MachineConfig m;char err[256];if(parse_machine_body(body,&m,err,sizeof(err))!=0||machine_store_add(ctx->machines,&m,err,sizeof(err))!=0){char *b=NULL;size_t l=0,c=0;appendf(&b,&l,&c,"{\"saved\":false,\"error\":");json_escape_append(&b,&l,&c,err);appendf(&b,&l,&c,"}");*out=resp(400,b);free(b);return 1;}char *b=NULL;size_t l=0,c=0;appendf(&b,&l,&c,"{\"saved\":true,\"id\":");json_escape_append(&b,&l,&c,m.id);appendf(&b,&l,&c,"}");*out=resp(201,b);free(b);return 1;}
    if(strncmp(path,"/api/machines/",14)==0&&strcmp(method,"DELETE")==0){const char *id=path+14;char current[64];pthread_mutex_lock(&ctx->monitor->lock);snprintf(current,sizeof(current),"%s",ctx->monitor->selected_machine_id);pthread_mutex_unlock(&ctx->monitor->lock);char err[256];if(machine_store_delete(ctx->machines,id,err,sizeof(err))!=0){char *b=NULL;size_t l=0,c=0;appendf(&b,&l,&c,"{\"deleted\":false,\"error\":");json_escape_append(&b,&l,&c,err);appendf(&b,&l,&c,"}");*out=resp(400,b);free(b);}else{if(strcmp(current,id)==0){ctx->remote_active=0;monitor_set_local(ctx->monitor);}*out=resp(200,"{\"deleted\":true}");}return 1;}
    if(strcmp(path,"/api/select-machine")==0&&strcmp(method,"POST")==0){char id[64],err[256];if(json_get_string(body,"id",id,sizeof(id))!=0){*out=resp(400,"{\"selected\":false,\"error\":\"Invalid machine id\"}");return 1;}MachineConfig m;if(machine_store_get(ctx->machines,id,&m)!=0){*out=resp(404,"{\"selected\":false}");return 1;}if(activate_machine(ctx,&m,err,sizeof(err))!=0){*out=resp(500,"{\"selected\":false,\"error\":\"Selection persistence failed\"}");return 1;}*out=resp(200,"{\"selected\":true}");return 1;}
    if(strcmp(path,"/api/system")==0&&strcmp(method,"GET")==0){MonitorSnapshot *s=malloc(sizeof(*s));char le[256]="";if(!s){*out=resp(500,"{\"error\":\"Out of memory\"}");return 1;}if(monitor_get_snapshot(ctx->monitor,s)!=0){free(s);pthread_mutex_lock(&ctx->monitor->lock);snprintf(le,sizeof(le),"%s",ctx->monitor->last_error);pthread_mutex_unlock(&ctx->monitor->lock);if(le[0]){char *b=NULL;size_t l=0,c=0;appendf(&b,&l,&c,"{\"available\":false,\"error\":");json_escape_append(&b,&l,&c,le);appendf(&b,&l,&c,"}");*out=resp(503,b);free(b);}else *out=resp(503,"{\"available\":false,\"error\":\"Snapshot not ready\"}");return 1;}pthread_mutex_lock(&ctx->monitor->lock);snprintf(le,sizeof(le),"%s",ctx->monitor->last_error);pthread_mutex_unlock(&ctx->monitor->lock);char *j=snapshot_system_json(s,le);free(s);*out=resp(200,j);free(j);return 1;}
    if(strcmp(path,"/api/processes")==0&&strcmp(method,"GET")==0){MonitorSnapshot *s=malloc(sizeof(*s));if(!s){*out=resp(500,"{\"error\":\"Out of memory\"}");return 1;}if(monitor_get_snapshot(ctx->monitor,s)!=0){free(s);*out=resp(503,"{\"processes\":[],\"error\":\"Snapshot not ready\"}");return 1;}char *j=processes_json(s);free(s);*out=resp(200,j);free(j);return 1;}
    if(strcmp(path,"/api/applications")==0&&strcmp(method,"GET")==0){MonitorSnapshot *s=malloc(sizeof(*s));if(!s){*out=resp(500,"{\"available\":false,\"error\":\"Out of memory\",\"applications\":[]}");return 1;}if(monitor_get_snapshot(ctx->monitor,s)!=0){free(s);*out=resp(503,"{\"available\":false,\"error\":\"Snapshot not ready\",\"applications\":[]}");return 1;}MachineConfig m;if(selected_machine(ctx,&m)!=0){free(s);*out=resp(404,"{\"available\":false,\"error\":\"Selected machine unavailable\",\"applications\":[]}");return 1;}char *j=NULL;char err[256]="";if(m.is_local)local_open_applications_json(s,&j,err,sizeof(err));else remote_open_applications_json(&m,ctx->identity_file,s,&j);free(s);if(!j){char *b=NULL;size_t l=0,c=0;appendf(&b,&l,&c,"{\"available\":false,\"error\":");json_escape_append(&b,&l,&c,err[0]?err:"Application collection failed");appendf(&b,&l,&c,",\"applications\":[]}");*out=resp(200,b);free(b);}else{*out=resp(200,j);free(j);}return 1;}
    if(strcmp(path,"/api/lifecycle")==0&&strcmp(method,"GET")==0){char *j=lifecycle_json(ctx->monitor);*out=resp(200,j);free(j);return 1;}
    if(strcmp(path,"/api/network")==0&&strcmp(method,"GET")==0){MonitorSnapshot *s=malloc(sizeof(*s));if(!s){*out=resp(500,"{\"error\":\"Out of memory\"}");return 1;}if(monitor_get_snapshot(ctx->monitor,s)!=0){free(s);*out=resp(503,"{\"available\":false}");return 1;}char *j=snapshot_system_json(s,NULL);free(s);*out=resp(200,j);free(j);return 1;}
    if(strcmp(path,"/api/process-demo")==0&&strcmp(method,"POST")==0){char *j=NULL;if(run_process_ipc_demo_json(&j)!=0){*out=resp(500,"{\"ok\":false,\"error\":\"IPC demo failed\"}");return 1;}*out=resp(200,j);free(j);return 1;}
    if(strncmp(path,"/api/process/",13)==0&&strcmp(method,"GET")==0){const char *p=path+13;char pidbuf[32];size_t i=0;while(isdigit((unsigned char)p[i])&&i+1<sizeof(pidbuf)){pidbuf[i]=p[i];i++;}pidbuf[i]='\0';if(i==0){*out=resp(400,"{\"error\":\"Invalid PID\"}");return 1;}pid_t pid=(pid_t)strtol(pidbuf,NULL,10);int want_maps=strcmp(p+i,"/maps")==0;MachineConfig m;if(selected_machine(ctx,&m)!=0){*out=resp(404,"{\"error\":\"Selected machine unavailable\"}");return 1;}char *j=NULL;int rc;if(m.is_local){if(want_maps)rc=process_maps_json(pid,&j);else{MonitorSnapshot *s=malloc(sizeof(*s));if(!s){*out=resp(500,"{\"error\":\"Out of memory\"}");return 1;}monitor_get_snapshot(ctx->monitor,s);rc=inspect_process_json(pid,s,&j);free(s);}}else{if(want_maps)rc=remote_process_maps_json(&m,ctx->identity_file,pid,&j);else{MonitorSnapshot *s=malloc(sizeof(*s));if(!s){*out=resp(500,"{\"error\":\"Out of memory\"}");return 1;}memset(s,0,sizeof(*s));monitor_get_snapshot(ctx->monitor,s);rc=remote_inspect_process_json(&m,ctx->identity_file,pid,s,&j);free(s);}}*out=resp(rc==0?200:(rc==-2?404:502),j?j:"{\"error\":\"Unavailable\"}");free(j);return 1;}
    if(strncmp(path,"/api/",5)==0){*out=resp(404,"{\"error\":\"API route not found\"}");return 1;}
    return 0;
}
