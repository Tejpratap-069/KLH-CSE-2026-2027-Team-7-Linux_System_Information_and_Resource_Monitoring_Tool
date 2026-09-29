#define _GNU_SOURCE
#include "web_server.h"
#include "utils.h"
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define MAX_REQUEST (64*1024)
#define MAX_BODY (32*1024)
#define MAX_URL 2048

typedef struct { int fd; WebServer *server; } ClientArg;

static const char *reason_phrase(int status){switch(status){case 200:return"OK";case 201:return"Created";case 400:return"Bad Request";case 404:return"Not Found";case 405:return"Method Not Allowed";case 413:return"Payload Too Large";case 500:return"Internal Server Error";case 502:return"Bad Gateway";case 503:return"Service Unavailable";default:return"OK";}}

static int send_response(int fd,int status,const char *ctype,const void *body,size_t len){char head[512];int n=snprintf(head,sizeof(head),"HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %zu\r\nCache-Control: no-store\r\nX-Content-Type-Options: nosniff\r\nConnection: close\r\n\r\n",status,reason_phrase(status),ctype?ctype:"text/plain; charset=utf-8",len);if(n<0)return-1;if(safe_write_all(fd,head,(size_t)n)!=0)return-1;if(len&&safe_write_all(fd,body,len)!=0)return-1;return 0;}

static const char *mime_for(const char *path){const char *e=strrchr(path,'.');if(!e)return"application/octet-stream";if(strcmp(e,".html")==0)return"text/html; charset=utf-8";if(strcmp(e,".css")==0)return"text/css; charset=utf-8";if(strcmp(e,".js")==0)return"application/javascript; charset=utf-8";if(strcmp(e,".svg")==0)return"image/svg+xml";if(strcmp(e,".png")==0)return"image/png";return"application/octet-stream";}

static int serve_static(WebServer *s,int fd,const char *path){const char *rel=NULL;if(strcmp(path,"/")==0||strcmp(path,"/index.html")==0)rel="index.html";else if(strcmp(path,"/styles.css")==0)rel="styles.css";else if(strcmp(path,"/app.js")==0)rel="app.js";else if(strncmp(path,"/assets/",8)==0&&strstr(path,"..")==NULL)rel=path+1;else return send_response(fd,404,"text/plain; charset=utf-8","Not found",9);char full[1024];size_t a=strlen(s->frontend_dir),b=strlen(rel);if(a+1+b+1>sizeof(full))return send_response(fd,400,"text/plain; charset=utf-8","Path too long",13);memcpy(full,s->frontend_dir,a);full[a]='/';memcpy(full+a+1,rel,b);full[a+1+b]='\0';int f=open(full,O_RDONLY|O_CLOEXEC);if(f<0)return send_response(fd,404,"text/plain; charset=utf-8","Not found",9);struct stat st;if(fstat(f,&st)!=0||st.st_size<0||st.st_size>8*1024*1024){close(f);return send_response(fd,500,"text/plain; charset=utf-8","Static file error",17);}size_t len=(size_t)st.st_size;char *buf=malloc(len?len:1);if(!buf){close(f);return send_response(fd,500,"text/plain; charset=utf-8","Out of memory",13);}size_t off=0;while(off<len){ssize_t n=read(f,buf+off,len-off);if(n<0){if(errno==EINTR)continue;free(buf);close(f);return-1;}if(n==0)break;off+=(size_t)n;}close(f);int rc=send_response(fd,200,mime_for(full),buf,off);free(buf);return rc;}

static ssize_t find_header_end(const char *buf,size_t len){for(size_t i=3;i<len;i++)if(buf[i-3]=='\r'&&buf[i-2]=='\n'&&buf[i-1]=='\r'&&buf[i]=='\n')return(ssize_t)(i+1);return-1;}

static int parse_content_length(const char *headers,size_t len){const char *p=headers,*end=headers+len;while(p<end){const char *nl=strstr(p,"\r\n");if(!nl||nl>end)break;if(strncasecmp(p,"Content-Length:",15)==0){p+=15;while(p<nl&&(*p==' '||*p=='\t'))p++;char tmp[32];size_t n=(size_t)(nl-p);if(n>=sizeof(tmp))return-1;memcpy(tmp,p,n);tmp[n]='\0';char *e=NULL;long v=strtol(tmp,&e,10);if(e==tmp||v<0||v>MAX_BODY)return-1;return(int)v;}p=nl+2;}return 0;}

static void *client_main(void *arg){ClientArg *ca=(ClientArg*)arg;int fd=ca->fd;WebServer *s=ca->server;free(ca);struct timeval tv={8,0};setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&tv,sizeof(tv));setsockopt(fd,SOL_SOCKET,SO_SNDTIMEO,&tv,sizeof(tv));char *req=malloc(MAX_REQUEST+1);if(!req){close(fd);return NULL;}size_t len=0;ssize_t hend=-1;int clen=0;while(len<MAX_REQUEST){ssize_t n=recv(fd,req+len,MAX_REQUEST-len,0);if(n<0){if(errno==EINTR)continue;break;}if(n==0)break;len+=(size_t)n;req[len]='\0';if(hend<0){hend=find_header_end(req,len);if(hend>0){clen=parse_content_length(req,(size_t)hend);if(clen<0){send_response(fd,413,"application/json","{\"error\":\"Request body too large\"}",34);free(req);close(fd);return NULL;}}}if(hend>0&&len>=(size_t)hend+(size_t)clen)break;}if(hend<0){send_response(fd,400,"application/json","{\"error\":\"Malformed request\"}",29);free(req);close(fd);return NULL;}req[len]='\0';char method[16],path[MAX_URL+1],version[16];if(sscanf(req,"%15s %2048s %15s",method,path,version)!=3){send_response(fd,400,"application/json","{\"error\":\"Bad request line\"}",28);free(req);close(fd);return NULL;}if(strlen(path)>MAX_URL){send_response(fd,400,"application/json","{\"error\":\"URL too long\"}",24);free(req);close(fd);return NULL;}char *q=strchr(path,'?');if(q)*q='\0';const char *body=req+hend;ApiResponse ar;memset(&ar,0,sizeof(ar));int handled=api_handle(s->api,method,path,body,&ar);if(handled){send_response(fd,ar.status,ar.content_type,ar.body?ar.body:"",ar.body?strlen(ar.body):0);api_response_free(&ar);}else if(strcmp(method,"GET")==0)serve_static(s,fd,path);else send_response(fd,405,"application/json","{\"error\":\"Method not allowed\"}",30);free(req);shutdown(fd,SHUT_RDWR);close(fd);return NULL;}

static void *client_worker(void *arg){ClientArg *ca=(ClientArg*)arg;WebServer *s=ca->server;client_main(arg);__sync_sub_and_fetch(&s->active_clients,1);return NULL;}

int web_server_run(WebServer *server){if(!server||!server->api||!server->stop_flag)return-1;int fd=socket(AF_INET,SOCK_STREAM,0);if(fd<0){perror("socket");return-1;}int one=1;setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&one,sizeof(one));struct sockaddr_in addr;memset(&addr,0,sizeof(addr));addr.sin_family=AF_INET;addr.sin_port=htons((uint16_t)server->port);addr.sin_addr.s_addr=htonl(server->bind_lan?INADDR_ANY:INADDR_LOOPBACK);if(bind(fd,(struct sockaddr*)&addr,sizeof(addr))!=0){fprintf(stderr,"Cannot bind %s:%d: %s\n",server->bind_lan?"0.0.0.0":"127.0.0.1",server->port,strerror(errno));if(errno==EADDRINUSE)fprintf(stderr,"Port %d is already in use. Try: ./monitor_web --port %d\n",server->port,server->port+1);close(fd);return-1;}if(listen(fd,64)!=0){perror("listen");close(fd);return-1;}server->listen_fd=fd;printf("monitor_web listening on http://%s:%d\n",server->bind_lan?"0.0.0.0":"localhost",server->port);fflush(stdout);while(!*server->stop_flag){struct pollfd pfd={fd,POLLIN,0};int pr=poll(&pfd,1,500);if(pr<0){if(errno==EINTR)continue;break;}if(pr==0)continue;if(pfd.revents&POLLIN){int cfd=accept(fd,NULL,NULL);if(cfd<0){if(errno==EINTR)continue;continue;}ClientArg *ca=malloc(sizeof(*ca));if(!ca){close(cfd);continue;}ca->fd=cfd;ca->server=server;pthread_t t;__sync_add_and_fetch(&server->active_clients,1);if(pthread_create(&t,NULL,client_worker,ca)!=0){__sync_sub_and_fetch(&server->active_clients,1);free(ca);close(cfd);}else pthread_detach(t);}}close(fd);server->listen_fd=-1;for(int i=0;i<100&&server->active_clients>0;i++)usleep(100000);if(server->active_clients>0)fprintf(stderr,"Warning: %d HTTP worker(s) still finishing during shutdown.\n",server->active_clients);return 0;}
