"""Check ARM64 PE architecture and matching MSF7 PDB identity; no extra packages."""
import pathlib
import struct
import sys

def u32(data, offset):
    return struct.unpack_from("<I", data, offset)[0]

def pe_identity(data):
    pe = u32(data, 0x3c)
    if data[pe:pe+4] != b"PE\0\0":
        raise ValueError("Not a PE image")
    machine, count = struct.unpack_from("<HH", data, pe+4)
    if machine not in (0xaa64, 0xa641, 0xa64e):
        raise ValueError(f"Not an ARM64-compatible image: {machine:#x}")
    optional = pe + 24
    if struct.unpack_from("<H", data, optional)[0] != 0x20b:
        raise ValueError("Expected PE32+")
    section_start = optional + struct.unpack_from("<H", data, pe+20)[0]
    def offset(rva):
        for i in range(count):
            start = section_start + i*40
            size, virtual, raw_size, raw = struct.unpack_from("<IIII", data, start+8)
            if virtual <= rva < virtual + max(size, raw_size):
                return raw + rva - virtual
        raise ValueError("Unmapped debug directory")
    rva, size = struct.unpack_from("<II", data, optional+112+6*8)
    if not rva:
        return None
    directory = offset(rva)
    for index in range(size//28):
        entry = directory + index*28
        if u32(data, entry+12) == 2:
            raw = u32(data, entry+24)
            if data[raw:raw+4] == b"RSDS":
                return data[raw+4:raw+20], u32(data, raw+20)
    return None

def pdb_identity(data):
    if not data.startswith(b"Microsoft C/C++ MSF 7.00"):
        raise ValueError("Expected MSF7 PDB")
    block, _, _, size, _, block_map = struct.unpack_from("<6I", data, 32)
    def gather(indices, length):
        return b"".join(data[i*block:(i+1)*block] for i in indices)[:length]
    count = (size+block-1)//block
    indices = struct.unpack_from(f"<{count}I", data, block_map*block)
    directory = gather(indices, size)
    streams = u32(directory, 0)
    sizes = struct.unpack_from(f"<{streams}I", directory, 4)
    cursor = 4 + streams*4
    for index, length in enumerate(sizes):
        count = 0 if length == 0xffffffff else (length+block-1)//block
        indices = struct.unpack_from(f"<{count}I", directory, cursor)
        cursor += count*4
        if index == 1:
            info = gather(indices, length)
            return info[12:28], u32(info, 8)
    raise ValueError("PDB information stream missing")

if __name__ == "__main__":
    image = pathlib.Path(sys.argv[1])
    try:
        identity = pe_identity(image.read_bytes())
    except Exception as e:
        print(f"Verification error in {image.name}: {e}", file=sys.stderr)
        raise
    if len(sys.argv) == 3:
        if identity is None or identity != pdb_identity(pathlib.Path(sys.argv[2]).read_bytes()):
            raise SystemExit(f"Executable/PDB identity mismatch: {image.name}")
        print(f"Verified ARM64 executable and matching PDB: {image.name}")
    else:
        print(f"Verified ARM64-compatible runtime: {image.name}")
