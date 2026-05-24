#![allow(non_camel_case_types)]

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
    MovsxRR = 41, M
<truncated 103 bytes>
ion_count: 0,
            micro_op_count: 0,
            micro_ops: [MicroOpInst {
                code: MicroOpCode::Nop, dst: Register::RNone,
                src1: Register::RNone, src2: Register::RNone,
                immediate: 0, operand_size: 8,
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

impl MicroOpInst {
    pub fn new(code: MicroOpCode, dst: Register, src1: Register, src2: Register, imm: BemiWord) -> Self {
        MicroOpInst { code, dst, src1, src2, immediate: imm, operand_size: 8 }
    }

    pub fn with_size(mut self, size: u8) -> Self {
        self.operand_size = size; self
    }
}
