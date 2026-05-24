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

    let elapse
<truncated 60 bytes>
.imm2_size); }
            if inst.disp_size > 0 { println!("    Displacement: {:#06x} ({} bytes)", inst.displacement, inst.disp_size); }
            println!();
            println!("    Next IP: {:#06x}", inst.ip_after(ip));
            if inst.category == InstCategory::Jmp || inst.category == InstCategory::Jcc || inst.category == InstCategory::Call {
                if !inst.is_far {
                    println!("    Branch target: {:#06x}", inst.branch_target(ip));
                } else {
                    println!("    Far target: {:#06x}:{:#06x}", inst.immediate2, inst.immediate);
                }
            }
        }
        None => println!("    Failed to decode"),
    }
}

fn main() {
    let args: Vec<String> = env::args().collect();
    let cmd = args.get(1).map(|s| s.as_str()).unwrap_or("help");

    print_banner();

    match cmd {
        "run" => cmd_run(&args[2..]),
        "bench" => cmd_bench(&args[2..]),
        "decode" => cmd_decode(&args[2..]),
        "help" | "--help" | "-h" => {
            println!("  Usage:");
            println!("    sim8088 run [disk_image] [max_cycles] [--trace]");
            println!("    sim8088 bench [max_cycles] [disk_image]");
            println!("    sim8088 decode <hex_bytes...>");
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
