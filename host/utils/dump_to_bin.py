# consolidate one or more dump output files from the emulator into a single
# binary file (useful for running through a disassembler.)

import os
import sys

if len(sys.argv) < 3:
    print("Usage: {} <output file> <file 1> <file 2> ... <file N>".format(
        os.path.basename(sys.argv[0]), sys.argv[0]))
    sys.exit(1)

OUTPUT_FILENAME = sys.argv[1]
INPUT_FILENAMES = sys.argv[2:]

output_file = open(OUTPUT_FILENAME, mode='wb')
for input_filename in INPUT_FILENAMES:
    with open(input_filename, 'r') as input_file:
        for line in input_file:
            _, data = tuple([part.strip() for part in line.split(':')])
            output_bytes = bytes.fromhex(data)
            output_file.write(output_bytes)
