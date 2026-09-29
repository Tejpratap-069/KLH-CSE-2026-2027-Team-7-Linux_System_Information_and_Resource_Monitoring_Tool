#define _GNU_SOURCE
#include "monitor.h"
#include "utils.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

const ProcessInfo *monitor_find_process(const MonitorSnapshot *snap, pid_t pid) {
    if(!snap) return NULL;
    for(size_t i=0;i<snap->process_count;i++) if(snap->processes[i].pid==pid) return &snap->processes[i];
    return NULL;
}

int collect_local_snapshot(MonitorSnapshot *out, const MonitorSnapshot *prev, char *err, size_t errcap) {
    if(!out) return -1;
    memset(out,0,sizeof(*out));
    out->source_type=SOURCE_LOCAL;
    snprintf(out->machine_id,sizeof(out->machine_id),"local");
    snprintf(out->machine_name,sizeof(out->machine_name),"This Laptop");
    snprintf(out->source_status,sizeof(out->source_status),"LOCAL — LIVE");
    double dt=1.0;
    if(prev && prev->timestamp_ms) {
        uint64_t now=now_ms();
        if(now>prev->timestamp_ms) dt=(double)(now-prev->timestamp_ms)/1000.0;
    }
    if(collect_system_info(&out->system,prev?&prev->system:NULL,dt,err,errcap)!=0) return -1;
    if(collect_processes(out->processes,MAX_PROCESSES,&out->process_count,out->system.mem_total_kb,
                         out->system.cpu_total_ticks,prev?prev->system.cpu_total_ticks:0,
                         prev?prev->processes:NULL,prev?prev->process_count:0,
                         out->system.cpu_cores,&out->system.process_running,&out->system.process_sleeping,&out->system.thread_total,
                         err,errcap)!=0) return -1;
    out->system.process_total=(int)out->process_count;
    out->timestamp_ms=now_ms();
    out->data_age_ms=0;
    return 0;
}

static void push_event(MonitorContext *ctx,const char *event,const TrackedProcess *t,uint64_t now,const ProcessInfo *p) {
    LifecycleEvent e; memset(&e,0,sizeof(e));
    snprintf(e.event,sizeof(e.event),"%s",event);
    e.timestamp_ms=now; e.pid=t->pid; e.ppid=t->ppid; e.start_ticks=t->start_ticks;
    snprintf(e.name,sizeof(e.name),"%s",t->name);
    e.cpu_percent=p?p->cpu_percent:t->latest_cpu; e.ram_mb=p?p->ram_mb:t->latest_ram;
    e.peak_cpu_percent=t->peak_cpu; e.peak_ram_mb=t->peak_ram;
    e.first_seen_ms=t->first_seen_ms; e.last_seen_ms=t->last_seen_ms;
    e.lifetime_ms=(now>=t->first_seen_ms)?now-t->first_seen_ms:0;
    snprintf(e.final_state,sizeof(e.final_state),"%s",strcmp(event,"EXITED")==0?"EXITED":"ACTIVE");
    if(ctx->lifecycle_count<MAX_LIFECYCLE_EVENTS) ctx->lifecycle[ctx->lifecycle_count++]=e;
    else {
        memmove(&ctx->lifecycle[0],&ctx->lifecycle[1],(MAX_LIFECYCLE_EVENTS-1)*sizeof(ctx->lifecycle[0]));
        ctx->lifecycle[MAX_LIFECYCLE_EVENTS-1]=e;
    }
}

static int find_tracked(MonitorContext *ctx,pid_t pid,uint64_t start) {
    for(int i=0;i<MAX_TRACKED_PROCESSES;i++) if(ctx->tracked[i].in_use && ctx->tracked[i].pid==pid && ctx->tracked[i].start_ticks==start) return i;
    return -1;
}

static int alloc_tracked(MonitorContext *ctx) {
    for(int i=0;i<MAX_TRACKED_PROCESSES;i++) if(!ctx->tracked[i].in_use) return i;
    return -1;
}

static void lifecycle_update_locked(MonitorContext *ctx,const MonitorSnapshot *snap,int suppress_initial) {
    uint64_t now=snap->timestamp_ms?snap->timestamp_ms:now_ms();
    for(int i=0;i<MAX_TRACKED_PROCESSES;i++) if(ctx->tracked[i].in_use) ctx->tracked[i].seen_this_round=0;
    for(size_t i=0;i<snap->process_count;i++) {
        const ProcessInfo *p=&snap->processes[i];
        int idx=find_tracked(ctx,p->pid,p->start_ticks);
        if(idx<0) {
            idx=alloc_tracked(ctx); if(idx<0) continue;
            TrackedProcess *t=&ctx->tracked[idx]; memset(t,0,sizeof(*t));
            t->in_use=1;t->pid=p->pid;t->ppid=p->ppid;t->start_ticks=p->start_ticks;snprintf(t->name,sizeof(t->name),"%s",p->name);
            t->first_seen_ms=now;t->last_seen_ms=now;t->last_active_event_ms=now;t->latest_cpu=p->cpu_percent;t->latest_ram=p->ram_mb;t->peak_cpu=p->cpu_percent;t->peak_ram=p->ram_mb;t->seen_this_round=1;
            if(!suppress_initial) push_event(ctx,"STARTED",t,now,p);
        } else {
            TrackedProcess *t=&ctx->tracked[idx]; t->seen_this_round=1;t->last_seen_ms=now;t->ppid=p->ppid;t->latest_cpu=p->cpu_percent;t->latest_ram=p->ram_mb;
            if(p->cpu_percent>t->peak_cpu) t->peak_cpu=p->cpu_percent;
            if(p->ram_mb>t->peak_ram) t->peak_ram=p->ram_mb;
            if(now-t->last_active_event_ms>=5000) { push_event(ctx,"ACTIVE",t,now,p); t->last_active_event_ms=now; }
        }
    }
    for(int i=0;i<MAX_TRACKED_PROCESSES;i++) {
        TrackedProcess *t=&ctx->tracked[i];
        if(t->in_use&&!t->seen_this_round) { push_event(ctx,"EXITED",t,now,NULL); memset(t,0,sizeof(*t)); }
    }
}

static void sleep_ms_interruptible(MonitorContext *ctx,unsigned ms) {
    const unsigned slice=100; unsigned left=ms;
    while(left&&!ctx->stop_requested){unsigned s=left<slice?left:slice;struct timespec ts={s/1000,(long)(s%1000)*1000000L};nanosleep(&ts,NULL);left-=s;}
}

static void *worker_main(void *arg) {
    MonitorContext *ctx=(MonitorContext*)arg;
    MonitorSnapshot *prev=calloc(1,sizeof(*prev));
    MonitorSnapshot *snap=calloc(1,sizeof(*snap));
    if(!prev||!snap){
        free(prev); free(snap);
        pthread_mutex_lock(&ctx->lock);
        snprintf(ctx->last_error,sizeof(ctx->last_error),"Out of memory starting monitor worker");
        pthread_mutex_unlock(&ctx->lock);
        return NULL;
    }
    int first=1;
    while(!ctx->stop_requested) {
        int source; uint64_t generation; monitor_remote_collect_fn cb; void *opaque; char mid[MAX_MACHINE_NAME],mname[MAX_MACHINE_NAME];
        pthread_mutex_lock(&ctx->lock); *prev=ctx->snapshot; source=ctx->selected_source; generation=ctx->selection_generation; cb=ctx->remote_collect; opaque=ctx->remote_opaque; snprintf(mid,sizeof(mid),"%s",ctx->selected_machine_id);snprintf(mname,sizeof(mname),"%s",ctx->selected_machine_name); pthread_mutex_unlock(&ctx->lock);
        char err[256]=""; int rc;
        if(source==SOURCE_REMOTE && cb) rc=cb(opaque,snap,err,sizeof(err)); else rc=collect_local_snapshot(snap,prev->timestamp_ms?prev:NULL,err,sizeof(err));
        pthread_mutex_lock(&ctx->lock);
        if(generation!=ctx->selection_generation || source!=ctx->selected_source){pthread_mutex_unlock(&ctx->lock);continue;}
        if(rc==0) {
            ctx->previous_snapshot=ctx->snapshot; ctx->snapshot=*snap;
            if(source==SOURCE_REMOTE){ctx->snapshot.source_type=SOURCE_REMOTE;snprintf(ctx->snapshot.machine_id,sizeof(ctx->snapshot.machine_id),"%s",mid);snprintf(ctx->snapshot.machine_name,sizeof(ctx->snapshot.machine_name),"%s",mname);snprintf(ctx->snapshot.source_status,sizeof(ctx->snapshot.source_status),"REMOTE — LIVE");}
            lifecycle_update_locked(ctx,&ctx->snapshot,first);
            ctx->last_error[0]='\0'; first=0;
        } else {
            snprintf(ctx->last_error,sizeof(ctx->last_error),"%s",err[0]?err:"Collection failed");
            if(source==SOURCE_REMOTE){snprintf(ctx->snapshot.source_status,sizeof(ctx->snapshot.source_status),"REMOTE — DISCONNECTED");ctx->snapshot.data_age_ms=ctx->snapshot.timestamp_ms?now_ms()-ctx->snapshot.timestamp_ms:0;}
        }
        pthread_mutex_unlock(&ctx->lock);
        sleep_ms_interruptible(ctx,ctx->interval_ms);
    }
    free(snap); free(prev);
    return NULL;
}

int monitor_init(MonitorContext *ctx,unsigned interval_ms){if(!ctx)return-1;memset(ctx,0,sizeof(*ctx));if(pthread_mutex_init(&ctx->lock,NULL)!=0)return-1;ctx->interval_ms=interval_ms?interval_ms:1000;ctx->selected_source=SOURCE_LOCAL;ctx->selection_generation=1;snprintf(ctx->selected_machine_id,sizeof(ctx->selected_machine_id),"local");snprintf(ctx->selected_machine_name,sizeof(ctx->selected_machine_name),"This Laptop");return 0;}
int monitor_start(MonitorContext *ctx){if(!ctx||ctx->worker_started)return-1;ctx->stop_requested=0;if(pthread_create(&ctx->worker,NULL,worker_main,ctx)!=0)return-1;ctx->worker_started=1;return 0;}
void monitor_stop(MonitorContext *ctx){if(!ctx)return;ctx->stop_requested=1;if(ctx->worker_started){pthread_join(ctx->worker,NULL);ctx->worker_started=0;}}
void monitor_destroy(MonitorContext *ctx){if(!ctx)return;monitor_stop(ctx);pthread_mutex_destroy(&ctx->lock);}

static void reset_tracking(MonitorContext *ctx){memset(ctx->tracked,0,sizeof(ctx->tracked));ctx->lifecycle_count=0;ctx->snapshot.timestamp_ms=0;ctx->previous_snapshot.timestamp_ms=0;}
int monitor_set_local(MonitorContext *ctx){if(!ctx)return-1;pthread_mutex_lock(&ctx->lock);ctx->selected_source=SOURCE_LOCAL;ctx->selection_generation++;ctx->remote_collect=NULL;ctx->remote_opaque=NULL;snprintf(ctx->selected_machine_id,sizeof(ctx->selected_machine_id),"local");snprintf(ctx->selected_machine_name,sizeof(ctx->selected_machine_name),"This Laptop");reset_tracking(ctx);pthread_mutex_unlock(&ctx->lock);return 0;}
int monitor_set_remote(MonitorContext *ctx,const char *machine_id,const char *machine_name,monitor_remote_collect_fn cb,void *opaque){if(!ctx||!machine_id||!machine_name||!cb)return-1;pthread_mutex_lock(&ctx->lock);ctx->selected_source=SOURCE_REMOTE;ctx->selection_generation++;ctx->remote_collect=cb;ctx->remote_opaque=opaque;snprintf(ctx->selected_machine_id,sizeof(ctx->selected_machine_id),"%s",machine_id);snprintf(ctx->selected_machine_name,sizeof(ctx->selected_machine_name),"%s",machine_name);reset_tracking(ctx);pthread_mutex_unlock(&ctx->lock);return 0;}
int monitor_get_snapshot(MonitorContext *ctx,MonitorSnapshot *out){if(!ctx||!out)return-1;pthread_mutex_lock(&ctx->lock);*out=ctx->snapshot;if(out->timestamp_ms)out->data_age_ms=now_ms()-out->timestamp_ms;pthread_mutex_unlock(&ctx->lock);return out->timestamp_ms?0:-1;}
size_t monitor_get_lifecycle(MonitorContext *ctx,LifecycleEvent *out,size_t max_count){if(!ctx||!out||!max_count)return 0;pthread_mutex_lock(&ctx->lock);size_t n=ctx->lifecycle_count<max_count?ctx->lifecycle_count:max_count;size_t start=ctx->lifecycle_count-n;for(size_t i=0;i<n;i++)out[i]=ctx->lifecycle[start+i];pthread_mutex_unlock(&ctx->lock);return n;}
