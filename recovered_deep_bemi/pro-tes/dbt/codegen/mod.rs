#![allow(dead_code)]

use crate::ir::*;
use crate::decoder::is_r8_or_above;

pub const MAX_CODE_SIZE: usize = 4096;
pub const MAX_RELOCATIONS: usize = 64;

pub struct CodeBuffer {
    pub bytes: [u8; MAX_CODE_SIZE],
    pub len: usize,
    reloc_offsets: [usize; MAX_RELOCATIONS],
    reloc_targets: [BemiWord; MAX_RELOCATIONS],
    reloc_count: usize,
}

impl CodeBuffer {
    pub fn new() -> Self {
        CodeBuffer {
            bytes: [0; MAX_CODE_SIZE], len: 0,
            reloc_offsets: [0; MAX_RELOCATIONS],
            reloc_targets: [0; MAX_RELOCATIONS],
            reloc_count: 0,
        }
    }

    pub fn emit_u8(&mut self, v: u8) {
        if self.len < MAX_CODE_SIZE { self.bytes[self.len] = v; self.len += 1; }
    }

    pub fn emit_u32(&mut self, v: u32) {
        let b = v.to_le_bytes();
        for i in 0..4 { self.emit_u8(b[i]); }
    }

    pub fn emit_u64(&mut self, v: u64) {
        let b = v.to_le_bytes();
        for i in 0..8 { self.emit_u8(b[i]); }
    }

    pub fn emit_rex(&mut self, w: bool, r: bool, x: bool, b: bool) {
        let mut rex = 0x40u8;
        if w { rex |= 0x08; } if r { rex |= 0x04; } if x { rex |= 0x02; } if b { rex |= 0x01; }
        if rex != 0x40 { self.emit_u8(rex); }
    }

    pub fn emit_modrm(&mut self, r#mod: u8, reg: u8, rm: u8) {
        self.emit_u8((r#mod << 6) | ((reg & 7) << 3) | (rm & 7));
    }

51:
<truncated 3928 bytes>
w = true; let r = is_r8_or_above(dst); let b = is_r8_or_above(src);
        buf.emit_rex(w, r, false, b);
        buf.emit_u8(0x85); buf.emit_modrm(3, Self::reg(dst), Self::reg(src));
    }

    pub fn emit_xchg_rr(buf: &mut CodeBuffer, dst: Register, src: Register) {
        let w = true; let r = is_r8_or_above(dst); let b = is_r8_or_above(src);
        buf.emit_rex(w, r, false, b);
        buf.emit_u8(0x87); buf.emit_modrm(3, Self::reg(dst), Self::reg(src));
    }

    pub fn emit_push_r(buf: &mut CodeBuffer, reg: Register) {
        let r = Self::reg(reg);
        if r >= 8 { buf.emit_rex(false, false, false, true); }
        buf.emit_u8(0x50 | (r & 7));
    }

    pub fn emit_pop_r(buf: &mut CodeBuffer, reg: Register) {
        let r = Self::reg(reg);
        if r >= 8 { buf.emit_rex(false, false, false, true); }
        buf.emit_u8(0x58 | (r & 7));
    }

    pub fn emit_ret(buf: &mut CodeBuffer) { buf.emit_u8(0xC3); }

    pub fn emit_jmp_rel32(buf: &mut CodeBuffer, target: BemiWord, current: BemiWord) {
        buf.emit_u8(0xE9); buf.emit_rel32(target, current);
    }

    pub fn emit_call_rel32(buf: &mut CodeBuffer, target: BemiWord, current: BemiWord) {
        buf.emit_u8(0xE8); buf.emit_rel32(target, current);
    }

    pub fn emit_nop(buf: &mut CodeBuffer) { buf.emit_u8(0x90); }

    fn reg(reg: Register) -> u8 {
        match reg {
            Register::RAX | Register::R8 => 0, Register::RCX | Register::R9 => 1,
            Register::RDX | Register::R10 => 2, Register::RBX | Register::R11 => 3,
            Register::RSP | Register::R12 => 4, Register::RBP | Register::R13 => 5,
            Register::RSI | Register::R14 => 6, Register::RDI | Register::R15 => 7,
            _ => 0,
        }
    }
}
