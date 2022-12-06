# Smartport on the Apple IIgs

All disk I/O calls from ProDOS or GS/OS, regardless of them targeting a native
ProDOS block device (i.e. the Apple Disk 3.5") or a Smartport device, use the
Slot 5 MLI.   This MLI provides two entry points for operating systems or
applications that bypass the ProDOS application layer; the ProDOS and Smartport
device drivers.

Under the hood, these calls are forwarded to the appropriate firmware handling
either ProDOS block devices or the Smartport device.  On Port 5 for example,
calls are made to an entry function located somewhere in the internal firmware
located in the $C500 block (see the Apple IIgs Firmware Reference Chapter 7 for
specifics.)

From here on, we'll assume calls are made using entry points at the above
mentioned $C500 firmware space.  Also this document focuses on devices attached
to an Apple IIgs, but it's likely most of this is applicable to older Apple IIs
with relevant firmware and or cards.


## Disk I/O and the IWM In a Nutshell

Later Apple II hardware support a slot-agnostic interface to devices physically
connected to the external disk I/O port.  Apple //c and IIgs systems supply this
physical port to daisy-chain both 5.25" and 3.5" drives.  For the //c this is
restrict to the Unidisk 3.5" due to a lack of 'fast' access.

```
              -----
+-----+       | P |     +-------------+                  +--------------------+
| IWM |.......| O |=====| Apple       |==================| 5.25" Disk II      |
|     |       | R |     | 3.5" Drives |                  | Drives             |
+-----+       | T |     +-------------+                  +--------------------+
              -----
```

Application programmers used to thinking of "Slots" 5 and 6 as physical card
slots instead should consider these *virtual* slots accessed via the IWM
(Integrated Woz Machine).  The IWM itself is evenetually connected to the Mega
II/MMC which allows applications to control disk I/O access independent of
physical port.

Programmers used to accessing Disk I/O via the original `BIT $C080,X` type
instructions where `X=#$60` can continue to do so (where signals are passed
to the IWM vs the Disk I/O card.).  This is how old Apple II+/IIe software can
run on a IIc and IIgs without physical card slots for Disk I/O.

## Smartport In a Nutshell

Like IDE, SCSI and later USB, Smartport allows multiple devices to coexist on
the same physical bus.

Smartport is a legacy Apple standard for sending I/O between several devices
on a single physical disk port.  As far as I know was used solely on Apple II
devices specifically to allow Apple IIs to use multiple drive types beyond the
Disk II (and Apple 3.5 Disk.)  At the same time, with Smartport it is possible
for all of these drive types to coexist on the same physical connection, despite
Disk II devices themselves not being Smartport compliant.

Unlike Disk II which offers direct access to the drive's motor and arm,
Smartport aware devices typically implement a series of commands on the device
that handle this for the host.   These commands are issued by the OS to read,
write data and query state.

NOTE: Certain devices like the Apple 3.5" operate outside of the Smartport
standard.  They have some "smart" features yet operate with less overhead and
simpler onboard logic that Smartport devices like the Unidisk 3.5.


## How Smartport Works

Smartport relies on compliant devices that cooperate with others on the
physical disk port.  Devices are daisy-chained as described below.  This
chaining allows devices farther up the chain to block I/O to devices down the
chain.

```
              -----
+-----+       | P |     +-------------+    +-----------+      +---------------+
| IWM |.......| O |=====| Apple       |====| Smartport |======| 5.25" Disk II |
|     |       | R |     | 3.5" Drives |    | Drive     |      | Drives        |
+-----+       | T |     +-------------+    +-----------+      +---------------+
              -----
```

For example, when the Smartport Drive is functioning, I/O to the Disk II is
blocked until the Smartport Drive releases its lock on the I/O lines.  As a
consequence, this  means that any device before the Smartport can intercept
communication on the bus.  This limitation requires that the host strictly
controls what device is active and when it's accessed.  This control logic
exists on the Smartport firmware or software device driver.  ProDOS by design
abstracts the enumeration of such devices, and its arbitration between user and
firmware.

The firmware sends signals on the bus to inform Smartport devices that it's
ready to read or write data.  These signals are specifically selected to *not*
conflict with Disk II operation.  When the Smartport bus comes online, the
signals that communicate this state have no practical effect on the Disk II
drive.   The Disk II will not receive signals as long as the bus is held by
the Smartport device and host.

Note that the IWM uses a special data line to activate the Apple 3.5 Disk.  This
line was added in later hardware (Apple IIgs and the //c+) to support the
Apple 3.5 Drive.  Smartport devices generally did not use the 3.5 line and
in the case of the Disk II was used as a ground line.  Likely other Smartport
devices do the same.  This is why the Apple 3.5 drives are listed first in the
daisy chain diagram above as to prevent other devices blocking the 3.5 line
signal.

This design allows Disk II drives to coexist with Smartport devices.  The
details of why are discussed in the next section.

## Technical

### Bus Arbitration

### I/O



## References

Apple IIgs Firmware Reference

Apple IIgs Hardware Reference

http://mirrors.apple2.org.za/apple.cabi.net/FAQs.and.INFO/DiskDrives/drive.order.txt
