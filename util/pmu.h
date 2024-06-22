#include <asm/unistd.h>
#include <linux/perf_event.h>
#include <linux/hw_breakpoint.h>
#include <stdint.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

static long perf_event_open(
  struct perf_event_attr* hw_event,
  pid_t pid,
  int cpu,
  int group_fd,
  unsigned long flags
) {
  int ret;
  ret = syscall(__NR_perf_event_open, hw_event, pid, cpu, group_fd, flags);
  return ret;
}

// To be used with type=PERF_TYPE_HW_CACHE.
const uint64_t PERF_COUNT_L1D_READ_MISS = PERF_COUNT_HW_CACHE_L1D
  | (PERF_COUNT_HW_CACHE_OP_READ << 8)
  | (PERF_COUNT_HW_CACHE_RESULT_MISS << 16);

// // To be used with type=PERF_TYPE_HW_CACHE.
// const uint64_t PERF_COUNT_L2_READ_MISS = PERF_COUNT_HW_CACHE_L2
//   | (PERF_COUNT_HW_CACHE_OP_READ << 8)
//   | (PERF_COUNT_HW_CACHE_RESULT_MISS << 16);

// To be used with type=PERF_TYPE_HW_CACHE.
const uint64_t PERF_COUNT_L3_READ_MISS = PERF_COUNT_HW_CACHE_LL
  | (PERF_COUNT_HW_CACHE_OP_READ << 8)
  | (PERF_COUNT_HW_CACHE_RESULT_MISS << 16);

// Open the PMU specified by event_type and config, or exit(EXIT_FAILURE)
// if that doesn't work.
//
// Returns the file descriptor given by perf_event_open. This can be given
// as an argument to StartPMU() or StopPMU() to start or stop measuring
// the opened PMU.
int OpenPMUOrDie(uint32_t event_type, uint64_t config) {
  struct perf_event_attr pe;
  int fd;

  memset(&pe, 0, sizeof(pe));
  pe.type = event_type;
  pe.size = sizeof(pe);
  pe.config = config;
  pe.disabled = 1;
  pe.exclude_kernel = 1;
  pe.exclude_hv = 1;

  fd = perf_event_open(&pe, 0, -1, -1, 0);
  if (fd == -1) {
    fprintf(stderr, "Error opening leader %llx\n", pe.config);
    perror("perf_event_open() failed");
    exit(EXIT_FAILURE);
  }

  return fd;
}

void StartPMU(int fd) {
  ioctl(fd, PERF_EVENT_IOC_RESET, 0);
  ioctl(fd, PERF_EVENT_IOC_ENABLE, 0);
}

long long StopPMU(int fd) {
  ioctl(fd, PERF_EVENT_IOC_DISABLE, 0);
  long long count;
  size_t bytesRead = read(fd, &count, sizeof(count));
  assert(bytesRead == sizeof(count));
  return count;
}
