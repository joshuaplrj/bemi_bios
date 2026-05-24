use iced_x86::{Instruction, Mnemonic, OpKind};
use crate::ir::{MicroOp, Opcode, Register};

pub fn translate_program(code: &[u8], bitness: u32) -> Result<Vec<MicroOp>, String> {
    let mut decoder = iced_x86::Decoder::with_ip(bitness, code, 0x1000, iced_x86::DecoderOptions::NONE);
    let mut micro_ops = Vec::new();

    for instruction in &mut decoder {
        micro_ops.extend(translate_instruction(&instruction)?);
    }

    Ok(micro_ops)
}

fn translate_instruction(instruction: &Instruction) -> Result<Vec<MicroOp>, String> {
    let mnemonic = instruction.mnemonic();
    let mut ops = Vec::new();

    match mnemonic {
        Mnemonic::Nop => ops.push(MicroOp::r_type(Opcode::Nop, Register::None, Register::None, Register::None)),
        Mnemonic::Mov => translate_mov(instruction, &mut ops)?,
        Mnemonic::Add => translate_binary_arithmetic(instruction, Opcode::Add, &mut ops)?,
        Mnemonic::Sub => translate_binary_arithmetic(instruction, Opcode::Sub, &mut ops)?,
        Mnemonic::Push => translate_push(instruction, &mut ops)?,
        Mnemonic::Pop => translate_pop(instruction, &mut ops)?,
        Mnemonic::Jmp => translate_jump(instruction, &mut ops)?,
        Mnemonic::Call => translate_call(instruction, &mut ops)?,
        Mnemonic::Ret => ops.push(MicroOp::r_type(Opcode::Return, Register::None, Register::None, Register::None)),
        _ => return Err(format!("Unsupported instruction: {:?}", mnemonic)),
    }

    Ok(o
<truncated 4334 bytes>
    }
    Ok(())
}

fn translate_push(inst: &Instruction, ops: &mut Vec<MicroOp>) -> Result<(), String> {
    let src = Register::from_iced(inst.op0_register()).unwrap_or(Register::None);
    ops.push(MicroOp::i_type(Opcode::LoadImm, Register::RTmp0, 8));
    ops.push(MicroOp::r_type(Opcode::Sub, Register::Rsp, Register::Rsp, Register::RTmp0));
    ops.push(MicroOp::s_type(Opcode::Store, src, Register::Rsp, 0));
    Ok(())
}

fn translate_pop(inst: &Instruction, ops: &mut Vec<MicroOp>) -> Result<(), String> {
    let dst = Register::from_iced(inst.op0_register()).unwrap_or(Register::None);
    ops.push(MicroOp::s_type(Opcode::Load, dst, Register::Rsp, 0));
    ops.push(MicroOp::i_type(Opcode::LoadImm, Register::RTmp0, 8));
    ops.push(MicroOp::r_type(Opcode::Add, Register::Rsp, Register::Rsp, Register::RTmp0));
    Ok(())
}

fn translate_jump(inst: &Instruction, ops: &mut Vec<MicroOp>) -> Result<(), String> {
    let target = inst.near_branch64();
    let next_ip = inst.next_ip();
    let displacement = (target as i64 - next_ip as i64) as i32;
    ops.push(MicroOp::i_type(Opcode::JumpRel, Register::None, displacement));
    Ok(())
}

fn translate_call(inst: &Instruction, ops: &mut Vec<MicroOp>) -> Result<(), String> {
    let target = inst.near_branch64();
    let next_ip = inst.next_ip();
    let displacement = (target as i64 - next_ip as i64) as i32;
    ops.push(MicroOp::i_type(Opcode::CallRel, Register::None, displacement));
    Ok(())
}

fn is_immediate(kind: OpKind) -> bool {
    matches!(
        kind,
        OpKind::Immediate8 | OpKind::Immediate16 | OpKind::Immediate32 | OpKind::Immediate64 |
        OpKind::Immediate8to16 | OpKind::Immediate8to32 | OpKind::Immediate8to64 | OpKind::Immediate32to64
    )
}
