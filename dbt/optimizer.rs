use crate::ir::{TranslationBlock, MicroOpCode};

pub fn optimize_block(tb: &mut TranslationBlock) {
    // 1. Eliminate redundant Nops and self-moves (MovRR with dst == src)
    let mut i = 0;
    while i < tb.micro_op_count as usize {
        let uop = &tb.micro_ops[i];
        if uop.code == MicroOpCode::Nop || (uop.code == MicroOpCode::MovRR && uop.dst == uop.src1) {
            tb.remove(i);
        } else {
            i += 1;
        }
    }

    // 2. Eliminate back-to-back redundant moves (e.g. MovRR dst, src; MovRR src, dst)
    if tb.micro_op_count > 1 {
        let mut i = 0;
        while i < (tb.micro_op_count - 1) as usize {
            let uop1 = tb.micro_ops[i];
            let uop2 = tb.micro_ops[i + 1];
            if uop1.code == MicroOpCode::MovRR && uop2.code == MicroOpCode::MovRR {
                if uop1.dst == uop2.src1 && uop1.src1 == uop2.dst {
                    tb.remove(i + 1);
                    continue;
                }
            }
            i += 1;
        }
    }
}
