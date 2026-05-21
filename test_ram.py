#!/usr/bin/env python3

import os
import sys
import math
import random
from pathlib import Path
from dataclasses import dataclass
from typing import List, Tuple, Optional
from jinja2 import Environment, FileSystemLoader

sys.path.insert(0, str(Path(__file__).parent))
from bram_generator import generate_bram_from_mif

#==============================================================================
# Device Pin Definitions (from Pins.ts)
#==============================================================================

DEVICE_PINS = {
    'FDP3P7': {
        'input': [
            'P151', 'P148', 'P150', 'P152', 'P160', 'P161', 'P162', 'P163', 'P164', 'P165',
            'P166', 'P169', 'P173', 'P174', 'P175', 'P191', 'P120', 'P116', 'P115', 'P114',
            'P113', 'P112', 'P111', 'P108', 'P102', 'P101', 'P100', 'P97', 'P96', 'P95',
            'P89', 'P88', 'P87', 'P86', 'P81', 'P75', 'P74', 'P70', 'P69', 'P68',
            'P64', 'P62', 'P61', 'P58', 'P57', 'P49', 'P47', 'P48', 'P192', 'P193',
            'P199', 'P200', 'P201', 'P202'
        ],
        'output': [
            'P7', 'P6', 'P5', 'P4', 'P9', 'P8', 'P16', 'P15', 'P11', 'P10',
            'P20', 'P18', 'P17', 'P22', 'P21', 'P23', 'P44', 'P45', 'P46', 'P43',
            'P40', 'P41', 'P42', 'P33', 'P34', 'P35', 'P36', 'P30', 'P31', 'P24',
            'P27', 'P29', 'P110', 'P109', 'P99', 'P98', 'P94', 'P93', 'P84', 'P83',
            'P82', 'P73', 'P71', 'P63', 'P60', 'P59', 'P56', 'P55', 'P167', 'P168',
            'P176', 'P187', 'P189', 'P194'
        ],
        'clock': 'P77'
    }
}

#==============================================================================
# Configuration Definitions
#==============================================================================

@dataclass
class RamConfig:
    name: str
    width: int
    depth: int
    is_dual_port: bool = False
    width_b: Optional[int] = None
    depth_b: Optional[int] = None
    
    @property
    def addr_width(self) -> int:
        return math.ceil(math.log2(self.depth))
    
    @property
    def addr_b_width(self) -> int:
        if self.is_dual_port and self.depth_b:
            return math.ceil(math.log2(self.depth_b))
        return self.addr_width
    
    @property
    def data_width_ones(self) -> str:
        return f"{self.width}'b" + "1" * self.width
    
    @property
    def data_b_width_ones(self) -> Optional[str]:
        if self.is_dual_port and self.width_b:
            return f"{self.width_b}'b" + "1" * self.width_b
        return None

_VALID_SINGLE_PORT = [
    (1, 4096), (2, 2048), (4, 1024), (8, 512), (16, 256),
    (2, 4096), (4, 2048), (8, 1024), (16, 512), (32, 256),
    (4, 4096), (8, 2048), (16, 1024), (32, 512), (64, 256),
    (8, 4096), (16, 2048), (32, 1024), (64, 512), (128, 256),
    (16, 4096), (32, 2048), (64, 1024), (128, 512), (256, 256),
]

_VALID_DUAL_PORT = [
    (1, 4096, 1, 4096),     # S1_S1
    (2, 2048, 2, 2048),     # S2_S2
    (4, 1024, 4, 1024),     # S4_S4
    (8, 512, 8, 512),       # S8_S8
    (16, 256, 16, 256),     # S16_S16
    (1, 4096, 2, 2048),     # S1_S2
    (1, 4096, 4, 1024),     # S1_S4
    (1, 4096, 8, 512),      # S1_S8
    (1, 4096, 16, 256),     # S1_S16
    (2, 2048, 4, 1024),     # S2_S4
    (2, 2048, 8, 512),      # S2_S8
    (2, 2048, 16, 256),     # S2_S16
    (4, 1024, 8, 512),      # S4_S8
    (4, 1024, 16, 256),     # S4_S16
    (8, 512, 16, 256),      # S8_S16
    
    (2, 4096, 2, 4096),     # S2_S2 (8Kb)
    (4, 2048, 4, 2048),     # S4_S4 (8Kb)
    (8, 1024, 8, 1024),     # S8_S8 (8Kb)
    (16, 512, 16, 512),     # S16_S16 (8Kb)
    (32, 256, 32, 256),     # S32_S32 (8Kb)
    (2, 4096, 4, 2048),     # S2_S4 (8Kb)
    (2, 4096, 8, 1024),     # S2_S8 (8Kb)
    (2, 4096, 16, 512),     # S2_S16 (8Kb)
    (2, 4096, 32, 256),     # S2_S32 (8Kb)
    (4, 2048, 8, 1024),     # S4_S8 (8Kb)
    (4, 2048, 16, 512),     # S4_S16 (8Kb)
    (4, 2048, 32, 256),     # S4_S32 (8Kb)
    (8, 1024, 16, 512),     # S8_S16 (8Kb)
    (8, 1024, 32, 256),     # S8_S32 (8Kb)
    (16, 512, 32, 256),     # S16_S32 (8Kb)
    
    (4, 4096, 4, 4096),     # S4_S4 (16Kb)
    (8, 2048, 8, 2048),     # S8_S8 (16Kb)
    (16, 1024, 16, 1024),   # S16_S16 (16Kb)
    (32, 512, 32, 512),     # S32_S32 (16Kb)
    (64, 256, 64, 256),     # S64_S64 (16Kb)
    (4, 4096, 8, 2048),     # S4_S8 (16Kb)
    (4, 4096, 16, 1024),    # S4_S16 (16Kb)
    (4, 4096, 32, 512),     # S4_S32 (16Kb)
    (4, 4096, 64, 256),     # S4_S64 (16Kb)
    (8, 2048, 16, 1024),    # S8_S16 (16Kb)
    (8, 2048, 32, 512),     # S8_S32 (16Kb)
    (8, 2048, 64, 256),     # S8_S64 (16Kb)
    (16, 1024, 32, 512),    # S16_S32 (16Kb)
    (16, 1024, 64, 256),    # S16_S64 (16Kb)
    (32, 512, 64, 256),     # S32_S64 (16Kb)
    
    (8, 4096, 8, 4096),     # S8_S8 (32Kb)
    (16, 2048, 16, 2048),   # S16_S16 (32Kb)
    (32, 1024, 32, 1024),   # S32_S32 (32Kb)
    (64, 512, 64, 512),     # S64_S64 (32Kb)
    (128, 256, 128, 256),   # S128_S128 (32Kb)
    (8, 4096, 16, 2048),    # S8_S16 (32Kb)
    (8, 4096, 32, 1024),    # S8_S32 (32Kb)
    (8, 4096, 64, 512),     # S8_S64 (32Kb)
    (8, 4096, 128, 256),    # S8_S128 (32Kb)
    (16, 2048, 32, 1024),   # S16_S32 (32Kb)
    (16, 2048, 64, 512),    # S16_S64 (32Kb)
    (16, 2048, 128, 256),   # S16_S128 (32Kb)
    (32, 1024, 64, 512),    # S32_S64 (32Kb)
    (32, 1024, 128, 256),   # S32_S128 (32Kb)
    (64, 512, 128, 256),    # S64_S128 (32Kb)
    
    (16, 4096, 16, 4096),   # S16_S16 (64Kb)
    (32, 2048, 32, 2048),   # S32_S32 (64Kb)
    (64, 1024, 64, 1024),   # S64_S64 (64Kb)
    (128, 512, 128, 512),   # S128_S128 (64Kb)
    (256, 256, 256, 256),   # S256_S256 (64Kb)
    (16, 4096, 32, 2048),   # S16_S32 (64Kb)
    (16, 4096, 64, 1024),   # S16_S64 (64Kb)
    (16, 4096, 128, 512),   # S16_S128 (64Kb)
    (16, 4096, 256, 256),   # S16_S256 (64Kb)
    (32, 2048, 64, 1024),   # S32_S64 (64Kb)
    (32, 2048, 128, 512),   # S32_S128 (64Kb)
    (32, 2048, 256, 256),   # S32_S256 (64Kb)
    (64, 1024, 128, 512),   # S64_S128 (64Kb)
    (64, 1024, 256, 256),   # S64_S256 (64Kb)
    (128, 512, 256, 256),   # S128_S256 (64Kb)
]

#==============================================================================
# Test File Generator
#==============================================================================

class TestFileGenerator:
    
    def __init__(self, base_dir: str):
        self.base_dir = Path(base_dir)
        self.script_dir = Path(__file__).parent.absolute()
        
        template_dir = self.script_dir / "templates_test"
        self.env = Environment(
            loader=FileSystemLoader(template_dir),
            trim_blocks=True,
            lstrip_blocks=True
        )
        
        self.mif_template = self.env.get_template("test_mif.j2")
        self.tb_template = self.env.get_template("test_testbench.j2")
    
    def _generate_config_name(self, width: int, depth: int, 
                              width_b: Optional[int] = None,
                              depth_b: Optional[int] = None) -> str:
        if width_b is None:
            return f"{width}x{depth}"
        else:
            return f"{width}x{depth}_{width_b}x{depth_b}"
    
    def _generate_test_data(self, config: RamConfig, seed: int = 42) -> List[dict]:
        random.seed(seed)
        data = []
        
        max_val = (1 << config.width) - 1
        
        for addr in range(config.depth):
            if addr < config.depth // 4:
                value = (addr * 7 + 13) & max_val
            elif addr < config.depth // 2:
                if addr == config.depth // 4:
                    value = 0
                elif addr == config.depth // 4 + 1:
                    value = max_val
                else:
                    value = (addr << (config.width // 2)) & max_val
            else:
                value = random.randint(0, max_val)
            
            hex_chars = (config.width + 3) // 4
            data.append({
                'addr': addr,
                'value': f"{value:0{hex_chars}X}"
            })
        
        return data
    
    def _format_mif_content(self, config: RamConfig, data: List[dict]) -> str:
        if config.is_dual_port:
            return self.mif_template.render(
                is_dual_port=True,
                width_a=config.width,
                depth_a=config.depth,
                width_b=config.width_b,
                depth_b=config.depth_b,
                data=data
            )
        else:
            return self.mif_template.render(
                is_dual_port=False,
                width=config.width,
                depth=config.depth,
                data=data
            )
    
    def _format_testbench(self, config: RamConfig, mif_filename: str) -> str:
        context = {
            'config_name': config.name,
            'addr_width': config.addr_width,
            'data_width': config.width,
            'depth': config.depth,
            'dut_module': "top",
            'mif_file': mif_filename,
            'is_dual_port': config.is_dual_port,
            'data_width_ones': config.data_width_ones,
        }
        
        if config.is_dual_port:
            context.update({
                'addr_b_width': config.addr_b_width,
                'data_b_width': config.width_b,
                'depth_b': config.depth_b,
                'data_b_width_ones': config.data_b_width_ones,
            })
        
        return self.tb_template.render(**context)
    
    def generate_single_test(self, config: RamConfig, output_subdir: str = "") -> Tuple[Path, Path]:
        if output_subdir:
            test_dir = self.base_dir / output_subdir / config.name
        else:
            test_dir = self.base_dir / config.name
        
        test_dir.mkdir(parents=True, exist_ok=True)
        
        mif_data = self._generate_test_data(config)
        mif_content = self._format_mif_content(config, mif_data)
        mif_path = test_dir / "test.mif"
        with open(mif_path, 'w', encoding='utf-8') as f:
            f.write(mif_content)
        
        tb_content = self._format_testbench(config, "test.mif")
        tb_path = test_dir / f"tb_test.sv"
        with open(tb_path, 'w', encoding='utf-8') as f:
            f.write(tb_content)
        
        self._generate_wrapper(config, test_dir)
        
        self._generate_constraint(config, test_dir)
        
        return mif_path, tb_path
    
    def _generate_wrapper(self, config: RamConfig, test_dir: Path):
        wrapper_template = self.env.get_template("wrapper_template.j2")
        
        context = {
            'config_name': config.name,
            'dut_module': "test",
            'addr_a_width': config.addr_width,
            'data_a_width': config.width,
            'is_dual_port': config.is_dual_port,
        }
        
        if config.is_dual_port:
            context.update({
                'addr_b_width': config.addr_b_width,
                'data_b_width': config.width_b,
            })
        
        wrapper_content = wrapper_template.render(**context)
        wrapper_path = test_dir / "top.v"
        with open(wrapper_path, 'w', encoding='utf-8') as f:
            f.write(wrapper_content)
    
    def _generate_constraint(self, config: RamConfig, test_dir: Path):
        device = 'FDP3P7'
        pins = DEVICE_PINS[device]
        
        input_pins = pins['input'].copy()
        output_pins = pins['output'].copy()
        clock_pin = pins['clock']
        
        port_mappings = []
        
        # Calculate required pins before allocation
        required_input = 2  # ena, wea
        required_output = config.width  # data_out_a
        
        if config.is_dual_port:
            required_input += 2  # enb, web
            required_input += config.addr_width  # addr_a
            required_input += config.width  # data_in_a
            required_input += config.addr_b_width  # addr_b
            width_b = config.width_b or 0
            required_input += width_b  # data_in_b
            required_output += width_b  # data_out_b
        else:
            required_input += config.addr_width  # addr_a
            required_input += config.width  # data_in_a
        
        if len(input_pins) < required_input:
            raise ValueError(
                f"[{config.name}] Insufficient input pins: "
                f"need {required_input}, available {len(input_pins)} "
                f"(missing {required_input - len(input_pins)} input pin(s))"
            )
        if len(output_pins) < required_output:
            raise ValueError(
                f"[{config.name}] Insufficient output pins: "
                f"need {required_output}, available {len(output_pins)} "
                f"(missing {required_output - len(output_pins)} output pin(s))"
            )
        
        # Clock - use dedicated clock pin P77
        # The D-flipflop in wrapper ensures proper clock network insertion
        port_mappings.append(f'  <port name="clk" position="{clock_pin}"/>')
        
        # Control signals (input)
        if input_pins:
            port_mappings.append(f'  <port name="ena" position="{input_pins.pop(0)}"/>')
        if input_pins:
            port_mappings.append(f'  <port name="wea" position="{input_pins.pop(0)}"/>')
        
        if config.is_dual_port:
            if input_pins:
                port_mappings.append(f'  <port name="enb" position="{input_pins.pop(0)}"/>')
            if input_pins:
                port_mappings.append(f'  <port name="web" position="{input_pins.pop(0)}"/>')
        
        # Address A (input)
        for i in range(config.addr_width):
            if input_pins:
                port_mappings.append(f'  <port name="addr_a[{i}]" position="{input_pins.pop(0)}"/>')
        
        # Data A input (input)
        # For 1-bit width, use scalar name (Yosys optimizes [0:0] to scalar)
        if config.width == 1:
            if input_pins:
                port_mappings.append(f'  <port name="data_in_a" position="{input_pins.pop(0)}"/>')
        else:
            for i in range(config.width):
                if input_pins:
                    port_mappings.append(f'  <port name="data_in_a[{i}]" position="{input_pins.pop(0)}"/>')
        
        # Data A output (output)
        if config.width == 1:
            if output_pins:
                port_mappings.append(f'  <port name="data_out_a" position="{output_pins.pop(0)}"/>')
        else:
            for i in range(config.width):
                if output_pins:
                    port_mappings.append(f'  <port name="data_out_a[{i}]" position="{output_pins.pop(0)}"/>')
        
        if config.is_dual_port:
            # Address B (input)
            for i in range(config.addr_b_width):
                if input_pins:
                    port_mappings.append(f'  <port name="addr_b[{i}]" position="{input_pins.pop(0)}"/>')
            
            # Data B input (input)
            width_b = config.width_b or 0
            if width_b == 1:
                if input_pins:
                    port_mappings.append(f'  <port name="data_in_b" position="{input_pins.pop(0)}"/>')
            else:
                for i in range(width_b):
                    if input_pins:
                        port_mappings.append(f'  <port name="data_in_b[{i}]" position="{input_pins.pop(0)}"/>')
            
            # Data B output (output)
            if width_b == 1:
                if output_pins:
                    port_mappings.append(f'  <port name="data_out_b" position="{output_pins.pop(0)}"/>')
            else:
                for i in range(width_b):
                    if output_pins:
                        port_mappings.append(f'  <port name="data_out_b[{i}]" position="{output_pins.pop(0)}"/>')
        
        xml_content = f'''<design name="top">
{chr(10).join(port_mappings)}
</design>'''
        
        constraint_path = test_dir / "top_cons.xml"
        with open(constraint_path, 'w', encoding='utf-8') as f:
            f.write(xml_content)
    
    def _check_pin_feasibility(self, config: RamConfig) -> Optional[str]:
        """Check if the device has enough pins for this config.
        Returns an error message if infeasible, otherwise None."""
        device = 'FDP3P7'
        pins = DEVICE_PINS[device]
        input_pins = pins['input']
        output_pins = pins['output']
        
        required_input = 2  # ena, wea
        required_output = config.width  # data_out_a
        
        if config.is_dual_port:
            required_input += 2  # enb, web
            required_input += config.addr_width
            required_input += config.width
            required_input += config.addr_b_width
            width_b = config.width_b or 0
            required_input += width_b
            required_output += width_b
        else:
            required_input += config.addr_width
            required_input += config.width
        
        if len(input_pins) < required_input:
            return f"input pins shortage (need {required_input}, have {len(input_pins)})"
        if len(output_pins) < required_output:
            return f"output pins shortage (need {required_output}, have {len(output_pins)})"
        return None
    
    def generate_all_tests(self) -> dict:
        stats = {
            'total': 0,
            'single_port': 0,
            'dual_port': 0,
            'skipped': 0,
            'generated': []
        }
        
        print(f"Generating test files to: {self.base_dir.absolute()}")
        print("=" * 60)
        
        print("\n[Single Port Configurations]")
        for width, depth in _VALID_SINGLE_PORT:
            config = RamConfig(
                name=self._generate_config_name(width, depth),
                width=width,
                depth=depth,
                is_dual_port=False
            )
            
            reason = self._check_pin_feasibility(config)
            if reason:
                print(f"  [SKIP] {config.name:12s} -> {reason}")
                stats['skipped'] += 1
                continue
            
            try:
                mif_path, tb_path = self.generate_single_test(config)
                print(f"  [OK]   {config.name:12s} -> {mif_path.parent}")
                
                stats['single_port'] += 1
                stats['total'] += 1
                stats['generated'].append({
                    'name': config.name,
                    'type': 'single',
                    'dir': str(mif_path.parent)
                })
            except ValueError as e:
                print(f"  [SKIP] {config.name:12s} -> {e}")
                stats['skipped'] += 1
                # Clean up partially created directory
                test_dir = self.base_dir / config.name
                if test_dir.exists():
                    import shutil
                    shutil.rmtree(test_dir)
        
        print("\n[Dual Port Configurations]")
        for width_a, depth_a, width_b, depth_b in _VALID_DUAL_PORT:
            config = RamConfig(
                name=self._generate_config_name(width_a, depth_a, width_b, depth_b),
                width=width_a,
                depth=depth_a,
                is_dual_port=True,
                width_b=width_b,
                depth_b=depth_b
            )
            
            reason = self._check_pin_feasibility(config)
            if reason:
                print(f"  [SKIP] {config.name:12s} -> {reason}")
                stats['skipped'] += 1
                continue
            
            try:
                mif_path, tb_path = self.generate_single_test(config)
                print(f"  [OK]   {config.name:12s} -> {mif_path.parent}")
                
                stats['dual_port'] += 1
                stats['total'] += 1
                stats['generated'].append({
                    'name': config.name,
                    'type': 'dual',
                    'dir': str(mif_path.parent),
                    'port_a': f"{depth_a}X{width_a}",
                    'port_b': f"{depth_b}X{width_b}"
                })
            except ValueError as e:
                print(f"  [SKIP] {config.name:12s} -> {e}")
                stats['skipped'] += 1
                # Clean up partially created directory
                test_dir = self.base_dir / config.name
                if test_dir.exists():
                    import shutil
                    shutil.rmtree(test_dir)
        
        print("\n" + "=" * 60)
        print(f"Generation complete!")
        print(f"  Generated:   {stats['total']}")
        print(f"  Skipped:     {stats['skipped']}")
        print(f"  Single-port: {stats['single_port']}")
        print(f"  Dual-port:   {stats['dual_port']}")
        
        return stats
    
    def generate_all_ips(self) -> dict:
        import json
        
        stats = {
            'total': 0,
            'success': 0,
            'failed': 0,
            'errors': []
        }
        
        print(f"\nGenerating IP files from MIF...")
        print("=" * 60)
        
        for subdir in sorted(self.base_dir.iterdir()):
            if not subdir.is_dir():
                continue
            
            mif_file = subdir / "test.mif"
            if not mif_file.exists():
                print(f"  [SKIP] {subdir.name}: MIF not found")
                continue
            
            stats['total'] += 1
            
            output_v = subdir / "test.v"
            result = generate_bram_from_mif(str(mif_file), str(output_v))
            
            if result.get('success'):
                stats['success'] += 1
                print(f"  [OK]   {subdir.name}: {result.get('message', 'Generated')}")
            else:
                stats['failed'] += 1
                error_msg = result.get('error', 'Unknown error')
                stats['errors'].append({
                    'config': subdir.name,
                    'error': error_msg
                })
                print(f"  [FAIL] {subdir.name}: {error_msg}")
        
        print("\n" + "=" * 60)
        print(f"IP Generation complete!")
        print(f"  Total: {stats['total']}")
        print(f"  Success: {stats['success']}")
        print(f"  Failed: {stats['failed']}")
        
        if stats['errors']:
            print("\nErrors:")
            for err in stats['errors']:
                print(f"  - {err['config']}: {err['error']}")
        
        return stats

#==============================================================================
# Main Entry
#==============================================================================

def main():
    try:
        print("Starting test_ram.py...")
        
        script_dir = Path(__file__).parent.absolute()
        print(f"Script directory: {script_dir}")
        
        test_base_dir = script_dir / "test"
        print(f"Output directory: {test_base_dir}")
        
        print("Creating generator...")
        generator = TestFileGenerator(str(test_base_dir))
        
        print("\nGenerating tests...")
        stats = generator.generate_all_tests()
        
        ip_stats = generator.generate_all_ips()
        
        print("\nAll done!")
        return 0
    except Exception as e:
        print(f"ERROR: {e}")
        import traceback
        traceback.print_exc()
        return 1

if __name__ == "__main__":
    sys.exit(main())
