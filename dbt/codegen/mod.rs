#![allow(dead_code)]

use crate::ir::*;

#[inline(always)]
fn is_r8_or_above(reg: Register) -> bool {
    let r = reg as u8;
    r >= 8 && r <= 15
}

pub const MAX_CODE_SIZE: usize = 4096;
pub const MAX_RELOCATIONS: usize = 64;

// ── Code Buffer ────────────────────────────────────────────────────────────────

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

    pub fn emit_rel32(&mut self, target: BemiWord, current: BemiWord) {
        let rel = target.wrapping_sub(current.wrapping_add(4)) as i32;
        self.emit_u32(rel as u32);
    }
}

// ── x64 Emitter ───────────────────────────────────────────────────────────────

pub struct X64Emitter;

impl X64Emitter {
    /// MOV dst, src  (REX.W 8B /r)
    pub fn emit_mov_rr(buf: &mut CodeBuffer, dst: Register, src: Register) {
        let w = true; let r = is_r8_or_above(dst); let b = is_r8_or_above(src);
        buf.emit_rex(w, r, false, b);
        buf.emit_u8(0x8B); buf.emit_modrm(3, Self::reg(dst), Self::reg(src));
    }

    /// MOV dst, imm64  (REX.W B8+rd io)
    pub fn emit_mov_ri(buf: &mut CodeBuffer, dst: Register, imm: u64) {
        let r = Self::reg(dst);
        let b = is_r8_or_above(dst);
        buf.emit_rex(true, false, false, b);
        buf.emit_u8(0xB8 | (r & 7));
        buf.emit_u64(imm);
    }

    /// TEST dst, src  (REX.W 85 /r)
    pub fn emit_test_rr(buf: &mut CodeBuffer, dst: Register, src: Register) {
        let w = true; let r = is_r8_or_above(dst); let b = is_r8_or_above(src);
        buf.emit_rex(w, r, false, b);
        buf.emit_u8(0x85); buf.emit_modrm(3, Self::reg(dst), Self::reg(src));
    }

    /// XCHG dst, src  (REX.W 87 /r)
    pub fn emit_xchg_rr(buf: &mut CodeBuffer, dst: Register, src: Register) {
        let w = true; let r = is_r8_or_above(dst); let b = is_r8_or_above(src);
        buf.emit_rex(w, r, false, b);
        buf.emit_u8(0x87); buf.emit_modrm(3, Self::reg(dst), Self::reg(src));
    }

    /// ADD dst, src  (REX.W 03 /r)
    pub fn emit_add_rr(buf: &mut CodeBuffer, dst: Register, src: Register) {
        let w = true; let r = is_r8_or_above(dst); let b = is_r8_or_above(src);
        buf.emit_rex(w, r, false, b);
        buf.emit_u8(0x03); buf.emit_modrm(3, Self::reg(dst), Self::reg(src));
    }

    /// ADD dst, imm32 sign-extended  (REX.W 81 /0 id)
    pub fn emit_add_ri(buf: &mut CodeBuffer, dst: Register, imm: i32) {
        let b = is_r8_or_above(dst);
        buf.emit_rex(true, false, false, b);
        buf.emit_u8(0x81); buf.emit_modrm(3, 0, Self::reg(dst));
        buf.emit_u32(imm as u32);
    }

    /// SUB dst, src  (REX.W 2B /r)
    pub fn emit_sub_rr(buf: &mut CodeBuffer, dst: Register, src: Register) {
        let w = true; let r = is_r8_or_above(dst); let b = is_r8_or_above(src);
        buf.emit_rex(w, r, false, b);
        buf.emit_u8(0x2B); buf.emit_modrm(3, Self::reg(dst), Self::reg(src));
    }

    /// SUB dst, imm32  (REX.W 81 /5 id)
    pub fn emit_sub_ri(buf: &mut CodeBuffer, dst: Register, imm: i32) {
        let b = is_r8_or_above(dst);
        buf.emit_rex(true, false, false, b);
        buf.emit_u8(0x81); buf.emit_modrm(3, 5, Self::reg(dst));
        buf.emit_u32(imm as u32);
    }

    /// AND dst, src  (REX.W 23 /r)
    pub fn emit_and_rr(buf: &mut CodeBuffer, dst: Register, src: Register) {
        let w = true; let r = is_r8_or_above(dst); let b = is_r8_or_above(src);
        buf.emit_rex(w, r, false, b);
        buf.emit_u8(0x23); buf.emit_modrm(3, Self::reg(dst), Self::reg(src));
    }

    /// OR dst, src  (REX.W 0B /r)
    pub fn emit_or_rr(buf: &mut CodeBuffer, dst: Register, src: Register) {
        let w = true; let r = is_r8_or_above(dst); let b = is_r8_or_above(src);
        buf.emit_rex(w, r, false, b);
        buf.emit_u8(0x0B); buf.emit_modrm(3, Self::reg(dst), Self::reg(src));
    }

    /// XOR dst, src  (REX.W 33 /r)
    pub fn emit_xor_rr(buf: &mut CodeBuffer, dst: Register, src: Register) {
        let w = true; let r = is_r8_or_above(dst); let b = is_r8_or_above(src);
        buf.emit_rex(w, r, false, b);
        buf.emit_u8(0x33); buf.emit_modrm(3, Self::reg(dst), Self::reg(src));
    }

    /// CMP dst, src  (REX.W 3B /r)
    pub fn emit_cmp_rr(buf: &mut CodeBuffer, dst: Register, src: Register) {
        let w = true; let r = is_r8_or_above(dst); let b = is_r8_or_above(src);
        buf.emit_rex(w, r, false, b);
        buf.emit_u8(0x3B); buf.emit_modrm(3, Self::reg(dst), Self::reg(src));
    }

    /// PUSH reg  (50+rd or REX 50+rd)
    pub fn emit_push_r(buf: &mut CodeBuffer, reg: Register) {
        let r = Self::reg(reg);
        if r >= 8 { buf.emit_rex(false, false, false, true); }
        buf.emit_u8(0x50 | (r & 7));
    }

    /// POP reg  (58+rd or REX 58+rd)
    pub fn emit_pop_r(buf: &mut CodeBuffer, reg: Register) {
        let r = Self::reg(reg);
        if r >= 8 { buf.emit_rex(false, false, false, true); }
        buf.emit_u8(0x58 | (r & 7));
    }

    /// RET near (C3)
    pub fn emit_ret(buf: &mut CodeBuffer) { buf.emit_u8(0xC3); }

    /// JMP rel32 (E9 cd)
    pub fn emit_jmp_rel32(buf: &mut CodeBuffer, target: BemiWord, current: BemiWord) {
        buf.emit_u8(0xE9); buf.emit_rel32(target, current);
    }

    /// CALL rel32 (E8 cd)
    pub fn emit_call_rel32(buf: &mut CodeBuffer, target: BemiWord, current: BemiWord) {
        buf.emit_u8(0xE8); buf.emit_rel32(target, current);
    }

    /// NOP (90)
    pub fn emit_nop(buf: &mut CodeBuffer) { buf.emit_u8(0x90); }

    /// Map Register enum to the 0-15 hardware encoding
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
