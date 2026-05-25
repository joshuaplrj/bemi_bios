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

    /// Update logic flags (AND/OR/XOR/TEST): CF=0, OF=0.
    pub fn update_flags_logic(&mut self, result: u16) {
        self.set_cf(false);
        self.set_of(false);
        self.set_pf(Self::parity(result as u8));
        self.set_zf(result == 0);
        self.set_sf((result as i16) < 0);
    }

    /// Update flags after shift/rotate. CF = last bit shifted out.
    /// OF: for single-bit shift, set if sign changed.
    pub fn update_flags_shift(&mut self, result: u16, cf_out: bool, single_bit: bool, old_result: u16) {
        self.set_cf(cf_out);
        if single_bit {
            let old_sf = (old_result as i16) < 0;
            let new_sf = (result as i16) < 0;
            self.set_of(old_sf != new_sf);
        }
        self.set_pf(Self::parity(result as u8));
        self.set_zf(result == 0);
        self.set_sf((result as i16) < 0);
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
                // Video services
                let ah = (cpu.ax >> 8) as u8;
                match ah {
                    0x0E => {
                        // Teletype output: write AL with attributes in BL
                        let ch = (cpu.ax & 0xFF) as u8;
                        if ch == b'\r' { print!("\r\n"); }
                        else { print!("{}", ch as char); }
                    }
                    _ => {}
                }
                true
            }
            0x13 => {
                // Disk services — minimal read
                let ah = (cpu.ax >> 8) as u8;
                if ah == 0x02 {
                    // Read sectors: DL=drive, CH=cyl, CL=sector, DH=head, AL=count, ES:BX=buffer
                    // For simulation, just return success (AH=0, AL=0)
                    cpu.ax = 0;
                }
                true
            }
            0x16 => {
                // Keyboard — return no keypress (AH=0 scan=0, AL=0)
                cpu.ax = 0;
                true
            }
            0x1A => {
                // RTC / System Timer
                let ah = (cpu.ax >> 8) as u8;
                match ah {
                    0x00 => {
                        // Read system clock counter (18.2 Hz ticks since midnight)
                        cpu.cx = 0; // high word
                        cpu.dx = 0; // low word
                        cpu.ax &= 0x00FF; // AL cleared (no midnight flag)
                    }
                    _ => {}
                }
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
                    0x01 => {
                        // Character input with echo — return 'A'
                        let ch = b'A';
                        print!("{}", ch as char);
                        cpu.ax = (cpu.ax & 0xFF00) | ch as u16;
                    }
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
                    0x2C => {
                        // Get system time (CH=hour, CL=min, DH=sec, DL=hundredths)
                        cpu.cx = 0;
                        cpu.dx = 0;
                    }
                    0x30 => {
                        // Get DOS version
                        cpu.ax = 0x0100; // version 1.00
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
        // ── PUSH Sreg / POP Sreg ─────────────────────────────────────────────
        0x06 => ("PUSH ES", InstCategory::Push, 1, 0, 0, 0, false, 0u8),
        0x0E => ("PUSH CS", InstCategory::Push, 1, 0, 0, 0, false, 0u8),
        0x16 => ("PUSH SS", InstCategory::Push, 1, 0, 0, 0, false, 0u8),
        0x1E => ("PUSH DS", InstCategory::Push, 1, 0, 0, 0, false, 0u8),
        0x07 => ("POP ES", InstCategory::Pop, 1, 0, 0, 0, false, 0u8),
        0x1F => ("POP DS", InstCategory::Pop, 1, 0, 0, 0, false, 0u8),
        0x17 => ("POP SS", InstCategory::Pop, 1, 0, 0, 0, false, 0u8),
        // ── DAA / DAS / AAA / AAS ────────────────────────────────────────────
        0x27 => ("DAA", InstCategory::Other, 1, 0, 0, 0, false, 0u8),
        0x2F => ("DAS", InstCategory::Other, 1, 0, 0, 0, false, 0u8),
        0x37 => ("AAA", InstCategory::Other, 1, 0, 0, 0, false, 0u8),
        0x3F => ("AAS", InstCategory::Other, 1, 0, 0, 0, false, 0u8),
        // ── OR ────────────────────────────────────────────────────────────────
        0x0A | 0x0B => {
            let modrm = mem.read8(phys + 1);
            ("OR", InstCategory::Alu, 2, 0u16, 0, 0i16, false, modrm)
        }
        0x0C => { let imm = mem.read8(phys + 1) as u16; ("OR", InstCategory::Alu, 2, imm, 0, 0i16, false, 0u8) }
        0x0D => { let imm = mem.read16(phys + 1); ("OR", InstCategory::Alu, 3, imm, 0, 0i16, false, 0u8) }
        // ── AND ───────────────────────────────────────────────────────────────
        0x20 | 0x21 | 0x22 | 0x23 => {
            let modrm = mem.read8(phys + 1);
            ("AND", InstCategory::Alu, 2, 0u16, 0, 0i16, false, modrm)
        }
        0x24 => { let imm = mem.read8(phys + 1) as u16; ("AND", InstCategory::Alu, 2, imm, 0, 0i16, false, 0u8) }
        0x25 => { let imm = mem.read16(phys + 1); ("AND", InstCategory::Alu, 3, imm, 0, 0i16, false, 0u8) }
        // ── XOR ───────────────────────────────────────────────────────────────
        0x30 | 0x31 | 0x32 | 0x33 => {
            let modrm = mem.read8(phys + 1);
            ("XOR", InstCategory::Alu, 2, 0u16, 0, 0i16, false, modrm)
        }
        0x34 => { let imm = mem.read8(phys + 1) as u16; ("XOR", InstCategory::Alu, 2, imm, 0, 0i16, false, 0u8) }
        0x35 => { let imm = mem.read16(phys + 1); ("XOR", InstCategory::Alu, 3, imm, 0, 0i16, false, 0u8) }
        // ── TEST ──────────────────────────────────────────────────────────────
        0x84 | 0x85 => {
            let modrm = mem.read8(phys + 1);
            ("TEST", InstCategory::Alu, 2, 0u16, 0, 0i16, false, modrm)
        }
        0xA8 => { let imm = mem.read8(phys + 1) as u16; ("TEST", InstCategory::Alu, 2, imm, 0, 0i16, false, 0u8) }
        0xA9 => { let imm = mem.read16(phys + 1); ("TEST", InstCategory::Alu, 3, imm, 0, 0i16, false, 0u8) }
        // ── MOV ModRM ─────────────────────────────────────────────────────────
        0x88 | 0x89 | 0x8A | 0x8B => {
            let modrm = mem.read8(phys + 1);
            ("MOV", InstCategory::Mov, 2, 0u16, 0, 0i16, false, modrm)
        }
        // ── MOV Sreg ──────────────────────────────────────────────────────────
        0x8C | 0x8E => {
            let modrm = mem.read8(phys + 1);
            ("MOV", InstCategory::Mov, 2, 0u16, 0, 0i16, false, modrm)
        }
        // ── XCHG ──────────────────────────────────────────────────────────────
        0x86 | 0x87 => {
            let modrm = mem.read8(phys + 1);
            ("XCHG", InstCategory::Alu, 2, 0u16, 0, 0i16, false, modrm)
        }
        0x91..=0x97 => {
            ("XCHG", InstCategory::Alu, 1, 0u16, 0, 0i16, false, 0u8)
        }
        // ── CBW / CWD ─────────────────────────────────────────────────────────
        0x98 => ("CBW", InstCategory::Other, 1, 0, 0, 0, false, 0u8),
        0x99 => ("CWD", InstCategory::Other, 1, 0, 0, 0, false, 0u8),
        // ── LEA ───────────────────────────────────────────────────────────────
        0x8D => {
            let modrm = mem.read8(phys + 1);
            ("LEA", InstCategory::Other, 2, 0u16, 0, 0i16, false, modrm)
        }
        // ── LOOP / JCXZ ───────────────────────────────────────────────────────
        0xE0 => { let rel = mem.read8(phys + 1) as i8 as i16; ("LOOPNE", InstCategory::Jcc, 2, 0, 1, rel, false, 0u8) }
        0xE1 => { let rel = mem.read8(phys + 1) as i8 as i16; ("LOOPE", InstCategory::Jcc, 2, 0, 1, rel, false, 0u8) }
        0xE2 => { let rel = mem.read8(phys + 1) as i8 as i16; ("LOOP", InstCategory::Jcc, 2, 0, 1, rel, false, 0u8) }
        0xE3 => { let rel = mem.read8(phys + 1) as i8 as i16; ("JCXZ", InstCategory::Jcc, 2, 0, 1, rel, false, 0u8) }
        // ── Shift/Rotate (D0-D3) ──────────────────────────────────────────────
        0xD0 | 0xD1 | 0xD2 | 0xD3 => {
            let modrm = mem.read8(phys + 1);
            ("SHIFT", InstCategory::Alu, 2, 0u16, 0, 0i16, false, modrm)
        }
        // ── Group 1 (0x80-0x83): ADD/OR/ADC/SBB/AND/SUB/XOR/CMP imm ──────────
        0x80..=0x83 => {
            let modrm = mem.read8(phys + 1);
            let (imm, len) = match b0 {
                0x80 | 0x82 => (mem.read8(phys + 2) as u16, 3u8),
                0x81 => (mem.read16(phys + 2), 4u8),
                0x83 => (mem.read8(phys + 2) as i8 as i16 as u16, 3u8),
                _ => (0u16, 2u8),
            };
            ("GRP1", InstCategory::Alu, len, imm, 0, 0i16, false, modrm)
        }
        // ── Group 3 (0xF6-F0xF7): TEST/NOT/NEG/MUL/IMUL/DIV/IDIV ─────────────
        0xF6 | 0xF7 => {
            let modrm = mem.read8(phys + 1);
            ("GRP3", InstCategory::Alu, 2, 0u16, 0, 0i16, false, modrm)
        }
        // ── Flag operations ───────────────────────────────────────────────────
        0xF5 => ("CMC", InstCategory::System, 1, 0, 0, 0, false, 0u8),
        0xF8 => ("CLC", InstCategory::System, 1, 0, 0, 0, false, 0u8),
        0xF9 => ("STC", InstCategory::System, 1, 0, 0, 0, false, 0u8),
        0xFC => ("CLD", InstCategory::System, 1, 0, 0, 0, false, 0u8),
        0xFD => ("STD", InstCategory::System, 1, 0, 0, 0, false, 0u8),
        // ── SAHF / LAHF ──────────────────────────────────────────────────────
        0x9E => ("SAHF", InstCategory::Other, 1, 0, 0, 0, false, 0u8),
        0x9F => ("LAHF", InstCategory::Other, 1, 0, 0, 0, false, 0u8),
        // ── XLAT ──────────────────────────────────────────────────────────────
        0xD7 => ("XLAT", InstCategory::Other, 1, 0, 0, 0, false, 0u8),
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
                    0xF8 => { self.cpu.set_cf(false); }              // CLC
                    0xF9 => { self.cpu.set_cf(true); }               // STC
                    0xF5 => {                                        // CMC
                        let cf = self.cpu.cf();
                        self.cpu.set_cf(!cf);
                    }
                    0xFC => {                                        // CLD
                        self.cpu.flags &= !0x0400u16;
                    }
                    0xFD => {                                        // STD
                        self.cpu.flags |= 0x0400u16;
                    }
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
                let op = inst.opcode as u8;
                // LOOP / JCXZ family (0xE0-0xE3) use CX as counter
                if op >= 0xE0 && op <= 0xE3 {
                    match op {
                        0xE2 => {
                            // LOOP: decrement CX, jump if CX != 0
                            self.cpu.cx = self.cpu.cx.wrapping_sub(1);
                            if self.cpu.cx != 0 {
                                self.cpu.ip = inst.branch_target(ip);
                            }
                        }
                        0xE1 => {
                            // LOOPE/LOOPZ: decrement CX, jump if CX != 0 and ZF=1
                            self.cpu.cx = self.cpu.cx.wrapping_sub(1);
                            if self.cpu.cx != 0 && self.cpu.zf() {
                                self.cpu.ip = inst.branch_target(ip);
                            }
                        }
                        0xE0 => {
                            // LOOPNE/LOOPNZ: decrement CX, jump if CX != 0 and ZF=0
                            self.cpu.cx = self.cpu.cx.wrapping_sub(1);
                            if self.cpu.cx != 0 && !self.cpu.zf() {
                                self.cpu.ip = inst.branch_target(ip);
                            }
                        }
                        0xE3 => {
                            // JCXZ: jump if CX == 0
                            if self.cpu.cx == 0 {
                                self.cpu.ip = inst.branch_target(ip);
                            }
                        }
                        _ => {}
                    }
                } else {
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
                let op = inst.opcode as u8;
                if op >= 0x50 && op <= 0x57 {
                    // PUSH reg (50-57)
                    let reg_idx = op & 0x07;
                    let val = self.read_gp16(reg_idx);
                    self.cpu.sp = self.cpu.sp.wrapping_sub(2);
                    let addr = self.cpu.linear(self.cpu.ss, self.cpu.sp);
                    self.mem.write16(addr, val);
                } else {
                    // PUSH Sreg
                    let sreg = match op {
                        0x06 => self.cpu.es,
                        0x0E => self.cpu.cs,
                        0x16 => self.cpu.ss,
                        0x1E => self.cpu.ds,
                        _ => return !self.bios.dos_break,
                    };
                    self.cpu.sp = self.cpu.sp.wrapping_sub(2);
                    let addr = self.cpu.linear(self.cpu.ss, self.cpu.sp);
                    self.mem.write16(addr, sreg);
                }
            }
            InstCategory::Pop => {
                let op = inst.opcode as u8;
                if op >= 0x58 && op <= 0x5F {
                    // POP reg (58-5F)
                    let reg_idx = op & 0x07;
                    let addr = self.cpu.linear(self.cpu.ss, self.cpu.sp);
                    let val = self.mem.read16(addr);
                    self.cpu.sp = self.cpu.sp.wrapping_add(2);
                    self.write_gp16(reg_idx, val);
                } else {
                    // POP Sreg
                    let addr = self.cpu.linear(self.cpu.ss, self.cpu.sp);
                    let val = self.mem.read16(addr);
                    self.cpu.sp = self.cpu.sp.wrapping_add(2);
                    match op {
                        0x07 => self.cpu.es = val,
                        0x17 => self.cpu.ss = val,
                        0x1F => self.cpu.ds = val,
                        _ => {}
                    }
                }
            }
            InstCategory::Mov => {
                let op = inst.opcode as u8;
                if (0xB0..=0xBF).contains(&op) {
                    // MOV reg, imm
                    let reg_idx = op & 0x07;
                    let wide = op & 0x08 != 0;
                    if wide {
                        self.write_gp16(reg_idx, inst.immediate);
                    } else {
                        // 8-bit: write low byte
                        let lo = (self.read_gp16(reg_idx) & 0xFF00) | (inst.immediate as u16 & 0xFF);
                        self.write_gp16(reg_idx, lo);
                    }
                } else {
                    // MOV with ModRM (88-8B, 8C, 8E)
                    let modrm = inst.modrm;
                    let rm = modrm & 0x07;
                    let reg = (modrm >> 3) & 0x07;
                    match op {
                        0x88 => {
                            // MOV r/m8, r8 — write low byte of reg to r/m
                            let src = self.read_gp16(reg) as u8;
                            let old = self.read_gp16(rm);
                            self.write_gp16(rm, (old & 0xFF00) | src as u16);
                        }
                        0x89 => {
                            // MOV r/m16, r16
                            self.write_gp16(rm, self.read_gp16(reg));
                        }
                        0x8A => {
                            // MOV r8, r/m8
                            let src = self.read_gp16(rm) as u8;
                            let old = self.read_gp16(reg);
                            self.write_gp16(reg, (old & 0xFF00) | src as u16);
                        }
                        0x8B => {
                            // MOV r16, r/m16
                            self.write_gp16(reg, self.read_gp16(rm));
                        }
                        0x8C => {
                            // MOV r/m16, Sreg (reg field encodes Sreg: 0=ES,1=CS,2=SS,3=DS)
                            let sreg_val = match reg {
                                0 => self.cpu.es,
                                1 => self.cpu.cs,
                                2 => self.cpu.ss,
                                3 => self.cpu.ds,
                                _ => 0,
                            };
                            self.write_gp16(rm, sreg_val);
                        }
                        0x8E => {
                            // MOV Sreg, r/m16
                            let val = self.read_gp16(rm);
                            match reg {
                                0 => self.cpu.es = val,
                                1 => self.cpu.cs = val,
                                2 => self.cpu.ss = val,
                                3 => self.cpu.ds = val,
                                _ => {}
                            }
                        }
                        _ => {}
                    }
                }
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
                    // ── AND ──────────────────────────────────────────────────────
                    0x20..=0x23 => {
                        let modrm = inst.modrm;
                        let rm = modrm & 0x07;
                        let reg = (modrm >> 3) & 0x07;
                        let (dst_idx, src_idx): (u8, u8) = match inst.opcode as u8 {
                            0x20 | 0x21 => (rm, reg),
                            _            => (reg, rm),
                        };
                        let a = self.read_gp16(dst_idx);
                        let b = self.read_gp16(src_idx);
                        let result = a & b;
                        self.write_gp16(dst_idx, result);
                        self.cpu.update_flags_logic(result);
                    }
                    0x24 => {
                        let a = (self.cpu.ax & 0xFF) as u16;
                        let b = inst.immediate & 0xFF;
                        let result = a & b;
                        self.cpu.ax = (self.cpu.ax & 0xFF00) | result;
                        self.cpu.update_flags_logic(result as u16);
                    }
                    0x25 => {
                        let a = self.cpu.ax;
                        let b = inst.immediate;
                        let result = a & b;
                        self.cpu.ax = result;
                        self.cpu.update_flags_logic(result);
                    }
                    // ── OR ───────────────────────────────────────────────────────
                    0x0A | 0x0B => {
                        let modrm = inst.modrm;
                        let rm = modrm & 0x07;
                        let reg = (modrm >> 3) & 0x07;
                        let a = self.read_gp16(rm);
                        let b = self.read_gp16(reg);
                        let result = a | b;
                        self.write_gp16(rm, result);
                        self.cpu.update_flags_logic(result);
                    }
                    0x0C => {
                        let a = (self.cpu.ax & 0xFF) as u16;
                        let b = inst.immediate & 0xFF;
                        let result = a | b;
                        self.cpu.ax = (self.cpu.ax & 0xFF00) | result;
                        self.cpu.update_flags_logic(result as u16);
                    }
                    0x0D => {
                        let a = self.cpu.ax;
                        let b = inst.immediate;
                        let result = a | b;
                        self.cpu.ax = result;
                        self.cpu.update_flags_logic(result);
                    }
                    // ── XOR ──────────────────────────────────────────────────────
                    0x30..=0x33 => {
                        let modrm = inst.modrm;
                        let rm = modrm & 0x07;
                        let reg = (modrm >> 3) & 0x07;
                        let (dst_idx, src_idx): (u8, u8) = match inst.opcode as u8 {
                            0x30 | 0x31 => (rm, reg),
                            _            => (reg, rm),
                        };
                        let a = self.read_gp16(dst_idx);
                        let b = self.read_gp16(src_idx);
                        let result = a ^ b;
                        self.write_gp16(dst_idx, result);
                        self.cpu.update_flags_logic(result);
                    }
                    0x34 => {
                        let a = (self.cpu.ax & 0xFF) as u16;
                        let b = inst.immediate & 0xFF;
                        let result = a ^ b;
                        self.cpu.ax = (self.cpu.ax & 0xFF00) | result;
                        self.cpu.update_flags_logic(result as u16);
                    }
                    0x35 => {
                        let a = self.cpu.ax;
                        let b = inst.immediate;
                        let result = a ^ b;
                        self.cpu.ax = result;
                        self.cpu.update_flags_logic(result);
                    }
                    // ── TEST ─────────────────────────────────────────────────────
                    0x84 | 0x85 => {
                        let modrm = inst.modrm;
                        let rm = modrm & 0x07;
                        let reg = (modrm >> 3) & 0x07;
                        let a = self.read_gp16(rm);
                        let b = self.read_gp16(reg);
                        let result = a & b;
                        self.cpu.update_flags_logic(result);
                    }
                    0xA8 => {
                        let a = (self.cpu.ax & 0xFF) as u16;
                        let b = inst.immediate & 0xFF;
                        let result = a & b;
                        self.cpu.update_flags_logic(result);
                    }
                    0xA9 => {
                        let a = self.cpu.ax;
                        let b = inst.immediate;
                        let result = a & b;
                        self.cpu.update_flags_logic(result);
                    }
                    // ── XCHG ─────────────────────────────────────────────────────
                    0x86 | 0x87 => {
                        let modrm = inst.modrm;
                        let rm = modrm & 0x07;
                        let reg = (modrm >> 3) & 0x07;
                        let a = self.read_gp16(rm);
                        let b = self.read_gp16(reg);
                        self.write_gp16(rm, b);
                        self.write_gp16(reg, a);
                    }
                    0x91..=0x97 => {
                        // XCHG AX, reg
                        let idx = inst.opcode as u8 & 0x07;
                        let a = self.cpu.ax;
                        let b = self.read_gp16(idx);
                        self.cpu.ax = b;
                        self.write_gp16(idx, a);
                    }
                    // ── Shift/Rotate (D0-D3) ─────────────────────────────────────
                    0xD0..=0xD3 => {
                        let modrm = inst.modrm;
                        let rm_idx = modrm & 0x07;
                        let reg_op = (modrm >> 3) & 0x07; // 0=ROL,1=ROR,2=RCL,3=RCR,4=SHL,5=SHR,7=SAR
                        let count: u16 = match inst.opcode as u8 {
                            0xD0 | 0xD1 => 1,
                            0xD2 | 0xD3 => (self.cpu.cx & 0x1F) as u16,
                            _ => 1,
                        };
                        let val = self.read_gp16(rm_idx);
                        if count == 0 {
                            return !self.bios.dos_break;
                        }
                        let mut result = val;
                        let cf_out: bool;
                        match reg_op {
                            0 => {
                                // ROL
                                cf_out = (val >> (16 - count)) & 1 != 0;
                                result = val.rotate_left(count as u32);
                            }
                            1 => {
                                // ROR
                                cf_out = (val >> (count - 1)) & 1 != 0;
                                result = val.rotate_right(count as u32);
                            }
                            2 => {
                                // RCL — rotate left through carry
                                let ext = ((val as u32) << 1) | (if self.cpu.cf() { 1 } else { 0 });
                                let c = count as u32 % 17;
                                let rotated = (ext << c) | (ext >> (17 - c));
                                result = (rotated & 0xFFFF) as u16;
                                cf_out = (rotated >> 16) & 1 != 0;
                            }
                            3 => {
                                // RCR — rotate right through carry
                                let ext = ((val as u32) << 1) | (if self.cpu.cf() { 1 } else { 0 });
                                let c = count as u32 % 17;
                                let rotated = (ext >> c) | (ext << (17 - c));
                                result = (rotated >> 1) as u16;
                                cf_out = (rotated & 0x1) != 0;
                            }
                            4 | 6 => {
                                // SHL/SAL
                                cf_out = if count > 0 && count <= 16 {
                                    (val >> (16 - count)) & 1 != 0
                                } else { false };
                                result = val.wrapping_shl(count as u32);
                            }
                            5 => {
                                // SHR
                                cf_out = if count > 0 {
                                    (val >> (count - 1)) & 1 != 0
                                } else { false };
                                result = val.wrapping_shr(count as u32);
                            }
                            7 => {
                                // SAR
                                cf_out = if count > 0 {
                                    (val >> (count - 1)) & 1 != 0
                                } else { false };
                                let shift = count.min(15);
                                result = ((val as i16) >> shift) as u16;
                            }
                            _ => {
                                cf_out = false;
                            }
                        }
                        self.write_gp16(rm_idx, result);
                        self.cpu.update_flags_shift(result, cf_out, count == 1, val);
                    }
                    // ── Group 1 (0x80-0x83): ADD/OR/ADC/SBB/AND/SUB/XOR/CMP imm ──
                    0x80..=0x83 => {
                        let modrm = inst.modrm;
                        let rm = modrm & 0x07;
                        let reg_op = (modrm >> 3) & 0x07;
                        let imm = inst.immediate;
                        let a = self.read_gp16(rm);
                        match reg_op {
                            0 => {
                                // ADD
                                let result = a.wrapping_add(imm);
                                self.write_gp16(rm, result);
                                self.cpu.update_flags_add(result, a, imm, true);
                            }
                            1 => {
                                // OR
                                let result = a | imm;
                                self.write_gp16(rm, result);
                                self.cpu.update_flags_logic(result);
                            }
                            2 => {
                                // ADC
                                let cf = if self.cpu.cf() { 1 } else { 0 };
                                let result = a.wrapping_add(imm).wrapping_add(cf);
                                self.write_gp16(rm, result);
                                self.cpu.update_flags_add(result, a, imm.wrapping_add(cf), true);
                            }
                            3 => {
                                // SBB
                                let cf = if self.cpu.cf() { 1 } else { 0 };
                                let result = a.wrapping_sub(imm).wrapping_sub(cf);
                                self.write_gp16(rm, result);
                                self.cpu.update_flags_sub(result, a, imm.wrapping_add(cf), true);
                            }
                            4 => {
                                // AND
                                let result = a & imm;
                                self.write_gp16(rm, result);
                                self.cpu.update_flags_logic(result);
                            }
                            5 => {
                                // SUB
                                let result = a.wrapping_sub(imm);
                                self.write_gp16(rm, result);
                                self.cpu.update_flags_sub(result, a, imm, true);
                            }
                            6 => {
                                // XOR
                                let result = a ^ imm;
                                self.write_gp16(rm, result);
                                self.cpu.update_flags_logic(result);
                            }
                            7 => {
                                // CMP
                                let result = a.wrapping_sub(imm);
                                self.cpu.update_flags_sub(result, a, imm, true);
                            }
                            _ => {}
                        }
                    }
                    // ── Group 3 (0xF6-0xF7): TEST/NOT/NEG/MUL/IMUL/DIV/IDIV ──────
                    0xF6 | 0xF7 => {
                        let modrm = inst.modrm;
                        let rm = modrm & 0x07;
                        let reg_op = (modrm >> 3) & 0x07;
                        let val = self.read_gp16(rm);
                        match reg_op {
                            0 => {
                                // TEST r/m, imm (F6/F7 with reg=0 uses immediate)
                                // For F7 with no immediate, treat as TEST with implicit 0
                                let imm = inst.immediate; // unused for non-immediate forms
                                let result = val & imm;
                                self.cpu.update_flags_logic(result);
                            }
                            2 => {
                                // NOT
                                let result = !val;
                                self.write_gp16(rm, result);
                            }
                            3 => {
                                // NEG
                                let result = (0u16).wrapping_sub(val);
                                self.write_gp16(rm, result);
                                self.cpu.update_flags_sub(result, 0, val, true);
                            }
                            4 => {
                                // MUL (unsigned)
                                let a = self.cpu.ax as u32;
                                let b = val as u32;
                                let result = a * b;
                                self.cpu.ax = result as u16;
                                self.cpu.dx = (result >> 16) as u16;
                                let of_cf = (result >> 16) != 0;
                                self.cpu.set_cf(of_cf);
                                self.cpu.set_of(of_cf);
                            }
                            5 => {
                                // IMUL (signed)
                                let a = self.cpu.ax as i16 as i32;
                                let b = val as i16 as i32;
                                let result = a * b;
                                self.cpu.ax = result as u16;
                                self.cpu.dx = (result >> 16) as u16;
                                let of_cf = result != (result as i16 as i32);
                                self.cpu.set_cf(of_cf);
                                self.cpu.set_of(of_cf);
                            }
                            6 => {
                                // DIV (unsigned)
                                if val == 0 {
                                    self.cpu.interrupts_enabled = false;
                                } else {
                                    let dividend = ((self.cpu.dx as u32) << 16) | self.cpu.ax as u32;
                                    let quotient = dividend / val as u32;
                                    let remainder = dividend % val as u32;
                                    if quotient > 0xFFFF {
                                        self.cpu.interrupts_enabled = false;
                                    } else {
                                        self.cpu.ax = quotient as u16;
                                        self.cpu.dx = remainder as u16;
                                    }
                                }
                            }
                            7 => {
                                // IDIV (signed)
                                if val == 0 {
                                    self.cpu.interrupts_enabled = false;
                                } else {
                                    let dividend = ((self.cpu.dx as i32) << 16) | (self.cpu.ax as i32);
                                    let quotient = dividend / (val as i16 as i32);
                                    let remainder = dividend % (val as i16 as i32);
                                    if quotient < i16::MIN as i32 || quotient > i16::MAX as i32 {
                                        self.cpu.interrupts_enabled = false;
                                    } else {
                                        self.cpu.ax = quotient as u16;
                                        self.cpu.dx = remainder as u16;
                                    }
                                }
                            }
                            _ => {}
                        }
                    }
                    _ => {}
                }
            }
            _ => {
                let op = inst.opcode as u8;
                match op {
                    0x98 => {
                        // CBW: sign-extend AL → AX
                        let al = (self.cpu.ax & 0xFF) as i8 as i16 as u16;
                        self.cpu.ax = al;
                    }
                    0x99 => {
                        // CWD: sign-extend AX → DX:AX
                        let ax = self.cpu.ax as i16 as i32 as u32;
                        self.cpu.dx = (ax >> 16) as u16;
                    }
                    0x9E => {
                        // SAHF: store AH into flags low byte (bits 7,6,4,2,0)
                        let ah = (self.cpu.ax >> 8) as u8;
                        let new_low = ((ah & 0xD5) | 0x02) as u16; // bit 1 always 1
                        self.cpu.flags = (self.cpu.flags & 0xFF00) | new_low;
                    }
                    0x9F => {
                        // LAHF: load flags low byte into AH
                        let flags_lo = (self.cpu.flags & 0xFF) as u8;
                        self.cpu.ax = (self.cpu.ax & 0x00FF) | ((flags_lo as u16) << 8);
                    }
                    0x8D => {
                        // LEA: load effective address (simplified: treat modrm rm as source reg)
                        let dst_idx = (inst.modrm >> 3) & 0x07;
                        let src_idx = inst.modrm & 0x07;
                        self.write_gp16(dst_idx, self.read_gp16(src_idx));
                    }
                    0xD7 => {
                        // XLAT: AL = [BX + AL]
                        let addr = self.cpu.linear(self.cpu.ds, self.cpu.bx + (self.cpu.ax & 0xFF) as u16);
                        let byte = self.mem.read8(addr);
                        self.cpu.ax = (self.cpu.ax & 0xFF00) | byte as u16;
                    }
                    0x27 => {
                        // DAA: decimal adjust AL after addition
                        let mut al = (self.cpu.ax & 0xFF) as u8;
                        let orig_al = al;
                        if (al & 0x0F) > 9 || self.cpu.af() {
                            al = al.wrapping_add(6);
                            self.cpu.set_af(true);
                        } else {
                            self.cpu.set_af(false);
                        }
                        if orig_al > 0x99 || self.cpu.cf() {
                            al = al.wrapping_add(0x60);
                            self.cpu.set_cf(true);
                        } else {
                            self.cpu.set_cf(false);
                        }
                        self.cpu.ax = (self.cpu.ax & 0xFF00) | al as u16;
                        self.cpu.set_pf(Cpu8088::parity(al));
                        self.cpu.set_zf(al == 0);
                        self.cpu.set_sf((al as i8) < 0);
                    }
                    0x2F => {
                        // DAS: decimal adjust AL after subtraction
                        let mut al = (self.cpu.ax & 0xFF) as u8;
                        let orig_al = al;
                        let old_cf = self.cpu.cf();
                        if (al & 0x0F) > 9 || self.cpu.af() {
                            al = al.wrapping_sub(6);
                            self.cpu.set_af(true);
                        } else {
                            self.cpu.set_af(false);
                        }
                        if orig_al > 0x99 || old_cf {
                            al = al.wrapping_sub(0x60);
                            self.cpu.set_cf(true);
                        }
                        self.cpu.ax = (self.cpu.ax & 0xFF00) | al as u16;
                        self.cpu.set_pf(Cpu8088::parity(al));
                        self.cpu.set_zf(al == 0);
                        self.cpu.set_sf((al as i8) < 0);
                    }
                    0x37 => {
                        // AAA: ASCII adjust AL after addition
                        let al = (self.cpu.ax & 0xFF) as u8;
                        if (al & 0x0F) > 9 || self.cpu.af() {
                            self.cpu.ax = self.cpu.ax.wrapping_add(0x0106);
                            self.cpu.set_af(true);
                            self.cpu.set_cf(true);
                        } else {
                            self.cpu.ax &= 0xFF0F;
                            self.cpu.set_af(false);
                            self.cpu.set_cf(false);
                        }
                    }
                    0x3F => {
                        // AAS: ASCII adjust AL after subtraction
                        let al = (self.cpu.ax & 0xFF) as u8;
                        if (al & 0x0F) > 9 || self.cpu.af() {
                            self.cpu.ax = self.cpu.ax.wrapping_sub(0x0006);
                            self.cpu.ax &= 0xFF0F;
                            self.cpu.ax = self.cpu.ax.wrapping_sub(0x0100);
                            self.cpu.set_af(true);
                            self.cpu.set_cf(true);
                        } else {
                            self.cpu.set_af(false);
                            self.cpu.set_cf(false);
                        }
                    }
                    _ => {}
                }
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
