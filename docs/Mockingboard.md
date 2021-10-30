# Mockingboard

## Milestones

* Reference Null Implementation
* Detection Code

## Architecture

### Use the external card slot system

See `ClemensCard` inside `clem_types.h`.  Like all card devices, the interface
uses the memory I/O and sync pattern common to other Clemens MMIO subsystems.

### General Mockingboard Technical

* The PSG (Programmable Sound Generator) has three tone oscillators
  * Accessible via channels A, B, C
  * NG (Noise Generator)
  * It's a AY-3-8910 chip
* The target Mockingboard will be a 2 PSG device
* There is one output per PSG/6522
* There are two outputs to the Mockingboard
* From the Apple II ($C400/$C480) MMIO access
  * Sends commands to a 6522 that then directs commands to the PSG
  * These registers map to 6522 registers
    * ORA
    * ORB
    * DDRA
    * DDRB
* 6522 specific operations should also be supported
  * RS0-RS3 allow access to the above registers plus some additional 6522
    specific functions
    * Interrupts
    *



### Clemens Integration

* io_sync Execution of synth loop occurs and IRQ simulation
* io_write will apply output to the synth engine
* io_read ???

