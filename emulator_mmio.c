#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "clem_disk.h"
#include "clem_mem.h"
#include "clem_mmio_defs.h"

#include "emulator_mmio.h"

#include "clem_debug.h"
#include "clem_device.h"
#include "clem_drive.h"
#include "clem_scc.h"
#include "clem_util.h"
#include "clem_vgc.h"


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

//  TODO:
//  1. 'disk' is no longer passed in
//  2. assign and eject no longer take a disk pointer
//  3. drive->has_disk determines whether the disk is mounted
//  4. clemens_insert_disk returns the disk pointer for apps to
//     write to
//  5. clemens_eject_disk returns the disk pointer for apps to read from
//  6. there will be no memcpys in assign and eject since the pointers
//     to drive->disk are the ones used for reading and writing disks
//  7. clemens_assign_disk_buffer(mmio, drive_type, bits, bits_end)
//     called by application on init()
//  8. this means clem_storage_unit.cpp doesn't allocate any nibble buffers
//     this means clem_storage_unit constructor takes as input the hard disk
//     buffer, and so does unserialize()
//          - clem_storage_unit does not handle allocation of disk buffers anymore
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
    clem_iwm_insert_disk_old(&mmio->dev_iwm, drive, disk);
    return true;
}

void clemens_assign_disk_buffer(ClemensMMIO *mmio, enum ClemensDriveType drive_type,
                                uint8_t *bits_data, uint8_t *bits_data_end) {
    struct ClemensDrive *drive = clemens_drive_get(mmio, drive_type);
    if (!drive)
        return;
    if (drive_type == kClemensDrive_3_5_D1 || drive_type == kClemensDrive_3_5_D2) {
        drive->disk.disk_type = CLEM_DISK_TYPE_3_5;
    } else if (drive_type == kClemensDrive_5_25_D1 || drive_type == kClemensDrive_5_25_D2) {
        drive->disk.disk_type = CLEM_DISK_TYPE_5_25;
    } else {
        drive->disk.disk_type = CLEM_DISK_TYPE_NONE;
    }
    drive->disk.bits_data = bits_data;
    drive->disk.bits_data_end = bits_data_end;
}

struct ClemensNibbleDisk *clemens_insert_disk(ClemensMMIO *mmio, enum ClemensDriveType drive_type) {
    struct ClemensDrive *drive = clemens_drive_get(mmio, drive_type);
    struct ClemensNibbleDisk *disk = NULL;
    if (drive) {
        disk = clem_iwm_insert_disk(&mmio->dev_iwm, drive);
        if (disk) {
            CLEM_LOG("%s inserting disk", s_drive_names[drive_type]);
        }
    }
    return disk;
}

unsigned clemens_eject_disk_in_progress(ClemensMMIO *mmio, enum ClemensDriveType drive_type) {
    struct ClemensDrive *drive = clemens_drive_get(mmio, drive_type);
    if (!drive)
        return CLEM_EJECT_DISK_STATUS_NONE;

    return clem_iwm_eject_disk_in_progress(&mmio->dev_iwm, drive);
}

struct ClemensNibbleDisk *clemens_eject_disk(ClemensMMIO *mmio, enum ClemensDriveType drive_type) {
    struct ClemensDrive *drive = clemens_drive_get(mmio, drive_type);
    if (!drive)
        return NULL;
    return clem_iwm_eject_disk(&mmio->dev_iwm, drive);
}

bool clemens_assign_smartport_disk(ClemensMMIO *mmio, unsigned drive_index,
                                   struct ClemensSmartPortDevice *device) {
    if (drive_index >= CLEM_SMARTPORT_DRIVE_LIMIT)
        return false;
    if (mmio->active_drives.smartport[drive_index].device.device_id !=
        CLEM_SMARTPORT_DEVICE_ID_NONE)
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
    mmio->active_drives.smartport[drive_index].unit_id = CLEM_SMARTPORT_DEVICE_ID_NONE;
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

ClemensVideo *clemens_get_graphics_video(ClemensVideo *video, ClemensMachine *clem,
                                         ClemensMMIO *mmio) {
    struct ClemensVGC *vgc = &mmio->vgc;
    uint8_t *memory;
    int i;
    bool use_page_2 = (mmio->mmap_register & CLEM_MEM_IO_MMAP_TXTPAGE2) &&
                      !(mmio->mmap_register & CLEM_MEM_IO_MMAP_80COLSTORE);
    video->vbl_counter = vgc->vbl_counter;
    video->rgb_buffer_size = 0;
    video->rgb = NULL;
    video->has_640_mode_scanlines = false;
    if (vgc->mode_flags & CLEM_VGC_SUPER_HIRES) {
        video->format = kClemensVideoFormat_Super_Hires;
        video->scanline_count = CLEM_VGC_SHGR_SCANLINE_COUNT;
        video->scanline_byte_cnt = 160;
        video->scanline_limit = CLEM_VGC_SHGR_SCANLINE_COUNT;
        video->scanlines = vgc->shgr_scanlines;
        video->rgb = vgc->shgr_palettes;
        video->rgb_buffer_size = sizeof(vgc->shgr_palettes);
        memory = clem->mem.mega2_bank_map[1] + 0x9d00;
        for (i = 0; i < video->scanline_count; ++i) {
            unsigned control = memory[i];
            video->scanlines[i].control = control;
            video->has_640_mode_scanlines = video->has_640_mode_scanlines ||
                                            ((control & CLEM_VGC_SCANLINE_CONTROL_640_MODE) != 0);
        }
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

void clemens_monitor_to_video_coordinates(ClemensMonitor *monitor, ClemensVideo *video, int16_t *vx,
                                          int16_t *vy, int16_t mx, int16_t my) {
    switch (video->format) {
    case kClemensVideoFormat_Super_Hires:
        if (video->has_640_mode_scanlines) {
            *vx = mx;
        } else {
            *vx = mx / 2;
        }
        *vy = my / 2;
        break;
    case kClemensVideoFormat_Double_Hires:
        *vx = mx;
        *vy = my / 2;
        break;
    case kClemensVideoFormat_Hires:
        *vx = mx / 2;
        *vy = my / 2;
        break;
    case kClemensVideoFormat_Double_Lores:
        *vx = mx / 7;
        *vy = my / 8;
        break;
    case kClemensVideoFormat_Lores:
        *vx = mx / 14;
        *vy = my / 8;
        break;
    default:
        break;
    }
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
    return CLEM_CLOCKS_PHI0_CYCLE * CLEM_MEGA2_CYCLES_PER_SECOND;
}

// https://stackoverflow.com/questions/12855643/how-to-convert-a-string-from-utf8-to-latin1-in-c-c
// converts this character to an ISO latin 1 ASCII equivalent - encoding unrecognized values
// using percent-sign encoding
//
// items like \n are converted to \r unless preceeded by a \r
//
const char *clemens_clipboard_push_utf8_atom(ClemensMMIO *mmio, const char *utf8_str,
                                             const char *utf8_end) {
    //  suboptimal but only called once every fraction of a second while there's an active clipboard
    //  there are methods to make this branchless - investigate if performance becomes an issue.
    unsigned len = 0, utf8_code, shift;
    unsigned char ch;

    if (mmio->dev_adb.clipboard.tail > CLEM_ADB_CLIPBOARD_BUFFER_LIMIT / 2) {
        return utf8_str;
    }

    if (utf8_str != utf8_end) {
        //  Determine UTF8 atom to ingest
        ch = (unsigned char)(*utf8_str);
        len = ch < 0x80 ? 1 : !(ch & 0x20) ? 2 : !(ch & 0x10) ? 3 : !(ch * 0x08) ? 4 : len;
        if (utf8_str + len > utf8_end)
            len = 0; /* throw out non utf8 bytes */
    }
    if (len == 0)
        return utf8_end;
    if (len == 1) {
        //  skip \r as it will (usually) be succeeded by a \n (if not, then ???)
        if (ch != '\r') {
            if (ch == '\n')
                ch = '\r';
            clem_adb_clipboard_push_ascii_char(&mmio->dev_adb, ch);
        }
    } else {
        utf8_code = (unsigned char)(*utf8_str & (0xff >> (len + 1))) << ((len - 1) * 6);
        for (--len; len; --len)
            utf8_code |= ((unsigned char)(*(++utf8_str)) - 0x80) << ((len - 1) * 6);
        // percent encode the bytes
        shift = 32;
        while (shift) {
            shift -= 8;
            ch = ((utf8_code >> shift) & 0xff);
            if (ch) {
                clem_adb_clipboard_push_ascii_char(&mmio->dev_adb, '%');
                clem_adb_clipboard_push_ascii_char(&mmio->dev_adb, '0' + (ch >> 4));
                clem_adb_clipboard_push_ascii_char(&mmio->dev_adb, '0' + (ch & 0xf));
            }
        }
    }
    return utf8_str + len;
}

bool clemens_is_mmio_initialized(const ClemensMMIO *mmio) {
    return mmio->state_type == kClemensMMIOStateType_Active;
}

void clemens_emulate_mmio(ClemensMachine *clem, ClemensMMIO *mmio) {
    struct Clemens65C816 *cpu = &clem->cpu;
    struct ClemensClock clock;
    struct ClemensDeviceMega2Memory m2mem;
    uint32_t delta_mega2_cycles;
    uint32_t card_result;
    uint32_t card_irqs;
    uint32_t card_nmis;
    unsigned i, cyc;
    ClemensCard* card;
    uint8_t dma_bank, dma_data;
    uint16_t dma_addr;
    uint8_t dma_latch;

    if (!cpu->pins.resbIn) {
        //  don't actually process MMIO until reset cycle has completed (resbIn==true)
        mmio->state_type = kClemensMMIOStateType_Reset;
        return;
    }
    if (mmio->state_type == kClemensMMIOStateType_Reset) {
        clem_mmio_bind_machine(clem, mmio);
        clem_disk_reset_drives(&mmio->active_drives);
        clem_mmio_reset(mmio, &clem->tspec);
        /* extension cards reset handling */
        clem_iwm_speed_disk_gate(mmio, &clem->tspec);
        clock.ts = clem->tspec.clocks_spent;
        clock.ref_step = CLEM_CLOCKS_PHI0_CYCLE;
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

    //  TODO: this mega2_cycles thing is not really used (really old code)... remove it.
    //        deal with 60hz timer differently.
    delta_mega2_cycles =
        (uint32_t)((clem->tspec.clocks_spent / CLEM_CLOCKS_PHI0_CYCLE) - mmio->mega2_cycles);
    mmio->mega2_cycles += delta_mega2_cycles;
    mmio->timer_60hz_us += delta_mega2_cycles;

    m2mem.e0_bank = mmio->e0_bank;
    m2mem.e1_bank = mmio->e1_bank;

    clock.ts = clem->tspec.clocks_spent;
    clock.ref_step = CLEM_CLOCKS_PHI0_CYCLE;

    card_nmis = 0;
    card_irqs = 0;
    for (i = 0; i < CLEM_CARD_SLOT_COUNT; ++i) {
        card = mmio->card_slot[i];
        if (!card)
            continue;
        card_result = (*card->io_sync)(&clock, card->context);
        if (card_result & CLEM_CARD_IRQ)
            card_irqs |= (CLEM_IRQ_SLOT_1 << i);
        if (card_result & CLEM_CARD_NMI)
            card_nmis |= (1 << i);
        if (card_result & CLEM_CARD_DMA) {
            //  run one dma per mega2 cycle - perhaps this is overkill given
            //  our use-case
            for (cyc = 0; cyc < delta_mega2_cycles; ++cyc) {
                //  address bus half-cycle
                dma_latch = (*card->io_dma)(&dma_bank, &dma_addr, true, card->context);
                if (!dma_latch) {
                    //  read data half-cycle
                    clem_read(clem, &dma_data, dma_addr, dma_bank, 0);
                }
                //  data half-cycle
                dma_latch = (*card->io_dma)(&dma_data, &dma_addr, false, card->context);
                if (dma_latch) {
                    //  write data half-cycle
                    clem_write(clem, dma_data, dma_addr, dma_bank, 0);     
                }
            }
        }
    }

    clem_vgc_sync(&mmio->vgc, &clock, clem->mem.mega2_bank_map[0], clem->mem.mega2_bank_map[1]);
    clem_iwm_glu_sync(&mmio->dev_iwm, &mmio->active_drives, &clem->tspec);
    clem_scc_glu_sync(&mmio->dev_scc, &clock);
    clem_sound_glu_sync(&mmio->dev_audio, &clock);
    clem_gameport_sync(&mmio->dev_adb.gameport, &clock);

    /* background execution of some async devices on the 60 hz timer */
    /* TODO: ADB autopoll should occur on the VBL, also mega2 cycles aren't 1us (close!)
       ADB should use clocks like all other subsystems (ADB was the second subsystem written
       for this emulator!) */
    clem_adb_glu_sync(&mmio->dev_adb, &m2mem, delta_mega2_cycles);

    while (mmio->timer_60hz_us >= CLEM_MEGA2_CYCLES_PER_60TH) {
        clem_timer_sync(&mmio->dev_timer, CLEM_MEGA2_CYCLES_PER_60TH);
        if (clem->resb_counter <= 0 && mmio->dev_adb.keyb.reset_key) {
            /* TODO: move into its own utility */
            clem->resb_counter = 2;
            clem->cpu.pins.resbIn = false;
        }
        mmio->timer_60hz_us -= CLEM_MEGA2_CYCLES_PER_60TH;
    }

    mmio->irq_line = (mmio->dev_adb.irq_line | mmio->dev_timer.irq_line | mmio->dev_audio.irq_line |
                      mmio->vgc.irq_line | mmio->dev_scc.irq_line | card_irqs);
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
