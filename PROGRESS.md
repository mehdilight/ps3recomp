# PROGRESS -- Castlevania: Lords of Shadow 2 (BLES01644) port

Runtime code in this repository comments say *"see PROGRESS.md phase NN"*. This is
that file. The port is **not** upstream work: it lives on the fork branch
`los2-port` (`mehdilight/ps3recomp`), and the per-title artefacts (lifted PPU and
SPU output, probes, `stubs.cpp`, `spu_images/`, boot logs) are **not** in this
repository at all.

Two documents, and you want the other one as well as this one:

| File | What it is |
|---|---|
| `PROGRESS.md` (this file, version-controlled) | phase log, in-repo changes, the blocker in brief |
| `../cvlos2-recomp/HANDOFF.md` (working tree, not version-controlled) | **read this first**: live diagnosis, exact next steps, probe inventory, oracle facts, pitfalls |

The phase numbering in old comments refers to the working log kept alongside the
title build. Phases 1-18, 20 and 21 were the bring-up steps described under
"Milestones" below; only the two phases that runtime code still cites are
reproduced verbatim here.

---

## Safety note -- do not push this to upstream

`origin` is the fork (`git@github.com:mehdilight/ps3recomp.git`, branch
`los2-port`). `upstream` is `sp00nznet/ps3recomp`. The changes on this branch are
title-driven bring-up: several HLEs are deliberately sized to what BLES01644 asks
for and are not general. Anything here that is genuinely general (the RSX Metal
pump fix, `sys_spu_thread_bind_queue`, the `cellSync`/`cellSpurs` layouts) is
worth a separate, minimal PR against `upstream` -- but do that from a clean branch,
never by pushing `los2-port`.

## Milestones (phases 1-18, 20, 21 -- in order)

1. Title loads: PPU lift, function table, TOC, imports, `PARAM.SFO` ->
   `cellGame_init_from_paramsfo` in the title's own `main.cpp` so the title id is
   `BLES01644`; `/dev_hdd0` pointed at RPCS3's `dev_hdd0` via `PS3_HDD0_ROOT`.
2. `cellFs*` bring-up through the VFS root; `BROKEN_GAMEDATA` resolved.
3. Framebuffer / GCM init; the boot videos (`Legal.bik`, `Konami.bik`,
   `MercurySteam.bik`) play.
4. Frame clock: the RSX Metal backend's cross-thread pump handoff was deadlocking
   vblank/flip; fixed with a bounded 2 ms `dispatch_sync(main)` handoff plus a
   completion semaphore (`libs/video/rsx_metal_backend.m`).
5. Guest shader pipeline: 29 pipelines decompiled to MSL.
6. SPU execution: real lifted SPU code runs (up to then the runtime had no
   SPU execution at all), which is what made the SPURS work necessary.
7. SPURS1 bring-up: `cellSpursCreateTask` runs the zlib streaming task over
   `Data00.packed`; the earlier deadlock was an EventFlag heartbeat that never
   advanced.
8. `sys_spu_thread_bind_queue` / `unbind_queue` implemented (NID 193/194).
9. FMOD SPU handshake; user events resolved through the **per-thread** `evt_bind`
   table first, with the group port table as a fallback (the fallback alone gave a
   `CELL_ENOTCONN` wedge).
10. Embedded SPU ELFs located in the EBOOT (`0x00Fxxxxx`, 52 of them) and the
    first ones lifted: the FMOD codec/DSP images and the Bink/RAD image.
11. Image-id discipline established: `spu_channels.c` hardcodes id 1 (FMOD),
    `spu_dma.h` id 2 (zlib), traces id 3 (radg0) / 6, so ids are load-bearing and
    new images are named to sort last.
12. Wrong-image dispatch fixed (`s_img_bind[64]` / `img_bind()` /
    `spu_raw_image_id_for()`): the SPU thread group's image is identified by the
    fingerprint resolved at import, not by the LS entry address.
13. SPURS2 API surface added, then cut back: `cellSpursCreateTaskset2` creates the
    title's two PhysX tasksets (`0x40171280`, `0x40173C00`, spurs `0x4016F200`).
14. PhysX PPU side mapped: `cellOvis` overlay-table stubs were failing, and the
    caller sized an allocation from the failure (`libs/misc/cellOvis.c` now
    returns `CELL_OK` for all entry points).
15. Missing small exports implemented to get to zero unresolved NIDs
    (`cellSpursTasksetGetSpursAddress`, `cellSpursJoinTask2`,
    `_cellSpursLFQueuePopBody`, `cellSysutilGetBgmPlaybackStatus2`).
16. Dispatcher decode: `func_00A52AD8` (worker) and `func_00A52BF4` (loop) fully
    transcribed; `func_00861318` is the exit check.
17. Physical build of the PhysX SPU task image (`0x00FBF280`, image 7) and its
    lift (97 functions, 97.3% text).
18. Revision-2 task API (`_cellSpursTaskAttribute2Initialize`, `cellSpursCreateTask2`)
    implemented, which is what finally let the title *create* its PhysX SPU tasks
    and dispatch them. Run has **0 unresolved NIDs**.

### Phase 19 -- SPURS queue waiter protocol

> "...the waiter half of the protocol is known (see PROGRESS.md phase 19): the
> 16-byte group at queue+0x20 is a ring of blocked task ids, byte[0] the count and
> byte[13] the cursor."

Used by `cellSpursQueuePushBody` (`libs/spurs/cellSpurs.c`) for its
`SPURS_QUEUE_PUSH=1` experiment: push into a candidate slot, wake the registered
waiter, and let the consumer's own DMA name the right slot. Kept because it is
still the only way to learn the data half of the SPURS-kernel queue.

### Phase 22 -- the counter-ring derivation (SUPERSEDED for the LFQueue)

> "...non-empty when (W1-W0) mod 2*W3 != 0, slot = index mod W3."

Derived by reading a consumer's decision block backwards, and it is what
`spu_spurs_lfqueue_push_body` (`runtime/spu/spu_workload.c`) and
`_cellSpursLFQueuePopBody` (`libs/spurs/cellSpurs.c`) still implement, reading
`W0 @0x00`, `W1 @0x04`, `W3 = depth @0x0C`.

**This model is wrong for `CellSpursLFQueue` and was replaced on 2026-10-18.** The
consumer at LS `0x3EEC` is recompiled SPU code that `GETLLAR`s the 128-byte line
itself and tests the word at `+0x24` for a direction of 3, then uses the
`push2`/`pop2` groups at `+0x30`/`+0x50` -- i.e. a real big-endian
`CellSyncLFQueue`, not a counter ring. `_cellSpursLFQueueInitialize` now writes
that faithful line. The evidence for the counter ring came from a *different*
queue family (`_cellSpursQueueInitialize`); see "Two protocols share the same
128-byte line" in `HANDOFF.md`. **The pop/push pair has not been migrated yet**,
so the tree is mid-migration and the LFQueue pop currently fails its own sanity
check.

---

## Current blocker (2026-10-18, commit 3b615dc)

The title boots to the first PhysX simulation step and stops there permanently.
`func_002EE150` (`_GetPhysicsResults`) polls `func_00856AE4`, which returns 0
because the ready flag `*(u8*)0x40B3FDE0` is never set: the dispatcher batch never
completes.

The dispatcher worker pops `{taskId, batchDescriptor}` from LFQueue `0x4094A700`;
`taskId == -1` means "no work", otherwise it joins that task and increments
`desc->completed`, and calls `desc->vtable[0x1C]()` when all of `desc->total` are
done. The SPU tasks never push, and the queue-side HLEs and the SPU-side protocol
do not agree on the line layout. That is the whole blocker, and it is a layout
bug, not a missing feature.

Next step is written out in `../cvlos2-recomp/HANDOFF.md` section 4: migrate
`_cellSpursLFQueuePopBody` to the faithful line, make its empty marker `-1`
(the worker would otherwise join task id 0 for ever), and expect
`[lfq] BADHDR q=0x4094A700` on the first run as the migration becomes live.

## In-repo changes on this branch

Commits on `los2-port` touch the runtime and libraries only:

* `libs/spurs/cellSpurs.{c,h}` -- the revision-2 task API, `cellSpursJoinTask2`,
  `cellSpursTasksetGetSpursAddress`, `_cellSpursLFQueuePopBody`, and the
  `_cellSpursLFQueueInitialize` rewrite to the faithful big-endian line.
* `libs/misc/cellOvis.c` -- overlay-table entry points are `CELL_OK`.
* `libs/system/cellSysutil.{c,h}` -- `cellSysutilGetBgmPlaybackStatus2`.
* `libs/video/rsx_metal_backend.m` -- bounded pump handoff.
* `runtime/spu/*` -- taskset/parked-task signalling, per-thread `evt_bind`,
  raw-image dump, lifted-thread plumbing.
* `runtime/ppu/*`, `runtime/syscalls/*` -- `sys_spu_thread_bind_queue`/`unbind`,
  HLE loader/sysprx handling, guest-stack backtrace helper.

Temporary diagnostic probes live in the title tree, not here, and their inventory
is in `HANDOFF.md` section 6.
