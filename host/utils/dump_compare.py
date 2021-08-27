INPUT_FILES = [
    'reference.txt',
    'memory_00.txt'
]

def create_memory_bytes(lines):
    memory = bytearray(65536)

    for idx, line in enumerate(lines):
        items = line.split(':')
        if len(items) != 2:
            raise RuntimeError(
                'Invalid memory line found line {}'.format(idx + 1))
        address = int(items[0], 16)
        hex_string = items[1].strip()
        byte_count = len(hex_string) // 2
        while byte_count > 0:
            byte_count -= 1
            hex_byte = hex_string[(byte_count * 2):(byte_count * 2) + 2]
            memory[address + byte_count] = int(hex_byte, 16)

    return memory


def diff_banks(ref_bank, test_bank):
    if len(ref_bank) != len(test_bank):
        print('Banks are of a different size')

    byte_limit = min(len(ref_bank), len(test_bank))
    byte_start = 0
    byte_end = 0

    while byte_end < byte_limit:
        if ref_bank[byte_end] == test_bank[byte_end]:
            if byte_start != byte_end:
                print('${:04X} to ${:04X}'.format(byte_start, byte_end - 1))
                hex_bytes_a = ref_bank[byte_start:byte_end]
                hex_bytes_b = test_bank[byte_start:byte_end]

                adr_start = byte_start
                while len(hex_bytes_a) > 0:
                    hex_string = ' '.join(['{:02X}'.format(v) for v in hex_bytes_a[0:16]])
                    print('A ${:04X}: '.format(adr_start) + hex_string)
                    hex_string = ' '.join(['{:02X}'.format(v) for v in hex_bytes_b[0:16]])
                    print('B ${:04X}: '.format(adr_start) + hex_string)
                    adr_start += min(16, len(hex_bytes_a))
                    hex_bytes_a = hex_bytes_a[16:]
                    hex_bytes_b = hex_bytes_b[16:]

                print('\n')

            byte_end += 1
            byte_start = byte_end
        else:
            byte_end += 1


memory_banks = []
for input_file in INPUT_FILES:
    lines = open(input_file, 'r').readlines()
    memory_banks.append(create_memory_bytes(lines))

# first file is the reference
# second file and later are the ones to compare against

if not memory_banks:
    raise RuntimeError('Requires at least two memory dump files')

reference_bank = memory_banks[0]
memory_banks = memory_banks[1:]
for test_bank in memory_banks:
    diff_banks(reference_bank, test_bank)
