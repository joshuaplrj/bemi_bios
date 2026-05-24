# Internal Research Documentation -- BEMI BIOS Project

**Classification:** Internal -- Not for external publication
**Purpose:** Raw research logs, experimental data, failed attempts, open questions, and design rationale underpinning the BEMI BIOS / Weaponized x86 architecture.
**Relationship to `docs/`:** The `docs/` folder contains the polished, externally-presentable narrative. This folder contains the unfiltered research trail -- the data, dead ends, speculative leaps, and working notes that generated those conclusions.

---

## Document Index

| Folder | Contents |
|---|---|
| `01_research_log/` | Dated lab notebook entries: experimental setups, observations, and raw reasoning during each phase of the project |
| `02_experimental_data/` | Raw measurement tables, CSV-formatted throughput data, methodology notes, and reproducibility details |
| `03_failed_experiments/` | Approaches that were tested and abandoned -- including why they failed and what was learned |
| `04_open_questions/` | Unresolved research questions, speculative hypotheses awaiting verification, and known blind spots |
| `05_design_rationale/` | Internal debates and trade-off analyses that led to key architectural decisions (v1.1->v1.2, Ring -1, TAGE pre-fill, etc.) |
| `06_references/` | Notes from patent analysis, academic papers, prior art in DBT, and chip teardowns that informed the research |

---

## Key Cross-References

- Published docs: [`docs/`](../docs/) -- the polished 14-chapter narrative
- Prototype firmware: [`bios_prototype.py`](../bios_prototype.py) -- Ring -1 DBT simulation
- Benchmark suite: [`run_all_benchmarks.py`](../run_all_benchmarks.py) -- the automated measurement harness
- Legacy OS simulation: [`legacy_os_benchmark.py`](../legacy_os_benchmark.py) -- DBT overhead model
- Hybrid BEMI: [`hybrid_bemi/`](../hybrid_bemi/) -- the Rust DBT translator + executor (v1.0 prototype)
- **v1.3 ROB Entry Density research**: 4B ROB entries pack 3.5x more per SRAM byte than x86's 14B entries. Split/distributed ROB eliminates CAM O(n^2) scaling. See [`docs/04_micro_op_deep_dive.md`](../docs/04_micro_op_deep_dive.md) and [`docs/08_weaponized_x86_bemi.md`](../docs/08_weaponized_x86_bemi.md) for derivations.

---

**Note:** Entries in this folder are raw and may contain contradictory statements, speculative leaps, or dead ends. That is by design -- this is a research archive, not a publication.


