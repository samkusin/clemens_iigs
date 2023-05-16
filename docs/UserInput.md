# User/Host Input to Emulator

The underlying I/O system of the host device forwards inputs to the emulator. 
The Apple IIGS supported keyboard, mouse and joystick inputs acquired from
ADB and legacy (Apple II) devices.  These devices are emulated through the ADB
layer implented in `clem_adb.c`.

## Keyboard

Keyboard press/release events with modifier key flags are passed to the 
emulator. These in turn are stored on a queue managed in `clem_adb.c`.  The 
events are fed to the legacy $C000 and $C010 registers as well as the new ADB 
registers `CLEM_MMIO_REG_ADB_x`

## Joystick

Axis and buttons inputs from the host game device APIs are forwarded to the
emulator.

Though the events are filted by the 'ADB' emulation layer, the events
are actually fed through a very simple equation the generates on/off signals.
These sginals are then translated to values on I/O registers 
`CLEM_MMIO_REG_PADDLx` and `CLEM_MMIO_REG_SWx` for axis and button switch
inputs respectively.

Up to two joystick host devices are currently supported.  Whether four single
axis controllers will be supported in the future depends on need.

## Mouse

Two mouse input sources are exposed to Apple IIgs applications: ADB and 
Peripheral Slot I/O.  On the ADB side, mouse inputs either originate from the
ADB mouse or in ROM 3 via keypad events if enabled.  

Finally mouse events are  fed by the host to the emulator via two methods: as 
deltas *and* absolute positions on the emulated screen.  

The former method of using deltas relies on the host  obtaining reliable
movement deltas which can be obtained using host APIs if available, or brute
force polling before and after pointer positions and resetting the mouse
pointer to center of screen.

A second method overcomes some limitations of the above described first method.
Having the host's mouse pointer locked to the center of screen results in
users having to toggle mouse lock on their own.  This can be cumbersome when
having to move back and forth between the emulator and the client machine's GUI.  

This second method injects *absolute* mouse positions directly into the client
machine's memory so that the Apple IIGS toolbox can use these positions in its
calls to `ReadMouse`.   The host's mouse pointer is still hidden while inside
the machine view.  But there is no mouse-lock on the host, allowing for 
relatively seemless integration between machine and host GUIs.

```
Host                View                Machine
--------------------------------------------------------------------------------
Host(X,Y)           View(X,Y)           IIGS: X=[0xE10190/2],Y=[0xE10191/3]
                                        //c: X=[0x000478/578], Y=[0x0004F8/5F8]
                                             Status=[0x0000778]
                                             Mode=[0x00007F8]
                                             Equivalent E0/E1 locations
                                             + Slot Number (4 always) for each
                                               address

How calls to `ClampMouse`, `HomeMouse` and `PosMouse` are handled will depend
on practical usage.
```

### Mouse References

#### Apple II firmware 
* https://www.applefritter.com/appleii-box/APPLE2/AppleMouseII/AppleMouseII.pdf

