
# The RESET sequence for the Apple IIgs

1. Set address bit 17 to point to bank 01 using Bit 0 of NEWVIDEO (seems to be
   always true, so this is just to assert this is the case on the hardware)
2. Reset the stack pointer to $01FB
3. Invoke a subroutine to perform some (likely?) cold-boot first-time
   initialization

## RESET Internal Subroutine (SUB_RESET_INTERNAL)

1. Set up some ENTRY points for toolbox-like calls made by the monitor firmware
   1. E1V_FF7629
   2. E1V_FFA9AA
2. Invokes a ENT_FWR_7629_TOOLBOX ($A3)
      1. Ensures Native Mode + Fast Speed + 8-bit registers
      2. Invokes API_FWR_7629_TOOLBOX which runs in native mode

## BRAM Storage

https://www.techtalkz.com/threads/any-raw-data-specs-for-the-apple-iigs-bram.189163/page-2#post-794483

According to Michael Fischer on page 74 of "Apple IIgs Technical
Reference" (Osborne McGraw Hill ISBN 0-07-881009-4):

"The values in the BatteryRAM are also stored, on a system startup, in
a buffer at $E1/02C0-03BF. When the Control Panel is called, it
ensures that the current BatteryRAM values are placed in the buffer.
Changes made by the user in the Control Panel are made to the values
in the buffer. When you quit the Control Panel, the changes are stored
in the BatteryRAM."

### API_FWR_7629_TOOLBOX

This is used by the Monitor firmware as well.  Y = {Function}

#### Function $A3

Likely BRAM validation check on boot


## Data

### $2C0-$3BF : BRAM and 32-bit checksum at end
