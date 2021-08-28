#include "serializer.h"

/* Serializing the Machine */

mpack_writer_t* clemens_serialize_machine(
    mpack_writer_t* writer,
    ClemensMachine* machine
) {
    //  save the following states
    //      card memory or references to cards
    //      mmio
    //      disk drives
    unsigned idx;

    //  begin
    mpack_build_map(writer);
    //      timers
    mpack_write_cstr(writer, "clocks_step");
    mpack_write_uint(writer, machine->clocks_step);
    mpack_write_cstr(writer, "clocks_step_fast");
    mpack_write_uint(writer, machine->clocks_step_fast);
    mpack_write_cstr(writer, "clocks_step_mega2");
    mpack_write_uint(writer, machine->clocks_step_mega2);
    mpack_write_cstr(writer, "clocks_spent");
    mpack_write_uint(writer, machine->clocks_spent);
    mpack_write_cstr(writer, "resb_counter");
    mpack_write_int(writer, machine->resb_counter);
    mpack_write_cstr(writer, "mmio_bypass");
    mpack_write_bool(writer, machine->mmio_bypass);
    //      cpu
    mpack_write_cstr(writer, "cpu");
    clemens_serialize(writer, &machine->cpu);
    //      memory
    mpack_write_cstr(writer, "fpi_bank_memory");
    mpack_start_array(writer, 256);
    for (idx = 0; idx < 256; ++idx) {
        mpack_write_bin(writer,(char *)machine->fpi_bank_map[idx], CLEM_IIGS_BANK_SIZE);
    }
    mpack_finish_array(writer);

    //      page map - this is not necessary as the values are
    //      hard coded by mmio_init and we can select the desired map
    //      after unserialization
    /*
    mpack_write_cstr(writer, "bank_page_map");
    mpack_start_array(writer, 256);
    for (idx = 0; idx < 256; ++idx) {

    }
    */
    //      card memory doesn't likely need to be serialized as the hardware
    //      ROMs are static.   the card states are covered in the mmio section
    /*
    mpack_write_cstr(writer, "card_slot_memory");
    mpack_start_array(writer, 7);
    for (idx = 0; idx < 7; ++idx) {

    }
    */
    //      mmio
    mpack_write_cstr(writer, "mega2_bank_map");
    mpack_start_array(writer, 2);
    for (idx = 0; idx < 2; ++idx) {
        mpack_write_bin(writer,(char *)machine->mega2_bank_map[idx], CLEM_IIGS_BANK_SIZE);
    }
    mpack_finish_array(writer);
    mpack_write_cstr(writer, "mmio");
    clemens_serialize_mmio(writer, &machine->mmio);
    mpack_write_cstr(writer, "active_drives");
    clemens_serialize_drives(writer, &machine->active_drives);

    //  end
    mpack_finish_map(writer);

    return writer;
}

mpack_writer_t* clemens_serialize_cpu(
    mpack_writer_t* writer,
    struct Clemens65C816* cpu
) {
    mpack_build_map(writer);
    mpack_write_cstr(writer, "pins");
    mpack_build_map(writer);
    mpack_write_cstr(writer, "adr");
    mpack_write_u16(writer, cpu->pins.adr);
    mpack_write_cstr(writer, "bank");
    mpack_write_u8(writer, cpu->pins.bank);
    mpack_write_cstr(writer, "data");
    mpack_write_u8(writer, cpu->pins.data);
    mpack_write_cstr(writer, "abortIn");
    mpack_write_bool(writer, cpu->pins.abortIn);
    mpack_write_cstr(writer, "busEnableIn");
    mpack_write_bool(writer, cpu->pins.busEnableIn);
    mpack_write_cstr(writer, "irqbIn");
    mpack_write_bool(writer, cpu->pins.irqbIn);
    mpack_write_cstr(writer, "nmiIn");
    mpack_write_bool(writer, cpu->pins.nmiIn);
    mpack_write_cstr(writer, "readyOut");
    mpack_write_bool(writer, cpu->pins.readyOut);
    mpack_write_cstr(writer, "resbIn");
    mpack_write_bool(writer, cpu->pins.resbIn);
    mpack_write_cstr(writer, "emulation");
    mpack_write_bool(writer, cpu->pins.emulation);
    mpack_write_cstr(writer, "vdaOut");
    mpack_write_bool(writer, cpu->pins.vdaOut);
    mpack_write_cstr(writer, "vpaOut");
    mpack_write_bool(writer, cpu->pins.vpaOut);
    mpack_write_cstr(writer, "rwbOut");
    mpack_write_bool(writer, cpu->pins.rwbOut);
    mpack_finish_map(writer);

    mpack_write_cstr(writer, "A");
    mpack_write_u16(writer, cpu->regs.A);
    mpack_write_cstr(writer, "X");
    mpack_write_u16(writer, cpu->regs.X);
    mpack_write_cstr(writer, "Y");
    mpack_write_u16(writer, cpu->regs.Y);
    mpack_write_cstr(writer, "D");
    mpack_write_u16(writer, cpu->regs.D);
    mpack_write_cstr(writer, "S");
    mpack_write_u16(writer, cpu->regs.S);
    mpack_write_cstr(writer, "PC");
    mpack_write_u16(writer, cpu->regs.PC);
    mpack_write_cstr(writer, "IR");
    mpack_write_u8(writer, cpu->regs.IR);
    mpack_write_cstr(writer, "P");
    mpack_write_u8(writer, cpu->regs.P);
    mpack_write_cstr(writer, "DBR");
    mpack_write_u8(writer, cpu->regs.DBR);
    mpack_write_cstr(writer, "PBR");
    mpack_write_u8(writer, cpu->regs.PBR);

    mpack_write_cstr(writer, "state_type");
    mpack_write_u8(writer, (uint8_t)cpu->state_type);
    mpack_write_cstr(writer, "cycles_spent");
    mpack_write_u32(writer, cpu->cycles_spent);
    mpack_write_cstr(writer, "enabled");
    mpack_write_bool(writer, cpu->enabled);

    mpack_finish_map(writer);
    return writer;
}


mpack_writer_t* clemens_serialize_mmio(
    mpack_writer_t* writer,
    struct ClemensMMIO* mmio
) {
    /* the page maps are not serialized - these are initialized on
       initialization and are meant to be consistent between sessions

       same goes for shadow maps - the mmap_register is used to initialize
       these values
    */
    unsigned idx;

    mpack_build_map(writer);
    /* VGC */
    mpack_write_cstr(writer, "VGC");
    mpack_build_map(writer);
    /* scanlines are intialized at start time */
    mpack_write_cstr(writer, "ts_last_frame");
    mpack_write_u64(writer, mmio->vgc.ts_last_frame);
    mpack_write_cstr(writer, "ts_scanline_0");
    mpack_write_u64(writer, mmio->vgc.ts_scanline_0);
    mpack_write_cstr(writer, "dt_scanline");
    mpack_write_u32(writer, mmio->vgc.dt_scanline);
    mpack_write_cstr(writer, "mode_flags");
    mpack_write_uint(writer, mmio->vgc.mode_flags);
    mpack_write_cstr(writer, "text_fg_color");
    mpack_write_uint(writer, mmio->vgc.text_fg_color);
    mpack_write_cstr(writer, "text_bg_color");
    mpack_write_uint(writer, mmio->vgc.text_bg_color);
    mpack_write_cstr(writer, "text_language");
    mpack_write_uint(writer, mmio->vgc.text_language);
    mpack_write_cstr(writer, "irq_line");
    mpack_write_uint(writer, mmio->vgc.irq_line);
    mpack_finish_map(writer);

    /* RTC */
    mpack_write_cstr(writer, "RTC");
    mpack_build_map(writer);
    mpack_write_cstr(writer, "xfer_started_time");
    mpack_write_u64(writer, mmio->dev_rtc.xfer_started_time);
    mpack_write_cstr(writer, "xfer_latency_duration");
    mpack_write_u64(writer, mmio->dev_rtc.xfer_latency_duration);
    mpack_write_cstr(writer, "state");
    mpack_write_uint(writer, mmio->dev_rtc.state);
    mpack_write_cstr(writer, "index");
    mpack_write_uint(writer, mmio->dev_rtc.index);
    mpack_write_cstr(writer, "flags");
    mpack_write_uint(writer, mmio->dev_rtc.flags);
    mpack_write_cstr(writer, "seconds_since_1904");
    mpack_write_uint(writer, mmio->dev_rtc.seconds_since_1904);
    mpack_write_cstr(writer, "data_c033");
    mpack_write_u8(writer, mmio->dev_rtc.data_c033);
    mpack_write_cstr(writer, "ctl_c034");
    mpack_write_u8(writer, mmio->dev_rtc.ctl_c034);
    mpack_write_cstr(writer, "bram");
    mpack_start_array(writer, 256);
    for (idx = 0; idx < 256; idx++) {
        mpack_write_u8(writer, mmio->dev_rtc.bram[idx]);
    }
    mpack_finish_array(writer);
    mpack_finish_map(writer);

    mpack_finish_map(writer);



    return writer;
}

mpack_writer_t* clemens_serialize_drives(
    mpack_writer_t* writer,
    struct ClemensDriveBay* drives
) {
    return writer;
}

/* Unserializing the Machine */

mpack_reader_t* clemens_unserialize_machine(
    mpack_reader_t* reader,
    ClemensMachine* machine
) {
    return reader;
}

mpack_reader_t* clemens_unserialize_cpu(
    mpack_reader_t* reader,
    struct Clemens65C816* cpu
) {
    return reader;
}


mpack_reader_t* clemens_unserialize_mmio(
    mpack_reader_t* reader,
    struct ClemensMMIO* mmio
) {
    return reader;
}

mpack_reader_t* clemens_unserialize_drives(
    mpack_reader_t* reader,
    struct ClemensDriveBay* cpu
) {
    return reader;
}
