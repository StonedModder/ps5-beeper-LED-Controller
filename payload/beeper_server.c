/* SPDX-License-Identifier: GPL-3.0-or-later
 * beeper_server.c - persistent PS5 beeper + LED TCP command server
 *
 * Listens on TCP port 9111. Accepts newline-terminated commands:
 *   BUZZ <0-3>   sceKernelIccSetBuzzer(n)
 *   VOL  <0-2>   sceKernelIccSetBuzzerDimSetting(n)     0=High 1=Med 2=Low
 *   MUTE <0-1>   sceKernelIccSetBuzzerOffSetting(n)     0=unmute 1=mute
 *   LED  <0-2>   sceKernelIccSetDynamicLedDimSetting(n) (values TBD)
 *   QUIT         shut down server
 *
 * Responds with: OK ret=0x<hex>\n  or  ERR <reason>\n
 */

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/socket.h>
#include <netinet/in.h>

#include <ps5/kernel.h>
#include <ps5/klog.h>
#include <ps5/nid.h>

#define BP(fmt, ...) klog_printf("[BeeperServer] " fmt, ##__VA_ARGS__)
#define SERVER_PORT  9111

extern int sceKernelIccSetBuzzer(int value);

#define LIBKERNEL_HANDLE 0x1

static intptr_t
resolve_sym(const char *sym)
{
    intptr_t addr = kernel_dynlib_dlsym(getpid(), LIBKERNEL_HANDLE, sym);
    if (addr) return addr;
    char nid[12];
    nid_encode(sym, nid);
    return kernel_dynlib_resolve(getpid(), LIBKERNEL_HANDLE, nid);
}

static void
elevate(void)
{
    uint8_t caps[16] = {
        0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
        0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff
    };
    pid_t mypid = getpid();
    kernel_set_ucred_authid(mypid, 0x3800000000010003l);
    kernel_set_ucred_caps(mypid, caps);
}

typedef struct {
    intptr_t vol_addr;
    intptr_t mute_addr;
    intptr_t led_addr;
} ServerSyms;

static void
handle_client(int fd, const ServerSyms *syms)
{
    typedef int (*IccFn)(int);

    char buf[64];
    int  pos = 0;

    while (1) {
        char c;
        int n = (int)recv(fd, &c, 1, 0);
        if (n <= 0) break;
        if (c == '\r') continue;
        if (c != '\n') {
            if (pos < (int)sizeof(buf) - 1) buf[pos++] = c;
            continue;
        }

        buf[pos] = '\0';
        pos = 0;

        char resp[64];
        char cmd[16];
        int  val = 0;

        if (sscanf(buf, "%15s %d", cmd, &val) < 1) {
            snprintf(resp, sizeof(resp), "ERR bad_input\n");
            send(fd, resp, strlen(resp), 0);
            continue;
        }

        if (strcmp(cmd, "QUIT") == 0) {
            snprintf(resp, sizeof(resp), "OK bye\n");
            send(fd, resp, strlen(resp), 0);
            BP("QUIT received\n");
            break;
        }

        int ret;

        if (strcmp(cmd, "BUZZ") == 0) {
            if (val < 0 || val > 3) {
                snprintf(resp, sizeof(resp), "ERR val_range 0-3\n");
                send(fd, resp, strlen(resp), 0);
                continue;
            }
            ret = sceKernelIccSetBuzzer(val);
            BP("BUZZ %d ret=0x%x\n", val, ret);

        } else if (strcmp(cmd, "VOL") == 0) {
            if (val < 0 || val > 2) {
                snprintf(resp, sizeof(resp), "ERR val_range 0-2\n");
                send(fd, resp, strlen(resp), 0);
                continue;
            }
            if (!syms->vol_addr) {
                snprintf(resp, sizeof(resp), "ERR sym_not_found\n");
                send(fd, resp, strlen(resp), 0);
                continue;
            }
            ret = ((IccFn)syms->vol_addr)(val);
            BP("VOL %d ret=0x%x\n", val, ret);

        } else if (strcmp(cmd, "MUTE") == 0) {
            if (val < 0 || val > 1) {
                snprintf(resp, sizeof(resp), "ERR val_range 0-1\n");
                send(fd, resp, strlen(resp), 0);
                continue;
            }
            if (!syms->mute_addr) {
                snprintf(resp, sizeof(resp), "ERR sym_not_found\n");
                send(fd, resp, strlen(resp), 0);
                continue;
            }
            ret = ((IccFn)syms->mute_addr)(val);
            BP("MUTE %d ret=0x%x\n", val, ret);

        } else if (strcmp(cmd, "LED") == 0) {
            if (val < 0 || val > 2) {
                snprintf(resp, sizeof(resp), "ERR val_range 0-2\n");
                send(fd, resp, strlen(resp), 0);
                continue;
            }
            if (!syms->led_addr) {
                snprintf(resp, sizeof(resp), "ERR sym_not_found\n");
                send(fd, resp, strlen(resp), 0);
                continue;
            }
            ret = ((IccFn)syms->led_addr)(val);
            BP("LED %d ret=0x%x\n", val, ret);

        } else {
            snprintf(resp, sizeof(resp), "ERR unknown_cmd\n");
            send(fd, resp, strlen(resp), 0);
            continue;
        }

        snprintf(resp, sizeof(resp), "OK ret=0x%x\n", (unsigned int)ret);
        send(fd, resp, strlen(resp), 0);
    }
}

int
main(void)
{
    elevate();

    ServerSyms syms;
    syms.vol_addr  = resolve_sym("sceKernelIccSetBuzzerDimSetting");
    syms.mute_addr = resolve_sym("sceKernelIccSetBuzzerOffSetting");
    syms.led_addr  = resolve_sym("sceKernelIccSetDynamicLedDimSetting");

    BP("=== BeeperServer start port=%d ===\n", SERVER_PORT);
    BP("vol  sceKernelIccSetBuzzerDimSetting     @ 0x%lx\n", syms.vol_addr);
    BP("mute sceKernelIccSetBuzzerOffSetting     @ 0x%lx\n", syms.mute_addr);
    BP("led  sceKernelIccSetDynamicLedDimSetting @ 0x%lx\n", syms.led_addr);

    int srv = socket(AF_INET, SOCK_STREAM, 0);
    if (srv < 0) {
        BP("socket() failed errno=%d\n", errno);
        return 1;
    }

    int opt = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = 0;
    addr.sin_port        = __builtin_bswap16(SERVER_PORT);

    if (bind(srv, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        BP("bind() failed errno=%d\n", errno);
        close(srv);
        return 1;
    }

    if (listen(srv, 4) < 0) {
        BP("listen() failed errno=%d\n", errno);
        close(srv);
        return 1;
    }

    BP("listening on port %d\n", SERVER_PORT);

    while (1) {
        int cli = accept(srv, NULL, NULL);
        if (cli < 0) {
            BP("accept() errno=%d\n", errno);
            continue;
        }
        BP("client connected\n");
        handle_client(cli, &syms);
        close(cli);
        BP("client disconnected\n");
    }

    close(srv);
    BP("=== BeeperServer exit ===\n");
    return 0;
}
