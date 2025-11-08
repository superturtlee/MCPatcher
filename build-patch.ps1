# PowerShell script to build PATCHED section of binpatch.txt from .asm files
# This script uses Microsoft MASM (ml64.exe) to assemble x64 assembly files
# and extracts the machine code to update the PATCHED section in binpatch.txt files

param(
    [Parameter(Mandatory=$false)]
    [string]$AsmFile = "",
    [Parameter(Mandatory=$false)]
    [switch]$All = $false,
    [Parameter(Mandatory=$false)]
    [switch]$Verify = $false
)

# Function to find ml64.exe
function Find-ML64 {
    # Try to find ml64.exe in PATH first
    $ml64 = Get-Command ml64.exe -ErrorAction SilentlyContinue
    if ($ml64) {
        return $ml64.Source
    }
    
    # Try to find it in Visual Studio installations
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        if ($vsPath) {
            $ml64Path = Get-ChildItem -Path "$vsPath" -Filter ml64.exe -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1
            if ($ml64Path) {
                return $ml64Path.FullName
            }
        }
    }
    
    # Try common installation paths
    $commonPaths = @(
        "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\*\bin\Hostx64\x64\ml64.exe",
        "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\*\bin\Hostx64\x64\ml64.exe",
        "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\Professional\VC\Tools\MSVC\*\bin\Hostx64\x64\ml64.exe",
        "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\Enterprise\VC\Tools\MSVC\*\bin\Hostx64\x64\ml64.exe",
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\*\bin\Hostx64\x64\ml64.exe",
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\*\bin\Hostx64\x64\ml64.exe",
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\Professional\VC\Tools\MSVC\*\bin\Hostx64\x64\ml64.exe",
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\Enterprise\VC\Tools\MSVC\*\bin\Hostx64\x64\ml64.exe"
    )
    
    foreach ($path in $commonPaths) {
        $found = Get-Item $path -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($found) {
            return $found.FullName
        }
    }
    
    throw "ml64.exe not found. Please ensure Visual Studio with C++ tools is installed."
}

# Function to assemble ASM file and extract machine code
function Build-PatchFromAsm {
    param(
        [Parameter(Mandatory=$true)]
        [string]$AsmFilePath,
        [Parameter(Mandatory=$true)]
        [string]$ML64Path,
        [Parameter(Mandatory=$true)]
        [string]$ScriptRoot
    )
    
    # Validate parameters
    if ([string]::IsNullOrWhiteSpace($AsmFilePath)) {
        throw "AsmFilePath is empty"
    }
    if ([string]::IsNullOrWhiteSpace($ML64Path)) {
        throw "ML64Path is empty"
    }
    if ([string]::IsNullOrWhiteSpace($ScriptRoot)) {
        throw "ScriptRoot is empty"
    }
    
    $asmFile = Get-Item $AsmFilePath
    $baseName = $asmFile.BaseName
    $binpatchFile = Join-Path $asmFile.DirectoryName "$baseName.binpatch.txt"
    
    Write-Host "Processing: $($asmFile.Name)" -ForegroundColor Cyan
    
    # Create temporary directory in script root for build artifacts
    $randomSuffix = Get-Random -Minimum 10000 -Maximum 99999
    $tempDirName = "_build_temp_$randomSuffix"
    $tempDir = Join-Path $ScriptRoot $tempDirName
    
    Write-Host "  Creating temp directory: $tempDir" -ForegroundColor Gray
    
    if ([string]::IsNullOrWhiteSpace($tempDir)) {
        throw "Failed to create temp directory path"
    }
    
    New-Item -ItemType Directory -Path $tempDir -Force | Out-Null
    
    try {
        # Copy ASM file to temp directory
        $tempAsmFile = Join-Path $tempDir "$baseName.asm"
        Copy-Item $asmFile.FullName $tempAsmFile
        
        # Assemble the file
        $objFile = Join-Path $tempDir "$baseName.obj"
        $errorFile = Join-Path $tempDir "error.txt"
        Write-Host "  Assembling with ml64.exe..." -ForegroundColor Gray
        
        $assembleArgs = @("/c", "/Fo$objFile", $tempAsmFile)
        $process = Start-Process -FilePath $ML64Path -ArgumentList $assembleArgs -NoNewWindow -Wait -PassThru -RedirectStandardError $errorFile
        
        if ($process.ExitCode -ne 0) {
            $errorContent = Get-Content $errorFile -Raw -ErrorAction SilentlyContinue
            Write-Host "  ERROR: Assembly failed!" -ForegroundColor Red
            if ($errorContent) {
                Write-Host $errorContent -ForegroundColor Red
            }
            return $false
        }
        
        if (-not (Test-Path $objFile)) {
            Write-Host "  ERROR: Object file not created!" -ForegroundColor Red
            return $false
        }
        
        # Extract machine code from .obj file
        Write-Host "  Extracting machine code..." -ForegroundColor Gray
        $machineCode = Extract-MachineCode -ObjFilePath $objFile
        
        if (-not $machineCode) {
            Write-Host "  ERROR: Failed to extract machine code!" -ForegroundColor Red
            return $false
        }
        
        # Format machine code for binpatch.txt
        $formattedCode = Format-MachineCode -MachineCode $machineCode
        
        # Update or create binpatch.txt file
        if (Test-Path $binpatchFile) {
            Update-BinpatchFile -BinpatchFilePath $binpatchFile -PatchedSection $formattedCode
            Write-Host "  Updated: $baseName.binpatch.txt" -ForegroundColor Green
        } else {
            Write-Host "  WARNING: $baseName.binpatch.txt not found, skipping update" -ForegroundColor Yellow
            return $false
        }
        
        return $true
    }
    finally {
        # Clean up temp directory
        if (Test-Path $tempDir) {
            Write-Host "  Cleaning up temp directory..." -ForegroundColor Gray
            Remove-Item -Path $tempDir -Recurse -Force -ErrorAction SilentlyContinue
        }
    }
}

# Function to extract machine code from OBJ file
function Extract-MachineCode {
    param([string]$ObjFilePath)
    
    # Read the OBJ file as bytes
    $objBytes = [System.IO.File]::ReadAllBytes($ObjFilePath)
    
    # COFF format: Find the .text section
    # COFF header is at offset 0
    # Section headers start after COFF header and optional header
    
    # Read COFF header
    $machine = [BitConverter]::ToUInt16($objBytes, 0)
    $numberOfSections = [BitConverter]::ToUInt16($objBytes, 2)
    $sizeOfOptionalHeader = [BitConverter]::ToUInt16($objBytes, 16)
    
    # Section headers start after COFF file header (20 bytes) + optional header
    $sectionHeaderOffset = 20 + $sizeOfOptionalHeader
    
    # Each section header is 40 bytes
    for ($i = 0; $i -lt $numberOfSections; $i++) {
        $offset = $sectionHeaderOffset + ($i * 40)
        
        # Read section name (first 8 bytes)
        $nameBytes = $objBytes[$offset..($offset + 7)]
        $sectionName = [System.Text.Encoding]::ASCII.GetString($nameBytes).TrimEnd([char]0)
        
        if ($sectionName -eq ".text" -or $sectionName.StartsWith(".text")) {
            # Found .text section
            $sizeOfRawData = [BitConverter]::ToUInt32($objBytes, $offset + 16)
            $pointerToRawData = [BitConverter]::ToUInt32($objBytes, $offset + 20)
            
            if ($sizeOfRawData -gt 0 -and $pointerToRawData -gt 0) {
                # Extract the machine code
                $code = $objBytes[$pointerToRawData..($pointerToRawData + $sizeOfRawData - 1)]
                return $code
            }
        }
    }
    
    return $null
}

# Function to format machine code for binpatch.txt
function Format-MachineCode {
    param([byte[]]$MachineCode)
    
    $lines = @()
    $bytesPerLine = 8
    
    for ($i = 0; $i -lt $MachineCode.Length; $i += $bytesPerLine) {
        $lineBytes = $MachineCode[$i..[Math]::Min($i + $bytesPerLine - 1, $MachineCode.Length - 1)]
        $hexBytes = $lineBytes | ForEach-Object { $_.ToString("X2") }
        $lines += ($hexBytes -join " ")
    }
    
    return $lines -join "`n"
}

# Function to update the PATCHED section in binpatch.txt
function Update-BinpatchFile {
    param(
        [string]$BinpatchFilePath,
        [string]$PatchedSection
    )
    
    # Read the existing file
    $content = Get-Content $BinpatchFilePath -Raw
    
    # Check if there's already a PATCHED section
    if ($content -match "(?s)(.*ORIGINAL.*?)(?:PATCHED.*)?$") {
        # Replace or add PATCHED section
        if ($content -match "PATCHED") {
            # Replace existing PATCHED section
            $content = $content -replace "(?s)(PATCHED).*$", "PATCHED`n$PatchedSection"
        } else {
            # Add PATCHED section
            $content = $content.TrimEnd() + "`nPATCHED`n$PatchedSection"
        }
    } else {
        Write-Host "  WARNING: ORIGINAL section not found in $BinpatchFilePath" -ForegroundColor Yellow
        return
    }
    
    # Ensure the file ends with a newline
    if (-not $content.EndsWith("`n")) {
        $content += "`n"
    }
    
    # Write back to file
    [System.IO.File]::WriteAllText($BinpatchFilePath, $content)
}

# Function to verify the PATCHED section matches the ASM file
function Verify-Patch {
    param([string]$AsmFilePath)
    
    $asmFile = Get-Item $AsmFilePath
    $baseName = $asmFile.BaseName
    $binpatchFile = Join-Path $asmFile.DirectoryName "$baseName.binpatch.txt"
    
    if (-not (Test-Path $binpatchFile)) {
        Write-Host "  WARNING: $baseName.binpatch.txt not found" -ForegroundColor Yellow
        return $false
    }
    
    # Read existing PATCHED section
    $content = Get-Content $binpatchFile -Raw
    if ($content -match "(?s)PATCHED\s+([\s\S]+)$") {
        $existingPatched = $matches[1].Trim()
        Write-Host "  Existing PATCHED section:" -ForegroundColor Gray
        Write-Host "  $($existingPatched -replace "`n", "`n  ")" -ForegroundColor Gray
        return $true
    } else {
        Write-Host "  WARNING: No PATCHED section found" -ForegroundColor Yellow
        return $false
    }
}

# Main script logic
try {
    Write-Host "`n=== MCPatcher - Build PATCHED Section from ASM ===" -ForegroundColor Cyan
    Write-Host ""
    
    # Get script root directory
    if ($PSScriptRoot) {
        $scriptRoot = $PSScriptRoot
    } else {
        $scriptRoot = (Get-Location).Path
    }
    
    # Validate script root
    if ([string]::IsNullOrWhiteSpace($scriptRoot)) {
        throw "Failed to determine script root directory"
    }
    
    Write-Host "Script root: $scriptRoot" -ForegroundColor Gray
    Write-Host ""
    
    # Find ml64.exe
    Write-Host "Locating ml64.exe..." -ForegroundColor Cyan
    $ml64Path = Find-ML64
    Write-Host "Found: $ml64Path" -ForegroundColor Green
    Write-Host ""
    
    # Get list of ASM files to process
    $asmFiles = @()
    
    if ($All) {
        # Process all .asm files in the current directory
        $asmFiles = Get-ChildItem -Path $scriptRoot -Filter "*.asm" -File | Where-Object { -not $_.Name.StartsWith("_") }
    } elseif ($AsmFile) {
        # Process specified file
        if (Test-Path $AsmFile) {
            $asmFiles = @(Get-Item $AsmFile)
        } else {
            Write-Host "ERROR: File not found: $AsmFile" -ForegroundColor Red
            exit 1
        }
    } else {
        # Default: process all .asm files in script root directory
        $asmFiles = Get-ChildItem -Path $scriptRoot -Filter "*.asm" -File | Where-Object { -not $_.Name.StartsWith("_") }
    }
    
    if ($asmFiles.Count -eq 0) {
        Write-Host "No ASM files found to process." -ForegroundColor Yellow
        exit 0
    }
    
    Write-Host "Found $($asmFiles.Count) ASM file(s) to process:" -ForegroundColor Cyan
    foreach ($file in $asmFiles) {
        Write-Host "  - $($file.Name) [$($file.FullName)]" -ForegroundColor Gray
    }
    Write-Host ""
    
    # Process each file
    $successCount = 0
    $failCount = 0
    
    foreach ($asmFileItem in $asmFiles) {
        # Ensure we have a valid file path
        $asmFilePath = $asmFileItem.FullName
        
        if ([string]::IsNullOrWhiteSpace($asmFilePath)) {
            Write-Host "ERROR: Empty file path encountered for $($asmFileItem.Name)" -ForegroundColor Red
            $failCount++
            continue
        }
        
        Write-Host "Processing file: $asmFilePath" -ForegroundColor Gray
        
        if ($Verify) {
            Verify-Patch -AsmFilePath $asmFilePath
        } else {
            if (Build-PatchFromAsm -AsmFilePath $asmFilePath -ML64Path $ml64Path -ScriptRoot $scriptRoot) {
                $successCount++
            } else {
                $failCount++
            }
        }
        Write-Host ""
    }
    
    # Summary
    if (-not $Verify) {
        Write-Host "=== Summary ===" -ForegroundColor Cyan
        Write-Host "Successfully processed: $successCount" -ForegroundColor Green
        if ($failCount -gt 0) {
            Write-Host "Failed: $failCount" -ForegroundColor Red
            exit 1
        }
    }
    
    Write-Host ""
    Write-Host "Done!" -ForegroundColor Green
    
} catch {
    Write-Host ""
    Write-Host "ERROR: $($_.Exception.Message)" -ForegroundColor Red
    Write-Host $_.ScriptStackTrace -ForegroundColor Red
    exit 1
}