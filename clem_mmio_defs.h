#ifndef CLEM_MMIO_DEFS_H
#define CLEM_MMIO_DEFS_H

#include "clem_defs.h"

/* The emulator ID as defined when reading c04f */
#define CLEM_EMULATOR_ID  0xce
#define CLEM_EMULATOR_VER 0x01

#define CLEM_MEGA2_CYCLES_PER_60TH (CLEM_MEGA2_CYCLES_PER_SECOND / 60)
#define CLEM_MEGA2_TIMER_1SEC_US   1000000
#define CLEM_MEGA2_TIMER_QSEC_US   266667

#define CLEM_1SEC_NS 1000000000
#define CLEM_1MS_NS  1000000

/** IRQ line masks (0xfff000000 are reserved for slot IRQs + NMIs*/
#define CLEM_IRQ_VGC_SCAN_LINE  (0x00000001)
#define CLEM_IRQ_VGC_BLANK      (0x00000002)
#define CLEM_IRQ_VGC_MASK       (0x0000000f)
#define CLEM_IRQ_TIMER_QSEC     (0x00000010)
#define CLEM_IRQ_TIMER_RTC_1SEC (0x00000020)
#define CLEM_IRQ_TIMER_MASK     (0x000000f0)
#define CLEM_IRQ_ADB_KEYB_SRQ   (0x00000100)
#define CLEM_IRQ_ADB_MOUSE_SRQ  (0x00000200) /* IIgs Unsupported */
#define CLEM_IRQ_ADB_MOUSE_EVT  (0x00000400)
#define CLEM_IRQ_ADB_DATA       (0x00000800)
#define CLEM_IRQ_ADB_MASK       (0x00000f00)
#define CLEM_IRQ_AUDIO_OSC      (0x00001000)
#define CLEM_IRQ_SLOT_1         (0x00100000)
#define CLEM_IRQ_SLOT_2         (0x00200000)
#define CLEM_IRQ_SLOT_3         (0x00400000)
#define CLEM_IRQ_SLOT_4         (0x00800000)
#define CLEM_IRQ_SLOT_5         (0x01000000)
#define CLEM_IRQ_SLOT_6         (0x02000000)
#define CLEM_IRQ_SLOT_7         (0x04000000)
// #define CLEM_CARD_NMI (0x40000000)
// #define CLEM_CARD_IRQ (0x80000000)

/** NMI line masks for card slot triggers */
#define CLEM_NMI_CARD_MASK (0x000000ff)

#define CLEM_CARD_SLOT_COUNT 7

//  These flags refer to bank 0 memory switches for address bit 17
//  0 = Main Bank, 1 = Aux Bank ZP, Stack and Language Card
#define CLEM_MEM_IO_MMAP_ALTZPLC 0x00000001

// consolidated mask for all Apple //e video areas influenced by 80COLSTORE
#define CLEM_MEM_IO_MMAP_OLDVIDEO 0x000000fe
//  0 = Main Bank RAM Read Enabled, 1 = Aux Bank RAM Read Enabled
#define CLEM_MEM_IO_MMAP_RAMRD 0x00000002
//  0 = Main Bank RAM Write Enabled, 1 = Aux Bank RAM Write Enabled
#define CLEM_MEM_IO_MMAP_RAMWRT 0x00000004
//  0 = Disabled 80 column storage (treats TXTPAGE2 as page 2 memory reliant on
//      RAMRD/RAMWRT)
//  1 = Enabled 80 column storage (treats TXTPAGE2 as page 1 aux memory)
//  80 column store flags should take precendence in this case
#define CLEM_MEM_IO_MMAP_80COLSTORE 0x00000008
//  Depends on 80COLSTORE.  Select Page 1/2 memory or main/aux page1 display
//  This switch must take precedence over RAMRD/RAMWRT for the selected regions
//  if 80COLSTORE is switched on
#define CLEM_MEM_IO_MMAP_TXTPAGE2 0x00000010
//  p89 of the //e reference indicates that to enable the 80COLSTORE/TXTPAGE2
//  switch for the HIRES region, you need to have HIRES mode active (text modes
//  always account for the 80COLSTORE, whether the current graphics mode is
//  full screen without text or not.)
#define CLEM_MEM_IO_MMAP_HIRES 0x00000020

//  Bits 4-7 These flags refer to the language card banks
#define CLEM_MEM_IO_MMAP_LC 0x00000f00
//  0 = Read LC ROM, 1 = Read LC RAM
#define CLEM_MEM_IO_MMAP_RDLCRAM 0x00000100
//  0 = Write protect LC RAM, 1 = Write enable LC RAM
#define CLEM_MEM_IO_MMAP_WRLCRAM 0x00000200
//  0 = LC Bank 1, 1 = LC Bank 2
#define CLEM_MEM_IO_MMAP_LCBANK2 0x00000400
//  0 = Internal ROM, 1 = Peripheral ROM
#define CLEM_MEM_IO_MMAP_CROM  0x000ff000
#define CLEM_MEM_IO_MMAP_C1ROM 0x00001000
#define CLEM_MEM_IO_MMAP_C2ROM 0x00002000
#define CLEM_MEM_IO_MMAP_C3ROM 0x00004000
#define CLEM_MEM_IO_MMAP_C4ROM 0x00008000
#define CLEM_MEM_IO_MMAP_C5ROM 0x00010000
#define CLEM_MEM_IO_MMAP_C6ROM 0x00020000
#define CLEM_MEM_IO_MMAP_C7ROM 0x00040000
#define CLEM_MEM_IO_MMAP_CXROM 0x00080000

// Bits 16-23 These flags refer to shadow register controls
#define CLEM_MEM_IO_MMAP_NSHADOW      0x0ff00000
#define CLEM_MEM_IO_MMAP_NSHADOW_TXT1 0x00100000
#define CLEM_MEM_IO_MMAP_NSHADOW_TXT2 0x00200000
#define CLEM_MEM_IO_MMAP_NSHADOW_HGR1 0x00400000
#define CLEM_MEM_IO_MMAP_NSHADOW_HGR2 0x00800000
#define CLEM_MEM_IO_MMAP_NSHADOW_SHGR 0x01000000
#define CLEM_MEM_IO_MMAP_NSHADOW_AUX  0x02000000
//  0 = Bank 00: I/O enabled + LC enabled,  1 = I/O disabled + LC disabled
#define CLEM_MEM_IO_MMAP_NIOLC 0x04000000

//  Bits 24-31 deal with memory mapped features not covered above

/**
 * IO Registers
 */

#define CLEM_MMIO_MAKE_IO_ADDRESS(_reg_) (0xc000 | (uint8_t)(_reg_))

/** Keyboard data (bits 6-0) and strobe (bit 7) */
#define CLEM_MMIO_REG_KEYB_READ 0x00

/** Write to this register to set PAGE2 to flip between text pages, 40 column
 *  text pages */
#define CLEM_MMIO_REG_80STOREOFF_WRITE 0x00
/** Write to this register to set PAGE2 to switch between main and aux, ala
 *  80 column text */
#define CLEM_MMIO_REG_80STOREON_WRITE 0x01
/** Read main memory 0x0200 - 0xBFFF */
#define CLEM_MMIO_REG_RDMAINRAM 0x02
/** Read aux memory 0x0200 - 0xBFFF */
#define CLEM_MMIO_REG_RDCARDRAM 0x03
/** Write main memory 0x0200 - 0xBFFF */
#define CLEM_MMIO_REG_WRMAINRAM 0x04
/** Write aux memory 0x0200 - 0xBFFF */
#define CLEM_MMIO_REG_WRCARDRAM 0x05
/** Write to enable peripheral ROM for C100 - C7FF */
#define CLEM_MMIO_REG_SLOTCXROM 0x06
/** Write to enable internal ROM for C100 - C7FF */
#define CLEM_MMIO_REG_INTCXROM 0x07
/** Write to enable main bank Page 0, Page 1 and LC */
#define CLEM_MMIO_REG_STDZP 0x08
/** Write to enable aux bank Page 0, Page 1 and LC */
#define CLEM_MMIO_REG_ALTZP 0x09
/** Write to enable internal ROM for C300 */
#define CLEM_MMIO_REG_INTC3ROM 0x0A
/** Write to enable peripheral ROM for C300 */
#define CLEM_MMIO_REG_SLOTC3ROM 0x0B
/** Write Switches for toggling 80 column mode display */
#define CLEM_MMIO_REG_80COLUMN_OFF 0x0C
#define CLEM_MMIO_REG_80COLUMN_ON  0x0D
/** Write Alternate Character Set Off/On */
#define CLEM_MMIO_REG_ALTCHARSET_OFF 0x0E
#define CLEM_MMIO_REG_ALTCHARSET_ON  0x0F
/** Read bit 7 for 'any-key down', read or write to clear strobe bit in $C000,
 *  and also provides the last key down - not clear in the //e or IIgs docs.
 * (per https://apple2.org.za/gswv/a2zine/faqs/csa2pfaq.html)
 */
#define CLEM_MMIO_REG_ANYKEY_STROBE 0x10
/** Read and test bit 7, 0 = LC bank 1, 1 = bank 2 */
#define CLEM_MMIO_REG_LC_BANK_TEST 0x11
/** Read and test bit 7, 0 = ROM, 1 = RAM */
#define CLEM_MMIO_REG_ROM_RAM_TEST 0x12
/** Bit 7: on = aux, off = main */
#define CLEM_MMIO_REG_RAMRD_TEST 0x13
/** Bit 7: on = aux, off = main */
#define CLEM_MMIO_REG_RAMWRT_TEST 0x14
/** 0 = slot ROM, 1 = internal ROM as source for the CXXX pages */
#define CLEM_MMIO_REG_READCXROM 0x15
/** Read bit 7 to detect bank 0 = main, 1 = aux bank */
#define CLEM_MMIO_REG_RDALTZP_TEST 0x16
/** Get ROM source for the C300 page */
#define CLEM_MMIO_REG_READC3ROM 0x17
/** Bit 7: on = 80COLSTORE on */
#define CLEM_MMIO_REG_80COLSTORE_TEST 0x18
/** Bit 7: on = not Vertical Blank  */
#define CLEM_MMIO_REG_VBLBAR 0x19
/** Bit 7: on = Full text mode, off = none or mixed */
#define CLEM_MMIO_REG_TXT_TEST 0x1A
/** Bit 7: on = Mixed text mode, off = full screen mode */
#define CLEM_MMIO_REG_MIXED_TEST 0x1B
/** Bit 7: on = page 2, off = page 1 */
#define CLEM_MMIO_REG_TXTPAGE2_TEST 0x1C
/** Bit 7: on = hires mode on */
#define CLEM_MMIO_REG_HIRES_TEST 0x1D
/** Bit 7: alternate character set on */
#define CLEM_MMIO_REG_ALTCHARSET_TEST 0x1E
/** Bit 7: 80 column mode on */
#define CLEM_MMIO_REG_80COLUMN_TEST 0x1F
/** Cassette Port (floating bus use only) */
#define CLEM_MMIO_REG_CASSETTE_PORT_NOP 0x20
/** Write Bit 7: 1 = monochrome, 0 = color */
#define CLEM_MMIO_REG_VGC_MONO 0x21
/** Text: Bits 7-4, background: bits 3-0 color */
#define CLEM_MMIO_REG_VGC_TEXT_COLOR 0x22
/** R/W VGC Interrupt Byte */
#define CLEM_MMIO_REG_VGC_IRQ_BYTE 0x23
/** Mouse button (bit 7) and movement status (btis 6:0) */
#define CLEM_MMIO_REG_ADB_MOUSE_DATA 0x24
/** Mask indicating which modifier keys are pressed */
#define CLEM_MMIO_REG_ADB_MODKEY 0x25
/** ADB GLU Command Data register */
#define CLEM_MMIO_REG_ADB_CMD_DATA 0x26
/** ADB status (key/mouse)  register */
#define CLEM_MMIO_REG_ADB_STATUS 0x27

/**
 * Primarily defines how memory is accessed by the video controller, with
 * the bank latch bit (0), which is always set to 1 AFAIK
 */
#define CLEM_MMIO_REG_NEWVIDEO 0x29
/** R/W? Character set language selection and NTSC/PAL region */
#define CLEM_MMIO_REG_LANGSEL 0x2B
/** ??? */
#define CLEM_MMIO_REG_CHARROM_TEST 0x2C
/** Selects Internal vs Peripheral ROM for slots 1 - 7, bit 0, 3 must be 0 */
#define CLEM_MMIO_REG_SLOTROMSEL 0x2D
/** Read Vertical counter bits?? */
#define CLEM_MMIO_REG_VGC_VERTCNT 0x2E
/** Read Vertical counter bits?? */
#define CLEM_MMIO_REG_VGC_HORIZCNT 0x2F
/** Speaker click */
#define CLEM_MMIO_REG_SPKR 0x30
/** Write Disk access 3.5" */
#define CLEM_MMIO_REG_DISK_INTERFACE 0x31
/** Write Scan interrupts (VGC, RTC) clear  */
#define CLEM_MMIO_REG_RTC_VGC_SCANINT 0x32
/** Real time clock data register */
#define CLEM_MMIO_REG_RTC_DATA 0x33
/** Real time clock + border colorl joint register */
#define CLEM_MMIO_REG_RTC_CTL 0x34
/**
 * Defines what areas of the FPI banks are disabled, and how I/O language
 * card space is treated on FPI bank 0 and 1.
 */
#define CLEM_MMIO_REG_SHADOW 0x35
/**
 * Defines fast/slow processor speed, system-wide shadowing behavior and other
 * items... (disk input?)
 */
#define CLEM_MMIO_REG_SPEED 0x36
/** SCC Command Register B */
#define CLEM_MMIO_REG_SCC_B_CMD 0x38
/** SCC Command Register A */
#define CLEM_MMIO_REG_SCC_A_CMD 0x39
/** SCC Data Register B */
#define CLEM_MMIO_REG_SCC_B_DATA 0x3A
/** SCC Data Register A */
#define CLEM_MMIO_REG_SCC_A_DATA 0x3B

/** Sound GLU Control Register */
#define CLEM_MMIO_REG_AUDIO_CTL 0x3C
/** Sound GLU Read/Write Data Register */
#define CLEM_MMIO_REG_AUDIO_DATA 0x3D
/** Sound GLU Data Address Lo byte Register */
#define CLEM_MMIO_REG_AUDIO_ADRLO 0x3E
/** Sound GLU Data Address Hi byte Register */
#define CLEM_MMIO_REG_AUDIO_ADRHI 0x3F
/** Enable specific Mega2 (Video, mouse, timer) interrupts */
#define CLEM_MMIO_REG_MEGA2_INTEN 0x41
/** Read Mega II mouse delta X - IIgs only, so TBD */
#define CLEM_MMIO_REG_MEGA2_MOUSE_DX 0x44
/** Read Mega II mouse delta Y - IIgs only, so TBD */
#define CLEM_MMIO_REG_MEGA2_MOUSE_DY 0x45
/** Various Mega II specific interrupt flags likely used only by firmware */
#define CLEM_MMIO_REG_DIAG_INTTYPE 0x46
/** Clears some MEGA2 based interrupts */
#define CLEM_MMIO_REG_CLRVBLINT 0x47
/** The emulator test function as defined via CLEM_MMIO_EMULATOR_DETECT_XXX */
#define CLEM_MMIO_REG_EMULATOR 0x4f
/** R/W Display graphics mode - will be mixed if correct flags are set */
#define CLEM_MMIO_REG_TXTCLR 0x50
/** R/W Display text mode only */
#define CLEM_MMIO_REG_TXTSET 0x51
/** R/W Clears mixed mode graphics */
#define CLEM_MMIO_REG_MIXCLR 0x52
/** R/W Sets mixed mode graphics */
#define CLEM_MMIO_REG_MIXSET 0x53
/** R/W enable page 1 or page 2 text modified by 80COLSTORE */
#define CLEM_MMIO_REG_TXTPAGE1 0x54
#define CLEM_MMIO_REG_TXTPAGE2 0x55
/** R/W enable lo-res graphics */
#define CLEM_MMIO_REG_LORES 0x56
/** R/W enable lo-res graphics */
#define CLEM_MMIO_REG_HIRES 0x57
/** R/W enable/disable annunciator pins */
#define CLEM_MMIO_REG_AN0_OFF 0x58
#define CLEM_MMIO_REG_AN0_ON  0x59
#define CLEM_MMIO_REG_AN1_OFF 0x5A
#define CLEM_MMIO_REG_AN1_ON  0x5B
#define CLEM_MMIO_REG_AN2_OFF 0x5C
#define CLEM_MMIO_REG_AN2_ON  0x5D
#define CLEM_MMIO_REG_AN3_OFF 0x5E
#define CLEM_MMIO_REG_AN3_ON  0x5F

/** Joystick Button 3 */
#define CLEM_MMIO_REG_SW3 0x60
/** Open Apple Key or Joystick Button 0 */
#define CLEM_MMIO_REG_SW0 0x61
/** Solid Apple Key or Joystick Button 1 */
#define CLEM_MMIO_REG_SW1 0x62
/** Joystick Button 2 */
#define CLEM_MMIO_REG_SW2 0x63
/** Paddle 0 */
#define CLEM_MMIO_REG_PADDL0 0x64
/** Paddle 1 */
#define CLEM_MMIO_REG_PADDL1 0x65
/** Paddle 2 */
#define CLEM_MMIO_REG_PADDL2 0x66
/** Paddle 3 */
#define CLEM_MMIO_REG_PADDL3 0x67
/**
 * Amalgom of the C08x registers
 */
#define CLEM_MMIO_REG_STATEREG 0x68

/**
 * Resets the paddle timers (note that //e docs state that reset occurs on
 * "addressing C07X will cause a reset. - //e tech ref manual- p190)
 *
 * Also note that different cards used various C07x memory addresses as IO
 * registers for things like bank select.  Determine need as it comes, and
 * always reset the paddles with accessing C07X?
 *
 * For example, the Transwarp card used c074, and some titles will write to it
 * to disable Transwarp (not IIgs fast mode).
 * - http://www.faqs.org/faqs/apple2/faq/part3/
 */
#define CLEM_MMIO_REG_PTRIG 0x70
/** Write 1 to disable transwarp, 0 to enable (no-op on Clemens) */
#define CLEM_MMIO_REG_C074_TRANSWARP 0x74
/** R1 - LC Bank 2, Read RAM, Write Protect */
#define CLEM_MMIO_REG_LC2_RAM_WP  0x80
#define CLEM_MMIO_REG_LC2_RAM_WP2 0x84
/** R2 - LC Bank 2, Read ROM, Write Enable */
#define CLEM_MMIO_REG_LC2_ROM_WE  0x81
#define CLEM_MMIO_REG_LC2_ROM_WE2 0x85
/** R1 - LC Bank 2, Read ROM, Write Protect */
#define CLEM_MMIO_REG_LC2_ROM_WP  0x82
#define CLEM_MMIO_REG_LC2_ROM_WP2 0x86
/** R2 - LC Bank 2, Read and Write Enable */
#define CLEM_MMIO_REG_LC2_RAM_WE  0x83
#define CLEM_MMIO_REG_LC2_RAM_WE2 0x87
/** R1 - LC Bank 1, Read RAM, Write Protect */
#define CLEM_MMIO_REG_LC1_RAM_WP  0x88
#define CLEM_MMIO_REG_LC1_RAM_WP2 0x8C
/** R2 - LC Bank 1, Read ROM, Write Enable */
#define CLEM_MMIO_REG_LC1_ROM_WE  0x89
#define CLEM_MMIO_REG_LC1_ROM_WE2 0x8D
/** R1 - LC Bank 2, Read ROM, Write Protect */
#define CLEM_MMIO_REG_LC1_ROM_WP  0x8A
#define CLEM_MMIO_REG_LC1_ROM_WP2 0x8E
/** R2 - LC Bank 1, Read and Write Enable */
#define CLEM_MMIO_REG_LC1_RAM_WE  0x8B
#define CLEM_MMIO_REG_LC1_RAM_WE2 0x8F

/** IWM registers */
#define CLEM_MMIO_REG_IWM_PHASE0_LO     0xE0
#define CLEM_MMIO_REG_IWM_PHASE0_HI     0xE1
#define CLEM_MMIO_REG_IWM_PHASE1_LO     0xE2
#define CLEM_MMIO_REG_IWM_PHASE1_HI     0xE3
#define CLEM_MMIO_REG_IWM_PHASE2_LO     0xE4
#define CLEM_MMIO_REG_IWM_PHASE2_HI     0xE5
#define CLEM_MMIO_REG_IWM_PHASE3_LO     0xE6
#define CLEM_MMIO_REG_IWM_PHASE3_HI     0xE7
#define CLEM_MMIO_REG_IWM_DRIVE_DISABLE 0xE8
#define CLEM_MMIO_REG_IWM_DRIVE_ENABLE  0xE9
#define CLEM_MMIO_REG_IWM_DRIVE_0       0xEA
#define CLEM_MMIO_REG_IWM_DRIVE_1       0xEB
#define CLEM_MMIO_REG_IWM_Q6_LO         0xEC
#define CLEM_MMIO_REG_IWM_Q6_HI         0xED
#define CLEM_MMIO_REG_IWM_Q7_LO         0xEE
#define CLEM_MMIO_REG_IWM_Q7_HI         0xEF

/** New video (C029) bitflag defines */
#define CLEM_MMIO_NEWVIDEO_BANKLATCH_INHIBIT 0x01
#define CLEM_MMIO_NEWVIDEO_DBLHIRES_MONO     0x20
#define CLEM_MMIO_NEWVIDEO_LINEARIZE_MEMORY  0x40
#define CLEM_MMIO_NEWVIDEO_SUPERHIRES_ENABLE 0x80

/** Speed register (C036) bitflag defines */
#define CLEM_MMIO_SPEED_DISK_FLAGS   0x0f
#define CLEM_MMIO_SPEED_POWERED_ON   0x40
#define CLEM_MMIO_SPEED_FAST_ENABLED 0x80

/** Interrupt type register (consolidated with Mega II) */
#define CLEM_MMIO_INTTYPE_IRQ  0x01
#define CLEM_MMIO_INTTYPE_VBL  0x08
#define CLEM_MMIO_INTTYPE_QSEC 0x10

/** Timer (internal, C023 partial) device flags */
#define CLEM_MMIO_TIMER_1SEC_ENABLED 0x00000040
#define CLEM_MMIO_TIMER_QSEC_ENABLED 0x00000100

/** Used for emulator detection at c04f
 * Apple II Technical Notes #201 (IIgs)
 * Identifying Emulators
 */
#define CLEM_MMIO_EMULATOR_DETECT_IDLE    0
#define CLEM_MMIO_EMULATOR_DETECT_START   1
#define CLEM_MMIO_EMULATOR_DETECT_VERSION 2

#define CLEM_RTC_BRAM_SIZE 256

/** Definitions for the Battery RAM used to access values from the ClemensDeviceRTC component */
#define CLEM_RTC_BRAM_SYSTEM_SPEED 0x20

/** Setting: ADB keyboard buffer size - this doesn't need to be large since
 *  Apple II apps typically expect to consume events via ISR or prompt polling
 *  of IO registers.  Our host should have the opportunity to send input to the
 *  emulator at a decent frequency (30-60hz) for us not to lose events
 */
#define CLEM_ADB_KEYB_BUFFER_LIMIT     8
#define CLEM_ADB_KEYB_TOGGLE_CAPS_LOCK 0x0000001

/** Gameport support - note that paddle axis values range from 0 to 1023, and
 *  there's support for up to 8 buttons.  Of course the Apple 2 only supports
 *  two buttons (possibly more with extended gameport support).  A host can
 *  supply states for up to 8 buttons, and the emulator can tread buttons 0, 2,
 *  4, and so on as 'button 0' and 1, 3, 5, ... as 'button 1' as an option
 */
#define CLEM_GAMEPORT_BUTTON_MASK_JOYSTICK_0 0x00000000
#define CLEM_GAMEPORT_BUTTON_MASK_JOYSTICK_1 0x80000000
#define CLEM_GAMEPORT_BUTTON_MASK_BUTTONS    0x000000ff
/* Changing this value could affect integer math calculations in clem_adb.c
   regarding discharge time for the capacitor used in the emualated gameport
   timing circuit */
#define CLEM_GAMEPORT_PADDLE_AXIS_VALUE_MAX 1023
/* Ohms - Maximum potentiometer resistance of 150kOhms */
#define CLEM_GAMEPORT_PADDLE_RESISTANCE 150000
/* Nanofarads - used for calculation purposes (0.022 uF capacitor) */
#define CLEM_GAMEPORT_PADDLE_CAPACITANCE_NF     22
#define CLEM_GAMEPORT_PADDLE_AXIS_VALUE_INVALID 0xffff
/* 2us additional delay as suggested from 7-29 of Understanding the Apple //e
   this conveniently allows us to treat a zero time as meaning 'no input'
   from the gameport
*/
#define CLEM_GAMEPORT_PADDLE_TIME_INTIIAL_NS 2000

/*
   R = Rmax * PDL/PDLmax
   t = RC  (C = 0.22 uF)
   t = R * (0.022*1e-6 F)
   seconds  = (Rmax * PDL / PDLmax)  * 0.022 * 1e-6
   microseconds = Rmax * (PDL / PDLmax) * 0.022
   nanoseconds = Rmax * PDL * 22 / PDLmax
*/
#define CLEM_GAMEPORT_CALCULATE_TIME_NS(_adb_, _index_)                                            \
    (((CLEM_GAMEPORT_PADDLE_RESISTANCE * (uint32_t)((_adb_)->gameport.paddle[_index_]) *           \
       CLEM_GAMEPORT_PADDLE_CAPACITANCE_NF) /                                                      \
      CLEM_GAMEPORT_PADDLE_AXIS_VALUE_MAX) +                                                       \
     CLEM_GAMEPORT_PADDLE_TIME_INTIIAL_NS)

/* Key codes */
#define CLEM_ADB_KEY_A 0x00
#define CLEM_ADB_KEY_S 0x01
#define CLEM_ADB_KEY_D 0x02
#define CLEM_ADB_KEY_F 0x03
#define CLEM_ADB_KEY_H 0x04
#define CLEM_ADB_KEY_G 0x05
#define CLEM_ADB_KEY_Z 0x06
#define CLEM_ADB_KEY_X 0x07
#define CLEM_ADB_KEY_C 0x08
#define CLEM_ADB_KEY_V 0x09
/* Skipped 0x0A */
#define CLEM_ADB_KEY_B          0x0B
#define CLEM_ADB_KEY_Q          0x0C
#define CLEM_ADB_KEY_W          0x0D
#define CLEM_ADB_KEY_E          0x0E
#define CLEM_ADB_KEY_R          0x0F
#define CLEM_ADB_KEY_T          0x10
#define CLEM_ADB_KEY_Y          0x11
#define CLEM_ADB_KEY_1          0x12
#define CLEM_ADB_KEY_2          0x13
#define CLEM_ADB_KEY_3          0x14
#define CLEM_ADB_KEY_4          0x15
#define CLEM_ADB_KEY_6          0x16
#define CLEM_ADB_KEY_5          0x17
#define CLEM_ADB_KEY_EQUALS     0x18
#define CLEM_ADB_KEY_9          0x19
#define CLEM_ADB_KEY_7          0x1A
#define CLEM_ADB_KEY_MINUS      0x1B
#define CLEM_ADB_KEY_8          0x1C
#define CLEM_ADB_KEY_0          0x1D
#define CLEM_ADB_KEY_RBRACKET   0x1E
#define CLEM_ADB_KEY_O          0x1F
#define CLEM_ADB_KEY_U          0x20
#define CLEM_ADB_KEY_LBRACKET   0x21
#define CLEM_ADB_KEY_I          0x22
#define CLEM_ADB_KEY_P          0x23
#define CLEM_ADB_KEY_RETURN     0x24
#define CLEM_ADB_KEY_L          0x25
#define CLEM_ADB_KEY_J          0x26
#define CLEM_ADB_KEY_APOSTRAPHE 0x27
#define CLEM_ADB_KEY_K          0x28
#define CLEM_ADB_KEY_SEMICOLON  0x29
#define CLEM_ADB_KEY_BACKSLASH  0x2A
#define CLEM_ADB_KEY_COMMA      0x2B
#define CLEM_ADB_KEY_FWDSLASH   0x2C
#define CLEM_ADB_KEY_N          0x2D
#define CLEM_ADB_KEY_M          0x2E
#define CLEM_ADB_KEY_PERIOD     0x2F
#define CLEM_ADB_KEY_TAB        0x30
#define CLEM_ADB_KEY_SPACE      0x31
#define CLEM_ADB_KEY_BACKQUOTE  0x32
#define CLEM_ADB_KEY_DELETE     0x33
/* Skipped 0x34 */
#define CLEM_ADB_KEY_ESCAPE             0x35
#define CLEM_ADB_KEY_LCTRL              0x36
#define CLEM_ADB_KEY_COMMAND_OPEN_APPLE 0x37
#define CLEM_ADB_KEY_LSHIFT             0x38
#define CLEM_ADB_KEY_CAPSLOCK           0x39
#define CLEM_ADB_KEY_OPTION             0x3A
#define CLEM_ADB_KEY_LEFT               0x3B
#define CLEM_ADB_KEY_RIGHT              0x3C
#define CLEM_ADB_KEY_DOWN               0x3D
#define CLEM_ADB_KEY_UP                 0x3E
/* Skipped 0x3F */
/* Skipped 0x40 */
#define CLEM_ADB_KEY_PAD_DECIMAL 0x41
/* Skipped 0x42 */
#define CLEM_ADB_KEY_PAD_MULTIPLY 0x43
/* Skipped 0x44 */
#define CLEM_ADB_KEY_PAD_PLUS 0x45
/* Skipped 0x46 */
#define CLEM_ADB_KEY_PAD_CLEAR_NUMLOCK 0x47
/* Skipped 0x48 */
/* Skipped 0x49 */
/* Skipped 0x4A */
#define CLEM_ADB_KEY_PAD_DIVIDE 0x4B
#define CLEM_ADB_KEY_PAD_ENTER  0x4C
/* Skipped 0x4D */
#define CLEM_ADB_KEY_PAD_MINUS 0x4E
/* Skipped 0x4F */
/* Skipped 0x50 */
#define CLEM_ADB_KEY_PAD_EQUALS 0x51
#define CLEM_ADB_KEY_PAD_0      0x52
#define CLEM_ADB_KEY_PAD_1      0x53
#define CLEM_ADB_KEY_PAD_2      0x54
#define CLEM_ADB_KEY_PAD_3      0x55
#define CLEM_ADB_KEY_PAD_4      0x56
#define CLEM_ADB_KEY_PAD_5      0x57
#define CLEM_ADB_KEY_PAD_6      0x58
#define CLEM_ADB_KEY_PAD_7      0x59
/* Skipped 0x5A */
#define CLEM_ADB_KEY_PAD_8 0x5B
#define CLEM_ADB_KEY_PAD_9 0x5C
/* Skipped 0x5D */
/* Skipped 0x5E */
/* Skipped 0x5F */
#define CLEM_ADB_KEY_F5 0x60
#define CLEM_ADB_KEY_F6 0x61
#define CLEM_ADB_KEY_F7 0x62
#define CLEM_ADB_KEY_F3 0x63
#define CLEM_ADB_KEY_F8 0x64
#define CLEM_ADB_KEY_F9 0x65
/* Skipped 0x66 */
#define CLEM_ADB_KEY_F11 0x67
/* Skipped 0x68 */
#define CLEM_ADB_KEY_F13 0x69
/* Skipped 0x6A */
#define CLEM_ADB_KEY_F14 0x6B
/* Skipped 0x6C */
#define CLEM_ADB_KEY_F10 0x6D
/* Skipped 0x6E */
#define CLEM_ADB_KEY_F12 0x6F
/* Skipped 0x70 */
#define CLEM_ADB_KEY_F15         0x71
#define CLEM_ADB_KEY_HELP_INSERT 0x72
#define CLEM_ADB_KEY_HOME        0x73
#define CLEM_ADB_KEY_PAGEUP      0x74
#define CLEM_ADB_KEY_PAD_DELETE  0x75
#define CLEM_ADB_KEY_F4          0x76
#define CLEM_ADB_KEY_END         0x77
#define CLEM_ADB_KEY_F2          0x78
#define CLEM_ADB_KEY_PAGEDOWN    0x79
#define CLEM_ADB_KEY_F1          0x7A
#define CLEM_ADB_KEY_RSHIFT      0x7B
#define CLEM_ADB_KEY_ROPTION     0x7C
#define CLEM_ADB_KEY_RCTRL       0x7D
/* Skipped 0x7E */
#define CLEM_ADB_KEY_RESET 0x7F

#define CLEM_ADB_KEY_CODE_LIMIT 0x80

/* GLU register flags */
#define CLEM_ADB_GLU_REG0_MOUSE_BTN         0x8000
#define CLEM_ADB_GLU_REG0_MOUSE_Y_DELTA     0x7f00
#define CLEM_ADB_GLU_REG0_MOUSE_ALWAYS_1    0x0080 /* Table 6-11 HWRef */
#define CLEM_ADB_GLU_REG0_MOUSE_X_DELTA     0x007f
#define CLEM_ADB_GLU_REG2_KEY_CAPS_TOGGLE   0x0002
#define CLEM_ADB_GLU_REG2_KEY_CLEAR_NUMLOCK 0x0080
#define CLEM_ADB_GLU_REG2_KEY_APPLE         0x0100
#define CLEM_ADB_GLU_REG2_KEY_OPTION        0x0200
#define CLEM_ADB_GLU_REG2_KEY_SHIFT         0x0400
#define CLEM_ADB_GLU_REG2_KEY_CTRL          0x0800
#define CLEM_ADB_GLU_REG2_KEY_RESET         0x1000
#define CLEM_ADB_GLU_REG2_KEY_CAPS          0x2000
#define CLEM_ADB_GLU_REG2_MODKEY_MASK                                                              \
    0x7f80 /* no scroll lock or LED bits per 'ADB - The Untold Story' */
#define CLEM_ADB_GLU_REG3_MASK_SRQ    0x2000
#define CLEM_ADB_GLU_REG3_DEVICE_MASK 0x0F00

/* See clemens_get_adb_key_modifier_states() */
#define CLEM_ADB_KEY_MOD_STATE_CAPS    CLEM_ADB_GLU_REG2_KEY_CAPS_TOGGLE
#define CLEM_ADB_KEY_MOD_STATE_NUMLOCK CLEM_ADB_GLU_REG2_KEY_CLEAR_NUMLOCK
#define CLEM_ADB_KEY_MOD_STATE_APPLE   CLEM_ADB_GLU_REG2_KEY_APPLE
#define CLEM_ADB_KEY_MOD_STATE_OPTION  CLEM_ADB_GLU_REG2_KEY_OPTION
#define CLEM_ADB_KEY_MOD_STATE_SHIFT   CLEM_ADB_GLU_REG2_KEY_SHIFT
#define CLEM_ADB_KEY_MOD_STATE_CTRL    CLEM_ADB_GLU_REG2_KEY_CTRL
#define CLEM_ADB_KEY_MOD_STATE_RESET   CLEM_ADB_GLU_REG2_KEY_RESET
#define CLEM_ADB_KEY_MOD_STATE_ESCAPE  0x00010000

/** Emulated duration of every 'step' iwm_glu_sync runs. 1.023 / 2 ~ 0.511 */
#define CLEM_IWM_SYNC_CLOCKS_FAST        (CLEM_CLOCKS_2MHZ_CYCLE * 4)
#define CLEM_IWM_SYNC_CLOCKS_NORMAL      (CLEM_CLOCKS_2MHZ_CYCLE * 8)
#define CLEM_IWM_SYNC_DISK_FRAME_NS      500
#define CLEM_IWM_SYNC_DISK_FRAME_NS_FAST 250
#define CLEM_IWM_DRIVE_RANDOM_BYTES      16
#define CLEM_IWM_DEBUG_BUFFER_SIZE       256

/*  Enable 3.5 drive seris */
#define CLEM_IWM_FLAG_DRIVE_35 0x00000001
/*  Drive system is active - in tandem with drive index selected */
#define CLEM_IWM_FLAG_DRIVE_ON 0x00000002
/*  Drive 1 selected - note IWM only allows one drive at a time, but the
    disk port has two pins for drive, so emulating that aspect */
#define CLEM_IWM_FLAG_DRIVE_1 0x00000004
/*  Drive 2 selected */
#define CLEM_IWM_FLAG_DRIVE_2 0x00000008
/*  Congolmerate mask for any-drive selected */
#define CLEM_IWM_FLAG_DRIVE_ANY (CLEM_IWM_FLAG_DRIVE_1 + CLEM_IWM_FLAG_DRIVE_2)
/* device flag, 3.5" side 2 (not used for 5.25")
   note, this really is used for 3.5" drive controller actions:
   https://llx.com/Neil/a2/disk
*/
#define CLEM_IWM_FLAG_HEAD_SEL 0x00000010
/* Places drive in 'write' mode */
#define CLEM_IWM_FLAG_WRITE_REQUEST 0x00000040
/*  Write protect for disk for 5.25", and the sense input bit for 3.5" drives*/
#define CLEM_IWM_FLAG_WRPROTECT_SENSE 0x00000080
/*  Read pulse from the disk/drive bitstream is on */
#define CLEM_IWM_FLAG_READ_DATA 0x00000100
/*  Write pulse input to drive */
#define CLEM_IWM_FLAG_WRITE_DATA 0x00000200
/*  Bit cell interval has passed */
#define CLEM_IWM_FLAG_PULSE_HIGH 0x00001000
/*  For debugging only */
#define CLEM_IWM_FLAG_READ_DATA_FAKE 0x00002000
#define CLEM_IWM_FLAG_WRITE_HI       0x00004000

#define CLEM_MONITOR_SIGNAL_NTSC 0
#define CLEM_MONITOR_SIGNAL_PAL  1

#define CLEM_MONITOR_COLOR_RGB  0
#define CLEM_MONITOR_COLOR_MONO 1

/* NTSC scanlines start at counter 7 and end at 198 (192 lines)
   VBL begins at 199 (scanline 192)
   see technote 39, 40 and clem_vgc.c for links
*/
/* Cycle count for horizontal scan in 1.023mhz cycles without stretch.
   Use ClemensTimespec for stretch calculations
*/
#define CLEM_VGC_HORIZ_SCAN_PHI0_CYCLES 65
#define CLEM_VGC_HORIZ_SCAN_TIME_NS     63700 /* this is with the stretch PHI0 cycle */
#define CLEM_VGC_NTSC_SCANLINE_COUNT    262
#define CLEM_VGC_NTSC_SCAN_TIME_NS      (CLEM_VGC_HORIZ_SCAN_TIME_NS * CLEM_VGC_NTSC_SCANLINE_COUNT)
#define CLEM_VGC_VBL_NTSC_LOWER_BOUND   199
#define CLEM_VGC_VBL_NTSC_UPPER_BOUND   (CLEM_VGC_NTSC_SCANLINE_COUNT - 1)
#define CLEM_VGC_PAL_SCANLINE_COUNT     312
#define CLEM_VGC_PAL_SCAN_TIME_NS       (CLEM_VGC_HORIZ_SCAN_TIME_NS * CLEM_VGC_PAL_SCAN_TIME_NS)

#define CLEM_VGC_FIRST_VISIBLE_SCANLINE_CNTR 7

#define CLEM_VGC_TEXT_SCANLINE_COUNT 24
#define CLEM_VGC_HGR_SCANLINE_COUNT  192
#define CLEM_VGC_SHGR_SCANLINE_COUNT 200

/* Text colors */
#define CLEM_VGC_COLOR_BLACK       0x00
#define CLEM_VGC_COLOR_DEEP_RED    0x01
#define CLEM_VGC_COLOR_DARK_BLUE   0x02
#define CLEM_VGC_COLOR_PURPLE      0x03
#define CLEM_VGC_COLOR_DARK_GREEN  0x04
#define CLEM_VGC_COLOR_DARK_GRAY   0x05
#define CLEM_VGC_COLOR_MEDIUM_BLUE 0x06
#define CLEM_VGC_COLOR_LIGHT_BLUE  0x07
#define CLEM_VGC_COLOR_BROWN       0x08
#define CLEM_VGC_COLOR_ORANGE      0x09
#define CLEM_VGC_COLOR_LIGHT_GRAY  0x0A
#define CLEM_VGC_COLOR_PINK        0x0B
#define CLEM_VGC_COLOR_GREEN       0x0C
#define CLEM_VGC_COLOR_YELLOW      0x0D
#define CLEM_VGC_COLOR_AQUAMARINE  0x0E
#define CLEM_VGC_COLOR_WHITE       0x0F

#define CLEM_VGC_GRAPHICS_MODE   0x00000001
#define CLEM_VGC_MIXED_TEXT      0x00000002
#define CLEM_VGC_80COLUMN_TEXT   0x00000004
#define CLEM_VGC_LORES           0x00000010
#define CLEM_VGC_HIRES           0x00000020
#define CLEM_VGC_RESOLUTION_MASK 0x000000F0
#define CLEM_VGC_SUPER_HIRES     0x00000100
#define CLEM_VGC_ALTCHARSET      0x00010000
#define CLEM_VGC_MONOCHROME      0x00020000
#define CLEM_VGC_PAL             0x00040000
#define CLEM_VGC_LANGUAGE        0x00080000
#define CLEM_VGC_ENABLE_VBL_IRQ  0x00100000
#define CLEM_VGC_DISABLE_AN3     0x00200000
#define CLEM_VGC_DBLRES_MASK     0x00200005
#define CLEM_VGC_INIT            0x80000000

#define CLEM_VGC_SCANLINE_CONTROL_640_MODE   (0x80)
#define CLEM_VGC_SCANLINE_CONTROL_INTERRUPT  (0x40)
#define CLEM_VGC_SCANLINE_COLORFILL_MODE     (0x20)
#define CLEM_VGC_SCANLINE_PALETTE_INDEX_MASK (0x0f)

#define CLEM_SCC_PORT_DTR        0x01
#define CLEM_SCC_PORT_HSKI       0x02
#define CLEM_SCC_PORT_TX_DATA_LO 0x04
#define CLEM_SCC_PORT_TX_DATA_HI 0x08
#define CLEM_SCC_PORT_RX_DATA_LO 0x10
#define CLEM_SCC_PORT_RX_DATA_HI 0x20
#define CLEM_SCC_PORT_GPI        0x40

#define CLEM_ENSONIQ_OSC_CTL_FREE_MODE 0x00
#define CLEM_ENSONIQ_OSC_CTL_M0        0x02
#define CLEM_ENSONIQ_OSC_CTL_SYNC      0x04
#define CLEM_ENSONIQ_OSC_CTL_SWAP      0x06
#define CLEM_ENSONIQ_OSC_CTL_HALT      0x01
#define CLEM_ENSONIQ_OSC_CTL_IE        0x08

#define CLEM_ENSONIQ_REG_OSC_FCLOW  0x00
#define CLEM_ENSONIQ_REG_OSC_FCHI   0x20
#define CLEM_ENSONIQ_REG_OSC_VOLUME 0x40
#define CLEM_ENSONIQ_REG_OSC_DATA   0x60
#define CLEM_ENSONIQ_REG_OSC_PTR    0x80
#define CLEM_ENSONIQ_REG_OSC_CTRL   0xa0
#define CLEM_ENSONIQ_REG_OSC_SIZE   0xc0
#define CLEM_ENSONIQ_REG_OSC_OIR    0xe0
#define CLEM_ENSONIQ_REG_OSC_ENABLE 0xe1
#define CLEM_ENSONIQ_REG_OSC_ADC    0xe2

//  see doc->osc_flags
#define CLEM_ENSONIQ_OSC_FLAG_CYCLE 0x01
#define CLEM_ENSONIQ_OSC_FLAG_OIR   0x02

/* enable/disable certain compile time diagnostics */

#define CLEM_AUDIO_DIAGNOSTICS 0

#endif
