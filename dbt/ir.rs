#![allow(non_camel_case_types)]

pub type BemiWord = u64;

pub const MAX_MICRO_OPS_PER_BLOCK: usize = 256;

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
    pub fn from_u8(v: u8) -> Self {
        match v {
            0 => Register::RAX, 1 => Register::RCX, 2 => Register::RDX, 3 => Register::RBX,
            4 => Register::RSP, 5 => Register::RBP, 6 => Register::RSI, 7 => Register::RDI,
            8 => Register::R8,  9 => Register::R9,  10 => Register::R10, 11 => Register::R11,
            12 => Register::R12, 13 => Register::R13, 14 => Register::R14, 15 => Register::R15,
            16 => Register::RTmp0, 17 => Register::RTmp1, 18 => Register::RTmp2,
            19 => Register::RTmp3, 20 => Register::RTmp4, 21 => Register::RTmp5,
            22 => Register::RTmp6, 23 => Register::RTmp7, 24 => Register::RTmp8,
            25 => Register::RTmp9, 26 => Register::RTmpA, 27 => Register::RTmpB,
            28 => Register::RTmpC, 29 => Register::RTmpD, 30 => Register::RTmpE,
            31 => Register::RTmpF, 32 => Register::RFlags,
            _ => Register::RNone,
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[repr(u8)]
pub enum MicroOpCode {
    Nop = 0,
    MovRR = 1, MovRI = 2, MovRM = 3, MovMR = 4, MovMI = 5,
    AddRR = 6, AddRI = 7, SubRR = 8, SubRI = 9,
    AdcRR = 10, SbbRR = 11,
    MulRR = 12, MulRI = 13, DivRR = 14, IdivRR = 15,
    ImulRR = 16, ImulRI = 17,
    CmpRR = 18, CmpRI = 19,
    JmpRel = 20, JmpAbs = 21, JccRel = 22,
    CallRel = 23, CallAbs = 24, Ret = 25,
    PushR = 26, PopR = 27,
    LoadRR = 28, StoreRR = 29,
    AndRR = 30, OrRR = 31, XorRR = 32,
    ShlRR = 33, ShrRR = 34, SarRR = 35,
    NotR = 36, NegR = 37, IncR = 38, DecR = 39,
    LeaRR = 40,
    MovsxRR = 41, MovzxRR = 42,
    Cmovcc = 43, Setcc = 44,
    Cdq = 45, Cqo = 46,
    Cpuid = 47, Rdmsr = 48, Wrmsr = 49,
    Hlt = 50, Cli = 51, Sti = 52,
    Invlpg = 53, Mfence = 54,
    Xsave = 55, Xrstor = 56, Wbinvd = 57,
    Syscall = 58, Sysret = 59,
    IntVec = 60, Iret = 61,
    Movsxd = 62,
    TraceExit = 63,
    MacroOpFused = 64, MacroOpPassthrough = 65,
    TestRR = 66, TestRI = 67,
    BsfRR = 68, BsrRR = 69,
    PopcntRR = 70,
    RolRI = 71, RorRI = 72, RclRI = 73, RcrRI = 74,
    BtRI = 75, BtrRI = 76, BtsRI = 77, BtcRI = 78,
    CmpxchgRR = 79,
    XaddRR = 80,
    XchgRR = 81,
}

impl MicroOpCode {
    pub fn from_u8(v: u8) -> Self {
        match v {
            0 => MicroOpCode::Nop,
            1 => MicroOpCode::MovRR, 2 => MicroOpCode::MovRI, 3 => MicroOpCode::MovRM,
            4 => MicroOpCode::MovMR, 5 => MicroOpCode::MovMI,
            6 => MicroOpCode::AddRR, 7 => MicroOpCode::AddRI,
            8 => MicroOpCode::SubRR, 9 => MicroOpCode::SubRI,
            10 => MicroOpCode::AdcRR, 11 => MicroOpCode::SbbRR,
            12 => MicroOpCode::MulRR, 13 => MicroOpCode::MulRI,
            14 => MicroOpCode::DivRR, 15 => MicroOpCode::IdivRR,
            16 => MicroOpCode::ImulRR, 17 => MicroOpCode::ImulRI,
            18 => MicroOpCode::CmpRR, 19 => MicroOpCode::CmpRI,
            20 => MicroOpCode::JmpRel, 21 => MicroOpCode::JmpAbs, 22 => MicroOpCode::JccRel,
            23 => MicroOpCode::CallRel, 24 => MicroOpCode::CallAbs, 25 => MicroOpCode::Ret,
            26 => MicroOpCode::PushR, 27 => MicroOpCode::PopR,
            28 => MicroOpCode::LoadRR, 29 => MicroOpCode::StoreRR,
            30 => MicroOpCode::AndRR, 31 => MicroOpCode::OrRR, 32 => MicroOpCode::XorRR,
            33 => MicroOpCode::ShlRR, 34 => MicroOpCode::ShrRR, 35 => MicroOpCode::SarRR,
            36 => MicroOpCode::NotR, 37 => MicroOpCode::NegR,
            38 => MicroOpCode::IncR, 39 => MicroOpCode::DecR,
            40 => MicroOpCode::LeaRR,
            41 => MicroOpCode::MovsxRR, 42 => MicroOpCode::MovzxRR,
            43 => MicroOpCode::Cmovcc, 44 => MicroOpCode::Setcc,
            45 => MicroOpCode::Cdq, 46 => MicroOpCode::Cqo,
            47 => MicroOpCode::Cpuid, 48 => MicroOpCode::Rdmsr, 49 => MicroOpCode::Wrmsr,
            50 => MicroOpCode::Hlt, 51 => MicroOpCode::Cli, 52 => MicroOpCode::Sti,
            53 => MicroOpCode::Invlpg, 54 => MicroOpCode::Mfence,
            55 => MicroOpCode::Xsave, 56 => MicroOpCode::Xrstor, 57 => MicroOpCode::Wbinvd,
            58 => MicroOpCode::Syscall, 59 => MicroOpCode::Sysret,
            60 => MicroOpCode::IntVec, 61 => MicroOpCode::Iret,
            62 => MicroOpCode::Movsxd,
            63 => MicroOpCode::TraceExit,
            64 => MicroOpCode::MacroOpFused, 65 => MicroOpCode::MacroOpPassthrough,
            66 => MicroOpCode::TestRR, 67 => MicroOpCode::TestRI,
            68 => MicroOpCode::BsfRR, 69 => MicroOpCode::BsrRR,
            70 => MicroOpCode::PopcntRR,
            71 => MicroOpCode::RolRI, 72 => MicroOpCode::RorRI,
            73 => MicroOpCode::RclRI, 74 => MicroOpCode::RcrRI,
            75 => MicroOpCode::BtRI, 76 => MicroOpCode::BtrRI,
            77 => MicroOpCode::BtsRI, 78 => MicroOpCode::BtcRI,
            79 => MicroOpCode::CmpxchgRR,
            80 => MicroOpCode::XaddRR,
            81 => MicroOpCode::XchgRR,
            _ => MicroOpCode::Nop,
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct MicroOpInst {
    pub code: MicroOpCode,
    pub dst: Register,
    pub src1: Register,
    pub src2: Register,
    pub immediate: BemiWord,
    pub operand_size: u8,
}

impl MicroOpInst {
    pub fn new(code: MicroOpCode, dst: Register, src1: Register, src2: Register, imm: BemiWord) -> Self {
        MicroOpInst { code, dst, src1, src2, immediate: imm, operand_size: 8 }
    }

    pub fn with_size(mut self, size: u8) -> Self {
        self.operand_size = size; self
    }
}

pub struct TranslationBlock {
    pub guest_ip: u64,
    pub guest_size: u32,
    pub instruction_count: u32,
    pub micro_op_count: u32,
    pub micro_ops: [MicroOpInst; MAX_MICRO_OPS_PER_BLOCK],
    pub translation_valid: bool,
}

impl TranslationBlock {
    pub fn new(guest_ip: u64) -> Self {
        TranslationBlock {
            guest_ip,
            guest_size: 0,
            instruction_count: 0,
            micro_op_count: 0,
            micro_ops: [MicroOpInst {
                code: MicroOpCode::Nop,
                dst: Register::RNone,
                src1: Register::RNone,
                src2: Register::RNone,
                immediate: 0,
                operand_size: 8,
            }; MAX_MICRO_OPS_PER_BLOCK],
            translation_valid: true,
        }
    }

    pub fn push(&mut self, inst: MicroOpInst) -> bool {
        let idx = self.micro_op_count as usize;
        if idx < MAX_MICRO_OPS_PER_BLOCK {
            self.micro_ops[idx] = inst;
            self.micro_op_count += 1;
            true
        } else {
            false
        }
    }

    pub fn get(&self, index: usize) -> Option<&MicroOpInst> {
        if index < self.micro_op_count as usize {
            Some(&self.micro_ops[index])
        } else {
            None
        }
    }

    pub fn get_mut(&mut self, index: usize) -> Option<&mut MicroOpInst> {
        if index < self.micro_op_count as usize {
            Some(&mut self.micro_ops[index])
        } else {
            None
        }
    }

    pub fn remove(&mut self, index: usize) {
        let count = self.micro_op_count as usize;
        if index < count {
            for j in index..count - 1 {
                self.micro_ops[j] = self.micro_ops[j + 1];
            }
            self.micro_op_count -= 1;
        }
    }
}
