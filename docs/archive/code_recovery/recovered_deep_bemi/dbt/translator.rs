use crate::ir::*;
use crate::decoder::map_register;
use iced_x86::{Instruction, Mnemonic, OpKind};

pub fn translate_block(code: &[u8], x86_ip: u64, bitness: u32) -> Result<TranslationBlock, String> {
    let mut decoder = iced_x86::Decoder::with_ip(bitness, code, x86_ip, iced_x86::DecoderOptions::NONE);
    let mut tb = TranslationBlock::new(x86_ip);
    
    let mut instruction = Instruction::default();
    let mut guest_bytes_read = 0;
    
    while decoder.can_decode() {
        decoder.decode_out(&mut instruction);
        guest_bytes_read += instruction.len();
        
        let uops = translate_instruction(&instruction)?;
        for uop in uops {
            if !tb.push(uop) {
                break;
            }
        }
        
        tb.instruction_count += 1;
        
        // Stop block decoding on branch/return/hlt instructions
        let mnemonic = instruction.mnemonic();
        if is_branch_or_ret(mnemonic) {
            break;
        }
    }
    
    tb.guest_size = guest_bytes_read;
    Ok(tb)
}

fn is_branch_or_ret(mnemonic: Mnemonic) -> bool {
    matches!(
        mnemonic,
        Mnemonic::Jmp | Mnemonic::Jcc | Mnemonic::Call | Mnemonic::Ret | Mnemonic::Iret | Mnemonic::Hlt
    )
}

fn translate_instruction(inst: &Instruction) -> Result<Vec<MicroOpInst>, String> {
    let mnemonic = inst.mnemonic();
    let mut ops = Vec::new();
    let op_size = in
<truncated 865 bytes>
er or reg-imm
        }
        Mnemonic::Or => {
            translate_binary(inst, MicroOpCode::OrRR, MicroOpCode::MovRI, &mut ops);
        }
        Mnemonic::Xor => {
            translate_binary(inst, MicroOpCode::XorRR, MicroOpCode::MovRI, &mut ops);
        }
        Mnemonic::Push => {
            let src = map_register(inst.op0_register());
            ops.push(MicroOpInst::new(MicroOpCode::PushR, Register::RNone, src, Register::RNone, 0).with_size(op_size));
        }
        Mnemonic::Pop => {
            let dst = map_register(inst.op0_register());
            ops.push(MicroOpInst::new(MicroOpCode::PopR, dst, Register::RNone, Register::RNone, 0).with_size(op_size));
        }
        Mnemonic::Inc => {
            let dst = map_register(inst.op0_register());
            ops.push(MicroOpInst::new(MicroOpCode::IncR, dst, dst, Register::RNone, 0).with_size(op_size));
        }
        Mnemonic::Dec => {
            let dst = map_register(inst.op0_register());
            ops.push(MicroOpInst::new(MicroOpCode::DecR, dst, dst, Register::RNone, 0).with_size(op_size));
        }
        Mnemonic::Jmp => {
            if inst.op0_kind() == OpKind::Register {
                let target = map_register(inst.op0_register());
                ops.push(MicroOpInst::new(MicroOpCode::JmpAbs, Register::RNone, target, Register::RNone, 0));
            } else {
                let target = inst.near_branch64();
                ops.push(MicroOpInst::new(MicroOpCode::JmpRel, Register::RNone, Register::RNone, Register::RNone, target));
            }
        }
        Mnemonic::Call => {
            if inst.op0_kind() == OpKind::Register {
                let target = map_register(inst.op0_register());
The above content does NOT show the entire file contents. If you need to view any lines of the file which were not shown to complete your task, call this tool again to view those lines.