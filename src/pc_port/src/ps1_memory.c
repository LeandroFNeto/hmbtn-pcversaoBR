/* Maps the PS1 address space and decodes 24-bit links. Kept free of the PSY-Q headers:
 * windows.h defines its own RECT and u_long. */

#define PC_PORT_NO_PSYQ_TYPES
#include <ps1_memory.h>

#include <SDL.h>

#include <string.h>

#ifdef _WIN32
#include <windows.h>
#endif

static int map_range(uintptr_t base, size_t size, const char *name) {
#ifdef _WIN32
    void *got;

    got = VirtualAlloc((void *)base, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (got == (void *)base) {
        return 0;
    }
    if (got != NULL) {
        VirtualFree(got, 0, MEM_RELEASE);
    }
    SDL_Log("cannot map %s at 0x%08X (%u bytes): error %lu", name, (unsigned)base, (unsigned)size,
            (unsigned long)GetLastError());
    return -1;
#else
    SDL_Log("cannot map %s at 0x%08X: no mapping for this platform", name, (unsigned)base);
    (void)size;
    return -1;
#endif
}

#ifdef _WIN32
static LONG WINAPI log_crash(EXCEPTION_POINTERS *info) {
    EXCEPTION_RECORD *record;

    record = info->ExceptionRecord;
    SDL_Log("host fault: code 0x%08lX at 0x%08X, access 0x%08X", (unsigned long)record->ExceptionCode,
            (unsigned)(uintptr_t)record->ExceptionAddress,
            record->NumberParameters >= 2 ? (unsigned)record->ExceptionInformation[1] : 0u);
    return EXCEPTION_CONTINUE_SEARCH;
}
#endif

void host_install_crash_logger(void) {
#ifdef _WIN32
    SetUnhandledExceptionFilter(log_crash);
#endif
}

#ifdef _WIN32
static HANDLE watched_thread;
static volatile const int *watched_counter;

/* Logs where the main thread is: eip, then the return addresses on its frame chain. */
static void log_main_stack(const char *label, int vblank) {
    CONTEXT ctx;
    BOOL got;
    DWORD frames[12];
    DWORD ebp;
    int n;
    int i;

    SuspendThread(watched_thread);
    memset(&ctx, 0, sizeof(ctx));
    ctx.ContextFlags = CONTEXT_CONTROL;
    got = GetThreadContext(watched_thread, &ctx);
    n = 0;
    if (got) {
        ebp = ctx.Ebp;
        while (n < 12 && ebp != 0 && !IsBadReadPtr((void *)(uintptr_t)ebp, 8)) {
            frames[n++] = ((DWORD *)(uintptr_t)ebp)[1];
            ebp = ((DWORD *)(uintptr_t)ebp)[0];
        }
    }
    ResumeThread(watched_thread);
    if (got) {
        SDL_Log("host %s at vblank %d, eip 0x%08lX", label, vblank, (unsigned long)ctx.Eip);
        for (i = 0; i < n; i++) {
            SDL_Log("host %s:   called from 0x%08lX", label, (unsigned long)frames[i]);
        }
    }
}

static DWORD WINAPI watchdog(LPVOID arg) {
    int last;
    int stalled_ms;
    int elapsed_ms;
    int sample;
    const char *env;

    (void)arg;
    env = getenv("HMBTN_SAMPLE_STACK");
    sample = env != NULL && strcmp(env, "1") == 0;
    last = *watched_counter;
    stalled_ms = 0;
    elapsed_ms = 0;
    for (;;) {
        Sleep(250);
        elapsed_ms += 250;
        if (sample && elapsed_ms % 3000 == 0) {
            log_main_stack("sample", *watched_counter);
        }
        if (*watched_counter != last) {
            last = *watched_counter;
            stalled_ms = 0;
            continue;
        }
        stalled_ms += 250;
        if (stalled_ms % 2000 == 0) {
            log_main_stack("stall", last);
        }
    }
    return 0;
}
#endif

void host_start_watchdog(const int *vblank_counter) {
#ifdef _WIN32
    DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &watched_thread, 0, FALSE,
                    DUPLICATE_SAME_ACCESS);
    watched_counter = vblank_counter;
    CreateThread(NULL, 0, watchdog, NULL, 0, NULL);
#else
    (void)vblank_counter;
#endif
}

int ps1_memory_map(void) {
    if (map_range(PS1_RAM_BASE, PS1_RAM_SIZE, "PS1 RAM") != 0) {
        return -1;
    }
    if (map_range(PS1_SCRATCHPAD_BASE, 0x1000, "PS1 scratchpad") != 0) {
        return -1;
    }
    return 0;
}

void *ps1_link_to_ptr(uint32_t link) {
    link &= PS1_LINK_END;
    if (link == 0 || link == PS1_LINK_END) {
        return NULL;
    }
    if (link < PS1_LINK_RAM_END) {
        return (void *)(uintptr_t)(PS1_RAM_BASE + link);
    }
    if (link >= PS1_LINK_IMAGE_START && link < PS1_LINK_IMAGE_END) {
        return (void *)(uintptr_t)link;
    }
    return NULL;
}

uint32_t ps1_ptr_to_link(const void *p) {
    return (uint32_t)(uintptr_t)p & PS1_LINK_END;
}
