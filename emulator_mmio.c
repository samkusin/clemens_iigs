#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "clem_mmio_defs.h"

#include "emulator.h"
#include "emulator_mmio.h"

#include "clem_debug.h"
#include "clem_device.h"
#include "clem_drive.h"
#include "clem_util.h"
#include "clem_vgc.h"

#include "clem_2img.h"
#include "clem_woz.h"

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"

static uint8_t s_empty_ram[CLEM_IIGS_BANK_SIZE];

static const char *s_drive_names[] = {
    "ClemensDisk 3.5 D1",
    "ClemensDisk 3.5 D2",
    "ClemensDisk 5.25 D1",
    "ClemensDisk 5.25 D2",
};

struct ClemensDrive *clemens_drive_get(ClemensMMIO *mmio, enum ClemensDriveType drive_type) {
    struct ClemensDrive *drive;
    switch (drive_type) {
    case kClemensDrive_3_5_D1:
        drive = &mmio->active_drives.slot5[0];
        break;
    case kClemensDrive_3_5_D2:
        drive = &mmio->active_drives.slot5[1];
        break;
    case kClemensDrive_5_25_D1:
        drive = &mmio->active_drives.slot6[0];
        break;
    case kClemensDrive_5_25_D2:
        drive = &mmio->active_drives.slot6[1];
        break;
    default:
        drive = NULL;
        break;
    }
    return drive;
}

struct ClemensSmartPortUnit *clemens_smartport_unit_get(ClemensMMIO *mmio, unsigned unit_index) {
    if (unit_index >= CLEM_SMARTPORT_DRIVE_LIMIT)
        return NULL;
    return &mmio->active_drives.smartport[unit_index];
}

bool clemens_assign_disk(ClemensMMIO *mmio, enum ClemensDriveType drive_type,
                         struct ClemensNibbleDisk *disk) {
    struct ClemensDrive *drive = clemens_drive_get(mmio, drive_type);
    if (!drive) {
        return false;
    }
    if (disk && drive->has_disk) {
        /* active disk found.; must unassign first */
        return false;
    }
    if (!disk) {
        return false;
    }
    /* filter out 'bad' disk/drive pairings before the IWM has a chance to flag
       them
    */
    if (drive_type == kClemensDrive_5_25_D1 || drive_type == kClemensDrive_5_25_D2) {
        if (disk->disk_type != CLEM_DISK_TYPE_5_25) {
            return false;
        }
    } else if (drive_type == kClemensDrive_3_5_D1 || drive_type == kClemensDrive_3_5_D2) {
        if (disk->disk_type != CLEM_DISK_TYPE_3_5) {
            return false;
        }
    } else {
        return false;
    }
    if (disk->disk_type != CLEM_DISK_TYPE_NONE) {
        CLEM_LOG("%s inserting disk", s_drive_names[drive_type]);
    }
    clem_iwm_insert_disk(&mmio->dev_iwm, drive, disk);
    return true;
}

void clemens_eject_disk(ClemensMMIO *mmio, enum ClemensDriveType drive_type,
                        struct ClemensNibbleDisk *disk) {
    struct ClemensDrive *drive = clemens_drive_get(mmio, drive_type);
    if (!drive)
        return;

    if (drive->disk.disk_type != CLEM_DISK_TYPE_NONE) {
        CLEM_LOG("%s ejecting disk", s_drive_names[drive_type]);
    }
    clem_iwm_eject_disk(&mmio->dev_iwm, drive, disk);
}

bool clemens_eject_disk_async(ClemensMMIO *mmio, enum ClemensDriveType drive_type,
                              struct ClemensNibbleDisk *disk) {
    struct ClemensDrive *drive = clemens_drive_get(mmio, drive_type);
    if (drive) {
        if (drive->disk.disk_type != CLEM_DISK_TYPE_NONE) {
            CLEM_LOG("%s ejecting disk", s_drive_names[drive_type]);
        }
        return clem_iwm_eject_disk_async(&mmio->dev_iwm, drive, disk);
    }
    clem_iwm_eject_disk(&mmio->dev_iwm, drive, disk);
    return true;
}

bool clemens_assign_smartport_disk(ClemensMMIO *mmio, unsigned drive_index,
                                   struct ClemensSmartPortDevice *device) {
    if (drive_index >= CLEM_SMARTPORT_DRIVE_LIMIT)
        return false;
    memcpy(&mmio->active_drives.smartport[drive_index].device, device,
           sizeof(struct ClemensSmartPortDevice));
    return true;
}

void clemens_remove_smartport_disk(ClemensMMIO *mmio, unsigned drive_index,
                                   struct ClemensSmartPortDevice *device) {
    if (drive_index >= CLEM_SMARTPORT_DRIVE_LIMIT)
        return;

    if (mmio->active_drives.smartport[drive_index].device.device_id ==
        CLEM_SMARTPORT_DEVICE_ID_NONE)
        return;
    memcpy(device, &mmio->active_drives.smartport[drive_index].device,
           sizeof(struct ClemensSmartPortDevice));
    memset(&mmio->active_drives.smartport[drive_index].device, 0,
           sizeof(struct ClemensSmartPortDevice));
    mmio->active_drives.smartport[drive_index].unit_id = 0;
}

bool clemens_is_drive_io_active(ClemensMMIO *mmio) {
    return clem_iwm_is_active(&mmio->dev_iwm, &mmio->active_drives);
}

ClemensMonitor *clemens_get_monitor(ClemensMonitor *monitor, ClemensMMIO *mmio) {
    struct ClemensVGC *vgc = &mmio->vgc;

    //  TODO: use vgc flags to detect NTSC vs PAL, Mono vs RGB
    monitor->signal = CLEM_MONITOR_SIGNAL_NTSC;
    monitor->signal = CLEM_MONITOR_COLOR_RGB;
    monitor->border_color = mmio->dev_rtc.ctl_c034 & 0x0f;
    monitor->text_color = ((vgc->text_bg_color & 0xf) << 4) | (vgc->text_fg_color & 0xf);

    if (vgc->mode_flags & CLEM_VGC_SUPER_HIRES) {
        monitor->width = 640;
        monitor->height = 400;
    } else {
        monitor->width = 560;
        monitor->height = 384;
    }
    return monitor;
}

ClemensVideo *clemens_get_text_video(ClemensVideo *video, ClemensMMIO *mmio) {
    struct ClemensVGC *vgc = &mmio->vgc;
    video->vbl_counter = vgc->vbl_counter;
    if (!(vgc->mode_flags & CLEM_VGC_GRAPHICS_MODE)) {
        video->scanline_start = 0;
    } else if (vgc->mode_flags & CLEM_VGC_MIXED_TEXT) {
        video->scanline_start = 20;
    } else {
        video->format = kClemensVideoFormat_None;
        return NULL;
    }
    video->scanline_count = CLEM_VGC_TEXT_SCANLINE_COUNT - video->scanline_start;
    video->scanline_limit = CLEM_VGC_TEXT_SCANLINE_COUNT;
    video->format = kClemensVideoFormat_Text;
    video->scanline_byte_cnt = 40;
    if ((mmio->mmap_register & CLEM_MEM_IO_MMAP_TXTPAGE2) &&
        !(mmio->mmap_register & CLEM_MEM_IO_MMAP_80COLSTORE)) {
        video->scanlines = vgc->text_2_scanlines;
    } else {
        video->scanlines = vgc->text_1_scanlines;
    }
    return video;
}

void _clem_build_rgba_palettes(ClemensVideo *video, uint8_t *e1_bank) {
    /* 4 bits per channel -> 8 bits
       byte 0 - green:blue, byte 1 - none:red nibbles
    */
    uint8_t *src = e1_bank + 0x9e00;
    unsigned i, r, g, b;
    for (i = 0; i < 256; ++i, src += 2) {
        b = src[0] & 0xf;
        g = src[0] >> 4;
        r = src[1] & 0xf;

        video->rgba[i] =
            (((r << 4) | r) << 24) | (((g << 4) | g) << 16) | (((b << 4) | b) << 8) | 0xff;
    }
    src = e1_bank + 0x9d00;
    for (i = 0; i < video->scanline_count; ++i) {
        video->scanlines[i].control = src[i];
    }
}

ClemensVideo *clemens_get_graphics_video(ClemensVideo *video, ClemensMachine *clem,
                                         ClemensMMIO *mmio) {
    struct ClemensVGC *vgc = &mmio->vgc;
    bool use_page_2 = (mmio->mmap_register & CLEM_MEM_IO_MMAP_TXTPAGE2) &&
                      !(mmio->mmap_register & CLEM_MEM_IO_MMAP_80COLSTORE);
    video->vbl_counter = vgc->vbl_counter;
    if (vgc->mode_flags & CLEM_VGC_SUPER_HIRES) {
        video->format = kClemensVideoFormat_Super_Hires;
        video->scanline_count = CLEM_VGC_SHGR_SCANLINE_COUNT;
        video->scanline_byte_cnt = 160;
        video->scanline_limit = CLEM_VGC_SHGR_SCANLINE_COUNT;
        video->scanlines = vgc->shgr_scanlines;
        _clem_build_rgba_palettes(video, clem->mem.mega2_bank_map[1]);
        return video;
    } else if (vgc->mode_flags & CLEM_VGC_GRAPHICS_MODE) {
        video->scanline_start = 0;
        if (vgc->mode_flags & CLEM_VGC_HIRES) {
            if ((vgc->mode_flags & CLEM_VGC_DBLRES_MASK) == CLEM_VGC_DBLRES_MASK) {
                video->format = kClemensVideoFormat_Double_Hires;
            } else {
                video->format = kClemensVideoFormat_Hires;
            }
            if (vgc->mode_flags & CLEM_VGC_MIXED_TEXT) {
                video->scanline_count = CLEM_VGC_HGR_SCANLINE_COUNT - 32;
            } else {
                video->scanline_count = CLEM_VGC_HGR_SCANLINE_COUNT;
            }
            video->scanline_limit = CLEM_VGC_HGR_SCANLINE_COUNT;
        } else {
            if ((vgc->mode_flags & CLEM_VGC_DBLRES_MASK) == CLEM_VGC_DBLRES_MASK) {
                video->format = kClemensVideoFormat_Double_Lores;
            } else {
                video->format = kClemensVideoFormat_Lores;
            }
            if (vgc->mode_flags & CLEM_VGC_MIXED_TEXT) {
                video->scanline_count = CLEM_VGC_TEXT_SCANLINE_COUNT - 4;
            } else {
                video->scanline_count = CLEM_VGC_TEXT_SCANLINE_COUNT;
            }
            video->scanline_limit = CLEM_VGC_TEXT_SCANLINE_COUNT;
        }
        video->scanline_byte_cnt = 40;
    } else {
        video->format = kClemensVideoFormat_None;
        return NULL;
    }
    if (vgc->mode_flags & CLEM_VGC_HIRES) {
        if (use_page_2) {
            video->scanlines = vgc->hgr_2_scanlines;
        } else {
            video->scanlines = vgc->hgr_1_scanlines;
        }
    } else {
        if (use_page_2) {
            video->scanlines = vgc->text_2_scanlines;
        } else {
            video->scanlines = vgc->text_1_scanlines;
        }
    }
    return video;
}

void clemens_assign_audio_mix_buffer(ClemensMMIO *mmio, struct ClemensAudioMixBuffer *mix_buffer) {
    memcpy(&mmio->dev_audio.mix_buffer, mix_buffer, sizeof(struct ClemensAudioMixBuffer));
    clem_sound_reset(&mmio->dev_audio);
}

ClemensAudio *clemens_get_audio(ClemensAudio *audio, ClemensMMIO *mmio) {
    struct ClemensDeviceAudio *device = &mmio->dev_audio;
    audio->data = device->mix_buffer.data;
    audio->frame_start = 0;
    audio->frame_count = device->mix_frame_index;
    audio->frame_stride = device->mix_buffer.stride;
    audio->frame_total = device->mix_buffer.frame_count;

    return audio;
}

void clemens_audio_next_frame(ClemensMMIO *mmio, unsigned consumed) {
    clem_sound_consume_frames(&mmio->dev_audio, consumed);
}

void clemens_input(ClemensMMIO *mmio, const struct ClemensInputEvent *input) {
    clem_adb_device_input(&mmio->dev_adb, input);
}

void clemens_input_key_toggle(ClemensMMIO *mmio, unsigned enabled) {
    clem_adb_device_key_toggle(&mmio->dev_adb, enabled);
}

unsigned clemens_get_adb_key_modifier_states(ClemensMMIO *mmio) {
    unsigned key_mod_state = mmio->dev_adb.keyb_reg[2];
    if (mmio->dev_adb.keyb.states[CLEM_ADB_KEY_ESCAPE]) {
        key_mod_state |= CLEM_ADB_KEY_MOD_STATE_ESCAPE;
    }
    return key_mod_state;
}

const uint8_t *clemens_get_ascii_from_a2code(unsigned input) {
    return clem_adb_ascii_from_a2code(input);
}

void clemens_rtc_set(ClemensMMIO *mmio, uint32_t seconds_since_1904) {
    clem_rtc_set_clock_time(&mmio->dev_rtc, seconds_since_1904);
}

const uint8_t *clemens_rtc_get_bram(ClemensMMIO *mmio, bool *is_dirty) {
    bool flag = clem_rtc_clear_bram_dirty(&mmio->dev_rtc);
    if (is_dirty) {
        *is_dirty = flag;
    }
    return mmio->dev_rtc.bram;
}

void clemens_rtc_set_bram_dirty(ClemensMMIO *mmio) { clem_rtc_set_bram_dirty(&mmio->dev_rtc); }

uint64_t clemens_clocks_per_second(ClemensMMIO *mmio, bool *is_slow_speed) {
    if (mmio->speed_c036 & CLEM_MMIO_SPEED_FAST_ENABLED) {
        *is_slow_speed = false;
    } else {
        *is_slow_speed = true;
    }
    return mmio->clocks_step_mega2 * CLEM_MEGA2_CYCLES_PER_SECOND;
}

static void _clem_mmio_write_hook(struct ClemensMemory *mem, struct ClemensTimeSpec *tspec,
                                  uint8_t data, uint16_t addr, uint8_t flags,
                                  bool *is_slow_access) {
    clem_mmio_write((ClemensMMIO *)mem->mmio_context, tspec, data, addr, flags, is_slow_access);
}

static uint8_t _clem_mmio_read_hook(struct ClemensMemory *mem, struct ClemensTimeSpec *tspec,
                                    uint16_t addr, uint8_t flags, bool *is_slow_access) {
    return clem_mmio_read((ClemensMMIO *)mem->mmio_context, tspec, addr, flags, is_slow_access);
}

static bool _clem_mmio_niolc(struct ClemensMemory *mem) {
    ClemensMMIO *mmio = (ClemensMMIO *)mem->mmio_context;
    return (mmio->mmap_register & CLEM_MEM_IO_MMAP_NIOLC) != 0;
}

void clemens_emulate_mmio(ClemensMachine *clem, ClemensMMIO *mmio) {
    struct Clemens65C816 *cpu = &clem->cpu;
    struct ClemensClock clock;
    uint32_t delta_mega2_cycles;
    uint32_t card_result;
    uint32_t card_irqs;
    uint32_t card_nmis;
    unsigned i;

    if (!cpu->pins.resbIn) {
        mmio->state_type = kClemensMMIOStateType_Reset;
        return;
    }
    if (mmio->state_type == kClemensMMIOStateType_Reset) {
        clem->mem.mmio_context = mmio;
        clem->mem.mmio_write = _clem_mmio_write_hook;
        clem->mem.mmio_read = _clem_mmio_read_hook;
        clem->mem.mmio_niolc = _clem_mmio_niolc;
        clem_disk_reset_drives(&mmio->active_drives);
        clem_mmio_reset(mmio, clem->tspec.clocks_step_mega2);
        /* extension cards reset handling */
        clem_iwm_speed_disk_gate(mmio, &clem->tspec);
        clock.ts = clem->tspec.clocks_spent;
        clock.ref_step = clem->tspec.clocks_step_mega2;
        for (i = 0; i < CLEM_CARD_SLOT_COUNT; ++i) {
            if (mmio->card_slot[i]) {
                mmio->card_slot[i]->io_reset(&clock, mmio->card_slot[i]->context);
            }
        }
        clem_iwm_speed_disk_gate(mmio, &clem->tspec);

        mmio->state_type = kClemensMMIOStateType_Active;
        return;
    }
    if (mmio->state_type != kClemensMMIOStateType_Active)
        return;

    //  record the last data access for switches that check if an I/O was accessed
    //  twice in succession
    if (clem->cpu.pins.vdaOut) {
        mmio->last_data_address = (((uint32_t)clem->cpu.pins.bank) << 16) | clem->cpu.pins.adr;
    }

    clem_iwm_speed_disk_gate(mmio, &clem->tspec);

    //  1 mega2 cycle = 1023 nanoseconds
    //  1 fast cycle = 1023 / (2864/1023) nanoseconds

    //  1 fast cycle = 1 mega2 cycle (ns) / (clocks_step_mega2 / clocks_step) =
    //      (1 mega2 cycle (ns) * clocks_step) / clocks_step_mega2

    // TODO: calculate delta_ns per emulate call to call 'real-time' systems
    //      like VGC
    delta_mega2_cycles =
        (uint32_t)((clem->tspec.clocks_spent / clem->tspec.clocks_step_mega2) - mmio->mega2_cycles);
    mmio->mega2_cycles += delta_mega2_cycles;
    mmio->timer_60hz_us += delta_mega2_cycles;

    clock.ts = clem->tspec.clocks_spent;
    clock.ref_step = clem->tspec.clocks_step_mega2;

    card_nmis = 0;
    card_irqs = 0;
    for (i = 0; i < CLEM_CARD_SLOT_COUNT; ++i) {
        if (!mmio->card_slot[i])
            continue;
        card_result = (*mmio->card_slot[i]->io_sync)(&clock, mmio->card_slot[i]->context);
        if (card_result & CLEM_CARD_IRQ)
            card_irqs |= (CLEM_IRQ_SLOT_1 << i);
        if (card_result & CLEM_CARD_NMI)
            card_nmis |= (1 << i);
    }

    clem_vgc_sync(&mmio->vgc, &clock, clem->mem.mega2_bank_map[0], clem->mem.mega2_bank_map[1]);
    clem_iwm_glu_sync(&mmio->dev_iwm, &mmio->active_drives, &clock);
    clem_scc_glu_sync(&mmio->dev_scc, &clock);
    clem_sound_glu_sync(&mmio->dev_audio, &clock);
    clem_gameport_sync(&mmio->dev_adb.gameport, &clock);

    /* background execution of some async devices on the 60 hz timer */
    while (mmio->timer_60hz_us >= CLEM_MEGA2_CYCLES_PER_60TH) {
        clem_timer_sync(&mmio->dev_timer, CLEM_MEGA2_CYCLES_PER_60TH);
        clem_adb_glu_sync(&mmio->dev_adb, CLEM_MEGA2_CYCLES_PER_60TH);
        if (clem->resb_counter <= 0 && mmio->dev_adb.keyb.reset_key) {
            /* TODO: move into its own utility */
            clem->resb_counter = 2;
            clem->cpu.pins.resbIn = false;
        }
        mmio->timer_60hz_us -= CLEM_MEGA2_CYCLES_PER_60TH;
    }

    mmio->irq_line = (mmio->dev_adb.irq_line | mmio->dev_timer.irq_line | mmio->dev_audio.irq_line |
                      mmio->vgc.irq_line | card_irqs);
    mmio->nmi_line = card_nmis;
    clem_iwm_speed_disk_gate(mmio, &clem->tspec);

    cpu->pins.irqbIn = mmio->irq_line == 0;
    cpu->pins.nmibIn = mmio->nmi_line == 0;

    /* IRQB low triggers an interrupt next frame */
    if (!cpu->pins.irqbIn && cpu->state_type == kClemensCPUStateType_Execute) {
        if (!(cpu->regs.P & kClemensCPUStatus_IRQDisable)) {
            cpu->state_type = kClemensCPUStateType_IRQ;
        }
    }
    /* NMIB overrides IRQB settings and ignores IRQ disable */
    if (!cpu->pins.nmibIn) {
        cpu->state_type = kClemensCPUStateType_NMI;
    }
}
