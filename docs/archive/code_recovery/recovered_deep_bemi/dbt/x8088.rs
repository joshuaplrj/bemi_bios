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
    /// Whether this is a FAR (inter-segment) control tran
<truncated 77 bytes>
alysis
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
The above content does NOT show the entire file contents. If you need to view any lines of the file which were not shown to complete your task, call this tool again to view those lines.