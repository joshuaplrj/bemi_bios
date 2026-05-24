use crate::ir::{MicroOp, Opcode, Register};

pub struct Machine {
    regs: [u64; 64],
    mem: Vec<u8>,
}

impl Machine {
    pub fn new(mem_size: usize) -> Self {
        Self {
            regs: [0u64; 64],
            mem: vec![0u8; mem_size],
        }
    }

    pub fn mem(&self) -> &[u8] {
        &self.mem
    }

    pub fn mem_mut(&mut self) -> &mut [u8] {
        &mut self.mem
    }

    pub fn reg(&self, r: Register) -> u64 {
        if r == Register::None {
            return 0;
        }
        self.regs[r as usize]
    }

    pub fn set_reg(&mut self, r: Register, val: u64) {
        if r == Register::None {
            return;
        }
        self.regs[r as usize] = val;
    }
}

fn sign_extend_12(v: u32) -> i32 {
    // v is 12-bit.
    let mut off = (v & 0xFFF) as i32;
    if (off & 0x800) != 0 {
        off |= !0xFFF;
    }
    off
}

fn sign_extend_18(v: u32) -> i32 {
    // v is 18-bit.
    let mut imm = (v & 0x3FFFF) as i32;
    if (imm & 0x20000) != 0 {
        imm |= !0x3FFFF;
    }
    imm
}

fn read_u64_le(mem: &[u8], addr: u64) -> Result<u64, String> {
    let a = addr as usize;
    if a.checked_add(8).is_none() || a + 8 > mem.len() {
        return Err(format!("Memory read OOB at 0x{addr:X}"));
    }
    let mut b = [0u8; 8];
<truncated 117 bytes>
             Opcode::Xor => a ^ b,
                    Opcode::Mul => a.wrapping_mul(b),
                    _ => unreachable!(),
                };
                m.set_reg(dst, out);
            }
            Opcode::LoadImm => {
                let dst = Register::from_u8(((u >> 8) & 0x3F) as u8);
                let imm = sign_extend_18(u >> 14) as i64 as u64;
                m.set_reg(dst, imm);
            }
            Opcode::Load => {
                let dst = Register::from_u8(((u >> 8) & 0x3F) as u8);
                let base = Register::from_u8(((u >> 14) & 0x3F) as u8);
                let off = sign_extend_12(u >> 20) as i64;
                let addr = (m.reg(base) as i64).wrapping_add(off) as u64;
                let val = read_u64_le(m.mem(), addr)?;
                m.set_reg(dst, val);
            }
            Opcode::Store => {
                let src = Register::from_u8(((u >> 8) & 0x3F) as u8);
                let base = Register::from_u8(((u >> 14) & 0x3F) as u8);
                let off = sign_extend_12(u >> 20) as i64;
                let addr = (m.reg(base) as i64).wrapping_add(off) as u64;
                let val = m.reg(src);
                write_u64_le(m.mem_mut(), addr, val)?;
            }
            Opcode::JumpRel | Opcode::CallRel => {
                return Err(format!(
                    "Control-flow micro-op {:?} not supported in execute_linear()",
                    op
                ));
            }
            Opcode::Push
            | Opcode::Pop
            | Opcode::Compare
            | Opcode::Unsupported => {
                return Err(format!("Unsupported micro-op {:?}", op));
            }
        }

        pc += 1;
    }

    Ok(())
}
