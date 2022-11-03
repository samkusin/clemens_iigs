## Release Workbook

### Build

* Build Windows and Linux versions with their default settings
  * Windows - D3D11
  * Linux - OpenGL
  * Debug and Release executables
* Ensure all suppported platforms' release binaries launch to startup

### Sanity Testing

All of the below should "feel" right For example, CTRL-DELETE at the Check
Startup Disk screen should show some blinking text since the ROM will change
character sets before displaying the Applesoft prompt.

#### Simple Video Mode Tests

* CTRL-DELETE to Applesoft BASIC prompt
* Diagnostic Test
  * This currently does not pass as we're still missing some features
* Test 80-column mode - PR#3
* Test 40-column mode - PR#0
* Test Lores and Double Lores Modes
  * GR
  * COLOR=?; HLIN 0,39 AT 0
  * PR#3; POKE 49246,0; HLINE 0,79 AT 24
  * TEXT
  * PR#0
* Test Hires
  * HGR
  * HCOLOR=?; HPLOT 0,0 to 279,191
  * TEXT
* Double Hires
  * TBD, but will depend on title testing for now
* Super Hires
  * For now depend on title testing

#### Title Tests I

These should be performed with an empty folder containing ONLY the executable to
ensure a clean-slate before the following tests.  It's not necessary to clear
the data folder for *each* title - but just before the first one.

Windows takes priority.  For Linux, perhaps run a subset of these.

* Disk Import and eject
  * At least one title from the following supported formats
    * WOZ
    * DSK
    * DO
    * PO
    * 2MG
  * Do this for the below titles
* DOS 3.3 Master Disk startup
* ProDOS 8 Master Disk startup
* ProDOS 16 Master Disk startup
* GS/OS System 5 Disk startup
  * System 6 will require Smartport support which is not ready yet
* Oregon Trail Disk startup (5.25" WOZ simple copy protection should pass)
* Neuromancer Disk startup
  * Audio and Visuals should look right
  * DOC Debugging Window should show proper activity
* Ultima IV (Remaster) Disk startup
  * Ensure system speed set to 'Slow' (Mockingboard music will play poorly
    otherwise as expected.)
  * Ensure Slot 4 is set to 'Your Card'
  * Mockingboard is OK
  * Check Write Protect toggle to verify disk writes work
* Ultima V Disk Startup
  * Mockingboard currently will not work (as expected with ROM 3?)
  * More complex disk/copy protection should be working

Success gives confidence that some baseline GS functionality is there:

* IWM 5.25" and 3.5" modes work with copy protection
* Hires and Mega 2 functions work - bank switching, etc.
* Super hires mode 320 and 640 modes work
* Keyboard and Mouse input works

### Title Tests II

* DOS 3.3 Master Disk
  * FORMAT blank disk
  * SAVE and LOAD a simple BASIC program
* ProDOS 8/16/GSOS
  * FORMAT a blank 3.5" disk using system utilities disk
* Choose an 8-bit Apple II game and play a little with it
* Choose Neuromancer, Kings Quest IV or another Apple IIgs title and play a bit

This list is not comprehensive and may just be considered "Sanity Plus" testing.

### Debugging

* No ROM testing (proper error at least.)
* Rebooting
* Mouse lock/lock input to emulator
* Breakpoints
* Step through instructions
* Dump memory
* Debug Trace file export
* Save and Load Snapshot under various titles (this might included in sanity
  tests to save time)
* BRAM - delete
  * Change settings in Control Panel
