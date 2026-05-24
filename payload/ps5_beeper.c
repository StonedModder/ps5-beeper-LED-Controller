/* SPDX-License-Identifier: GPL-3.0-or-later
 * ps5_beeper.c - PS5 hardware beeper library implementation
 */

#include <stdint.h>
#include <unistd.h>

#include <ps5/kernel.h>
#include <ps5/nid.h>

#include "ps5_beeper.h"

extern int sceKernelIccSetBuzzer(int value);

int ps5_beeper_silent(void) { return sceKernelIccSetBuzzer(PS5_BEEPER_VALUE_SILENT); }
int ps5_beeper_single(void) { return sceKernelIccSetBuzzer(PS5_BEEPER_VALUE_SINGLE); }
int ps5_beeper_error(void)  { return sceKernelIccSetBuzzer(PS5_BEEPER_VALUE_ERROR);  }
int ps5_beeper_long(void)   { return sceKernelIccSetBuzzer(PS5_BEEPER_VALUE_LONG);   }
int ps5_beeper_raw(int v)   { return sceKernelIccSetBuzzer(v);                        }

/* handle=0x1 is libkernel for own process (kernel_dynlib_handle(getpid()) fails) */
#define PS5_BEEPER_LIBKERNEL_HANDLE 0x1

static intptr_t
beeper_resolve(const char *sym)
{
    intptr_t addr = kernel_dynlib_dlsym(getpid(), PS5_BEEPER_LIBKERNEL_HANDLE, sym);
    if (addr) return addr;
    char nid[12];
    nid_encode(sym, nid);
    return kernel_dynlib_resolve(getpid(), PS5_BEEPER_LIBKERNEL_HANDLE, nid);
}

int
ps5_beeper_set_volume(int level)
{
    intptr_t addr = beeper_resolve("sceKernelIccSetBuzzerDimSetting");
    if (!addr) return -1;
    typedef int (*Fn)(int);
    return ((Fn)addr)(level);
}

int
ps5_beeper_set_mute(int mute)
{
    intptr_t addr = beeper_resolve("sceKernelIccSetBuzzerOffSetting");
    if (!addr) return -1;
    typedef int (*Fn)(int);
    return ((Fn)addr)(mute);
}

int
ps5_beeper_set_led(int level)
{
    intptr_t addr = beeper_resolve("sceKernelIccSetDynamicLedDimSetting");
    if (!addr) return -1;
    typedef int (*Fn)(int);
    return ((Fn)addr)(level);
}
