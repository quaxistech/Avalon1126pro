#!/usr/bin/env python3
"""
RISC-V Disassembler for Avalon A1126pro K210 Firmware
=====================================================
Generates disassembly with symbol resolution and cross-references
"""

import struct
import subprocess
import re
import os
from dataclasses import dataclass
from typing import List, Dict, Tuple, Optional

@dataclass
class Instruction:
    address: int
    raw: bytes
    mnemonic: str
    operands: str
    comment: str = ""

@dataclass
class Function:
    name: str
    start: int
    end: int
    instructions: List[Instruction]
    calls: List[int]
    strings_refs: List[int]

class RiscVDisassembler:
    """RISC-V Disassembler using objdump backend"""
    
    # K210 memory map
    SRAM_BASE = 0x80000000
    FLASH_BASE = 0x00000000
    
    # Common register names
    REG_NAMES = {
        'x0': 'zero', 'x1': 'ra', 'x2': 'sp', 'x3': 'gp',
        'x4': 'tp', 'x5': 't0', 'x6': 't1', 'x7': 't2',
        'x8': 's0/fp', 'x9': 's1', 'x10': 'a0', 'x11': 'a1',
        'x12': 'a2', 'x13': 'a3', 'x14': 'a4', 'x15': 'a5',
        'x16': 'a6', 'x17': 'a7', 'x18': 's2', 'x19': 's3',
        'x20': 's4', 'x21': 's5', 'x22': 's6', 'x23': 's7',
        'x24': 's8', 'x25': 's9', 'x26': 's10', 'x27': 's11',
        'x28': 't3', 'x29': 't4', 'x30': 't5', 'x31': 't6',
    }
    
    def __init__(self, firmware_path: str):
        self.firmware_path = firmware_path
        with open(firmware_path, 'rb') as f:
            self.data = f.read()
        self.strings = self._extract_strings()
        self.functions: Dict[int, Function] = {}
        
    def _extract_strings(self) -> Dict[int, str]:
        """Extract string table for cross-referencing"""
        strings = {}
        current = b''
        start = 0
        
        for i, b in enumerate(self.data):
            if 32 <= b < 127:
                if not current:
                    start = i
                current += bytes([b])
            else:
                if len(current) >= 4:
                    try:
                        strings[start] = current.decode('ascii')
                    except:
                        pass
                current = b''
        return strings
    
    def disassemble_range(self, start: int, end: int, base_addr: int = 0x80000000) -> List[str]:
        """Disassemble a range of bytes using riscv64 objdump"""
        
        # Extract the range to a temp file
        temp_file = '/tmp/k210_section.bin'
        with open(temp_file, 'wb') as f:
            f.write(self.data[start:end])
        
        # Run objdump
        cmd = [
            'riscv64-unknown-elf-objdump',
            '-D',
            '-b', 'binary',
            '-m', 'riscv:rv64',
            f'--adjust-vma={base_addr + start}',
            temp_file
        ]
        
        try:
            result = subprocess.run(cmd, capture_output=True, text=True, timeout=30)
            lines = result.stdout.split('\n')
            
            # Parse output
            disasm = []
            for line in lines:
                if ':' in line and '\t' in line:
                    # Parse: "80000000:\t1234abcd \tinstruction"
                    disasm.append(line.strip())
            
            return disasm
        except Exception as e:
            print(f"Disassembly error: {e}")
            return []
        finally:
            if os.path.exists(temp_file):
                os.remove(temp_file)
    
    def find_function_boundaries(self) -> List[Tuple[int, int]]:
        """Find function start/end by prologue/epilogue patterns"""
        
        functions = []
        
        # RISC-V function patterns (compressed and full)
        # Prologue: addi sp, sp, -N  (save stack)
        # Epilogue: ret (jalr zero, ra, 0) or c.ret
        
        # Common prologue patterns (16-bit compressed)
        # 0x1141 = c.addi sp, sp, -16
        # 0x7139 = c.addi16sp sp, -112
        # etc.
        
        prologue_patterns = [
            b'\x01\x11',  # c.addi sp,-16
            b'\x39\x71',  # c.addi16sp -112
            b'\x79\x71',  # c.addi16sp -144
            b'\x41\x11',  # c.addi sp,-48
        ]
        
        # Full 32-bit: addi sp, sp, -X (0x113 opcode)
        # Format: imm[11:0] rs1 000 rd 0010011
        
        for i in range(0, min(len(self.data) - 4, 0x100000), 2):
            word = self.data[i:i+2]
            
            for pattern in prologue_patterns:
                if word == pattern:
                    # Potential function start
                    # Look for epilogue (ret = c.jr ra = 0x8082)
                    for j in range(i + 4, min(i + 0x2000, len(self.data) - 2), 2):
                        if self.data[j:j+2] == b'\x82\x80':  # c.ret
                            functions.append((i, j + 2))
                            break
                    break
        
        return functions[:200]  # Limit for performance
    
    def analyze_function(self, start: int, end: int) -> Optional[Function]:
        """Analyze a single function"""
        
        disasm = self.disassemble_range(start, end)
        if not disasm:
            return None
        
        instructions = []
        calls = []
        string_refs = []
        
        for line in disasm:
            # Parse instruction
            parts = line.split('\t')
            if len(parts) >= 2:
                addr_str = parts[0].rstrip(':')
                try:
                    addr = int(addr_str, 16)
                except:
                    continue
                
                raw_hex = parts[1].strip() if len(parts) > 1 else ""
                mnemonic = parts[2].strip() if len(parts) > 2 else ""
                operands = parts[3].strip() if len(parts) > 3 else ""
                
                # Check for call/jump instructions
                if 'jal' in mnemonic or 'call' in mnemonic:
                    # Extract target address
                    match = re.search(r'0x([0-9a-f]+)', operands)
                    if match:
                        calls.append(int(match.group(1), 16))
                
                # Check for string references (lui + addi pattern)
                if 'lui' in mnemonic or 'auipc' in mnemonic:
                    match = re.search(r'0x([0-9a-f]+)', operands)
                    if match:
                        ref_addr = int(match.group(1), 16)
                        if ref_addr in self.strings:
                            string_refs.append(ref_addr)
                
                instructions.append(Instruction(
                    address=addr,
                    raw=bytes.fromhex(raw_hex.replace(' ', '')) if raw_hex else b'',
                    mnemonic=mnemonic,
                    operands=operands
                ))
        
        return Function(
            name=f"sub_{start:08X}",
            start=start,
            end=end,
            instructions=instructions,
            calls=calls,
            strings_refs=string_refs
        )
    
    def generate_annotated_disassembly(self, output_path: str):
        """Generate fully annotated disassembly file"""
        
        print("Finding function boundaries...")
        func_bounds = self.find_function_boundaries()
        print(f"Found {len(func_bounds)} potential functions")
        
        with open(output_path, 'w') as f:
            f.write("; Avalon A1126pro K210 Firmware Disassembly\n")
            f.write("; Generated by RISC-V Disassembler\n")
            f.write("; Base address: 0x80000000 (SRAM)\n")
            f.write("; Architecture: RISC-V 64-bit with C extension\n")
            f.write(";\n")
            f.write("; Memory Map:\n")
            f.write(";   0x00000000 - 0x00FFFFFF : SPI Flash\n")
            f.write(";   0x80000000 - 0x805FFFFF : SRAM (6MB)\n")
            f.write(";   0x40000000 - 0x4FFFFFFF : Peripherals\n")
            f.write(";\n" + "=" * 70 + "\n\n")
            
            # Disassemble boot section
            f.write("; BOOT SECTION (0x00000000 - 0x00001000)\n")
            f.write(";" + "-" * 69 + "\n\n")
            
            boot_disasm = self.disassemble_range(0, 0x1000)
            for line in boot_disasm[:100]:
                f.write(f"  {line}\n")
            
            f.write("\n\n")
            
            # Disassemble functions
            f.write("; FUNCTIONS\n")
            f.write(";" + "-" * 69 + "\n\n")
            
            for i, (start, end) in enumerate(func_bounds[:50]):
                func = self.analyze_function(start, end)
                if func:
                    f.write(f"\n; Function: {func.name}\n")
                    f.write(f"; Start: 0x{start:08X}, End: 0x{end:08X}\n")
                    
                    if func.calls:
                        f.write(f"; Calls: {', '.join(f'0x{c:08X}' for c in func.calls[:5])}\n")
                    if func.strings_refs:
                        for ref in func.strings_refs[:3]:
                            if ref in self.strings:
                                f.write(f'; String ref: "{self.strings[ref][:50]}"\n')
                    
                    f.write(f"{func.name}:\n")
                    for instr in func.instructions:
                        f.write(f"  {instr.address:08X}:  {instr.mnemonic:12s} {instr.operands}\n")
                    f.write("\n")
                    
                if i % 10 == 0:
                    print(f"  Processed {i}/{len(func_bounds)} functions...")
        
        print(f"Disassembly saved to {output_path}")
    
    def generate_idc_script(self, output_path: str):
        """Generate IDC script for IDA Pro"""
        
        with open(output_path, 'w') as f:
            f.write('// Avalon A1126pro IDA Pro Script\n')
            f.write('// Apply this script after loading binary at 0x80000000\n\n')
            f.write('#include <idc.idc>\n\n')
            f.write('static main() {\n')
            
            # Add string labels
            f.write('  // String references\n')
            for addr, s in list(self.strings.items())[:500]:
                safe_name = re.sub(r'[^a-zA-Z0-9_]', '_', s[:30])
                f.write(f'  MakeName(0x{addr:08X}, "str_{safe_name}");\n')
            
            f.write('}\n')
        
        print(f"IDA script saved to {output_path}")

def main():
    firmware_path = '/home/quaxis/projects/FirmwareASIC/Avalon/A1126pro/donor_dump.bin'
    output_dir = '/home/quaxis/projects/FirmwareASIC/Avalon/A1126pro/analysis'
    
    print("=" * 60)
    print("RISC-V Disassembler for Avalon A1126pro")
    print("=" * 60)
    
    disasm = RiscVDisassembler(firmware_path)
    
    print(f"\nFirmware size: {len(disasm.data)} bytes")
    print(f"Strings found: {len(disasm.strings)}")
    
    # Generate annotated disassembly
    print("\nGenerating annotated disassembly...")
    disasm.generate_annotated_disassembly(os.path.join(output_dir, 'disassembly.asm'))
    
    # Generate IDA script
    print("\nGenerating IDA script...")
    disasm.generate_idc_script(os.path.join(output_dir, 'ida_script.idc'))
    
    print("\n[✓] Done!")

if __name__ == '__main__':
    main()
