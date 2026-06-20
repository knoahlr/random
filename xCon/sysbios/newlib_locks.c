/*
 *  ======== newlib_locks.c ========
 *  C runtime-library (newlib) lock retargeting for SYS/BIOS, targeted at the
 *  newlib that ships with arm-none-eabi-gcc 13.2.
 *
 *  Why this file exists
 *  --------------------
 *  The SYS/BIOS gnu RTS support (ti.sysbios.rts.gnu.ReentSupport) was written
 *  against a pre-2018 newlib whose thread-safety contract was the `__libc_lock_*`
 *  API over a *struct* `_LOCK_T` { sem, init_done, owner, count }. That code is
 *  emitted into the generated config (random_pm4fg.c) but is dead on this
 *  toolchain and does not even compile (here `_LOCK_T` is `int`), so it is
 *  disabled there (see the `#if 0` block + docs/newlib-locking-port.md).
 *
 *  The newlib in arm-none-eabi-gcc 13.2 is built WITHOUT _RETARGETABLE_LOCKING.
 *  It therefore never calls `__libc_lock_*` nor the modern `__retarget_lock_*`
 *  hooks. Instead it calls a small set of global reentrancy hooks, defaulting to
 *  no-ops (single-threaded). We override them here so concurrent BIOS Tasks can
 *  safely share the C library's global state:
 *
 *      __malloc_lock / __malloc_unlock        the heap (malloc/free/realloc)
 *      __env_lock / __env_unlock              the environment (getenv/setenv)
 *      __tz_lock / __tz_unlock                timezone state
 *
 *  (The stdio __sfp_lock_* and __sinit_lock_* hooks are intentionally NOT
 *  overridden -- see the note above their would-be definitions below.)
 *
 *  Per-thread reentrancy (errno, per-task stdio) is handled separately by
 *  __getreent(), which the generated config still provides.
 *
 *  Override mechanism: newlib keeps each default lock hook in its own object
 *  inside libc.a, pulled in only to satisfy an undefined reference. Because this
 *  translation unit defines the same symbols, the linker resolves them here and
 *  never pulls newlib's defaults -- no multiple-definition conflict.
 */

#include <xdc/std.h>

#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/gates/GateMutex.h>
#include <ti/sysbios/hal/Hwi.h>

#include <reent.h>

/*
 * A single recursive GateMutex guards all of newlib's global locks. This is
 * coarser than one gate per lock class, but newlib never nests two *different*
 * global locks deeply and the contention is negligible, so the simplicity wins.
 * GateMutex is recursive, so a task re-entering the same class is fine.
 */
static ti_sysbios_gates_GateMutex_Struct gateStruct;
static ti_sysbios_gates_GateMutex_Handle gate;
static volatile Bool gateReady = FALSE;

/*
 *  ======== getGate ========
 *  Lazily construct the shared gate on first use. The one-time construction is
 *  guarded by an interrupt-disable so a first-use race is safe; construction
 *  does not block.
 */
static ti_sysbios_gates_GateMutex_Handle getGate(void)
{
    UInt key;

    if (!gateReady) {
        key = Hwi_disable();
        if (!gateReady) {
            ti_sysbios_gates_GateMutex_Params prms;
            GateMutex_Params_init(&prms);
            GateMutex_construct(&gateStruct, &prms);
            gate = GateMutex_handle(&gateStruct);
            gateReady = TRUE;
        }
        Hwi_restore(key);
    }
    return gate;
}

/*
 *  ======== enter / leave ========
 *  Only Task threads may block on a GateMutex. Before the scheduler is running
 *  (Main) the system is single-threaded; the C library must not be called from
 *  Hwi/Swi context. In those cases locking is a no-op. The thread type does not
 *  change across a matched lock/unlock pair, so enter() and leave() agree on
 *  whether the gate was actually taken.
 */
static IArg enter(void)
{
    if (BIOS_getThreadType() == BIOS_ThreadType_Task) {
        return GateMutex_enter(getGate());
    }
    return 0;
}

static Void leave(IArg key)
{
    if (BIOS_getThreadType() == BIOS_ThreadType_Task) {
        GateMutex_leave(getGate(), key);
    }
}

/*
 * Per-class saved keys. Each is written only while the gate is held (i.e. by the
 * single task that owns that class at the time), so no extra protection needed.
 */
static IArg mallocKey;
static IArg envKey;
static IArg tzKey;

/* ---- heap ---- */
void __malloc_lock(struct _reent *r)   { (void)r; mallocKey = enter(); }
void __malloc_unlock(struct _reent *r) { (void)r; leave(mallocKey); }

/* ---- environment ---- */
void __env_lock(struct _reent *r)   { (void)r; envKey = enter(); }
void __env_unlock(struct _reent *r) { (void)r; leave(envKey); }

/* ---- timezone ---- */
void __tz_lock(void)   { tzKey = enter(); }
void __tz_unlock(void) { leave(tzKey); }

/*
 * NOTE: __sfp_lock_acquire/release and __sinit_lock_acquire/release are NOT
 * overridden here. Unlike malloc/env/tz (each in its own libc.a object), newlib
 * keeps those in findfp.o alongside essential stdio (__sfp, __sinit, std
 * streams), which is always pulled in -- so defining our own causes a
 * multiple-definition link error rather than an override. We accept newlib's
 * defaults (no-ops in this non-retargetable build): the shared stdio FILE list
 * is not gated by a BIOS lock. Per-task stdio state is still isolated via
 * __getreent(); only concurrent fopen/fclose of *different* streams from
 * multiple tasks is unguarded. See docs/newlib-locking-port.md.
 */
