#!/usr/bin/env python3
"""
Avalon A1126pro (K210) Firmware Analyzer
========================================
Deep analysis and decompilation toolkit for donor_dump.bin

K210 Memory Map:
- 0x00000000-0x00FFFFFF: SPI Flash (mapped)
- 0x80000000-0x805FFFFF: SRAM (6MB)
- 0x80600000-0x807FFFFF: AI SRAM (2MB)
- 0x40000000-0x4FFFFFFF: Peripheral registers

Firmware Structure:
- Slot A: 0x000000-0x1FFFFF (2MB) - Main firmware
- Slot B: 0x200000-0x3FFFFF (2MB) - Backup/OTA firmware
- Config: 0x7F0000-0x7FFFFF (64KB) - Configuration area
"""

import struct
import re
import os
from dataclasses import dataclass
from typing import List, Dict, Tuple, Optional
import json

@dataclass
class Section:
    name: str
    offset: int
    size: int
    description: str

@dataclass
class Symbol:
    name: str
    address: int
    size: int
    type: str  # 'function', 'string', 'data'

class K210FirmwareAnalyzer:
    # K210 kflash header magic
    KFLASH_MAGIC = 0x00B9DB00
    
    # Memory base addresses
    SRAM_BASE = 0x80000000
    FLASH_BASE = 0x00000000
    
    # Known section boundaries
    SECTIONS = {
        'boot_header': (0x000000, 0x00001C),
        'boot_code': (0x00001C, 0x00BA00),
        'main_code': (0x00BA00, 0x095000),
        'rodata': (0x095000, 0x0A8000),
        'web_resources': (0x0A8000, 0x0B8000),
        'rtos_code': (0x0B8000, 0x100000),
        'slot_b': (0x200000, 0x400000),
        'config': (0x7F0000, 0x800000),
    }
    
    def __init__(self, firmware_path: str):
        self.firmware_path = firmware_path
        with open(firmware_path, 'rb') as f:
            self.data = f.read()
        self.symbols: List[Symbol] = []
        self.strings: Dict[int, str] = {}
        
    def verify_header(self) -> bool:
        """Verify K210 kflash header"""
        magic = struct.unpack('<I', self.data[0:4])[0]
        return magic == self.KFLASH_MAGIC
    
    def parse_header(self) -> dict:
        """Parse kflash firmware header"""
        header = {}
        header['magic'] = struct.unpack('<I', self.data[0:4])[0]
        header['flags'] = struct.unpack('<I', self.data[4:8])[0]
        header['checksum'] = struct.unpack('<I', self.data[8:12])[0]
        return header
    
    def extract_strings(self, min_length: int = 8) -> Dict[int, str]:
        """Extract all printable strings from firmware"""
        strings = {}
        current = b''
        start_offset = 0
        
        for i, b in enumerate(self.data):
            if 32 <= b < 127 or b in (9, 10, 13):  # printable + whitespace
                if not current:
                    start_offset = i
                current += bytes([b])
            else:
                if len(current) >= min_length:
                    try:
                        s = current.decode('utf-8')
                        strings[start_offset] = s
                    except:
                        pass
                current = b''
        
        self.strings = strings
        return strings
    
    def find_function_names(self) -> List[Symbol]:
        """Extract function name symbols from string table"""
        functions = []
        patterns = [
            rb'avalon\d+_\w+',
            rb'cgminer_\w+',
            rb'miner_\w+',
            rb'pool_\w+',
            rb'sha\d*_\w+',
            rb'lwip_\w+',
            rb'freertos_\w+',
            rb'k210_\w+',
            rb'spi_\w+',
            rb'gpio_\w+',
            rb'uart_\w+',
            rb'i2c_\w+',
            rb'timer_\w+',
            rb'plic_\w+',
            rb'sysctl_\w+',
        ]
        
        for pattern in patterns:
            for match in re.finditer(pattern, self.data, re.IGNORECASE):
                name = match.group(0).decode('ascii', errors='ignore')
                functions.append(Symbol(
                    name=name,
                    address=match.start(),
                    size=len(name),
                    type='function_ref'
                ))
        
        return functions
    
    def find_avalon10_config(self) -> dict:
        """Extract Avalon10 mining configuration parameters"""
        config = {}
        
        # Command line options
        option_pattern = rb'--avalon10-([a-z0-9-]+)'
        options = []
        for match in re.finditer(option_pattern, self.data):
            options.append(match.group(1).decode('ascii'))
        config['options'] = list(set(options))
        
        # Find default values in strings
        defaults = {}
        default_pattern = rb'default[:\s]+(\d+)'
        for match in re.finditer(default_pattern, self.data, re.IGNORECASE):
            # Context around the match
            start = max(0, match.start() - 50)
            context = self.data[start:match.start()].decode('ascii', errors='ignore')
            if 'freq' in context.lower():
                defaults['frequency'] = int(match.group(1))
            elif 'volt' in context.lower():
                defaults['voltage'] = int(match.group(1))
            elif 'temp' in context.lower():
                defaults['temperature'] = int(match.group(1))
            elif 'fan' in context.lower():
                defaults['fan'] = int(match.group(1))
        
        config['defaults'] = defaults
        return config
    
    def extract_web_interface(self) -> List[Tuple[int, str]]:
        """Extract embedded web interface files"""
        web_files = []
        
        # Find HTML content
        html_pattern = rb'<!DOCTYPE[^>]*>|<html[^>]*>'
        for match in re.finditer(html_pattern, self.data, re.IGNORECASE):
            offset = match.start()
            # Find end of HTML
            end_pos = self.data.find(b'</html>', offset)
            if end_pos != -1:
                content = self.data[offset:end_pos+7].decode('utf-8', errors='ignore')
                web_files.append((offset, content))
        
        # Find JavaScript
        js_pattern = rb'function\s+\w+\s*\([^)]*\)\s*\{'
        for match in re.finditer(js_pattern, self.data):
            web_files.append((match.start(), f"JS function at 0x{match.start():X}"))
        
        return web_files
    
    def analyze_code_sections(self) -> List[Section]:
        """Identify code sections by instruction patterns"""
        sections = []
        
        # RISC-V function prologue: addi sp, sp, -X (compressed or full)
        # Compressed: c.addi16sp or similar
        # Full: 0x113 (addi with sp)
        
        # Find potential function starts
        func_starts = []
        
        # Look for common function entry patterns
        for i in range(0, len(self.data) - 4, 2):
            word = struct.unpack('<H', self.data[i:i+2])[0]
            
            # c.addi sp, sp patterns (stack frame setup)
            if (word & 0xFFFF) in [0x1101, 0x1141, 0x7139, 0x7179]:  # Common prologues
                func_starts.append(i)
        
        print(f"Found {len(func_starts)} potential function entries")
        return sections
    
    def export_symbols(self, output_path: str):
        """Export symbols for IDA/Ghidra"""
        symbols = {
            'strings': {hex(k): v for k, v in self.strings.items()},
            'functions': [
                {'name': s.name, 'address': hex(s.address), 'type': s.type}
                for s in self.symbols
            ]
        }
        
        with open(output_path, 'w') as f:
            json.dump(symbols, f, indent=2)
    
    def create_ghidra_script(self) -> str:
        """Generate Ghidra Python script for analysis"""
        script = '''# Ghidra script for Avalon A1126pro firmware
# Auto-generated by firmware_analyzer.py

from ghidra.program.model.symbol import SourceType
from ghidra.program.model.data import StringDataType

# Memory regions for K210
SRAM_BASE = 0x80000000
FLASH_BASE = 0x00000000

# Known function addresses (from string analysis)
FUNCTIONS = {
'''
        # Add known function references
        for sym in self.symbols:
            if sym.type == 'function_ref':
                script += f'    0x{sym.address:08X}: "{sym.name}",\n'
        
        script += '''}

# Apply function names
for addr, name in FUNCTIONS.items():
    try:
        func = getFunctionAt(toAddr(addr))
        if func:
            func.setName(name, SourceType.USER_DEFINED)
    except:
        pass

print("Avalon A1126pro firmware symbols applied")
'''
        return script
    
    def create_ida_script(self) -> str:
        """Generate IDA Python script for analysis"""
        script = '''# IDA Python script for Avalon A1126pro firmware
# Auto-generated by firmware_analyzer.py

import idaapi
import idc

# Memory map
SRAM_BASE = 0x80000000
FLASH_BASE = 0x00000000

# String references
STRINGS = {
'''
        for addr, s in list(self.strings.items())[:500]:  # Limit for readability
            escaped = s.replace('\\', '\\\\').replace('"', '\\"').replace('\n', '\\n')[:80]
            script += f'    0x{addr:08X}: "{escaped}",\n'
        
        script += '''}

# Apply string labels
for addr, name in STRINGS.items():
    idc.set_name(addr, f"str_{addr:08X}", idc.SN_NOWARN)

print("Avalon A1126pro strings labeled")
'''
        return script
    
    def full_analysis(self) -> dict:
        """Run complete firmware analysis"""
        results = {}
        
        print("=" * 60)
        print("Avalon A1126pro Firmware Analysis")
        print("=" * 60)
        
        # Header
        print("\n[1] Verifying firmware header...")
        results['valid_header'] = self.verify_header()
        results['header'] = self.parse_header()
        print(f"  Magic: 0x{results['header']['magic']:08X}")
        print(f"  Valid kflash: {results['valid_header']}")
        
        # Size
        results['size'] = len(self.data)
        print(f"  Size: {results['size']} bytes ({results['size']//1024//1024} MB)")
        
        # Strings
        print("\n[2] Extracting strings...")
        self.extract_strings(min_length=6)
        results['string_count'] = len(self.strings)
        print(f"  Found {results['string_count']} strings")
        
        # Functions
        print("\n[3] Finding function references...")
        self.symbols = self.find_function_names()
        results['function_count'] = len(self.symbols)
        print(f"  Found {results['function_count']} function references")
        
        # Avalon10 config
        print("\n[4] Extracting Avalon10 configuration...")
        results['avalon10_config'] = self.find_avalon10_config()
        print(f"  Options: {len(results['avalon10_config']['options'])}")
        
        # CGMiner version
        print("\n[5] Identifying CGMiner version...")
        cgminer_match = re.search(rb'cgminer (\d+\.\d+\.\d+)', self.data)
        if cgminer_match:
            results['cgminer_version'] = cgminer_match.group(1).decode('ascii')
            print(f"  Version: {results['cgminer_version']}")
        
        # Web interface
        print("\n[6] Analyzing web interface...")
        web_files = self.extract_web_interface()
        results['web_files_count'] = len(web_files)
        print(f"  Found {results['web_files_count']} web resources")
        
        # Key addresses
        print("\n[7] Key memory addresses:")
        key_strings = ['CGMiner', 'avalon10', 'FreeRTOS', 'lwIP', 'K210']
        for s in key_strings:
            pos = self.data.find(s.encode())
            if pos != -1:
                print(f"  {s}: 0x{pos:06X}")
        
        return results

def main():
    import sys
    
    if len(sys.argv) < 2:
        firmware_path = '/home/quaxis/projects/FirmwareASIC/Avalon/A1126pro/donor_dump.bin'
    else:
        firmware_path = sys.argv[1]
    
    if not os.path.exists(firmware_path):
        print(f"Error: {firmware_path} not found")
        return
    
    analyzer = K210FirmwareAnalyzer(firmware_path)
    results = analyzer.full_analysis()
    
    # Export results
    output_dir = os.path.dirname(firmware_path)
    analysis_dir = os.path.join(output_dir, 'analysis')
    os.makedirs(analysis_dir, exist_ok=True)
    
    # Save results
    with open(os.path.join(analysis_dir, 'analysis_results.json'), 'w') as f:
        # Convert to JSON-serializable format
        json_results = {
            k: v if not isinstance(v, bytes) else v.hex() 
            for k, v in results.items()
        }
        json.dump(json_results, f, indent=2, default=str)
    
    # Export symbols
    analyzer.export_symbols(os.path.join(analysis_dir, 'symbols.json'))
    
    # Generate scripts
    with open(os.path.join(analysis_dir, 'ghidra_script.py'), 'w') as f:
        f.write(analyzer.create_ghidra_script())
    
    with open(os.path.join(analysis_dir, 'ida_script.py'), 'w') as f:
        f.write(analyzer.create_ida_script())
    
    print(f"\n[✓] Analysis complete! Results saved to {analysis_dir}/")
    print("\nNext steps:")
    print("1. Load donor_dump.bin into Ghidra/IDA as RISC-V 64-bit")
    print("2. Set base address to 0x80000000")
    print("3. Run the generated scripts to apply symbols")
    print("4. Cross-reference with kendryte-freertos-sdk source")

if __name__ == '__main__':
    main()
