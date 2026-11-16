/* Copyright libuv project contributors. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "uv.h"
#include "internal.h"

#define INCL_DOS
#define INCL_DOSERRORS
#include <os2emx.h>

#include <string.h>
#include <unistd.h>

int uv_uptime(double* uptime) {

  int r;
  struct timespec sp;

  r = clock_gettime(CLOCK_MONOTONIC, &sp);
  if (r)
    return UV__ERR(errno);

  *uptime = sp.tv_sec;
  return 0;

}

/* OS/2 has no API for a process's resident set, so report the committed
 * private memory instead: walk the private arenas (below 512 MB, and the
 * high memory arena up to the virtual address limit) with DosQueryMem() and
 * add up the committed, non-shared pages. Reserved but uncommitted memory
 * (e.g. V8's address space reservations) and shared memory (DLL code, the
 * shared arena) are not counted. */
int uv_resident_set_memory(size_t* rss) {
  const ULONG low_end = 0x20000000;  /* 512 MB: end of the low arenas. */
  ULONG high_end = low_end;
  ULONG limit_mb = 0;
  ULONG addr;
  ULONG end;
  ULONG size;
  ULONG flags;
  uint64_t total = 0;

  if (DosQuerySysInfo(QSV_VIRTUALADDRESSLIMIT, QSV_VIRTUALADDRESSLIMIT,
                      &limit_mb, sizeof(limit_mb)) == NO_ERROR &&
      limit_mb > 512 && limit_mb <= 4096) {
    high_end = (limit_mb == 4096) ? 0xFFFFF000 : limit_mb << 20;
  }

  addr = 0x10000;  /* The first 64 KB are never mapped. */
  end = low_end;
  for (;;) {
    if (addr >= end) {
      if (end == low_end && high_end > low_end) {
        addr = low_end;
        end = high_end;
        continue;
      }
      break;
    }

    size = end - addr;
    if (DosQueryMem((PVOID) addr, &size, &flags) != NO_ERROR || size == 0) {
      /* Not queryable (e.g. between arenas): move on to the next arena. */
      addr = end;
      continue;
    }

    if ((flags & PAG_COMMIT) && !(flags & (PAG_FREE | PAG_SHARED)))
      total += size;

    /* Don't wrap around at the top of the address space. */
    if (size > end - addr)
      break;
    addr += size;
  }

  *rss = (size_t) total;
  return 0;
}

/* Per-processor counters returned by DosPerfSysCall(CMD_KI_RDCNT), in units
 * of the timer frequency (DosTmrQueryFreq). */
typedef struct {
  ULONG time_low, time_high;
  ULONG idle_low, idle_high;
  ULONG busy_low, busy_high;
  ULONG intr_low, intr_high;
} uv__os2_cpuutil_t;

#define UV__OS2_CMD_KI_RDCNT 0x63
#define UV__OS2_ORD_DOSPERFSYSCALL 976

typedef ULONG APIENTRY (*uv__os2_perfsyscall_t)(ULONG, ULONG, ULONG, ULONG);

static uint64_t uv__os2_ticks_to_ms(ULONG low, ULONG high, ULONG freq) {
  uint64_t ticks = ((uint64_t) high << 32) | low;
  return freq ? ticks * 1000 / freq : 0;
}

static char* uv__os2_cpu_model(void) {
#if defined(__i386__) || defined(__x86_64__)
  unsigned int regs[12];
  unsigned int max;
  unsigned int i;
  char* p;

  __asm__ __volatile__("cpuid"
                       : "=a"(max), "=b"(regs[0]), "=c"(regs[1]), "=d"(regs[2])
                       : "a"(0x80000000));
  if (max >= 0x80000004) {
    for (i = 0; i < 3; i++)
      __asm__ __volatile__("cpuid"
                           : "=a"(regs[i * 4]), "=b"(regs[i * 4 + 1]),
                             "=c"(regs[i * 4 + 2]), "=d"(regs[i * 4 + 3])
                           : "a"(0x80000002 + i));
    p = (char*) regs;
    p[sizeof(regs) - 1] = '\0';
    while (*p == ' ')
      p++;
    if (*p != '\0')
      return uv__strdup(p);
  }
#endif
  return uv__strdup("unknown");
}

/* OS/2 has no per-CPU clock speed or user/system split: speed is 0, busy time
 * is reported as user time and interrupt time as irq. */
int uv_cpu_info(uv_cpu_info_t** cpu_infos, int* count) {
  ULONG ncpus = 0;
  ULONG freq = 0;
  ULONG i;
  HMODULE doscalls;
  uv__os2_perfsyscall_t perfsyscall = NULL;
  uv__os2_cpuutil_t* util = NULL;
  uv_cpu_info_t* infos;

  *cpu_infos = NULL;
  *count = 0;

  if (DosQuerySysInfo(QSV_NUMPROCESSORS, QSV_NUMPROCESSORS,
                      &ncpus, sizeof(ncpus)) != NO_ERROR || ncpus == 0)
    ncpus = 1;

  infos = uv__calloc(ncpus, sizeof(*infos));
  if (infos == NULL)
    return UV_ENOMEM;

  util = uv__calloc(ncpus, sizeof(*util));
  if (util != NULL &&
      DosTmrQueryFreq(&freq) == NO_ERROR &&
      DosQueryModuleHandle((PCSZ) "DOSCALLS", &doscalls) == NO_ERROR &&
      DosQueryProcAddr(doscalls, UV__OS2_ORD_DOSPERFSYSCALL, NULL,
                       (PFN*) &perfsyscall) == NO_ERROR) {
    if (perfsyscall(UV__OS2_CMD_KI_RDCNT, (ULONG) util, 0, 0) != NO_ERROR)
      memset(util, 0, ncpus * sizeof(*util));
  }

  for (i = 0; i < ncpus; i++) {
    infos[i].model = uv__os2_cpu_model();
    if (infos[i].model == NULL) {
      uv__free(util);
      uv_free_cpu_info(infos, i);
      return UV_ENOMEM;
    }
    infos[i].speed = 0;
    if (util != NULL) {
      infos[i].cpu_times.user =
          uv__os2_ticks_to_ms(util[i].busy_low, util[i].busy_high, freq);
      infos[i].cpu_times.idle =
          uv__os2_ticks_to_ms(util[i].idle_low, util[i].idle_high, freq);
      infos[i].cpu_times.irq =
          uv__os2_ticks_to_ms(util[i].intr_low, util[i].intr_high, freq);
    }
  }

  uv__free(util);
  *cpu_infos = infos;
  *count = (int) ncpus;
  return 0;
}

// @todo implement
uint64_t uv_get_constrained_memory(void) {

  return 0;  /* Memory constraints are unknown. */

}

uint64_t uv_get_free_memory(void) {

  APIRET rc = NO_ERROR;
  ULONG ulValue = 0;

  rc = DosQuerySysInfo(QSV_TOTAVAILMEM,QSV_TOTAVAILMEM,&ulValue,sizeof(ulValue));
  if (NO_ERROR == rc)
    return (uint64_t)ulValue;

  return 0;

}

uint64_t uv_get_available_memory(void) {

  return uv_get_free_memory();

}

uint64_t uv_get_total_memory(void) {

  APIRET rc = NO_ERROR;
  ULONG ulValue = 0;

  rc = DosQuerySysInfo(QSV_TOTPHYSMEM,QSV_TOTPHYSMEM,&ulValue,sizeof(ulValue));
  if (NO_ERROR == rc)
    return (uint64_t)ulValue;

  return 0;

}

void uv_loadavg(double avg[3]) {

  avg[0] = 0;
  avg[1] = 0;
  avg[2] = 0;

}

int uv_exepath(char* buffer, size_t* size) {
  char path[PATH_MAX];
  char exepath[PATH_MAX];

  if (buffer == NULL || size == NULL || *size == 0)
    return -EINVAL;

  if (_execname(path, sizeof(path)) == -1)
    return -EIO;

  /* Get a real name not an upper-cased name, and convert back-slashes to
   * slashes.
   */
  if (!realpath(path, exepath))
    return -errno;

  /* Copy to buffer at most *size bytes. */
  strncpy(buffer, exepath, *size);
  buffer[*size - 1] = '\0';
  *size = strlen(buffer);

  return 0;
}

int uv__random_os2random(void* buf, size_t buflen) {

  int i;
  char *buf2 = buf;

  if (buflen == 0)
    return 0;

  for (i = 0; i < buflen;) {
      unsigned long ul = (unsigned long)random();
      *buf2++ = (unsigned char)(ul);
      i++;
      /* Don't write past the end of buf when buflen is odd. */
      if (i < buflen)
        *buf2++ = (unsigned char)(ul >> 8);
      i++;
  }

  return 0;
}
