# Apple IO registers

## Essential


### $C015 SLOTCXROM

Read only - 1 = slot ROM enabled, 0 = internal ROM enablaed
See STATEREG for details


### $C029 - NEWVIDEO

Bit 0 - always assume it's going to be '1' for now - meaning the 17th
        address bit is ignored - if it ever changes to '0' in code, decipher
        the code to see what it's trying to do


### $C02D - SLOT

Bit N - slot N device select (#3 and #0 are labeled DO NOT MODIFY)

If Bit N == 0, select internal device
If Bit N == 1, select external device (slot ROM address space $Cx00 and
        corresponding I/O space $C090 to $C0FF (16 byte line per slot)

### $C035 SHADOW

FPI Register

Shadow register.
Bit 6 - IOLC (IO/Language Card) Off = I/O LC, On = RAM area - seems should
        always be off for proper operation according to docs

### $C036 SPEED

FPI Register

- Bit 7 - CPU speed
- Bit 6 - Power status (R/W) 1 when powered on, 0 n/a
- Bit 5 - Do not mofify
- Bit 4 - Shadowing enabled for all RAM banks vs just 00,01 - For the IIgs
        - should be off, with no real value emulating this when on
          - guessing when on, bank 00/02/04...$7E = $e0, bank 01/03/... = $e1
- Bit 3,2,1,0 Slots 7-4 disk motor on/off detect


### $C068 - STATEREG

* Fast Read
* Slow Write

### Slot ROM select

* Fast Read
* Slow Write



## Memory Bank 0 and 1 Softswitch behavior

The shadow register $C035 affects behavior on banks 0 and 1.  For example,
IOLC affects both banks.  But Bank 0 effects must also take softswitches
(i.e. ALTZP) behavior into account.  So access to Bank 01 from the CPU
standpoint is affected, as well as Bank 00 + softswitch flags from FPI/Mega2.



