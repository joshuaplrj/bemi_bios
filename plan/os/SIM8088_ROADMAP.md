# 8088 Simulator – Build & Execution Roadmap

## Objective
Restore the `sim8088` binary to a buildable, runnable state and incrementally
improve the 8088 interpreter so it can execute meaningful 8086/8088 code
(the `bench` program and eventually a real boot sector / MS-DOS 1.0).

## Current State (discovered on `main`)
- **`dbt/Cargo.toml` is missing**, so `cd dbt && cargo build` fails.
  An archived manifest exists at
  `docs/archive/code_recovery/recovered_deep_bemi/dbt/cargo.toml`.
- `dbt/Cargo.lock` and `dbt/target/` are present (build artifacts from a
  previous build), but `target/` is gitignored and `Cargo.lock` is gitignored
  (`.gitignore` lines 14–15).
- **`dbt/x8088.rs`** has a minimal 8088 decoder + interpreter
  (`X8088Executor::step()`) that handles ~20 opcode groups. Instructions used
  by the `bench` program (e.g. `ADD AX,AX` = `0x01 0xC0`, `DEC BX` = `0x4B`)
  are treated as unknown (`"???"`), causing IP to advance by only 1 byte and
  misalign subsequent decode. Conditional jumps `Jcc` are hardcoded
  "always taken".
- **`dbt/bin/sim8088.rs`** has three commands: `run`, `bench`, `decode`.
  It uses `X8088Executor` (not the DBT translator/executor pipeline).
- **No `msdos.img`** is present (`*.img` gitignored). `run` falls back to a
  dummy 512-byte empty disk.
- **DBT pipeline** (`ir.rs`, `translator.rs`, `executor/mod.rs`,
  `codegen/mod.rs`, `optimizer.rs`) is complete enough for x86_64 translation
  but is **not wired** into `sim8088`.

---

## Phase 1 – Restore Build System

| # | Task | Detail |
|---|------|--------|
| 1.1 | Create `dbt/Cargo.toml` | Copy content from `docs/archive/code_recovery/recovered_deep_bemi/dbt/cargo.toml`. Ensure filename is `Cargo.toml` (uppercase). Content: <br>`[package]`<br>`name = "bemi-dbt"`<br>`version = "1.3.0"`<br>`edition = "2021"`<br>`description = "BEMI Dynamic Binary Translation Pipeline"`<br><br>`[lib]`<br>`name = "bemi_dbt"`<br>`path = "lib.rs"`<br><br>`[[bin]]`<br>`name = "sim8088"`<br>`path = "bin/sim8088.rs"`<br>`required-features = ["x8088"]`<br><br>`[features]`<br>`default = []`<br>`x8088 = []`<br><br>`[dependencies]`<br>`iced-x86 = { version = "1.21.0", default-features = false, features = ["decoder", "std"] }` |
| 1.2 | Build the crate | `cd dbt && cargo build --features x8088` |
| 1.3 | Verify `sim8088 help` | `cargo run --features x8088 -- sim8088 help` |
| 1.4 | Test bench with 200M cycles | `cargo run --features x8088 -- sim8088 bench 200000000`<br>Verifies the binary runs end-to-end without panicking.<br>(Current bench output is unreliable due to incomplete decoder — fixed in Phase 2.) |

## Phase 2 – Make `sim8088 bench` Correct

| # | Task | Detail |
|---|------|--------|
| 2.1 | Expand `decode_8088` mini-decoder in `dbt/x8088.rs` | Add opcodes:<br>- `0x00` (ADD r/m8, r8)<br>- `0x01` (ADD r/m16, r16)<br>- `0x02` (ADD r8, r/m8)<br>- `0x03` (ADD r16, r/m16)<br>- `0x28` (SUB r/m8, r8)<br>- `0x29` (SUB r/m16, r16)<br>- `0x2A` (SUB r8, r/m8)<br>- `0x2B` (SUB r16, r/m16)<br>- `0x38` (CMP r/m8, r8)<br>- `0x39` (CMP r/m16, r16)<br>- `0x3A` (CMP r8, r/m8)<br>- `0x3B` (CMP r16, r/m16)<br>- `0x48` (DEC AX) through `0x4F` (DEC DI) — 16-bit register decrement<br>- `0x40` (INC AX) through `0x47` (INC DI) — 16-bit register increment<br>- `0x04` (ADD AL, imm8)<br>- `0x05` (ADD AX, imm16)<br>- `0x2C` (SUB AL, imm8)<br>- `0x2D` (SUB AX, imm16)<br>- `0x3C` (CMP AL, imm8)<br>- `0x3D` (CMP AX, imm16) |
| 2.2 | Fix `Jcc` dispatch in `X8088Executor::step` | Replace `let taken = true` with real flag checks. For each `0x70`–`0x7F` opcode, evaluate ZF, SF, OF, CF against the condition encoded in the low nibble. |
| 2.3 | Execute ALU ops in `X8088Executor::step` | Handle `InstCategory::Alu` dispatch for ADD, SUB, CMP, INC, DEC with proper register operand extraction and flag updates. |
| 2.4 | Verify bench | `cargo run --features x8088 -- sim8088 bench 200000000`<br>Expected: AX=0x0000, BX=0x0000, CX=0x0000, DX=0x0000, State = HLT reached.<br>The program: MOV AX,1; MOV BX,16; loop: ADD AX,AX; DEC BX; JNZ loop; HLT.<br>After 16 iterations: AX=0 (overflow wraps), BX=0, HLT. |

## Phase 3 – Full Opcode Coverage (COMPLETE)

### 3a. Logic Operations
| Opcode Range | Group | Status |
|---|---|---|
| 0x0A-0x0D | OR (ModRM + imm8/imm16) | done |
| 0x20-0x25 | AND (ModRM + imm8/imm16) | done |
| 0x30-0x35 | XOR (ModRM + imm8/imm16) | done |
| 0x84-0x85, 0xA8-0xA9 | TEST (ModRM + imm8/imm16) | done |

### 3b. MOV/XCHG/Convert
| Opcode Range | Group | Status |
|---|---|---|
| 0x88-0x8B | MOV ModRM (r/m ↔ reg) | done |
| 0x8C, 0x8E | MOV Sreg | done |
| 0x86-0x87, 0x91-0x97 | XCHG | done |
| 0x98-0x99 | CBW/CWD | done |
| 0x8D | LEA | done |

### 3c. Group 1 (0x80-0x83)
| reg_op | Operation | Status |
|---|---|---|
| 0-7 | ADD/OR/ADC/SBB/AND/SUB/XOR/CMP imm | done |

### 3d. Shift/Rotate + Group 3
| Opcode Range | Group | Status |
|---|---|---|
| 0xD0-0xD3 | SHIFT (ROL/ROR/RCL/RCR/SHL/SHR/SAR by 1 or CL) | done |
| 0xF6-0xF7 | GRP3 (TEST/NOT/NEG/MUL/IMUL/DIV/IDIV) | done |

### 3e. Control Flow + Flags + Misc
| Opcode Range | Group | Status |
|---|---|---|
| 0xE0-0xE3 | LOOPNE/LOOPE/LOOP/JCXZ | done |
| 0xF5,0xF8,0xF9,0xFC,0xFD | CMC/CLC/STC/CLD/STD | done |
| 0x06,0x07,0x0E,0x16,0x17,0x1E,0x1F | PUSH/POP Sreg | done |
| 0x27,0x2F,0x37,0x3F | DAA/DAS/AAA/AAS | done |
| 0x9E,0x9F | SAHF/LAHF | done |
| 0xD7 | XLAT | done |

**Total opcode coverage: ~100+ primary opcodes** (all of 8086/8088 except 2-byte escape 0x0F, FPU, string ops, IN/OUT, segment overrides).

## Phase 4 – BIOS Handler Expansion (COMPLETE)

| INT | Services | Status |
|---|---|---|
| INT 10h AH=0Eh | Teletype output (\r → \r\n) | done |
| INT 13h AH=02h | Read sectors (returns success) | done |
| INT 16h | Keyboard (no keypress) | done |
| INT 1Ah AH=00h | System clock counter | done |
| INT 21h AH=01h | Char input with echo | done |
| INT 21h AH=02h | Char output | done |
| INT 21h AH=09h | String output ($-terminated) | done |
| INT 21h AH=2Ch | Get system time | done |
| INT 21h AH=30h | Get DOS version | done |
| INT 21h AH=4Ch | Exit with return code | done |

## Phase 5 – Prefixes, I/O, String Ops, ModRM Fixes (COMPLETE)

### Prefix / Segment Override Support
| Opcode | Prefix | Status |
|---|---|---|
| 0x26 | ES override | done |
| 0x2E | CS override | done |
| 0x36 | SS override | done |
| 0x3E | DS override | done |
| 0xF0 | LOCK (ignored) | done |
| 0xF2/0xF3 | REPNE/REP (ignored) | done |

### IN/OUT
| Opcode | Operation | Status |
|---|---|---|
| 0xE4-0xE5 | IN AL/AX, imm8 | done |
| 0xE6-0xE7 | OUT imm8, AL/AX | done |
| 0xEC-0xED | IN AL/AX, DX | done (COM1 ports handled) |
| 0xEE-0xEF | OUT DX, AL/AX | done (COM1 output) |

### String Operations
| Opcode | Operation | Status |
|---|---|---|
| 0xA4-0xA5 | MOVSB/MOVSW | done |
| 0xA6-0xA7 | CMPSB/CMPSW | done |
| 0xAA-0xAB | STOSB/STOSW | done |
| 0xAC-0xAD | LODSB/LODSW | done |
| 0xAE-0xAF | SCASB/SCASW | done |

### ModRM Fixes
- `modrm_inst_len()` computes correct total instruction length including memory displacement bytes
- `prefix_count` tracked and added to IP advancement
- `ip_after()` accounts for prefix bytes

## Phase 6 – DBT Pipeline (Future)

| # | Task | Detail |
|---|---|---|
| 6.1 | Assess DBT integration | Translator targets x86_64; needs 16-bit mode adapter. |
| 6.2 | Wire DBT pipeline to `sim8088` | `simdbt` binary or feature flag in `sim8088`. Out of scope. |

---

## Definition of Done

- [x] `os` branch created from `main`
- [x] `plan/os/SIM8088_ROADMAP.md` committed
- [x] `dbt/Cargo.toml` exists and `cd dbt && cargo build --features x8088` compiles cleanly
- [x] `sim8088 help` prints usage
- [x] `sim8088 decode <bytes>` outputs correctly decoded instructions with ModRM lengths
- [x] `sim8088 bench 200000000` produces correct state (AX=0, BX=0, HLT)
- [x] `sim8088 test` runs comprehensive 57-instruction opcode verification (HLT reached, all results correct)
- [x] `sim8088 run` loads disk images without panicking (run command verified)
- [x] BIOS handles INT 10h, 13h, 16h, 1Ah, 20h, 21h

## Verified `sim8088 test` Output
```
  Mode    : test (comprehensive opcode verification)

  57 instructions traced covering:
  AND OR XOR TEST  MOV-ModRM  XCHG  CBW/CWD  SHIFT  LOOP
  GRP1(80-83)  INC/DEC  CLC/CMC  MUL/DIV  SAHF/LAHF  HLT

  [RESULT] Simulation complete:
    Cycles executed : 57
    AX              : 0346
    CX              : 0000
    DX              : 0001
    BX              : 0003
    State           : HLT reached
```

## Verified Bench Output (200M cycles)
```
  AX=0000  BX=0000  CX=0000  DX=0000  State: HLT reached  Cycles: 51
```

## Verified Decode Output
```
  [0x0100]       MOV   B8 01 00          Next IP: 0x0103
  [0x0103]       MOV   BB 10 00          Next IP: 0x0106
  [0x0106]       ADD   01 C0             Next IP: 0x0108
  [0x0108]       DEC   4B                Next IP: 0x0109
  [0x0109]       Jcc   75 FB             Next IP: 0x010b  Branch target: 0x0106
  [0x010b]       HLT   F4                Next IP: 0x010c
```

---

## Key Files Reference

| File | Purpose |
|------|---------|
| `dbt/Cargo.toml` | Crate manifest (RESTORED) |
| `dbt/bin/sim8088.rs` | Binary entrypoint (run/bench/decode/test) |
| `dbt/x8088.rs` | 8088 interpreter + full decoder (~100+ opcodes) |
| `dbt/lib.rs` | Library root (guards iced-x86 modules behind `x8088` feature) |
| `dbt/ir.rs` | BEMI IR types & TranslationBlock |
| `dbt/translator.rs` | x86_64 → BEMI IR translator (iced-x86) |
| `dbt/executor/mod.rs` | IR executor with register file + 60+ micro-ops |
| `dbt/codegen/mod.rs` | x86_64 native code emitter |
| `dbt/optimizer.rs` | Peephole optimization on TranslationBlock |
| `plan/os/SIM8088_ROADMAP.md` | This file |

## Notes

- `target/` and `Cargo.lock` are gitignored (`.gitignore` lines 14–15).
- `msdos.img` must be obtained separately (not in repo).
- All `cargo` commands must include `--features x8088` (the library crate is `no_std` by default).
- Correct cargo invocation: `cargo run --features x8088 -- help` (not `cargo run -- sim8088`).
- `sim8088` uses its own interpreter in `x8088.rs`, not the DBT translator/executor pipeline.
- Remaining unimplemented: 2-byte escape (0x0F), FPU (0xD8-0xDF), REP prefix for string ops.
