# 09. The Bemi BIOS -- Ring -1 Firmware Design

## 9.1 The Firmware Architecture Philosophy

Standard BIOS and UEFI firmware operates at **Ring 0** -- the same privilege level as the operating
system kernel. This means the BIOS must hand off control to the OS completely; once the OS is running,
the firmware has no authority over it.

The Bemi BIOS operates at **Ring -1** -- a privilege level *below* Ring 0, implemented via the
CPU's hardware virtualization extensions (VMX on Intel, SVM on AMD). Ring -1 is the **hypervisor
privilege level** -- used by VMware, Hyper-V, and KVM to host virtual machines.

By running the Bemi translation layer at Ring -1, the BIOS achieves the following:

1. **Invisible to the OS:** The legacy operating system believes it is running on native x86 hardware.
   It cannot detect or interact with the Ring -1 DBT layer.
2. **Full interception authority:** Every Ring 0 (kernel) operation -- every system call, every
   interrupt, every privileged instruction -- passes through Ring -1 before execution.
3. **Hardware-locked performance:** The trace cache is locked into L3 cache during BIOS initialization,
   ensuring trace-cache hits never miss to DRAM.

---

## 9.2 Boot Modes: The EFI Variable Decision

The Bemi BIOS reads a specific UEFI EFI variable at boot time to determine which execution mode
to use. The prototype implementation is in `bemi_bios/bios_prototype.py`.

### Mode A: Native Bemi Boot

EFI variable: `BEMI_NATIVE=1`

```
[BIOS] POST: 36 Cores, L1/L2/L3 Caches OK.
[BIOS] EFI variable: BEMI_NATIVE=1
[BIOS] DBT Translation bounds: DISABLED
[BIOS] Handing execution to Native Bemi OS bootloader...
```

No Ring -1 hypervisor is activated. The BIOS hands control directly to the Bemi-native OS EFI
bootloader. This is the highest-performance path -- zero firmware overhead after boot.

### Mode B: Legacy x86 Boot

EFI variable: `BEMI_NATIVE=0` (or absent)

```
[BIOS] POST: 36 Cores, L1/L2/L3 Caches OK.
[BIOS] Scanning boot devices...
[BIOS] Detected: legacy_x86_bootloader.efi  [or MBR]
[BIOS] Legacy OS Detected. Initializing Ring -1 DBT Translator...
[BIOS] -> Hardware TSO hooks: ENABLED
[BIOS] -> Macro-Op Fusion pipeline: ENABLED
[BIOS] -> Shadow APIC & CR3 Paging: ENABLED
[BIOS] Weaponized x86 Translation Layer is LOCKED beneath OS visibility.
[BIOS] Handing execution to Legacy OS via Translation Matrix...
```

The Ring -1 hypervisor is initialised *before* the OS bootloader runs. By the time the OS kernel
starts executing, the entire DBT infrastructure is already in place. The OS boots into a
fully-intercepted execution environment without knowing it.

---

## 9.3 MS-DOS 1.0 as the Legacy OS Case Study

MS-DOS 1.0 was chosen as the primary legacy OS benchmark workload for two reasons:

1. **It is open-source** -- the codebase is publicly available for study, making the simulation
   parameters verifiable against real source code.
2. **It is minimal** -- the kernel is approximately **6,400 bytes (6.4 KB)** (`IBMDOS.COM`).
   This means the entire kernel fits in the Ring -1 DBT trace cache with room to spare.

### MS-DOS 1.0 Technical Profile

| Property | Value |
|---|---|
| Kernel size | ~6.4 KB (`IBMDOS.COM`) |
| Source code | ~4,000 lines of 8086 assembly |
| System call interface | INT 21h, functions 00h through 2Dh (45 functions) |
| BIOS dependencies | INT 10h (video), INT 13h (disk), INT 16h (keyboard) |
| Scheduling | None -- single-tasking, cooperative |
| Memory model | Segmented (CS:IP, DS, ES, SS) |
| File system | FAT (File Allocation Table) |

### System Call Flow in MS-DOS 1.0

Every MS-DOS service request follows this path:

```
User program executes: INT 21h (with AH = function code)
     |
     v
8086 CPU saves FLAGS, CS, IP to stack  [3 memory writes]
     |
     v
CPU clears IF, TF flags
     |
     v
CPU reads IVT[21h * 4] to get CS:IP of INT 21h handler  [1 memory read]
     |
     v
CPU loads new CS:IP, begins executing DOS handler
     |
     v  [30% of INT 21h calls relay to BIOS]
     v
BIOS INT 10h/13h/16h: Another 51-cycle INT sequence begins
     |
     v
Handler executes, returns via IRET  [reverse of INT]
```

**Total cost on native x86 8086:** The `INT` instruction takes **51 clock cycles** for the
mandatory stack saves and IVT lookups. This is a documented hardware constant, derived from
the 8086 timing tables.

For a typical interactive DOS session:
- ~200,000 INT 21h calls (file I/O, console input/output)
- 30% of these relay to BIOS sub-interrupts: 60,000 additional INT calls
- ~50,000 hardware timer interrupts (INT 8h) for scheduling

**Total interrupt events: ~310,000 at 51+ cycles each = 15.8+ million cycles of pure interrupt overhead.**

---

## 9.4 The Ring -1 Trace Cache Mechanism

When the Bemi BIOS boots with a legacy OS:

### Step 1: Kernel Pre-Translation (during BIOS POST, invisible to OS)

The Ring -1 DBT reads the entire 6.4 KB DOS kernel from disk and pre-translates it into Bemi
Macro-Op sequences. These translated sequences are stored in the **trace cache** -- a dedicated
region of L3 cache that is hardware-protected from eviction by the OS.

Because the DOS kernel is only 6.4 KB:
- Original x86 bytes: 6,400 bytes
- Bemi Macro-Op translation (at ~1.5x expansion): ~9,600 bytes = ~9.4 KB
- An L3 cache line is 64 bytes -> 150 cache lines
- This is negligible relative to the 32 MB total L3 cache

The entire DOS kernel lives in L3 cache permanently, at zero eviction risk.

### Step 2: INT 21h Interception

When the legacy OS executes `INT 21h`, the Ring -1 hypervisor intercepts the instruction
*before* the 8086 INT sequence begins. The interception path:

```
Guest OS executes: INT 21h
     |
     v  [Ring -1 VM exit triggered -- hardware mechanism]
     v
Bemi BIOS Ring -1 handler wakes (cost: ~4 cycles -- VMX/SVM exit overhead)
     |
     v
Trace cache lookup: hash(21h, AH_value) -> Bemi Macro-Op trace entry
     |
     v  [L3 cache hit: 8 cycles]
     v
Bemi execution engine runs the pre-translated handler (1-cycle decode, 1.3x fusion)
     |
     v
Result written to guest registers, control returned to OS (VMX/SVM entry: ~4 cycles)
```

**Total Ring -1 intercepted cost: ~4 + 8 + execution + 4 = ~16+ cycles plus execution.**

But the key saving is the INT setup overhead:
- Native x86: 51 cycles *before* the handler even starts executing
- Bemi BIOS: 8 cycles (trace cache hit L3 latency) to look up the handler

For the non-execution portion alone (setup overhead), the savings are: `51 - 8 = 43 cycles per INT`.

### Step 3: BIOS Relay Elimination

On native x86, 30% of INT 21h calls relay to BIOS interrupts (INT 10h, 13h, 16h). Each relay
costs another 51 cycles. On native x86, a relayed call costs `51 + 51 = 102 cycles`.

On Bemi BIOS, both the DOS handler and the BIOS handler are pre-translated and in the trace cache.
The relay is a single internal trace-cache lookup -- no second INT instruction is executed.
Cost: still just the single 8-cycle trace-cache hit for the DOS entry, plus the BIOS trace
execution inline.

**Per-call saving for relayed calls: `102 - 8 = 94 cycles`.**

### Step 4: Hardware Timer Interrupts

Hardware timer interrupts (INT 8h) are handled by the Shadow APIC. The Bemi BIOS installs a
shadow APIC register bank that intercepts the APIC timer signal before it generates a Ring 0
interrupt in the guest OS.

The timer handler (which merely increments the system clock and dispatches any pending tasks)
is pre-translated in the trace cache. The pre-vectored cost is ~20 cycles (including Ring -1
VMX exit/entry overhead), compared to the native 131 cycles (51 INT cycles + ~80 cycles of
handler code running at 4-cycle CISC decode).

---

## 9.5 The Benchmark Parameters and Results

From `bemi_bios/legacy_os_benchmark.py`:

| Environment | INT 21h Cost | HW Int Cost | Backend Throughput | Total Ticks |
|---|---|---|---|---|---|
| Legacy BIOS + Native x86 (24 threads) | 51 cycles | 131 cycles | 6.0 ops/cyc | 3,301,666 |
| Bemi BIOS + Ring -1 DBT + Weaponized Bemi (144 threads, v1.2) | 8 cycles | 20 cycles | 46.8 ops/cyc | 55,555 |
| Bemi BIOS + v1.3 ROB Density (84 threads) | 8 cycles | 20 cycles | 27.3 ops/cyc | 95,238 |

**MS-DOS 1.0 runs 59.43x faster on the Bemi BIOS (v1.2).**
**With v1.3 ROB Entry Density (84 threads): 34.7x faster** (backend TP = 84/4 x 1.3 = 27.3).

This speedup is **fully emergent** from the simulation model -- no multiplier is hardcoded.
The 59.43x comes from the compounding of:
- INT cost reduction: 51 -> 8 cycles (6.375x for direct calls)
- BIOS relay elimination: 102 -> 8 cycles (12.75x for relayed calls)
- Hardware timer reduction: 131 -> 20 cycles (6.55x)
- Backend throughput: x86 = 24/4 = 6.0, Bemi v1.2 = 144/4 x 1.3 = 46.8 (7.8x), Bemi v1.3 = 84/4 x 1.3 = 27.3 (4.55x)

These factors multiply non-linearly because they all apply simultaneously, producing the
observed ~59x aggregate improvement.

---

## 9.6 Future BIOS Development Roadmap

The `bemi_bios/TODO.md` outlines the remaining engineering work:

**Phase 1 -- Firmware Consolidation**
- UEFI 2.8 memory map compliance (ensure OS payload compatibility)
- SMM (System Management Mode) sandboxing (prevent SMM from pausing the guest OS)
- Secure Boot: bridge legacy x86 Microsoft keys with native Bemi keys

**Phase 2 -- Translation Optimisations**
- Page Table Walk Acceleration: shadow page tables for CR3 operations
- APIC interrupt routing latency optimisation

**Phase 3 -- Hardware Emulation**
- AVX-512 and CPUID spoofing: broadcast genuine x86 CPUID feature flags while internally
  routing those code paths through Bemi's passthrough infrastructure

