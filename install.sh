#!/usr/bin/env bash
# Safe-C Interactive Toolchain Installer & Uninstaller (Linux / POSIX / MSYS2 / Git Bash)
# Finds GCC/G++ on the system, asks for confirmation or manual path entry,
# installs Safe-C plugin/specs/runtime files safely, and supports clean uninstall
# without disturbing other parts of the system.

set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"

ACTION=""
ARG_GCC=""
ARG_GPP=""
NO_RUNTIME=0
ASSUME_YES=0
FROM_WEB=0
RELEASE_TAG=""
REPO_URL="https://github.com/Kush-Kelaiya22/Safe-C"
WEB_TMP_DIR=""

cleanup_web() {
    if [[ -n "${WEB_TMP_DIR:-}" && -d "$WEB_TMP_DIR" ]]; then
        rm -rf "$WEB_TMP_DIR"
    fi
}
trap cleanup_web EXIT

usage() {
    cat <<'EOF'
Safe-C Toolchain Installer & Uninstaller

Usage:
  ./install.sh [install | uninstall] [OPTIONS]

Actions:
  install, --install       Install or update Safe-C into GCC/G++ (interactive menu default)
  uninstall, --uninstall   Uninstall Safe-C from GCC/G++ without disturbing other files

Options:
  --gcc <path>             Pre-select path to gcc executable or toolchain bin directory
  --g++ <path>, --gpp <path>
                           Pre-select path to g++ executable (defaults to sibling of gcc)
  --from-web               Download prebuilt plugin, specs, and runtime files from GitHub Releases
  --release <tag>          Release tag to download from GitHub (implies --from-web; default: latest)
  --no-runtime             Install only the compiler plugin and specs (skip libsafe_c_rt/headers)
  -y, --yes                Accept confirmations non-interactively
  -h, --help               Show this help message

Examples:
  ./install.sh
  ./install.sh --from-web
  ./install.sh install --from-web --release v1.2.3 --gcc /usr/bin/gcc
  ./install.sh uninstall
  ./install.sh --uninstall --gcc /opt/mingw64/bin/gcc.exe --yes
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        install|--install)
            ACTION="install"
            shift
            ;;
        uninstall|--uninstall)
            ACTION="uninstall"
            shift
            ;;
        --gcc)
            [[ $# -lt 2 ]] && { echo "Error: --gcc requires a path argument." >&2; exit 1; }
            ARG_GCC="$2"
            shift 2
            ;;
        --g++|--gpp)
            [[ $# -lt 2 ]] && { echo "Error: $1 requires a path argument." >&2; exit 1; }
            ARG_GPP="$2"
            shift 2
            ;;
        --from-web)
            FROM_WEB=1
            shift
            ;;
        --release|--version)
            [[ $# -lt 2 ]] && { echo "Error: $1 requires a release tag argument." >&2; exit 1; }
            RELEASE_TAG="$2"
            FROM_WEB=1
            shift 2
            ;;
        --no-runtime)
            NO_RUNTIME=1
            shift
            ;;
        -y|--yes)
            ASSUME_YES=1
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "Unknown argument: $1" >&2
            usage >&2
            exit 1
            ;;
    esac
done

UNAME_S="$(uname -s 2>/dev/null || echo Linux)"
case "$UNAME_S" in
    MINGW*|MSYS*|CYGWIN*)
        IS_WINDOWS=1
        PLUGIN_EXT=".dll"
        ;;
    *)
        IS_WINDOWS=0
        PLUGIN_EXT=".so"
        ;;
esac

to_shell_path() {
    local p="$1"
    # Trim trailing CR and trailing slashes (except root /)
    p="${p//$'\r'/}"
    if [[ "$IS_WINDOWS" -eq 1 ]] && command -v cygpath >/dev/null 2>&1; then
        p="$(cygpath -u "$p" 2>/dev/null || printf '%s' "$p")"
    fi
    while [[ ${#p} -gt 1 && "$p" == */ ]]; do
        p="${p%/}"
    done
    printf '%s' "$p"
}

to_native_path() {
    local p="$1"
    if [[ "$IS_WINDOWS" -eq 1 ]] && command -v cygpath >/dev/null 2>&1; then
        cygpath -w "$p" 2>/dev/null || printf '%s' "$p"
    else
        printf '%s' "$p"
    fi
}

canon_path() {
    local p="$1"
    if command -v readlink >/dev/null 2>&1; then
        readlink -f -- "$p" 2>/dev/null && return 0
    fi
    if command -v realpath >/dev/null 2>&1; then
        realpath -- "$p" 2>/dev/null && return 0
    fi
    printf '%s\n' "$p"
}

prompt_yn() {
    local msg="$1"
    local default_ans="${2:-Y}"
    if [[ "$ASSUME_YES" -eq 1 ]]; then
        return 0
    fi
    local reply=""
    read -r -p "$msg " reply
    reply="${reply:-$default_ans}"
    case "$reply" in
        [Yy]|[Yy][Ee][Ss]) return 0 ;;
        *) return 1 ;;
    esac
}

run_priv_dir() {
    local target_dir="$1"
    shift
    local check_dir="$target_dir"
    while [[ ! -e "$check_dir" && "$check_dir" != "/" && "$check_dir" != "." ]]; do
        check_dir="$(dirname -- "$check_dir")"
    done
    if [[ "${EUID:-$(id -u)}" -eq 0 ]] || [[ -w "$check_dir" ]] || [[ "$IS_WINDOWS" -eq 1 ]]; then
        "$@"
    elif command -v sudo >/dev/null 2>&1; then
        echo "[sudo] Elevating permissions to modify $target_dir ..."
        sudo "$@"
    else
        echo "Error: No write permission for $target_dir and sudo is not available." >&2
        return 1
    fi
}

is_real_gcc() {
    local cmd="$1"
    [[ -n "$cmd" && -x "$cmd" ]] || return 1
    "$cmd" -dumpspecs >/dev/null 2>&1
}

resolve_sibling_compiler() {
    local base_cmd="$1"
    local wanted="$2" # gcc or g++
    local dir base candidate

    dir="$(dirname -- "$base_cmd")"
    base="$(basename -- "$base_cmd")"

    if [[ "$wanted" == "g++" ]]; then
        case "$base" in
            *gcc.exe) candidate="$dir/${base%gcc.exe}g++.exe" ;;
            *gcc-*)   candidate="$dir/${base/gcc-/g++-}" ;;
            *gcc)     candidate="$dir/${base%gcc}g++" ;;
            *)        candidate="$dir/g++" ;;
        esac
        [[ "$IS_WINDOWS" -eq 1 && ! -x "$candidate" && -x "${candidate}.exe" ]] && candidate="${candidate}.exe"
    else
        case "$base" in
            *g++.exe) candidate="$dir/${base%g++.exe}gcc.exe" ;;
            *g++-*)   candidate="$dir/${base/g++-/gcc-}" ;;
            *g++)     candidate="$dir/${base%g++}gcc" ;;
            *)        candidate="$dir/gcc" ;;
        esac
        [[ "$IS_WINDOWS" -eq 1 && ! -x "$candidate" && -x "${candidate}.exe" ]] && candidate="${candidate}.exe"
    fi

    if is_real_gcc "$candidate"; then
        printf '%s' "$candidate"
        return 0
    fi
    printf ''
    return 1
}

resolve_compiler_input() {
    local raw="$1"
    local mode="${2:-gcc}" # preferred kind when directory given
    raw="${raw#\"}"
    raw="${raw%\"}"
    raw="${raw#\'}"
    raw="${raw%\'}"
    raw="$(to_shell_path "$raw")"

    if [[ -d "$raw" ]]; then
        local c
        for c in "$raw/$mode" "$raw/${mode}.exe" "$raw/gcc" "$raw/gcc.exe" "$raw/g++" "$raw/g++.exe"; do
            if is_real_gcc "$c"; then
                printf '%s' "$c"
                return 0
            fi
        done
        return 1
    fi

    if [[ -x "$raw" ]] && is_real_gcc "$raw"; then
        printf '%s' "$raw"
        return 0
    fi

    if command -v "$raw" >/dev/null 2>&1; then
        local resolved
        resolved="$(command -v "$raw")"
        if is_real_gcc "$resolved"; then
            printf '%s' "$resolved"
            return 0
        fi
    fi

    return 1
}

query_gcc_info() {
    local cmd="$1"
    local search_dirs ver_out raw_install raw_plugin

    search_dirs="$("$cmd" -print-search-dirs 2>/dev/null || true)"
    ver_out="$("$cmd" -v 2>&1 || true)"

    Q_VERSION="$("$cmd" --version 2>/dev/null | head -n 1 | tr -d '\r')"

    raw_install="$(printf '%s\n' "$search_dirs" | awk -F': ' '/^install:/{print $2; exit}')"
    if [[ -z "$raw_install" ]]; then
        local libgcc_file
        libgcc_file="$("$cmd" -print-libgcc-file-name 2>/dev/null || true)"
        [[ -n "$libgcc_file" ]] && raw_install="$(dirname -- "$libgcc_file")"
    fi
    Q_INSTALL_DIR="$(to_shell_path "$raw_install")"

    raw_plugin="$("$cmd" -print-file-name=plugin 2>/dev/null || true)"
    Q_PLUGIN_DIR=""
    if [[ -n "$raw_plugin" && "$raw_plugin" != "plugin" ]]; then
        Q_PLUGIN_DIR="$(to_shell_path "$raw_plugin")"
    fi

    if printf '%s' "$ver_out" | grep -q -- '--enable-plugin'; then
        Q_HAS_PLUGIN=1
    else
        Q_HAS_PLUGIN=0
    fi
}

discover_compilers() {
    FOUND_COMPILERS=()
    local -A seen=()
    local names=(gcc gcc-16 gcc-15 gcc-14 gcc-13 gcc-12)
    local extra_paths=(
        /usr/bin/gcc
        /usr/local/bin/gcc
    )
    if [[ "$IS_WINDOWS" -eq 1 ]]; then
        extra_paths+=(
            /ucrt64/bin/gcc.exe
            /mingw64/bin/gcc.exe
            /c/msys64/ucrt64/bin/gcc.exe
            /c/msys64/mingw64/bin/gcc.exe
            /c/mingw64/bin/gcc.exe
            /c/MinGW/bin/gcc.exe
            /c/TDM-GCC-64/bin/gcc.exe
        )
    fi

    local p resolved canon
    for p in "${names[@]}"; do
        if command -v "$p" >/dev/null 2>&1; then
            resolved="$(command -v "$p")"
            if is_real_gcc "$resolved"; then
                canon="$(canon_path "$resolved")"
                if [[ -z "${seen[$canon]+x}" ]]; then
                    seen["$canon"]=1
                    FOUND_COMPILERS+=("$resolved")
                fi
            fi
        fi
    done
    for p in "${extra_paths[@]}"; do
        if is_real_gcc "$p"; then
            canon="$(canon_path "$p")"
            if [[ -z "${seen[$canon]+x}" ]]; then
                seen["$canon"]=1
                FOUND_COMPILERS+=("$p")
            fi
        fi
    done
}

print_compiler_summary() {
    local gcc_bin="$1"
    local gpp_bin="$2"
    query_gcc_info "$gcc_bin"
    echo ""
    echo "Selected GCC/G++ Toolchain:"
    echo "  gcc           : $(to_native_path "$gcc_bin")  [$Q_VERSION]"
    if [[ -n "$gpp_bin" ]]; then
        local gpp_ver
        gpp_ver="$("$gpp_bin" --version 2>/dev/null | head -n 1 | tr -d '\r')"
        echo "  g++           : $(to_native_path "$gpp_bin")  [$gpp_ver]"
    else
        echo "  g++           : (not paired / using $gcc_bin)"
    fi
    echo "  Install dir   : $(to_native_path "$Q_INSTALL_DIR")"
    if [[ "$Q_HAS_PLUGIN" -eq 1 && -n "$Q_PLUGIN_DIR" ]]; then
        echo "  Plugin dir    : $(to_native_path "$Q_PLUGIN_DIR")  (--enable-plugin supported)"
    else
        echo "  Plugin support: disabled (--enable-plugin absent; MinGW addon mode on Windows)"
    fi
}

prompt_manual_compiler() {
    while true; do
        echo ""
        local in_gcc="" in_gpp="" resolved_main="" resolved_gcc="" resolved_gpp=""
        read -r -p "Enter path to gcc or g++ executable (or bin dir, 'q' to cancel): " in_gcc
        [[ "$in_gcc" == "q" || "$in_gcc" == "Q" ]] && return 1
        if [[ -z "$in_gcc" ]]; then
            echo "Path cannot be empty."
            continue
        fi
        if ! resolved_main="$(resolve_compiler_input "$in_gcc" "gcc")"; then
            echo "Error: '$in_gcc' is not a valid GCC/G++ executable or directory containing one."
            continue
        fi

        local base
        base="$(basename -- "$resolved_main")"
        if [[ "$base" == *g++* ]]; then
            resolved_gpp="$resolved_main"
            resolved_gcc="$(resolve_sibling_compiler "$resolved_main" "gcc" || true)"
            if [[ -z "$resolved_gcc" ]]; then
                resolved_gcc="$resolved_main"
            fi
        else
            resolved_gcc="$resolved_main"
            resolved_gpp="$(resolve_sibling_compiler "$resolved_main" "g++" || true)"
        fi

        if [[ -n "$resolved_gpp" ]]; then
            read -r -p "Optional g++ path [Enter to keep '$(to_native_path "$resolved_gpp")']: " in_gpp
        else
            read -r -p "Optional g++ path [Enter to skip or enter path]: " in_gpp
        fi
        if [[ -n "$in_gpp" ]]; then
            if ! resolved_gpp="$(resolve_compiler_input "$in_gpp" "g++")"; then
                echo "Warning: '$in_gpp' is not a valid g++ executable; keeping previous value."
            fi
        fi

        print_compiler_summary "$resolved_gcc" "$resolved_gpp"
        echo ""
        if prompt_yn "Confirm using this compiler path? [Y/n]:" "Y"; then
            SELECTED_GCC="$resolved_gcc"
            SELECTED_GPP="$resolved_gpp"
            return 0
        fi
    done
}

select_compiler_interactive() {
    SELECTED_GCC=""
    SELECTED_GPP=""

    if [[ -n "$ARG_GCC" ]]; then
        if ! SELECTED_GCC="$(resolve_compiler_input "$ARG_GCC" "gcc")"; then
            echo "Error: Specified --gcc path '$ARG_GCC' is not a valid GCC executable." >&2
            return 1
        fi
        if [[ -n "$ARG_GPP" ]]; then
            if ! SELECTED_GPP="$(resolve_compiler_input "$ARG_GPP" "g++")"; then
                echo "Error: Specified --g++ path '$ARG_GPP' is not a valid G++ executable." >&2
                return 1
            fi
        else
            SELECTED_GPP="$(resolve_sibling_compiler "$SELECTED_GCC" "g++" || true)"
        fi
        print_compiler_summary "$SELECTED_GCC" "$SELECTED_GPP"
        if [[ "$ASSUME_YES" -eq 1 ]]; then
            return 0
        fi
        echo ""
        local reply=""
        read -r -p "Is this compiler path correct? [Y/n/m(anual)]: " reply
        reply="${reply:-Y}"
        case "$reply" in
            [Yy]|[Yy][Ee][Ss]) return 0 ;;
            [Nn]|[Mm]|[Mm][Aa][Nn][Uu][Aa][Ll]) prompt_manual_compiler; return $? ;;
            *) return 1 ;;
        esac
    fi

    discover_compilers

    if [[ ${#FOUND_COMPILERS[@]} -eq 0 ]]; then
        echo "No gcc installation found automatically on PATH or standard directories."
        if [[ "$ASSUME_YES" -eq 1 ]]; then
            echo "Error: Pass --gcc <path> when using --yes without gcc on PATH." >&2
            return 1
        fi
        prompt_manual_compiler
        return $?
    fi

    if [[ ${#FOUND_COMPILERS[@]} -eq 1 || "$ASSUME_YES" -eq 1 ]]; then
        local cand="${FOUND_COMPILERS[0]}"
        local cand_gpp=""
        if [[ -n "$ARG_GPP" ]]; then
            cand_gpp="$(resolve_compiler_input "$ARG_GPP" "g++" || true)"
        else
            cand_gpp="$(resolve_sibling_compiler "$cand" "g++" || true)"
        fi
        print_compiler_summary "$cand" "$cand_gpp"
        if [[ "$ASSUME_YES" -eq 1 ]]; then
            SELECTED_GCC="$cand"
            SELECTED_GPP="$cand_gpp"
            return 0
        fi
        echo ""
        local reply=""
        read -r -p "Is this gcc/g++ path correct? [Y/n/m(anual)]: " reply
        reply="${reply:-Y}"
        case "$reply" in
            [Yy]|[Yy][Ee][Ss])
                SELECTED_GCC="$cand"
                SELECTED_GPP="$cand_gpp"
                return 0
                ;;
            [Nn]|[Mm]|[Mm][Aa][Nn][Uu][Aa][Ll])
                prompt_manual_compiler
                return $?
                ;;
            *)
                return 1
                ;;
        esac
    fi

    echo ""
    echo "Detected GCC installations on this system:"
    local idx=1
    local c ver
    for c in "${FOUND_COMPILERS[@]}"; do
        ver="$("$c" --version 2>/dev/null | head -n 1 | tr -d '\r')"
        echo "  $idx) $(to_native_path "$c")  [$ver]"
        idx=$((idx + 1))
    done
    echo "  m) Enter gcc/g++ path manually"
    echo "  q) Cancel"
    echo ""

    local choice=""
    read -r -p "Select compiler [1-${#FOUND_COMPILERS[@]}/m/q] (default 1): " choice
    choice="${choice:-1}"

    case "$choice" in
        [Qq]) return 1 ;;
        [Mm]|[Mm][Aa][Nn][Uu][Aa][Ll])
            prompt_manual_compiler
            return $?
            ;;
        *)
            if [[ "$choice" =~ ^[0-9]+$ ]] && (( choice >= 1 && choice <= ${#FOUND_COMPILERS[@]} )); then
                local chosen="${FOUND_COMPILERS[$((choice - 1))]}"
                local chosen_gpp=""
                if [[ -n "$ARG_GPP" ]]; then
                    chosen_gpp="$(resolve_compiler_input "$ARG_GPP" "g++" || true)"
                else
                    chosen_gpp="$(resolve_sibling_compiler "$chosen" "g++" || true)"
                fi
                print_compiler_summary "$chosen" "$chosen_gpp"
                echo ""
                local ok_reply=""
                read -r -p "Is this gcc/g++ path correct? [Y/n/m(anual)]: " ok_reply
                ok_reply="${ok_reply:-Y}"
                case "$ok_reply" in
                    [Yy]|[Yy][Ee][Ss])
                        SELECTED_GCC="$chosen"
                        SELECTED_GPP="$chosen_gpp"
                        return 0
                        ;;
                    [Nn]|[Mm]|[Mm][Aa][Nn][Uu][Aa][Ll])
                        prompt_manual_compiler
                        return $?
                        ;;
                    *)
                        return 1
                        ;;
                esac
            else
                echo "Invalid selection."
                return 1
            fi
            ;;
    esac
}

find_source_artifacts() {
    local base_dir="${1:-$SCRIPT_DIR}"
    PLUGIN_SRC=""
    SPECS_SRC=""
    RT_FILES=()
    HEADER_DIR=""

    local p
    for p in \
        "$base_dir/build/lib/safe_c${PLUGIN_EXT}" \
        "$base_dir/build/bin/safe_c${PLUGIN_EXT}" \
        "$base_dir/lib/safe_c${PLUGIN_EXT}" \
        "$base_dir/bin/safe_c${PLUGIN_EXT}" \
        "$base_dir/build_mingw/bin/safe_c${PLUGIN_EXT}" \
        "$base_dir/safe_c${PLUGIN_EXT}"; do
        if [[ -f "$p" ]]; then
            PLUGIN_SRC="$p"
            break
        fi
    done

    for p in \
        "$base_dir/src/specs/safe_c.specs" \
        "$base_dir/specs/safe_c.specs"; do
        if [[ -f "$p" ]]; then
            SPECS_SRC="$p"
            break
        fi
    done

    for p in \
        "$base_dir/build/lib/libsafe_c_rt.so" \
        "$base_dir/build/lib/libsafe_c_rt.a" \
        "$base_dir/build/lib/libsafe_c_rt.dll.a" \
        "$base_dir/build/bin/libsafe_c_rt.dll" \
        "$base_dir/lib/libsafe_c_rt.so" \
        "$base_dir/lib/libsafe_c_rt.a" \
        "$base_dir/lib/libsafe_c_rt.dll.a" \
        "$base_dir/bin/libsafe_c_rt.dll"; do
        if [[ -f "$p" ]]; then
            RT_FILES+=("$p")
        fi
    done

    if [[ -d "$base_dir/include/safe_c" ]]; then
        HEADER_DIR="$base_dir/include/safe_c"
    fi
}

http_download() {
    local url="$1"
    local out_file="$2"
    if command -v curl >/dev/null 2>&1; then
        if curl -fsSL "$url" -o "$out_file" 2>/dev/null; then
            return 0
        fi
        # Retry explicitly over IPv4 in case dual-stack IPv6 stalls
        if curl -4 -fsSL "$url" -o "$out_file"; then
            return 0
        fi
    elif command -v wget >/dev/null 2>&1; then
        if wget -qO "$out_file" "$url" || wget -4 -qO "$out_file" "$url"; then
            return 0
        fi
    else
        echo "Error: Neither 'curl' nor 'wget' is installed to download from the web." >&2
        return 1
    fi
    return 1
}

resolve_latest_release_tag() {
    local effective=""
    if command -v curl >/dev/null 2>&1; then
        effective="$(curl -fsSLI -o /dev/null -w '%{url_effective}' "${REPO_URL}/releases/latest" 2>/dev/null || true)"
        if [[ -z "$effective" || "$effective" != *"/releases/tag/"* ]]; then
            effective="$(curl -4 -fsSLI -o /dev/null -w '%{url_effective}' "${REPO_URL}/releases/latest" 2>/dev/null || true)"
        fi
    elif command -v wget >/dev/null 2>&1; then
        effective="$(wget -q --max-redirect=10 --spider -S "${REPO_URL}/releases/latest" 2>&1 | awk '/Location:/{loc=$2} END{gsub(/\r/,"",loc); print loc}')"
    fi

    if [[ "$effective" == *"/releases/tag/"* ]]; then
        printf '%s' "${effective##*/releases/tag/}" | tr -d '[:space:]'
        return 0
    fi
    return 1
}

download_from_web() {
    local tag="$RELEASE_TAG"
    echo ""
    if [[ -z "$tag" || "$tag" == "latest" ]]; then
        echo "Resolving latest Safe-C release from GitHub..."
        if ! tag="$(resolve_latest_release_tag)" || [[ -z "$tag" ]]; then
            echo "Error: Could not resolve latest release tag from ${REPO_URL}/releases/latest." >&2
            return 1
        fi
    elif [[ "$tag" != v* ]]; then
        tag="v$tag"
    fi

    local ver="${tag#v}"
    local archive="" subdir=""
    if [[ "$IS_WINDOWS" -eq 1 ]]; then
        archive="Safe-C-${ver}-win64.zip"
        subdir="Safe-C-${ver}-win64"
    else
        archive="Safe-C-${ver}-Linux.tar.gz"
        subdir="Safe-C-${ver}-Linux"
    fi

    local dl_url="${REPO_URL}/releases/download/${tag}/${archive}"
    WEB_TMP_DIR="$(mktemp -d)"
    local archive_path="$WEB_TMP_DIR/$archive"

    echo "Downloading ${archive} (${tag}) from GitHub Releases..."
    if ! http_download "$dl_url" "$archive_path" || [[ ! -s "$archive_path" ]]; then
        echo "Error: Failed to download $dl_url" >&2
        return 1
    fi

    local extract_dir="$WEB_TMP_DIR/extracted"
    mkdir -p "$extract_dir"
    if [[ "$archive" == *.tar.gz ]]; then
        tar -xzf "$archive_path" -C "$extract_dir"
    else
        if command -v unzip >/dev/null 2>&1; then
            unzip -q "$archive_path" -d "$extract_dir"
        elif command -v python3 >/dev/null 2>&1; then
            python3 -m zipfile -e "$archive_path" "$extract_dir"
        elif command -v powershell.exe >/dev/null 2>&1; then
            powershell.exe -NoProfile -Command "Expand-Archive -Path '$(to_native_path "$archive_path")' -DestinationPath '$(to_native_path "$extract_dir")' -Force"
        else
            echo "Error: Need 'unzip', 'python3', or 'powershell.exe' to extract $archive." >&2
            return 1
        fi
    fi

    local pkg_root="$extract_dir/$subdir"
    if [[ ! -d "$pkg_root" ]]; then
        pkg_root="$(find "$extract_dir" -mindepth 1 -maxdepth 2 -type d \( -name "lib" -o -name "bin" \) -exec dirname {} \; | head -n 1 || true)"
    fi
    if [[ -z "$pkg_root" || ! -d "$pkg_root" ]]; then
        pkg_root="$extract_dir"
    fi

    find_source_artifacts "$pkg_root"
    if [[ -z "$PLUGIN_SRC" || -z "$SPECS_SRC" ]]; then
        echo "Error: Downloaded archive did not contain safe_c${PLUGIN_EXT} and safe_c.specs." >&2
        return 1
    fi
    echo "[Downloaded] Ready to load ${tag} from web package."
    return 0
}

build_local_project() {
    local gen_args=()
    if command -v ninja >/dev/null 2>&1; then
        gen_args=(-G Ninja)
    fi
    local compiler_args=()
    [[ -n "$SELECTED_GCC" ]] && compiler_args+=("-DCMAKE_C_COMPILER=$(to_native_path "$SELECTED_GCC")")
    [[ -n "$SELECTED_GPP" ]] && compiler_args+=("-DCMAKE_CXX_COMPILER=$(to_native_path "$SELECTED_GPP")")

    cmake -S "$SCRIPT_DIR" -B "$SCRIPT_DIR/build" "${gen_args[@]}" -DCMAKE_BUILD_TYPE=Release "${compiler_args[@]}"
    cmake --build "$SCRIPT_DIR/build" --config Release

    find_source_artifacts "$SCRIPT_DIR"
    if [[ -z "$PLUGIN_SRC" ]]; then
        echo "Error: Build succeeded but safe_c${PLUGIN_EXT} was still not found." >&2
        return 1
    fi
    return 0
}

acquire_artifacts() {
    if [[ "$FROM_WEB" -eq 1 ]]; then
        download_from_web
        return $?
    fi

    find_source_artifacts "$SCRIPT_DIR"
    if [[ -n "$PLUGIN_SRC" ]]; then
        return 0
    fi

    echo ""
    echo "Safe-C binary (safe_c${PLUGIN_EXT}) was not found locally in build/ or lib/bin/."

    if [[ -f "$SCRIPT_DIR/CMakeLists.txt" ]] && command -v cmake >/dev/null 2>&1; then
        if [[ "$ASSUME_YES" -eq 1 ]]; then
            download_from_web
            return $?
        fi
        echo "Choose how to obtain Safe-C files:"
        echo "  1) Download prebuilt Safe-C release from GitHub Releases (default)"
        echo "  2) Build Safe-C locally from source with CMake"
        echo "  3) Cancel"
        local src_choice=""
        read -r -p "Choose option [1/2/3] (default 1): " src_choice
        src_choice="${src_choice:-1}"
        case "$src_choice" in
            1) download_from_web; return $? ;;
            2) build_local_project; return $? ;;
            *) echo "Aborted."; return 1 ;;
        esac
    fi

    if prompt_yn "Download latest prebuilt Safe-C release from GitHub Releases? [Y/n]:" "Y"; then
        download_from_web
        return $?
    fi
    echo "Aborted. Build Safe-C locally or pass --from-web."
    return 1
}

generate_merged_specs() {
    local gcc_cmd="$1"
    local specs_src="$2"
    local out_file="$3"

    local rule
    rule="$(awk '
        { gsub(/\r$/, "") }
        /^[[:space:]]*$/ { next }
        /^[[:space:]]*#/ { next }
        /^[[:space:]]*\*/ { next }
        {
            sub(/^[[:space:]]*\+[[:space:]]*/, "")
            sub(/^[[:space:]]+/, "")
            print
            exit
        }
    ' "$specs_src")"

    if [[ -z "$rule" ]]; then
        echo "Error: Failed to parse Safe-C rule from $specs_src" >&2
        return 1
    fi

    # Merge the Safe-C rule into GCC's full built-in specs dump so *libgcc:,
    # *link:, and unwinding flags stay intact.
    "$gcc_cmd" -dumpspecs | tr -d '\r' | awk -v safe_rule="$rule" '
        BEGIN { in_self = 0; replaced = 0; self_buf = "" }
        /^\*self_spec:[[:space:]]*$/ {
            print
            in_self = 1
            replaced = 1
            next
        }
        in_self == 1 {
            if (/^\*/) {
                if (self_buf != "") {
                    print self_buf " " safe_rule
                } else {
                    print safe_rule
                }
                print ""
                in_self = 0
                print
                next
            }
            if ($0 !~ /^[[:space:]]*$/) {
                if (index($0, "fplugin=safe_c") == 0) {
                    self_buf = (self_buf == "" ? $0 : self_buf " " $0)
                }
            }
            next
        }
        { print }
        END {
            if (in_self == 1) {
                if (self_buf != "") {
                    print self_buf " " safe_rule
                } else {
                    print safe_rule
                }
            } else if (replaced == 0) {
                print ""
                print "*self_spec:"
                print safe_rule
            }
        }
    ' > "$out_file"
}

resolve_runtime_dest_dirs() {
    local gcc_bin="$1"
    local bin_dir prefix
    bin_dir="$(dirname -- "$gcc_bin")"
    prefix="$(dirname -- "$bin_dir")"

    if [[ "$IS_WINDOWS" -eq 1 ]]; then
        RT_DEST_BIN="$prefix/bin"
        RT_DEST_LIB="$prefix/lib"
        RT_DEST_INC="$prefix/include/safe_c"
    elif [[ "$prefix" == "/usr" ]]; then
        RT_DEST_BIN=""
        RT_DEST_LIB="/usr/local/lib"
        RT_DEST_INC="/usr/local/include/safe_c"
    else
        RT_DEST_BIN=""
        RT_DEST_LIB="$prefix/lib"
        RT_DEST_INC="$prefix/include/safe_c"
    fi
}

install_single_target() {
    local gcc_cmd="$1"
    local gpp_cmd="$2"

    query_gcc_info "$gcc_cmd"
    local install_dir="$Q_INSTALL_DIR"
    local plugin_dir="$Q_PLUGIN_DIR"
    local manifest="$install_dir/safe_c.manifest"
    local dest_specs="$install_dir/specs"
    local specs_bak="$install_dir/specs.safe_c.bak"

    if [[ -z "$install_dir" ]]; then
        echo "Error: Could not determine GCC install directory for $gcc_cmd" >&2
        return 1
    fi

    # Non-plugin MinGW-w64 fallback delegates PE import injection cleanly
    if [[ "$Q_HAS_PLUGIN" -ne 1 || -z "$plugin_dir" ]]; then
        local py_cmd=""
        command -v python3 >/dev/null 2>&1 && py_cmd="python3"
        [[ -z "$py_cmd" ]] && command -v python >/dev/null 2>&1 && py_cmd="python"
        local helper="$SCRIPT_DIR/tools/integrate_toolchain.py"
        if [[ "$IS_WINDOWS" -eq 1 && -n "$py_cmd" && -f "$helper" ]]; then
            echo "Non-plugin MinGW-w64 detected; invoking integrate_toolchain.py ..."
            "$py_cmd" "$helper" "$(to_native_path "$gcc_cmd")"
            return $?
        fi
        echo "Error: Target GCC does not advertise --enable-plugin (plugin dir: '${plugin_dir:-none}')." >&2
        echo "If running on Linux, install the GCC plugin development package or use a plugin-enabled GCC." >&2
        return 1
    fi

    local tmp_specs tmp_manifest
    tmp_specs="$(mktemp)"
    tmp_manifest="$(mktemp)"

    generate_merged_specs "$gcc_cmd" "$SPECS_SRC" "$tmp_specs"

    {
        echo "# Safe-C installation manifest (managed by install.sh)"
        echo "GCC=$(to_native_path "$gcc_cmd")"
        [[ -n "$gpp_cmd" ]] && echo "GPP=$(to_native_path "$gpp_cmd")"
        echo "INSTALL_DIR=$install_dir"
    } > "$tmp_manifest"

    run_priv_dir "$install_dir" mkdir -p "$plugin_dir"

    local dest_plugin="$plugin_dir/safe_c${PLUGIN_EXT}"
    run_priv_dir "$plugin_dir" cp -f "$PLUGIN_SRC" "$dest_plugin"
    echo "PLUGIN_FILE=$dest_plugin" >> "$tmp_manifest"
    echo "[Installed] Plugin -> $(to_native_path "$dest_plugin")"

    # Back up pre-existing non-Safe-C specs once before overwriting
    if [[ -f "$dest_specs" ]]; then
        if ! grep -q "fplugin=safe_c" "$dest_specs" 2>/dev/null; then
            if [[ ! -f "$specs_bak" ]]; then
                run_priv_dir "$install_dir" cp -p "$dest_specs" "$specs_bak"
                echo "[Backup] Preserved existing specs -> $(to_native_path "$specs_bak")"
            fi
        fi
    fi
    [[ -f "$specs_bak" ]] && echo "SPECS_BACKUP=$specs_bak" >> "$tmp_manifest"

    run_priv_dir "$install_dir" cp -f "$tmp_specs" "$dest_specs"
    if [[ "$IS_WINDOWS" -eq 0 ]]; then
        run_priv_dir "$install_dir" chmod 0644 "$dest_specs" "$dest_plugin"
    fi
    echo "SPECS_FILE=$dest_specs" >> "$tmp_manifest"
    echo "[Installed] Specs  -> $(to_native_path "$dest_specs")"

    # Optional runtime guardrails & headers (isolated paths, fully tracked)
    if [[ "$NO_RUNTIME" -eq 0 ]]; then
        resolve_runtime_dest_dirs "$gcc_cmd"
        local rt_installed=0
        local f base dest_f
        for f in "${RT_FILES[@]:-}"; do
            [[ -f "$f" ]] || continue
            base="$(basename -- "$f")"
            if [[ "$base" == *.dll && -n "$RT_DEST_BIN" ]]; then
                run_priv_dir "$RT_DEST_BIN" mkdir -p "$RT_DEST_BIN"
                dest_f="$RT_DEST_BIN/$base"
                run_priv_dir "$RT_DEST_BIN" cp -f "$f" "$dest_f"
            else
                run_priv_dir "$RT_DEST_LIB" mkdir -p "$RT_DEST_LIB"
                dest_f="$RT_DEST_LIB/$base"
                run_priv_dir "$RT_DEST_LIB" cp -f "$f" "$dest_f"
            fi
            echo "EXTRA_FILE=$dest_f" >> "$tmp_manifest"
            echo "[Installed] Runtime -> $(to_native_path "$dest_f")"
            rt_installed=1
        done

        if [[ -n "$HEADER_DIR" && -d "$HEADER_DIR" ]]; then
            run_priv_dir "$RT_DEST_INC" mkdir -p "$RT_DEST_INC"
            echo "EXTRA_DIR=$RT_DEST_INC" >> "$tmp_manifest"
            local hdr
            for hdr in "$HEADER_DIR"/*.h; do
                [[ -f "$hdr" ]] || continue
                dest_f="$RT_DEST_INC/$(basename -- "$hdr")"
                run_priv_dir "$RT_DEST_INC" cp -f "$hdr" "$dest_f"
                echo "EXTRA_FILE=$dest_f" >> "$tmp_manifest"
            done
            echo "[Installed] Headers -> $(to_native_path "$RT_DEST_INC")"
        fi

        if [[ "$rt_installed" -eq 1 && "$IS_WINDOWS" -eq 0 && "$RT_DEST_LIB" == "/usr/local/lib" ]] && command -v ldconfig >/dev/null 2>&1; then
            run_priv_dir "$RT_DEST_LIB" ldconfig 2>/dev/null || true
        fi
    fi

    run_priv_dir "$install_dir" cp -f "$tmp_manifest" "$manifest"
    rm -f "$tmp_specs" "$tmp_manifest"
}

verify_toolchain_works() {
    local gcc_cmd="$1"
    local gpp_cmd="$2"
    local tmp_dir
    tmp_dir="$(mktemp -d)"

    cat > "$tmp_dir/ok.c" <<'EOF'
#include <stdlib.h>
int main(void) {
    int *p = (int *)malloc(sizeof(int));
    if (!p) return 0;
    *p = 42;
    free(p);
    return 0;
}
EOF
    cat > "$tmp_dir/bad.c" <<'EOF'
#include <stdlib.h>
int main(void) {
    int *p = (int *)malloc(sizeof(int));
    free(p);
    free(p);
    return 0;
}
EOF

    echo ""
    echo "Verifying compiler integration..."
    local verify_err=""
    if ! verify_err="$("$gcc_cmd" -c "$tmp_dir/ok.c" -o "$tmp_dir/ok.o" 2>&1)"; then
        echo "Warning: Valid C compile check failed:" >&2
        printf '%s\n' "$verify_err" >&2
        if [[ "$IS_WINDOWS" -eq 0 && ( "$verify_err" == *"incompatible version"* || "$verify_err" == *"cannot load plugin"* || "$verify_err" == *"undefined symbol"* ) ]]; then
            echo "Hint: Prebuilt Linux safe_c.so targets CI's GCC ABI version. Run './install.sh' inside a source clone without '--from-web' to build locally for your host GCC." >&2
        fi
        rm -rf "$tmp_dir"
        return 1
    fi

    if "$gcc_cmd" -c "$tmp_dir/bad.c" -o "$tmp_dir/bad.o" >/dev/null 2>&1; then
        echo "Warning: Test unsafe C file compiled without halting. Check specs/plugin loading." >&2
    else
        echo "  [OK] C auto-load check: valid code passed, unsafe double-free blocked."
    fi

    if [[ -n "$gpp_cmd" ]]; then
        cat > "$tmp_dir/ok.cpp" <<'EOF'
#include <vector>
int main() {
    std::vector<int> v{1, 2, 3};
    return v[0] == 1 ? 0 : 1;
}
EOF
        if "$gpp_cmd" -std=c++20 "$tmp_dir/ok.cpp" -o "$tmp_dir/ok_cpp" >/dev/null 2>&1; then
            echo "  [OK] C++20 compile & link check passed."
        else
            echo "Warning: C++20 link check failed!" >&2
            rm -rf "$tmp_dir"
            return 1
        fi
    fi

    rm -rf "$tmp_dir"
    return 0
}

do_install() {
    select_compiler_interactive || return 1
    acquire_artifacts || return 1

    if [[ -z "$SPECS_SRC" || ! -f "$SPECS_SRC" ]]; then
        echo "Error: safe_c.specs not found in $SCRIPT_DIR/src/specs or $SCRIPT_DIR/specs" >&2
        return 1
    fi

    query_gcc_info "$SELECTED_GCC"
    local gcc_install="$Q_INSTALL_DIR"
    local gpp_install="$gcc_install"
    if [[ -n "$SELECTED_GPP" ]]; then
        query_gcc_info "$SELECTED_GPP"
        gpp_install="$Q_INSTALL_DIR"
    fi

    echo ""
    echo "Ready to install Safe-C from:"
    echo "  Plugin source : $(to_native_path "$PLUGIN_SRC")"
    echo "  Specs source  : $(to_native_path "$SPECS_SRC")"
    if ! prompt_yn "Proceed with installation? [Y/n]:" "Y"; then
        echo "Installation cancelled."
        return 1
    fi

    install_single_target "$SELECTED_GCC" "$SELECTED_GPP"

    if [[ -n "$SELECTED_GPP" && -n "$gpp_install" && "$gpp_install" != "$gcc_install" ]]; then
        echo ""
        echo "Note: g++ resolves to a separate GCC install directory ($gpp_install); installing there too..."
        install_single_target "$SELECTED_GPP" "$SELECTED_GPP"
    fi

    if ! verify_toolchain_works "$SELECTED_GCC" "$SELECTED_GPP"; then
        echo "" >&2
        echo "Verification failed; reverting Safe-C installation so your compiler stays intact..." >&2
        ASSUME_YES=1 uninstall_single_target "$SELECTED_GCC" || true
        if [[ -n "$SELECTED_GPP" && -n "$gpp_install" && "$gpp_install" != "$gcc_install" ]]; then
            ASSUME_YES=1 uninstall_single_target "$SELECTED_GPP" || true
        fi
        return 1
    fi
    echo ""
    echo "Safe-C installation complete. Standard 'gcc' and 'g++' commands now check memory safety automatically."
    echo "To disable temporarily on a build: export SAFE_C_DISABLE=1  (or pass -DSAFE_C_DISABLE)."
}

uninstall_single_target() {
    local gcc_cmd="$1"
    query_gcc_info "$gcc_cmd"
    local install_dir="$Q_INSTALL_DIR"
    local plugin_dir="$Q_PLUGIN_DIR"
    local manifest="$install_dir/safe_c.manifest"
    local need_ldconfig=0

    if [[ -z "$install_dir" ]]; then
        echo "Error: Could not determine GCC install directory for $gcc_cmd" >&2
        return 1
    fi

    echo ""
    if [[ -f "$manifest" ]]; then
        echo "Found Safe-C installation manifest: $(to_native_path "$manifest")"
        local plugin_file="" specs_file="" specs_bak=""
        local extra_files=() extra_dirs=()
        local line key val
        while IFS= read -r line; do
            [[ -z "$line" || "$line" == \#* ]] && continue
            key="${line%%=*}"
            val="${line#*=}"
            case "$key" in
                PLUGIN_FILE) plugin_file="$val" ;;
                SPECS_FILE) specs_file="$val" ;;
                SPECS_BACKUP) specs_bak="$val" ;;
                EXTRA_FILE) extra_files+=("$val") ;;
                EXTRA_DIR) extra_dirs+=("$val") ;;
            esac
        done < "$manifest"

        echo "The following Safe-C changes will be reverted cleanly:"
        [[ -n "$plugin_file" ]] && echo "  Remove plugin : $(to_native_path "$plugin_file")"
        if [[ -n "$specs_bak" && -f "$specs_bak" ]]; then
            echo "  Restore specs : $(to_native_path "$specs_bak") -> $(to_native_path "$specs_file")"
        elif [[ -n "$specs_file" ]]; then
            echo "  Remove specs  : $(to_native_path "$specs_file") (only if Safe-C managed)"
        fi
        if [[ ${#extra_files[@]} -gt 0 ]]; then
            echo "  Remove ${#extra_files[@]} Safe-C runtime/header file(s)"
        fi

        if ! prompt_yn "Proceed with Safe-C uninstall? [Y/n]:" "Y"; then
            echo "Uninstall cancelled."
            return 1
        fi

        if [[ -n "$specs_bak" && -f "$specs_bak" ]]; then
            run_priv_dir "$install_dir" mv -f "$specs_bak" "${specs_file:-$install_dir/specs}"
            echo "[Restored] Pre-existing specs file."
        elif [[ -n "$specs_file" && -f "$specs_file" ]]; then
            if grep -q "fplugin=safe_c" "$specs_file" 2>/dev/null; then
                run_priv_dir "$install_dir" rm -f "$specs_file"
                echo "[Removed] $(to_native_path "$specs_file")"
            else
                echo "[Preserved] $(to_native_path "$specs_file") contains non-Safe-C customization; leaving untouched."
            fi
        fi

        if [[ -n "$plugin_file" && -f "$plugin_file" ]]; then
            run_priv_dir "$(dirname -- "$plugin_file")" rm -f "$plugin_file"
            echo "[Removed] $(to_native_path "$plugin_file")"
        fi

        local f
        for f in "${extra_files[@]:-}"; do
            if [[ -f "$f" ]]; then
                run_priv_dir "$(dirname -- "$f")" rm -f "$f"
                echo "[Removed] $(to_native_path "$f")"
                [[ "$f" == /usr/local/lib/*.so ]] && need_ldconfig=1
            fi
        done

        local d
        for d in "${extra_dirs[@]:-}"; do
            if [[ -d "$d" ]]; then
                run_priv_dir "$d" rmdir "$d" 2>/dev/null && echo "[Removed] empty dir $(to_native_path "$d")" || true
            fi
        done

        run_priv_dir "$install_dir" rm -f "$manifest"
    else
        echo "No manifest at $(to_native_path "$manifest"); checking standard Safe-C files for this toolchain..."
        local candidate_specs="$install_dir/specs"
        local candidate_bak="$install_dir/specs.safe_c.bak"
        local candidate_plugin=""
        [[ -n "$plugin_dir" ]] && candidate_plugin="$plugin_dir/safe_c${PLUGIN_EXT}"

        if ! prompt_yn "Check and remove Safe-C files from $(to_native_path "$install_dir")? [Y/n]:" "Y"; then
            echo "Uninstall cancelled."
            return 1
        fi

        if [[ -f "$candidate_bak" ]]; then
            run_priv_dir "$install_dir" mv -f "$candidate_bak" "$candidate_specs"
            echo "[Restored] $(to_native_path "$candidate_specs") from backup."
        elif [[ -f "$candidate_specs" ]]; then
            if grep -q "fplugin=safe_c" "$candidate_specs" 2>/dev/null; then
                run_priv_dir "$install_dir" rm -f "$candidate_specs"
                echo "[Removed] $(to_native_path "$candidate_specs")"
            else
                echo "[Preserved] $(to_native_path "$candidate_specs") is not a Safe-C specs file; leaving untouched."
            fi
        fi

        if [[ -n "$candidate_plugin" && -f "$candidate_plugin" ]]; then
            run_priv_dir "$plugin_dir" rm -f "$candidate_plugin"
            echo "[Removed] $(to_native_path "$candidate_plugin")"
        fi

        # Also clean up non-plugin MinGW .orig backups if present
        if [[ "$IS_WINDOWS" -eq 1 ]] && command -v python3 >/dev/null 2>&1 && [[ -f "$SCRIPT_DIR/tools/integrate_toolchain.py" ]]; then
            python3 "$SCRIPT_DIR/tools/integrate_toolchain.py" --uninstall "$(to_native_path "$gcc_cmd")" || true
        fi

        # Clean up known Safe-C-only runtime artifacts if present
        resolve_runtime_dest_dirs "$gcc_cmd"
        local rt_known
        for rt_known in \
            "$RT_DEST_LIB/libsafe_c_rt.so" \
            "$RT_DEST_LIB/libsafe_c_rt.a" \
            "$RT_DEST_LIB/libsafe_c_rt.dll.a" \
            "${RT_DEST_BIN:+$RT_DEST_BIN/libsafe_c_rt.dll}"; do
            if [[ -n "$rt_known" && -f "$rt_known" ]]; then
                run_priv_dir "$(dirname -- "$rt_known")" rm -f "$rt_known"
                echo "[Removed] $(to_native_path "$rt_known")"
                [[ "$rt_known" == /usr/local/lib/*.so ]] && need_ldconfig=1
            fi
        done
        local h
        for h in api.h common.h typestate.h; do
            if [[ -f "$RT_DEST_INC/$h" ]]; then
                run_priv_dir "$RT_DEST_INC" rm -f "$RT_DEST_INC/$h"
                echo "[Removed] $(to_native_path "$RT_DEST_INC/$h")"
            fi
        done
        if [[ -d "$RT_DEST_INC" ]]; then
            run_priv_dir "$RT_DEST_INC" rmdir "$RT_DEST_INC" 2>/dev/null || true
        fi
    fi

    if [[ "$need_ldconfig" -eq 1 && "$IS_WINDOWS" -eq 0 ]] && command -v ldconfig >/dev/null 2>&1; then
        run_priv_dir "/usr/local/lib" ldconfig 2>/dev/null || true
    fi
}

do_uninstall() {
    select_compiler_interactive || return 1

    query_gcc_info "$SELECTED_GCC"
    local gcc_install="$Q_INSTALL_DIR"
    local gpp_install="$gcc_install"
    if [[ -n "$SELECTED_GPP" ]]; then
        query_gcc_info "$SELECTED_GPP"
        gpp_install="$Q_INSTALL_DIR"
    fi

    uninstall_single_target "$SELECTED_GCC" || return 1
    if [[ -n "$SELECTED_GPP" && -n "$gpp_install" && "$gpp_install" != "$gcc_install" ]]; then
        uninstall_single_target "$SELECTED_GPP" || return 1
    fi

    # Verify stock gcc/g++ still work cleanly after uninstall
    local tmp_dir
    tmp_dir="$(mktemp -d)"
    echo "int main(void) { return 0; }" > "$tmp_dir/test.c"
    if "$SELECTED_GCC" "$tmp_dir/test.c" -o "$tmp_dir/test_out" >/dev/null 2>&1; then
        echo ""
        echo "[OK] Post-uninstall check: gcc compiles cleanly without Safe-C."
    fi
    rm -rf "$tmp_dir"

    echo "Safe-C uninstalled cleanly. System compiler and unrelated files were preserved."
}

main() {
    if [[ -z "$ACTION" ]]; then
        echo "Safe-C Toolchain Installer & Uninstaller"
        echo "  1) Install / Update Safe-C into GCC/G++"
        echo "  2) Uninstall Safe-C cleanly"
        echo "  3) Exit"
        local menu_choice=""
        read -r -p "Choose action [1/2/3] (default 1): " menu_choice
        menu_choice="${menu_choice:-1}"
        case "$menu_choice" in
            1|install) ACTION="install" ;;
            2|uninstall) ACTION="uninstall" ;;
            3|q|Q|exit) exit 0 ;;
            *) echo "Invalid choice." >&2; exit 1 ;;
        esac
    fi

    case "$ACTION" in
        install) do_install ;;
        uninstall) do_uninstall ;;
        *) usage >&2; exit 1 ;;
    esac
}

main
