# Interrupts

See Table 4-1 of IIgs Technical Reference and the Firmware reference for a
summarized full list of interrupts supported.

## Interrupt Handling

### Key Items

- Reset takes priority
- NMI and ABORT likely will not be implemented
- Devices bring IRQB low
- Many devices allow ISR to clear the interrupt bit
- Should use the clocks_spent component to determine timing
- Verify correct BRK/COP handling

### Method

- Every device 'raises' an IRQ (IRQ signal level low on the CPU)
- Every device fills in the data needed from the host system so the CPU can
  access it on next execute()
- Either the device or the CPU can clear the interrupt status
  - VGC may only be cleared by the app?  Maybe not?
- Keep a mask of interrupts enabled per frame for debugging purposes
- CPU needs to perform correct ops on an IRQ low if interrupts are not disabled
- First pass implementation order
  - 1 second timer (VGC/RTC device)
  - 0.25 timer
  - ADB + Mouse

### Important MMIO registers

- C023 (VGC interrupt enable switches, status)
- C032 (VGC interrupt clear switches)
- C026 (ADB interrupt SRQ status)
- C027 (ADB interrupt switches, status)
- C041 (Mega 2 interrupt switches, Mouse, VBL, quarter sec)
- C046 (Mega 2 interrupt statuses)
- SCC and Ensoniq TBD

```python
def execute():
    # CPU
    handle_interrupts()
    cpu_execute()
    update_time()

    # devices - this is not an accurate reflection of how device will be
    #   handled per frame. instead devices are listed here so we're aware that
    #   at some point they are processed.
    #
    # some of these 'devices' will not actually be polled but instead our API
    # will trigger interrupts, and the CPU will request data on demand for
    # efficiency purposes.  polling would introduce overhead where timing is
    # is critical, since the app will call execute() *per* instruction.
    scc()
    video()
    ensoniq()
    cards()
    iwm()               # no IRQs?
    adb()
```

### To Implement

- 0.25 second (0.26667?) timer
- Mouse
- ADB Response
- ADB SRQ
- 1 second VGC/RTC Timer
- VBL (http://www.1000bit.it/support/manuali/apple/technotes/iigs/tn.iigs.040.html)


### Quirks?

- "A taken branch delays interrupt handling by one instruction?"
http://forum.6502.org/viewtopic.php?f=4&t=1634

### VGC

- VGC (http://www.1000bit.it/support/manuali/apple/technotes/iigs/tn.iigs.039.html)

### Do not implement

- Keyboard (references say this will not work)
