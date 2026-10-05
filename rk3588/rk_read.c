#define _GNU_SOURCE

#include "../common/batch_protocol.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/gpio.h>
#include <linux/spi/spidev.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <time.h>

/*
 * RK3588 做 SPI 主机，把 STM32 内存里的一帧读进本进程的 buf。
 *
 *   ./rk_read /dev/spidev0.0 /dev/gpiochip1 14 15
 *
 * 最后两个参数分别是 READY、ACK 在这个 gpiochip 里的线号。
 * 上例对应鲁班猫 5 的物理 8、10 脚，需关闭这两脚的 UART/PWM 复用。
 * 线号和 spidev 节点以你板子的设备树为准。
 *
 * 接线见 stm32/batch_link.c 顶部。两边共地，电平都是 3.3V。
 */

static int gpio_ready_fd(const char *chip_path, unsigned int line)
{
    struct gpiohandle_request req;
    int fd = open(chip_path, O_RDONLY);
    if (fd < 0) {
        perror(chip_path);
        return -1;
    }

    memset(&req, 0, sizeof(req));
    req.lineoffsets[0] = line;
    req.flags = GPIOHANDLE_REQUEST_INPUT;
    req.lines = 1;
    snprintf(req.consumer_label, sizeof(req.consumer_label), "batch-ready");

    if (ioctl(fd, GPIO_GET_LINEHANDLE_IOCTL, &req) < 0) {
        perror("GPIO_GET_LINEHANDLE_IOCTL");
        close(fd);
        return -1;
    }
    close(fd);
    return req.fd;
}

static int gpio_get(int fd)
{
    struct gpiohandle_data data;
    memset(&data, 0, sizeof(data));
    if (ioctl(fd, GPIOHANDLE_GET_LINE_VALUES_IOCTL, &data) < 0) {
        perror("GPIOHANDLE_GET_LINE_VALUES_IOCTL");
        return -1;
    }
    return data.values[0] ? 1 : 0;
}

static int gpio_ack_fd(const char *path, unsigned int line)
{
    struct gpiohandle_request req = {0};
    int chip = open(path, O_RDONLY);
    if (chip < 0) { perror(path); return -1; }
    req.lineoffsets[0] = line;
    req.flags = GPIOHANDLE_REQUEST_OUTPUT;
    req.lines = 1;
    snprintf(req.consumer_label, sizeof(req.consumer_label), "batch-ack");
    if (ioctl(chip, GPIO_GET_LINEHANDLE_IOCTL, &req) < 0) {
        perror("ACK request"); close(chip); return -1;
    }
    close(chip);
    return req.fd;
}
static int ack_set(int fd, int high)
{
    struct gpiohandle_data data = {0};
    data.values[0] = high;
    if (ioctl(fd, GPIOHANDLE_SET_LINE_VALUES_IOCTL, &data) < 0) {
        perror("ACK set"); return -1;
    }
    return 0;
}
static uint64_t now_ms(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) < 0) { perror("clock"); exit(1); }
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}
static int wait_level(int fd, int expected)
{
    uint64_t start = now_ms();
    for (;;) {
        int value = gpio_get(fd);
        if (value < 0) return -1;
        if (value == expected) return 0;
        if (!expected && now_ms() - start >= 10000) {
            fprintf(stderr, "READY timeout waiting for %d\n", expected);
            return -1;
        }
        usleep(200);
    }
}
static int parse_line(const char *value, unsigned int *line)
{
    char *end;
    unsigned long n;
    errno = 0;
    n = strtoul(value, &end, 0);
    if (errno || !*value || *end || value[0] == '-' || n > UINT32_MAX) return -1;
    *line = (unsigned int)n;
    return 0;
}

static int spi_open(const char *path)
{
    int fd = open(path, O_RDWR);
    uint8_t mode = SPI_MODE_0;
    uint8_t bits = 8;
    uint32_t speed = 1000000;

    if (fd < 0) {
        perror(path);
        return -1;
    }
    if (ioctl(fd, SPI_IOC_WR_MODE, &mode) < 0 ||
        ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits) < 0 ||
        ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed) < 0) {
        perror("spi setup");
        close(fd);
        return -1;
    }
    return fd;
}

static int spi_read_frame(int fd, uint8_t *rx)
{
    uint8_t tx[BATCH_FRAME_SIZE];
    struct spi_ioc_transfer tr;

    memset(tx, 0xFF, sizeof(tx));
    memset(&tr, 0, sizeof(tr));
    tr.tx_buf = (uintptr_t)tx;
    tr.rx_buf = (uintptr_t)rx;
    tr.len = BATCH_FRAME_SIZE;
    tr.speed_hz = 1000000;
    tr.bits_per_word = 8;

    int transferred = ioctl(fd, SPI_IOC_MESSAGE(1), &tr);
    if (transferred != (int)BATCH_FRAME_SIZE) {
        if (transferred < 0) perror("SPI_IOC_MESSAGE");
        else fprintf(stderr, "Short SPI transfer: %d/%u bytes\n", transferred, (unsigned int)BATCH_FRAME_SIZE);
        return -1;
    }
    return 0;
}

int main(int argc, char **argv)
{
    int spi = -1, ready = -1, ackfd = -1, result = 1;
    unsigned int ready_line, ack_line;
    uint8_t frame[BATCH_FRAME_SIZE];
    uint32_t last_seq = 0;
    int have_seq = 0;
    if (argc != 5 || parse_line(argv[3], &ready_line) ||
        parse_line(argv[4], &ack_line) || ready_line == ack_line) {
        fprintf(stderr, "Usage: %s SPIDEV GPIOCHIP READY_LINE ACK_LINE\n", argv[0]);
        return 1;
    }
    spi = spi_open(argv[1]);
    if (spi < 0) goto out;
    ready = gpio_ready_fd(argv[2], ready_line);
    if (ready < 0) goto out;
    ackfd = gpio_ack_fd(argv[2], ack_line);
    if (ackfd < 0) goto out;
    for (;;) {
        batch_hdr_t hdr;
        const uint8_t *payload = frame + sizeof(hdr);
        if (wait_level(ready, 1) || spi_read_frame(spi, frame)) goto out;
        memcpy(&hdr, frame, sizeof(hdr));
        if (hdr.magic != BATCH_MAGIC || !hdr.length || hdr.length > BATCH_MAX_PAYLOAD ||
            batch_frame_crc(&hdr, payload) != hdr.crc32) {
            fprintf(stderr, "Invalid frame; no ACK sent, data retained on STM32.\n");
            goto out;
        }
        if (have_seq && hdr.seq != last_seq && hdr.seq != last_seq + 1u) {
            fprintf(stderr, "Sequence discontinuity: previous=%u received=%u\n", last_seq, hdr.seq);
            goto out;
        }
        if (!have_seq || hdr.seq != last_seq) {
            if (fwrite(payload, 1, hdr.length, stdout) != hdr.length || fflush(stdout) != 0) {
                perror("stdout"); goto out;
            }
            last_seq = hdr.seq;
            have_seq = 1;
        }
        if (ack_set(ackfd, 1) || wait_level(ready, 0) || ack_set(ackfd, 0)) goto out;
        fprintf(stderr, "Accepted seq=%u length=%u\n", hdr.seq, hdr.length);
    }
out:
    /* Leave ACK high on handshake timeout: lowering it would release an
     * unconfirmed transaction. Kernel releases the line on process exit. */
    if (ackfd >= 0) close(ackfd);
    if (ready >= 0) close(ready);
    if (spi >= 0) close(spi);
    return result;
}
