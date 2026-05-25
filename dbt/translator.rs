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

    tb.guest_size = guest_bytes_read as u32;
    Ok(tb)
}

fn is_branch_or_ret(mnemonic: Mnemonic) -> bool {
    matches!(
        mnemonic,
        Mnemonic::Jmp
        | Mnemonic::Jo | Mnemonic::Jno | Mnemonic::Jb | Mnemonic::Jae
        | Mnemonic::Je | Mnemonic::Jne | Mnemonic::Jbe | Mnemonic::Ja
        | Mnemonic::Js | Mnemonic::Jns | Mnemonic::Jp | Mnemonic::Jnp
        | Mnemonic::Jl | Mnemonic::Jge | Mnemonic::Jle | Mnemonic::Jg
        | Mnemonic::Call | Mnemonic::Ret | Mnemonic::Iret | Mnemonic::Hlt
    )
}

fn is_jcc(mnemonic: Mnemonic) -> bool {
    matches!(
        mnemonic,
        Mnemonic::Jo | Mnemonic::Jno | Mnemonic::Jb | Mnemonic::Jae
        | Mnemonic::Je | Mnemonic::Jne | Mnemonic::Jbe | Mnemonic::Ja
        | Mnemonic::Js | Mnemonic::Jns | Mnemonic::Jp | Mnemonic::Jnp
        | Mnemonic::Jl | Mnemonic::Jge | Mnemonic::Jle | Mnemonic::Jg
    )
}

fn is_setcc(mnemonic: Mnemonic) -> bool {
    matches!(
        mnemonic,
        Mnemonic::Seta | Mnemonic::Setae | Mnemonic::Setb | Mnemonic::Setbe
        | Mnemonic::Sete | Mnemonic::Setne
        | Mnemonic::Setg | Mnemonic::Setge | Mnemonic::Setl | Mnemonic::Setle
        | Mnemonic::Seto | Mnemonic::Setno | Mnemonic::Sets | Mnemonic::Setns
        | Mnemonic::Setp | Mnemonic::Setnp
    )
}

fn translate_instruction(inst: &Instruction) -> Result<Vec<MicroOpInst>, String> {
    let mnemonic = inst.mnemonic();
    let mut ops = Vec::new();
    let op_size: u8 = 8;

    match mnemonic {
        Mnemonic::Nop => {
            ops.push(MicroOpInst::new(MicroOpCode::Nop, Register::RNone, Register::RNone, Register::RNone, 0));
        }
        Mnemonic::Mov => {
            translate_mov(inst, &mut ops);
        }
        Mnemonic::Movzx => {
            let dst = map_register(inst.op0_register());
            let src = map_register(inst.op1_register());
            ops.push(MicroOpInst::new(MicroOpCode::MovzxRR, dst, src, Register::RNone, 0).with_size(op_size));
        }
        Mnemonic::Movsx => {
            let dst = map_register(inst.op0_register());
            let src = map_register(inst.op1_register());
            ops.push(MicroOpInst::new(MicroOpCode::MovsxRR, dst, src, Register::RNone, 0).with_size(op_size));
        }
        Mnemonic::Movsxd => {
            let dst = map_register(inst.op0_register());
            let src = map_register(inst.op1_register());
            ops.push(MicroOpInst::new(MicroOpCode::Movsxd, dst, src, Register::RNone, 0).with_size(op_size));
        }
        Mnemonic::Add => {
            translate_binary(inst, MicroOpCode::AddRR, MicroOpCode::AddRI, &mut ops);
        }
        Mnemonic::Sub => {
            translate_binary(inst, MicroOpCode::SubRR, MicroOpCode::SubRI, &mut ops);
        }
        Mnemonic::Cmp => {
            translate_binary(inst, MicroOpCode::CmpRR, MicroOpCode::CmpRI, &mut ops);
        }
        Mnemonic::And => {
            translate_binary(inst, MicroOpCode::AndRR, MicroOpCode::AndRR, &mut ops);
        }
        Mnemonic::Or => {
            translate_binary(inst, MicroOpCode::OrRR, MicroOpCode::OrRR, &mut ops);
        }
        Mnemonic::Xor => {
            translate_binary(inst, MicroOpCode::XorRR, MicroOpCode::XorRR, &mut ops);
        }
        Mnemonic::Shl | Mnemonic::Sal => {
            translate_binary(inst, MicroOpCode::ShlRR, MicroOpCode::ShlRR, &mut ops);
        }
        Mnemonic::Shr => {
            translate_binary(inst, MicroOpCode::ShrRR, MicroOpCode::ShrRR, &mut ops);
        }
        Mnemonic::Sar => {
            translate_binary(inst, MicroOpCode::SarRR, MicroOpCode::SarRR, &mut ops);
        }
        Mnemonic::Rol => {
            let dst = map_register(inst.op0_register());
            let imm = inst.immediate64();
            ops.push(MicroOpInst::new(MicroOpCode::RolRI, dst, dst, Register::RNone, imm).with_size(op_size));
        }
        Mnemonic::Ror => {
            let dst = map_register(inst.op0_register());
            let imm = inst.immediate64();
            ops.push(MicroOpInst::new(MicroOpCode::RorRI, dst, dst, Register::RNone, imm).with_size(op_size));
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
        m if is_jcc(m) => {
            let target = inst.near_branch64();
            ops.push(MicroOpInst::new(MicroOpCode::JccRel, Register::RNone, Register::RNone, Register::RNone, target));
        }
        m if is_setcc(m) => {
            let dst = map_register(inst.op0_register());
            ops.push(MicroOpInst::new(MicroOpCode::Setcc, dst, Register::RNone, Register::RNone, m as u64).with_size(op_size));
        }
        Mnemonic::Call => {
            if inst.op0_kind() == OpKind::Register {
                let target = map_register(inst.op0_register());
                ops.push(MicroOpInst::new(MicroOpCode::CallAbs, Register::RNone, target, Register::RNone, 0));
            } else {
                let target = inst.near_branch64();
                ops.push(MicroOpInst::new(MicroOpCode::CallRel, Register::RNone, Register::RNone, Register::RNone, target));
            }
        }
        Mnemonic::Ret | Mnemonic::Retf => {
            ops.push(MicroOpInst::new(MicroOpCode::Ret, Register::RNone, Register::RNone, Register::RNone, 0));
        }
        Mnemonic::Hlt => {
            ops.push(MicroOpInst::new(MicroOpCode::Hlt, Register::RNone, Register::RNone, Register::RNone, 0));
        }
        Mnemonic::Cli => {
            ops.push(MicroOpInst::new(MicroOpCode::Cli, Register::RNone, Register::RNone, Register::RNone, 0));
        }
        Mnemonic::Sti => {
            ops.push(MicroOpInst::new(MicroOpCode::Sti, Register::RNone, Register::RNone, Register::RNone, 0));
        }
        _ => {
            // Safe fallback: emit Nop
            ops.push(MicroOpInst::new(MicroOpCode::Nop, Register::RNone, Register::RNone, Register::RNone, 0));
        }
    }

    Ok(ops)
}

fn translate_mov(inst: &Instruction, ops: &mut Vec<MicroOpInst>) {
    let dst_kind = inst.op0_kind();
    let src_kind = inst.op1_kind();
    let op_size: u8 = 8;

    match (dst_kind, src_kind) {
        (OpKind::Register, OpKind::Register) => {
            let dst = map_register(inst.op0_register());
            let src = map_register(inst.op1_register());
            ops.push(MicroOpInst::new(MicroOpCode::MovRR, dst, src, Register::RNone, 0).with_size(op_size));
        }
        (OpKind::Register, kind) if is_imm(kind) => {
            let dst = map_register(inst.op0_register());
            let imm = inst.immediate64();
            ops.push(MicroOpInst::new(MicroOpCode::MovRI, dst, Register::RNone, Register::RNone, imm).with_size(op_size));
        }
        (OpKind::Register, OpKind::Memory) => {
            let dst = map_register(inst.op0_register());
            let base = map_register(inst.memory_base());
            let disp = inst.memory_displacement64();
            ops.push(MicroOpInst::new(MicroOpCode::MovRM, dst, base, Register::RNone, disp).with_size(op_size));
        }
        (OpKind::Memory, OpKind::Register) => {
            let src = map_register(inst.op1_register());
            let base = map_register(inst.memory_base());
            let disp = inst.memory_displacement64();
            ops.push(MicroOpInst::new(MicroOpCode::MovMR, base, src, Register::RNone, disp).with_size(op_size));
        }
        (OpKind::Memory, kind) if is_imm(kind) => {
            let base = map_register(inst.memory_base());
            let disp = inst.memory_displacement64();
            let imm = inst.immediate64();
            let _ = disp;
            ops.push(MicroOpInst::new(MicroOpCode::MovMI, base, Register::RNone, Register::RNone, imm).with_size(op_size));
        }
        _ => {}
    }
}

fn translate_binary(inst: &Instruction, rr_code: MicroOpCode, ri_code: MicroOpCode, ops: &mut Vec<MicroOpInst>) {
    let dst_kind = inst.op0_kind();
    let src_kind = inst.op1_kind();
    let op_size: u8 = 8;

    match (dst_kind, src_kind) {
        (OpKind::Register, OpKind::Register) => {
            let dst = map_register(inst.op0_register());
            let src = map_register(inst.op1_register());
            ops.push(MicroOpInst::new(rr_code, dst, dst, src, 0).with_size(op_size));
        }
        (OpKind::Register, kind) if is_imm(kind) => {
            let dst = map_register(inst.op0_register());
            let imm = inst.immediate64();
            ops.push(MicroOpInst::new(ri_code, dst, dst, Register::RNone, imm).with_size(op_size));
        }
        _ => {}
    }
}

fn is_imm(kind: OpKind) -> bool {
    matches!(
        kind,
        OpKind::Immediate8 | OpKind::Immediate16 | OpKind::Immediate32 | OpKind::Immediate64
        | OpKind::Immediate8to16 | OpKind::Immediate8to32 | OpKind::Immediate8to64
        | OpKind::Immediate32to64
    )
}
