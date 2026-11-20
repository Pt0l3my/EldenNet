/* SPDX-License-Identifier: MIT */
/*
 * graceui_sys.h: how a mod finds GraceUI (D70, D136), header-only C, compiled into the mod. Call it from a thread of the
 * mod's own, never from DllMain: GraceUI's first request takes a mutex and starts a thread, which must not happen under
 * the loader's lock.
 *
 *   GraceUiSysFound found = {sizeof found};
 *   const GraceUiApi1* api = graceui_sys_find(GRACEUI_SYS_LOAD, GRACEUI_SYS_TIMEOUT_MS, &found);
 *   if (api != NULL && GRACEUI_API1_COVERS(api, connect)) { ... api->connect(&request, &answer) ... }
 *
 * Each time it looks, it looks three ways, in this order: the module named graceui.dll; every module loaded in the
 * process that exports GraceUI_RequestApi, the first whose table answers (so a renamed copy, or a second copy that
 * idles, is no obstacle); and, if the mod allows it (GRACEUI_SYS_LOAD), GraceUI's canonical copy, GraceUI\graceui.dll in
 * the folder of the game's executable, which it loads. On each it calls GraceUI_RequestApi(1). It looks again every
 * 250 ms for at most the time it is given (EST). Null means no GraceUI that serves ABI 1: the mod's UI is off, never an
 * error dialog. Kernel32 is all it needs.
 */
#ifndef GRACEUI_SYS_H
#define GRACEUI_SYS_H

#include <windows.h>

#include <tlhelp32.h>

#include "graceui.h"

/* C++ code that includes this header keeps -Wold-style-cast: the helper's casts are named ones there.
 * SAFETY: the helper reinterprets only GetProcAddress's answer, as the export's own type, and a module's handle as an
 * address inside it, each where it is used, with its reason there. */
#ifdef __cplusplus
#define GRACEUI_SYS_REINTERPRET(type, value) reinterpret_cast<type>(value)
#define GRACEUI_SYS_STATIC(type, value) static_cast<type>(value)
extern "C" {
#else
#define GRACEUI_SYS_REINTERPRET(type, value) ((type)(value))
#define GRACEUI_SYS_STATIC(type, value) ((type)(value))
#endif

#define GRACEUI_SYS_POLL_MS 250u      /* between two looks */
#define GRACEUI_SYS_TIMEOUT_MS 30000u /* the default bound on the looking (EST) */

#define GRACEUI_SYS_LOAD 1u /* flag: also load GraceUI's canonical copy if no loaded module answers */
#define GRACEUI_SYS_STOP_IF_ABSENT 2u /* flag: stop looking once a look finds GraceUI not installed (D164): nothing
                                         answered, no module named graceui.dll, no GraceUI\graceui.dll */

#define GRACEUI_SYS_BY_NAME 1u /* how it was found */
#define GRACEUI_SYS_BY_SCAN 2u
#define GRACEUI_SYS_BY_PATH 3u

typedef struct GraceUiSysFound {
  uint32_t size;      /* sizeof(GraceUiSysFound) */
  uint32_t how;       /* GRACEUI_SYS_BY_*, or 0 if nothing answered */
  uint32_t version;   /* the GraceUI found: its GraceUI_Version(), or 0 */
  uint32_t attempts;  /* how many times the helper looked */
  HMODULE module;     /* the module whose table answered */
  uint32_t installed; /* at the last look: 1 if a module named graceui.dll was loaded or GraceUI\graceui.dll was in
                         the game's folder, else 0 (D164): with nothing answering, 0 means GraceUI is not installed */
} GraceUiSysFound;

/* ABI 1's table from `module`, if it exports GraceUI_RequestApi and serves ABI 1; its version in *version. */
static inline const GraceUiApi1* graceui_sys_table_of(HMODULE module, uint32_t* version) {
  FARPROC request_export = NULL;
  FARPROC version_export = NULL;
  GraceUiRequestApiFn request = NULL;
  GraceUiVersionFn version_of = NULL;
  const void* table = NULL;
  if (module == NULL || (request_export = GetProcAddress(module, "GraceUI_RequestApi")) == NULL) {
    return NULL;
  }
  /* SAFETY: graceui.h gives the export of that name this type; void (*)(void) is the one type every compiler lets a
   * function pointer pass through. */
  request = GRACEUI_SYS_REINTERPRET(GraceUiRequestApiFn, GRACEUI_SYS_REINTERPRET(void (*)(void), request_export));
  table = request(GRACEUI_ABI_MAJOR);
  if (table == NULL) {
    return NULL; /* an idle copy (a second one, D72), or a GraceUI without ABI 1 */
  }
  if (version != NULL && (version_export = GetProcAddress(module, "GraceUI_Version")) != NULL) {
    /* SAFETY: as above, for GraceUI_Version. */
    version_of = GRACEUI_SYS_REINTERPRET(GraceUiVersionFn, GRACEUI_SYS_REINTERPRET(void (*)(void), version_export));
    *version = version_of();
  }
  return GRACEUI_SYS_STATIC(const GraceUiApi1*, table);
}

/* The second way: every loaded module that exports GraceUI_RequestApi, in load order, but `skip`. Each is held by a
 * reference while it is asked, so that another thread cannot unload it meanwhile. */
static inline const GraceUiApi1* graceui_sys_scan(HMODULE skip, GraceUiSysFound* found) {
  const GraceUiApi1* api = NULL;
  HANDLE snapshot = INVALID_HANDLE_VALUE;
  MODULEENTRY32W entry;
  BOOL more = FALSE;
  int tries = 0;
  for (tries = 0; tries < 8; ++tries) {
    snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, 0);
    if (snapshot != INVALID_HANDLE_VALUE || GetLastError() != ERROR_BAD_LENGTH) {
      break; /* ERROR_BAD_LENGTH: modules were loading; Windows asks for another try */
    }
  }
  if (snapshot == INVALID_HANDLE_VALUE) {
    return NULL;
  }
  ZeroMemory(&entry, sizeof entry);
  entry.dwSize = sizeof entry;
  for (more = Module32FirstW(snapshot, &entry); more && api == NULL; more = Module32NextW(snapshot, &entry)) {
    HMODULE held = NULL;
    /* SAFETY: a module's handle is its base address, an address inside it, which GetModuleHandleExW only reads to find
     * the module; the reference it takes keeps the module loaded until FreeLibrary. */
    if (entry.hModule == skip ||
        !GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, GRACEUI_SYS_REINTERPRET(LPCWSTR, entry.hModule),
                            &held)) {
      continue;
    }
    api = graceui_sys_table_of(held, found != NULL ? &found->version : NULL);
    if (api != NULL && found != NULL) {
      found->how = GRACEUI_SYS_BY_SCAN;
      found->module = held;
    }
    FreeLibrary(held); /* the active copy has pinned itself: its table outlives this reference */
  }
  CloseHandle(snapshot);
  return api;
}

/* GraceUI\graceui.dll in the folder of the process's executable (the game's folder), into `path`: 0 if it does not fit. */
static inline int graceui_sys_path(wchar_t* path, DWORD capacity) {
  const wchar_t tail[] = L"GraceUI\\graceui.dll";
  const DWORD tail_units = GRACEUI_SYS_STATIC(DWORD, sizeof tail / sizeof tail[0]); /* with its NUL */
  DWORD length = GetModuleFileNameW(NULL, path, capacity);
  DWORD i = 0;
  if (length == 0 || length >= capacity) {
    return 0;
  }
  while (length > 0 && path[length - 1] != L'\\' && path[length - 1] != L'/') {
    --length;
  }
  if (length == 0 || length + tail_units > capacity) {
    return 0;
  }
  for (i = 0; i < tail_units; ++i) {
    path[length + i] = tail[i];
  }
  return 1;
}

/* The third way: GraceUI\graceui.dll in the game's folder, loaded. */
static inline const GraceUiApi1* graceui_sys_load(GraceUiSysFound* found) {
  wchar_t path[2048];
  HMODULE module = NULL;
  const GraceUiApi1* api = NULL;
  if (!graceui_sys_path(path, 2048)) {
    return NULL;
  }
  module = LoadLibraryExW(path, NULL, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
  if (module == NULL) {
    return NULL;
  }
  api = graceui_sys_table_of(module, found != NULL ? &found->version : NULL);
  if (api == NULL) {
    FreeLibrary(module); /* not a GraceUI that serves ABI 1 here, or an idle copy: let it go */
    return NULL;
  }
  if (found != NULL) {
    found->how = GRACEUI_SYS_BY_PATH;
    found->module = module;
  }
  return api;
}

/* Looks once, the three ways (GRACEUI_SYS_LOAD allowing the third). */
static inline const GraceUiApi1* graceui_sys_find_once(uint32_t flags, GraceUiSysFound* found) {
  const HMODULE named = GetModuleHandleW(L"graceui.dll");
  const GraceUiApi1* api = NULL;
  if (found != NULL) {
    ++found->attempts;
  }
  api = graceui_sys_table_of(named, found != NULL ? &found->version : NULL);
  if (api != NULL) {
    if (found != NULL) {
      found->how = GRACEUI_SYS_BY_NAME;
      found->module = named;
      found->installed = 1u; /* a loaded graceui.dll is installed (D164; EldenNet's part 12 found this branch left 0) */
    }
    return api;
  }
  api = graceui_sys_scan(named, found);
  if (api == NULL && (flags & GRACEUI_SYS_LOAD) != 0) {
    api = graceui_sys_load(found);
  }
  if (found != NULL) {
    wchar_t path[2048];
    found->installed = (api != NULL || named != NULL ||
                        (graceui_sys_path(path, 2048) && GetFileAttributesW(path) != INVALID_FILE_ATTRIBUTES))
                           ? 1u
                           : 0u;
  }
  return api;
}

/* Looks every GRACEUI_SYS_POLL_MS until GraceUI answers or `timeout_ms` has passed (0 looks once). */
static inline const GraceUiApi1* graceui_sys_find(uint32_t flags, uint32_t timeout_ms, GraceUiSysFound* found) {
  const ULONGLONG start = GetTickCount64();
  const GraceUiApi1* api = NULL;
  if (found != NULL) {
    found->how = 0;
    found->version = 0;
    found->attempts = 0;
    found->module = NULL;
    found->installed = 0;
  }
  for (;;) {
    GraceUiSysFound look;
    GraceUiSysFound* into = found;
    if (into == NULL) {
      ZeroMemory(&look, sizeof look);
      into = &look;
    }
    api = graceui_sys_find_once(flags, into);
    if (api != NULL || GetTickCount64() - start >= timeout_ms) {
      return api;
    }
    if ((flags & GRACEUI_SYS_STOP_IF_ABSENT) != 0 && into->installed == 0) {
      return NULL; /* nothing answered, no graceui.dll is loaded and none is in the game's folder: not installed */
    }
    Sleep(GRACEUI_SYS_POLL_MS);
  }
}

#ifdef __cplusplus
}
#endif

#endif /* GRACEUI_SYS_H */
