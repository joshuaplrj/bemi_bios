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

                let dst = Register::from_u8(((self.0 >> 8) & 0x3F) as u8);
                let src1 = Register::from_u8(((self.0 >> 14) & 0x3F) as u8);
                let src2 = Register::from_u8(((self.0 >> 20) & 0x3F) as u8);
                if src2 == Register::None {
                    if src1 == Register::None {
                        write!(f, "{:?} {:?}", op, dst)
                    } else {
                        write!(f, "{:?} {:?}, {:?}", op, dst, src1)
                    }
                } else {
                    write!(f, "{:?} {:?}, {:?}, {:?}", op, dst, src1, src2)
                }
            }
        }
    }
}

The above content does NOT show the entire file contents. If you need to view any lines of the file which were not shown to complete your task, call this tool again to view those lines.