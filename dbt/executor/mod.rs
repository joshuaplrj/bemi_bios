#![allow(dead_code)]

use crate::ir::*;

// ── Register file ──────────────────────────────────────────────────────────────

#[derive(Debug, Clone)]
pub struct RegisterFile {
    pub regs: [BemiWord; 33],
}

impl RegisterFile {
    pub fn new() -> Self {
        RegisterFile { regs: [0; 33] }
    }

    pub fn read(&self, reg: Register) -> BemiWord {
        match reg {
            Register::RNone => 0,
            _ => self.regs[reg as usize],
        }
    }

    pub fn write(&mut self, reg: Register, value: BemiWord) {
        if reg != Register::RNone {
            self.regs[reg as usize] = value;
        }
    }

    pub fn get_flags(&self) -> BemiWord {
        self.regs[Register::RFlags as usize]
    }

    pub fn set_flags(&mut self, flags: BemiWord) {
        self.regs[Register::RFlags as usize] = flags;
    }

    /// Update EFLAGS-like status bits after an arithmetic operation.
    /// Bit 0 = CF, bit 6 = ZF, bit 7 = SF, bit 11 = OF.
    pub fn update_flags_arith(&mut self, result: BemiWord, operand1: BemiWord, operand2: BemiWord, is_sub: bool) {
        let mut flags = 0u64;
        if result == 0 { flags |= 0x40; }                          // ZF
        if result & (1u64 << 63) != 0 { flags |= 0x80; }           // SF
        let carry = if is_sub {
            operand1 < operand2
        } else {
            (BemiWord::MAX - operand1) < operand2
        };
        if carry { flags |= 0x01; }                                 // CF
        let overflow = if is_sub {
            ((operand1 ^ operand2) & (operand1 ^ result)) >> 63
        } else {
            ((operand1 ^ result) & (operand2 ^ result)) >> 63
        };
        if overflow != 0 { flags |= 0x0800; }                      // OF
        self.set_flags(flags);
    }

    pub fn update_flags_logic(&mut self, result: BemiWord) {
        let mut flags = 0u64;
        if result == 0 { flags |= 0x40; }                          // ZF
        if result & (1u64 << 63) != 0 { flags |= 0x80; }           // SF
        // CF and OF are cleared for logic ops
        self.set_flags(flags);
    }
}

// ── Executor ───────────────────────────────────────────────────────────────────

pub struct Executor<'a> {
    pub regs: RegisterFile,
    pub memory_read: Option<&'a mut dyn FnMut(u64, bool, &mut u64)>,
    pub memory_write: Option<&'a mut dyn FnMut(u64, bool, u64)>,
}

impl<'a> Executor<'a> {
    pub fn new() -> Self {
        Executor {
            regs: RegisterFile::new(),
            memory_read: None,
            memory_write: None,
        }
    }

    pub fn execute_block(&mut self, tb: &TranslationBlock) -> Result<Option<BemiWord>, &'static str> {
        let mut pc = 0;
        let mut next_ip: Option<BemiWord> = None;

        while pc < tb.micro_op_count as usize {
            let inst = tb.micro_ops[pc];
            pc += 1;
            self.execute_inst(&inst, &mut next_ip)?;
            if matches!(inst.code, MicroOpCode::Ret | MicroOpCode::JmpRel | MicroOpCode::JmpAbs
                                  | MicroOpCode::JccRel | MicroOpCode::CallRel | MicroOpCode::CallAbs
                                  | MicroOpCode::TraceExit | MicroOpCode::Hlt) {
                break;
            }
        }

        Ok(next_ip)
    }

    fn execute_inst(&mut self, inst: &MicroOpInst, next_ip: &mut Option<BemiWord>) -> Result<(), &'static str> {
        match inst.code {
            MicroOpCode::Nop => {}

            MicroOpCode::MovRR => {
                let val = self.regs.read(inst.src1);
                self.regs.write(inst.dst, val);
            }

            MicroOpCode::MovRI => {
                self.regs.write(inst.dst, inst.immediate);
            }

            MicroOpCode::MovRM => {
                let addr = self.regs.read(inst.src1).wrapping_add(inst.immediate);
                let mut val = 0u64;
                if let Some(mr) = self.memory_read.as_mut() {
                    let wide = inst.operand_size == 8;
                    mr(addr, wide, &mut val);
                }
                self.regs.write(inst.dst, val);
            }

            MicroOpCode::MovMR => {
                let addr = self.regs.read(inst.dst).wrapping_add(inst.immediate);
                let val = self.regs.read(inst.src1);
                if let Some(mw) = self.memory_write.as_mut() {
                    let wide = inst.operand_size == 8;
                    mw(addr, wide, val);
                }
            }

            MicroOpCode::MovMI => {
                let addr = self.regs.read(inst.dst).wrapping_add(inst.immediate);
                if let Some(mw) = self.memory_write.as_mut() {
                    let wide = inst.operand_size == 8;
                    mw(addr, wide, inst.src1 as u64);
                }
            }

            MicroOpCode::AddRR => {
                let a = self.regs.read(inst.src1);
                let b = self.regs.read(inst.src2);
                let result = a.wrapping_add(b);
                self.regs.write(inst.dst, result);
                self.regs.update_flags_arith(result, a, b, false);
            }

            MicroOpCode::AddRI => {
                let a = self.regs.read(inst.src1);
                let b = inst.immediate;
                let result = a.wrapping_add(b);
                self.regs.write(inst.dst, result);
                self.regs.update_flags_arith(result, a, b, false);
            }

            MicroOpCode::SubRR => {
                let a = self.regs.read(inst.src1);
                let b = self.regs.read(inst.src2);
                let result = a.wrapping_sub(b);
                self.regs.write(inst.dst, result);
                self.regs.update_flags_arith(result, a, b, true);
            }

            MicroOpCode::SubRI => {
                let a = self.regs.read(inst.src1);
                let b = inst.immediate;
                let result = a.wrapping_sub(b);
                self.regs.write(inst.dst, result);
                self.regs.update_flags_arith(result, a, b, true);
            }

            MicroOpCode::AdcRR => {
                let a = self.regs.read(inst.src1);
                let b = self.regs.read(inst.src2);
                let cf = self.regs.get_flags() & 0x01;
                let result = a.wrapping_add(b).wrapping_add(cf);
                self.regs.write(inst.dst, result);
                self.regs.update_flags_arith(result, a, b, false);
            }

            MicroOpCode::SbbRR => {
                let a = self.regs.read(inst.src1);
                let b = self.regs.read(inst.src2);
                let cf = self.regs.get_flags() & 0x01;
                let result = a.wrapping_sub(b).wrapping_sub(cf);
                self.regs.write(inst.dst, result);
                self.regs.update_flags_arith(result, a, b, true);
            }

            MicroOpCode::MulRR => {
                let a = self.regs.read(Register::RAX);
                let b = self.regs.read(inst.src1);
                let result = (a as u128) * (b as u128);
                self.regs.write(Register::RAX, result as u64);
                self.regs.write(Register::RDX, (result >> 64) as u64);
                let overflow = result > u64::MAX as u128;
                let mut flags = self.regs.get_flags() & !0x0801;
                if overflow { flags |= 0x0801; }
                self.regs.set_flags(flags);
            }

            MicroOpCode::ImulRR => {
                let a = self.regs.read(inst.src1) as i64;
                let b = self.regs.read(inst.src2) as i64;
                let result = (a as i128) * (b as i128);
                self.regs.write(inst.dst, result as u64);
                let overflow = result != result as i64 as i128;
                let mut flags = self.regs.get_flags() & !0x0801;
                if overflow { flags |= 0x0801; }
                self.regs.set_flags(flags);
            }

            MicroOpCode::ImulRI => {
                let a = self.regs.read(inst.src1) as i64;
                let b = inst.immediate as i64;
                let result = (a as i128) * (b as i128);
                self.regs.write(inst.dst, result as u64);
                let overflow = result != result as i64 as i128;
                let mut flags = self.regs.get_flags() & !0x0801;
                if overflow { flags |= 0x0801; }
                self.regs.set_flags(flags);
            }

            MicroOpCode::DivRR => {
                let divisor = self.regs.read(inst.src1);
                if divisor == 0 {
                    return Err("Division by zero");
                }
                let dividend_lo = self.regs.read(Register::RAX);
                let dividend_hi = self.regs.read(Register::RDX);
                let dividend = ((dividend_hi as u128) << 64) | (dividend_lo as u128);
                let quotient = (dividend / divisor as u128) as u64;
                let remainder = (dividend % divisor as u128) as u64;
                self.regs.write(Register::RAX, quotient);
                self.regs.write(Register::RDX, remainder);
            }

            MicroOpCode::IdivRR => {
                let divisor = self.regs.read(inst.src1) as i64;
                if divisor == 0 {
                    return Err("Signed division by zero");
                }
                let dividend_lo = self.regs.read(Register::RAX) as i64;
                let dividend_hi = self.regs.read(Register::RDX) as i64;
                let dividend = ((dividend_hi as i128) << 64) | (dividend_lo as i64 as i128);
                let quotient = (dividend / divisor as i128) as i64 as u64;
                let remainder = (dividend % divisor as i128) as i64 as u64;
                self.regs.write(Register::RAX, quotient);
                self.regs.write(Register::RDX, remainder);
            }

            MicroOpCode::CmpRR => {
                let a = self.regs.read(inst.src1);
                let b = self.regs.read(inst.src2);
                let result = a.wrapping_sub(b);
                self.regs.update_flags_arith(result, a, b, true);
            }

            MicroOpCode::CmpRI => {
                let a = self.regs.read(inst.src1);
                let b = inst.immediate;
                let result = a.wrapping_sub(b);
                self.regs.update_flags_arith(result, a, b, true);
            }

            MicroOpCode::TestRR => {
                let a = self.regs.read(inst.src1);
                let b = self.regs.read(inst.src2);
                let result = a & b;
                self.regs.update_flags_logic(result);
            }

            MicroOpCode::TestRI => {
                let a = self.regs.read(inst.src1);
                let b = inst.immediate;
                let result = a & b;
                self.regs.update_flags_logic(result);
            }

            MicroOpCode::AndRR => {
                let a = self.regs.read(inst.src1);
                let b = self.regs.read(inst.src2);
                let result = a & b;
                self.regs.write(inst.dst, result);
                self.regs.update_flags_logic(result);
            }

            MicroOpCode::OrRR => {
                let a = self.regs.read(inst.src1);
                let b = self.regs.read(inst.src2);
                let result = a | b;
                self.regs.write(inst.dst, result);
                self.regs.update_flags_logic(result);
            }

            MicroOpCode::XorRR => {
                let a = self.regs.read(inst.src1);
                let b = self.regs.read(inst.src2);
                let result = a ^ b;
                self.regs.write(inst.dst, result);
                self.regs.update_flags_logic(result);
            }

            MicroOpCode::NotR => {
                let a = self.regs.read(inst.src1);
                self.regs.write(inst.dst, !a);
            }

            MicroOpCode::NegR => {
                let a = self.regs.read(inst.src1);
                let result = 0u64.wrapping_sub(a);
                self.regs.write(inst.dst, result);
                self.regs.update_flags_arith(result, 0, a, true);
            }

            MicroOpCode::IncR => {
                let a = self.regs.read(inst.src1);
                let result = a.wrapping_add(1);
                self.regs.write(inst.dst, result);
                // INC does not modify CF — preserve it
                let cf = self.regs.get_flags() & 0x01;
                self.regs.update_flags_arith(result, a, 1, false);
                let flags = (self.regs.get_flags() & !0x01) | cf;
                self.regs.set_flags(flags);
            }

            MicroOpCode::DecR => {
                let a = self.regs.read(inst.src1);
                let result = a.wrapping_sub(1);
                self.regs.write(inst.dst, result);
                // DEC does not modify CF — preserve it
                let cf = self.regs.get_flags() & 0x01;
                self.regs.update_flags_arith(result, a, 1, true);
                let flags = (self.regs.get_flags() & !0x01) | cf;
                self.regs.set_flags(flags);
            }

            MicroOpCode::ShlRR => {
                let a = self.regs.read(inst.src1);
                let shift = (self.regs.read(inst.src2) & 0x3F) as u32;
                let result = if shift >= 64 { 0 } else { a << shift };
                self.regs.write(inst.dst, result);
                self.regs.update_flags_logic(result);
            }

            MicroOpCode::ShrRR => {
                let a = self.regs.read(inst.src1);
                let shift = (self.regs.read(inst.src2) & 0x3F) as u32;
                let result = if shift >= 64 { 0 } else { a >> shift };
                self.regs.write(inst.dst, result);
                self.regs.update_flags_logic(result);
            }

            MicroOpCode::SarRR => {
                let a = self.regs.read(inst.src1) as i64;
                let shift = (self.regs.read(inst.src2) & 0x3F) as u32;
                let result = if shift >= 64 { if a < 0 { -1i64 } else { 0 } } else { a >> shift } as u64;
                self.regs.write(inst.dst, result);
                self.regs.update_flags_logic(result);
            }

            MicroOpCode::RolRI => {
                let a = self.regs.read(inst.src1);
                let shift = (inst.immediate & 63) as u32;
                let result = a.rotate_left(shift);
                self.regs.write(inst.dst, result);
            }

            MicroOpCode::RorRI => {
                let a = self.regs.read(inst.src1);
                let shift = (inst.immediate & 63) as u32;
                let result = a.rotate_right(shift);
                self.regs.write(inst.dst, result);
            }

            MicroOpCode::RclRI | MicroOpCode::RcrRI => {
                // Simplified: treat as rotate (no CF threading)
                let a = self.regs.read(inst.src1);
                let shift = (inst.immediate & 63) as u32;
                let result = if matches!(inst.code, MicroOpCode::RclRI) {
                    a.rotate_left(shift)
                } else {
                    a.rotate_right(shift)
                };
                self.regs.write(inst.dst, result);
            }

            MicroOpCode::BtRI | MicroOpCode::BtrRI | MicroOpCode::BtsRI | MicroOpCode::BtcRI => {
                let base = self.regs.read(inst.src1);
                let bit = inst.immediate & 63;
                let mask = 1u64 << bit;
                let cf = (base >> bit) & 1;
                let mut flags = self.regs.get_flags() & !0x01;
                flags |= cf;
                self.regs.set_flags(flags);
                let result = match inst.code {
                    MicroOpCode::BtRI => base,
                    MicroOpCode::BtrRI => base & !mask,
                    MicroOpCode::BtsRI => base | mask,
                    MicroOpCode::BtcRI => base ^ mask,
                    _ => unreachable!(),
                };
                if !matches!(inst.code, MicroOpCode::BtRI) {
                    self.regs.write(inst.dst, result);
                }
            }

            MicroOpCode::BsfRR => {
                let src = self.regs.read(inst.src1);
                if src == 0 {
                    let flags = self.regs.get_flags() | 0x40;  // ZF=1
                    self.regs.set_flags(flags);
                } else {
                    self.regs.write(inst.dst, src.trailing_zeros() as u64);
                    let flags = self.regs.get_flags() & !0x40;  // ZF=0
                    self.regs.set_flags(flags);
                }
            }

            MicroOpCode::BsrRR => {
                let src = self.regs.read(inst.src1);
                if src == 0 {
                    let flags = self.regs.get_flags() | 0x40;  // ZF=1
                    self.regs.set_flags(flags);
                } else {
                    self.regs.write(inst.dst, (63 - src.leading_zeros()) as u64);
                    let flags = self.regs.get_flags() & !0x40;  // ZF=0
                    self.regs.set_flags(flags);
                }
            }

            MicroOpCode::PopcntRR => {
                let src = self.regs.read(inst.src1);
                let count = src.count_ones() as u64;
                self.regs.write(inst.dst, count);
                let flags = if count == 0 { self.regs.get_flags() | 0x40 } else { self.regs.get_flags() & !0x40 };
                self.regs.set_flags(flags);
            }

            MicroOpCode::LeaRR => {
                // LEA: dst = src1 + immediate (address calculation, no memory access)
                let base = self.regs.read(inst.src1);
                let result = base.wrapping_add(inst.immediate);
                self.regs.write(inst.dst, result);
            }

            MicroOpCode::LoadRR => {
                let addr = self.regs.read(inst.src1).wrapping_add(self.regs.read(inst.src2));
                let mut val = 0u64;
                if let Some(mr) = self.memory_read.as_mut() {
                    let wide = inst.operand_size == 8;
                    mr(addr, wide, &mut val);
                }
                self.regs.write(inst.dst, val);
            }

            MicroOpCode::StoreRR => {
                let addr = self.regs.read(inst.dst).wrapping_add(self.regs.read(inst.src2));
                let val = self.regs.read(inst.src1);
                if let Some(mw) = self.memory_write.as_mut() {
                    let wide = inst.operand_size == 8;
                    mw(addr, wide, val);
                }
            }

            MicroOpCode::MovsxRR => {
                let src = self.regs.read(inst.src1);
                let result = match inst.operand_size {
                    1 => src as i8 as i64 as u64,
                    2 => src as i16 as i64 as u64,
                    4 => src as i32 as i64 as u64,
                    _ => src,
                };
                self.regs.write(inst.dst, result);
            }

            MicroOpCode::MovzxRR => {
                let src = self.regs.read(inst.src1);
                let result = match inst.operand_size {
                    1 => src & 0xFF,
                    2 => src & 0xFFFF,
                    4 => src & 0xFFFFFFFF,
                    _ => src,
                };
                self.regs.write(inst.dst, result);
            }

            MicroOpCode::Movsxd => {
                let src = self.regs.read(inst.src1) as i32 as i64 as u64;
                self.regs.write(inst.dst, src);
            }

            MicroOpCode::PushR => {
                let val = self.regs.read(inst.src1);
                let rsp = self.regs.read(Register::RSP).wrapping_sub(8);
                self.regs.write(Register::RSP, rsp);
                if let Some(mw) = self.memory_write.as_mut() {
                    mw(rsp, true, val);
                }
            }

            MicroOpCode::PopR => {
                let rsp = self.regs.read(Register::RSP);
                let mut val = 0u64;
                if let Some(mr) = self.memory_read.as_mut() {
                    mr(rsp, true, &mut val);
                }
                self.regs.write(Register::RSP, rsp.wrapping_add(8));
                self.regs.write(inst.dst, val);
            }

            MicroOpCode::JmpRel => {
                let ip = self.regs.read(Register::RAX); // current IP stored in RAX for tracing
                let target = ip.wrapping_add(inst.immediate);
                *next_ip = Some(target);
            }

            MicroOpCode::JmpAbs => {
                let target = self.regs.read(inst.src1);
                *next_ip = Some(target);
            }

            MicroOpCode::JccRel => {
                let flags = self.regs.get_flags();
                let zf = (flags >> 6) & 1;
                let sf = (flags >> 7) & 1;
                let cf = flags & 1;
                let of = (flags >> 11) & 1;
                // inst.src2 encodes the condition code (same as x86 Jcc condition values)
                let condition = inst.src2 as u8;
                let taken = match condition {
                    0x0 => of != 0,         // JO
                    0x1 => of == 0,         // JNO
                    0x2 => cf != 0,         // JB/JC/JNAE
                    0x3 => cf == 0,         // JAE/JNB/JNC
                    0x4 => zf != 0,         // JE/JZ
                    0x5 => zf == 0,         // JNE/JNZ
                    0x6 => cf != 0 || zf != 0,  // JBE/JNA
                    0x7 => cf == 0 && zf == 0,  // JA/JNBE
                    0x8 => sf != 0,         // JS
                    0x9 => sf == 0,         // JNS
                    0xC => sf != of,        // JL/JNGE
                    0xD => sf == of,        // JGE/JNL
                    0xE => zf != 0 || sf != of, // JLE/JNG
                    0xF => zf == 0 && sf == of, // JG/JNLE
                    _ => false,
                };
                if taken {
                    let ip = self.regs.read(Register::RAX);
                    *next_ip = Some(ip.wrapping_add(inst.immediate));
                }
            }

            MicroOpCode::CallRel => {
                let ip = self.regs.read(Register::RAX);
                let ret_addr = ip; // return address = next instruction
                let rsp = self.regs.read(Register::RSP).wrapping_sub(8);
                self.regs.write(Register::RSP, rsp);
                if let Some(mw) = self.memory_write.as_mut() {
                    mw(rsp, true, ret_addr);
                }
                *next_ip = Some(ip.wrapping_add(inst.immediate));
            }

            MicroOpCode::CallAbs => {
                let ip = self.regs.read(Register::RAX);
                let ret_addr = ip;
                let rsp = self.regs.read(Register::RSP).wrapping_sub(8);
                self.regs.write(Register::RSP, rsp);
                if let Some(mw) = self.memory_write.as_mut() {
                    mw(rsp, true, ret_addr);
                }
                *next_ip = Some(self.regs.read(inst.src1));
            }

            MicroOpCode::Ret => {
                let rsp = self.regs.read(Register::RSP);
                let mut ret_addr = 0u64;
                if let Some(mr) = self.memory_read.as_mut() {
                    mr(rsp, true, &mut ret_addr);
                }
                self.regs.write(Register::RSP, rsp.wrapping_add(8));
                *next_ip = Some(ret_addr);
            }

            MicroOpCode::Cmovcc => {
                let flags = self.regs.get_flags();
                if flags != 0 {
                    let val = self.regs.read(inst.src1);
                    self.regs.write(inst.dst, val);
                }
            }

            MicroOpCode::Setcc => {
                let flags = self.regs.get_flags();
                let val = if flags != 0 { 1u64 } else { 0u64 };
                self.regs.write(inst.dst, val);
            }

            MicroOpCode::Cdq | MicroOpCode::Cqo => {
                let rax = self.regs.read(Register::RAX);
                if rax & 0x80000000_00000000 != 0 {
                    self.regs.write(Register::RDX, !0u64);
                } else {
                    self.regs.write(Register::RDX, 0);
                }
            }

            MicroOpCode::XchgRR => {
                let a = self.regs.read(inst.src1);
                let b = self.regs.read(inst.src2);
                self.regs.write(inst.src1, b);
                self.regs.write(inst.src2, a);
            }

            MicroOpCode::CmpxchgRR => {
                let acc = self.regs.read(Register::RAX);
                let mem = self.regs.read(inst.src1);
                if acc == mem {
                    self.regs.write(inst.src1, self.regs.read(inst.src2));
                    let flags = self.regs.get_flags() | 0x40;  // ZF=1
                    self.regs.set_flags(flags);
                } else {
                    self.regs.write(Register::RAX, mem);
                    let flags = self.regs.get_flags() & !0x40;  // ZF=0
                    self.regs.set_flags(flags);
                }
            }

            MicroOpCode::XaddRR => {
                let a = self.regs.read(inst.src1);
                let b = self.regs.read(inst.src2);
                let result = a.wrapping_add(b);
                self.regs.write(inst.src2, a); // src2 gets old src1
                self.regs.write(inst.src1, result);
                self.regs.update_flags_arith(result, a, b, false);
            }

            MicroOpCode::Cpuid | MicroOpCode::Rdmsr | MicroOpCode::Wrmsr |
            MicroOpCode::Hlt | MicroOpCode::Cli | MicroOpCode::Sti |
            MicroOpCode::Invlpg | MicroOpCode::Mfence |
            MicroOpCode::Xsave | MicroOpCode::Xrstor | MicroOpCode::Wbinvd |
            MicroOpCode::Syscall | MicroOpCode::Sysret |
            MicroOpCode::IntVec | MicroOpCode::Iret => {}

            MicroOpCode::MulRI | MicroOpCode::TraceExit |
            MicroOpCode::MacroOpFused | MicroOpCode::MacroOpPassthrough => {}
        }

        Ok(())
    }
}
