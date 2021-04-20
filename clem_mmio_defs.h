//  support read or write operations
#define CLEM_MMIO_PAGE_WRITE_OK     0x00000001
//  use a mask of the requested bank and the 17th address bit of the read/write
//  bank
#define CLEM_MMIO_PAGE_MAINAUX      0x10000000
//  use the original bank register
#define CLEM_MMIO_PAGE_DIRECT       0x20000000
//  use card memory vs internal
#define CLEM_MMIO_PAGE_CARDMEM      0x40000000
//  redirects to Mega2 I/O registers
#define CLEM_MMIO_PAGE_IOADDR       0x80000000

//  convenience flags to identify source of memory
#define CLEM_MMIO_BANK_MEMORY       0x30000000
#define CLEM_MMIO_IO_MEMORY         0xC0000000
#define CLEM_MMIO_PAGE_TYPE_MASK    0xF0000000

//  These flags refer to bank 0 memory switches for address bit 17
//  0 = Main Bank, 1 = Aux Bank ZP, Stack and Language Card
#define CLEM_MMIO_MMAP_ALTZPLC      0x00000001
//  0 = Main Bank RAM Read Enabled, 1 = Aux Bank RAM Read Enabled
#define CLEM_MMIO_MMAP_RAMRD        0x00000002
//  0 = Main Bank RAM Write Enabled, 1 = Aux Bank RAM Write Enabled
#define CLEM_MMIO_MMAP_RAMWRT       0x00000004

//  Bits 4-7 These flags refer to the language card banks
#define CLEM_MMIO_MMAP_LC           0x000000f0
//  0 = Read LC ROM, 1 = Read LC RAM
#define CLEM_MMIO_MMAP_RDLCRAM      0x00000010
//  0 = Write protect LC RAM, 1 = Write enable LC RAM
#define CLEM_MMIO_MMAP_WRLCRAM      0x00000020
//  0 = LC Bank 1, 1 = LC Bank 2
#define CLEM_MMIO_MMAP_LCBANK2      0x00000040
//  0 = Internal ROM, 1 = Peripheral ROM
#define CLEM_MMIO_MMAP_C1ROM        0x00000100
#define CLEM_MMIO_MMAP_C2ROM        0x00000200
#define CLEM_MMIO_MMAP_C3ROM        0x00000400
#define CLEM_MMIO_MMAP_C4ROM        0x00000800
#define CLEM_MMIO_MMAP_C5ROM        0x00001000
#define CLEM_MMIO_MMAP_C6ROM        0x00002000
#define CLEM_MMIO_MMAP_C7ROM        0x00004000
#define CLEM_MMIO_MMAP_CXROM        0x00008000
#define CLEM_MMIO_MMAP_CROM         0x0000ff00

// Bits 16-23 These flags refer to shadow register controls
#define CLEM_MMIO_MMAP_NSHADOW      0x00ff0000
#define CLEM_MMIO_MMAP_NSHADOW_TXT1 0x00010000
#define CLEM_MMIO_MMAP_NSHADOW_TXT2 0x00020000
#define CLEM_MMIO_MMAP_NSHADOW_HGR1 0x00040000
#define CLEM_MMIO_MMAP_NSHADOW_HGR2 0x00080000
#define CLEM_MMIO_MMAP_NSHADOW_SHGR 0x00100000
#define CLEM_MMIO_MMAP_NSHADOW_AUX  0x00200000

//  0 = Bank 00: I/O enabled + LC enabled,  1 = I/O disabled + LC disabled
#define CLEM_MMIO_MMAP_NIOLC        0x01000000

/** Flags for _clem_mmio_read */
#define CLEM_MMIO_READ_NO_OP        0x01

#define CLEM_IS_MMIO_READ_NO_OP(_flags_) (((_flags_) & CLEM_MMIO_READ_NO_OP) != 0)

/**
 * IO Registers
 */
/** Keyboard data (bits 6-0) and strobe (bit 7) */
#define CLEM_MMIO_REG_KEYB_READ         0x00

/** Write to this register to set PAGE2 to flip between text pages, 40 column
 *  text pages */
#define CLEM_MMIO_REG_80STOREOFF_WRITE  0x00
/** Write to this register to set PAGE2 to switch between main and aux, ala
 *  80 column text */
#define CLEM_MMIO_REG_80STOREON_WRITE   0x01
/** Write to enable peripheral ROM for C100 - C7FF */
#define CLEM_MMIO_REG_SLOTCXROM         0x06
/** Write to enable internal ROM for C100 - C7FF */
#define CLEM_MMIO_REG_INTCXROM          0x07
/** Write to enable main bank Page 0, Page 1 and LC */
#define CLEM_MMIO_REG_STDZP             0x08
/** Write to enable aux bank Page 0, Page 1 and LC */
#define CLEM_MMIO_REG_ALTZP             0x09
/** Write to enable peripheral ROM for C300 */
#define CLEM_MMIO_REG_SLOTC3ROM         0x0A
/** Write to enable internal ROM for C300 */
#define CLEM_MMIO_REG_INTC3ROM          0x0B
/** Read bit 7 for 'any-key down', read or write to clear strobe bit in $C000,
 *  and also provides the last key down - not clear in the //e or IIgs docs.
 * (per https://apple2.org.za/gswv/a2zine/faqs/csa2pfaq.html)
*/
#define CLEM_MMIO_REG_ANYKEY_STROBE     0x10
/** Read and test bit 7, 0 = LC bank 1, 1 = bank 2 */
#define CLEM_MMIO_REG_LC_BANK_TEST      0x11
/** Read and test bit 7, 0 = ROM, 1 = RAM */
#define CLEM_MMIO_REG_ROM_RAM_TEST      0x12
/** 1 = slot ROM, 0 = internal ROM as source for the CXXX pages */
#define CLEM_MMIO_REG_READCXROM         0x15
/** Read bit 7 to detect bank 0 = main, 1 = aux bank */
#define CLEM_MMIO_REG_RDALTZP           0x16
/** Get ROM source for the C300 page */
#define CLEM_MMIO_REG_READC3ROM         0x17
/** Mouse button (bit 7) and movement status (btis 6:0) */
#define CLEM_MMIO_REG_ADB_MOUSE_DATA    0x24
/** Mask indicating which modifier keys are pressed */
#define CLEM_MMIO_REG_ADB_MODKEY        0x25
/** ADB GLU Command Data register */
#define CLEM_MMIO_REG_ADB_CMD_DATA      0x26
/** ADB status (key/mouse)  register */
#define CLEM_MMIO_REG_ADB_STATUS        0x27

/**
 * Primarily defines how memory is accessed by the video controller, with
 * the bank latch bit (0), which is always set to 1 AFAIK
 */
#define CLEM_MMIO_REG_NEWVIDEO          0x29
/** Selects Internal vs Peripheral ROM for slots 1 - 7, bit 0, 3 must be 0 */
#define CLEM_MMIO_REG_SLOTROMSEL        0x2D
/** Real time clock data register */
#define CLEM_MMIO_REG_RTC_DATA          0x33
/** Real time clock + border colorl joint register */
#define CLEM_MMIO_REG_RTC_CTL           0x34
/**
 * Defines what areas of the FPI banks are disabled, and how I/O language
 * card space is treated on FPI bank 0 and 1.
 */
#define CLEM_MMIO_REG_SHADOW            0x35
/**
 * Defines fast/slow processor speed, system-wide shadowing behavior and other
 * items... (disk input?)
 */
#define CLEM_MMIO_REG_SPEED             0x36
/** SCC Command Register A */
#define CLEM_MMIO_REG_SCC_A_CMD         0x39
/** SCC Data Register A */
#define CLEM_MMIO_REG_SCC_A_DATA        0x3B
/** SCC Command Register B */
#define CLEM_MMIO_REG_SCC_B_CMD         0x38
/** SCC Data Register B */
#define CLEM_MMIO_REG_SCC_B_DATA        0x3A
/** Various Mega II specific interrupt flags likely used only by firmware */
#define CLEM_MMIO_REG_DIAG_INTTYPE      0x46

/** Joystick Button 3 */
#define CLEM_MMIO_REG_BTN3              0x62
/** Open Apple Key or Joystick Button 0 */
#define CLEM_MMIO_REG_BTN0              0x61
/** Solid Apple Key or Joystick Button 1 */
#define CLEM_MMIO_REG_BTN1              0x62
/** Joystick Button 2 */
#define CLEM_MMIO_REG_BTN2              0x63
/**
 * Amalgom of the C08x registers
 */
#define CLEM_MMIO_REG_STATEREG          0x68

/** R1 - LC Bank 2, Read RAM, Write Protect */
#define CLEM_MMIO_REG_LC2_RAM_WP        0x80
/** R2 - LC Bank 2, Read ROM, Write Enable */
#define CLEM_MMIO_REG_LC2_ROM_WE        0x81
/** R1 - LC Bank 2, Read ROM, Write Protect */
#define CLEM_MMIO_REG_LC2_ROM_WP        0x82
/** R2 - LC Bank 2, Read and Write Enable */
#define CLEM_MMIO_REG_LC2_RAM_WE        0x83
/** R1 - LC Bank 1, Read RAM, Write Protect */
#define CLEM_MMIO_REG_LC1_RAM_WP        0x88
/** R2 - LC Bank 1, Read ROM, Write Enable */
#define CLEM_MMIO_REG_LC1_ROM_WE        0x89
/** R1 - LC Bank 2, Read ROM, Write Protect */
#define CLEM_MMIO_REG_LC1_ROM_WP        0x8A
/** R2 - LC Bank 1, Read and Write Enable */
#define CLEM_MMIO_REG_LC1_RAM_WE        0x8B

/** New video (C029) bitflag defines */
#define CLEM_MMIO_NEWVIDEO_BANKLATCH_INHIBIT  0x01

/** Speed register (C036) bitflag defines */
#define CLEM_MMIO_SPEED_POWERED_ON      0x40
#define CLEM_MMIO_SPEED_FAST_ENABLED    0x80

/** Interrupt type register (consolidated with Mega II) */
#define CLEM_MMIO_INTTYPE_IRQ           0x01

/** Timer (internal, C023 partial) device flags */
#define CLEM_MMIO_TIMER_1SEC_ENABLED    0x00000040
#define CLEM_MMIO_TIMER_QSEC_ENABLED    0x00000100
