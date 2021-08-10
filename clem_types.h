#ifndef CLEM_TYPES_H
#define CLEM_TYPES_H

#include <stdbool.h>
#include <stdint.h>

#include "clem_defs.h"

typedef uint64_t clem_clocks_time_t;
typedef uint32_t clem_clocks_duration_t;

#define CLEM_TIME_UNINITIALIZED         ((clem_clocks_time_t)(-1))

struct ClemensClock {
    clem_clocks_time_t ts;
    clem_clocks_duration_t ref_step;
};

/* Typically used as parameters to MMIO functions that require context on how
   they were called (as part of a MMIO read or write operation)
*/
#define CLEM_IO_READ                0x00
#define CLEM_IO_WRITE               0x01


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
 * @brief Real time clock device and BRAM interface
 *
 */
struct ClemensDeviceRTC {
    clem_clocks_time_t xfer_started_time;
    clem_clocks_duration_t xfer_latency_duration;

    unsigned state;
    unsigned index;
    unsigned flags;

    unsigned seconds_since_1904;

    uint8_t bram[256];

    /*  these values are set by the app */
    uint8_t data_c033;
    uint8_t ctl_c034;
};

struct ClemensDeviceKeyboard {
    uint8_t keys[CLEM_ADB_KEYB_BUFFER_LIMIT];
    uint8_t states[CLEM_ADB_KEY_CODE_LIMIT];    // should be ascii, so 128
    int size;
    int delay_ms;
    int rate_per_sec;
    int timer_us;
    int repeat_count;
    uint8_t last_a2_key_down;
    bool reset_key;
};

struct ClemensDeviceMouse {
    int x;
    int y;
};

struct ClemensDeviceGameport {
    uint8_t paddle[4];
    uint8_t btn_mask;
    uint8_t ann_mask;
};

struct ClemensDeviceADB {
    unsigned state;
    unsigned version;           /* Different ROMs expect different versions */
    unsigned poll_timer_us;     /* 60 hz timer (machine time) */
    unsigned mode_flags;        /* ADB modes */
    bool is_keypad_down;        /* Used to determine keypad modifier status */
    bool is_asciikey_down;      /* Used to determine c010 anykey down status */
    bool has_modkey_changed;    /* FIXME: Used for modifier key latch? */

    uint8_t io_key_last_ascii;  /* The last ascii key pressed, bit 7 strobe */

    /* Host-GLU registers */
    uint16_t keyb_reg[4];       /**< mocked GLU keyboard registers */
    uint16_t mouse_reg[4];      /**< mocked GLU mouse registers */

    uint8_t cmd_reg;            /**< command type */
    uint8_t cmd_flags;          /**< meant to reflect C026 when not data */
    uint8_t cmd_status;         /**< meant to approximately reflect C027 */
    uint8_t cmd_data_limit;     /**< expected cnt of bytes for send/recv */
    uint8_t cmd_data_sent;      /**< current index into cmd_data sent 2-way */
    uint8_t cmd_data_recv;      /**< current index into cmd_data recv 2-way */
    uint8_t cmd_data[16];       /**< command data */

    struct ClemensDeviceKeyboard keyb;
    struct ClemensDeviceMouse mouse;
    struct ClemensDeviceGameport gameport;

    uint8_t ram[256];           /**< Microcontroller RAM */

    uint32_t irq_line;          /**< IRQ flags passed to machine */
};

struct ClemensDeviceSCC {
    clem_clocks_time_t ts_last_frame;

    /** Internal state that drives how the cmd/data registers are interpreted */
    unsigned state;
    unsigned selected_reg[2];

    uint8_t serial[2];          /**< See CLEM_SCC_PORT_xxx */

    uint32_t irq_line;          /**< IRQ flags passed to machine */
};

/**
 * @brief This buffer is supplied by the host and represents a complete
 * 16-bit stereo PCM buffer
 *
 * This buffer is written to by the clemens machine, and consumed by the host
 * as input to the audio device's playback buffer.
 *
 */
struct ClemensAudioMixBuffer {
    uint8_t* data;
    unsigned stride;
    unsigned frame_count;
    unsigned frames_per_second; /**< target audio frequency */
};

struct ClemensDeviceAudio {
    uint8_t sound_ram[65536];

    unsigned address;           /**< 16-bit address into RAM or registers */
    unsigned ram_read_cntr;     /**< RAM read counter, reset on address change */

    uint8_t doc_reg[256];       /**< DOC register values */

    bool addr_auto_inc;         /**< Address auto incremented on access */
    bool is_access_ram;         /**< If true, sound RAM, if false, registers */
    bool is_busy;               /**< DOC busy */

    /* settings */
    uint8_t volume;             /**< 0 - 15 */

    /* host supplied mix buffer */
    struct ClemensAudioMixBuffer mix_buffer;
    clem_clocks_time_t ts_last_frame;
    clem_clocks_duration_t dt_mix_frame;
    clem_clocks_duration_t dt_mix_sample;
    unsigned mix_frame_index;

    /* test code */
    float tone_frame_delta;
    float tone_theta;

    /* the device's IRQ line */
    uint32_t irq_line;
};

/** Really, this is part of the RTC/VGC, but for separation of concerns,
 *  pulling out into its own component
 */
struct ClemensDeviceTimer {
    unsigned irq_1sec_us;       /**< used to trigger IRQ one sec */
    unsigned irq_qtrsec_us;     /**< used to trigger IRQ quarter sec */
    unsigned flags;             /**< interrupt  */
    uint32_t irq_line;          /**< IRQ flags passed to machine */
};

/**
 * @brief Identifies the input event sent to the ADB controller
 *
 */
enum ClemensInputType {
    kClemensInputType_None,
    kClemensInputType_KeyDown,
    kClemensInputType_KeyUp,
    kClemensInputType_MouseButtonDown,
    kClemensInputType_MouseButtonUp
};

/**
 * @brief Consolidated input structure passed into the emulator
 *
 * Input is dispatched to the ADB device, which then provides input data to
 * the ClemensMachine
 *
 */
struct ClemensInputEvent {
    enum ClemensInputType type;
    /* value based on the input type (ADB keycode, mouse or gamepad button) */
    unsigned value;
};

struct ClemensDebugJSRContext {
    unsigned adr;
    unsigned jmp;
    uint16_t sp;
};

struct ClemensDeviceDebugger {
    unsigned ioreg_read_ctr[256];
    unsigned ioreg_write_ctr[256];
    struct ClemensDebugJSRContext jsr_contexts[CLEM_DEBUG_JSR_CONTEXT_LIMIT];
    unsigned jsr_context_count;
};

/**
 * @brief Each scanline contains offsets into different bank memory regions
 *
 */
struct ClemensScanline {
    unsigned offset;
    unsigned meta;
};

struct ClemensVGC {
    struct ClemensScanline text_1_scanlines[CLEM_VGC_TEXT_SCANLINE_COUNT];
    struct ClemensScanline text_2_scanlines[CLEM_VGC_TEXT_SCANLINE_COUNT];
    struct ClemensScanline hgr_1_scanlines[CLEM_VGC_HGR_SCANLINE_COUNT];
    struct ClemensScanline hgr_2_scanlines[CLEM_VGC_HGR_SCANLINE_COUNT];
    struct ClemensScanline shgr_scanlines[CLEM_VGC_SHGR_SCANLINE_COUNT];

    /* Used for precise-ish timing of vertical blank and scanline irqs */
    clem_clocks_time_t ts_last_frame;
    clem_clocks_time_t ts_scanline_0;
    clem_clocks_duration_t dt_scanline;


    /* amalgom of possible display modes */
    unsigned mode_flags;
    unsigned text_fg_color;
    unsigned text_bg_color;
    unsigned text_language;

    uint32_t irq_line;          /**< IRQ flags passed to machine */
};

/**
 * IWM emulation of c0x0-c0xf for IWM devices.   Note that the IWM can only
 * access one drive at a time (in tandem with the disk interface register)
 *
 */
struct ClemensDeviceIWM {
    /** A reference clocks value at the last disk update. */
    clem_clocks_time_t last_clocks_ts;
    /** Used for async write timing */
    clem_clocks_time_t last_write_clocks_ts;
    /** Used for lss processing per frame */
    clem_clocks_duration_t lss_clocks_lag;

    /** Drive I/O */
    unsigned io_flags;          /**< Disk port I/O flags */
    unsigned out_phase;         /**< PH0-PH3 bits sent to drive */

    /** Internal Registers */
    uint8_t data;               /**< IO switch data (D0-D7) */
    uint8_t latch;              /**< data latch (work register for IWM) */
    uint8_t write_out;          /**< TODO: Remove.. written byte out */
    uint8_t disk_motor_on;      /**< bits 0-3 represent ports 4-7 */

    bool q6_switch;             /**< Q6 state switch */
    bool q7_switch;             /**< Q7 stage switch */
    bool timer_1sec_disabled;   /**< Turn motor off immediately */
    bool async_write_mode;      /**< If True, IWM delays writes until ready */
    bool latch_mode;            /**< If True, latch value lasts for full 8 xfer */
    bool clock_8mhz;            /**< If True, 8mhz clock - never used? */
    bool enable_debug;          /**< If True, activates file logging */

    unsigned state;             /**< The current IWM register state */
    unsigned ns_latch_hold;     /**< The latch value expiration timer */
    unsigned ns_drive_hold;     /**< Time until drive motor off */
    unsigned lss_state;         /**< State of our custom LSS */
    unsigned lss_write_counter; /**< Used for detecting write underruns */
    unsigned lss_update_dt_ns;  /**< Fast mode = 250ns, Slow = 500ns */

    uint64_t debug_timer_ns;
    uint32_t debug_value;       /**< option displayed during iwm_debug_event */
};



/**
 * @brief FPI + MEGA2 MMIO Interface
 *
 */
struct ClemensMMIO {
    /* Provides remapping of memory read/write access per bank.  For the IIgs,
       this map covers shadowed memory as well as language card and main/aux
       bank access.
    */
    struct ClemensMMIOPageMap* bank_page_map[256];
    /* The different page mapping types */
    struct ClemensMMIOPageMap fpi_direct_page_map;
    struct ClemensMMIOPageMap fpi_main_page_map;
    struct ClemensMMIOPageMap fpi_aux_page_map;
    struct ClemensMMIOPageMap fpi_rom_page_map;
    struct ClemensMMIOPageMap mega2_main_page_map;
    struct ClemensMMIOPageMap mega2_aux_page_map;
    struct ClemensMMIOPageMap empty_page_map;

    /* Shadow maps for bank 00, 01 */
    struct ClemensMMIOShadowMap fpi_mega2_main_shadow_map;
    struct ClemensMMIOShadowMap fpi_mega2_aux_shadow_map;

    /* All devices */
    struct ClemensVGC vgc;
    struct ClemensDeviceRTC dev_rtc;
    struct ClemensDeviceADB dev_adb;
    struct ClemensDeviceTimer dev_timer;
    struct ClemensDeviceDebugger dev_debug;
    struct ClemensDeviceAudio dev_audio;
    struct ClemensDeviceIWM dev_iwm;
    struct ClemensDeviceSCC dev_scc;

    /* Registers that do not fall easily within a device struct */
    uint32_t mmap_register;     // memory map flags- CLEM_MMIO_MMAP_
    uint8_t new_video_c029;     // see kClemensMMIONewVideo_xxx
    uint8_t speed_c036;         // see kClemensMMIOSpeed_xxx
    uint8_t flags_c08x;         // used to detect double reads

    uint64_t mega2_cycles;      // number of mega2 pulses/ticks since startup
    uint32_t timer_60hz_us;     // used for executing logic per 1/60th second
    int32_t card_expansion_rom_index;   // card slot has the mutex on C800-CFFF

    /* All ticks are mega2 cycles */
    uint32_t irq_line;          // see CLEM_IRQ_XXX flags, if !=0 triggers irqb
};

/*  ClemensDrive Data
 */

struct ClemensWOZDisk;

/**
 * @brief
 *
 */
struct ClemensDrive {
    struct ClemensWOZDisk* data;    /**< The parsed WOZ data */
    int qtr_track_index;        /**< Current track position of the head */
    unsigned track_byte_index;  /**< byte index into track */
    unsigned track_bit_shift;   /**< bit offset into current byte */
    unsigned track_bit_length;  /**< current track bit length */
    unsigned pulse_ns;          /**< nanosecond timer for pulse input */
    unsigned zero_count;        /**< number of non-pulses found in succession */

    /**
     * 4-bit Q0-3 entry 5.25" = stepper control
     * Control/Status/Strobe bits for 3.5"
     */
    unsigned q03_switch;
    unsigned cog_orient;        /**< emulated orientation of stepper cog */
    unsigned state_35;          /**< 3.5" state machine */
    unsigned query_35;          /**< 3.5" status query  */
    unsigned step_timer_35_ns;  /**< 3.5" track step timer */
    bool select_35;             /**< 3.5" select state */
    bool write_pulse;           /**< Changes in the write field translate as pulses */

    uint32_t random_bits[8];    /**< used for random pulse generation */
    uint8_t random_bit_index;   /**< bit index into 32-byte buffer */
    uint8_t real_track_index;   /**< the index into the raw woz track data */
};

struct ClemensDriveBay {
    struct ClemensDrive slot5[2];
    struct ClemensDrive slot6[2];
};

/**
 * @brief
 *
 */
enum ClemensDriveType {
    kClemensDrive_3_5,
    kClemensDrive_3_5_D1 = kClemensDrive_3_5,
    kClemensDrive_3_5_D2,
    kClemensDrive_5_25,
    kClemensDrive_5_25_D1 = kClemensDrive_5_25,
    kClemensDrive_5_25_D2
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
    uint32_t cycles_spent;
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
    bool irqbIn;                        // Interrupt Request
    bool nmiIn;                         // Non-Maskable Interrupt
    bool readyOut;                      // if false, then WAIT
    bool resbIn;                        // RESET
    bool emulation;                     // Emulation Status
    bool vdaOut;                        // Valid Data Address
    bool vpaOut;                        // Valid Program Address
    bool rwbOut;                        // Read/Write byte
    /*
    bool vpbOut;                        // Vector Pull
    bool mlbOut;                        // Memory Lock
    */
};

enum ClemensCPUStateType {
    kClemensCPUStateType_None,
    kClemensCPUStateType_Reset,
    kClemensCPUStateType_Execute,
    kClemensCPUStateType_IRQ
};

struct Clemens65C816 {
    struct ClemensCPUPins pins;
    struct ClemensCPURegs regs;
    enum ClemensCPUStateType state_type;
    uint32_t cycles_spent;
    bool enabled;           // set to false by STP, and true by RESET
};

enum {
    kClemensDebugFlag_None              = 0,
    kClemensDebugFlag_StdoutOpcode      = (1 << 0),
    kClemensDebugFlag_OpcodeCallback    = (1 << 1),
    kClemensDebugFlag_DebugLogOpcode    = (1 << 2)
};

typedef void (*ClemensOpcodeCallback)(struct ClemensInstruction*,
                                      const char*, void*);


/**
 * @brief
 *
 */
enum ClemensVideoFormat {
    kClemensVideoFormat_Text,
    kClemensVideoFormat_Text_Alternate,
    kClemensVideoFormat_Lores,
    kClemensVideoFormat_Hires,
    kClemensVideoFormat_Double_Hires,
    kClemensVideoFormat_Count
};

/**
 * @brief
 *
 */
typedef struct {
    struct ClemensScanline* scanlines;
    int scanline_byte_cnt;
    int scanline_start;
    int scanline_count;
    enum ClemensVideoFormat format;
} ClemensVideo;

typedef struct {
    unsigned int signal;        /**< See CLEM_MONITOR_xxx */
    unsigned int color;         /**< See CLEM_MONITOR_xxx */
    unsigned width;
    unsigned height;
    unsigned border_color;      /**< See CLEM_VGC_COLOR_xxx */
    unsigned text_color;        /**< bits 0-3 = foreground, 4-7 = background */
} ClemensMonitor;

typedef struct {
    uint8_t* data;              /** format will always by 16-bit pcm stereo */
    unsigned frame_total;       /** total number of frames in the buffer */
    unsigned frame_start;       /** --frame-- index into the data buffer */
    unsigned frame_count;       /** --frame-- count (this can wrap around) */
    unsigned frame_stride;      /** each frame is this size */
} ClemensAudio;

/**
 * @brief
 *
 */
typedef struct {
    struct Clemens65C816 cpu;
    /* clocks spent per cycle as set by the current speed settings */
    clem_clocks_duration_t clocks_step;
    /* clocks spent per cycle in fast mode */
    clem_clocks_duration_t clocks_step_fast;
    /* typically FPI speed mhz * clocks_step_fast */
    clem_clocks_duration_t clocks_step_mega2;
    /* clock timer - never change once system has been started */
    clem_clocks_time_t clocks_spent;

    uint8_t* fpi_bank_map[256];     // $00 - $ff
    uint8_t* mega2_bank_map[2];     // $e0 - $e1
    /** ROM memory for individual card slots.  Only slots 1,2,4,5,6,7 are used
     *  and each slot should have 256 bytes allocated for it */
    uint8_t* card_slot_memory[7];
    /** Expansion ROM area for each card.  This area is paged into addressable
     *  memory with the correct IO instructions.  Each area should be 2K in
     *  size.  As with card slot memory, slot 3 is ignored */
    uint8_t* card_slot_expansion_memory[7];

    /** Internal, tracks cycle count for holding down the reset key */
    int resb_counter;

    struct ClemensMMIO mmio;
    struct ClemensDriveBay active_drives;

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
