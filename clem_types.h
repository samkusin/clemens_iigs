#ifndef CLEM_TYPES_H
#define CLEM_TYPES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "clem_defs.h"

typedef struct ClemensMachine ClemensMachine;
typedef void (*LoggerFn)(int level, ClemensMachine *machine, const char *msg);

#ifdef __cplusplus
extern "C" {
#endif

/* Typically used as parameters to MMIO functions that require context on how
   they were called (as part of a MMIO read or write operation)
*/
#define CLEM_IO_READ  0x00
#define CLEM_IO_WRITE 0x01

struct ClemensMemoryPageInfo {
    uint8_t read;
    uint8_t write;
    uint8_t bank_read;
    uint8_t bank_write;
    uint32_t flags;
};

struct ClemensMemoryShadowMap {
    uint8_t pages[256];
};

struct ClemensMemoryPageMap {
    struct ClemensMemoryPageInfo pages[256];
    struct ClemensMemoryShadowMap *shadow_map;
};

struct ClemensTimeSpec {
    /* clocks spent per cycle as set by the current speed settings */
    clem_clocks_duration_t clocks_step;
    /* clocks spent per cycle in fast mode */
    clem_clocks_duration_t clocks_step_fast;

    /** clock timer - never change once system has been reset */
    clem_clocks_time_t clocks_spent;
    /* this is used for synchronization to the phi0 clock */
    clem_clocks_time_t clocks_next_phi0;
    /** the clocks duration used for the final scanline cycle, which for NTSC is
        the stretch cycle */
    clem_clocks_duration_t phi0_clocks_stretch;
    clem_clocks_duration_t phi0_current_step;
    /** used for calculating the next clock edge for PHI0, and when 0 is at the
    stretch cycle */
    unsigned mega2_scanline_ctr;
};

/* Note that in emulation mode, the EmulatedBrk flag should be
   stored in the status register - for our purposes we mock this
   behvaior only when the application has access to the status
   register in emulation mode (i.e. PHP, PLP)

   We do this to limit the number of conditional checks for the
   X status register in instructions (always set to 1 in emulation
   mode, better than a check for two flags - emulation and x_status)
*/
enum {
    kClemensCPUStatus_Carry = (1 << 0),             // C
    kClemensCPUStatus_Zero = (1 << 1),              // Z
    kClemensCPUStatus_IRQDisable = (1 << 2),        // I
    kClemensCPUStatus_Decimal = (1 << 3),           // D
    kClemensCPUStatus_Index = (1 << 4),             // X,
    kClemensCPUStatus_EmulatedBrk = (1 << 4),       // B on 6502
    kClemensCPUStatus_MemoryAccumulator = (1 << 5), // M,
    kClemensCPUStatus_Overflow = (1 << 6),          // V
    kClemensCPUStatus_Negative = (1 << 7)           // N
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
    kClemensCPUAddrMode_Operand,
    kClemensCPUAddrMode_Count
};

struct ClemensOpcodeDesc {
    enum ClemensCPUAddrMode addr_mode;
    char name[4];
};

struct ClemensInstruction {
    struct ClemensOpcodeDesc *desc;
    uint16_t addr;
    uint16_t value;
    uint8_t pbr;
    uint8_t bank;
    uint8_t opc;
    bool opc_8;
    uint32_t cycles_spent;
};

struct ClemensCPURegs {
    uint16_t A;
    uint16_t X;
    uint16_t Y;
    uint16_t D;  // Direct
    uint16_t S;  // Stack
    uint16_t PC; // Program Counter
    uint8_t IR;  // Instruction Register
    uint8_t P;   // Processor Status
    uint8_t DBR; // Data Bank (Memory)
    uint8_t PBR; // Program Bank (Memory)
};

struct ClemensCPUPins {
    uint16_t adr;     // A0-A15 Address
    uint8_t bank;     // bank
    uint8_t data;     // data
    bool abortIn;     // ABORTB In
    bool busEnableIn; // Bus Enable
    bool irqbIn;      // Interrupt Request
    bool nmibIn;      // Non-Maskable Interrupt
    bool readyOut;    // if false, then WAIT
    bool resbIn;      // RESET
    bool emulation;   // Emulation Status
    bool vdaOut;      // Valid Data Address
    bool vpaOut;      // Valid Program Address
    bool rwbOut;      // Read/Write byte
    bool ioOut;       // IO access (for introspection purposes - not on the 65816)
                      /*
                      bool vpbOut;                        // Vector Pull
                      bool mlbOut;                        // Memory Lock
                      */
};

enum ClemensCPUStateType {
    kClemensCPUStateType_None,
    kClemensCPUStateType_Reset,
    kClemensCPUStateType_Execute,
    kClemensCPUStateType_IRQ,
    kClemensCPUStateType_NMI
};

struct Clemens65C816 {
    struct ClemensCPUPins pins;
    struct ClemensCPURegs regs;
    enum ClemensCPUStateType state_type;
    uint32_t cycles_spent;
    bool enabled; // set to false by STP, and true by RESET
};

enum {
    kClemensDebugFlag_None = 0,
    kClemensDebugFlag_StdoutOpcode = (1 << 0),
    kClemensDebugFlag_OpcodeCallback = (1 << 1),
    kClemensDebugFlag_DebugLogOpcode = (1 << 2)
};

typedef void (*ClemensOpcodeCallback)(struct ClemensInstruction *, const char *, void *);

struct ClemensMemory {
    /* each used bank MUST be 64K (65536) bytes */
    uint8_t *fpi_bank_map[256]; // $00 - $ff
    bool fpi_bank_used[256];
    uint8_t *mega2_bank_map[2]; // $e0 - $e1

    /* Provides remapping of memory read/write access per bank.  For the IIgs,
       this map covers shadowed memory as well as language card and main/aux
       bank access.
    */
    struct ClemensMemoryPageMap *bank_page_map[256];

    /* The MMIO context passed into the memory callbacks to MMIO (for customization) */
    /* THESE MUST BE SET FOR THE IIGS */
    void *mmio_context;
    void (*mmio_write)(struct ClemensMemory *, struct ClemensTimeSpec *, uint8_t /* data */,
                       uint16_t /* addr */, uint8_t /* flags */, bool * /*is_slow_access*/);
    uint8_t (*mmio_read)(struct ClemensMemory *, struct ClemensTimeSpec *, uint16_t /* addr */,
                         uint8_t /* flags*/, bool *);
    bool (*mmio_niolc)(struct ClemensMemory *);
};

struct ClemensDeviceDebugger {
    LoggerFn log_message;
    uint16_t pc; /* these values are passed from the CPU per frame */
    uint8_t pbr;
};

/**
 * @brief
 *
 */
typedef struct ClemensMachine {
    struct Clemens65C816 cpu;
    struct ClemensTimeSpec tspec;
    struct ClemensMemory mem;

    /** Internal, tracks cycle count for holding down the reset key */
    int resb_counter;

    /* Optional callback for debugging purposes.
       When issued, it's guaranteed that all registers/CPU state has been
       updated (and can be viewed as an accurate state of the machine after
       running the opcode)
    */
    struct ClemensDeviceDebugger dev_debug;
    uint32_t debug_flags; // See enum kClemensDebugFlag_
    void *debug_user_ptr;
    /* opcode print callback */
    ClemensOpcodeCallback opcode_post;
    /* logger callback (if NULL, uses stdout) */
    LoggerFn logger_fn;
} ClemensMachine;

#ifdef __cplusplus
}
#endif

#endif
