use crate::ir::Register;
use iced_x86;

pub fn is_r8_or_above(reg: Register) -> bool {
    let r = reg as u8;
    r >= 8 && r <= 15
}

pub fn map_register(iced_reg: iced_x86::Register) -> Register {
    match iced_reg {
        iced_x86::Register::RAX | iced_x86::Register::EAX | iced_x86::Register::AX | iced_x86::Register::AL | iced_x86::Register::AH => Register::RAX,
        iced_x86::Register::RCX | iced_x86::Register::ECX | iced_x86::Register::CX | iced_x86::Register::CL | iced_x86::Register::CH => Register::RCX,
        iced_x86::Register::RDX | iced_x86::Register::EDX | iced_x86::Register::DX | iced_x86::Register::DL | iced_x86::Register::DH => Register::RDX,
        iced_x86::Register::RBX | iced_x86::Register::EBX | iced_x86::Register::BX | iced_x86::Register::BL | iced_x86::Register::BH => Register::RBX,
        iced_x86::Register::RSP | iced_x86::Register::ESP | iced_x86::Register::SP | iced_x86::Register::SPL => Register::RSP,
        iced_x86::Register::RBP | iced_x86::Register::EBP | iced_x86::Register::BP | iced_x86::Register::BPL => Register::RBP,
        iced_x86::Register::RSI | iced_x86::Register::ESI | iced_x86::Register::SI | iced_x86::Register::SIL => Register::RSI,
        iced_x86::Register::RDI | iced_x86::Register::EDI | iced_x86::Register::DI | iced_x86::Register::DIL => Register::RDI,
        iced_x86::Register::R8  | iced_x86::Register::R8D  | iced_x86::Register::R8W  | iced_x86::Register::R8B  => Register::R8,
        iced_x86::Register::R9  | iced_x86::Register::R9D  | iced_x86::Register::R9W  | iced_x86::Register::R9B  => Register::R9,
        iced_x86::Register::R10 | iced_x86::Register::R10D | iced_x86::Register::R10W | iced_x86::Register::R10B => Register::R10,
        iced_x86::Register::R11 | iced_x86::Register::R11D | iced_x86::Register::R11W | iced_x86::Register::R11B => Register::R11,
        iced_x86::Register::R12 | iced_x86::Register::R12D | iced_x86::Register::R12W | iced_x86::Register::R12B => Register::R12,
        iced_x86::Register::R13 | iced_x86::Register::R13D | iced_x86::Register::R13W | iced_x86::Register::R13B => Register::R13,
        iced_x86::Register::R14 | iced_x86::Register::R14D | iced_x86::Register::R14W | iced_x86::Register::R14B => Register::R14,
        iced_x86::Register::R15 | iced_x86::Register::R15D | iced_x86::Register::R15W | iced_x86::Register::R15B => Register::R15,
        _ => Register::RNone,
    }
}
