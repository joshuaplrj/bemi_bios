"#![allow(non_camel_case_types)]
#![allow(dead_code)]

pub type BemiWord = u64;

pub const MAX_MICRO_OPS_PER_BLOCK: usize = 256;
pub const MAX_CODE_SIZE: usize = 4096;

pub const X86_ROB_ENTRY_BYTES: usize = 14;
pub const RISC_ROB_ENTRY_BYTES: usize = 4;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[repr(u8)]
pub enum Register {
    RAX = 0, RCX = 1, RDX = 2, RBX = 3,
    RSP = 4, RBP = 5, RSI = 6, RDI = 7,
    R8  = 8, R9  = 9, R10 = 10, R11 = 11,
    R12 = 12, R13 = 13, R14 = 14, R15 = 15,
    RTmp0 = 16, RTmp1 = 17, RTmp2 = 18, RTmp3 = 19,
    RTmp4 = 20, RTmp5 = 21, RTmp6 = 22, RTmp7 = 23,
    RTmp8 = 24, RTmp9 = 25, RTmpA = 26, RTmpB = 27,
    RTmpC = 28, RTmpD = 29, RTmpE = 30, RTmpF = 31,
    RFlags = 32,
    RNone = 0xFF,
}

impl Register {
    pub fn from_u8(val: u8) -> Self {
        match val {
            0 => Register::RAX,
            1 => Register::RCX,
            2 => Register::RDX,
            3 => Register::RBX,
            4 => Register::RSP,
            5 => Register::RBP,
            6 => Register::RSI,
            7 => Register::RDI,
            8 => Register::R8,
            9 => Register::R9,
            10 => Register::R10,
            11 => Register::R11,
            12 => Register::R12,
            13 => Register::R13,
            14 => Register::R14,
            15 => Register::R15,
            16 => Register::RTmp0,
            17 => Register::RTmp1,
            18 => Register::RTmp2,
            19 => Register::RTmp3,
            20 => Register::RTmp4,
            21 => Register::RTmp5,
            22 => Register::RTmp6,
            23 => Register::RTmp7,
            24 => Register::RTmp8,
            25 => Register::RTmp9,
            26 => Register::RTmpA,
            27 => Register::RTmpB,
            28 => Register::RTmpC,
            29 => Register::RTmpD,
            30 => Register::RTmpE,
            31 => Register::RTmpF,
            32 => Register::RFlags,
            _ => Register::RNone,
        }
    }
}\
<truncated 3782 bytes>