CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined
.PHONY: test
test: build/test_batch build/test_rk
	./build/test_batch
	./build/test_rk
build:
	mkdir -p build
build/test_batch: test_batch.c ../stm32/batch_link.c ../stm32/batch_link.h ../common/batch_protocol.h mock/stm32f1xx_hal.h | build
	$(CC) $(CFLAGS) -Imock -I../stm32 test_batch.c ../stm32/batch_link.c -o $@
build/test_rk: test_rk.c ../rk3588/rk_read.c ../common/batch_protocol.h | build
	$(CC) $(CFLAGS) -Ilinux_uapi test_rk.c -o $@
