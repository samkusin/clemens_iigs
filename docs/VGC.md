# VGC and Mega II Video Emulation

This document covers emulation of:

* Text Pages 1, 2 and GR modes
* HGR Pages 1, 2
* 80 column text
* Double Hires Mode
* Super Hires 320 and 640
* Interrupts and host rendering
* Various mixed text + graphics modes
* Border color
* Background color
* MouseText support
* Monochrome / Color switch

## Procedure

* Emulators must update the display upon a vertical blank signal from Clemens
* After the signal, the emulator should stop calling `clemens_emulate` and
  perform any post-processing of the graphics data from the emulated VGC
* Once the emulator is done with this video data, it can again call
  `clemens_emulate`

## Display Technical

Below is a brief overview of *each* video mode to be supported.  All display
memory is shadowed from $E0/$E1, but the VGC always reads from $E0/E1

### Text Modes

* All text modes can select between two character sets
* See SETALTCHAR IO switch to enable alternate set
* Support for inverse and flashing modes based on the character set chosen
* Much of this is exposed to the emulator in a way so it can select the font
  how set up shader parameters for inverse/flashing as needed
* Color text supported (IIgs)
  * IO Register $C022 - Screen Color


### 40 Column Text

* 40 x 24 ASCII cells
* Follows row-staggered pattern endemic to all Apple II modes
  * Memory order rows 0, 8, 16, 1, 9, 17, 2, 10, 18, etc...
  * There are gaps every three rows (0, 8, 16 are contiguous then 8 byte 'hole')
* Data in memory $E00400 - $E007FF (shadowed)
* Page 2 is $0800 - $0BFF

### 80 Column Text

* 80 x 24 ASCII cells
* Staggered columns between main and aux memory $E1/$E0 $0400-$07FF
  * Note aux memory is column 0, main is column 1, ...
* Otherwise the same principles for 40 column text
* Page 2 is not supported - the switch is used to activate the 'double res'
  display

### Lo-res Mode

* Each cell has two pixels (4-bits per pixel)
* Pixels are vertical (Column 0 + Rows 0,1 for first cell)
* Memory layout the same as 40 column text otherwise
* Supports Page 2 as well

### Double Lo-res Mode (experimental?)

* Follows Lo-res + 80 column mode

### Hi-res Mode

* 280 x 192 (280 x 160 in mixed mode, 40 column text mode only?)
* For each byte, 7 bits represent pixels, 1 bit represents 4 color palette

### Double Hi-res Mode

* 560 x 192 (560 x 160 in mixed mode, 80 column text)
* Like 80 column text, only one page supported


## Interrupt Technical

References: HW p. 58-60

* IO access in register $C023 (enable), $C032 (clear)
* 1 hz interrupt (RTC)
* Super Hi-res scanline interrupt
  * each line has an on/off for interrupt triggering
  * not supported for Apple II modes


## Display

* Expose when vertical blanking starts to the host
  * The host should wait until vertical blanking starts
  * Render emulated screen to a texture
  * Draw the screen when vertical blanking ends?
* Offscreen texture is 512x256 (will be expanded once dbl/super hires supported)
  * Render Apple IIx character sets onto textures (stb?)
  * Draw monospaced characters based on clemens text data at specific areas
* Just copy the screen data from the clemens buffers and process them separately
  * Can offload emulator to a thread/core when everything works
  * For now isolate all of the rendering code into a separate module/function
  *

### Text Rendering (40 column)
