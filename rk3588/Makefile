CC ?= gcc
CFLAGS ?= -O2 -Wall -Wextra

rk_read: rk_read.c ../common/batch_protocol.h
	$(CC) $(CFLAGS) -o rk_read rk_read.c

clean:
	rm -f rk_read
