# SPDX-License-Identifier: GPL-3.0-or-later
# PS5 Beeper Server payload

PS5_HOST ?= ps5
PS5_PORT ?= 9021

include $(PS5_PAYLOAD_SDK)/toolchain/prospero.mk

CFLAGS  += -Wall -Wextra -g -O2 -fPIC -fno-stack-protector
LDFLAGS += -lSceUserService -lpthread

.PHONY: all clean deploy

all: beeper_server.elf

beeper_server.elf: beeper_server.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

deploy: beeper_server.elf
	$(PS5_PAYLOAD_SDK)/bin/prospero-deploy -h $(PS5_HOST) -p $(PS5_PORT) $<

clean:
	rm -f beeper_server.elf
