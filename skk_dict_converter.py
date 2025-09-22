import struct

def convert_skk_dict_to_c_array(input_file, output_file):
    entries = []
    with open(input_file, 'r', encoding='utf-8') as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith('#'):
                continue
            
            parts = line.split(' /', 1)
            if len(parts) != 2:
                print(f"Warning: Skipping malformed line: {line}")
                continue
            
            yomi = parts[0].strip()
            candidates_str = parts[1].strip().rstrip('/')
            candidates = candidates_str.split('/')
            
            entries.append({'yomi': yomi, 'candidates': candidates})

    # Sort entries by yomi for binary search
    entries.sort(key=lambda x: x['yomi'])

    data_part = b""
    index_part = b""
    
    # Build data part and collect offsets for index part
    offsets = []
    for entry in entries:
        current_offset = len(data_part)
        offsets.append(current_offset)
        
        # Format: yomi,candidate1,candidate2,...
        entry_str = entry['yomi'] + "," + ",".join(entry['candidates'])
        data_part += entry_str.encode('utf-8') + b'\0' # Null-terminate each entry

    # Calculate header values
    size_keyword = len(entries)
    # Header is 12 bytes (3 * 4-byte uints)
    keyword_index_top = 12 
    keyword_data_top = keyword_index_top + (size_keyword * 4)

    # Build header
    header = struct.pack('<III', size_keyword, keyword_index_top, keyword_data_top)

    # Build index part
    for offset in offsets:
        index_part += struct.pack('<I', offset)

    # Combine all parts
    binary_data = header + index_part + data_part

    # Write to C header file
    with open(output_file, 'w', encoding='utf-8') as f:
        f.write(f"// Generated from {input_file} by skk_dict_converter.py\n")
        f.write(f"// Total entries: {size_keyword}\n")
        f.write(f"// Data size: {len(binary_data)} bytes\n\n")
        f.write("const unsigned char embedded_skk_dict[] = {\n")
        
        for i, byte in enumerate(binary_data):
            if i % 16 == 0:
                f.write("    ")
            f.write(f"0x{byte:02x},")
            if i % 16 == 15:
                f.write("\n")
        f.write("\n};\n")

if __name__ == "__main__":
    convert_skk_dict_to_c_array("test_skk_dict.txt", "test_skk_dict_data.h")
