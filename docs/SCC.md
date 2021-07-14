# Serial Communications Chip Emulation

https://ia801901.us.archive.org/30/items/bitsavers_zilogdataBUSSCCZ8530SCCSerialCommunicationsControl_13081094/00-2057-03_Z8030_Z-BUS_SCC_Z8530_SCC_Serial_Communications_Controller_Technical_Manual_Sep1986.pdf

http://www.1000bit.it/support/manuali/apple/technotes/iigs/tn.iigs.018.html



### Frontend
* Two channels A and B
* Each channel communicates via a Command and Data register

### Backend
* 9 read registers + 15 write registers per channel


## Notes

* 2 Serial Ports on the IIgs system
  * provide sync/async communications
  * channel A and channel B
* 7 pins per port (+ GND = 8)
  * DTR
  * HSKI (handshake in)
  * TX data -
  * TX data +
  * RX data -
  * RX data +
  * GPI (general purpose input)
* Apple II emulation limited
  * asynchronous
  * ACIA not supported (the older serial interface)
* Zilog 8530 serial communications controller
* Each port has two MMIO registers
  * Command
  * Data
  * Table 7-11 has a list of SCC read register functions
  * Table 7-12 has a list of SCC write register functions
* Controller hardware I/O 8530
  * Channel Select (A or B)
  * Chip Enable - SCC is active when enabled
  * Data (8-bits) bidirectional I/O
  * Control/Data I/O flag
  * Read flag and Write flag (combined = reset)
  * Many of the following are per channel
    * CTS (clear to send) to port
    * DCD (data carrier detect input)
    * DTR (data carrier detect output)
    * RTS (request to send outputs)
    * RTxC (recv/xmit clocks input)
    * RxD (recv data inputs)
    * SYNC (synchronization input or output)
    * TRxC (xmit/recv clocks input)
    * TxD (xmit data output)
    * W/REQ (wait/request outputs)

* Accessing registers
  * Set current register
  * Read or Write to register
  * communication modes are established by programming write registers
  * as data is received or transmitted, read register values may change
  *