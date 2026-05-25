"""
Windows Performance Optimizer
=============================
Applies system-level optimizations to boost Ollama inference throughput:
  - CPU priority class boost (HIGH_PRIORITY_CLASS)
  - Processor affinity (lock to performance cores)
  - Large page support
  - NUMA awareness
  - Thread count configuration
"""
import ctypes
import ctypes.wintypes
import os
import sys
import threading

# Windows API constants
PROCESS_ALL_ACCESS = 0x1F0FFF
PROCESS_SET_INFORMATION = 0x0200
HIGH_PRIORITY_CLASS = 0x00000080
REALTIME_PRIORITY_CLASS = 0x00000100
ABOVE_NORMAL_PRIORITY_CLASS = 0x00008000

# Large Page constants
MEM_LARGE_PAGES = 0x20000000
MEM_COMMIT = 0x00001000
MEM_RESERVE = 0x00002000
PAGE_READWRITE = 0x04

kernel32 = ctypes.windll.kernel32
advapi32 = ctypes.windll.advapi32


def enable_large_page_privilege():
    """Enable SeLockMemoryPrivilege for large page support."""
    try:
        import ctypes.wintypes
        TOKEN_ADJUST_PRIVILEGES = 0x0020
        TOKEN_QUERY = 0x0008
        SE_PRIVILEGE_ENABLED = 0x2

        class LUID(ctypes.Structure):
            _fields_ = [("LowPart", ctypes.wintypes.DWORD), ("HighPart", ctypes.wintypes.LONG)]

        class LUID_AND_ATTRIBUTES(ctypes.Structure):
            _fields_ = [("Luid", LUID), ("Attributes", ctypes.wintypes.DWORD)]

        class TOKEN_PRIVILEGES(ctypes.Structure):
            _fields_ = [("PrivilegeCount", ctypes.wintypes.DWORD),
                        ("Privileges", LUID_AND_ATTRIBUTES * 1)]

        h_token = ctypes.wintypes.HANDLE()
        if not advapi32.OpenProcessToken(kernel32.GetCurrentProcess(),
                                          TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY,
                                          ctypes.byref(h_token)):
            return False

        tp = TOKEN_PRIVILEGES()
        tp.PrivilegeCount = 1
        if not advapi32.LookupPrivilegeValueW(None, "SeLockMemoryPrivilege",
                                                ctypes.byref(tp.Privileges[0].Luid)):
            kernel32.CloseHandle(h_token)
            return False

        tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED
        result = advapi32.AdjustTokenPrivileges(h_token, False, ctypes.byref(tp),
                                                 ctypes.sizeof(tp), None, None)
        kernel32.CloseHandle(h_token)
        return result != 0
    except Exception:
        return False


def set_process_priority(high=True):
    """Set current process to HIGH_PRIORITY_CLASS."""
    try:
        prio = HIGH_PRIORITY_CLASS if high else ABOVE_NORMAL_PRIORITY_CLASS
        kernel32.SetPriorityClass(kernel32.GetCurrentProcess(), prio)
        return True
    except Exception:
        return False


def get_physical_core_count():
    """Get number of physical CPU cores."""
    try:
        import psutil
        return psutil.cpu_count(logical=False)
    except ImportError:
        try:
            import multiprocessing
            return multiprocessing.cpu_count() // 2
        except Exception:
            return 4


def get_logical_core_count():
    try:
        import psutil
        return psutil.cpu_count(logical=True)
    except ImportError:
        import multiprocessing
        return multiprocessing.cpu_count()


def get_processor_groups():
    """Get NUMA / processor group information."""
    info = {"groups": 1, "cores_per_group": get_physical_core_count(), "numa_nodes": 1}
    try:
        get_active_processor_count = kernel32.GetActiveProcessorCount
        get_active_processor_group_count = kernel32.GetActiveProcessorGroupCount
        if hasattr(kernel32, 'GetActiveProcessorGroupCount'):
            info["groups"] = get_active_processor_group_count()
    except Exception:
        pass
    return info


def set_affinity_to_performance_cores():
    """Lock process to physical cores only (skip hyperthread siblings)."""
    try:
        logical = get_logical_core_count()
        physical = get_physical_core_count()

        # On hybrid architectures, prefer the performance cores
        # Standard strategy: use first half of physical cores
        affinity_mask = 0
        for i in range(physical):
            affinity_mask |= (1 << i)

        process_handle = kernel32.GetCurrentProcess()
        kernel32.SetProcessAffinityMask(process_handle, affinity_mask)
        return True
    except Exception:
        return False


def set_ollama_thread_count(num_threads=None):
    """Set OLLAMA_NUM_PARALLEL environment variable."""
    if num_threads is None:
        num_threads = max(1, get_physical_core_count() - 1)
    os.environ["OLLAMA_NUM_PARALLEL"] = str(num_threads)
    os.environ["OLLAMA_NUM_THREADS"] = str(num_threads)
    return num_threads


def apply_all_optimizations(config):
    """Apply all system-level optimizations in one call."""
    results = {}

    if config.get("cpu_high_priority", True):
        results["priority_boost"] = set_process_priority(high=True)

    if config.get("lock_to_performance_cores", True):
        results["affinity_locked"] = set_affinity_to_performance_cores()

    if config.get("enable_large_pages", True):
        results["large_pages"] = enable_large_page_privilege()

    nt = config.get("num_threads", 0)
    if nt <= 0:
        nt = max(1, get_physical_core_count() - 1)
    results["threads_set"] = set_ollama_thread_count(nt)

    results["physical_cores"] = get_physical_core_count()
    results["logical_cores"] = get_logical_core_count()
    results["processor_groups"] = get_processor_groups()

    return results


def measure_cpu_info():
    """Return detailed CPU information for display."""
    info = {
        "physical_cores": get_physical_core_count(),
        "logical_cores": get_logical_core_count(),
    }
    try:
        import psutil
        info["cpu_freq_mhz"] = psutil.cpu_freq().current if psutil.cpu_freq() else 0
        mem = psutil.virtual_memory()
        info["total_ram_gb"] = round(mem.total / (1024 ** 3), 1)
        info["available_ram_gb"] = round(mem.available / (1024 ** 3), 1)
    except ImportError:
        pass
    try:
        import platform
        info["processor_name"] = platform.processor()
    except Exception:
        pass
    return info
