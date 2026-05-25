use std::env;
use std::fs;
use std::process;

use bemi_dbt::x8088::*;

const DEFAULT_DISK: &str = "msdos.img";

fn print_banner() {
    println!("╔══════════════════════════════════════════════════════════════╗");
    println!("║        BEMI 8088 Simulator — MS-DOS 1.0 Testbed           ║");
    println!("║     Dynamic Binary Translation via BEMI IR Pipeline        ║");
    println!("╚══════════════════════════════════════════════════════════════╝");
    println!();
}

fn run_simulation(exec: &mut X8088Executor, max_cycles: u64, trace: bool) {
    exec.trace = trace;
    let start = std::time::Instant::now();
    let mut cycles = 0u64;

    while cycles < max_cycles {
        let more = exec.step();
        cycles += 1;
        if !more || exec.cpu.halted || exec.bios.dos_break {
            break;
        }
        if cycles % 10_000_000 == 0 {
            let elapsed = start.elapsed();
            let rate = cycles as f64 / elapsed.as_secs_f64();
            let mips = rate / 1_000_000.0;
            println!("  [PROGRESS] {} cycles in {:.2}s ({:.2} MIPS)", cycles, elapsed.as_secs_f64(), mips);
        }
    }

    let elapsed = start.elapsed();
    let rate = cycles as f64 / elapsed.as_secs_f64();
    let mips = rate / 1_000_000.0;

    println!();
    println!("  [RESULT] Simulation complete:");
    println!("    Cycles executed : {}", cycles);
    println!("    Wall time       : {:.4}s", elapsed.as_secs_f64());
    println!("    Throughput      : {:.2} MIPS", mips);
    println!("    AX              : {:04X}", exec.cpu.ax);
    println!("    CX              : {:04X}", exec.cpu.cx);
    println!("    DX              : {:04X}", exec.cpu.dx);
    println!("    BX              : {:04X}", exec.cpu.bx);
    println!("    Executor cycles : {}", exec.cycles);
    if exec.cpu.halted {
        println!("    State           : HLT reached");
    } else if exec.bios.dos_break {
        println!("    State           : DOS INT 20h / program exit");
    } else {
        println!("    State           : cycle limit reached");
    }
}

fn cmd_run(args: &[String]) {
    let disk = args.get(0).map(|s| s.as_str()).unwrap_or(DEFAULT_DISK);
    let max_cycles: u64 = args.get(1).and_then(|s| s.parse().ok()).unwrap_or(50_000_000);
    let trace = args.iter().any(|a| a == "--trace");

    println!("  Mode    : run");
    println!("  Disk    : {}", disk);
    println!("  MaxCyc  : {}", max_cycles);
    println!("  Trace   : {}", trace);
    println!();

    let disk_data = if let Ok(data) = fs::read(disk) {
        data
    } else {
        println!("  [WARN] Disk image '{}' not found — running with empty disk", disk);
        vec![0u8; 512]
    };

    let mut exec = X8088Executor::new();
    // Load MBR at 0x7C00 (standard BIOS boot address)
    exec.load_image(&disk_data, 0x7C00);
    // Set CPU entry to the boot sector
    exec.set_entry(0x0000, 0x7C00, 0x0000, 0xFFFE);

    run_simulation(&mut exec, max_cycles, trace);
}

fn cmd_bench(args: &[String]) {
    let max_cycles: u64 = args.get(0).and_then(|s| s.parse().ok()).unwrap_or(100_000);

    println!("  Mode    : bench");
    println!("  MaxCyc  : {}", max_cycles);
    println!();

    // Build an 8088 benchmark program:
    //   MOV AX, 0x0001
    //   MOV BX, 16
    //   loop:  ADD AX, AX
    //          DEC BX
    //          JNZ loop
    //   HLT
    let program: &[u8] = &[
        0xB8, 0x01, 0x00,   // MOV AX, 1
        0xBB, 0x10, 0x00,   // MOV BX, 16
        // loop:
        0x01, 0xC0,         // ADD AX, AX
        0x4B,               // DEC BX
        0x75, 0xFB,         // JNZ loop (-5)
        0xF4,               // HLT
    ];

    let mut exec = X8088Executor::new();
    exec.load_image(program, 0x0100);
    exec.set_entry(0x0000, 0x0100, 0x0000, 0xFFFE);

    run_simulation(&mut exec, max_cycles, false);
}

fn cmd_decode(args: &[String]) {
    if args.is_empty() {
        eprintln!("  Usage: sim8088 decode <hex_bytes...>");
        eprintln!("  Example: sim8088 decode B8 01 00 BB 02 00 01 D8");
        process::exit(1);
    }

    let bytes: Vec<u8> = args.iter()
        .filter_map(|s| u8::from_str_radix(s, 16).ok())
        .collect();

    if bytes.is_empty() {
        eprintln!("  Error: no valid hex bytes provided");
        process::exit(1);
    }

    println!("  Decoding {} bytes:", bytes.len());
    println!();

    // Load bytes into a temporary executor's memory at 0x0100
    let mut exec = X8088Executor::new();
    exec.load_image(&bytes, 0x0100);
    exec.set_entry(0x0000, 0x0100, 0x0000, 0xFFFE);

    let mut ip: u16 = 0x0100;
    let end_ip = 0x0100u16 + bytes.len() as u16;

    while ip < end_ip {
        match decode_8088(&exec.mem, 0x0000, ip) {
            Some(inst) => {
                let phys = ((0u32) << 4) + ip as u32;
                let len = inst.length as usize + inst.prefix_count as usize;
                let raw: Vec<String> = (0..len)
                    .map(|i| format!("{:02X}", exec.mem.read8(phys + i as u32)))
                    .collect();
                println!("  [{:#06x}]  {:>8}   {}", ip, inst.mnemonic, raw.join(" "));
                if inst.disp_size > 0 {
                    println!("             Displacement: {:#06x} ({} bytes)", inst.displacement, inst.disp_size);
                }
                println!("             Next IP: {:#06x}", inst.ip_after(ip));
                if inst.category == InstCategory::Jmp || inst.category == InstCategory::Jcc || inst.category == InstCategory::Call {
                    if !inst.is_far {
                        println!("             Branch target: {:#06x}", inst.branch_target(ip));
                    } else {
                        println!("             Far target: {:04X}:{:04X}", inst.immediate2, inst.immediate);
                    }
                }
                println!();
                ip = inst.ip_after(ip);
            }
            None => {
                println!("  [{:#06x}]  <failed to decode>", ip);
                break;
            }
        }
    }
}

fn cmd_test() {
    println!("  Mode    : test (comprehensive opcode verification)");
    println!();

    // Test program covering AND, OR, XOR, TEST, MOV ModRM, XCHG,
    // CBW/CWD, SHIFT, LOOP, GROUPS, MUL/DIV, FLAGS
    let program: &[u8] = &[
        // Part 1: Logic ops
        0xB8, 0x34, 0x12,   // MOV AX, 0x1234
        0xBB, 0x78, 0x56,   // MOV BX, 0x5678
        0x21, 0xD8,         // AND AX, BX           => AX = 0x1230
        0x0D, 0x0F, 0x00,   // OR  AX, 0x000F       => AX = 0x123F
        0x35, 0x3F, 0x12,   // XOR AX, 0x123F       => AX = 0x0000
        0x31, 0xDB,         // XOR BX, BX           => BX = 0x0000 (set ZF=1)
        0xBB, 0x88, 0x77,   // MOV BX, 0x7788
        0x85, 0xDB,         // TEST BX, BX          (ZF=0, SF=0)
        0xBB, 0x00, 0x00,   // MOV BX, 0x0000
        0x85, 0xDB,         // TEST BX, BX          (ZF=1)
        // Part 2: MOV ModRM
        0xB9, 0xFF, 0xFF,   // MOV CX, 0xFFFF
        0x8B, 0xC1,         // MOV AX, CX           => AX = 0xFFFF
        0x89, 0xC8,         // MOV AX, CX           => AX = 0xFFFF
        0xB8, 0xAA, 0x55,   // MOV AX, 0x55AA
        0x8A, 0xC4,         // MOV AL, AH           => AX = 0x55AA (AH=0x55->AL, so AX=0x5555)
        // Part 3: XCHG
        0xBA, 0x01, 0x00,   // MOV DX, 0x0001
        0x92,               // XCHG AX, DX          => AX=0x0001, DX=0x5555
        0x87, 0xD0,         // XCHG AX, DX          => AX=0x5555, DX=0x0001
        // Part 4: CBW / CWD
        0xB8, 0x82, 0x00,   // MOV AX, 0x0082
        0x98,               // CBW (sign-extend AL=0x82->0xFF82) => AX=0xFF82
        0x99,               // CWD (sign-extend AX=0xFF82->DX:AX) => DX=0xFFFF
        // Part 5: Shift/Rotate
        0xB8, 0x01, 0x00,   // MOV AX, 1
        0xB1, 0x04,         // MOV CL, 4
        0xD3, 0xE0,         // SHL AX, CL           => AX = 0x0010
        0xD1, 0xE8,         // SHR AX, 1            => AX = 0x0008
        // Part 6: LOOP
        0xB8, 0x10, 0x00,   // MOV AX, 16
        0xB9, 0x05, 0x00,   // MOV CX, 5
        // loop1:
        0x05, 0x01, 0x00,   //   ADD AX, 1
        0xE2, 0xFC,         //   LOOP loop1         => AX = 16 + 5 = 21 (0x0015)
        // Part 7: Group 1 (83)
        0x83, 0xC0, 0x05,   // ADD AX, 5            => AX = 0x001A
        0x83, 0xE0, 0x0F,   // AND AX, 0x0F         => AX = 0x000A
        0x83, 0xF0, 0x04,   // XOR AX, 4            => AX = 0x000E
        0x83, 0xF8, 0x0E,   // CMP AX, 0x0E         => ZF=1
        // Part 8: INC/DEC
        0x40,               // INC AX               => AX = 0x000F
        0x4B,               // DEC BX (BX=0)        => BX = 0xFFFF
        // Part 9: CLD/STD/CLC/STC/CMC
        0xF8,               // CLC (CF=0)
        0xF5,               // CMC (CF=1)
        0xF8,               // CLC (CF=0)
        // Part 10: MUL/DIV
        0xB8, 0x06, 0x00,   // MOV AX, 6
        0xBB, 0x02, 0x00,   // MOV BX, 2
        0xF7, 0xE3,         // MUL BX               => AX=12 (0x000C), DX=0
        0xBA, 0x00, 0x00,   // MOV DX, 0
        0xB8, 0x0D, 0x00,   // MOV AX, 13
        0xBB, 0x03, 0x00,   // MOV BX, 3
        0xF7, 0xF3,         // DIV BX               => AX=4, DX=1
        // Part 11: SAHF/LAHF
        0xB8, 0x46, 0x03,   // MOV AX, 0x0346
        0x9E,               // SAHF                 (SF=0, ZF=1, AF=0, PF=1, CF=0)
        0x9F,               // LAHF (load flags into AH) => AX = flags_lo | 0x0300
        // Part 12: HLT
        0xF4,               // HLT
    ];

    let mut exec = X8088Executor::new();
    exec.load_image(program, 0x0100);
    exec.set_entry(0x0000, 0x0100, 0x0000, 0xFFFE);
    exec.trace = true;

    run_simulation(&mut exec, 200, true);
}

fn main() {
    let args: Vec<String> = env::args().collect();
    let cmd = args.get(1).map(|s| s.as_str()).unwrap_or("help");

    print_banner();

    match cmd {
        "run"   => cmd_run(&args[2..].to_vec()),
        "bench" => cmd_bench(&args[2..].to_vec()),
        "decode" => cmd_decode(&args[2..].to_vec()),
        "test"  => cmd_test(),
        "help" | "--help" | "-h" => {
            println!("  Usage:");
            println!("    sim8088 run [disk_image] [max_cycles] [--trace]");
            println!("    sim8088 bench [max_cycles]");
            println!("    sim8088 decode <hex_bytes...>");
            println!("    sim8088 test");
            println!("    sim8088 help");
            println!();
            println!("  Examples:");
            println!("    sim8088 run msdos.img 5000000");
            println!("    sim8088 bench 30000000");
            println!("    sim8088 decode B8 01 00 BB 02 00 01 D8");
        }
        _ => {
            eprintln!("  Unknown command: {}", cmd);
            eprintln!("  Usage: sim8088 <run|bench|decode|help>");
            process::exit(1);
        }
    }
}
