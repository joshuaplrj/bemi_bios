"#![allow(dead_code)]

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
            ((operand1 ^ result) & (operand2 ^ result)) >> 63
        };
        if overflow != 0 { flags |= 0x0800; }
        self.set_flags(flags);
    }
}

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

    pub fn execute_block(&mut self, tb: &TranslationBlock) -> Result<Option<BemiWord>, String> {
        let mut pc = 0;
        let mut next_ip: Opti
<truncated 11800 bytes>