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

// @todo implement
int uv_resident_set_memory(size_t* rss) {

  *rss = 0;
  return 0;

}

// @todo implement
int uv_cpu_info(uv_cpu_info_t** cpu_infos, int* count) {

  *cpu_infos = NULL;
  *count = 0;
  return UV_ENOSYS;

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

  APIRET rc = NO_ERROR;
  PTIB ptib = NULL;
  PPIB ppib = NULL;

  if (buffer == NULL || size == NULL || *size < CCHMAXPATH)
    return UV_EINVAL;

  rc = DosGetInfoBlocks(&ptib,&ppib);
  if (ppib) {
    memset(buffer,0,*size);
    rc = DosQueryModuleName(ppib->pib_hmte,*size,buffer);
    if (NO_ERROR == rc) {
      /* Set new size. */
      *size = strlen(buffer);
      return 0;
    } else
      return ((ERROR_BAD_LENGTH == rc) ? UV_EAI_OVERFLOW : UV_EAI_FAIL);
  }
  return UV_EAI_FAIL;

}

void uv_free_cpu_info(uv_cpu_info_t* cpu_infos, int count) {
  (void)cpu_infos;
  (void)count;
}
