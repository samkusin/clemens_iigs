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


## LSS

Jim Sather's Output of the Disk II LSS
```
//                Q7 L (Read)                                     Q7 H (Write)
//    Q6 L (Shift)            Q6 H (Load)             Q6 L (Shift)             Q6 H (Load)
//  QA L        QA H        QA L        QA H        QA L        QA H        QA L        QA H
// 1     0     1     0     1     0     1     0     1     0     1     0     1     0     1     0
0x18, 0x18, 0x18, 0x18, 0x0A, 0x0A, 0x0A, 0x0A, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, // 0
0x2D, 0x2D, 0x38, 0x38, 0x0A, 0x0A, 0x0A, 0x0A, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, // 1
0xD8, 0x38, 0x08, 0x28, 0x0A, 0x0A, 0x0A, 0x0A, 0x39, 0x39, 0x39, 0x39, 0x3B, 0x3B, 0x3B, 0x3B, // 2
0xD8, 0x48, 0x48, 0x48, 0x0A, 0x0A, 0x0A, 0x0A, 0x48, 0x48, 0x48, 0x48, 0x48, 0x48, 0x48, 0x48, // 3
0xD8, 0x58, 0xD8, 0x58, 0x0A, 0x0A, 0x0A, 0x0A, 0x58, 0x58, 0x58, 0x58, 0x58, 0x58, 0x58, 0x58, // 4
0xD8, 0x68, 0xD8, 0x68, 0x0A, 0x0A, 0x0A, 0x0A, 0x68, 0x68, 0x68, 0x68, 0x68, 0x68, 0x68, 0x68, // 5
0xD8, 0x78, 0xD8, 0x78, 0x0A, 0x0A, 0x0A, 0x0A, 0x78, 0x78, 0x78, 0x78, 0x78, 0x78, 0x78, 0x78, // 6
0xD8, 0x88, 0xD8, 0x88, 0x0A, 0x0A, 0x0A, 0x0A, 0x08, 0x08, 0x88, 0x88, 0x08, 0x08, 0x88, 0x88, // 7
0xD8, 0x98, 0xD8, 0x98, 0x0A, 0x0A, 0x0A, 0x0A, 0x98, 0x98, 0x98, 0x98, 0x98, 0x98, 0x98, 0x98, // 8
0xD8, 0x29, 0xD8, 0xA8, 0x0A, 0x0A, 0x0A, 0x0A, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, // 9
0xCD, 0xBD, 0xD8, 0xB8, 0x0A, 0x0A, 0x0A, 0x0A, 0xB9, 0xB9, 0xB9, 0xB9, 0xBB, 0xBB, 0xBB, 0xBB, // A
0xD9, 0x59, 0xD8, 0xC8, 0x0A, 0x0A, 0x0A, 0x0A, 0xC8, 0xC8, 0xC8, 0xC8, 0xC8, 0xC8, 0xC8, 0xC8, // B
0xD9, 0xD9, 0xD8, 0xA0, 0x0A, 0x0A, 0x0A, 0x0A, 0xD8, 0xD8, 0xD8, 0xD8, 0xD8, 0xD8, 0xD8, 0xD8, // C
0xD8, 0x08, 0xE8, 0xE8, 0x0A, 0x0A, 0x0A, 0x0A, 0xE8, 0xE8, 0xE8, 0xE8, 0xE8, 0xE8, 0xE8, 0xE8, // D
0xFD, 0xFD, 0xF8, 0xF8, 0x0A, 0x0A, 0x0A, 0x0A, 0xF8, 0xF8, 0xF8, 0xF8, 0xF8, 0xF8, 0xF8, 0xF8, // E
0xDD, 0x4D, 0xE0, 0xE0, 0x0A, 0x0A, 0x0A, 0x0A, 0x88, 0x88, 0x08, 0x08, 0x88, 0x88, 0x08, 0x08  // F
```

addr = 0
latch = ? (data byte)
if bit_in then addr=0 else addr = 1
if latch:7==1 then addr |= 0x02
if q6:hi(load) then addr |= 0x04
if q7:hi(write) then addr |= 0x08
addr |= (state << 4)

command = rom[addr]
state = (command >> 4)

0x0 CLR = clears latch to 0
0x8 NOP = no-op
0x9 SL0 = shift left, bit 0 = 0
0xA SR = shift right, if readOnly then latch |= 0x80
0xB LD = load latch with data on bus
0xD SL1 = shift left, bit 0 = 1

execute two instructions per cycle (2mhz on a 1mhz speed = 2 instructions per 1us)