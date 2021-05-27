# IWM Emulation

## Control registers

Accesssed from C0E0-C0EF (slot 6 I/O.)

## Hardware Simulation

* No IWM Sync function
  * Synchronous handling per command sent to the IWM via MMIO
  * Too time critical to dedicate its own handler in the clemens_execute
    function
* Track and maintain the head position
* Handle phase/step motor simulation
  * Tracks and half tracks (and quarter tracks?)
  * Simulate timing, lookup optimal delay between cog rotation
  * Momentum value effects timing, if it's important simulate this
* Motor on/off
  * 1 second on to full rotation
  * 1 second before going to off
* Read and Write
  * When starting a read/write, calculate the position on the track based on
    timing
  * 200 ms per rotation (300 RPM)
  * random bits as described in the WOZ spec

## Integration

### State Machine

* Drive System state RESET : for each drive
  * If drive disabled transition to DISABLE
  * Else transition to IDLE
* DISABLE state
  * Motor is off
  * If enable action and transition to IDLE
  * If write to odd ioreg
    * If Q6, Q7 ON, set mode
  * If read from even ioreg
    * If Q6 ON, Q7 OFF, return status
    * If Q6 OFF, Q7 ON, return handshake
* IDLE state
  * Motor is on
  * If disable action and transition to DISABLE
  * If write to odd ioreg
    * If Q6, Q6 ON, write data
  * If read from even ioreg
    * If Q6, Q6 OFF, read data
    * If Q6 ON, Q7 OFF, return status
    * If Q6 OFF, Q7 ON, return handshake

### Disk Drives

As best as it can, emulate the disk port (except that 3.5 and 5.25 are separate
ports, and only one is active as controlled by the IWM)

Series receive the following inputs, sent to the drive based on drive index 1/2

* Inputs
  * "power" = on/off
  * 4-bits (5.25 = stepper)
  * write switch
  * R/W data bit
  * drive select
  *

### WOZ File Format

* Basic WOZ 2 File Reads
* IWM integration
* Basic WOZ 2 Writes
* Finish WOZ 1 File Handling

### Technical Approach 5.25"

* Use microsecond emulation timer?  or nanosecond emulation timer?
* Fine tune precision of track index via magnet states for each phase (0-3)
* Support quarter tracks by checking if adjacent phases are ON
* Simulation cw/ccw motion of stepper to move track head inside or outside
  * Verify correlation between phase 'direction' and physical head movement
  * Phase 3,2,1,0
  * direction is null
  * Phase 1 ON, 0 OFF = positive direction
  * Phase 3 ON, 0 OFF = negative direction
  * If Phase 0,1 or 1,2 or 2,3 or 3,0 are ON (exclusive), then its a quarter track movement (vs a half/full track movement)
  * Else it's a half/full track movement if direction != null
  * since there are 16 possible states here, we can probably put this in a table
  * if there's been a change in the magnet coil state, then we will change the head
    * easy to do with a 4-bit mask (phase 3,2,1,0)
    * since we index by quarter track, increments will be:
      * 0, +-1, +-2 (none, quarter, half)
      * this is because to move a full track, requires a two step sequence that will be executed by application code
      * so, exclusive adjacent phases on will increment or decrement by 1
        * 0110, 0011 = negative quarter track
        * 0110, 1100 = positive quarter track
      * and adjacent on/off will increment or decrement by 2
        * 0001, 0010 = increment 2
        * 0001, 1000 = decrement 2
  * Disk motor simulation
    * one second to full spin-up or spin down
    * allow reads/writes while disk is moving
      * 200ms per rotation (300 RPM)
      * even when IWM has turned it off but not yet at 0%
      * even when IWM has turned it on but not yet at 100%
      * experiment with practical cases when the time comes
      * because spin-up will decrease read delay times until we reach optimal timing
      * and spin-down will increase read delay times until we reach optiming timing
      * again, implement timings and experiment when its needed (later, not now!)
    * when track head moves, calculate the effective track/sector offset per specified by WOZ
  * Read/Write simulation
    * 4-bit register
    * randomness added if more than two zeros in a row?  research (randomness of 30% vs 50%)
    * Read timing vs write timing and flux?
    * write protect state
    * read latch (read from IO register = data from disk and also IWM control state)
  * Notify emulator when track head hits track 0 > 1 time so we can simulate the clanking
  * Emulator GUI should show as much of disk state as possible

### Technical Approach: 3.5"


### Technical Approach: Smartport?