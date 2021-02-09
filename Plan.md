# Clements GS

## Summary

The Clements GS emulator's goal is to *emulate* the Apple IIgs design (ROM3
with ROM1 if possible compatibility.)

## Technical

All control, fast memory and ROM is handled through the *EmulatorBus*.  The *EmulatorBus* in turn controls the 65C816, FPI(CYA?) and MEGA2 components.  The
*MEGA2* will be the primary interface to the I/O of the system.

### EmulatorBus

This component polls the CPU, executing until a certain amount of *clocks* has
passed, **or** when reading from or writing to memory.

Interrupts may also return control back to the EmulatorBus.

The *clocks* here refers to millihertz cycles.  Assuming no I/O is performed,
the CPU should run until we need to:

* refresh the display
* read from or write to memory
* handle a hardware interrupt
