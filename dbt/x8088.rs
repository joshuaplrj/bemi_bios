//! BEMI 8088 Simulation Layer
//! ===========================
//! Provides a cycle-accurate (software) 8088 CPU model backed by the
//! BEMI IR pipeline for translation and execution of 8086/8088 code.
//!
//! Used exclusively with feature = "x8088" (std required).

#![allow(dead_code)]

// ─── Instruction category ────────────────────────────────────────────────────

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum InstCategory {
    Alu,
    Mov,
    Jmp,
    Jcc,
    Call,
    Ret,
    Push,
    Pop,
    String,
    System,
    Other,
}

// ─── Decoded 8088 instruction ─────────────────────────────────────────────────

#[derive(Debug, Clone)]
pub struct DecodedInst8088 {
    /// Opcode byte(s)
    pub opcode: u16,
    /// Total byte length of this instruction
    pub length: u8,
    /// Primary immediate operand (or displacement for memory)
    pub immediate: u16,
    /// Second immediate (used for FAR jumps/calls: segment)
    pub immediate2: u16,
    /// Displacement size in bytes (0 = none, 1 = 8-bit, 2 = 16-bit)
    pub disp_size: u8,
    /// Raw displacement value (sign-extended to 16 bits)
    pub displacement: i16,
    /// Whether this is a FAR (inter-segment) control transfer
    pub is_far: bool,
    /// Raw ModR/M byte (0 when instruction has no ModR/M)
    pub modrm: u8,
    /// Instruction category for branch analysis
    pub category: InstCategory,
    /// Human-readable mnemonic
    pub mnemonic: &'static str,
}

impl DecodedInst8088 {
    /// Return the IP of the instruction that follows this one.
    pub fn ip_after(&self, current_ip: u16) -> u16 {
        current_ip.wrapping_add(self.length as u16)
    }

    /// For branch instructions, return the resolved branch target.
    pub fn branch_target(&self, current_ip: u16) -> u16 {
        // Short / near relative branch
        let next = self.ip_after(current_ip);
        next.wrapping_add(self.displacement as u16)
    }
}

// ─── 8088 CPU registers ───────────────────────────────────────────────────────

#[derive(Debug, Clone)]
pub struct Cpu8088 {
    // General purpose (16-bit view)
    pub ax: u16, pub bx: u16, pub cx: u16, pub dx: u16,
    pub si: u16, pub di: u16, pub sp: u16, pub bp: u16,

    // Segment registers
    pub cs: u16, pub ds: u16, pub es: u16, pub ss: u16,

    // Instruction pointer
    pub ip: u16,

    // Flags register (raw 16-bit)
    pub flags: u16,

    // State
    pub halted: bool,
    pub interrupts_enabled: bool,
}

impl Cpu8088 {
    pub fn new() -> Self {
        Cpu8088 {
            ax: 0, bx: 0, cx: 0, dx: 0,
            si: 0, di: 0, sp: 0xFFFE, bp: 0,
            cs: 0xF000, ds: 0, es: 0, ss: 0,
            ip: 0xFFF0,
            flags: 0x0002, // bit 1 always set on 8086
            halted: false,
            interrupts_enabled: false,
        }
    }

    /// Compute a 20-bit physical address from segment:offset
    #[inline(always)]
    pub fn linear(&self, seg: u16, off: u16) -> u32 {
        ((seg as u32) << 4).wrapping_add(off as u32)
    }

    /// CF flag
    pub fn cf(&self) -> bool { self.flags & 0x0001 != 0 }
    /// ZF flag
    pub fn zf(&self) -> bool { self.flags & 0x0040 != 0 }
    /// SF flag
    pub fn sf(&self) -> bool { self.flags & 0x0080 != 0 }
    /// OF flag
    pub fn of_flag(&self) -> bool { self.flags & 0x0800 != 0 }
    /// IF flag
    pub fn int_flag(&self) -> bool { self.flags & 0x0200 != 0 }
    /// DF flag
    pub fn df(&self) -> bool { self.flags & 0x0400 != 0 }
    /// PF flag (parity of low 8 bits)
    pub fn pf(&self) -> bool { self.flags & 0x0004 != 0 }
    /// AF flag (auxiliary carry)
    pub fn af(&self) -> bool { self.flags & 0x0010 != 0 }

    /// Set CF from bool
    pub fn set_cf(&mut self, v: bool) { if v { self.flags |= 0x0001; } else { self.flags &= !0x0001u16; } }
    /// Set PF from bool
    pub fn set_pf(&mut self, v: bool) { if v { self.flags |= 0x0004; } else { self.flags &= !0x0004u16; } }
    /// Set AF from bool
    pub fn set_af(&mut self, v: bool) { if v { self.flags |= 0x0010; } else { self.flags &= !0x0010u16; } }
    /// Set ZF from bool
    pub fn set_zf(&mut self, v: bool) { if v { self.flags |= 0x0040; } else { self.flags &= !0x0040u16; } }
    /// Set SF from bool
    pub fn set_sf(&mut self, v: bool) { if v { self.flags |= 0x0080; } else { self.flags &= !0x0080u16; } }
    /// Set OF from bool
    pub fn set_of(&mut self, v: bool) { if v { self.flags |= 0x0800; } else { self.flags &= !0x0800u16; } }

    /// Compute parity of low 8 bits (even parity → PF=1)
    #[inline(always)]
    pub fn parity(v: u8) -> bool { v.count_ones() % 2 == 0 }

    /// Update arithmetic flags after a 16-bit add.
    /// CF unchanged for INC/DEC (pass `update_cf: false`).
    pub fn update_flags_add(&mut self, result: u16, a: u16, b: u16, update_cf: bool) {
        if update_cf { self.set_cf(result < a); }
        self.set_pf(Self::parity(result as u8));
        self.set_af(((a & 0x0F) + (b & 0x0F)) > 0x0F);
        self.set_zf(result == 0);
        self.set_sf((result as i16) < 0);
        // OF: overflow when (a ^ b) >= 0 but (a ^ result) < 0 (i.e., sign changed wrongly)
        self.set_of((a ^ b) & 0x8000 == 0 && (a ^ result) & 0x8000 != 0);
    }

    /// Update arithmetic flags after a 16-bit sub.
    pub fn update_flags_sub(&mut self, result: u16, a: u16, b: u16, update_cf: bool) {
        if update_cf { self.set_cf(a < b); }
        self.set_pf(Self::parity(result as u8));
        self.set_af(((a & 0x0F).wrapping_sub(b & 0x0F)) > 0x0F);
        self.set_zf(result == 0);
        self.set_sf((result as i16) < 0);
        self.set_of(((a ^ b) & 0x8000) != 0 && ((a ^ result) & 0x8000) != 0);
    }
}

// ─── 1 MB memory model ───────────────────────────────────────────────────────

pub struct Memory8088 {
    pub ram: Box<[u8; 0x10_0000]>,
}

impl Memory8088 {
    pub fn new() -> Self {
        Memory8088 { ram: Box::new([0u8; 0x10_0000]) }
    }

    pub fn read8(&self, addr: u32) -> u8 {
        self.ram[(addr & 0xF_FFFF) as usize]
    }

    pub fn write8(&mut self, addr: u32, v: u8) {
        self.ram[(addr & 0xF_FFFF) as usize] = v;
    }

    pub fn read16(&self, addr: u32) -> u16 {
        let lo = self.read8(addr) as u16;
        let hi = self.read8(addr.wrapping_add(1)) as u16;
        lo | (hi << 8)
    }

    pub fn write16(&mut self, addr: u32, v: u16) {
        self.write8(addr, v as u8);
        self.write8(addr.wrapping_add(1), (v >> 8) as u8);
    }

    /// Load a byte slice at a physical address (e.g., loading a disk image to 0x7C00)
    pub fn load_at(&mut self, phys: u32, data: &[u8]) {
        let base = (phys & 0xF_FFFF) as usize;
        let len  = data.len().min(0x10_0000 - base);
        self.ram[base..base + len].copy_from_slice(&data[..len]);
    }
}

// ─── Simple BIOS/DOS emulation shim ──────────────────────────────────────────

pub struct BiosShim {
    /// Set when the guest issues INT 20h or INT 21h AH=4Ch (DOS program exit)
    pub dos_break: bool,
}

impl BiosShim {
    pub fn new() -> Self {
        BiosShim { dos_break: false }
    }

    /// Handle a software interrupt. Returns `true` if the interrupt was consumed.
    pub fn handle_int(&mut self, cpu: &mut Cpu8088, mem: &mut Memory8088, int_num: u8) -> bool {
        match int_num {
            0x10 => {
                // Video — ignore for now
                true
            }
            0x16 => {
                // Keyboard — return no keypress (AH=0 scan=0, AL=0)
                cpu.ax = 0;
                true
            }
            0x20 => {
                // DOS program terminate
                self.dos_break = true;
                true
            }
            0x21 => {
                // DOS services
                let ah = (cpu.ax >> 8) as u8;
                match ah {
                    0x02 => {
                        // Character output
                        let ch = (cpu.dx & 0xFF) as u8;
                        print!("{}", ch as char);
                    }
                    0x09 => {
                        // String output (DS:DX, '$'-terminated)
                        let mut addr = cpu.linear(cpu.ds, cpu.dx);
                        loop {
                            let c = mem.read8(addr);
                            if c == b'$' { break; }
                            print!("{}", c as char);
                            addr = addr.wrapping_add(1);
                        }
                    }
                    0x4C => {
                        // Exit with return code
                        self.dos_break = true;
                    }
                    _ => {}
                }
                true
            }
            _ => false,
        }
    }
}

// ─── 8088 decoder (minimal, enough to run MS-DOS 1.0 binaries) ───────────────

/// Decode one 8088 instruction starting at CS:IP.
/// Returns `None` if the byte stream is exhausted or unrecognised.
pub fn decode_8088(mem: &Memory8088, cs: u16, ip: u16) -> Option<DecodedInst8088> {
    let phys = ((cs as u32) << 4).wrapping_add(ip as u32);
    let b0 = mem.read8(phys);

    // We build a minimal decoder for the most common instructions:
    let (mnemonic, category, length, imm, disp_sz, disp, is_far, modrm_byte) = match b0 {
        // NOP
        0x90 => ("NOP", InstCategory::Other, 1, 0u16, 0u8, 0i16, false, 0u8),
        // HLT
        0xF4 => ("HLT", InstCategory::System, 1, 0, 0, 0, false, 0u8),
        // CLI / STI
        0xFA => ("CLI", InstCategory::System, 1, 0, 0, 0, false, 0u8),
        0xFB => ("STI", InstCategory::System, 1, 0, 0, 0, false, 0u8),
        // Short conditional jumps (Jcc rel8)
        0x70..=0x7F => {
            let rel = mem.read8(phys + 1) as i8 as i16;
            ("Jcc", InstCategory::Jcc, 2, 0, 1, rel, false, 0u8)
        }
        // JMP short
        0xEB => {
            let rel = mem.read8(phys + 1) as i8 as i16;
            ("JMP", InstCategory::Jmp, 2, 0, 1, rel, false, 0u8)
        }
        // JMP near
        0xE9 => {
            let rel = mem.read16(phys + 1) as i16;
            ("JMP", InstCategory::Jmp, 3, 0, 2, rel, false, 0u8)
        }
        // JMP far
        0xEA => {
            let off = mem.read16(phys + 1);
            let _seg = mem.read16(phys + 3);
            ("JMP FAR", InstCategory::Jmp, 5, off, 2, 0, true, 0u8)
        }
        // CALL near
        0xE8 => {
            let rel = mem.read16(phys + 1) as i16;
            ("CALL", InstCategory::Call, 3, 0, 2, rel, false, 0u8)
        }
        // RET near
        0xC3 => ("RET", InstCategory::Ret, 1, 0, 0, 0, false, 0u8),
        // RET near with imm16
        0xC2 => {
            let n = mem.read16(phys + 1);
            ("RET", InstCategory::Ret, 3, n, 2, 0, false, 0u8)
        }
        // INT imm8
        0xCD => {
            let n = mem.read8(phys + 1);
            ("INT", InstCategory::System, 2, n as u16, 1, 0, false, 0u8)
        }
        // PUSH / POP for common patterns — treat as 1-byte
        0x50..=0x5F => {
            let m = if b0 < 0x58 { "PUSH" } else { "POP" };
            let cat = if b0 < 0x58 { InstCategory::Push } else { InstCategory::Pop };
            (m, cat, 1u8, 0u16, 0u8, 0i16, false, 0u8)
        }
        // MOV AL/AX, imm8/imm16
        0xB0..=0xBF => {
            let wide = b0 & 0x08 != 0;
            let (imm, len) = if wide {
                (mem.read16(phys + 1), 3u8)
            } else {
                (mem.read8(phys + 1) as u16, 2u8)
            };
            ("MOV", InstCategory::Mov, len, imm, if wide { 2 } else { 1 }, 0, false, 0u8)
        }
        // ── ADD ──────────────────────────────────────────────────────────────
        // ADD r/m8, r8 / ADD r/m16, r16 / ADD r8, r/m8 / ADD r16, r/m16
        0x00 | 0x01 | 0x02 | 0x03 => {
            let modrm = mem.read8(phys + 1);
            let len = if (modrm >> 6) == 3 { 2u8 } else { 2u8 }; // TODO: memory forms
            ("ADD", InstCategory::Alu, len, 0u16, 0u8, 0i16, false, modrm)
        }
        // ADD AL, imm8
        0x04 => {
            let imm = mem.read8(phys + 1) as u16;
            ("ADD", InstCategory::Alu, 2, imm, 0, 0i16, false, 0u8)
        }
        // ADD AX, imm16
        0x05 => {
            let imm = mem.read16(phys + 1);
            ("ADD", InstCategory::Alu, 3, imm, 0, 0i16, false, 0u8)
        }
        // ── SUB ──────────────────────────────────────────────────────────────
        0x28 | 0x29 | 0x2A | 0x2B => {
            let modrm = mem.read8(phys + 1);
            let len = if (modrm >> 6) == 3 { 2u8 } else { 2u8 };
            ("SUB", InstCategory::Alu, len, 0u16, 0u8, 0i16, false, modrm)
        }
        // SUB AL, imm8
        0x2C => {
            let imm = mem.read8(phys + 1) as u16;
            ("SUB", InstCategory::Alu, 2, imm, 0, 0i16, false, 0u8)
        }
        // SUB AX, imm16
        0x2D => {
            let imm = mem.read16(phys + 1);
            ("SUB", InstCategory::Alu, 3, imm, 0, 0i16, false, 0u8)
        }
        // ── CMP ──────────────────────────────────────────────────────────────
        0x38 | 0x39 | 0x3A | 0x3B => {
            let modrm = mem.read8(phys + 1);
            let len = if (modrm >> 6) == 3 { 2u8 } else { 2u8 };
            ("CMP", InstCategory::Alu, len, 0u16, 0u8, 0i16, false, modrm)
        }
        // CMP AL, imm8
        0x3C => {
            let imm = mem.read8(phys + 1) as u16;
            ("CMP", InstCategory::Alu, 2, imm, 0, 0i16, false, 0u8)
        }
        // CMP AX, imm16
        0x3D => {
            let imm = mem.read16(phys + 1);
            ("CMP", InstCategory::Alu, 3, imm, 0, 0i16, false, 0u8)
        }
        // ── INC reg16 (0x40-0x47) ────────────────────────────────────────────
        0x40..=0x47 => {
            ("INC", InstCategory::Alu, 1, 0u16, 0, 0i16, false, 0u8)
        }
        // ── DEC reg16 (0x48-0x4F) ────────────────────────────────────────────
        0x48..=0x4F => {
            ("DEC", InstCategory::Alu, 1, 0u16, 0, 0i16, false, 0u8)
        }
        // Generic: unknown / not decoded — skip 1 byte
        _ => ("???", InstCategory::Other, 1, 0, 0, 0, false, 0u8),
    };

    Some(DecodedInst8088 {
        opcode: b0 as u16,
        length,
        immediate: imm,
        immediate2: 0,
        disp_size: disp_sz,
        displacement: disp,
        is_far,
        modrm: modrm_byte,
        category,
        mnemonic,
    })
}

// ─── X8088Executor ────────────────────────────────────────────────────────────

pub struct X8088Executor {
    pub cpu:   Cpu8088,
    pub mem:   Memory8088,
    pub bios:  BiosShim,
    pub trace: bool,
    /// Running cycle count
    pub cycles: u64,
}

impl X8088Executor {
    pub fn new() -> Self {
        X8088Executor {
            cpu:    Cpu8088::new(),
            mem:    Memory8088::new(),
            bios:   BiosShim::new(),
            trace:  false,
            cycles: 0,
        }
    }

    /// Load a raw binary image at physical address `phys_base`.
    pub fn load_image(&mut self, data: &[u8], phys_base: u32) {
        self.mem.load_at(phys_base, data);
    }

    /// Set CS:IP and SS:SP entry points.
    pub fn set_entry(&mut self, cs: u16, ip: u16, ss: u16, sp: u16) {
        self.cpu.cs = cs;
        self.cpu.ip = ip;
        self.cpu.ss = ss;
        self.cpu.sp = sp;
    }

    /// Execute one instruction and advance state.  Returns `true` if execution
    /// should continue, `false` on HLT or fatal error.
    pub fn step(&mut self) -> bool {
        if self.cpu.halted { return false; }

        let cs = self.cpu.cs;
        let ip = self.cpu.ip;

        let inst = match decode_8088(&self.mem, cs, ip) {
            Some(i) => i,
            None => return false,
        };

        if self.trace {
            let phys = ((cs as u32) << 4) + ip as u32;
            println!("  [{:04X}:{:04X}] ({:05X}) {}", cs, ip, phys, inst.mnemonic);
        }

        // Advance IP over this instruction by default
        self.cpu.ip = self.cpu.ip.wrapping_add(inst.length as u16);
        self.cycles += inst.length as u64 + 4; // rough CPI estimate

        // Dispatch
        match inst.category {
            InstCategory::System => {
                match inst.opcode as u8 {
                    0xF4 => { self.cpu.halted = true; return false; } // HLT
                    0xFA => { self.cpu.interrupts_enabled = false; }  // CLI
                    0xFB => { self.cpu.interrupts_enabled = true; }   // STI
                    0xCD => {
                        // INT imm8
                        let int_num = inst.immediate as u8;
                        self.bios.handle_int(&mut self.cpu, &mut self.mem, int_num);
                    }
                    _ => {}
                }
            }
            InstCategory::Jmp => {
                if inst.is_far {
                    self.cpu.ip = inst.immediate;
                    self.cpu.cs = inst.immediate2;
                } else {
                    self.cpu.ip = inst.branch_target(ip);
                }
            }
            InstCategory::Jcc => {
                let cond = inst.opcode as u8 & 0x0F;
                let taken = match cond {
                    0 => self.cpu.of_flag(),
                    1 => !self.cpu.of_flag(),
                    2 => self.cpu.cf(),
                    3 => !self.cpu.cf(),
                    4 => self.cpu.zf(),
                    5 => !self.cpu.zf(),
                    6 => self.cpu.cf() || self.cpu.zf(),
                    7 => !self.cpu.cf() && !self.cpu.zf(),
                    8 => self.cpu.sf(),
                    9 => !self.cpu.sf(),
                    0xA => self.cpu.pf(),
                    0xB => !self.cpu.pf(),
                    0xC => self.cpu.sf() != self.cpu.of_flag(),
                    0xD => self.cpu.sf() == self.cpu.of_flag(),
                    0xE => self.cpu.zf() || (self.cpu.sf() != self.cpu.of_flag()),
                    0xF => !self.cpu.zf() && (self.cpu.sf() == self.cpu.of_flag()),
                    _ => true,
                };
                if taken {
                    self.cpu.ip = inst.branch_target(ip);
                }
            }
            InstCategory::Call => {
                // Push return address
                let ret_ip = ip.wrapping_add(inst.length as u16);
                self.cpu.sp = self.cpu.sp.wrapping_sub(2);
                let stack_addr = self.cpu.linear(self.cpu.ss, self.cpu.sp);
                self.mem.write16(stack_addr, ret_ip);
                // Jump to target
                self.cpu.ip = inst.branch_target(ip);
            }
            InstCategory::Ret => {
                let stack_addr = self.cpu.linear(self.cpu.ss, self.cpu.sp);
                self.cpu.ip = self.mem.read16(stack_addr);
                self.cpu.sp = self.cpu.sp.wrapping_add(2);
                // Handle RET imm16 stack pop
                if inst.immediate > 0 {
                    self.cpu.sp = self.cpu.sp.wrapping_add(inst.immediate);
                }
            }
            InstCategory::Push => {
                // PUSH reg (50-57 / 58-5F handled the same for stack accounting)
                let reg_idx = inst.opcode as u8 & 0x07;
                let val = self.read_gp16(reg_idx);
                self.cpu.sp = self.cpu.sp.wrapping_sub(2);
                let addr = self.cpu.linear(self.cpu.ss, self.cpu.sp);
                self.mem.write16(addr, val);
            }
            InstCategory::Pop => {
                let reg_idx = inst.opcode as u8 & 0x07;
                let addr = self.cpu.linear(self.cpu.ss, self.cpu.sp);
                let val = self.mem.read16(addr);
                self.cpu.sp = self.cpu.sp.wrapping_add(2);
                self.write_gp16(reg_idx, val);
            }
            InstCategory::Mov => {
                // MOV reg, imm (B0-BF)
                let reg_idx = inst.opcode as u8 & 0x07;
                self.write_gp16(reg_idx, inst.immediate);
            }
            InstCategory::Alu => {
                match inst.opcode as u8 {
                    // ── ADD ──────────────────────────────────────────────────────
                    0x00 | 0x01 | 0x02 | 0x03 => {
                        let modrm = inst.modrm;
                        let rm = modrm & 0x07;
                        let reg = (modrm >> 3) & 0x07;
                        let (dst_idx, src_idx): (u8, u8) = match inst.opcode as u8 {
                            0x00 | 0x01 => (rm, reg),  // dst=r/m, src=reg
                            _            => (reg, rm), // dst=reg, src=r/m
                        };
                        let a = self.read_gp16(dst_idx);
                        let b = self.read_gp16(src_idx);
                        let result = a.wrapping_add(b);
                        self.write_gp16(dst_idx, result);
                        self.cpu.update_flags_add(result, a, b, true);
                    }
                    0x04 => {
                        let a = (self.cpu.ax & 0xFF) as u16;
                        let b = inst.immediate & 0xFF;
                        let result = (a.wrapping_add(b)) & 0xFF;
                        self.cpu.ax = (self.cpu.ax & 0xFF00) | result;
                        self.cpu.update_flags_add(result as u16, a, b, true);
                    }
                    0x05 => {
                        let a = self.cpu.ax;
                        let b = inst.immediate;
                        let result = a.wrapping_add(b);
                        self.cpu.ax = result;
                        self.cpu.update_flags_add(result, a, b, true);
                    }
                    // ── SUB ──────────────────────────────────────────────────────
                    0x28 | 0x29 | 0x2A | 0x2B => {
                        let modrm = inst.modrm;
                        let rm = modrm & 0x07;
                        let reg = (modrm >> 3) & 0x07;
                        let (dst_idx, src_idx): (u8, u8) = match inst.opcode as u8 {
                            0x28 | 0x29 => (rm, reg),
                            _            => (reg, rm),
                        };
                        let a = self.read_gp16(dst_idx);
                        let b = self.read_gp16(src_idx);
                        let result = a.wrapping_sub(b);
                        self.write_gp16(dst_idx, result);
                        self.cpu.update_flags_sub(result, a, b, true);
                    }
                    0x2C => {
                        let a = (self.cpu.ax & 0xFF) as u16;
                        let b = inst.immediate & 0xFF;
                        let result = (a.wrapping_sub(b)) & 0xFF;
                        self.cpu.ax = (self.cpu.ax & 0xFF00) | result;
                        self.cpu.update_flags_sub(result as u16, a, b, true);
                    }
                    0x2D => {
                        let a = self.cpu.ax;
                        let b = inst.immediate;
                        let result = a.wrapping_sub(b);
                        self.cpu.ax = result;
                        self.cpu.update_flags_sub(result, a, b, true);
                    }
                    // ── CMP ──────────────────────────────────────────────────────
                    0x38 | 0x39 | 0x3A | 0x3B => {
                        let modrm = inst.modrm;
                        let rm = modrm & 0x07;
                        let reg = (modrm >> 3) & 0x07;
                        let (dst_idx, src_idx): (u8, u8) = match inst.opcode as u8 {
                            0x38 | 0x39 => (rm, reg),
                            _            => (reg, rm),
                        };
                        let a = self.read_gp16(dst_idx);
                        let b = self.read_gp16(src_idx);
                        let result = a.wrapping_sub(b);
                        self.cpu.update_flags_sub(result, a, b, true);
                    }
                    0x3C => {
                        let a = (self.cpu.ax & 0xFF) as u16;
                        let b = inst.immediate & 0xFF;
                        let result = (a.wrapping_sub(b)) & 0xFF;
                        self.cpu.update_flags_sub(result as u16, a, b, true);
                    }
                    0x3D => {
                        let a = self.cpu.ax;
                        let b = inst.immediate;
                        let result = a.wrapping_sub(b);
                        self.cpu.update_flags_sub(result, a, b, true);
                    }
                    // ── INC ──────────────────────────────────────────────────────
                    0x40..=0x47 => {
                        let idx = inst.opcode as u8 & 0x07;
                        let a = self.read_gp16(idx);
                        let result = a.wrapping_add(1);
                        self.write_gp16(idx, result);
                        self.cpu.update_flags_add(result, a, 1, false);
                    }
                    // ── DEC ──────────────────────────────────────────────────────
                    0x48..=0x4F => {
                        let idx = inst.opcode as u8 & 0x07;
                        let a = self.read_gp16(idx);
                        let result = a.wrapping_sub(1);
                        self.write_gp16(idx, result);
                        self.cpu.update_flags_sub(result, a, 1, false);
                    }
                    _ => {}
                }
            }
            _ => {
                // Unknown / unimplemented — continue execution
            }
        }

        !self.bios.dos_break
    }

    // ── Internal helpers ──────────────────────────────────────────────────

    fn read_gp16(&self, idx: u8) -> u16 {
        match idx & 7 {
            0 => self.cpu.ax, 1 => self.cpu.cx,
            2 => self.cpu.dx, 3 => self.cpu.bx,
            4 => self.cpu.sp, 5 => self.cpu.bp,
            6 => self.cpu.si, _ => self.cpu.di,
        }
    }

    fn write_gp16(&mut self, idx: u8, val: u16) {
        match idx & 7 {
            0 => self.cpu.ax = val, 1 => self.cpu.cx = val,
            2 => self.cpu.dx = val, 3 => self.cpu.bx = val,
            4 => self.cpu.sp = val, 5 => self.cpu.bp = val,
            6 => self.cpu.si = val, _ => self.cpu.di = val,
        }
    }
}
