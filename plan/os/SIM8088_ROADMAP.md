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

## Phase 3 – Make `sim8088 decode` Accurate

| # | Task | Detail |
|---|------|--------|
| 3.1 | Verify decode output | `cargo run --features x8088 -- sim8088 decode B8 01 00 BB 10 00 01 C0 4B 75 FB F4`<br>Expect 6 correct instructions with proper mnemonics, lengths, and branch target. |
| 3.2 | Fix `decode_8088` if needed | Ensure modrm-decoded instructions report correct length (2 bytes for reg-reg, 3-4 for reg-mem). |

## Phase 4 – Handler Expansion (Optional, for `sim8088 run`)

| # | Task | Detail |
|---|------|--------|
| 4.1 | Expand BIOS shim | Add more INT 10h/13h/16h/21h handlers as needed for MS-DOS 1.0 boot. |
| 4.2 | Implement segment override prefix | `0x26` (ES), `0x2E` (CS), `0x36` (SS), `0x3E` (DS). |

## Phase 5 – Wire DBT Pipeline (Future)

| # | Task | Detail |
|---|------|--------|
| 5.1 | Assess DBT integration | Translator (`dbt/translator.rs`) targets x86_64. Needs a 16-bit mode adapter or a separate 8088-aware translator to feed `TranslationBlock` → `Executor`. |
| 5.2 | Create new binary or extend | `simdbt` binary or feature flag in `sim8088`. Out of scope for Phase 1–2. |

---

## Definition of Done

- [x] `os` branch created from `main`
- [x] `plan/os/SIM8088_ROADMAP.md` committed
- [ ] `dbt/Cargo.toml` exists and `cd dbt && cargo build --features x8088` compiles cleanly
- [ ] `cargo run --features x8088 -- sim8088 help` prints usage
- [ ] `cargo run --features x8088 -- sim8088 decode B8 01 00 BB 10 00 01 C0 4B 75 FB F4` outputs 6 correctly decoded instructions
- [ ] `cargo run --features x8088 -- sim8088 bench 200000000` produces correct register state and terminates with HLT (not cycle limit)

---

## Key Files Reference

| File | Purpose |
|------|---------|
| `dbt/Cargo.toml` | Crate manifest (MISSING, to be created) |
| `dbt/bin/sim8088.rs` | Binary entrypoint (run/bench/decode) |
| `dbt/x8088.rs` | 8088 interpreter + mini-decoder |
| `dbt/lib.rs` | Library root (guards iced-x86 modules behind `x8088` feature) |
| `dbt/ir.rs` | BEMI IR types & TranslationBlock |
| `dbt/translator.rs` | x86_64 → BEMI IR translator (iced-x86) |
| `dbt/executor/mod.rs` | IR executor with register file + 60+ micro-ops |
| `dbt/codegen/mod.rs` | x86_64 native code emitter |
| `dbt/optimizer.rs` | Peephole optimization on TranslationBlock |
| `plan/os/SIM8088_ROADMAP.md` | This file |

---

## Notes

- `target/` and `Cargo.lock` are currently gitignored (`.gitignore` lines 14–15). Consider un-ignoring `Cargo.lock` for deterministic builds of the binary crate.
- `msdos.img` must be obtained separately (not in repo). The `run` command falls back to a dummy 512-byte disk if no image is provided.
- All `cargo` commands must include `--features x8088` because the `sim8088` binary depends on the `iced-x86` crate, which is only compiled when the `x8088` feature is enabled (the library crate is `no_std` by default).
- The `sim8088` binary currently does **not** use the DBT pipeline (`translator`/`executor`/`codegen`). It has its own mini-decoder and interpreter in `x8088.rs`. Phase 2 focuses on making this mini-interpreter functional before any DBT integration.
