#define _GNU_SOURCE
#include "api.h"
#include "machine_store.h"
#include "monitor.h"
#include "web_server.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static volatile sig_atomic_t g_stop=0;
static void on_signal(int sig){(void)sig;g_stop=1;}
static void install_signals(void){struct sigaction sa;memset(&sa,0,sizeof(sa));sa.sa_handler=on_signal;sigemptyset(&sa.sa_mask);sigaction(SIGINT,&sa,NULL);sigaction(SIGTERM,&sa,NULL);signal(SIGPIPE,SIG_IGN);}

int main(int argc,char **argv){
    int port=8080,bind_lan=0;
    for(int i=1;i<argc;i++){
        if(strcmp(argv[i],"--port")==0&&i+1<argc){port=atoi(argv[++i]);if(port<1||port>65535){fprintf(stderr,"Invalid port\n");return 2;}}
        else if(strcmp(argv[i],"--bind-lan")==0)bind_lan=1;
        else if(strcmp(argv[i],"--help")==0){printf("Usage: %s [--port N] [--bind-lan]\n",argv[0]);return 0;}
        else{fprintf(stderr,"Unknown option: %s\n",argv[i]);return 2;}
    }
    install_signals();
    MachineStore store;
    if(machine_store_init(&store,"config/machines.conf")!=0){fprintf(stderr,"Failed to initialize machine store\n");return 1;}
    MonitorContext *monitor=calloc(1,sizeof(*monitor));
    ApiContext *api=calloc(1,sizeof(*api));
    if(!monitor||!api){fprintf(stderr,"Out of memory\n");free(api);free(monitor);machine_store_destroy(&store);return 1;}
    if(monitor_init(monitor,1000)!=0||monitor_start(monitor)!=0){fprintf(stderr,"Failed to start monitor\n");monitor_destroy(monitor);free(api);free(monitor);machine_store_destroy(&store);return 1;}
    api_context_init(api,monitor,&store);
    WebServer ws;memset(&ws,0,sizeof(ws));ws.port=port;ws.bind_lan=bind_lan;ws.stop_flag=&g_stop;ws.api=api;snprintf(ws.frontend_dir,sizeof(ws.frontend_dir),"frontend");
    if(bind_lan)fprintf(stderr,"WARNING: --bind-lan exposes the dashboard to the local network. Use only on trusted networks.\n");
    int rc=web_server_run(&ws);
    g_stop=1;monitor_stop(monitor);monitor_destroy(monitor);api_context_destroy(api);free(api);free(monitor);machine_store_destroy(&store);
    printf("monitor_web shut down cleanly.\n");return rc==0?0:1;
}
