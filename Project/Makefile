CC := gcc
CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -O2 -pthread -Iinclude
LDFLAGS := -pthread

COMMON_OBJS := src/utils.o src/system_info.o src/process_monitor.o src/monitor.o src/process_demo.o src/application_monitor.o
WEB_OBJS := backend/machine_store.o backend/remote_ssh.o backend/api.o backend/web_server.o backend/web_main.o
TERM_OBJS := src/terminal_main.o

.PHONY: all clean test web-run debug
all: system_monitor monitor_web

system_monitor: $(COMMON_OBJS) $(TERM_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

monitor_web: $(COMMON_OBJS) $(WEB_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

backend/%.o: backend/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f src/*.o backend/*.o system_monitor monitor_web

web-run: monitor_web
	./monitor_web

test: all
	bash scripts/test_all.sh

debug:
	$(MAKE) clean
	$(MAKE) CFLAGS='-std=c11 -Wall -Wextra -Wpedantic -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -pthread -Iinclude' LDFLAGS='-pthread -fsanitize=address,undefined'
