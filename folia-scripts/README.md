# Folia Feature Patches Merger

This script helps merge feature patches from `folia-server/paper-patches/features/*.patch` into the Folia server source code repository at `folia-server/src/minecraft/java/`.

## Background

When working with Folia (a Paper fork for Minecraft servers), the build system uses Paperweight to manage patches. The `src/minecraft/java/` directory is an auto-generated temporary git repository that needs to have feature patches applied to it.

## Prerequisites

Before using this script, ensure you have:

1. A properly set up Folia development environment
2. The following directory structure:
   ```
   project-root/
   └── folia-server/
       ├── .gradle/
       │   └── caches/
       │       └── paperweight/
       │           └── taskCache/
       │               └── runFoliaSetup/   # Paperweight cache (git repository)
       ├── src/
       │   └── minecraft/
       │       └── java/                    # Auto-generated temporary git repository
       └── paper-patches/
           └── features/
               └── *.patch                  # Feature patches to apply
   ```

## Usage

### Basic Usage

```bash
# Navigate to project directory and run the script
./folia-scripts/merge-patches.sh -p /path/to/project

# Or use current directory
cd /path/to/project
./folia-scripts/merge-patches.sh
```

### Dry Run

Preview what patches would be applied without making changes:

```bash
./folia-scripts/merge-patches.sh --dry-run
```

### Handling Conflicts

If a patch fails to apply due to conflicts:

1. The script will pause and show instructions
2. Fix the conflicts in the affected files
3. Stage resolved files: `git add <files>`
4. Continue git am: `git am --continue`
5. Resume the script: `./merge-patches.sh --continue`

To abort a failed patch operation:

```bash
./folia-scripts/merge-patches.sh --abort
```

### Command Line Options

| Option | Description |
|--------|-------------|
| `-p, --project-dir DIR` | Set project directory (default: current directory) |
| `-n, --dry-run` | Show what would be done without making changes |
| `-c, --continue` | Continue after resolving conflicts |
| `-a, --abort` | Abort the current patch operation |
| `-h, --help` | Show help message |

## What the Script Does

1. **Validates** the project structure to ensure all required directories exist
2. **Changes git remote** to the local Paperweight cache at:
   `file://$projectdir/folia-server/.gradle/caches/paperweight/taskCache/runFoliaSetup/`
3. **Applies patches** from `folia-server/paper-patches/features/*.patch` using `git am --3way`
4. **Handles conflicts** by saving state and providing instructions for manual resolution

## Troubleshooting

### "Directory not found" Error

Ensure you're running the script from the correct project directory and that you've run the Folia setup tasks first (e.g., `./gradlew applyPatches`).

### "Not a git repository" Error

The `src/minecraft/java/` directory must be initialized as a git repository. This is typically done automatically by Paperweight during setup.

### Patch Conflicts

When conflicts occur:
1. Open the conflicting files in your editor
2. Look for conflict markers (`<<<<<<<`, `=======`, `>>>>>>>`)
3. Resolve the conflicts by choosing the correct code
4. Remove the conflict markers
5. Stage the files and continue

## Related

- [Paper](https://github.com/PaperMC/Paper) - The Minecraft server software
- [Folia](https://github.com/PaperMC/Folia) - Paper fork with parallel region processing
- [Paperweight](https://github.com/PaperMC/paperweight) - Gradle plugin for Paper development
