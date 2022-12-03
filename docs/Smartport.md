# Smartport on the Apple IIgs

All disk I/O calls from ProDOS or GS/OS, regardless of them targeting a native
ProDOS block device (i.e. the Apple Disk 3.5") or a Smartport device, use the
Smartport call interface.

Under the hood, these calls are forwarded to the appropriate firmware handling
either ProDOS block devices or the Smartport device.  On Port 5 for example,
calls are made to an entry function located somewhere in the internal firmware
located in the $C500 block (see the Apple IIgs Firmware Reference Chapter 7 for
specifics.)

From here on, we'll assume calls are made using entry points at the above
mentioned $C500 firmware space.

## From IWM to Device

Since the internal firmware at $C500 interfaces with the disk I/O physical port
using the IWM IC, the emulator will handle calls to various drives at the
same emulator subsystem (located at `clem_iwm.c`.)

Traditional disk devices on the Apple II offloaded most of its controller
logic to application code.  The application code operated these disks using
memory mapped I/O operations that in turn controlled the IWM.  Finally the IWM
sent signals to the disk port and to the simple (a.k.a 'dumb') controller IC on
the Disk II which managed the stepper motor that controlled the drive arm as
well as the spindle motor.

The Apple Disk 3.5" introduced *slightly* more complex controller logic on the
drive-side.  Commands were sent to the disk drive through a combination of
setting the legacy stepper motor + drive select pins.  These commands moved
the drive arm and operated the spindle like the legacy Disk II.  Added commands
handled disk ejection and a few other queries.

When Smartport devices were introduced, the hardware designers had to leverage
the existing IWM.  So through a combination of certain IWM state sets and
commands, applications can read data from or write data to a Smartport device
without conflicting with I/O to the Apple Disk 3.5 (on Port 5.)

This approach works - but has problems.  For one, in some cases this approach
won't work on slot 6 where legacy Disk II 5.25" disk drives operate.  Because
most 5.25" software employs copy protection by operating the drive arm in
"non-standard" ways, certain stepper motor bit combinations must work on
such drives that would otherwise be forwarded to a Smartport device.
