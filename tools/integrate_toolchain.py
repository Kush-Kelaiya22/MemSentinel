#!/usr/bin/env python3
"""
Safe-C Toolchain Integration Helper
Enables Safe-C to auto-load by default in GCC/G++ MinGW-w64 toolchains with zero arguments or flags.
Supports both plugin-enabled GCC and non-plugin MinGW-w64 distributions.
"""

import sys
import os
import shutil
import struct
import subprocess

def align_up(x, align):
    return (x + align - 1) & ~(align - 1)

def inject_import(exe_path, out_path, dll_name, func_name):
    with open(exe_path, 'rb') as f:
        data = bytearray(f.read())

    e_lfanew = struct.unpack_from('<I', data, 0x3c)[0]
    num_sections = struct.unpack_from('<H', data, e_lfanew + 6)[0]
    opt_size = struct.unpack_from('<H', data, e_lfanew + 20)[0]
    sec_align = struct.unpack_from('<I', data, e_lfanew + 24 + 32)[0]
    file_align = struct.unpack_from('<I', data, e_lfanew + 24 + 36)[0]
    import_rva, import_size = struct.unpack_from('<II', data, e_lfanew + 24 + 120)

    sec_offset = e_lfanew + 24 + opt_size
    sections = []
    for i in range(num_sections):
        s_data = data[sec_offset + i*40 : sec_offset + (i+1)*40]
        name = s_data[:8].rstrip(b'\0')
        vsize, vaddr, raw_size, raw_offset = struct.unpack_from('<IIII', s_data, 8)
        sections.append({'name': name, 'vsize': vsize, 'vaddr': vaddr, 'raw_size': raw_size, 'raw_offset': raw_offset})

    # Check if already injected
    if any(s['name'] == b'.safec' for s in sections):
        print(f"  [Info] {exe_path} already has .safec section.")
        return False

    last_sec = sections[-1]
    new_vaddr = align_up(last_sec['vaddr'] + max(last_sec['vsize'], last_sec['raw_size']), sec_align)
    new_raw_offset = align_up(len(data), file_align)

    import_sec = None
    for s in sections:
        if s['vaddr'] <= import_rva < s['vaddr'] + s['vsize']:
            import_sec = s
            break
    if not import_sec:
        return False
    import_file_offset = import_sec['raw_offset'] + (import_rva - import_sec['vaddr'])

    existing_descs = bytearray()
    curr_off = import_file_offset
    while True:
        desc = data[curr_off : curr_off + 20]
        oft, time_date, forwarder, name_rva, ft = struct.unpack('<IIIII', desc)
        if oft == 0 and name_rva == 0:
            break
        existing_descs.extend(desc)
        curr_off += 20

    num_existing = len(existing_descs) // 20

    sec_data = bytearray()
    new_descs_len = (num_existing + 2) * 20
    sec_data.extend(existing_descs)
    sec_data.extend(b'\x00' * 40)

    def pad8(b):
        while len(b) % 8 != 0:
            b += b'\x00'
        return b

    hn_offset = len(sec_data)
    sec_data.extend(struct.pack('<H', 0))
    sec_data.extend(func_name.encode('ascii') + b'\x00')
    sec_data = pad8(sec_data)
    hn_rva = new_vaddr + hn_offset

    oft_offset = len(sec_data)
    sec_data.extend(struct.pack('<Q', hn_rva))
    sec_data.extend(struct.pack('<Q', 0))
    oft_rva = new_vaddr + oft_offset

    ft_offset = len(sec_data)
    sec_data.extend(struct.pack('<Q', hn_rva))
    sec_data.extend(struct.pack('<Q', 0))
    ft_rva = new_vaddr + ft_offset

    dll_offset = len(sec_data)
    sec_data.extend(dll_name.encode('ascii') + b'\x00')
    sec_data = pad8(sec_data)
    dll_rva = new_vaddr + dll_offset

    new_desc = struct.pack('<IIIII', oft_rva, 0, 0, dll_rva, ft_rva)
    sec_data[num_existing * 20 : (num_existing + 1) * 20] = new_desc

    new_import_rva = new_vaddr
    new_import_size = new_descs_len

    raw_sec_size = align_up(len(sec_data), file_align)
    vsec_size = align_up(len(sec_data), sec_align)
    sec_data.extend(b'\x00' * (raw_sec_size - len(sec_data)))

    struct.pack_into('<H', data, e_lfanew + 6, num_sections + 1)
    struct.pack_into('<I', data, e_lfanew + 24 + 56, new_vaddr + vsec_size)
    struct.pack_into('<II', data, e_lfanew + 24 + 120, new_import_rva, new_import_size)

    new_sec_header = struct.pack(
        '<8sIIIIIIHHI',
        b'.safec\0\0',
        len(sec_data),
        new_vaddr,
        raw_sec_size,
        new_raw_offset,
        0, 0, 0, 0,
        0xC0000040
    )
    new_header_pos = sec_offset + num_sections * 40
    data[new_header_pos : new_header_pos + 40] = new_sec_header

    data.extend(b'\x00' * (new_raw_offset - len(data)))
    data.extend(sec_data)

    with open(out_path, 'wb') as f:
        f.write(data)
    return True

def build_merged_specs(gcc_cmd, specs_src):
    with open(specs_src, 'r', encoding='utf-8') as f:
        rule = next(
            l.strip() for l in f.read().splitlines()
            if l.strip() and not l.lstrip().startswith('#') and not l.lstrip().startswith('*')
        )
    clean_rule = rule[2:] if rule.startswith('+ ') else rule

    raw = subprocess.check_output([gcc_cmd, '-dumpspecs'], text=True)
    marker = '*self_spec:\n'
    idx = raw.find(marker)
    if idx != -1:
        end_idx = raw.find('\n*cc1_cpu:', idx)
        if end_idx != -1:
            return raw[:idx] + marker + clean_rule + '\n' + raw[end_idx:]
    return raw + '\n\n*self_spec:\n' + clean_rule + '\n\n'

def main():
    uninstall = '--uninstall' in sys.argv[1:]
    pos_args = [a for a in sys.argv[1:] if a != '--uninstall']
    gcc_cmd = pos_args[0] if pos_args else 'gcc'

    # Resolve search dirs
    try:
        res = subprocess.run([gcc_cmd, '-print-search-dirs'], capture_output=True, text=True, check=True)
    except Exception as e:
        print(f"Error querying {gcc_cmd}: {e}")
        return 1

    install_dir = None
    programs_dirs = []
    for line in res.stdout.splitlines():
        if line.startswith('install:'):
            install_dir = line.split(':', 1)[1].strip()
        elif line.startswith('programs:'):
            parts = line.split('=', 1)[1].strip().split(';')
            programs_dirs = [p for p in parts if p]

    if not install_dir:
        print("Could not locate GCC install directory.")
        return 1

    print(f"Target GCC Install Dir: {install_dir}")

    # Check for plugin support
    has_plugin_support = False
    try:
        ver_res = subprocess.run([gcc_cmd, '-v'], capture_output=True, text=True)
        if '--enable-plugin' in ver_res.stderr or '--enable-plugin' in ver_res.stdout:
            has_plugin_support = True
    except Exception:
        has_plugin_support = False

    repo_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    plugin_ext = '.dll' if os.name == 'nt' else '.so'
    candidate_bins = [
        os.path.join(repo_dir, 'build', 'lib', f'safe_c{plugin_ext}'),
        os.path.join(repo_dir, 'build', 'bin', f'safe_c{plugin_ext}'),
        os.path.join(repo_dir, 'build_mingw', 'bin', f'safe_c{plugin_ext}'),
    ]
    built_dll = next((p for p in candidate_bins if os.path.exists(p)), candidate_bins[0])

    if uninstall:
        # Restore backups
        specs_path = os.path.join(install_dir, 'specs')
        if os.path.exists(specs_path):
            os.remove(specs_path)
            print("Removed specs file.")
        for p in programs_dirs:
            for tool in ['cc1.exe', 'cc1plus.exe']:
                orig = os.path.join(p, tool + '.orig')
                curr = os.path.join(p, tool)
                if os.path.exists(orig):
                    shutil.copy2(orig, curr)
                    os.remove(orig)
                    print(f"Restored {tool} from {orig}.")
        print("Safe-C auto-loading successfully uninstalled.")
        return 0

    if not os.path.exists(built_dll):
        print(f"Error: Safe-C binary not found at {built_dll}. Build the project first.")
        return 1

    # Remove any specs in non-plugin toolchain so it doesn't try to pass -fplugin
    if not has_plugin_support:
        sp = os.path.join(install_dir, 'specs')
        if os.path.exists(sp):
            os.remove(sp)

    # Plugin-capable toolchain
    if has_plugin_support:
        try:
            plugin_res = subprocess.run([gcc_cmd, '-print-file-name=plugin'], capture_output=True, text=True, check=True)
            plugin_dir = plugin_res.stdout.strip()
        except Exception:
            plugin_dir = ""

        if plugin_dir and os.path.exists(plugin_dir):
            os.makedirs(plugin_dir, exist_ok=True)
            dest_dll = os.path.join(plugin_dir, f'safe_c{plugin_ext}')
            shutil.copy2(built_dll, dest_dll)
            print(f"Copied plugin to {dest_dll}")

            specs_src = os.path.join(repo_dir, 'src', 'specs', 'safe_c.specs')
            dest_specs = os.path.join(install_dir, 'specs')
            if os.path.exists(dest_specs):
                os.remove(dest_specs)
            merged_specs = build_merged_specs(gcc_cmd, specs_src)
            with open(dest_specs, 'w', encoding='utf-8') as f:
                f.write(merged_specs)
            print(f"Installed merged specs to {dest_specs}")
            print("Safe-C auto-loading successfully enabled via native GCC specs.")
            return 0

    # Non-plugin MinGW-w64 toolchain (direct CC1 injection)
    print("MinGW-w64 without plugin support detected. Integrating directly into compiler backend...")
    for p in programs_dirs:
        for tool in ['cc1.exe', 'cc1plus.exe']:
            target_exe = os.path.join(p, tool)
            if os.path.exists(target_exe):
                backup_exe = target_exe + '.orig'
                if not os.path.exists(backup_exe):
                    shutil.copy2(target_exe, backup_exe)
                    print(f"Created backup: {backup_exe}")
                
                # Copy safe_c.dll adjacent to cc1.exe
                dest_dll = os.path.join(p, 'safe_c.dll')
                shutil.copy2(built_dll, dest_dll)
                print(f"Copied safe_c.dll to {dest_dll}")

                ok = inject_import(backup_exe, target_exe, 'safe_c.dll', 'safe_c_addon_inspect')
                if ok:
                    print(f"Successfully integrated Safe-C into {tool}.")
                else:
                    print(f"{tool} is already integrated.")

    print("\nSafe-C successfully integrated for MinGW-w64! Zero arguments/flags required.")
    return 0

if __name__ == '__main__':
    sys.exit(main())
