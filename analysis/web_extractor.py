#!/usr/bin/env python3
"""
Web Interface Extractor for Avalon A1126pro
============================================
Extracts embedded HTML, CSS, JavaScript from firmware
"""

import struct
import re
import os
import base64
from typing import List, Dict, Tuple

class WebExtractor:
    def __init__(self, firmware_path: str):
        with open(firmware_path, 'rb') as f:
            self.data = f.read()
    
    def find_html_files(self) -> List[Tuple[int, int, str]]:
        """Find HTML content in firmware"""
        files = []
        
        # Pattern for HTML documents
        patterns = [
            (rb'<!DOCTYPE\s+html[^>]*>', rb'</html>'),
            (rb'<html[^>]*>', rb'</html>'),
        ]
        
        for start_pattern, end_pattern in patterns:
            for match in re.finditer(start_pattern, self.data, re.IGNORECASE):
                start = match.start()
                # Find closing tag
                end_match = re.search(end_pattern, self.data[start:start+0x20000], re.IGNORECASE)
                if end_match:
                    end = start + end_match.end()
                    content = self.data[start:end].decode('utf-8', errors='ignore')
                    files.append((start, end, content))
        
        return files
    
    def find_css_styles(self) -> List[Tuple[int, str]]:
        """Find CSS content"""
        styles = []
        
        # Inline styles in <style> tags
        pattern = rb'<style[^>]*>(.*?)</style>'
        for match in re.finditer(pattern, self.data, re.IGNORECASE | re.DOTALL):
            css = match.group(1).decode('utf-8', errors='ignore')
            if len(css) > 50:  # Ignore tiny snippets
                styles.append((match.start(), css))
        
        # Standalone CSS
        css_pattern = rb'\{[^{}]*\{[^{}]*\}[^{}]*\}'  # Nested braces
        for match in re.finditer(css_pattern, self.data):
            css = match.group(0).decode('utf-8', errors='ignore')
            if 'color' in css or 'font' in css or 'margin' in css:
                styles.append((match.start(), css))
        
        return styles
    
    def find_javascript(self) -> List[Tuple[int, str]]:
        """Find JavaScript content"""
        scripts = []
        
        # <script> tags
        pattern = rb'<script[^>]*>(.*?)</script>'
        for match in re.finditer(pattern, self.data, re.IGNORECASE | re.DOTALL):
            js = match.group(1).decode('utf-8', errors='ignore')
            if len(js) > 50:
                scripts.append((match.start(), js))
        
        # Function definitions
        func_pattern = rb'function\s+(\w+)\s*\([^)]*\)\s*\{[^}]{20,}\}'
        for match in re.finditer(func_pattern, self.data):
            js = match.group(0).decode('utf-8', errors='ignore')
            scripts.append((match.start(), js))
        
        return scripts
    
    def find_json_configs(self) -> List[Tuple[int, str]]:
        """Find JSON configuration data"""
        configs = []
        
        # JSON objects
        pattern = rb'\{["\'][a-zA-Z_]+["\']:[^{}]{10,}\}'
        for match in re.finditer(pattern, self.data):
            try:
                json_str = match.group(0).decode('utf-8')
                if '"' in json_str or "'" in json_str:
                    configs.append((match.start(), json_str))
            except:
                pass
        
        return configs
    
    def find_api_endpoints(self) -> List[str]:
        """Find CGI/API endpoints"""
        endpoints = []
        
        # CGI patterns
        patterns = [
            rb'["\']([^"\']*\.cgi)["\']',
            rb'action=["\']([^"\']+)["\']',
            rb'url:\s*["\']([^"\']+)["\']',
            rb'href=["\']([^"\']*\.cgi)["\']',
        ]
        
        for pattern in patterns:
            for match in re.finditer(pattern, self.data):
                endpoint = match.group(1).decode('utf-8', errors='ignore')
                if endpoint and endpoint not in endpoints:
                    endpoints.append(endpoint)
        
        return endpoints
    
    def extract_to_directory(self, output_dir: str):
        """Extract all web resources to directory"""
        
        os.makedirs(output_dir, exist_ok=True)
        
        print("Extracting web interface from firmware...")
        
        # Extract HTML
        html_files = self.find_html_files()
        print(f"  Found {len(html_files)} HTML documents")
        
        for i, (start, end, content) in enumerate(html_files):
            filename = f'page_{i:02d}_0x{start:06X}.html'
            with open(os.path.join(output_dir, filename), 'w') as f:
                f.write(content)
        
        # Extract CSS
        css_files = self.find_css_styles()
        print(f"  Found {len(css_files)} CSS blocks")
        
        all_css = ""
        for offset, css in css_files[:20]:
            all_css += f"/* Offset: 0x{offset:06X} */\n{css}\n\n"
        
        with open(os.path.join(output_dir, 'styles.css'), 'w') as f:
            f.write(all_css)
        
        # Extract JavaScript
        js_files = self.find_javascript()
        print(f"  Found {len(js_files)} JavaScript blocks")
        
        all_js = ""
        for offset, js in js_files[:30]:
            all_js += f"// Offset: 0x{offset:06X}\n{js}\n\n"
        
        with open(os.path.join(output_dir, 'scripts.js'), 'w') as f:
            f.write(all_js)
        
        # Extract API endpoints
        endpoints = self.find_api_endpoints()
        print(f"  Found {len(endpoints)} API endpoints")
        
        with open(os.path.join(output_dir, 'api_endpoints.txt'), 'w') as f:
            f.write("CGI/API Endpoints found in firmware:\n")
            f.write("=" * 40 + "\n\n")
            for ep in sorted(set(endpoints)):
                f.write(f"{ep}\n")
        
        # Create index
        self.create_index_html(output_dir, html_files, endpoints)
        
        print(f"\n  Extracted to: {output_dir}")
    
    def create_index_html(self, output_dir: str, html_files: List, endpoints: List[str]):
        """Create an index page for extracted resources"""
        
        index = '''<!DOCTYPE html>
<html>
<head>
    <title>Avalon A1126pro Web Interface - Extracted</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; }
        h1 { color: #333; }
        .section { margin: 20px 0; padding: 10px; background: #f5f5f5; }
        a { color: #0066cc; }
        ul { list-style: none; padding: 0; }
        li { margin: 5px 0; }
    </style>
</head>
<body>
    <h1>Avalon A1126pro Firmware - Web Interface</h1>
    
    <div class="section">
        <h2>Firmware Information</h2>
        <ul>
            <li><strong>Model:</strong> Avalon A1126pro</li>
            <li><strong>CGMiner Version:</strong> 4.11.1</li>
            <li><strong>Processor:</strong> Kendryte K210</li>
        </ul>
    </div>
    
    <div class="section">
        <h2>Extracted HTML Pages</h2>
        <ul>
'''
        for i, (start, end, content) in enumerate(html_files):
            filename = f'page_{i:02d}_0x{start:06X}.html'
            title = re.search(r'<title>([^<]+)</title>', content, re.IGNORECASE)
            title_text = title.group(1) if title else f"Page {i}"
            index += f'            <li><a href="{filename}">{title_text}</a> (0x{start:06X})</li>\n'
        
        index += '''        </ul>
    </div>
    
    <div class="section">
        <h2>API Endpoints</h2>
        <ul>
'''
        for ep in sorted(set(endpoints))[:20]:
            index += f'            <li>{ep}</li>\n'
        
        index += '''        </ul>
    </div>
    
    <div class="section">
        <h2>Resources</h2>
        <ul>
            <li><a href="styles.css">Extracted CSS</a></li>
            <li><a href="scripts.js">Extracted JavaScript</a></li>
            <li><a href="api_endpoints.txt">All API Endpoints</a></li>
        </ul>
    </div>
</body>
</html>
'''
        with open(os.path.join(output_dir, 'index.html'), 'w') as f:
            f.write(index)


def main():
    firmware_path = '/home/quaxis/projects/FirmwareASIC/Avalon/A1126pro/donor_dump.bin'
    output_dir = '/home/quaxis/projects/FirmwareASIC/Avalon/A1126pro/analysis/web_interface'
    
    extractor = WebExtractor(firmware_path)
    extractor.extract_to_directory(output_dir)


if __name__ == '__main__':
    main()
