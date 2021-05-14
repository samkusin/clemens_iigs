# IWM Emulation

## Control registers

Accesssed from C0E0-C0EF (slot 6 I/O.)

## Hardware Simulation

### IWM Sync

* Track and maintain the head position
* Handle phase/step motor simulation
  * Tracks and half tracks (and quarter tracks?)
  * Simulate timing, lookup optimal delay between cog rotation
  * Momentum value effects timing, if it's important simulate this
* Motor on/off
  * 1 second on to full rotation
  * 1 second before going to off
* Read and Write
  * When starting a read/write, select a sector on that track
    * DSK format - this is random
    * WOZ format - ???
  * During a read, write, I *think* we need to convert from bytes to nibbles
    * GCR format conversion - since I think DSK format is in byte format
    * Data is read in GCR format regardless, and the DOS will convert it

## Track and Sector Navigation

### DSK File Format

This format is an unnibblized 140K disk.  Since 5.25" disk formats are
nibblized (GCR), we'll need to ingest the raw data, encoding the data in GCR
nibble format that a 65xxx OS will read.

### WOZ File Format

TODO

## Technical Approach

### Part I: DSK drive emulation

* Ingest a DSK format, converting it to track and sector GCR nibbles
* 5.25" Disk Drive Emulation with Stepper Motor Phases and Spindles
* Test program that reads using the exposed registers
  * Test compares with the raw DSK format
* Test program that writes using the exposed registers
  * Test compares with the raw input DSK format


### Part II: DSK Drive and IWM

* A Boot Sector Zero application that loads additional tracks into memory
* Write a IWM state machine that accesses this disk drive
* Write a 6502 application that accesses the IWM
  * Loads tracks from boot sector 0

### Part III: Smartport 3.5" emulation


