# RAM-Test

RAM-Test is the validation and backend verification companion for **IP-Generator** and **UFDE+**. It generates the full matrix of BRAM test configurations, runs them through the complete FDP3P7 FPGA flow (synthesis → mapping → packing → placement → routing → bitstream), and verifies each stage with ModelSim simulations.

> **This repository does not generate IP cores.** For BRAM/PLL generation and the `img2mif` tool, see the [IP-Generator](https://github.com/FrancisCYH/IP-Generator) repository.

---

## What It Does

| Step | Script / Tool | Input | Output |
|------|---------------|-------|--------|
| **Test Generation** | `test_ram.py` | BRAM configuration matrix | `test/` — MIF, Verilog, wrapper, testbench, constraints |
| **FPGA Synthesis** | `run_flow.bat` | `test/<config>/` | Post-synthesis netlists, XML, bitstream |
| **Batch Synthesis** | `run_all.bat` | `test/` directory | All configurations processed in batch |
| **RTL Simulation** | `run_sim_quick.bat` | `test/<config>/` | ModelSim compilation & waveform-less smoke test |
| **Stage Simulation** | `run_sim_stage.bat` | `test/<config>/` + stage | Post-synthesis stage verification (rtl / map / pack / route) |
| **Batch Simulation** | `run_all_sim_stages.bat` | `test/` directory | All stages for all configs |

---

## Project Layout

```
RAM-Test/
│
│  === Test Generation ===
│
├── test_ram.py             # Automated test-bench generator (full BRAM config matrix)
├── templates_test/         # SystemVerilog testbench, wrapper, and MIF templates
│   ├── test_testbench.j2
│   ├── wrapper_template.j2
│   └── test_mif.j2
│
│  === FPGA Backend Flow ===
│
├── run_flow.bat            # Single config: syn → map → pack → place → route → bit
├── run_all.bat             # Batch runner over an entire test directory
│
│  === Simulation ===
│
├── run_sim_quick.bat       # Quick RTL-only ModelSim simulation
├── run_sim_stage.bat       # Per-stage simulation (rtl | map | pack | route)
├── run_all_sim_stages.bat  # Batch multi-stage simulation runner
│
│  === Toolchain & Resources ===
│
├── bin/                    # Backend toolchain binaries + resource libraries
│   ├── yosys.exe
│   ├── yosys-abc.exe
│   ├── map.exe
│   ├── pack.exe
│   ├── place.exe
│   ├── route.exe
│   ├── bitgen.exe
│   ├── resource/           # Yosys scripts, cell libraries, arch files, edalib
│   │   ├── yosys/
│   │   ├── hw_lib/
│   │   └── edalib/
│   └── share/              # Yosys built-in simulation & techmap libraries
│       ├── simlib.v
│       ├── simcells.v
│       ├── techmap.v
│       └── fde/
│
└── scripts/
    └── preprocess_netlist.ps1
```

---

## Requirements

- **Windows 10/11** (all automation scripts are `.bat` and the backend toolchain provides Windows `.exe` binaries)
- **Python** ≥ 3.8 with **jinja2**
- **ModelSim** (Windows path expected by default at `D:\ModelSim\win64`)
- **Yosys** + **FDE-Source** backend toolchain (see [Obtaining the Backend Toolchain](#obtaining-the-backend-toolchain))

---

## Quick Start

### 1. Generate the Test Matrix

```bash
python test_ram.py
```

This creates every supported BRAM configuration under `test/`, each containing:

- `test.mif` — initialization data
- `test.v` — generated BRAM IP (from IP-Generator)
- `top.v` — wrapper module
- `tb_test.sv` — SystemVerilog testbench
- `top_cons.xml` — pin constraints

### 2. Run the FPGA Flow (Single Config)

```bash
run_flow.bat test\16x1024
```

Steps executed:
1. **Synthesis** — Yosys (`top_yosys_syn.edf`)
2. **Mapping** — `top_yosys_map.v`
3. **Packing** — `top_yosys_pack.v`
4. **Placement** — `top_yosys_place.xml`
5. **Routing** — `top_yosys_route.v`
6. **BitGen** — `top_yosys_bit.bit`

### 3. Run the FPGA Flow (All Configs)

```bash
run_all.bat test
```

Results are collected in `test\_results\`:
- `passed.txt`
- `failed.txt`

### 4. Simulation

#### RTL Only (Quick Smoke Test)

```bash
:: Default ModelSim path
run_sim_quick.bat test\16x1024

:: Custom ModelSim path
run_sim_quick.bat test\16x1024 "C:\ModelSim\win64"
```

#### Post-Synthesis Stages (Per-Stage)

```bash
run_sim_stage.bat test\16x1024 rtl
run_sim_stage.bat test\16x1024 map
run_sim_stage.bat test\16x1024 pack
run_sim_stage.bat test\16x1024 route

:: With custom ModelSim path (3rd argument)
run_sim_stage.bat test\16x1024 rtl "C:\ModelSim\win64"
```

#### Batch All Stages

```bash
:: Run all stages for all configs that have a bitstream
run_all_sim_stages.bat test

:: Only RTL and Map
run_all_sim_stages.bat test rtl,map

:: Custom simulator path
run_all_sim_stages.bat test rtl,map "C:\ModelSim\win64"
```

Results are saved to `test\_sim_stage_results\` with per-stage summaries.

---

## Supported BRAM Configurations

The test matrix is based on the 4Kb `RAMB4_Sx` primitive. Up to 16 primitives can be combined in parallel.

### Single-Port (25 configurations)

| Capacity | width × depth |
|----------|---------------|
| 4Kb  | 1×4096, 2×2048, 4×1024, 8×512, 16×256 |
| 8Kb  | 2×4096, 4×2048, 8×1024, 16×512, 32×256 |
| 16Kb | 4×4096, 8×2048, 16×1024, 32×512, 64×256 |
| 32Kb | 8×4096, 16×2048, 32×1024, 64×512, 128×256 |
| 64Kb | 16×4096, 32×2048, 64×1024, 128×512, 256×256 |

### Dual-Port (75 configurations)

Symmetric and asymmetric dual-port are supported; total capacity of Port A must equal Port B. See `_VALID_DUAL_PORT` in `test_ram.py` for the complete list.

---

## Obtaining the Backend Toolchain

The flow scripts expect a `bin/` directory containing the following binaries and resource libraries:

```
bin/
├── yosys.exe          # Open-source logic synthesis
├── yosys-abc.exe      # ABC integration for Yosys
├── import.exe         # Verilog netlist → internal XML (FDE-Source)
├── map.exe            # Technology mapping (FDE-Source)
├── pack.exe           # Cluster packing (FDE-Source)
├── place.exe          # Physical placement (FDE-Source)
├── route.exe          # Signal routing (FDE-Source)
├── bitgen.exe         # Bitstream generation (FDE-Source)
├── resource/          # TCL scripts, hardware libs, and simulation libs
│   ├── yosys/
│   ├── hw_lib/
│   └── edalib/
└── share/             # Yosys built-in simulation & techmap libraries
    ├── simlib.v
    ├── simcells.v
    ├── techmap.v
    └── fde/
```

### Where do the binaries come from?

| Binary | Source Project | How to obtain |
|--------|----------------|---------------|
| `yosys.exe` | [YosysHQ/yosys](https://github.com/YosysHQ/yosys) | Build from source or download a pre-built Windows release. Place both `yosys.exe` and `yosys-abc.exe` in `bin/`. |
| `yosys-abc.exe` | [YosysHQ/yosys](https://github.com/YosysHQ/yosys) | Built together with Yosys (ABC is bundled as a submodule). |
| `import.exe` | FDE-Source | Build from the `FDE-Source` repository. This tool converts the Yosys-generated Verilog netlist into the internal XML format used by the FDP3P7 backend. |
| `map.exe` | FDE-Source | Build from the `FDE-Source` repository. Performs technology mapping from generic gates to FDP3P7 primitives. |
| `pack.exe` | FDE-Source | Build from the `FDE-Source` repository. Clusters mapped primitives into logic blocks. |
| `place.exe` | FDE-Source | Build from the `FDE-Source` repository. Determines the physical locations of logic blocks on the FDP3P7 fabric. |
| `route.exe` | FDE-Source | Build from the `FDE-Source` repository. Routes interconnections between placed blocks. |
| `bitgen.exe` | FDE-Source | Build from the `FDE-Source` repository. Generates the final FPGA bitstream from the routed design. |

### Resource & Share directories

The `resource/` and `share/` directories are **not** hand-written for this project; they ship with the backend toolchain:

- **`resource/yosys/`** — Yosys TCL scripts (`yosys_fde.tcl`), technology mapping files (`techmap.v`, `cells_map.v`), and the FDE simulation library (`fdesimlib.v`). These are invoked by `run_flow.bat` during synthesis.
- **`resource/hw_lib/`** — FDP3P7 architecture and cell definition files (`fdp3p7_arch.xml`, `fdp3p7_dly.xml`, `fdp3p7_cil.xml`, `fdp3_cell.xml`, etc.). These are read by `map.exe`, `pack.exe`, `place.exe`, `route.exe`, and `bitgen.exe`.
- **`resource/edalib/`** — Simulation libraries (`custom_simlib.v`, `MAPPING_SIMLIB.v`, `PACKING_SIMLIB_FDP3P7.v`, `ROUTING_SIMLIB_FDP3P7.v`) used by ModelSim in `run_sim_stage.bat`.
- **`share/`** — Yosys built-in standard cell libraries (`simlib.v`, `simcells.v`, `techmap.v`) and FDE-specific mappings (`fde/`). Yosys searches this directory at runtime.


## Author

[@FrancisCYH](https://github.com/FrancisCYH)
