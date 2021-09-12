#include "serializer.h"
#include "clem_woz.h"
#include "clem_mem.h"

/* Serializing the Machine */


union ClemensSerializerVariant {
    uint8_t* blob;
    uint64_t u64;
    clem_clocks_time_t clocks;
    clem_clocks_duration_t duration;
    float f32;
    uint32_t u32;
    int32_t i32;
    uint16_t u16;
    uint8_t u8;
    bool b;
};

#define CLEM_SERIALIZER_CUSTOM_RECORD_AUDIO_MIX_BUFFER      0x00000001
#define CLEM_SERIALIZER_CUSTOM_RECORD_WOZ_DISK              0x00000002


#define CLEM_SERIALIZER_RECORD_COUNT(_records_) \
    (sizeof(_records_) / sizeof(struct ClemensSerializerRecord))

#define CLEM_SERIALIZER_RECORD(_struct_, _type_, _name_) \
    { #_name_, _type_, kClemensSerializerTypeEmpty, offsetof(_struct_, _name_), 0, 0, NULL}

#define CLEM_SERIALIZER_RECORD_PARAM(_struct_, _type_, _arr_type_, _name_, _size_, _param_, _records_) \
    { #_name_, _type_, _arr_type_, offsetof(_struct_, _name_), _size_, _param_, _records_}

#define CLEM_SERIALIZER_RECORD_ARRAY(_struct_, _type_, _name_, _size_, _param_) \
    CLEM_SERIALIZER_RECORD_PARAM(_struct_, kClemensSerializerTypeArray, _type_, _name_, _size_, _param_, NULL)

#define CLEM_SERIALIZER_RECORD_ARRAY_OBJECTS(_struct_, _name_, _size_, _object_type_, _records_) \
    CLEM_SERIALIZER_RECORD_PARAM(_struct_, kClemensSerializerTypeArray, kClemensSerializerTypeObject, _name_, _size_, sizeof(_object_type_), _records_)

#define CLEM_SERIALIZER_RECORD_BOOL(_struct_, _name_) \
    CLEM_SERIALIZER_RECORD(_struct_, kClemensSerializerTypeBool, _name_)

#define CLEM_SERIALIZER_RECORD_UINT8(_struct_, _name_) \
    CLEM_SERIALIZER_RECORD(_struct_, kClemensSerializerTypeUInt8, _name_)

#define CLEM_SERIALIZER_RECORD_UINT16(_struct_, _name_) \
    CLEM_SERIALIZER_RECORD(_struct_, kClemensSerializerTypeUInt16, _name_)

#define CLEM_SERIALIZER_RECORD_UINT32(_struct_, _name_) \
    CLEM_SERIALIZER_RECORD(_struct_, kClemensSerializerTypeUInt32, _name_)

#define CLEM_SERIALIZER_RECORD_UINT64(_struct_, _name_) \
    CLEM_SERIALIZER_RECORD(_struct_, kClemensSerializerTypeUInt64, _name_)

#define CLEM_SERIALIZER_RECORD_INT32(_struct_, _name_) \
    CLEM_SERIALIZER_RECORD(_struct_, kClemensSerializerTypeInt32, _name_)

#define CLEM_SERIALIZER_RECORD_FLOAT(_struct_, _name_) \
    CLEM_SERIALIZER_RECORD(_struct_, kClemensSerializerTypeFloat, _name_)

#define CLEM_SERIALIZER_RECORD_DURATION(_struct_, _name_) \
    CLEM_SERIALIZER_RECORD(_struct_, kClemensSerializerTypeDuration, _name_)

#define CLEM_SERIALIZER_RECORD_CLOCKS(_struct_, _name_) \
    CLEM_SERIALIZER_RECORD(_struct_, kClemensSerializerTypeClocks, _name_)

#define CLEM_SERIALIZER_RECORD_BLOB(_struct_, _name_, _size_) \
    CLEM_SERIALIZER_RECORD_PARAM( \
        _struct_, kClemensSerializerTypeBlob, kClemensSerializerTypeEmpty, _name_, _size_, 0, NULL)

#define CLEM_SERIALIZER_RECORD_OBJECT( \
    _struct_, _name_, _object_type_, _records_) \
    CLEM_SERIALIZER_RECORD_PARAM( \
        _struct_, kClemensSerializerTypeObject, kClemensSerializerTypeEmpty, _name_, sizeof(_object_type_), 0, _records_)

#define CLEM_SERIALIZER_RECORD_CUSTOM(_struct_, _name_, _object_type_, _record_id_) \
    CLEM_SERIALIZER_RECORD_PARAM(_struct_, kClemensSerializerTypeCustom, kClemensSerializerTypeEmpty, \
        _name_, sizeof(_object_type_), _record_id_, NULL)

#define CLEM_SERIALIZER_RECORD_EMPTY() \
    { NULL, kClemensSerializerTypeEmpty, kClemensSerializerTypeEmpty, 0, 0, 0, NULL}

struct ClemensSerializerRecord kCPUPins[] = {
    CLEM_SERIALIZER_RECORD_UINT16(struct ClemensCPUPins, adr),
    CLEM_SERIALIZER_RECORD_UINT8(struct ClemensCPUPins, bank),
    CLEM_SERIALIZER_RECORD_UINT8(struct ClemensCPUPins, data),
    CLEM_SERIALIZER_RECORD_BOOL(struct ClemensCPUPins, abortIn),
    CLEM_SERIALIZER_RECORD_BOOL(struct ClemensCPUPins, busEnableIn),
    CLEM_SERIALIZER_RECORD_BOOL(struct ClemensCPUPins, irqbIn),
    CLEM_SERIALIZER_RECORD_BOOL(struct ClemensCPUPins, nmiIn),
    CLEM_SERIALIZER_RECORD_BOOL(struct ClemensCPUPins, readyOut),
    CLEM_SERIALIZER_RECORD_BOOL(struct ClemensCPUPins, resbIn),
    CLEM_SERIALIZER_RECORD_BOOL(struct ClemensCPUPins, emulation),
    CLEM_SERIALIZER_RECORD_BOOL(struct ClemensCPUPins, vdaOut),
    CLEM_SERIALIZER_RECORD_BOOL(struct ClemensCPUPins, vpaOut),
    CLEM_SERIALIZER_RECORD_BOOL(struct ClemensCPUPins, rwbOut),
    CLEM_SERIALIZER_RECORD_EMPTY()
};

struct ClemensSerializerRecord kCPURegs[] = {
    CLEM_SERIALIZER_RECORD_UINT16(struct ClemensCPURegs, A),
    CLEM_SERIALIZER_RECORD_UINT16(struct ClemensCPURegs, X),
    CLEM_SERIALIZER_RECORD_UINT16(struct ClemensCPURegs, Y),
    CLEM_SERIALIZER_RECORD_UINT16(struct ClemensCPURegs, D),
    CLEM_SERIALIZER_RECORD_UINT16(struct ClemensCPURegs, S),
    CLEM_SERIALIZER_RECORD_UINT16(struct ClemensCPURegs, PC),
    CLEM_SERIALIZER_RECORD_UINT8(struct ClemensCPURegs, IR),
    CLEM_SERIALIZER_RECORD_UINT8(struct ClemensCPURegs, P),
    CLEM_SERIALIZER_RECORD_UINT8(struct ClemensCPURegs, DBR),
    CLEM_SERIALIZER_RECORD_UINT8(struct ClemensCPURegs, PBR),
    CLEM_SERIALIZER_RECORD_EMPTY()
};

struct ClemensSerializerRecord kCPU[] = {
    CLEM_SERIALIZER_RECORD_OBJECT(
        struct Clemens65C816, pins, struct ClemensCPUPins, kCPUPins),
    CLEM_SERIALIZER_RECORD_OBJECT(
        struct Clemens65C816, regs, struct ClemensCPURegs, kCPURegs),
    CLEM_SERIALIZER_RECORD_INT32(struct Clemens65C816, state_type),
    CLEM_SERIALIZER_RECORD_UINT32(struct Clemens65C816, cycles_spent),
    CLEM_SERIALIZER_RECORD_BOOL(struct Clemens65C816, enabled),
    CLEM_SERIALIZER_RECORD_EMPTY()
};

struct ClemensSerializerRecord kVGC[] = {
    /* scan lines are generated */
    CLEM_SERIALIZER_RECORD_CLOCKS(struct ClemensVGC, ts_last_frame),
    CLEM_SERIALIZER_RECORD_CLOCKS(struct ClemensVGC, ts_scanline_0),
    CLEM_SERIALIZER_RECORD_DURATION(struct ClemensVGC, dt_scanline),
    CLEM_SERIALIZER_RECORD_UINT32(struct ClemensVGC, mode_flags),
    CLEM_SERIALIZER_RECORD_UINT32(struct ClemensVGC, text_fg_color),
    CLEM_SERIALIZER_RECORD_UINT32(struct ClemensVGC, text_bg_color),
    CLEM_SERIALIZER_RECORD_UINT32(struct ClemensVGC, text_language),
    CLEM_SERIALIZER_RECORD_UINT32(struct ClemensVGC, irq_line),
    CLEM_SERIALIZER_RECORD_EMPTY()
};

struct ClemensSerializerRecord kRTC[] = {
    CLEM_SERIALIZER_RECORD_CLOCKS(struct ClemensDeviceRTC, xfer_started_time),
    CLEM_SERIALIZER_RECORD_DURATION(struct ClemensDeviceRTC, xfer_latency_duration),
    CLEM_SERIALIZER_RECORD_UINT32(struct ClemensDeviceRTC, state),
    CLEM_SERIALIZER_RECORD_UINT32(struct ClemensDeviceRTC, index),
    CLEM_SERIALIZER_RECORD_UINT32(struct ClemensDeviceRTC, flags),
    CLEM_SERIALIZER_RECORD_UINT32(struct ClemensDeviceRTC, seconds_since_1904),
    CLEM_SERIALIZER_RECORD_ARRAY(
        struct ClemensDeviceRTC, kClemensSerializerTypeUInt8, bram, 256, 0),
    CLEM_SERIALIZER_RECORD_UINT8(struct ClemensDeviceRTC, data_c033),
    CLEM_SERIALIZER_RECORD_UINT8(struct ClemensDeviceRTC, ctl_c034),
    CLEM_SERIALIZER_RECORD_EMPTY()
};

struct ClemensSerializerRecord kADBKeyboard[] = {
    CLEM_SERIALIZER_RECORD_ARRAY(
        struct ClemensDeviceKeyboard, kClemensSerializerTypeUInt8, keys, CLEM_ADB_KEYB_BUFFER_LIMIT, 0),
    CLEM_SERIALIZER_RECORD_ARRAY(
        struct ClemensDeviceKeyboard, kClemensSerializerTypeUInt8, states, CLEM_ADB_KEY_CODE_LIMIT, 0),
    CLEM_SERIALIZER_RECORD_INT32(struct ClemensDeviceKeyboard, size),
    CLEM_SERIALIZER_RECORD_INT32(struct ClemensDeviceKeyboard, delay_ms),
    CLEM_SERIALIZER_RECORD_INT32(struct ClemensDeviceKeyboard, rate_per_sec),
    CLEM_SERIALIZER_RECORD_INT32(struct ClemensDeviceKeyboard, timer_us),
    CLEM_SERIALIZER_RECORD_INT32(struct ClemensDeviceKeyboard, repeat_count),
    CLEM_SERIALIZER_RECORD_UINT8(struct ClemensDeviceKeyboard, last_a2_key_down),
    CLEM_SERIALIZER_RECORD_BOOL(struct ClemensDeviceKeyboard, reset_key),
    CLEM_SERIALIZER_RECORD_EMPTY()
};

struct ClemensSerializerRecord kADBMouse[] = {
    CLEM_SERIALIZER_RECORD_INT32(struct ClemensDeviceMouse, x),
    CLEM_SERIALIZER_RECORD_INT32(struct ClemensDeviceMouse, y),
    CLEM_SERIALIZER_RECORD_EMPTY()
};

struct ClemensSerializerRecord kGameport[] = {
    CLEM_SERIALIZER_RECORD_ARRAY(
        struct ClemensDeviceGameport, kClemensSerializerTypeUInt8, paddle, 4, 0),
    CLEM_SERIALIZER_RECORD_UINT8(struct ClemensDeviceGameport, btn_mask),
    CLEM_SERIALIZER_RECORD_UINT8(struct ClemensDeviceGameport, ann_mask),
    CLEM_SERIALIZER_RECORD_EMPTY()
};

struct ClemensSerializerRecord kTimer[] = {
    CLEM_SERIALIZER_RECORD_UINT32(struct ClemensDeviceTimer, irq_1sec_us),
    CLEM_SERIALIZER_RECORD_UINT32(struct ClemensDeviceTimer, irq_qtrsec_us),
    CLEM_SERIALIZER_RECORD_UINT32(struct ClemensDeviceTimer, flags),
    CLEM_SERIALIZER_RECORD_UINT32(struct ClemensDeviceTimer, irq_line),
    CLEM_SERIALIZER_RECORD_EMPTY()
};

struct ClemensSerializerRecord kJSRContext[] = {
    CLEM_SERIALIZER_RECORD_UINT32(struct ClemensDebugJSRContext, adr),
    CLEM_SERIALIZER_RECORD_UINT32(struct ClemensDebugJSRContext, jmp),
    CLEM_SERIALIZER_RECORD_UINT16(struct ClemensDebugJSRContext, sp),
    CLEM_SERIALIZER_RECORD_EMPTY()
};

struct ClemensSerializerRecord kDebugger[] = {
    CLEM_SERIALIZER_RECORD_ARRAY(
        struct ClemensDeviceDebugger, kClemensSerializerTypeUInt32, ioreg_read_ctr, 256, 0),
    CLEM_SERIALIZER_RECORD_ARRAY(
        struct ClemensDeviceDebugger, kClemensSerializerTypeUInt32, ioreg_write_ctr, 256, 0),
    CLEM_SERIALIZER_RECORD_ARRAY_OBJECTS(
        struct ClemensDeviceDebugger, jsr_contexts, CLEM_DEBUG_JSR_CONTEXT_LIMIT, struct ClemensDebugJSRContext, kJSRContext),
    CLEM_SERIALIZER_RECORD_UINT32(struct ClemensDeviceDebugger, jsr_context_count),
    CLEM_SERIALIZER_RECORD_EMPTY()
};

struct ClemensSerializerRecord kAudio[] = {
    CLEM_SERIALIZER_RECORD_ARRAY(struct ClemensDeviceAudio, kClemensSerializerTypeUInt8, sound_ram, 65536, 0),
    CLEM_SERIALIZER_RECORD_UINT32(struct ClemensDeviceAudio, address),
    CLEM_SERIALIZER_RECORD_UINT32(struct ClemensDeviceAudio, ram_read_cntr),
    CLEM_SERIALIZER_RECORD_ARRAY(struct ClemensDeviceAudio, kClemensSerializerTypeUInt8, doc_reg, 256, 0),
    CLEM_SERIALIZER_RECORD_BOOL(struct ClemensDeviceAudio, addr_auto_inc),
    CLEM_SERIALIZER_RECORD_BOOL(struct ClemensDeviceAudio, is_access_ram),
    CLEM_SERIALIZER_RECORD_BOOL(struct ClemensDeviceAudio, is_busy),
    CLEM_SERIALIZER_RECORD_UINT8(struct ClemensDeviceAudio, volume),
    CLEM_SERIALIZER_RECORD_BOOL(struct ClemensDeviceAudio, a2_speaker),
    CLEM_SERIALIZER_RECORD_BOOL(struct ClemensDeviceAudio, a2_speaker_tense),
    CLEM_SERIALIZER_RECORD_CUSTOM(struct ClemensDeviceAudio, mix_buffer,
        struct ClemensAudioMixBuffer, CLEM_SERIALIZER_CUSTOM_RECORD_AUDIO_MIX_BUFFER),
    CLEM_SERIALIZER_RECORD_CLOCKS(struct ClemensDeviceAudio, ts_last_frame),
    CLEM_SERIALIZER_RECORD_DURATION(struct ClemensDeviceAudio, dt_mix_frame),
    CLEM_SERIALIZER_RECORD_DURATION(struct ClemensDeviceAudio, dt_mix_sample),
    CLEM_SERIALIZER_RECORD_UINT32(struct ClemensDeviceAudio, mix_frame_index),
    CLEM_SERIALIZER_RECORD_FLOAT(struct ClemensDeviceAudio, tone_frame_delta),
    CLEM_SERIALIZER_RECORD_FLOAT(struct ClemensDeviceAudio, tone_theta),
    CLEM_SERIALIZER_RECORD_UINT32(struct ClemensDeviceAudio, tone_frequency),
    CLEM_SERIALIZER_RECORD_UINT32(struct ClemensDeviceAudio, irq_line),
    CLEM_SERIALIZER_RECORD_EMPTY()
};

struct ClemensSerializerRecord kSCC[] = {
    CLEM_SERIALIZER_RECORD_CLOCKS(struct ClemensDeviceSCC, ts_last_frame),
    CLEM_SERIALIZER_RECORD_ARRAY(struct ClemensDeviceSCC, kClemensSerializerTypeUInt32, selected_reg, 2, 0),
    CLEM_SERIALIZER_RECORD_ARRAY(struct ClemensDeviceSCC, kClemensSerializerTypeUInt8, serial, 2, 0),
    CLEM_SERIALIZER_RECORD_UINT32(struct ClemensDeviceSCC, irq_line),
    CLEM_SERIALIZER_RECORD_EMPTY()
};

struct ClemensSerializerRecord kIWM[] = {
    CLEM_SERIALIZER_RECORD_CLOCKS(struct ClemensDeviceIWM, last_clocks_ts),
    CLEM_SERIALIZER_RECORD_CLOCKS(struct ClemensDeviceIWM, last_write_clocks_ts),
    CLEM_SERIALIZER_RECORD_DURATION(struct ClemensDeviceIWM, lss_clocks_lag),
    CLEM_SERIALIZER_RECORD_UINT32(struct ClemensDeviceIWM, io_flags),
    CLEM_SERIALIZER_RECORD_UINT32(struct ClemensDeviceIWM, out_phase),
    CLEM_SERIALIZER_RECORD_BOOL(struct ClemensDeviceIWM, enbl2),
    CLEM_SERIALIZER_RECORD_UINT8(struct ClemensDeviceIWM, data),
    CLEM_SERIALIZER_RECORD_UINT8(struct ClemensDeviceIWM, latch),
    CLEM_SERIALIZER_RECORD_UINT8(struct ClemensDeviceIWM, write_out),
    CLEM_SERIALIZER_RECORD_UINT8(struct ClemensDeviceIWM, disk_motor_on),
    CLEM_SERIALIZER_RECORD_BOOL(struct ClemensDeviceIWM, q6_switch),
    CLEM_SERIALIZER_RECORD_BOOL(struct ClemensDeviceIWM, q7_switch),
    CLEM_SERIALIZER_RECORD_BOOL(struct ClemensDeviceIWM, timer_1sec_disabled),
    CLEM_SERIALIZER_RECORD_BOOL(struct ClemensDeviceIWM, async_write_mode),
    CLEM_SERIALIZER_RECORD_BOOL(struct ClemensDeviceIWM, latch_mode),
    CLEM_SERIALIZER_RECORD_BOOL(struct ClemensDeviceIWM, clock_8mhz),
    CLEM_SERIALIZER_RECORD_UINT32(struct ClemensDeviceIWM, state),
    CLEM_SERIALIZER_RECORD_UINT32(struct ClemensDeviceIWM, ns_latch_hold),
    CLEM_SERIALIZER_RECORD_UINT32(struct ClemensDeviceIWM, ns_drive_hold),
    CLEM_SERIALIZER_RECORD_UINT32(struct ClemensDeviceIWM, lss_state),
    CLEM_SERIALIZER_RECORD_UINT32(struct ClemensDeviceIWM, lss_write_counter),
    CLEM_SERIALIZER_RECORD_UINT32(struct ClemensDeviceIWM, lss_update_dt_ns),
    /* skip enable_debug and all debug options */
    CLEM_SERIALIZER_RECORD_EMPTY()
};


struct ClemensSerializerRecord kADB[] = {
    CLEM_SERIALIZER_RECORD_UINT32(struct ClemensDeviceADB, state),
    CLEM_SERIALIZER_RECORD_UINT32(struct ClemensDeviceADB, version),
    CLEM_SERIALIZER_RECORD_UINT32(struct ClemensDeviceADB, poll_timer_us),
    CLEM_SERIALIZER_RECORD_UINT32(struct ClemensDeviceADB, mode_flags),
    CLEM_SERIALIZER_RECORD_BOOL(struct ClemensDeviceADB, is_keypad_down),
    CLEM_SERIALIZER_RECORD_BOOL(struct ClemensDeviceADB, is_asciikey_down),
    CLEM_SERIALIZER_RECORD_BOOL(struct ClemensDeviceADB, has_modkey_changed),
    CLEM_SERIALIZER_RECORD_UINT8(struct ClemensDeviceADB, io_key_last_ascii),
    CLEM_SERIALIZER_RECORD_ARRAY(
        struct ClemensDeviceADB, kClemensSerializerTypeUInt16, keyb_reg, 4, 0),
    CLEM_SERIALIZER_RECORD_ARRAY(
        struct ClemensDeviceADB, kClemensSerializerTypeUInt16, mouse_reg, 4, 0),
    CLEM_SERIALIZER_RECORD_UINT8(struct ClemensDeviceADB, cmd_reg),
    CLEM_SERIALIZER_RECORD_UINT8(struct ClemensDeviceADB, cmd_flags),
    CLEM_SERIALIZER_RECORD_UINT8(struct ClemensDeviceADB, cmd_status),
    CLEM_SERIALIZER_RECORD_UINT8(struct ClemensDeviceADB, cmd_data_limit),
    CLEM_SERIALIZER_RECORD_UINT8(struct ClemensDeviceADB, cmd_data_sent),
    CLEM_SERIALIZER_RECORD_UINT8(struct ClemensDeviceADB, cmd_data_recv),
    CLEM_SERIALIZER_RECORD_ARRAY(
        struct ClemensDeviceADB, kClemensSerializerTypeUInt8, cmd_data, 16, 0),
    CLEM_SERIALIZER_RECORD_OBJECT(struct ClemensDeviceADB, keyb, struct ClemensDeviceKeyboard, kADBKeyboard),
    CLEM_SERIALIZER_RECORD_OBJECT(struct ClemensDeviceADB, mouse, struct ClemensDeviceMouse, kADBMouse),
    CLEM_SERIALIZER_RECORD_OBJECT(struct ClemensDeviceADB, gameport, struct ClemensDeviceGameport, kGameport),
    CLEM_SERIALIZER_RECORD_ARRAY(
        struct ClemensDeviceADB, kClemensSerializerTypeUInt8, ram, 256, 0),
    CLEM_SERIALIZER_RECORD_UINT32(struct ClemensDeviceADB, irq_line),
    CLEM_SERIALIZER_RECORD_EMPTY()
};

struct ClemensSerializerRecord kMMIO[] = {
    /* all page maps are generated from mmap_register and the shadow register */
    CLEM_SERIALIZER_RECORD_OBJECT(struct ClemensMMIO, vgc, struct ClemensVGC, kVGC),
    CLEM_SERIALIZER_RECORD_OBJECT(struct ClemensMMIO, dev_rtc, struct ClemensDeviceRTC, kRTC),
    CLEM_SERIALIZER_RECORD_OBJECT(struct ClemensMMIO, dev_adb, struct ClemensDeviceADB, kADB),
    CLEM_SERIALIZER_RECORD_OBJECT(struct ClemensMMIO, dev_timer, struct ClemensDeviceTimer, kTimer),
    CLEM_SERIALIZER_RECORD_OBJECT(struct ClemensMMIO, dev_debug, struct ClemensDeviceDebugger, kDebugger),
    CLEM_SERIALIZER_RECORD_OBJECT(struct ClemensMMIO, dev_audio, struct ClemensDeviceAudio, kAudio),
    CLEM_SERIALIZER_RECORD_OBJECT(struct ClemensMMIO, dev_iwm, struct ClemensDeviceIWM, kIWM),
    CLEM_SERIALIZER_RECORD_OBJECT(struct ClemensMMIO, dev_scc, struct ClemensDeviceSCC, kSCC),
    CLEM_SERIALIZER_RECORD_UINT32(struct ClemensMMIO, mmap_register),
    CLEM_SERIALIZER_RECORD_UINT32(struct ClemensMMIO, last_data_address),
    CLEM_SERIALIZER_RECORD_UINT8(struct ClemensMMIO, new_video_c029),
    CLEM_SERIALIZER_RECORD_UINT8(struct ClemensMMIO, speed_c036),
    CLEM_SERIALIZER_RECORD_UINT64(struct ClemensMMIO, mega2_cycles),
    CLEM_SERIALIZER_RECORD_UINT32(struct ClemensMMIO, timer_60hz_us),
    CLEM_SERIALIZER_RECORD_INT32(struct ClemensMMIO, card_expansion_rom_index),
    CLEM_SERIALIZER_RECORD_UINT32(struct ClemensMMIO, irq_line),

    CLEM_SERIALIZER_RECORD_EMPTY()
};

struct ClemensSerializerRecord kDrive[] = {
    CLEM_SERIALIZER_RECORD_CUSTOM(
        struct ClemensDrive, data, struct ClemensWOZDisk,
        CLEM_SERIALIZER_CUSTOM_RECORD_WOZ_DISK),
    CLEM_SERIALIZER_RECORD_INT32(struct ClemensDrive, qtr_track_index),
    CLEM_SERIALIZER_RECORD_UINT32(struct ClemensDrive, track_byte_index),
    CLEM_SERIALIZER_RECORD_UINT32(struct ClemensDrive, track_bit_shift),
    CLEM_SERIALIZER_RECORD_UINT32(struct ClemensDrive, track_bit_length),
    CLEM_SERIALIZER_RECORD_UINT32(struct ClemensDrive, pulse_ns),
    CLEM_SERIALIZER_RECORD_UINT32(struct ClemensDrive, read_buffer),
    CLEM_SERIALIZER_RECORD_UINT32(struct ClemensDrive, q03_switch),
    CLEM_SERIALIZER_RECORD_UINT32(struct ClemensDrive, cog_orient),
    CLEM_SERIALIZER_RECORD_UINT32(struct ClemensDrive, state_35),
    CLEM_SERIALIZER_RECORD_UINT32(struct ClemensDrive, query_35),
    CLEM_SERIALIZER_RECORD_UINT32(struct ClemensDrive, step_timer_35_ns),
    CLEM_SERIALIZER_RECORD_BOOL(struct ClemensDrive, select_35),
    CLEM_SERIALIZER_RECORD_BOOL(struct ClemensDrive, write_pulse),
    CLEM_SERIALIZER_RECORD_BOOL(struct ClemensDrive, real_track_index),
    CLEM_SERIALIZER_RECORD_ARRAY(
        struct ClemensDrive, kClemensSerializerTypeUInt8, random_bits,
        CLEM_IWM_DRIVE_RANDOM_BYTES, 0),
    CLEM_SERIALIZER_RECORD_UINT32(struct ClemensDrive, random_bit_index),
    CLEM_SERIALIZER_RECORD_EMPTY()
};


struct ClemensSerializerRecord kDriveBay[] = {
    CLEM_SERIALIZER_RECORD_ARRAY_OBJECTS(
        struct ClemensDriveBay, slot5, 2, struct ClemensDrive, kDrive),
    CLEM_SERIALIZER_RECORD_ARRAY_OBJECTS(
        struct ClemensDriveBay, slot6, 2, struct ClemensDrive, kDrive),
    CLEM_SERIALIZER_RECORD_EMPTY()
};

struct ClemensSerializerRecord kMachine[] = {
    CLEM_SERIALIZER_RECORD_OBJECT(ClemensMachine, cpu, struct Clemens65C816, kCPU),
    CLEM_SERIALIZER_RECORD_DURATION(ClemensMachine, clocks_step),
    CLEM_SERIALIZER_RECORD_DURATION(ClemensMachine, clocks_step_fast),
    CLEM_SERIALIZER_RECORD_DURATION(ClemensMachine, clocks_step_mega2),
    CLEM_SERIALIZER_RECORD_CLOCKS(ClemensMachine, clocks_spent),
    CLEM_SERIALIZER_RECORD_INT32(ClemensMachine, resb_counter),
    CLEM_SERIALIZER_RECORD_BOOL(ClemensMachine, mmio_bypass),
    CLEM_SERIALIZER_RECORD_OBJECT(ClemensMachine, mmio, struct ClemensMMIO, kMMIO),
    CLEM_SERIALIZER_RECORD_OBJECT(ClemensMachine, active_drives, struct ClemensDriveBay, kDriveBay),

    /* FPI bank map has custom serialization */
    CLEM_SERIALIZER_RECORD_ARRAY(ClemensMachine,
        kClemensSerializerTypeBlob, mega2_bank_map, 2, CLEM_IIGS_BANK_SIZE),
    CLEM_SERIALIZER_RECORD_EMPTY()
};

struct ClemensSerializerRecord kWOZDisk[] = {
    CLEM_SERIALIZER_RECORD_UINT32(struct ClemensWOZDisk, disk_type),
    CLEM_SERIALIZER_RECORD_UINT32(struct ClemensWOZDisk, boot_type),
    CLEM_SERIALIZER_RECORD_UINT32(struct ClemensWOZDisk, flags),
    CLEM_SERIALIZER_RECORD_UINT32(struct ClemensWOZDisk, required_ram_kb),
    CLEM_SERIALIZER_RECORD_UINT32(struct ClemensWOZDisk, max_track_size_bytes),
    CLEM_SERIALIZER_RECORD_UINT32(struct ClemensWOZDisk, bit_timing_ns),
    CLEM_SERIALIZER_RECORD_UINT32(struct ClemensWOZDisk, track_count),
    CLEM_SERIALIZER_RECORD_UINT32(struct ClemensWOZDisk, default_track_bit_length),
    CLEM_SERIALIZER_RECORD_ARRAY(struct ClemensWOZDisk,
        kClemensSerializerTypeUInt8, meta_track_map, CLEM_WOZ_LIMIT_QTR_TRACKS, 0),
    CLEM_SERIALIZER_RECORD_ARRAY(struct ClemensWOZDisk,
        kClemensSerializerTypeUInt32, track_byte_offset, CLEM_WOZ_LIMIT_QTR_TRACKS, 0),
    CLEM_SERIALIZER_RECORD_ARRAY(struct ClemensWOZDisk,
        kClemensSerializerTypeUInt32, track_byte_count, CLEM_WOZ_LIMIT_QTR_TRACKS, 0),
    CLEM_SERIALIZER_RECORD_ARRAY(struct ClemensWOZDisk,
        kClemensSerializerTypeUInt32, track_bits_count, CLEM_WOZ_LIMIT_QTR_TRACKS, 0),
    CLEM_SERIALIZER_RECORD_ARRAY(struct ClemensWOZDisk,
        kClemensSerializerTypeUInt8, track_initialized, CLEM_WOZ_LIMIT_QTR_TRACKS, 0),
    CLEM_SERIALIZER_RECORD_ARRAY(struct ClemensWOZDisk, kClemensSerializerTypeUInt8, creator, 32, 0),
    CLEM_SERIALIZER_RECORD_EMPTY()
};

static unsigned clemens_serialize_custom(
    mpack_writer_t* writer,
    void* ptr,
    unsigned sz,
    unsigned record_id
);


unsigned clemens_serialize_record(
    mpack_writer_t* writer,
    uintptr_t data_adr,
    const struct ClemensSerializerRecord* record
) {
    union ClemensSerializerVariant variant;
    unsigned sz = 0;
    switch (record->type) {
        case kClemensSerializerTypeBool:
            variant.b = *(bool *)(data_adr + record->offset);
            mpack_write_bool(writer, variant.b);
            sz = sizeof(bool);
            break;
        case kClemensSerializerTypeUInt8:
            variant.u8 = *(uint8_t *)(data_adr + record->offset);
            mpack_write_u8(writer, variant.b);
            sz = sizeof(uint8_t);
            break;
        case kClemensSerializerTypeUInt16:
            variant.u16 = *(uint16_t *)(data_adr + record->offset);
            mpack_write_u16(writer, variant.u16);
            sz = sizeof(uint16_t);
            break;
        case kClemensSerializerTypeUInt32:
            variant.u32 = *(uint32_t *)(data_adr + record->offset);
            mpack_write_u32(writer, variant.u32);
            sz = sizeof(uint32_t);
            break;
        case kClemensSerializerTypeInt32:
            variant.i32 = *(int32_t *)(data_adr + record->offset);
            mpack_write_i32(writer, variant.i32);
            sz = sizeof(int32_t);
            break;
        case kClemensSerializerTypeUInt64:
            variant.u64 = *(uint64_t *)(data_adr + record->offset);
            mpack_write_u64(writer, variant.u64);
            sz = sizeof(uint64_t);
            break;
        case kClemensSerializerTypeFloat:
            variant.f32 = *(float *)(data_adr + record->offset);
            mpack_write_float(writer, variant.f32);
            sz = sizeof(float);
            break;
        case kClemensSerializerTypeDuration:
            variant.duration = *(clem_clocks_duration_t *)(
                data_adr + record->offset);
            mpack_write_uint(writer, variant.duration);
            sz = sizeof(clem_clocks_duration_t);
            break;
        case kClemensSerializerTypeClocks:
            variant.clocks = *(clem_clocks_time_t *)(
                data_adr + record->offset);
            mpack_write_u64(writer, variant.clocks);
            sz = sizeof(clem_clocks_time_t);
            break;
        case kClemensSerializerTypeBlob:
            variant.blob = *(uint8_t **)(data_adr + record->offset);
            mpack_build_map(writer);
            mpack_write_cstr(writer, "ok");
            if (variant.blob) {
                mpack_write_true(writer);
                mpack_write_cstr(writer, "blob");
                mpack_write_bin(writer, (const char *)variant.blob, record->size);
            } else {
                mpack_write_false(writer);
            }
            mpack_complete_map(writer);
            sz = sizeof(uint8_t*);
            break;
        case kClemensSerializerTypeArray:
            sz = clemens_serialize_array(writer, data_adr + record->offset, record);
            break;
        case kClemensSerializerTypeObject:
            sz = clemens_serialize_object(writer, data_adr + record->offset, record);
            break;
        case kClemensSerializerTypeCustom:
            sz = clemens_serialize_custom(
                writer, (void *)(data_adr + record->offset), record->size, record->param);
            break;
    }
    return sz;
}

unsigned clemens_serialize_array(
    mpack_writer_t* writer,
    uintptr_t data_adr,
    const struct ClemensSerializerRecord* record
) {
    uintptr_t array_value_adr = data_adr;
    unsigned idx;
    struct ClemensSerializerRecord value_record;
    /* generate a record of one value of the array type where the offset is
       relative to the start of the array (0) and the offset into the owner
       (data_adr)
    */
    mpack_start_array(writer, record->size);
    memset(&value_record, 0, sizeof(value_record));
    value_record.type = record->array_type;
    value_record.records = record->records;     /* for arrays of objects */
    if (value_record.type == kClemensSerializerTypeBlob ||
        value_record.type == kClemensSerializerTypeObject
    ) {
        value_record.size = record->param;
    }
    for (idx = 0; idx < record->size; ++idx) {

        array_value_adr += clemens_serialize_record(
            writer, array_value_adr, &value_record);
    }
    mpack_finish_array(writer);
    return array_value_adr - data_adr;
}

static void clemens_serialize_records(
    mpack_writer_t* writer,
    uintptr_t data_adr,
    const struct ClemensSerializerRecord* record
 ) {
    while (record->type != kClemensSerializerTypeEmpty) {
        mpack_write_cstr(writer, record->name);
        clemens_serialize_record(writer, data_adr, record);
        ++record;
    }
}

static unsigned clemens_serialize_custom(
    mpack_writer_t* writer,
    void* ptr,
    unsigned sz,
    unsigned record_id
) {
    struct ClemensAudioMixBuffer* audio_mix_buffer;
    struct ClemensWOZDisk* woz_disk;

    unsigned blob_size;

    mpack_build_map(writer);
    switch (record_id) {
        case CLEM_SERIALIZER_CUSTOM_RECORD_AUDIO_MIX_BUFFER:
            audio_mix_buffer = (struct ClemensAudioMixBuffer *)(ptr);
            mpack_write_cstr(writer, "frame_count");
            mpack_write_u32(writer, audio_mix_buffer->frame_count);
            mpack_write_cstr(writer, "frames_per_second");
            mpack_write_u32(writer, audio_mix_buffer->frames_per_second);
            mpack_write_cstr(writer, "stride");
            mpack_write_u32(writer, audio_mix_buffer->stride);
            mpack_write_cstr(writer, "data");
            mpack_write_bin(writer, (char *)audio_mix_buffer->data,
                audio_mix_buffer->frame_count * audio_mix_buffer->stride);
            break;

        case CLEM_SERIALIZER_CUSTOM_RECORD_WOZ_DISK:
            woz_disk = *(struct ClemensWOZDisk **)ptr;
            mpack_write_cstr(writer, "valid");
            mpack_write_bool(writer, woz_disk != NULL);
            if (woz_disk) {
                clemens_serialize_records(writer, (uintptr_t)woz_disk, &kWOZDisk[0]);
                mpack_write_cstr(writer, "bits_data");
                if (woz_disk->bits_data != NULL) {
                    mpack_write_bool(writer, true);
                    blob_size = (woz_disk->bits_data_end - woz_disk->bits_data);
                    mpack_write_cstr(writer, "blob");
                    mpack_write_bin(writer, (char *)woz_disk->bits_data, blob_size);
                } else {
                    mpack_write_bool(writer, false);
                }
            }
            break;

    }
    mpack_complete_map(writer);
    return sz;
}


unsigned clemens_serialize_object(
    mpack_writer_t* writer,
    uintptr_t data_adr,
    const struct ClemensSerializerRecord* record
) {
    const struct ClemensSerializerRecord* child = record->records;
    mpack_build_map(writer);
    clemens_serialize_records(writer, data_adr, child);
    mpack_complete_map(writer);
    return record->size;
}


mpack_writer_t* clemens_serialize_machine(
    mpack_writer_t* writer,
    ClemensMachine* machine
) {
    struct ClemensSerializerRecord root;
    union ClemensSerializerVariant variant;
    void* data_adr = (void *)machine;
    unsigned idx;

    memset(&root, 0, sizeof(root));
    root.type = kClemensSerializerTypeRoot;
    root.records = &kMachine[0];
    clemens_serialize_object(writer, (uintptr_t)data_adr, &root);

    /* serialize FPI banks - this lies outside the procedural laying out of
       values to serialize via record arrays since the logic is here is very
       special cased to avoid unnecessary serialization
    */
    for (idx = 0; idx < 256; ++idx) {
        mpack_write_bool(writer, machine->fpi_bank_used[idx]);
        if (machine->fpi_bank_used[idx]) {
            mpack_write_u8(writer, (uint8_t)(idx & 0xff));
            mpack_write_bin(
                writer, (char *)machine->fpi_bank_map[idx], CLEM_IIGS_BANK_SIZE);
        }
    }

    return writer;
}

/* Unserializing the Machine */

static unsigned clemens_unserialize_custom(
    mpack_reader_t* reader,
    void* ptr,
    unsigned sz,
    unsigned record_id,
    ClemensSerializerAllocateCb alloc_cb,
    void* context
);

unsigned clemens_unserialize_record(
    mpack_reader_t* reader,
    uintptr_t data_adr,
    const struct ClemensSerializerRecord* record,
    ClemensSerializerAllocateCb alloc_cb,
    void* context
) {
    union ClemensSerializerVariant variant;
    unsigned sz = 0;
    char key[64];

    switch (record->type) {
        case kClemensSerializerTypeBool:
            variant.b = mpack_expect_bool(reader);
            *(bool *)(data_adr + record->offset) = variant.b;
            sz = sizeof(bool);
            break;
        case kClemensSerializerTypeUInt8:
            variant.u8 = mpack_expect_u8(reader);
            *(uint8_t *)(data_adr + record->offset) = variant.u8;
            sz = sizeof(uint8_t);
            break;
        case kClemensSerializerTypeUInt16:
            variant.u16 = mpack_expect_u16(reader);
            *(uint16_t *)(data_adr + record->offset) = variant.u16;
            sz = sizeof(uint16_t);
            break;
        case kClemensSerializerTypeUInt32:
            variant.u32 = mpack_expect_u32(reader);
            *(uint32_t *)(data_adr + record->offset) = variant.u32;
            sz = sizeof(uint32_t);
            break;
        case kClemensSerializerTypeInt32:
            variant.i32 = mpack_expect_i32(reader);
            *(int32_t *)(data_adr + record->offset) = variant.i32;
            sz = sizeof(int32_t);
            break;
        case kClemensSerializerTypeUInt64:
            variant.u64 = mpack_expect_u64(reader);
            *(uint64_t *)(data_adr + record->offset) = variant.u64;
            sz = sizeof(uint64_t);
            break;
        case kClemensSerializerTypeFloat:
            variant.f32 = mpack_expect_float(reader);
            *(float *)(data_adr + record->offset) = variant.f32;
            sz = sizeof(float);
            break;
        case kClemensSerializerTypeDuration:
            variant.duration = mpack_expect_uint(reader);
            *(clem_clocks_duration_t *)(data_adr + record->offset) = (
                variant.duration);
            sz = sizeof(clem_clocks_duration_t);
            break;
        case kClemensSerializerTypeClocks:
            variant.clocks = mpack_expect_u64(reader);
            *(clem_clocks_time_t *)(data_adr + record->offset) = (
                variant.clocks);
            sz = sizeof(clem_clocks_time_t);
            break;
        case kClemensSerializerTypeBlob:
            variant.blob = NULL;
            mpack_expect_map(reader);
            mpack_expect_cstr(reader, key, sizeof(key));
            if (mpack_expect_bool(reader)) {
                mpack_expect_cstr(reader, key, sizeof(key));
                sz = mpack_expect_bin(reader);
                variant.blob = *(uint8_t **)(data_adr + record->offset);
                if (!variant.blob) {
                    variant.blob = (*alloc_cb)(sz, context);
                }
                if (sz > record->size) {
                    mpack_done_bin(reader);
                    return CLEM_SERIALIZER_INVALID_RECORD;
                }
                mpack_read_bytes(reader, (char *)variant.blob, sz);
                mpack_done_bin(reader);
            }
            mpack_done_map(reader);
            *(uint8_t **)(data_adr + record->offset) = variant.blob;
            sz = sizeof(uint8_t*);
            break;
        case kClemensSerializerTypeArray:
            sz = clemens_unserialize_array(
                reader, data_adr + record->offset, record, alloc_cb, context);
            break;
        case kClemensSerializerTypeObject:
            sz = clemens_unserialize_object(
                reader, data_adr + record->offset, record, alloc_cb, context);
            break;
        case kClemensSerializerTypeCustom:
            sz = clemens_unserialize_custom(
                reader, (void *)(data_adr + record->offset), record->size,
                record->param, alloc_cb, context);
            break;
    }
    return sz;
}

unsigned clemens_unserialize_array(
    mpack_reader_t* reader,
    uintptr_t data_adr,
    const struct ClemensSerializerRecord* record,
    ClemensSerializerAllocateCb alloc_cb,
    void* context
) {
    uintptr_t array_value_adr = data_adr;
    struct ClemensSerializerRecord value_record;
    unsigned array_size = mpack_expect_array(reader);
    unsigned idx;
    if (array_size != record->size) {
        return CLEM_SERIALIZER_INVALID_RECORD;
    }
    memset(&value_record, 0, sizeof(value_record));
    value_record.type = record->array_type;
    value_record.records = record->records;
    value_record.size = record->param;     /* for arrays of objects */
    for (idx = 0; idx < array_size; ++idx) {
        array_value_adr += clemens_unserialize_record(
            reader, array_value_adr, &value_record, alloc_cb, context);
    }
    mpack_done_array(reader);
    return array_value_adr - data_adr;
}

static void clemens_unserialize_records(
    mpack_reader_t* reader,
    uintptr_t data_adr,
    const struct ClemensSerializerRecord* record,
    ClemensSerializerAllocateCb alloc_cb,
    void* context
) {
    char key[64];
    while (record->type != kClemensSerializerTypeEmpty) {
        mpack_expect_cstr(reader, key, sizeof(key));
        clemens_unserialize_record(reader, data_adr, record, alloc_cb, context);
        ++record;
    }
}

static unsigned clemens_unserialize_custom(
    mpack_reader_t* reader,
    void* ptr,
    unsigned sz,
    unsigned record_id,
    ClemensSerializerAllocateCb alloc_cb,
    void* context
) {
    struct ClemensAudioMixBuffer* audio_mix_buffer;
    struct ClemensWOZDisk* woz_disk;
    char key[64];
    unsigned v0, v1;
    mpack_expect_map(reader);
    switch (record_id) {
        case CLEM_SERIALIZER_CUSTOM_RECORD_AUDIO_MIX_BUFFER:
            audio_mix_buffer = (struct ClemensAudioMixBuffer *)(ptr);
            v0 = audio_mix_buffer->stride * audio_mix_buffer->frame_count;
            mpack_expect_cstr(reader, key, sizeof(key));
            audio_mix_buffer->frame_count = mpack_expect_u32(reader);
            mpack_expect_cstr(reader, key, sizeof(key));
            audio_mix_buffer->frames_per_second = mpack_expect_u32(reader);
            mpack_expect_cstr(reader, key, sizeof(key));
            audio_mix_buffer->stride = mpack_expect_u32(reader);
            mpack_expect_cstr(reader, key, sizeof(key));
            v1 = mpack_expect_bin(reader);
            if (v0 != v1) {
                audio_mix_buffer->data = (*alloc_cb)(v1, context);
            }
            mpack_read_bytes(reader, (char *)audio_mix_buffer->data, v1);
            mpack_done_bin(reader);
            break;
        case CLEM_SERIALIZER_CUSTOM_RECORD_WOZ_DISK:
            woz_disk = *(struct ClemensWOZDisk **)ptr;

            mpack_expect_cstr(reader, key, sizeof(key));
            if (mpack_expect_bool(reader)) {
                if (!woz_disk) {
                    //  create the disk object
                    *(struct ClemensWOZDisk **)(ptr) = (
                        (struct ClemensWOZDisk *)(*alloc_cb)(
                            sizeof(struct ClemensWOZDisk), context));
                    woz_disk = *(struct ClemensWOZDisk **)ptr;
                }
            } else {
                if (woz_disk) {
                    //  invalidate it
                    *(struct ClemensWOZDisk **)(ptr) = NULL;
                    woz_disk = NULL;
                }
            }
            if (woz_disk) {
                clemens_unserialize_records(
                    reader, (uintptr_t)woz_disk, &kWOZDisk[0], alloc_cb, context);
                mpack_expect_cstr(reader, key, sizeof(key));
                if (mpack_expect_bool(reader)) {
                    mpack_expect_cstr(reader, key, sizeof(key));
                    v0 = mpack_expect_bin(reader);
                    v1 = (woz_disk->bits_data_end - woz_disk->bits_data);
                    if (v0 > v1) {
                        woz_disk->bits_data = (uint8_t*)(*alloc_cb)(v0, context);
                        woz_disk->bits_data_end = woz_disk->bits_data + v0;
                    }
                    mpack_read_bytes(reader, (char *)woz_disk->bits_data, v0);
                    mpack_done_bin(reader);
                } else {
                    woz_disk->bits_data = NULL;
                    woz_disk->bits_data_end = NULL;
                }
            }
            break;
    }
    mpack_done_map(reader);
    return sz;
}

unsigned clemens_unserialize_object(
    mpack_reader_t* reader,
    uintptr_t data_adr,
    const struct ClemensSerializerRecord* record,
    ClemensSerializerAllocateCb alloc_cb,
    void* context
) {
    struct ClemensSerializerRecord* child = record->records;
    mpack_expect_map(reader);
    clemens_unserialize_records(reader, data_adr, child, alloc_cb, context);
    mpack_done_map(reader);
    return record->size;
}

mpack_reader_t* clemens_unserialize_machine(
    mpack_reader_t* reader,
    ClemensMachine* machine,
    ClemensSerializerAllocateCb alloc_cb,
    void* context
) {
    struct ClemensSerializerRecord root;
    union ClemensSerializerVariant variant;
    void* data_adr = (void *)machine;
    unsigned idx, sz;

    memset(&root, 0, sizeof(root));
    root.type = kClemensSerializerTypeRoot;
    root.records = &kMachine[0];

    if (clemens_unserialize_object(
            reader,
            (uintptr_t)data_adr,
            &root,
            alloc_cb, context) == CLEM_SERIALIZER_INVALID_RECORD
    ) {
        return NULL;
    }


    /* unserialize FPI banks - this lies outside the procedural laying out of
       values to serialize via record arrays since the logic is here is very
       special cased to avoid unnecessary serialization
    */
    for (idx = 0; idx < 256; ++idx) {
        machine->fpi_bank_used[idx] = mpack_expect_bool(reader);
        if (machine->fpi_bank_used[idx]) {
            if (mpack_expect_u8(reader) != (uint8_t)(idx & 0xff)) {
                return NULL;
            }
            sz = mpack_expect_bin(reader);
            if (!machine->fpi_bank_map[idx]) {
                machine->fpi_bank_map[idx] = (*alloc_cb)(sz, context);
            }
            mpack_read_bytes(reader, (char *)machine->fpi_bank_map[idx], sz);
            mpack_done_bin(reader);
        }
    }


    /* mmio restore state */
    if (!machine->mmio_bypass) {
        clem_mmio_restore(&machine->mmio);
    }

    return reader;
}