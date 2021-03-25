#ifndef CLEM_TYPES_H
#define CLEM_TYPES_H

#include <stdbool.h>
#include <stdint.h>

#include "clem_defs.h"


/* Note that in emulation mode, the EmulatedBrk flag should be
   stored in the status register - for our purposes we mock this
   behvaior only when the application has access to the status
   register in emulation mode (i.e. PHP, PLP)

   We do this to limit the number of conditional checks for the
   X status register in instructions (always set to 1 in emulation
   mode, better than a check for two flags - emulation and x_status)
*/
enum {
    kClemensCPUStatus_Carry              = (1 << 0),     // C
    kClemensCPUStatus_Zero               = (1 << 1),     // Z
    kClemensCPUStatus_IRQDisable         = (1 << 2),     // I
    kClemensCPUStatus_Decimal            = (1 << 3),     // D
    kClemensCPUStatus_Index              = (1 << 4),     // X,
    kClemensCPUStatus_EmulatedBrk        = (1 << 4),     // B on 6502
    kClemensCPUStatus_MemoryAccumulator  = (1 << 5),     // M,
    kClemensCPUStatus_Overflow           = (1 << 6),     // V
    kClemensCPUStatus_Negative           = (1 << 7)      // N
};

enum ClemensCPUAddrMode {
    kClemensCPUAddrMode_None,
    kClemensCPUAddrMode_Immediate,
    kClemensCPUAddrMode_Absolute,
    kClemensCPUAddrMode_AbsoluteLong,
    kClemensCPUAddrMode_DirectPage,
    kClemensCPUAddrMode_DirectPageIndirect,
    kClemensCPUAddrMode_DirectPageIndirectLong,
    kClemensCPUAddrMode_Absolute_X,
    kClemensCPUAddrMode_AbsoluteLong_X,
    kClemensCPUAddrMode_Absolute_Y,
    kClemensCPUAddrMode_DirectPage_X,
    kClemensCPUAddrMode_DirectPage_Y,
    kClemensCPUAddrMode_DirectPage_X_Indirect,
    kClemensCPUAddrMode_DirectPage_Indirect_Y,
    kClemensCPUAddrMode_DirectPage_IndirectLong_Y,
    kClemensCPUAddrMode_MoveBlock,
    kClemensCPUAddrMode_Stack_Relative,
    kClemensCPUAddrMode_Stack_Relative_Indirect_Y,
    kClemensCPUAddrMode_PCRelative,
    kClemensCPUAddrMode_PCRelativeLong,
    kClemensCPUAddrMode_PC,
    kClemensCPUAddrMode_PCIndirect,
    kClemensCPUAddrMode_PCIndirect_X,
    kClemensCPUAddrMode_PCLong,
    kClemensCPUAddrMode_PCLongIndirect,
    kClemensCPUAddrMode_Operand
};

struct ClemensOpcodeDesc {
    enum ClemensCPUAddrMode addr_mode;
    char name[4];
};

static struct ClemensOpcodeDesc sOpcodeDescriptions[256];

struct ClemensInstruction {
    struct ClemensOpcodeDesc* desc;
    uint16_t addr;
    uint16_t value;
    uint8_t pbr;
    uint8_t bank;
    bool opc_8;
};

struct ClemensCPURegs {
    uint16_t A;
    uint16_t X;
    uint16_t Y;
    uint16_t D;                         // Direct
    uint16_t S;                         // Stack
    uint16_t PC;                        // Program Counter
    uint8_t IR;                         // Instruction Register
    uint8_t P;                          // Processor Status
    uint8_t DBR;                        // Data Bank (Memory)
    uint8_t PBR;                        // Program Bank (Memory)
};

struct ClemensCPUPins {
    uint16_t adr;                       // A0-A15 Address
    uint8_t bank;                       // bank
    uint8_t data;                       // data
    bool abortIn;                       // ABORTB In
    bool busEnableIn;                   // Bus Enable
    bool irqIn;                         // Interrupt Request
    bool nmiIn;                         // Non-Maskable Interrupt
    bool readyOut;                      // if false, then WAIT
    bool resbIn;                        // RESET
    bool emulation;                     // Emulation Status
    /*
    bool vpbOut;                        // Vector Pull
    bool rwbOut;                        // Read/Write byte
    bool vdaOut;                        // Valid Data Address
    bool vpaOut;                        // Valid Program Address
    bool mlbOut;                        // Memory Lock
    */
};

enum ClemensCPUStateType {
    kClemensCPUStateType_None,
    kClemensCPUStateType_Reset,
    kClemensCPUStateType_Execute
};

struct Clemens65C816 {
    struct ClemensCPUPins pins;
    struct ClemensCPURegs regs;
    enum ClemensCPUStateType state_type;
    uint32_t cycles_spent;
    bool enabled;           // set to false by STP, and true by RESET
};


/** NewVideo Register $C029 bits 4-1 ignored */
enum {
    // If 0, use all other Apple II video modes, else Super Hires
    kClemensMMIONewVideo_SHGR_Enable        = (1 << 7),
    // If 0, for Apple II video memory layout, else Super Hires (contiguous)
    kClemensMMIONewVideo_SHGR_Memory_Enable = (1 << 6),
    // If 0, color mode (140 x 192 16 colors), else 560 x 192 mono
    kClemensMMIONewVideo_AUXHGR_Color_Inhibit = (1 << 5),
    // If The docs here are a bit contradictory
    //  Hardware Reference: p16 vs p89
    //  On = 17th bit (D0) used for auxillary bank addressing, but can also use
    //      softswitches?
    //  Off = use soft-switches only through 00,e0
    //  This relies on the shadow register as well?
    kClemensMMIONewVideo_BANKLATCH_Inhibit      = (1 << 0)
};

/** Speed Register $C036 */
enum {
    //  if 1, then fastest mode enabled? 2.8mhz
    kClemensMMIOSpeed_FAST_Enable       = (1 << 7),
    //  always 1 on ROM 03, unknown on ROM 01, older GS devices
    kClemensMMIOSpeed_PoweredOn         = (1 << 6)
};


struct ClemensMMIOPageInfo {
    uint8_t read;
    uint8_t write;
    uint8_t bank_read;
    uint8_t bank_write;
    uint32_t flags;
};

struct ClemensMMIOShadowMap {
    uint8_t pages[256];
};

struct ClemensMMIOPageMap {
  struct ClemensMMIOPageInfo pages[256];
  struct ClemensMMIOShadowMap* shadow_map;
};


/**
 * @brief Real time clock device interface
 *
 */
struct ClemensDeviceRTC {
    uint8_t inb_c033;
    uint8_t outb_c033;
    uint8_t screen_border_color;    // color indices 0 - 15

};

struct ClemensMMIO {
    uint32_t mmap_register; // consolidated memory map flags- CLEM_MMIO_MMAP_
    uint8_t new_video_c029; // see kClemensMMIONewVideo_xxx
    uint8_t speed_c036;     // see kClemensMMIOSpeed_xxx
    uint8_t rtc_inb_c033;   // RTC inbound data register storage
    uint8_t rtc_outb_c033;  // RTC outbound data register storage
    uint8_t flags_c08x;     // used to detect double reads

    /* Provides remapping of memory read/write access per bank.  For the IIgs,
       this map covers shadowed memory as well as language card and main/aux
       bank access.
    */
    struct ClemensMMIOPageMap* bank_page_map[256];
    struct ClemensMMIOPageMap fpi_direct_page_map;
    struct ClemensMMIOPageMap fpi_main_page_map;
    struct ClemensMMIOPageMap fpi_aux_page_map;
    struct ClemensMMIOPageMap fpi_rom_page_map;
    struct ClemensMMIOPageMap mega2_main_page_map;
    struct ClemensMMIOPageMap mega2_aux_page_map;

    struct ClemensMMIOShadowMap fpi_mega2_main_shadow_map;
    struct ClemensMMIOShadowMap fpi_mega2_aux_shadow_map;
};

enum {
    kClemensDebugFlag_None              = 0,
    kClemensDebugFlag_StdoutOpcode      = (1 << 0),
    kClemensDebugFlag_OpcodeCallback    = (1 << 1)
};

typedef void (*ClemensOpcodeCallback)(struct ClemensInstruction*,
                                      const char*, void*);

typedef struct {
    struct Clemens65C816 cpu;
    uint32_t clocks_step;
    uint32_t clocks_step_mega2;     // typically FPI speed mhz * clocks_step
    uint32_t clocks_spent;

    uint8_t* fpi_bank_map[256];     // $00 - $ff
    uint8_t* mega2_bank_map[2];     // $e0 - $e1

    struct ClemensMMIO mmio;

    /* Optional callback for debugging purposes.
       When issued, it's guaranteed that all registers/CPU state has been
       updated (and can be viewed as an accurate state of the machine after
       running the opcode)
    */
    uint32_t debug_flags;           // See enum kClemensDebugFlag_
    void* debug_user_ptr;
    ClemensOpcodeCallback opcode_post;
} ClemensMachine;

#endif
