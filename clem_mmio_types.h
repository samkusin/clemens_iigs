#ifndef CLEM_MMIO_TYPES_H
#define CLEM_MMIO_TYPES_H

#include "clem_types.h"

#include "clem_disk.h"
#include "clem_mmio_defs.h"
#include "clem_smartport.h"

#ifdef __cplusplus
extern "C" {
#endif

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

    uint8_t bram[CLEM_RTC_BRAM_SIZE];

    /*  these values are set by the app */
    uint8_t data_c033;
    uint8_t ctl_c034;
};

struct ClemensDeviceKeyboard {
    uint8_t keys[CLEM_ADB_KEYB_BUFFER_LIMIT];
    uint8_t states[CLEM_ADB_KEY_CODE_LIMIT]; // should be ascii, so 128
    int size;
    int delay_ms;
    int rate_per_sec;
    int timer_us;
    int repeat_count;
    uint8_t last_a2_key_down;
    bool reset_key;
};

struct ClemensDeviceMouse {
    unsigned pos[CLEM_ADB_KEYB_BUFFER_LIMIT];
    int size;
    bool btn_down;
};

struct ClemensDeviceGameport {
    clem_clocks_time_t ts_last_frame;
    /* value is from 0 to CLEM_GAMEPORT_PADDLE_AXIS_VALUE_MAX or at UINT_MAX if
       the value is not set by the host per frame. */
    uint16_t paddle[4];
    /* on PTRIG, paddle_timer[x] takes on the time value calculated from the
       input paddle value (if the paddle value is at UINT_MAX per above, then
       paddle_timer[x] == 0, which becomes a no-op during the sync()).  Otherwise
       every frame the timer value is decremented and when reaching 0, flips the
       paddle high bit off at PADDLn */
    uint32_t paddle_timer_ns[4];
    /* C064...C067 bit 7 maps to items 0 - 4. */
    uint8_t paddle_timer_state[4];
    uint8_t btn_mask[2];
    uint8_t ann_mask;
};

struct ClemensDeviceADB {
    unsigned state;
    unsigned version;        /* Different ROMs expect different versions */
    unsigned poll_timer_us;  /* 60 hz timer (machine time) */
    unsigned mode_flags;     /* ADB modes */
    bool is_keypad_down;     /* Used to determine keypad modifier status */
    bool is_asciikey_down;   /* Used to determine c010 anykey down status */
    bool has_modkey_changed; /* FIXME: Used for modifier key latch? */

    uint8_t io_key_last_ascii; /* The last ascii key pressed, bit 7 strobe */

    /* Host-GLU registers */
    uint16_t keyb_reg[4];  /**< mocked GLU keyboard registers */
    uint16_t mouse_reg[4]; /**< mocked GLU mouse registers */

    uint8_t cmd_reg;        /**< command type */
    uint8_t cmd_flags;      /**< meant to reflect C026 when not data */
    uint8_t cmd_status;     /**< meant to approximately reflect C027 */
    uint8_t cmd_data_limit; /**< expected cnt of bytes for send/recv */
    uint8_t cmd_data_sent;  /**< current index into cmd_data sent 2-way */
    uint8_t cmd_data_recv;  /**< current index into cmd_data recv 2-way */
    uint8_t cmd_data[16];   /**< command data */

    struct ClemensDeviceKeyboard keyb;
    struct ClemensDeviceMouse mouse;
    struct ClemensDeviceGameport gameport;

    uint8_t ram[256]; /**< Microcontroller RAM */

    uint32_t irq_dispatch; /* IRQ should be dispatched next sync */
    uint32_t irq_line;     /**< IRQ flags passed to machine */
};

struct ClemensDeviceSCC {
    clem_clocks_time_t ts_last_frame;

    /** Internal state that drives how the cmd/data registers are interpreted */
    unsigned state;
    unsigned selected_reg[2];

    uint8_t serial[2]; /**< See CLEM_SCC_PORT_xxx */

    uint32_t irq_line; /**< IRQ flags passed to machine */
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
    uint8_t *data;
    unsigned stride;
    unsigned frame_count;
    unsigned frames_per_second; /**< target audio frequency */
};

struct ClemensDeviceEnsoniq {
    /** Clocks budget for oscillator sync */
    clem_clocks_duration_t dt_budget;
    /** cycle counter with 1 cycle per oscillator */
    unsigned cycle;
    /** PCM output (floating point per channel) */
    float voice[16];

    uint8_t sound_ram[65536];
    uint8_t reg[256];      /**< DOC register values */
    unsigned acc[32];      /**< Oscillator running accumulator */
    uint16_t ptr[32];      /**< Stored pointer from last cycle */
    uint8_t osc_flags[32]; /**< IRQ flagged */

    unsigned address;       /**< 16-bit address into RAM or registers */
    unsigned ram_read_cntr; /**< RAM read counter, reset on address change */

    bool addr_auto_inc; /**< Address auto incremented on access */
    bool is_access_ram; /**< If true, sound RAM, if false, registers */
    bool is_busy;       /**< DOC busy */
};

struct ClemensDeviceAudio {
    struct ClemensDeviceEnsoniq doc;

    /* settings */
    uint8_t volume;        /**< 0 - 15 */
    bool a2_speaker;       /**< the c030 switch */
    bool a2_speaker_tense; /**< the a2 speaker state tense, relax */
    int32_t a2_speaker_frame_count;
    int32_t a2_speaker_frame_threshold;
    float a2_speaker_level;

    /* host supplied mix buffer */
    struct ClemensAudioMixBuffer mix_buffer;
    clem_clocks_time_t ts_last_frame;
    clem_clocks_duration_t dt_mix_frame;
    clem_clocks_duration_t dt_mix_sample;
    unsigned mix_frame_index;

    /* test code */
    float tone_frame_delta;
    float tone_theta;
    unsigned tone_frequency;

    /* the device's IRQ line */
    uint32_t irq_line;

#if CLEM_AUDIO_DIAGNOSTICS
    unsigned diag_dt_ns;
    unsigned diag_delta_frames;
    clem_clocks_duration_t diag_dt;
#endif
};

/** Really, this is part of the RTC/VGC, but for separation of concerns,
 *  pulling out into its own component
 */
struct ClemensDeviceTimer {
    uint32_t irq_1sec_us;   /**< used to trigger IRQ one sec */
    uint32_t irq_qtrsec_us; /**< used to trigger IRQ quarter sec */
    uint32_t flags;         /**< interrupt  */
    uint32_t irq_line;      /**< IRQ flags passed to machine */
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
    kClemensInputType_MouseButtonUp,
    kClemensInputType_MouseMove,
    kClemensInputType_Paddle,
    kClemensInputType_PaddleDisconnected
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
    /* Value based on the input type (ADB keycode, mouse or gamepad button.)
       Mouse pointer deltas as reported by host scaled for the ADB.  These are
       in the range of +- 64 and are packed into the value (upper + lower 16 bits
       for Y and X respectively.)

       Gameport values for each paddle are stored as X = value_a, Y = value_b
       Forward two joystick input by toggling the gameport_button_mask with
       CLEM_GAMEPORT_BUTTON_MASK_JOYSTICK_0 or CLEM_GAMEPORT_BUTTON_MASK_JOYSTICK_1
    */
    int16_t value_a;
    int16_t value_b;
    /* Key toggle mask */
    union {
        unsigned adb_key_toggle_mask;
        unsigned gameport_button_mask;
    };
};

/**
 * @brief Each scanline contains offsets into different bank memory regions
 *
 */
struct ClemensScanline {
    unsigned offset;
    unsigned control; /**< Used for IIgs scanline control */
};

struct ClemensVGC {
    struct ClemensScanline text_1_scanlines[CLEM_VGC_TEXT_SCANLINE_COUNT];
    struct ClemensScanline text_2_scanlines[CLEM_VGC_TEXT_SCANLINE_COUNT];
    struct ClemensScanline hgr_1_scanlines[CLEM_VGC_HGR_SCANLINE_COUNT];
    struct ClemensScanline hgr_2_scanlines[CLEM_VGC_HGR_SCANLINE_COUNT];
    struct ClemensScanline shgr_scanlines[CLEM_VGC_SHGR_SCANLINE_COUNT];
    /* bgr- (4:4:4) bits 0 - 11 */
    uint16_t shgr_palettes[16 * CLEM_VGC_SHGR_SCANLINE_COUNT];

    /* Used for precise-ish timing of vertical blank and scanline irqs */
    clem_clocks_time_t ts_last_frame;
    clem_clocks_duration_t dt_scanline;
    unsigned vbl_counter;
    unsigned v_counter;

    /* amalgom of possible display modes */
    unsigned mode_flags;
    unsigned text_fg_color;
    unsigned text_bg_color;
    unsigned text_language;

    bool scanline_irq_enable;
    bool vbl_started; /**< Limits VBL IRQ */

    uint32_t irq_line; /**< IRQ flags passed to machine */
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
    /** Used for determining if applications are actually using the IWM for RW disk access*/
    uint32_t data_access_time_ns;

    /** Clocks delta per update, has two modes - fast and slow, 4mhz/2mhz */
    clem_clocks_duration_t state_update_clocks_dt;

    /** Drive I/O */
    unsigned io_flags;  /**< Disk port I/O flags */
    unsigned out_phase; /**< PH0-PH3 bits sent to drive */
    bool enable2;       /**< Disk II disabled (enable2 high) */

    /** Internal Registers */
    uint8_t data;          /**< IO switch data (D0-D7) */
    uint8_t latch;         /**< data latch (work register for IWM) */
    uint8_t write_out;     /**< TODO: Remove.. written byte out */
    uint8_t disk_motor_on; /**< bits 0-3 represent ports 4-7 */

    bool q6_switch;           /**< Q6 state switch */
    bool q7_switch;           /**< Q7 stage switch */
    bool timer_1sec_disabled; /**< Turn motor off immediately */
    bool async_write_mode;    /**< If True, IWM delays writes until ready */
    bool latch_mode;          /**< If True, latch value lasts for full 8 xfer */
    bool clock_8mhz;          /**< If True, 8mhz clock - never used? */

    unsigned state;         /**< The current IWM register state */
    unsigned ns_latch_hold; /**< The latch value expiration timer */
    unsigned ns_drive_hold; /**< Time until drive motor off */
    unsigned lss_state;     /**< State of our custom LSS */
    unsigned lss_write_reg; /**< Used for detecting write underruns */

    bool enable_debug; /**< If True, activates file logging */
};

/*  ClemensDrive Data
 */

struct ClemensWOZDisk;
struct Clemens2IMGDisk;

/**
 * @brief
 *
 */
struct ClemensDrive {
    struct ClemensNibbleDisk disk; /**< Disk Nibble Data */

    //  TODO: Move the below to the host - we only care about nibblized data
    //  here
    int qtr_track_index;       /**< Current track position of the head */
    unsigned track_byte_index; /**< byte index into track */
    unsigned track_bit_shift;  /**< bit offset into current byte */
    unsigned track_bit_length; /**< current track bit length */
    unsigned pulse_ns;         /**< nanosecond timer for pulse input */
    unsigned read_buffer;      /**< Used for MC3470 emulation */

    /**
     * 4-bit Q0-3 entry 5.25" = stepper control
     * Control/Status/Strobe bits for 3.5"
     */
    unsigned ctl_switch;
    unsigned cog_orient;       /**< emulated orientation of stepper cog */
    unsigned step_timer_35_ns; /**< 3.5" track step timer */
    uint16_t status_mask_35;   /**< 3.5" status mask */
    bool write_pulse;          /**< Changes in the write field translate as pulses */
    bool is_spindle_on;        /**< Drive spindle running */
    bool has_disk;             /**< Has a disk in the drive */

    uint8_t real_track_index; /**< the index into the raw woz track data */

    /** used for random pulse generation */
    uint8_t random_bits[CLEM_IWM_DRIVE_RANDOM_BYTES];
    unsigned random_bit_index;
};

struct ClemensDriveBay {
    struct ClemensDrive slot5[2];
    struct ClemensDrive slot6[2];
    struct ClemensSmartPortUnit smartport[CLEM_SMARTPORT_DRIVE_LIMIT];
};

/**
 * @brief Reflects the CPU state on the MMIO
 *
 */
enum ClemensMMIOStateType {
    kClemensMMIOStateType_None,
    kClemensMMIOStateType_Reset,
    kClemensMMIOStateType_Active
};

/**
 * @brief FPI + MEGA2 MMIO Interface
 *
 */
typedef struct ClemensMMIO {
    /** Handlers for all slots */
    ClemensCard *card_slot[CLEM_CARD_SLOT_COUNT];
    /** Expansion ROM area for each card.  This area is paged into addressable
     *  memory with the correct IO instructions.  Each area should be 2K in
     *  size.  As with card slot memory, slot 3 is ignored */
    uint8_t *card_slot_expansion_memory[CLEM_CARD_SLOT_COUNT];
    /* pointer to the array of bank page map pointers initialized by the
       parent machine
    */
    struct ClemensMemoryPageMap **bank_page_map;
    /* The different page mapping types */
    struct ClemensMemoryPageMap fpi_direct_page_map;
    struct ClemensMemoryPageMap fpi_main_page_map;
    struct ClemensMemoryPageMap fpi_aux_page_map;
    struct ClemensMemoryPageMap fpi_rom_page_map;
    struct ClemensMemoryPageMap mega2_main_page_map;
    struct ClemensMemoryPageMap mega2_aux_page_map;
    struct ClemensMemoryPageMap empty_page_map;

    /* Shadow maps for bank 00, 01 */
    struct ClemensMemoryShadowMap fpi_mega2_main_shadow_map;
    struct ClemensMemoryShadowMap fpi_mega2_aux_shadow_map;

    /* All devices */
    struct ClemensDeviceDebugger *dev_debug;
    struct ClemensVGC vgc;
    struct ClemensDeviceRTC dev_rtc;
    struct ClemensDeviceADB dev_adb;
    struct ClemensDeviceTimer dev_timer;
    struct ClemensDeviceAudio dev_audio;
    struct ClemensDeviceIWM dev_iwm;
    struct ClemensDeviceSCC dev_scc;
    /* Peripherals */
    struct ClemensDriveBay active_drives;

    /* Registers that do not fall easily within a device struct */
    enum ClemensMMIOStateType state_type;
    uint32_t mmap_register;     // memory map flags- CLEM_MEM_IO_MMAP_
    uint32_t last_data_address; // used for c08x switches
    uint32_t emulator_detect;   // used for the c04f emulator test (state)
    uint8_t new_video_c029;     // see kClemensMMIONewVideo_xxx
    uint8_t speed_c036;         // see kClemensMMIOSpeed_xxx
    uint8_t fpi_ram_bank_count; // the number of RAM banks available to the memory mapper

    clem_clocks_duration_t clocks_step_mega2;
    uint64_t mega2_cycles;            // number of mega2 pulses/ticks since startup
    uint32_t timer_60hz_us;           // used for executing logic per 1/60th second
    int32_t card_expansion_rom_index; // card slot has the mutex on C800-CFFF

    /* All ticks are mega2 cycles */
    uint32_t irq_line; // see CLEM_IRQ_XXX flags, if !=0 triggers irqb
    uint32_t nmi_line; // see ClEM_NMI_XXX flags
} ClemensMMIO;

/**
 * @brief
 *
 */
enum ClemensVideoFormat {
    kClemensVideoFormat_None,
    kClemensVideoFormat_Text,
    kClemensVideoFormat_Lores,
    kClemensVideoFormat_Hires,
    kClemensVideoFormat_Double_Lores,
    kClemensVideoFormat_Double_Hires,
    kClemensVideoFormat_Super_Hires
};

/**
 * @brief
 *
 */
typedef struct {
    struct ClemensScanline *scanlines;
    int scanline_byte_cnt;
    int scanline_start;
    int scanline_count;
    int scanline_limit;
    enum ClemensVideoFormat format;
    unsigned vbl_counter;
    /** Pointer to 200 scanlines of 16 colors (4:4:4) each = 3200 * 2 bytes.  RGB word
        where bits 0-3 are blue, 4-7 are green and 8-11 are red.  This pointer is
        owned by the internal VGC data structure and remains valid until the next call to
        clemens_emulate_cpu(). */
    uint16_t *rgb;
    unsigned rgb_buffer_size;
} ClemensVideo;

typedef struct {
    unsigned int signal; /**< See CLEM_MONITOR_xxx */
    unsigned int color;  /**< See CLEM_MONITOR_xxx */
    unsigned width;
    unsigned height;
    unsigned border_color; /**< See CLEM_VGC_COLOR_xxx */
    unsigned text_color;   /**< bits 0-3 = foreground, 4-7 = background */
} ClemensMonitor;

typedef struct {
    uint8_t *data;         /** format will always by 32-bit float stereo */
    unsigned frame_total;  /** total number of frames in the buffer */
    unsigned frame_start;  /** --frame-- index into the data buffer */
    unsigned frame_count;  /** --frame-- count (this can wrap around) */
    unsigned frame_stride; /** each frame is this size */
} ClemensAudio;

#ifdef __cplusplus
}
#endif

#endif
