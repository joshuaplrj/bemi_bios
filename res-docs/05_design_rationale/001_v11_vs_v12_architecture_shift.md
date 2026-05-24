# Design Rationale -- v1.1 -> v1.2 Architecture Shift

**Date:** 2024-11-10
**Decision:** Keep x86 decoder -> "Weaponized x86 Bemi" (v1.2)
**Replaces:** v1.1 "Pure RISC" architecture

---

## Context

The BEMI architecture went through two major design iterations:

- **v1.0** (abandoned): Remove decoder, scale ROB -- failed due to O(n?) CAM scaling
- **v1.1** (published in Chapter 7): Remove decoder, increase issue width + moderate ROB scaling -- 5.2x IPC, 36 threads, 65W
- **v1.2** (published in Chapter 8): Keep decoder, replace back-end with dense RISC units -- 1.3x IPC, 144 threads, 85W
- **v1.3** (software prototype): Rust DBT translator + executor (`hybrid_bemi/`); proves v1.2 throughput model with real x86 code execution

The shift from v1.1 to v1.2 was driven by a single insight discovered during silicon area modeling.

## The Silicon Area Revelation

In early October 2024, we built a detailed area model for a 6nm CMOS process. The model estimated die area for three execution back-end designs:

| Component | Area (mm?) |
|---|---|
| x86 execution back-end (full) | 2.25 mm? |
| x86 front-end decoder | 0.75 mm? |
| RISC execution unit (1 ALU + 1 FPU + LS) | 0.15 mm? |
| ARM M1 Firestorm core (estimated) | ~1.8 mm? |
| Intel Golden Cove P-core (estimated) | ~3.0 mm? |

**The key ratio:** `0.15 mm? per RISC unit vs 2.25 mm? for an x86 back-end`. This is a **15x density advantage**.

The x86 back-end is ~2.25 mm? because:
- Complex scheduler (O(n?) CAM)
- 10+ execution ports with bypass networks
- Large physical register file (300+ entries)
- Instruction wakeup logic (broadcast buses)
- Store forwarding and memory ordering logic

The RISC back-end is ~0.15 mm? because:
- Simple 2-issue scheduler (linear CAM, small)
- 3 execution ports
- 64-entry register file (smaller because RISC has fewer architectural registers)
- No store forwarding (delegated to cache hierarchy)

## The Debate

### Case for v1.1 (Pure RISC):

**Advocates argued:**
- Clean-slate design with no x86 legacy baggage
- Lower TDP (65W vs 85W)
- Higher single-thread IPC (5.2 vs 1.3)
- Better for a future-native BEMI ecosystem

**Problems:**
- 36 threads is only 1.5x the x86 baseline of 24 -- not compelling for a new architecture
- Requires a full software ecosystem (compiler, toolchain, OS)
- No backward compatibility without a separate x86 core
- 5.2x IPC is only achievable with extremely wide issue (16-wide) which is hard to implement at 5 GHz

### Case for v1.2 (Weaponized):

**Advocates argued:**
- 144 threads = 6x the x86 baseline without the decoder -- and 7.8x with fusion
- Backward compatible at the hardware level (boots legacy x86 OSes)
- Smaller engineering risk (uses existing x86 front-end and toolchains)
- The fusion bonus sweetener is free (the decoder already does it)

**Problems:**
- Higher TDP (85W -- though still 15W below 100W x86)
- Lower single-thread performance (1.3x vs 5.2x)
- Architectural kludge (keeping the x86 decoder feels impure)
- The interconnect problem (15 units sharing a cache)

## The Deciding Factor

The decision came down to **market viability**. A 7.8x multi-thread speedup on existing x86 software is a product you can sell today. A 5.2x single-thread speedup on new BEMI-native software is a product you need to build an ecosystem for.

The v1.1 architecture was not abandoned -- it was deferred. If BEMI gains market share, a future v2.0 may be a pure RISC design with a software x86 compatibility layer (like Apple's Rosetta but baked into the OS).

## Current Plan

- **v1.2 (Weaponized):** First silicon. Immediate compatibility. Ship to OEMs.
- **v1.3 (Software DBT):** Rust prototype that proves the concept on existing x86 hardware. Developer research platform.
- **v2.0 (Pure RISC):** Future architecture. Higher single-thread IPC. Targets the post-x86 transition.

## Residual Tension

Team members disagree on whether v1.2's path dependency is harmful. By keeping the x86 decoder, we are locking ourselves into x86's front-end limitations (4-wide decode, micro-op cache complexity) for the foreseeable future. The v1.1 team considers this a strategic error.

**Compromise:** The v1.2 design includes a "legacy mode disable" EFI variable. If set, the decoder is powered off and the RISC back-ends receive instructions directly from a BEMI-native front-end. This allows v2.0 experimentation on v1.2 silicon.

## References

- Area model spreadsheet: `06_references/6nm_area_model.ods`
- v1.1 spec (archived): `06_references/v11_architecture_spec_draft.md`
- v1.2 spec (current): `docs/08_weaponized_x86_bemi.md`

