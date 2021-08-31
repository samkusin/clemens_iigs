#include "serializer.h"

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

#define CLEM_SERIALIZER_RECORD_COUNT(_records_) \
    (sizeof(_records_) / sizeof(struct ClemensSerializerRecord))

#define CLEM_SERIALIZER_RECORD(_struct_, _type_, _name_) \
    { #_name_, _type_, kClemensSerializerTypeEmpty, offsetof(_struct_, _name_), 0, 0, NULL}

#define CLEM_SERIALIZER_RECORD_PARAM(_struct_, _type_, _arr_type_, _name_, _size_, _param_, _records_) \
    { #_name_, _type_, _arr_type_, offsetof(_struct_, _name_), _size_, _param_, _records_}

#define CLEM_SERIALIZER_RECORD_ARRAY(_struct_, _type_, _name_, _size_, _param_) \
    CLEM_SERIALIZER_RECORD_PARAM(_struct_, kClemensSerializerTypeArray, _type_, _name_, _size_, _param_, NULL)

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

struct ClemensSerializerRecord kDebugger[] = {
    CLEM_SERIALIZER_RECORD_EMPTY()
};

struct ClemensSerializerRecord kAudio[] = {
    CLEM_SERIALIZER_RECORD_EMPTY()
};

struct ClemensSerializerRecord kSCC[] = {
    CLEM_SERIALIZER_RECORD_EMPTY()
};

struct ClemensSerializerRecord kIWM[] = {
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

struct ClemensSerializerRecord kMachine[] = {
    CLEM_SERIALIZER_RECORD_OBJECT(ClemensMachine, cpu, struct Clemens65C816, kCPU),
    CLEM_SERIALIZER_RECORD_DURATION(ClemensMachine, clocks_step),
    CLEM_SERIALIZER_RECORD_DURATION(ClemensMachine, clocks_step_fast),
    CLEM_SERIALIZER_RECORD_DURATION(ClemensMachine, clocks_step_mega2),
    CLEM_SERIALIZER_RECORD_CLOCKS(ClemensMachine, clocks_spent),
    CLEM_SERIALIZER_RECORD_INT32(ClemensMachine, resb_counter),
    CLEM_SERIALIZER_RECORD_BOOL(ClemensMachine, mmio_bypass),
    CLEM_SERIALIZER_RECORD_OBJECT(ClemensMachine, mmio, struct ClemensMMIO, kMMIO),
    CLEM_SERIALIZER_RECORD_ARRAY(ClemensMachine,
        kClemensSerializerTypeBlob, fpi_bank_map, 256, CLEM_IIGS_BANK_SIZE),
    CLEM_SERIALIZER_RECORD_ARRAY(ClemensMachine,
        kClemensSerializerTypeBlob, mega2_bank_map, 2, CLEM_IIGS_BANK_SIZE),
    CLEM_SERIALIZER_RECORD_EMPTY()
};

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
        case kClemensSerializerTypeUInt64:
            variant.u64 = *(uint64_t *)(data_adr + record->offset);
            mpack_write_u64(writer, variant.u64);
            sz = sizeof(uint64_t);
            break;
        case kClemensSerializerTypeInt32:
            variant.i32 = *(int32_t *)(data_adr + record->offset);
            mpack_write_i32(writer, variant.i32);
            sz = sizeof(int32_t);
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
            if (variant.blob) {
                mpack_write_bin(writer, (const char *)variant.blob, record->size);
            } else {
                mpack_write_nil(writer);
            }
            sz = sizeof(uint8_t*);
            break;
        case kClemensSerializerTypeArray:
            sz = clemens_serialize_array(writer, data_adr + record->offset, record);
            break;
        case kClemensSerializerTypeObject:
            sz = clemens_serialize_object(writer, data_adr + record->offset, record);
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
    if (value_record.type == kClemensSerializerTypeBlob) {
        value_record.size = record->param;
    }
    for (idx = 0; idx < record->size; ++idx) {

        array_value_adr += clemens_serialize_record(
            writer, array_value_adr, &value_record);
    }
    mpack_finish_array(writer);
    return array_value_adr - data_adr;
}

unsigned clemens_serialize_object(
    mpack_writer_t* writer,
    uintptr_t data_adr,
    const struct ClemensSerializerRecord* record
) {
    const struct ClemensSerializerRecord* child = record->records;
    mpack_build_map(writer);
    while (child->type != kClemensSerializerTypeEmpty) {
        mpack_write_cstr(writer, child->name);
        clemens_serialize_record(writer, data_adr, child);
        ++child;
    }
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

    memset(&root, 0, sizeof(root));
    root.type = kClemensSerializerTypeRoot;
    root.records = &kMachine[0];
    clemens_serialize_object(writer, (uintptr_t)data_adr, &root);

    return writer;
}

/* Unserializing the Machine */

unsigned clemens_unserialize_record(
    mpack_reader_t* reader,
    uintptr_t data_adr,
    const struct ClemensSerializerRecord* record,
    ClemensSerializerAllocateCb alloc_cb
) {
    union ClemensSerializerVariant variant;
    unsigned sz = 0;
    mpack_tag_t tag;

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
        case kClemensSerializerTypeFloat:
            variant.f32 = mpack_expect_float(reader);
            *(float *)(data_adr + record->offset) = variant.f32;;
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
            tag = mpack_read_tag(reader);
            if (tag.type == mpack_type_bin) {
                sz = tag.v.l;
            } else {
                sz = 0;
            }
            if (sz) {
                variant.blob = *(uint8_t **)(data_adr + record->offset);
                if (!variant.blob) {
                    variant.blob = (*alloc_cb)(sz);
                }
                if (sz > record->size) {
                    return CLEM_SERIALIZER_INVALID_RECORD;
                }
                mpack_read_bytes(reader, (char *)variant.blob, sz);
            }
            *(uint8_t **)(data_adr + record->offset) = variant.blob;
            sz = sizeof(uint8_t*);
            break;
        case kClemensSerializerTypeArray:
            sz = clemens_unserialize_array(reader, data_adr + record->offset, record, alloc_cb);
            break;
        case kClemensSerializerTypeObject:
            sz = clemens_unserialize_object(reader, data_adr + record->offset, record, alloc_cb);
            break;
    }
    return sz;
}

unsigned clemens_unserialize_array(
    mpack_reader_t* reader,
    uintptr_t data_adr,
    const struct ClemensSerializerRecord* record,
    ClemensSerializerAllocateCb alloc_cb
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
    value_record.records = record->records;     /* for arrays of objects */
    for (idx = 0; idx < array_size; ++idx) {
        array_value_adr += clemens_unserialize_record(
            reader, array_value_adr, &value_record, alloc_cb);
    }
    mpack_done_array(reader);
    return array_value_adr - data_adr;
}

unsigned clemens_unserialize_object(
    mpack_reader_t* reader,
    uintptr_t data_adr,
    const struct ClemensSerializerRecord* record,
    ClemensSerializerAllocateCb alloc_cb
) {
    char key[64];
    struct ClemensSerializerRecord* child = record->records;
    mpack_expect_map(reader);
    while (child->type != kClemensSerializerTypeEmpty) {
        mpack_expect_cstr(reader, key, sizeof(key));
        clemens_unserialize_record(reader, data_adr, child, alloc_cb);
        ++child;
    }
    mpack_done_map(reader);
    return record->size;
}

mpack_reader_t* clemens_unserialize_machine(
    mpack_reader_t* reader,
    ClemensMachine* machine,
    ClemensSerializerAllocateCb alloc_cb
) {
    struct ClemensSerializerRecord* record = &kMachine[0];
    union ClemensSerializerVariant variant;
    void* data_adr = (void *)machine;

    clemens_unserialize_object(reader, (uintptr_t)data_adr, record, alloc_cb);

    return reader;
}
