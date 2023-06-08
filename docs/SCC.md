# SCC Zilog 8530 Emulation for the Apple IIGS

## Serial Communications

### Basics

Three simple 'wires' connecting two devices - Transmit, Receive and Ground - 
represent a minimal serial connection.  Ground (GND) is ignored for emulation
purposes.  Transmit (TxD) and Receive (RxD) lines perform just those tasks for a
serial endpoint.

The SCC controls the signaling on these wires with additional pins.  To control
these pins, applications send commands to the SCC via I/O register access.

The Zilog 8530 provides two serial channels A and B.  On the Apple IIGS, these
channels are used for Slot 1/Printer (A) and Slot 2/Modem (B) control.

### Control

Basic flow control is offered through the Zilog 'Modem Control Logic' unit for
each channel.  As mentioned above, these pins are both read and controlled 
using the command interface.  The Clemens emulator calls such interfaces,
"GLU's" - glue? - borrowed from various IIGS references.

#### Signals

Signal names followed by emulated port pin with peripheral.

* TxD (DB8-3) - transmit bit at current baud rate
* RxD (DB8-5) - receive bit at current baud rate
* RTS (DB8-7) - request to send (send me more data so I can receive it)
* CTS (DB8-7) - clear to send (i should transmit more data when possible)
* DTR (DB8-1) - data terminal ready (signals to initiate comms)
* DSR (DB8-2) - data set ready / handshake (signals to another computer that it's ready)
* DCD (NONE) - carrier signal from a modem 
  - Zilog supports this for RS-232
  - IIGS port doesn't have this connection

The Zilog supports more signals that may be unused by the IIGS (i.e. like DCD.)
Implementation will depend on practical need.

## Interrupt and Timings


## References

https://ia801901.us.archive.org/30/items/bitsavers_zilogdataBUSSCCZ8530SCCSerialCommunicationsControl_13081094/00-2057-03_Z8030_Z-BUS_SCC_Z8530_SCC_Serial_Communications_Controller_Technical_Manual_Sep1986.pdf

http://www.1000bit.it/support/manuali/apple/technotes/iigs/tn.iigs.018.html
