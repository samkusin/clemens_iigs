//  support read or write operations
#define CLEM_MMIO_PAGE_WRITE_OK     0x00000001
//  use the original bank register
#define CLEM_MMIO_PAGE_DIRECT       0x20000000
//  redirects to Mega2 I/O registers
#define CLEM_MMIO_PAGE_IOADDR       0x40000000
//  shadows to even or odd mega2 bank based on original bank register bit 0
#define CLEM_MMIO_PAGE_SHADOWED     0x80000000

/**
 * Selects ROM source for the CXXX pages
 */
#define CLEM_MMIO_ADDR_SLOTCXROM    0xC015
/**
 * Primarily defines how memory is accessed by the video controller, with
 * the bank latch bit (0), which is always set to 1 AFAIK
 */
#define CLEM_MMIO_ADDR_NEWVIDEO     0xC029
/**
 * Defines what areas of the FPI banks are disabled, and how I/O language
 * card space is treated on FPI bank 0 and 1.
 */
#define CLEM_MMIO_ADDR_SHADOW       0xC035
/**
 * Defines fast/slow processor speed, system-wide shadowing behavior and other
 * items... (disk input?)
 */
#define CLEM_MMIO_ADDR_SPEED        0xC036
/**
 * Amalgom of the C08x registers
 */
#define CLEM_MMIO_ADDR_STATEREG     0xC068
