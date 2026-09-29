#define _GNU_SOURCE
#include "application_monitor.h"
#include "utils.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define APP_CAPTURE_LIMIT (2 * 1024 * 1024)
#define MAX_OPEN_APPS 256

typedef struct {
    pid_t pid;
    char name[128];
    char display_name[160];
    char title[512];
    char kind[24];
    double cpu_percent;
    double ram_mb;
    int process_count;
} OpenApplication;

static int contains_casefold(const char *s, const char *needle) {
    if (!s || !needle || !*needle) return 0;
    size_t nl = strlen(needle);
    for (const char *p = s; *p; ++p) {
        size_t i = 0;
        while (i < nl && p[i] && tolower((unsigned char)p[i]) == tolower((unsigned char)needle[i])) i++;
        if (i == nl) return 1;
    }
    return 0;
}

int is_wsl_environment(void) {
    const char *interop = getenv("WSL_INTEROP");
    const char *distro = getenv("WSL_DISTRO_NAME");
    if ((interop && *interop) || (distro && *distro)) return 1;
    char buf[512];
    if (read_text_file("/proc/sys/kernel/osrelease", buf, sizeof(buf), NULL) == 0 &&
        (contains_casefold(buf, "microsoft") || contains_casefold(buf, "wsl"))) return 1;
    if (read_text_file("/proc/version", buf, sizeof(buf), NULL) == 0 &&
        contains_casefold(buf, "microsoft")) return 1;
    return 0;
}

const char *application_kind_for_name(const char *name) {
    if (!name) return "application";
    if (contains_casefold(name, "chrome") || contains_casefold(name, "msedge") ||
        contains_casefold(name, "firefox") || contains_casefold(name, "brave") ||
        contains_casefold(name, "opera") || contains_casefold(name, "vivaldi") ||
        contains_casefold(name, "chromium")) return "browser";
    if (contains_casefold(name, "code") || contains_casefold(name, "idea") ||
        contains_casefold(name, "pycharm") || contains_casefold(name, "eclipse") ||
        contains_casefold(name, "devenv")) return "developer";
    if (contains_casefold(name, "terminal") || contains_casefold(name, "konsole") ||
        strcmp(name, "cmd") == 0 || contains_casefold(name, "powershell")) return "terminal";
    return "application";
}

void application_friendly_name(const char *name, char *out, size_t cap) {
    if (!out || cap == 0) return;
    if (!name) name = "Unknown";
    if (contains_casefold(name, "msedge")) snprintf(out, cap, "Microsoft Edge");
    else if (contains_casefold(name, "chrome") && !contains_casefold(name, "chromium")) snprintf(out, cap, "Google Chrome");
    else if (contains_casefold(name, "chromium")) snprintf(out, cap, "Chromium");
    else if (contains_casefold(name, "firefox")) snprintf(out, cap, "Mozilla Firefox");
    else if (contains_casefold(name, "brave")) snprintf(out, cap, "Brave");
    else if (contains_casefold(name, "vivaldi")) snprintf(out, cap, "Vivaldi");
    else if (contains_casefold(name, "opera")) snprintf(out, cap, "Opera");
    else if (strcasecmp(name, "code") == 0 || contains_casefold(name, "code.exe")) snprintf(out, cap, "Visual Studio Code");
    else if (contains_casefold(name, "windowsTerminal")) snprintf(out, cap, "Windows Terminal");
    else if (contains_casefold(name, "gnome-terminal") || contains_casefold(name, "konsole")) snprintf(out, cap, "Terminal");
    else if (strcasecmp(name, "explorer") == 0 || contains_casefold(name, "explorer.exe")) snprintf(out, cap, "File Explorer");
    else if (strcasecmp(name, "notepad") == 0 || contains_casefold(name, "notepad.exe")) snprintf(out, cap, "Notepad");
    else if (contains_casefold(name, "nautilus")) snprintf(out, cap, "Files");
    else if (contains_casefold(name, "soffice") || contains_casefold(name, "libreoffice")) snprintf(out, cap, "LibreOffice");
    else if (contains_casefold(name, "spotify")) snprintf(out, cap, "Spotify");
    else if (contains_casefold(name, "discord")) snprintf(out, cap, "Discord");
    else {
        snprintf(out, cap, "%s", name);
        if (out[0]) out[0] = (char)toupper((unsigned char)out[0]);
    }
}

int application_name_is_noise(const char *name) {
    static const char *exact[] = {
        "systemd","dbus-daemon","pipewire","wireplumber","pulseaudio","gjs","xwayland",
        "bash","zsh","fish","sh","ssh","sshd","sleep","cron","polkitd","rtkit-daemon",
        "gdm","login","wsl","init","supervisord","log_forwarder","monitor_web","system_monitor",
        "timeout",NULL
    };
    static const char *prefix[] = {"gvfs","at-spi","xdg-","dconf","ibus","tracker-miner","gnome-keyring",NULL};
    if (!name || !*name) return 1;
    for(int i=0;exact[i];++i) if(strcasecmp(name,exact[i])==0) return 1;
    for(int i=0;prefix[i];++i) if(strncasecmp(name,prefix[i],strlen(prefix[i]))==0) return 1;
    return 0;
}

static int proc_has_gui_session(pid_t pid) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/environ", pid);
    int fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) return 0;
    char buf[65536];
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n <= 0) return 0;
    buf[n] = '\0';
    size_t i = 0;
    while (i < (size_t)n) {
        const char *entry = buf + i;
        size_t remain = (size_t)n - i;
        size_t len = strnlen(entry, remain);
        if (len >= 8 && (strncmp(entry, "DISPLAY=", 8) == 0 || strncmp(entry, "WAYLAND_DISPLAY=", 16) == 0)) return 1;
        i += len + 1;
    }
    return 0;
}

static int proc_looks_like_gui_client(pid_t pid, const char *name) {
    if (application_kind_for_name(name) != NULL) {
        const char *kind = application_kind_for_name(name);
        if (strcmp(kind,"browser")==0 || strcmp(kind,"developer")==0 || strcmp(kind,"terminal")==0) return 1;
    }
    char path[64]; snprintf(path,sizeof(path),"/proc/%d/maps",pid);
    char *maps=NULL; size_t n=0;
    if (read_text_file_alloc(path, 1024*1024, &maps, &n) != 0) return 0;
    (void)n;
    const char *tokens[]={"libX11.so","libxcb.so","libwayland-client.so","libgtk-3.so","libgtk-4.so","libQt5Gui.so","libQt6Gui.so","libgdk-3.so","libgdk-4.so",NULL};
    int gui=0; for(int i=0;tokens[i];++i) if(strstr(maps,tokens[i])){gui=1;break;}
    free(maps); return gui;
}

static int app_index_by_display(OpenApplication *apps, size_t count, const char *display_name) {
    for (size_t i = 0; i < count; ++i) if (strcmp(apps[i].display_name, display_name) == 0) return (int)i;
    return -1;
}

static void append_app_json(char **b, size_t *l, size_t *c, const OpenApplication *a) {
    appendf(b,l,c,"{\"pid\":%d,\"name\":", a->pid); json_escape_append(b,l,c,a->name);
    appendf(b,l,c,",\"application\":"); json_escape_append(b,l,c,a->display_name);
    appendf(b,l,c,",\"kind\":"); json_escape_append(b,l,c,a->kind);
    appendf(b,l,c,",\"title\":"); json_escape_append(b,l,c,a->title);
    appendf(b,l,c,",\"cpu_percent\":%.3f,\"ram_mb\":%.3f,\"process_count\":%d}",
            a->cpu_percent, a->ram_mb, a->process_count);
}

static int linux_apps_json(const MonitorSnapshot *snap, char **json_out, const char *platform, const char *note) {
    OpenApplication apps[MAX_OPEN_APPS];
    size_t count = 0;
    memset(apps, 0, sizeof(apps));
    for (size_t i = 0; i < snap->process_count; ++i) {
        const ProcessInfo *p = &snap->processes[i];
        if (application_name_is_noise(p->name) || !proc_has_gui_session(p->pid) || !proc_looks_like_gui_client(p->pid,p->name)) continue;
        char friendly[160]; application_friendly_name(p->name, friendly, sizeof(friendly));
        int idx = app_index_by_display(apps, count, friendly);
        if (idx < 0) {
            if (count >= MAX_OPEN_APPS) break;
            idx = (int)count++;
            apps[idx].pid = p->pid;
            snprintf(apps[idx].name, sizeof(apps[idx].name), "%s", p->name);
            snprintf(apps[idx].display_name, sizeof(apps[idx].display_name), "%s", friendly);
            snprintf(apps[idx].kind, sizeof(apps[idx].kind), "%s", application_kind_for_name(p->name));
            apps[idx].process_count = 0;
        }
        apps[idx].process_count++;
        apps[idx].cpu_percent += p->cpu_percent;
        apps[idx].ram_mb += p->ram_mb;
        if (p->pid < apps[idx].pid) apps[idx].pid = p->pid;
    }
    char *b=NULL; size_t l=0,c=0;
    appendf(&b,&l,&c,"{\"available\":true,\"source\":\"local\",\"platform\":");
    json_escape_append(&b,&l,&c,platform);
    appendf(&b,&l,&c,",\"detection_mode\":\"proc-gui-session\",\"note\":"); json_escape_append(&b,&l,&c,note);
    appendf(&b,&l,&c,",\"applications\":[");
    for (size_t i=0;i<count;i++){if(i)appendf(&b,&l,&c,",");append_app_json(&b,&l,&c,&apps[i]);}
    appendf(&b,&l,&c,"]}");
    *json_out=b;
    return 0;
}

static int powershell_path(char *out, size_t cap) {
    const char *paths[] = {
        "/mnt/c/Windows/System32/WindowsPowerShell/v1.0/powershell.exe",
        "/mnt/c/Program Files/PowerShell/7/pwsh.exe",
        NULL
    };
    for (int i=0;paths[i];++i) if (access(paths[i], X_OK) == 0) { snprintf(out,cap,"%s",paths[i]); return 0; }
    snprintf(out,cap,"powershell.exe");
    return 0;
}

static int run_windows_app_probe(char **out, char *err, size_t errcap) {
    static const char *script =
        "[Console]::OutputEncoding=[Text.UTF8Encoding]::new($false);"
        "$ErrorActionPreference='SilentlyContinue';"
        "$cores=[Math]::Max(1,[Environment]::ProcessorCount);"
        "$a=@{}; Get-Process | ForEach-Object { $c=0; if($null -ne $_.CPU){$c=[double]$_.CPU}; $a[$_.Id]=$c };"
        "Start-Sleep -Milliseconds 200;"
        "Get-Process | Where-Object { $_.MainWindowHandle -ne 0 -and -not [string]::IsNullOrWhiteSpace($_.MainWindowTitle) } | Sort-Object Id | ForEach-Object {"
        "$n=($_.ProcessName -replace '[|\\t\\r\\n]',' '); $t=($_.MainWindowTitle -replace '[|\\t\\r\\n]',' ');"
        "$c=0; if($null -ne $_.CPU){$c=[double]$_.CPU}; $old=0; if($a.ContainsKey($_.Id)){$old=[double]$a[$_.Id]};"
        "$pct=[Math]::Max(0,(($c-$old)/0.2/$cores*100)); $ws=[int64]$_.WorkingSet64;"
        "('APP|{0}|{1}|{2}|{3}|{4}' -f $_.Id,$n,$ws,$pct.ToString('F3',[Globalization.CultureInfo]::InvariantCulture),$t) }"
        ;
    int p[2]; if (pipe(p)!=0){snprintf(err,errcap,"pipe() failed: %s",strerror(errno));return -1;}
    pid_t pid=fork();
    if(pid<0){int e=errno;close(p[0]);close(p[1]);snprintf(err,errcap,"fork() failed: %s",strerror(e));return -1;}
    if(pid==0){
        dup2(p[1],STDOUT_FILENO);dup2(p[1],STDERR_FILENO);close(p[0]);close(p[1]);
        char exe[512];powershell_path(exe,sizeof(exe));
        char *const argv[]={exe,"-NoLogo","-NoProfile","-NonInteractive","-Command",(char*)script,NULL};
        execv(exe,argv);
        char *const fallback[]={"powershell.exe","-NoLogo","-NoProfile","-NonInteractive","-Command",(char*)script,NULL};
        execvp("powershell.exe",fallback);
        _exit(127);
    }
    close(p[1]);int flags=fcntl(p[0],F_GETFL,0);if(flags>=0)fcntl(p[0],F_SETFL,flags|O_NONBLOCK);
    size_t cap=8192,len=0;char *buf=malloc(cap);if(!buf){kill(pid,SIGKILL);waitpid(pid,NULL,0);close(p[0]);return -1;}buf[0]='\0';
    uint64_t start=now_ms();int status=0,done=0,eof=0,timed=0;
    while(!done||!eof){
        struct pollfd fd={p[0],POLLIN|POLLHUP,0};poll(&fd,1,80);
        for(;;){char tmp[4096];ssize_t n=read(p[0],tmp,sizeof(tmp));if(n>0){if(len+(size_t)n+1>APP_CAPTURE_LIMIT){kill(pid,SIGKILL);waitpid(pid,NULL,0);free(buf);close(p[0]);snprintf(err,errcap,"Windows application output exceeded limit");return -1;}if(len+(size_t)n+1>cap){size_t nc=cap*2;while(nc<len+(size_t)n+1)nc*=2;char *nb=realloc(buf,nc);if(!nb){kill(pid,SIGKILL);waitpid(pid,NULL,0);free(buf);close(p[0]);return -1;}buf=nb;cap=nc;}memcpy(buf+len,tmp,(size_t)n);len+=(size_t)n;buf[len]='\0';continue;}if(n==0)eof=1;break;}
        if(!done){pid_t w=waitpid(pid,&status,WNOHANG);if(w==pid)done=1;}
        if(now_ms()-start>5000){kill(pid,SIGKILL);waitpid(pid,&status,0);done=1;timed=1;}
        if(done&&eof)break;
    }
    close(p[0]);
    if(timed||!WIFEXITED(status)||WEXITSTATUS(status)!=0){
        if(timed)snprintf(err,errcap,"Windows application probe timed out");
        else if(WIFEXITED(status)&&WEXITSTATUS(status)==127)snprintf(err,errcap,"Windows PowerShell is unavailable from WSL");
        else snprintf(err,errcap,"Windows application probe failed: %.160s",buf);
        free(buf);return -1;
    }
    *out=buf;return 0;
}

static int windows_apps_json(char **json_out, char *err, size_t errcap) {
    char *raw=NULL;
    if(run_windows_app_probe(&raw,err,errcap)!=0) return -1;
    OpenApplication apps[MAX_OPEN_APPS];size_t count=0;memset(apps,0,sizeof(apps));
    char *save=NULL;
    for(char *line=strtok_r(raw,"\n",&save);line&&count<MAX_OPEN_APPS;line=strtok_r(NULL,"\n",&save)){
        trim_newline(line);if(strncmp(line,"APP|",4)!=0)continue;
        char *p=line+4,*f[5]={0};int nf=0;f[nf++]=p;
        for(char *q=p;*q&&nf<5;q++)if(*q=='|'){*q='\0';f[nf++]=q+1;}
        if(nf<5)continue;
        char *end=NULL;long pidv=strtol(f[0],&end,10);if(end==f[0]||pidv<=0)continue;
        OpenApplication *a=&apps[count++];a->pid=(pid_t)pidv;snprintf(a->name,sizeof(a->name),"%s",f[1]);application_friendly_name(f[1],a->display_name,sizeof(a->display_name));snprintf(a->kind,sizeof(a->kind),"%s",application_kind_for_name(f[1]));
        unsigned long long ws=strtoull(f[2],NULL,10);a->ram_mb=(double)ws/(1024.0*1024.0);a->cpu_percent=strtod(f[3],NULL);a->process_count=1;snprintf(a->title,sizeof(a->title),"%s",f[4]);
    }
    free(raw);
    char *b=NULL;size_t l=0,c=0;
    appendf(&b,&l,&c,"{\"available\":true,\"source\":\"windows-host\",\"platform\":\"wsl2-windows-host\",\"detection_mode\":\"windows-visible-window\",\"note\":");
    json_escape_append(&b,&l,&c,"Visible native Windows applications are read from Windows Get-Process through WSL interop. Browser rows include the visible browser window title, which can identify the current web app/page title but not every browser tab or its URL.");
    appendf(&b,&l,&c,",\"applications\":[");for(size_t i=0;i<count;i++){if(i)appendf(&b,&l,&c,",");append_app_json(&b,&l,&c,&apps[i]);}appendf(&b,&l,&c,"]}");*json_out=b;return 0;
}

int local_open_applications_json(const MonitorSnapshot *snap, char **json_out, char *err, size_t errcap) {
    if(!snap||!json_out) return -1;
    *json_out=NULL;
    if(is_wsl_environment()){
        if(windows_apps_json(json_out,err,errcap)==0)return 0;
        char note[512];snprintf(note,sizeof(note),"WSL detected, but Windows visible-window collection is unavailable (%s). Showing Linux GUI-session processes from WSL instead. Windows-native apps require WSL interop/PowerShell.",err&&*err?err:"unknown error");
        return linux_apps_json(snap,json_out,"wsl-linux-fallback",note);
    }
    return linux_apps_json(snap,json_out,"native-linux","Applications are identified from real Linux processes whose /proc/<PID>/environ contains DISPLAY or WAYLAND_DISPLAY. This dependency-light mode identifies GUI-session processes; exact compositor window titles are not guaranteed.");
}
