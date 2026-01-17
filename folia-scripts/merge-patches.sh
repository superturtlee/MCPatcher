#!/bin/bash
#
# Folia Feature Patches Merger Script
# 
# This script merges feature patches from folia-server/paper-patches/features/*.patch
# into the folia-server/src/minecraft/java/ git repository and handles conflicts.
#
# Usage: ./merge-patches.sh [options]
# Options:
#   -p, --project-dir DIR    Set project directory (default: current directory)
#   -n, --dry-run            Show what would be done without making changes
#   -c, --continue           Continue after resolving conflicts
#   -a, --abort              Abort the current patch operation
#   -h, --help               Show this help message
#

set -e

# Default values
PROJECT_DIR="."
DRY_RUN=false
CONTINUE_MODE=false
ABORT_MODE=false

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Print colored output
log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Show usage
show_help() {
    cat << EOF
Folia Feature Patches Merger Script

This script merges feature patches from folia-server/paper-patches/features/*.patch
into the folia-server/src/minecraft/java/ git repository and handles conflicts.

Usage: $(basename "$0") [options]

Options:
  -p, --project-dir DIR    Set project directory (default: current directory)
  -n, --dry-run            Show what would be done without making changes
  -c, --continue           Continue after resolving conflicts
  -a, --abort              Abort the current patch operation
  -h, --help               Show this help message

Example:
  $(basename "$0") -p /path/to/project

The script will:
  1. Change git remote to local file path (paperweight cache)
  2. Apply patches from folia-server/paper-patches/features/
  3. Handle merge conflicts interactively

EOF
}

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -p|--project-dir)
            PROJECT_DIR="$2"
            shift 2
            ;;
        -n|--dry-run)
            DRY_RUN=true
            shift
            ;;
        -c|--continue)
            CONTINUE_MODE=true
            shift
            ;;
        -a|--abort)
            ABORT_MODE=true
            shift
            ;;
        -h|--help)
            show_help
            exit 0
            ;;
        *)
            log_error "Unknown option: $1"
            show_help
            exit 1
            ;;
    esac
done

# Resolve absolute path
PROJECT_DIR=$(cd "$PROJECT_DIR" && pwd)
log_info "Project directory: $PROJECT_DIR"

# Define paths
FOLIA_SERVER_DIR="$PROJECT_DIR/folia-server"
JAVA_SRC_DIR="$FOLIA_SERVER_DIR/src/minecraft/java"
PATCHES_DIR="$FOLIA_SERVER_DIR/paper-patches/features"
PAPERWEIGHT_CACHE="$FOLIA_SERVER_DIR/.gradle/caches/paperweight/taskCache/runFoliaSetup"
STATE_FILE="$FOLIA_SERVER_DIR/.patch-merge-state"

# Validate project structure
validate_structure() {
    local errors=0
    
    if [[ ! -d "$FOLIA_SERVER_DIR" ]]; then
        log_error "Directory not found: $FOLIA_SERVER_DIR"
        errors=$((errors + 1))
    fi
    
    if [[ ! -d "$JAVA_SRC_DIR" ]]; then
        log_error "Java source directory not found: $JAVA_SRC_DIR"
        log_info "This directory should be an auto-generated temporary git repository."
        errors=$((errors + 1))
    fi
    
    if [[ ! -d "$PATCHES_DIR" ]]; then
        log_error "Patches directory not found: $PATCHES_DIR"
        errors=$((errors + 1))
    fi
    
    if [[ ! -d "$JAVA_SRC_DIR/.git" ]]; then
        log_error "Not a git repository: $JAVA_SRC_DIR"
        errors=$((errors + 1))
    fi
    
    if [[ $errors -gt 0 ]]; then
        log_error "Please ensure the Folia development environment is properly set up."
        exit 1
    fi
    
    log_success "Project structure validated"
}

# Change git remote to local paperweight cache
setup_remote() {
    local remote_url="file://$PAPERWEIGHT_CACHE"
    
    log_info "Setting up git remote to: $remote_url"
    
    if [[ "$DRY_RUN" == true ]]; then
        log_info "[DRY RUN] Would set remote 'origin' to: $remote_url"
        return
    fi
    
    cd "$JAVA_SRC_DIR"
    
    # Check if origin exists
    if git remote get-url origin &>/dev/null; then
        local current_url
        current_url=$(git remote get-url origin)
        log_info "Current remote URL: $current_url"
        
        if [[ "$current_url" == "$remote_url" ]]; then
            log_info "Remote already set to correct URL"
        else
            git remote set-url origin "$remote_url"
            log_success "Remote URL updated"
        fi
    else
        git remote add origin "$remote_url"
        log_success "Remote 'origin' added"
    fi
}

# Get list of patch files sorted by name into an array
# Usage: get_patch_files_array array_name
get_patch_files_array() {
    local -n result_array=$1
    result_array=()
    while IFS= read -r -d '' file; do
        result_array+=("$file")
    done < <(find "$PATCHES_DIR" -maxdepth 1 -name "*.patch" -type f -print0 2>/dev/null | sort -z)
}

# Save state for continue operation
save_state() {
    local current_index="$1"
    shift
    local -a patches=("$@")
    
    # Write state file
    {
        echo "CURRENT_INDEX=$current_index"
        echo "PATCH_COUNT=${#patches[@]}"
        local i=0
        for patch in "${patches[@]}"; do
            echo "PATCH_$i=$patch"
            i=$((i + 1))
        done
    } > "$STATE_FILE"
}

# Load state for continue operation
# Returns: sets LOADED_INDEX and LOADED_PATCHES array
load_state() {
    if [[ -f "$STATE_FILE" ]]; then
        source "$STATE_FILE"
        LOADED_INDEX="$CURRENT_INDEX"
        LOADED_PATCHES=()
        for ((i=0; i<PATCH_COUNT; i++)); do
            local var_name="PATCH_$i"
            LOADED_PATCHES+=("${!var_name}")
        done
        return 0
    fi
    return 1
}

# Clear state file
clear_state() {
    rm -f "$STATE_FILE"
}

# Apply a single patch
apply_patch() {
    local patch_file="$1"
    local patch_name
    patch_name=$(basename "$patch_file")
    
    log_info "Applying patch: $patch_name"
    
    if [[ "$DRY_RUN" == true ]]; then
        log_info "[DRY RUN] Would apply: $patch_file"
        return 0
    fi
    
    cd "$JAVA_SRC_DIR"
    
    # Try to apply the patch with git am
    if git am --3way "$patch_file" 2>/dev/null; then
        log_success "Successfully applied: $patch_name"
        return 0
    fi
    
    # Patch failed - check if it's a conflict
    if git am --show-current-patch &>/dev/null; then
        log_warn "Conflict detected while applying: $patch_name"
        log_info ""
        log_info "To resolve the conflict:"
        log_info "  1. Fix the conflicts in the affected files"
        log_info "  2. Stage the resolved files: git add <files>"
        log_info "  3. Continue with: git am --continue"
        log_info "  4. Then run this script with --continue"
        log_info ""
        log_info "To abort the patch operation:"
        log_info "  git am --abort"
        log_info "  Or run this script with --abort"
        return 1
    fi
    
    # Check if patch was already applied (empty patch)
    if git am --skip 2>/dev/null; then
        log_warn "Patch appears to be already applied or empty: $patch_name"
        return 0
    fi
    
    log_error "Failed to apply patch: $patch_name"
    return 1
}

# Handle continue mode
handle_continue() {
    if ! load_state; then
        log_error "No patch operation in progress"
        exit 1
    fi
    
    cd "$JAVA_SRC_DIR"
    
    # Check if there's an ongoing am session
    if [[ -d "$JAVA_SRC_DIR/.git/rebase-apply" ]]; then
        log_info "Continuing git am operation..."
        if ! git am --continue; then
            log_error "Failed to continue git am. Please resolve conflicts and try again."
            exit 1
        fi
    fi
    
    # Continue with remaining patches
    apply_remaining_patches "$LOADED_INDEX" "${LOADED_PATCHES[@]}"
}

# Handle abort mode
handle_abort() {
    cd "$JAVA_SRC_DIR"
    
    # Abort git am if in progress
    if [[ -d "$JAVA_SRC_DIR/.git/rebase-apply" ]]; then
        log_info "Aborting git am operation..."
        git am --abort || true
    fi
    
    clear_state
    log_success "Patch operation aborted"
}

# Apply remaining patches after continue
apply_remaining_patches() {
    local start_index="$1"
    shift
    local -a all_patches=("$@")
    
    for ((i=start_index+1; i<${#all_patches[@]}; i++)); do
        local patch="${all_patches[$i]}"
        if ! apply_patch "$patch"; then
            save_state "$i" "${all_patches[@]}"
            exit 1
        fi
    done
    
    clear_state
    log_success "All patches applied successfully!"
}

# Main patch application loop
apply_all_patches() {
    local -a patch_files
    get_patch_files_array patch_files
    
    if [[ ${#patch_files[@]} -eq 0 ]]; then
        log_warn "No patch files found in: $PATCHES_DIR"
        exit 0
    fi
    
    local patch_count=${#patch_files[@]}
    log_info "Found $patch_count patch file(s) to apply"
    
    if [[ "$DRY_RUN" == true ]]; then
        log_info "[DRY RUN] Patches to apply:"
        for patch in "${patch_files[@]}"; do
            log_info "  - $(basename "$patch")"
        done
        exit 0
    fi
    
    local i=0
    for patch in "${patch_files[@]}"; do
        if ! apply_patch "$patch"; then
            save_state "$i" "${patch_files[@]}"
            log_error "Patch operation paused. Fix conflicts and use --continue to resume."
            exit 1
        fi
        i=$((i + 1))
    done
    
    clear_state
    log_success "All patches applied successfully!"
}

# Main execution
main() {
    # Handle special modes
    if [[ "$CONTINUE_MODE" == true ]]; then
        handle_continue
        exit 0
    fi
    
    if [[ "$ABORT_MODE" == true ]]; then
        handle_abort
        exit 0
    fi
    
    # Normal operation
    log_info "=== Folia Feature Patches Merger ==="
    log_info ""
    
    # Validate project structure
    validate_structure
    
    # Set up remote
    setup_remote
    
    # Apply patches
    apply_all_patches
    
    log_info ""
    log_success "=== Patch merge completed ==="
}

main "$@"
