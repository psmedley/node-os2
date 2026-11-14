/*
 * DLL initialization/termination for libnode.dll on OS/2.
 *
 * kLIBC's default _DLL_InitTerm (libc_dll.lib, dllinit.obj) runs a DLL's
 * static destructors at DLL termination, after exit() has finished and
 * OS/2 has ended the process's other threads. Node's destructors expect
 * what they get on other platforms, where a shared library's static
 * destructors run inside exit(): e.g. LegacyTracingAgent's destructor runs
 * its libuv loop once, and libuv aborts when poll() fails at that point.
 *
 * So run the static destructors from an atexit() handler instead, as on
 * other platforms. If the process ends without exit() (abort(), _exit()),
 * they are skipped, as they are there too.
 */

#define INCL_BASE
#include <os2.h>
#include <stdlib.h>

int _CRT_init(void);
void _CRT_term(void);
void __ctordtorInit(void);
void __ctordtorTerm(void);

static void RunStaticDestructors(void) {
  __ctordtorTerm();
}

unsigned long _System _DLL_InitTerm(unsigned long module, unsigned long flag) {
  (void)module;
  switch (flag) {
    case 0: /* The DLL is being loaded into a process. */
      if (_CRT_init() != 0)
        return 0;
      __ctordtorInit();
      atexit(RunStaticDestructors);
      return 1;
    case 1: /* The DLL is being freed; destructors ran from exit(). */
      _CRT_term();
      return 1;
  }
  return 0;
}
