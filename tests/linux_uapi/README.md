Test-only Linux v6.6 UAPI fixtures from torvalds/linux/include/uapi:
gpio.h, const.h, spi/spidev.h, spi/spi.h, asm-generic/ioctl.h (as linux/ioctl.h).
Original SPDX notices retained. types.h is a local host simulation shim.
Production rk_read uses the target Linux system headers, never these fixtures.
These tests simulate syscalls; they do not validate a physical SPI controller.
