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



### WOZ File Format



