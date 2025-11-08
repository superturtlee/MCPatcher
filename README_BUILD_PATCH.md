# Build Patch Script

This document explains how to use the `build-patch.ps1` script to automatically generate the PATCHED section of `.binpatch.txt` files from `.asm` (assembly) files.

## Overview

The MCPatcher project uses binary patching to modify game executables. Each patch consists of:
- An `.asm` file containing the x64 assembly code for the patch
- A `.binpatch.txt` file containing:
  - `ORIGINAL` section: The original machine code to be replaced
  - `PATCHED` section: The new machine code (generated from the .asm file)

The `build-patch.ps1` script automates the generation of the PATCHED section by:
1. Assembling the `.asm` file using Microsoft MASM (ml64.exe)
2. Extracting the machine code from the resulting object file
3. Formatting it to match the binpatch.txt format
4. Updating the PATCHED section in the corresponding `.binpatch.txt` file

## Requirements

- Windows operating system
- Microsoft Visual Studio with C++ build tools (provides ml64.exe)
  - Visual Studio 2022 (any edition) with "Desktop development with C++" workload
  - Or Visual Studio Build Tools 2022 with C++ tools

## Usage

### Command Line

#### Build all patches
```batch
build-patch.bat -All
```
or
```powershell
.\build-patch.ps1 -All
```

#### Build a specific patch
```batch
build-patch.bat -AsmFile XStoreQueryGameLicenseAsync.asm
```
or
```powershell
.\build-patch.ps1 -AsmFile XStoreQueryGameLicenseAsync.asm
```

#### Verify existing patches (without rebuilding)
```batch
build-patch.bat -Verify -All
```
or
```powershell
.\build-patch.ps1 -Verify -All
```

### Parameters

- `-AsmFile <path>`: Specifies a single .asm file to process
- `-All`: Process all .asm files in the current directory
- `-Verify`: Display existing PATCHED sections without rebuilding

If no parameters are specified, `-All` is used by default.

## How It Works

### Assembly File Format

Assembly files should be in MASM x64 format with the following structure:

```asm
option casemap:none
.code

XStoreQueryGameLicenseAsync proc
    push    rdi                         ; 57
    sub     rsp, 20h                    ; 48 83 EC 20
    ; ... more instructions
    ret                                 ; C3
XStoreQueryGameLicenseAsync endp
end
```

Comments showing the expected machine code are optional but helpful for verification.

### Binpatch File Format

The `.binpatch.txt` files have the following format:

```
ORIGINAL
48 8B C4 57 48 83 EC 30
48 C7 40 E8 FE FF FF FF
...
PATCHED
57 48 83 EC 20 48 89 D7
48 31 C0 48 C7 47 18 00
...
```

- Each line contains up to 8 bytes in hexadecimal format (uppercase)
- Bytes are separated by spaces
- The last line may contain fewer than 8 bytes

### Script Process

1. **Locate ml64.exe**: The script searches for ml64.exe in:
   - System PATH
   - Visual Studio 2022 installations (all editions)
   - Common installation directories

2. **Assemble**: Uses ml64.exe with `/c` flag to assemble the .asm file to .obj

3. **Extract**: Reads the COFF object file and extracts the `.text` section containing machine code

4. **Format**: Converts the binary machine code to hexadecimal string format (8 bytes per line)

5. **Update**: Replaces the PATCHED section in the corresponding .binpatch.txt file

## CI/CD Integration

The script is integrated into the GitHub Actions workflow (`.github/workflows/msbuild.yml`):

```yaml
- name: Build PATCHED sections from ASM files
  shell: cmd
  run: build-patch.bat -All
```

This step runs after the main MSBuild step and before copying files to artifacts, ensuring that all binpatch.txt files are up-to-date with the latest assembly code.

## Troubleshooting

### "ml64.exe not found"

**Solution**: Install Visual Studio 2022 with the "Desktop development with C++" workload, or install the Visual Studio Build Tools 2022.

### "Assembly failed"

**Causes**:
- Syntax errors in the .asm file
- Incorrect MASM directives
- Missing section definitions

**Solution**: Review the error message from ml64.exe and fix the assembly syntax.

### "Failed to extract machine code"

**Causes**:
- The .obj file doesn't contain a .text section
- The object file is corrupted

**Solution**: Verify that the .asm file creates executable code (not just data).

## File Locations

- `build-patch.ps1` - Main PowerShell script
- `build-patch.bat` - Batch file wrapper for easier execution
- `*.asm` - Assembly source files
- `*.binpatch.txt` - Binary patch definition files

## Example

Given these files:
- `XStoreQueryGameLicenseAsync.asm` - Assembly source
- `XStoreQueryGameLicenseAsync.binpatch.txt` - Patch definition (with ORIGINAL section)

Running:
```batch
build-patch.bat -AsmFile XStoreQueryGameLicenseAsync.asm
```

Will:
1. Assemble `XStoreQueryGameLicenseAsync.asm` to create `XStoreQueryGameLicenseAsync.obj`
2. Extract the machine code from the object file
3. Update the PATCHED section in `XStoreQueryGameLicenseAsync.binpatch.txt`

## Notes

- The script creates temporary files in `%TEMP%` during processing
- Temporary files are automatically cleaned up after processing
- The ORIGINAL section in .binpatch.txt files is never modified
- Backup your .binpatch.txt files before first use if needed
