//  support read or write operations
#define CLEM_MMIO_PAGE_WRITE_OK     0x00000001
//  use a mask of the requested bank and the 17th address bit of the read/write
//  bank
#define CLEM_MMIO_PAGE_MAINAUX      0x10000000
//  use the original bank register
#define CLEM_MMIO_PAGE_DIRECT       0x40000000
//  redirects to Mega2 I/O registers
#define CLEM_MMIO_PAGE_IOADDR       0x80000000

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

/**
 * Selects ROM source for the CXXX pages
 */
#define CLEM_MMIO_REG_READCXROM    0x15
/**
 * Primarily defines how memory is accessed by the video controller, with
 * the bank latch bit (0), which is always set to 1 AFAIK
 */
#define CLEM_MMIO_REG_NEWVIDEO     0x29
/**
 * Defines what areas of the FPI banks are disabled, and how I/O language
 * card space is treated on FPI bank 0 and 1.
 */
#define CLEM_MMIO_REG_SHADOW       0x35
/**
 * Defines fast/slow processor speed, system-wide shadowing behavior and other
 * items... (disk input?)
 */
#define CLEM_MMIO_REG_SPEED        0x36
/**
 * Amalgom of the C08x registers
 */
#define CLEM_MMIO_REG_STATEREG     0x68
