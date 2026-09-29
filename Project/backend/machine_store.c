#define _GNU_SOURCE
#include "machine_store.h"
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int allowed_token(const char *s, int host) {
    if(!s||!*s) return 0;
    size_t n=strlen(s); if(n>255) return 0;
    for(size_t i=0;i<n;i++) {
        unsigned char c=(unsigned char)s[i];
        if(isalnum(c)||c=='.'||c=='_'||c=='-'||(host&&c==':')) continue;
        return 0;
    }
    return 1;
}

void machine_make_id(const char *display_name,const char *host,char *out,size_t outcap){
    if(!out||!outcap) return;
    size_t j=0;
    const char *src=(display_name&&*display_name)?display_name:host;
    for(size_t i=0;src&&src[i]&&j+1<outcap;i++){unsigned char c=(unsigned char)src[i];if(isalnum(c))out[j++]=(char)tolower(c);else if((c==' '||c=='-'||c=='_')&&j&&out[j-1]!='-')out[j++]='-';}
    while(j&&out[j-1]=='-') j--;
    if(!j){snprintf(out,outcap,"remote");} else out[j]='\0';
}

int validate_machine_config(const MachineConfig *m,char *err,size_t errcap){
    if(!m){if(err&&errcap)snprintf(err,errcap,"Missing machine configuration");return -1;}
    if(m->is_local)return 0;
    if(!m->display_name[0]||strlen(m->display_name)>=sizeof(m->display_name)){if(err&&errcap)snprintf(err,errcap,"Display name is required");return -1;}
    for(const char *p=m->display_name;*p;p++){unsigned char c=(unsigned char)*p;if(c<' '||c=='|'||c=='\n'||c=='\r'){if(err&&errcap)snprintf(err,errcap,"Display name contains invalid characters");return -1;}}
    if(!allowed_token(m->host,1)){if(err&&errcap)snprintf(err,errcap,"Hostname/IP contains invalid characters");return -1;}
    if(!allowed_token(m->username,0)){if(err&&errcap)snprintf(err,errcap,"SSH username contains invalid characters");return -1;}
    if(m->port<1||m->port>65535){if(err&&errcap)snprintf(err,errcap,"SSH port must be 1-65535");return -1;}
    if(m->id[0]&&!allowed_token(m->id,0)){if(err&&errcap)snprintf(err,errcap,"Machine ID contains invalid characters");return -1;}
    return 0;
}

int machine_store_init(MachineStore *store,const char *path){
    if(!store||!path) return -1;
    memset(store,0,sizeof(*store));if(pthread_mutex_init(&store->lock,NULL)!=0)return-1;snprintf(store->config_path,sizeof(store->config_path),"%s",path);snprintf(store->selected_id,sizeof(store->selected_id),"local");return machine_store_load(store);
}
void machine_store_destroy(MachineStore *store){if(store)pthread_mutex_destroy(&store->lock);}

int machine_store_load(MachineStore *store){
    if(!store) return -1;
    pthread_mutex_lock(&store->lock);store->count=0;
    MachineConfig local;memset(&local,0,sizeof(local));local.is_local=1;local.port=0;snprintf(local.id,sizeof(local.id),"local");snprintf(local.display_name,sizeof(local.display_name),"This Laptop");snprintf(local.host,sizeof(local.host),"localhost");store->machines[store->count++]=local;
    FILE *f=fopen(store->config_path,"r");
    if(f){char line[768];while(fgets(line,sizeof(line),f)&&store->count<MAX_MACHINES){if(line[0]=='#'||line[0]=='\n')continue;line[strcspn(line,"\r\n")]='\0';MachineConfig m;memset(&m,0,sizeof(m));char *save=NULL;char *id=strtok_r(line,"|",&save),*name=strtok_r(NULL,"|",&save),*host=strtok_r(NULL,"|",&save),*user=strtok_r(NULL,"|",&save),*port=strtok_r(NULL,"|",&save);if(!id||!name||!host||!user||!port)continue;snprintf(m.id,sizeof(m.id),"%s",id);snprintf(m.display_name,sizeof(m.display_name),"%s",name);snprintf(m.host,sizeof(m.host),"%s",host);snprintf(m.username,sizeof(m.username),"%s",user);m.port=atoi(port);if(validate_machine_config(&m,NULL,0)==0)store->machines[store->count++]=m;}fclose(f);}
    pthread_mutex_unlock(&store->lock);return 0;
}

int machine_store_save(MachineStore *store){
    if(!store)return-1;
    char tmp[600];snprintf(tmp,sizeof(tmp),"%s.tmp",store->config_path);
    FILE *f=fopen(tmp,"w");if(!f)return-1;
    fprintf(f,"# id|display_name|host|username|port\n");
    for(size_t i=0;i<store->count;i++){MachineConfig *m=&store->machines[i];if(m->is_local)continue;fprintf(f,"%s|%s|%s|%s|%d\n",m->id,m->display_name,m->host,m->username,m->port);}
    if(fclose(f)!=0){unlink(tmp);return-1;}
    if(rename(tmp,store->config_path)!=0){unlink(tmp);return-1;}return 0;
}

size_t machine_store_list(MachineStore *store,MachineConfig *out,size_t max_count){if(!store||!out)return 0;pthread_mutex_lock(&store->lock);size_t n=store->count<max_count?store->count:max_count;memcpy(out,store->machines,n*sizeof(*out));pthread_mutex_unlock(&store->lock);return n;}
int machine_store_get(MachineStore *store,const char *id,MachineConfig *out){if(!store||!id||!out)return-1;pthread_mutex_lock(&store->lock);int rc=-1;for(size_t i=0;i<store->count;i++)if(strcmp(store->machines[i].id,id)==0){*out=store->machines[i];rc=0;break;}pthread_mutex_unlock(&store->lock);return rc;}

int machine_store_add(MachineStore *store,const MachineConfig *machine,char *err,size_t errcap){
    if(!store||!machine) return -1;
    MachineConfig m=*machine;if(validate_machine_config(&m,err,errcap)!=0)return-1;if(!m.id[0])machine_make_id(m.display_name,m.host,m.id,sizeof(m.id));
    pthread_mutex_lock(&store->lock);if(store->count>=MAX_MACHINES){pthread_mutex_unlock(&store->lock);if(err&&errcap)snprintf(err,errcap,"Machine limit reached");return-1;}
    char base[64];snprintf(base,sizeof(base),"%s",m.id);int suffix=2,unique=0;while(!unique){unique=1;for(size_t i=0;i<store->count;i++)if(strcmp(store->machines[i].id,m.id)==0){unique=0;snprintf(m.id,sizeof(m.id),"%.48s-%d",base,suffix++);break;}}
    store->machines[store->count++]=m;int rc=machine_store_save(store);pthread_mutex_unlock(&store->lock);if(rc!=0&&err&&errcap)snprintf(err,errcap,"Could not persist machine config");return rc;
}

int machine_store_delete(MachineStore *store,const char *id,char *err,size_t errcap){
    if(!store||!id||strcmp(id,"local")==0){if(err&&errcap)snprintf(err,errcap,"Local machine cannot be deleted");return-1;}
    pthread_mutex_lock(&store->lock);size_t idx=store->count;for(size_t i=0;i<store->count;i++)if(strcmp(store->machines[i].id,id)==0){idx=i;break;}if(idx==store->count){pthread_mutex_unlock(&store->lock);if(err&&errcap)snprintf(err,errcap,"Machine not found");return-1;}memmove(&store->machines[idx],&store->machines[idx+1],(store->count-idx-1)*sizeof(store->machines[0]));store->count--;if(strcmp(store->selected_id,id)==0)snprintf(store->selected_id,sizeof(store->selected_id),"local");int rc=machine_store_save(store);pthread_mutex_unlock(&store->lock);return rc;
}
int machine_store_select(MachineStore *store,const char *id,char *err,size_t errcap){if(!store||!id)return-1;MachineConfig m;if(machine_store_get(store,id,&m)!=0){if(err&&errcap)snprintf(err,errcap,"Machine not found");return-1;}pthread_mutex_lock(&store->lock);snprintf(store->selected_id,sizeof(store->selected_id),"%s",id);pthread_mutex_unlock(&store->lock);return 0;}
