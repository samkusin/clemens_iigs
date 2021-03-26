
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

### API_FWR_7629_TOOLBOX

This is used by the Monitor firmware as well.  Y = {Function}

#### Function $A3

* SCC initialization?
* Extra $E1 variable initialization and setup?
* v = 1
* SUB_TMP_API_7629_A3_PRAM_REFRESH?
  * v = 1; X = 0; Y = 0; A = ??
  * SAVE A0
  * (0) A = Y
  * SAVE A
  * (0) A = A & $E0
  * (0) A >>= 5         (bits 7-5) lower (CRC?)
  * ($38) A |= $38
  * ($B8) A |= $80
  * (?, $B8) A <-> B
  * (0, $B8) RESTORE A
  * (0, $B8) A = A & $1F
  * (0, $B8) A <<= 2    (bits 4-0)       (CRC?)
  * ($B8, 0) A <-> B
  * Call SUB_TMP_RTC_CHECK
  * (0, $B8) A <-> B
  * Call SUB_TMP_RTC_CHECK
  * RESTORE A0
  * Call SUB_TMP_RTC_CHECK2?

##### SUB_TMP_RTC_CHECK

* v = 0 (nop for SUB_TMP_RTC_CHECK2), A = data
* Read or write to RTC chip ->
  * Data <- A
  * ... TBD ...
  * A <- Data