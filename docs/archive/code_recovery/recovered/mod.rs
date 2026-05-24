#![allow(dead_code)]

use crate::ir::*;

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

    pub fn update_flags_arith(&mut self, result: BemiWord, operand1: BemiWord, operand2: BemiWord, is_sub: bool) {
        let mut flags = 0u64;
        if result == 0 { flags |= 0x40; }
        if result & (1u64 << 63) != 0 { flags |= 0x80; }
        let carry = if is_sub {
            operand1 < operand2
        } else {
            (BemiWord::MAX - operand1) < operand2
        };
        if carry { flags |= 0x01; }
        let overflow = if is_sub {
            ((operand1 ^ operand2) & (operand1 ^ result)) >> 63
        } else {
            ((operand1 ^ result) & (operand2 ^ result)) >> 
<truncated 74 bytes>
Ret => {
                let sp = self.regs.read(Register::RSP);
                let mut ret_addr = 0u64;
                if let Some(mr) = self.memory_read {
                    mr(sp, false, &mut ret_addr);
                }
                self.regs.write(Register::RSP, sp.wrapping_add(8));
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

            MicroOpCode::Cpuid | MicroOpCode::Rdmsr | MicroOpCode::Wrmsr |
            MicroOpCode::Hlt | MicroOpCode::Cli | MicroOpCode::Sti |
            MicroOpCode::Invlpg | MicroOpCode::Mfence |
            MicroOpCode::Xsave | MicroOpCode::Xrstor | MicroOpCode::Wbinvd |
            MicroOpCode::Syscall | MicroOpCode::Sysret |
            MicroOpCode::IntVec | MicroOpCode::Iret => {}

            MicroOpCode::MulRI | MicroOpCode::LoadRR | MicroOpCode::StoreRR |
            MicroOpCode::Movsxd |
            MicroOpCode::TraceExit | MicroOpCode::MacroOpFused | MicroOpCode::MacroOpPassthrough => {}
        }
    }
}
