# Smartport Emulation of HDDs

Traditional Smartport Harddrives attached to the drive cable and accessed via
Slot 5 firmware have the following limitations:

* Cannot be booted from
* Bit transfer at emulated machine speeds results in slow data transfer

Emulators like KEGS implement their own firmware for an emulated Smartport 
drive that boots from a slot other than 5 (i.e. Slot 7).   The firmware 
is wired directly into the emulator so that block transfers can be done at
the host machine's speed.   These improvements solve the two limitations noted 
above.

Fortunately the IIGS Firmware reference documents Smartport firmware technical
requirements, so that implementations can start from there.  Some ROM listing
is also required to understand the finer points of how the firmware reads
command parameters and treats the ProDOS firmware entry point vs the Smartport
one.

From this documentation, one can write a custom firmware for a Slot 7 card.

## Emulator Changes

The emulator itself must handle actual data transfer.  This requirement is in
contrast to Slot 5 Smartport firmware which relies on the IIGS ROM to manage
the handshaking back and forth from the IWM (and results in slow data transfer
and the Slot 5 boot limitation.)

To do this, our custom firmware will invoke custom 65816 instructions via the
`WDM` opcode.  The CPU emulator will forward the request to an appropriate 
implementation.  Basically this is a form of DMA where the emulator on a write
operation copies data from IIGS memory to the emulated Smartport block device.
The reverse direction is true for read operations.

## Procedure (Boot)

* Select 'Your Card' on the IIGS control panel for Slot 7.
* Map custom firmware page to $C700
* ROM firmware checks $C700 for the appropriate Smartport byte signature
* ROM firmware switches control to our custom boot firmware for Slot 7 
* The custom firmware loads the boot blocks into memory, hitting the custom
  `WDM` instructions to load block data

## Procedure (MLI)

* ProDOS will forward MLI requests to the appropriate ProDOS entry point
* Our custom firmware handles this entry point and block transfer per the 
  procedure described above

## WDM Implementation

The `WDM` is a two byte instruction meant for custom opcodes.  Originally meant
for future 65816 architectures, it may be safe at this point to treat `WDM` as
a way to extend emulator behavior from IIGS applications.

The second byte is a way to define a custom instruction.  The emulator detects
`WDM` and the following byte. 

* Byte 2 = `0x01` is currently implemented for debugging
* Byte 2 = `0x02` will be our custom instruction for HDD control
* Emulator will pull bytes from code that define
  * Command Type (8-bit) = bit 7 hi indicates an 'extended' instruction which
    allows for access to the full range of IIGS memory
  * 24-bit Pointer to memory (order: lo, hi, bank), bank ignored for standard
    instructions, points to 0x00


