/* Exercise the actual reader with simulated kernel ioctl boundaries. */
#define main reader_main
#define open mock_open
#define close mock_close
#define ioctl mock_ioctl
#define usleep mock_usleep
#define clock_gettime mock_clock_gettime
#define fwrite mock_fwrite
#define fflush mock_fflush
#include "../rk3588/rk_read.c"
#undef main
#undef open
#undef close
#undef ioctl
#undef usleep
#undef clock_gettime
#undef fwrite
#undef fflush
#include <assert.h>
#include <stdarg.h>
static int scenario, reads, acknowledgments, output_frames, closes, ack_high;
static uint64_t simulated_ms;
int mock_open(const char *path, int flags, ...)
{ (void)flags; return strcmp(path, "spi") == 0 ? 10 : 20; }
int mock_close(int fd) { (void)fd; ++closes; return 0; }
int mock_usleep(useconds_t n) { simulated_ms += n / 1000 + 1; return 0; }
int mock_clock_gettime(clockid_t clock, struct timespec *ts)
{ (void)clock; ts->tv_sec = simulated_ms / 1000; ts->tv_nsec = (simulated_ms % 1000) * 1000000; return 0; }
size_t mock_fwrite(const void *p, size_t size, size_t count, FILE *stream)
{ (void)stream; assert(size == 1 && count == 7); assert(((const uint8_t *)p)[0] == 0x5a);
  if (scenario == 2) { errno = ENOSPC; return 0; }
  ++output_frames; return count; }
int mock_fflush(FILE *stream) { (void)stream; if (scenario == 5) { errno = ENOSPC; return -1; } return 0; }
int mock_ioctl(int fd, unsigned long request, ...)
{
    va_list args; void *p;
    va_start(args, request); p = va_arg(args, void *); va_end(args);
    if (request == GPIO_GET_LINEHANDLE_IOCTL) {
        struct gpiohandle_request *r = p;
        r->fd = r->flags == GPIOHANDLE_REQUEST_INPUT ? 30 : 40;
        return 0;
    }
    if (request == GPIOHANDLE_GET_LINE_VALUES_IOCTL) {
        struct gpiohandle_data *d = p; assert(fd == 30);
        if (reads >= 2 && !ack_high) { errno = EIO; return -1; }
        d->values[0] = scenario == 4 ? 1 : !ack_high;
        return 0;
    }
    if (request == GPIOHANDLE_SET_LINE_VALUES_IOCTL) {
        struct gpiohandle_data *d = p; assert(fd == 40);
        ack_high = d->values[0]; if (ack_high) ++acknowledgments;
        return 0;
    }
    if (request == SPI_IOC_MESSAGE(1)) {
        struct spi_ioc_transfer *t = p;
        uint8_t *frame = (void *)(uintptr_t)t->rx_buf;
        batch_hdr_t hdr = {BATCH_MAGIC, (uint32_t)reads + 1, 7, 0};
        ++reads; memset(frame, 0, BATCH_FRAME_SIZE);
        memset(frame + sizeof(hdr), 0x5a, 7);
        hdr.crc32 = batch_frame_crc(&hdr, frame + sizeof(hdr));
        if (scenario == 1) ++hdr.seq; /* Metadata corruption must be detected. */
        memcpy(frame, &hdr, sizeof(hdr));
        return scenario == 3 ? 10 : (int)BATCH_FRAME_SIZE;
    }
    return 0; /* SPI setup */
}
static void run(int which)
{
    char *argv[] = {"rk_read", "spi", "chip", "12", "13"};
    scenario = which; reads = acknowledgments = output_frames = closes = ack_high = 0;
    simulated_ms = 0;
    assert(reader_main(5, argv) == 1); /* Fixture ends via GPIO read error. */
    assert(closes == 5); /* Two chip handles and three owned descriptors. */
    if (!which) assert(reads == 2 && acknowledgments == 2 && output_frames == 2);
    if (which == 1 || which == 2 || which == 3 || which == 5) assert(!acknowledgments);
    if (which == 4) assert(acknowledgments == 1 && simulated_ms >= 10000);
}
int main(void)
{
    unsigned int line;
    assert(parse_line("-1", &line)); assert(parse_line("12junk", &line));
    assert(parse_line("4294967296", &line)); assert(!parse_line("12", &line));
    for (int i = 0; i <= 5; ++i) run(i);
    puts("reader tests passed");
}
