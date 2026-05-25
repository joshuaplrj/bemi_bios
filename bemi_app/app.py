"""
Bemi App — Windows GUI Application
===================================
Plug-and-play Windows application for running and benchmarking
Ollama LLM inference with Bemi BIOS acceleration analysis.

Features:
  - One-click Ollama install/start/model pull
  - System-level performance optimization (CPU priority, affinity, large pages)
  - Live inference benchmarking with token/second measurement
  - Bemi v7.2 acceleration projections
  - Comparison with other Bemi workloads
  - Export reports

Built with tkinter — included with every Python installation.
No external GUI dependencies required.
"""
import tkinter as tk
from tkinter import ttk, scrolledtext, messagebox, filedialog
import threading
import time
import os
import sys
import json
from datetime import datetime

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from config import load_config, save_config, DEFAULT_PROMPTS, MODELS, DEFAULT_MODEL
from ollama_service import (
    is_installed, is_running, install, start_service,
    pull_model, list_models, is_model_available
)
from win_perf import apply_all_optimizations, measure_cpu_info
from inference import run_benchmark, run_inference_sync
from bemi_analysis import (
    compute_projections, run_detailed_analysis,
    get_cached_bemi_result, BEMI_SPEEDUP, COMPARISON_WORKLOADS
)

APP_TITLE = "Bemi App — LLM Inference Accelerator"
FONT = ("Segoe UI", 10)
FONT_BOLD = ("Segoe UI", 10, "bold")
FONT_TITLE = ("Segoe UI", 16, "bold")
FONT_MONO = ("Consolas", 9)
BG = "#1e1e2e"
FG = "#cdd6f4"
BG_PANEL = "#181825"
BG_INPUT = "#313244"
FG_ACCENT = "#89b4fa"
FG_GREEN = "#a6e3a1"
FG_YELLOW = "#f9e2af"
FG_RED = "#f38ba8"
FG_PURPLE = "#cba6f7"

class BemiApp(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title(APP_TITLE)
        self.geometry("1024x720")
        self.minsize(900, 600)
        self.configure(bg=BG)
        self.config = load_config()
        self.benchmark_result = None
        self.optimization_result = None
        self._setup_styles()
        self._build_ui()

    def _setup_styles(self):
        style = ttk.Style(self)
        style.theme_use("clam")
        style.configure("TFrame", background=BG)
        style.configure("TLabel", background=BG, foreground=FG, font=FONT)
        style.configure("TLabelframe", background=BG, foreground=FG)
        style.configure("TLabelframe.Label", background=BG, foreground=FG_ACCENT, font=FONT_BOLD)
        style.configure("TButton", font=FONT, padding=6)
        style.configure("TNotebook", background=BG, borderwidth=2)
        style.configure("TNotebook.Tab", font=FONT_BOLD, padding=[12, 4])
        style.map("TNotebook.Tab",
                  background=[("selected", BG_PANEL), ("!selected", BG)],
                  foreground=[("selected", FG_ACCENT)])
        style.configure("Accent.TButton", font=FONT_BOLD)
        style.configure("Large.TButton", font=("Segoe UI", 12, "bold"), padding=10)

    def _build_ui(self):
        # Header
        header = tk.Frame(self, bg=BG_PANEL, height=50)
        header.pack(fill=tk.X, side=tk.TOP)
        tk.Label(header, text=APP_TITLE, font=FONT_TITLE,
                 bg=BG_PANEL, fg=FG_ACCENT).pack(pady=8)

        # Status bar
        self.status_var = tk.StringVar(value="Ready")
        status_bar = tk.Frame(self, bg=BG_PANEL, height=24)
        status_bar.pack(fill=tk.X, side=tk.BOTTOM)
        tk.Label(status_bar, textvariable=self.status_var, font=("Consolas", 8),
                 bg=BG_PANEL, fg=FG_GREEN, anchor=tk.W).pack(fill=tk.X, padx=8)

        # Notebook tabs
        self.notebook = ttk.Notebook(self)
        self.notebook.pack(fill=tk.BOTH, expand=True, padx=6, pady=6)

        self._build_setup_tab()
        self._build_config_tab()
        self._build_benchmark_tab()
        self._build_analysis_tab()

    def _build_setup_tab(self):
        tab = ttk.Frame(self.notebook)
        self.notebook.add(tab, text="  Setup  ")

        main = tk.Frame(tab, bg=BG)
        main.pack(fill=tk.BOTH, expand=True, padx=16, pady=16)

        # System Info
        frm = tk.LabelFrame(main, text=" System Information ", bg=BG, fg=FG_ACCENT,
                            font=FONT_BOLD, padx=10, pady=10)
        frm.pack(fill=tk.X, pady=(0, 12))
        self.sys_text = tk.Text(frm, height=6, font=FONT_MONO,
                                bg=BG_INPUT, fg=FG, relief=tk.FLAT, padx=8, pady=6)
        self.sys_text.pack(fill=tk.X)
        self._update_sysinfo()

        # Ollama Status
        frm = tk.LabelFrame(main, text=" Ollama Status ", bg=BG, fg=FG_ACCENT,
                            font=FONT_BOLD, padx=10, pady=10)
        frm.pack(fill=tk.BOTH, expand=True, pady=(0, 12))

        self.ollama_status_var = tk.StringVar()
        btn_frame = tk.Frame(frm, bg=BG)
        btn_frame.pack(fill=tk.X, pady=(6, 4))

        self.install_btn = tk.Button(btn_frame, text="1. Install Ollama",
                                     command=self._install_ollama, font=FONT_BOLD,
                                     bg="#45475a", fg=FG)
        self.install_btn.pack(side=tk.LEFT, padx=(0, 6))

        self.start_btn = tk.Button(btn_frame, text="2. Start Service",
                                   command=self._start_ollama, font=FONT_BOLD,
                                   bg="#45475a", fg=FG)
        self.start_btn.pack(side=tk.LEFT, padx=(0, 6))

        self.stop_btn = tk.Button(btn_frame, text="Stop Service",
                                  command=self._stop_ollama, font=FONT,
                                  bg="#45475a", fg=FG)
        self.stop_btn.pack(side=tk.LEFT)

        tk.Label(frm, textvariable=self.ollama_status_var, font=FONT_MONO,
                 bg=BG, fg=FG_GREEN).pack(pady=(6, 0))
        self._check_ollama_status()

        # Model Management
        frm = tk.LabelFrame(main, text=" Model Management ", bg=BG, fg=FG_ACCENT,
                            font=FONT_BOLD, padx=10, pady=10)
        frm.pack(fill=tk.BOTH, expand=True, pady=(0, 12))

        model_row = tk.Frame(frm, bg=BG)
        model_row.pack(fill=tk.X, pady=4)
        tk.Label(model_row, text="Model:", bg=BG, fg=FG).pack(side=tk.LEFT)
        self.model_var = tk.StringVar(value=self.config.get("model", DEFAULT_MODEL))
        cb = ttk.Combobox(model_row, textvariable=self.model_var, values=MODELS,
                          font=FONT, width=18)
        cb.pack(side=tk.LEFT, padx=6)
        tk.Button(model_row, text="Pull Model", command=self._pull_model,
                  font=FONT, bg="#45475a", fg=FG).pack(side=tk.LEFT, padx=(0, 6))
        tk.Button(model_row, text="Refresh Models", command=self._refresh_models,
                  font=FONT, bg="#45475a", fg=FG).pack(side=tk.LEFT)

        self.model_list_text = tk.Text(frm, height=4, font=FONT_MONO,
                                       bg=BG_INPUT, fg=FG, relief=tk.FLAT, padx=8, pady=6)
        self.model_list_text.pack(fill=tk.X, pady=(8, 0))

        # Log output
        self.log_text = scrolledtext.ScrolledText(
            main, height=8, font=FONT_MONO, bg=BG_INPUT, fg=FG, relief=tk.FLAT)
        self.log_text.pack(fill=tk.BOTH, expand=True)

    def _build_config_tab(self):
        tab = ttk.Frame(self.notebook)
        self.notebook.add(tab, text="  Config  ")

        main = tk.Frame(tab, bg=BG)
        main.pack(fill=tk.BOTH, expand=True, padx=16, pady=16)

        # Optimization toggles
        frm = tk.LabelFrame(main, text=" System Optimizations ", bg=BG, fg=FG_ACCENT,
                            font=FONT_BOLD, padx=10, pady=10)
        frm.pack(fill=tk.X, pady=(0, 12))

        self.high_prio_var = tk.BooleanVar(value=self.config.get("cpu_high_priority", True))
        self.affinity_var = tk.BooleanVar(value=self.config.get("lock_to_performance_cores", True))
        self.large_pages_var = tk.BooleanVar(value=self.config.get("enable_large_pages", True))
        self.numa_var = tk.BooleanVar(value=self.config.get("numa_aware", True))
        self.auto_start_var = tk.BooleanVar(value=self.config.get("auto_start_ollama", True))

        toggles = [
            ("CPU High Priority", self.high_prio_var, "Set process to HIGH_PRIORITY_CLASS"),
            ("Lock to Performance Cores", self.affinity_var, "Avoid hyperthreading siblings"),
            ("Enable Large Pages", self.large_pages_var, "Reduce TLB misses (needs admin)"),
            ("NUMA Aware", self.numa_var, "Keep memory local to processor group"),
            ("Auto-Start Ollama", self.auto_start_var, "Start Ollama service automatically"),
        ]

        for label, var, desc in toggles:
            row = tk.Frame(frm, bg=BG)
            row.pack(fill=tk.X, pady=2)
            tk.Checkbutton(row, text=label, variable=var, selectcolor=BG_INPUT,
                           bg=BG, fg=FG, font=FONT, activebackground=BG,
                           activeforeground=FG_ACCENT).pack(side=tk.LEFT)
            tk.Label(row, text=desc, bg=BG, fg="#6c7086", font=("Segoe UI", 8)).pack(side=tk.LEFT, padx=10)

        # Benchmark Config
        frm = tk.LabelFrame(main, text=" Benchmark Settings ", bg=BG, fg=FG_ACCENT,
                            font=FONT_BOLD, padx=10, pady=10)
        frm.pack(fill=tk.X, pady=(0, 12))

        row = tk.Frame(frm, bg=BG)
        row.pack(fill=tk.X, pady=2)
        tk.Label(row, text="Prompts:", bg=BG, fg=FG).pack(side=tk.LEFT)
        self.prompt_count_var = tk.StringVar(value="5")
        tk.Spinbox(row, textvariable=self.prompt_count_var, from_=1, to=20,
                   width=5, font=FONT, bg=BG_INPUT, fg=FG, buttonbackground=BG_INPUT).pack(side=tk.LEFT, padx=6)
        tk.Label(row, text="(standard prompts from config)", bg=BG, fg="#6c7086").pack(side=tk.LEFT)

        row = tk.Frame(frm, bg=BG)
        row.pack(fill=tk.X, pady=2)
        tk.Label(row, text="Warmup runs:", bg=BG, fg=FG).pack(side=tk.LEFT)
        self.warmup_var = tk.StringVar(value=str(self.config.get("warmup_runs", 1)))
        tk.Spinbox(row, textvariable=self.warmup_var, from_=0, to=5,
                   width=5, font=FONT, bg=BG_INPUT, fg=FG, buttonbackground=BG_INPUT).pack(side=tk.LEFT, padx=6)

        # Apply button
        tk.Button(main, text="APPLY OPTIMIZATIONS", command=self._apply_optimizations,
                  font=("Segoe UI", 12, "bold"), bg=FG_GREEN, fg="#1e1e2e",
                  padx=20, pady=8).pack(pady=(10, 0))

        self.opt_result_text = tk.Text(main, height=8, font=FONT_MONO,
                                       bg=BG_INPUT, fg=FG, relief=tk.FLAT)
        self.opt_result_text.pack(fill=tk.BOTH, expand=True, pady=(10, 0))

    def _build_benchmark_tab(self):
        tab = ttk.Frame(self.notebook)
        self.notebook.add(tab, text="  Benchmark  ")

        main = tk.Frame(tab, bg=BG)
        main.pack(fill=tk.BOTH, expand=True, padx=16, pady=16)

        # Run button
        tk.Button(main, text="RUN BENCHMARK",
                  command=self._run_benchmark,
                  font=("Segoe UI", 14, "bold"),
                  bg=FG_ACCENT, fg="#1e1e2e",
                  padx=30, pady=12).pack(pady=(0, 12))

        # Progress
        self.bench_progress_var = tk.StringVar(value="Ready to benchmark")
        tk.Label(main, textvariable=self.bench_progress_var,
                 font=FONT_MONO, bg=BG, fg=FG_YELLOW).pack(fill=tk.X)

        # Results
        self.bench_result_text = tk.Text(main, height=14, font=FONT_MONO,
                                         bg=BG_INPUT, fg=FG, relief=tk.FLAT)
        self.bench_result_text.pack(fill=tk.BOTH, expand=True, pady=(8, 8))

        # Bottom: quick prompt
        frm = tk.LabelFrame(main, text=" Quick Inference (Test) ", bg=BG, fg=FG_ACCENT,
                            font=FONT_BOLD, padx=10, pady=10)
        frm.pack(fill=tk.X)

        row = tk.Frame(frm, bg=BG)
        row.pack(fill=tk.X, pady=4)
        self.quick_prompt_var = tk.StringVar(value="Explain quantum computing in one sentence.")
        tk.Entry(row, textvariable=self.quick_prompt_var, font=FONT,
                 bg=BG_INPUT, fg=FG, insertbackground=FG).pack(side=tk.LEFT, fill=tk.X, expand=True)
        tk.Button(row, text="Run", command=self._quick_inference,
                  font=FONT_BOLD, bg=FG_GREEN, fg="#1e1e2e", padx=12).pack(side=tk.LEFT, padx=6)

        self.quick_result_var = tk.StringVar(value="")
        tk.Label(frm, textvariable=self.quick_result_var, font=FONT_MONO,
                 bg=BG, fg=FG_GREEN).pack(pady=(6, 0), anchor=tk.W)

    def _build_analysis_tab(self):
        tab = ttk.Frame(self.notebook)
        self.notebook.add(tab, text="  Analysis  ")

        main = tk.Frame(tab, bg=BG)
        main.pack(fill=tk.BOTH, expand=True, padx=16, pady=16)

        # Bemi Projection
        frm = tk.LabelFrame(main, text=" Bemi v7.2 Acceleration Projection ", bg=BG,
                            fg=FG_ACCENT, font=FONT_BOLD, padx=10, pady=10)
        frm.pack(fill=tk.X, pady=(0, 12))

        row = tk.Frame(frm, bg=BG)
        row.pack(fill=tk.X, pady=4)
        tk.Label(row, text=f"Bemi v7.2 Speedup Factor: ", bg=BG, fg=FG).pack(side=tk.LEFT)
        tk.Label(row, text=f"{BEMI_SPEEDUP}x", bg=BG, fg=FG_GREEN, font=FONT_BOLD).pack(side=tk.LEFT)

        row = tk.Frame(frm, bg=BG)
        row.pack(fill=tk.X, pady=4)
        tk.Label(row, text="Your native tokens/sec:", bg=BG, fg=FG).pack(side=tk.LEFT)
        self.native_tps_var = tk.StringVar(value="0")
        tk.Entry(row, textvariable=self.native_tps_var, font=FONT, width=10,
                 bg=BG_INPUT, fg=FG, insertbackground=FG).pack(side=tk.LEFT, padx=6)
        tk.Label(row, text="(or paste from benchmark)", bg=BG, fg="#6c7086").pack(side=tk.LEFT)
        tk.Button(row, text="Calculate Projection", command=self._calc_projection,
                  font=FONT, bg=FG_ACCENT, fg="#1e1e2e").pack(side=tk.LEFT, padx=10)

        self.projection_var = tk.StringVar(value="")
        tk.Label(frm, textvariable=self.projection_var, font=("Segoe UI", 11, "bold"),
                 bg=BG, fg=FG_GREEN, wraplength=900).pack(pady=(8, 0), anchor=tk.W)

        # Mechanisms
        frm = tk.LabelFrame(main, text=" Acceleration Mechanisms ", bg=BG,
                            fg=FG_ACCENT, font=FONT_BOLD, padx=10, pady=10)
        frm.pack(fill=tk.X, pady=(0, 12))

        from bemi_analysis import BEMI_MECHANISMS
        self.mech_text = tk.Text(frm, height=9, font=FONT_MONO,
                                 bg=BG_INPUT, fg=FG, relief=tk.FLAT)
        self.mech_text.pack(fill=tk.BOTH)
        for name, data in BEMI_MECHANISMS.items():
            self.mech_text.insert(tk.END, f"  {name:<35} {data['factor']:>5.1f}x\n")
            self.mech_text.insert(tk.END, f"    {data['detail']}\n")
        self.mech_text.insert(tk.END, f"\n  {'Combined Effect':<35} {BEMI_SPEEDUP:>5.1f}x\n")
        self.mech_text.configure(state=tk.DISABLED)

        # Workload comparison
        frm = tk.LabelFrame(main, text=" Other Bemi Workload Speedups ", bg=BG,
                            fg=FG_ACCENT, font=FONT_BOLD, padx=10, pady=10)
        frm.pack(fill=tk.BOTH, expand=True)

        comp_text = tk.Text(frm, height=6, font=FONT_MONO,
                            bg=BG_INPUT, fg=FG, relief=tk.FLAT)
        comp_text.pack(fill=tk.BOTH)
        for name, sp in COMPARISON_WORKLOADS.items():
            marker = " *** HIGHEST" if sp == BEMI_SPEEDUP else ""
            comp_text.insert(tk.END, f"  {name:<30} {sp:>6.2f}x{marker}\n")
        comp_text.configure(state=tk.DISABLED)

        # Export button
        btn_frame = tk.Frame(main, bg=BG)
        btn_frame.pack(fill=tk.X, pady=(10, 0))
        tk.Button(btn_frame, text="EXPORT REPORT",
                  command=self._export_report,
                  font=("Segoe UI", 12, "bold"),
                  bg=FG_PURPLE, fg="#1e1e2e",
                  padx=20, pady=8).pack(side=tk.RIGHT)

    # ─── Action methods ────────────────────────────────────────────

    def _log(self, msg):
        self.log_text.insert(tk.END, msg + "\n")
        self.log_text.see(tk.END)
        self.log_text.update_idletasks()

    def _set_status(self, msg, color=FG_GREEN):
        self.status_var.set(msg)
        self.status_bar_color = color

    def _update_sysinfo(self):
        info = measure_cpu_info()
        lines = []
        lines.append(f"  Processor:      {info.get('processor_name', 'Unknown')}")
        lines.append(f"  Physical Cores: {info['physical_cores']}")
        lines.append(f"  Logical Cores:  {info['logical_cores']}")
        if info.get("cpu_freq_mhz"):
            lines.append(f"  CPU Frequency:  {info['cpu_freq_mhz']:.0f} MHz")
        if info.get("total_ram_gb"):
            lines.append(f"  Total RAM:     {info['total_ram_gb']} GB  |  Available: {info['available_ram_gb']} GB")
        lines.append(f"  OS:            {sys.getwindowsversion().major}.{sys.getwindowsversion().minor}")
        lines.append(f"  Python:        {sys.version.split()[0]}")
        self.sys_text.delete(1.0, tk.END)
        self.sys_text.insert(1.0, "\n".join(lines))
        self.sys_text.configure(state=tk.DISABLED)

    def _check_ollama_status(self):
        installed = is_installed()
        running = is_running()
        if installed and running:
            self.ollama_status_var.set("Ollama: INSTALLED and RUNNING")
        elif installed:
            self.ollama_status_var.set("Ollama: INSTALLED (service not running)")
        else:
            self.ollama_status_var.set("Ollama: NOT INSTALLED")

        self.install_btn.configure(state=tk.NORMAL if not installed else tk.DISABLED)
        self.start_btn.configure(state=tk.NORMAL if installed and not running else tk.DISABLED)
        self.stop_btn.configure(state=tk.NORMAL if running else tk.DISABLED)
        self._refresh_models()

    def _install_ollama(self):
        def _run():
            self._log("Installing Ollama...")
            self._set_status("Installing Ollama...", FG_YELLOW)
            try:
                install(progress_callback=lambda m: (self._log(m), self.after(0, lambda: None)))
                self._log("Ollama installation complete.")
            except Exception as e:
                self._log(f"Install error: {e}")
                self._set_status(f"Install error: {str(e)[:60]}", FG_RED)
            self.after(0, self._check_ollama_status)
            self._set_status("Ready", FG_GREEN)
        threading.Thread(target=_run, daemon=True).start()

    def _start_ollama(self):
        def _run():
            self._log("Starting Ollama service...")
            self._set_status("Starting Ollama...", FG_YELLOW)
            port = self.config.get("ollama_port", 11434)
            ok = start_service(port, progress_callback=lambda m: (self._log(m), self.after(0, lambda: None)))
            if ok:
                self._log(f"Ollama running on port {port}")
            else:
                self._log("Failed to start Ollama service.")
            self.after(0, self._check_ollama_status)
            self._set_status("Ready", FG_GREEN)
        threading.Thread(target=_run, daemon=True).start()

    def _stop_ollama(self):
        from ollama_service import stop_service
        stop_service(progress_callback=self._log)
        self._check_ollama_status()

    def _pull_model(self):
        model = self.model_var.get()
        def _run():
            self._log(f"Pulling model: {model}...")
            self._set_status(f"Pulling {model}...", FG_YELLOW)
            ok = pull_model(model, progress_callback=lambda m: (self._log(m), self.after(0, lambda: None)))
            self._log(f"Pull {'successful' if ok else 'FAILED'}")
            self.after(0, self._refresh_models)
            self._set_status("Ready", FG_GREEN)
        threading.Thread(target=_run, daemon=True).start()

    def _refresh_models(self):
        models = list_models()
        self.model_list_text.delete(1.0, tk.END)
        self.model_list_text.insert(1.0, models if models else "(no models found)")
        self.model_list_text.configure(state=tk.DISABLED)

    def _apply_optimizations(self):
        self.config["cpu_high_priority"] = self.high_prio_var.get()
        self.config["lock_to_performance_cores"] = self.affinity_var.get()
        self.config["enable_large_pages"] = self.large_pages_var.get()
        self.config["numa_aware"] = self.numa_var.get()
        self.config["auto_start_ollama"] = self.auto_start_var.get()
        save_config(self.config)

        self.opt_result_text.delete(1.0, tk.END)
        self.opt_result_text.insert(tk.END, "Applying system optimizations...\n\n")
        self.update()

        results = apply_all_optimizations(self.config)
        self.optimization_result = results

        lines = ["Optimization Results:", "=" * 40]
        for key, val in results.items():
            lines.append(f"  {key}: {val}")
        self.opt_result_text.insert(tk.END, "\n".join(lines))
        self.opt_result_text.configure(state=tk.DISABLED)
        self._set_status("Optimizations applied", FG_GREEN)

    def _run_benchmark(self):
        if self.config.get("auto_start_ollama", True):
            running = is_running()
            if not running:
                port = self.config.get("ollama_port", 11434)
                start_service(port, progress_callback=self._log)
                if not is_running(f"http://localhost:{port}"):
                    messagebox.showerror("Error", "Could not start Ollama service.")
                    return

        model = self.model_var.get()
        if not is_model_available(model):
            messagebox.showerror("Error", f"Model '{model}' not available. Pull it first.")
            return

        prompts = DEFAULT_PROMPTS
        warmup = int(self.warmup_var.get()) > 0

        def _run():
            self._set_status("Running benchmark...", FG_YELLOW)
            self.bench_progress_var.set("Running benchmark...")
            self.bench_result_text.delete(1.0, tk.END)
            self.bench_result_text.insert(tk.END, f"Model: {model}\n")
            self.bench_result_text.insert(tk.END, f"Prompts: {len(prompts)}\n")
            self.bench_result_text.insert(tk.END, f"Warmup: {'Yes' if warmup else 'No'}\n")
            self.bench_result_text.insert(tk.END, "-" * 50 + "\n\n")

            port = self.config.get("ollama_port", 11434)
            base_url = f"http://localhost:{port}"
            result = run_benchmark(
                model, prompts, warmup=warmup,
                progress_callback=lambda m: (self.bench_progress_var.set(m),
                                              self.bench_result_text.insert(tk.END, f"  {m}\n"),
                                              self.bench_result_text.see(tk.END),
                                              self.after(0, lambda: None)),
                base_url=base_url
            )

            self.after(0, lambda: self._show_bench_result(result))

        threading.Thread(target=_run, daemon=True).start()

    def _show_bench_result(self, result):
        self.benchmark_result = result
        self.bench_result_text.insert(tk.END, "\n" + "=" * 50 + "\n")
        self.bench_result_text.insert(tk.END, "BENCHMARK RESULTS\n")
        self.bench_result_text.insert(tk.END, "=" * 50 + "\n\n")

        if not result.get("success"):
            self.bench_result_text.insert(tk.END, f"FAILED: {result.get('error', 'unknown')}\n")
            self._set_status("Benchmark failed", FG_RED)
            return

        self.bench_result_text.insert(tk.END, f"  Avg Eval Tokens/sec:        {result['avg_eval_tokens_per_second']:.2f}\n")
        self.bench_result_text.insert(tk.END, f"  Avg Prompt Tokens/sec:      {result['avg_prompt_tokens_per_second']:.2f}\n")
        self.bench_result_text.insert(tk.END, f"  Avg Overall Tokens/sec:     {result['avg_overall_tokens_per_second']:.2f}\n")
        self.bench_result_text.insert(tk.END, f"  Total Completion Tokens:    {result['total_completion_tokens']}\n")
        self.bench_result_text.insert(tk.END, f"  Total Duration:             {result['total_duration_seconds']:.2f}s\n")
        self.bench_result_text.insert(tk.END, f"  Successful Runs:            {result['num_successful']}/{result['num_runs']}\n")

        # Per-run
        self.bench_result_text.insert(tk.END, "\nPer-prompt details:\n")
        self.bench_result_text.insert(tk.END, "-" * 50 + "\n")
        for i, r in enumerate(result.get("runs", [])):
            if r.get("success"):
                self.bench_result_text.insert(tk.END,
                    f"  [{i+1}] Eval: {r['eval_tokens_per_second']:.1f} tok/s | "
                    f"Prompt: {r['prompt_eval_tokens_per_second']:.1f} tok/s | "
                    f"Tokens: {r['completion_tokens']}\n")
            else:
                self.bench_result_text.insert(tk.END, f"  [{i+1}] FAILED: {r.get('error', '')}\n")

        # Bemi projection
        self.bench_result_text.insert(tk.END, f"\n{'=' * 50}\n")
        self.bench_result_text.insert(tk.END, f"  Bemi v7.2 PROJECTION: "
                                            f"{result['avg_eval_tokens_per_second'] * BEMI_SPEEDUP:.0f} tokens/sec "
                                            f"({BEMI_SPEEDUP}x speedup)\n")

        self.native_tps_var.set(f"{result['avg_eval_tokens_per_second']:.1f}")
        self.bench_progress_var.set("Benchmark complete!")
        self._set_status(f"Benchmark: {result['avg_eval_tokens_per_second']:.1f} eval tok/s", FG_GREEN)
        self.bench_result_text.see(tk.END)

    def _quick_inference(self):
        model = self.model_var.get()
        prompt = self.quick_prompt_var.get()
        if not prompt.strip():
            return
        def _run():
            self.quick_result_var.set("Running...")
            self.update()
            port = self.config.get("ollama_port", 11434)
            base_url = f"http://localhost:{port}"
            r = run_inference_sync(model, prompt, timeout=120, base_url=base_url)
            if r.get("success"):
                self.quick_result_var.set(
                    f"Eval: {r['eval_tokens_per_second']:.1f} tok/s | "
                    f"Prompt: {r['prompt_eval_tokens_per_second']:.1f} tok/s | "
                    f"Tokens: {r['completion_tokens']} | "
                    f"Bemi projected: {r['eval_tokens_per_second'] * BEMI_SPEEDUP:.0f} tok/s")
            else:
                self.quick_result_var.set(f"Error: {r.get('error', 'unknown')}")
        threading.Thread(target=_run, daemon=True).start()

    def _calc_projection(self):
        try:
            native = float(self.native_tps_var.get())
        except ValueError:
            native = 0
        if native <= 0:
            self.projection_var.set("Enter your native tokens/sec from the benchmark above.")
            return
        proj = compute_projections(native)
        self.projection_var.set(
            f"Native: {native:.1f} tok/s → Bemi v7.2 Projected: {proj['projected_tps']:.0f} tok/s "
            f"({proj['speedup']:.1f}x speedup)")

    def _export_report(self):
        path = filedialog.asksaveasfilename(
            defaultextension=".md",
            filetypes=[("Markdown", "*.md"), ("Text", "*.txt"), ("All", "*.*")],
            initialfile="bemi_benchmark_report.md",
            title="Export Benchmark Report"
        )
        if not path:
            return

        now = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        lines = []
        lines.append("# Bemi App Benchmark Report")
        lines.append(f"**Generated:** {now}")
        lines.append("")
        lines.append("## System Information")
        info = measure_cpu_info()
        for k, v in info.items():
            lines.append(f"- **{k}:** {v}")
        lines.append("")

        if self.optimization_result:
            lines.append("## System Optimizations Applied")
            for k, v in self.optimization_result.items():
                lines.append(f"- **{k}:** {v}")
            lines.append("")

        if self.benchmark_result and self.benchmark_result.get("success"):
            r = self.benchmark_result
            lines.append("## Native Ollama Benchmark")
            lines.append(f"- Model: {r['model']}")
            lines.append(f"- Avg Eval Tokens/sec: **{r['avg_eval_tokens_per_second']:.2f}**")
            lines.append(f"- Avg Prompt Tokens/sec: {r['avg_prompt_tokens_per_second']:.2f}")
            lines.append(f"- Avg Overall Tokens/sec: {r['avg_overall_tokens_per_second']:.2f}")
            lines.append(f"- Total Completion Tokens: {r['total_completion_tokens']}")
            lines.append(f"- Total Duration: {r['total_duration_seconds']:.2f}s")
            lines.append(f"- Successful Runs: {r['num_successful']}/{r['num_runs']}")
            lines.append("")

            lines.append("## Bemi v7.2 Projection")
            projected = r['avg_eval_tokens_per_second'] * BEMI_SPEEDUP
            lines.append(f"- Native: {r['avg_eval_tokens_per_second']:.2f} tok/s")
            lines.append(f"- Bemi Projected: **{projected:.0f} tok/s**")
            lines.append(f"- Speedup: **{BEMI_SPEEDUP}x**")
            lines.append("")
        else:
            lines.append("## Bemi Simulation Results")
            cached = get_cached_bemi_result()
            if cached:
                lines.append(f"- Simulated Stock x86: {cached['legacy']['tokens_per_second']:.3f} tok/s")
                lines.append(f"- Simulated Bemi v7.2: {cached['bemi']['tokens_per_second']:.2f} tok/s")
                lines.append(f"- Speedup: {cached['speedup']:.1f}x")
                lines.append(f"- Energy Savings: {cached['energy_savings']:.1f}x")
            lines.append("")

        lines.append(f"*Report generated by Bemi App on branch `win-app`*")

        with open(path, "w", encoding="utf-8") as f:
            f.write("\n".join(lines))

        messagebox.showinfo("Report Exported", f"Report saved to:\n{path}")
        self._set_status(f"Report exported to {os.path.basename(path)}", FG_GREEN)


def main():
    app = BemiApp()
    try:
        app.iconbitmap(default="")
    except Exception:
        pass
    app.mainloop()


if __name__ == "__main__":
    main()
