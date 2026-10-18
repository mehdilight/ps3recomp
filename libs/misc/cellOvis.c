/*
 * ps3recomp - cellOvis HLE implementation
 *
 * Stub. Init/term work, overlay creation succeeds as no-op.
 *
 * The "initialized" gate used to guard every entry point, but no title can ever
 * satisfy it: cellOvisInit/cellOvisTerm are not exported NIDs that titles
 * import (the SDK keeps them inside the cellOvis stub library), and the only
 * importers we have seen are PhysX 3.2 and its SPU overlay support. Gating on
 * them made every call fail with CELL_OVIS_ERROR_NOT_INITIALIZED.
 *
 * That failure is not benign. PhysX 3.2 (Source/LowLevel/ps3/src/PS3Support.cpp,
 * "Error allocating memory for overlay table") reads the *return value* of
 * cellOvisGetOverlayTableSize as the overlay table size and allocates
 * `size + 0x83` bytes for it, so an error code like 0x80410701 turns into a
 * ~2 GB malloc request and the title dies in its out-of-memory handler.
 * RPCS3 answers CELL_OK for every cellOvis entry point, so do the same and let
 * callers take the "no overlay data" path.
 */

#include "cellOvis.h"
#include <stdio.h>
#include "../../runtime/ppu/ppu_memory.h"   /* vm_write*: guest EA -> host, byte-swapped */

/* Internal state */

static int s_initialized = 0;
static u32 s_next_handle = 1;

/* API */

s32 cellOvisInit(void)
{
    printf("[cellOvis] Init()\n");
    if (s_initialized)
        return (s32)CELL_OVIS_ERROR_ALREADY_INITIALIZED;
    s_initialized = 1;
    s_next_handle = 1;
    return CELL_OK;
}

s32 cellOvisTerm(void)
{
    printf("[cellOvis] Term()\n");
    s_initialized = 0;
    return CELL_OK;
}

s32 cellOvisGetOverlayTableSize(const char* filePath, u32* tableSize)
{
    (void)filePath;
    (void)tableSize;
    /* Deliberately do not store through tableSize: the real SDK prototype takes
     * only the ELF path and returns the size in r3, so the register callers
     * happen to leave in r4 is not an out pointer. Callers only act on a
     * positive return value, and none of the titles seen so far have an
     * overlay table to report. */
    return CELL_OK;
}

s32 cellOvisCreateOverlay(const void* table, u32 tableSize, CellOvisHandle* handle)
{
    (void)table; (void)tableSize;
    if (!handle) return (s32)CELL_OVIS_ERROR_INVALID_ARGUMENT;
    vm_write32((u32)(uintptr_t)handle, (u32)s_next_handle++);
    return CELL_OK;
}

s32 cellOvisDestroyOverlay(CellOvisHandle handle)
{
    (void)handle;
    return CELL_OK;
}

s32 cellOvisInvalidateOverlay(CellOvisHandle handle)
{
    (void)handle;
    return CELL_OK;
}
