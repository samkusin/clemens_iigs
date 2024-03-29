#ifndef CLEM_EMULATOR_MMIO_H
#define CLEM_EMULATOR_MMIO_H

#include "clem_disk.h"
#include "clem_mmio.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief
 *
 * @param mmio
 * @return true
 * @return false
 */
bool clemens_is_mmio_initialized(const ClemensMMIO *mmio);

/**
 * @brief Emulate the I/O portion of an Apple IIgs
 *
 * This should be paired with calls to clemens_emulate_cpu().  The calls are
 * separated out from the original clemens_emulate() function to: facilitate
 * unit testing, and to allow emulation of other 65816 devices.
 *
 * @param clem
 * @param mmio
 */
void clemens_emulate_mmio(ClemensMachine *clem, ClemensMMIO *mmio);

/**
 * @brief Returns the emulated system's clocks per second
 *
 * @param mmio The MMIO component
 * @param is_slow_speed (Required) Points to a boolean indicating current system speed as read from
 *                      the SPEED register (not BRAM)
 * @return uint64_t The clocks per second
 */
uint64_t clemens_clocks_per_second(ClemensMMIO *mmio, bool *is_slow_speed);

/**
 * @brief Sets the audio buffer used by the clemens audio system for mixing
 * Ensoniq and Apple II speaker output
 *
 * @param mmio
 * @param mix_buffer
 */
void clemens_assign_audio_mix_buffer(ClemensMMIO *mmio, struct ClemensAudioMixBuffer *mix_buffer);

/**
 * @brief Return the current audio buffer, and advances the read head to the
 * current write head
 *
 * @param audio
 * @param mmio
 * @return ClemensAudio*
 */
ClemensAudio *clemens_get_audio(ClemensAudio *audio, ClemensMMIO *mmio);

/**
 * @brief After the host is done with the audio frame, call this.
 *
 * @param clem
 * @param consumed
 */
void clemens_audio_next_frame(ClemensMMIO *mmio, unsigned consumed);

/**
 * @brief
 *
 * @param mmio
 * @param drive_type
 * @return struct ClemensDrive*
 */
struct ClemensDrive *clemens_drive_get(ClemensMMIO *mmio, enum ClemensDriveType drive_type);

/**
 * @brief
 *
 * @param mmio
 * @param drive_type
 * @return struct ClemensDrive*
 */
struct ClemensSmartPortUnit *clemens_smartport_unit_get(ClemensMMIO *mmio, unsigned unit_index);

/**
 * @brief
 *
 * @param mmio
 * @param drive_type
 * @param disk
 * @return true
 * @return false
 */
bool clemens_assign_disk(ClemensMMIO *mmio, enum ClemensDriveType drive_type,
                         struct ClemensNibbleDisk *disk);

/**
 * @brief
 *
 * @param mmio
 * @param drive_type
 * @param bits_data
 * @param bits_data_end
 */
void clemens_assign_disk_buffer(ClemensMMIO *mmio, enum ClemensDriveType drive_type,
                                uint8_t *bits_data, uint8_t *bits_data_end);

/**
 * @brief Insert a disk of specified type, returning the disk that emulators populate
 *
 * @param mmio
 * @param drive_type
 * @return struct ClemensNibbleDisk*
 */
struct ClemensNibbleDisk *clemens_insert_disk(ClemensMMIO *mmio, enum ClemensDriveType drive_type);

/**
 * @brief
 *
 * @param mmio
 * @param drive_type
 * @return struct ClemensNibbleDisk*
 */
struct ClemensNibbleDisk *clemens_eject_disk(ClemensMMIO *mmio, enum ClemensDriveType drive_type);

/**
 * @brief Polls the current eject status until done (returns false)
 *
 * @param mmio
 * @param drive_type
 * @return unsigned See CLEM_EJECT_DISK_STATUS_XXX
 */
unsigned clemens_eject_disk_in_progress(ClemensMMIO *mmio, enum ClemensDriveType drive_type);

/**
 * @brief Assigns a SmartPort device to one of the available SmartPort slots.
 *
 * Note, that the any device *contexts* are assumed to be managed by the calling
 * application.  The device object itself is copied to one of the available slots.
 *
 * @param mmio
 * @param drive_index Always 0 for now
 * @param device This object is copied into one the available slots
 * @return true
 * @return false
 */
bool clemens_assign_smartport_disk(ClemensMMIO *mmio, unsigned drive_index,
                                   struct ClemensSmartPortDevice *device);

/**
 * @brief Unassigned the designated SmartPort device.
 *
 * The device data is copied into the passed device struct and cleared from the
 * active drive bay.
 *
 * @param mmio
 * @param drive_index The device at this index is removed.
 * @param device The assigned device is copied into this structure.
 * @return true
 * @return false
 */
void clemens_remove_smartport_disk(ClemensMMIO *mmio, unsigned drive_index,
                                   struct ClemensSmartPortDevice *device);

/**
 * @brief If the IWM is actively reading or writing to a drive, returns true.
 *
 * This can be used by the emulator to turn on fast emulation when reading
 * from a disk.
 *
 * Criteria for this state depends on a combination of IWM states.
 *
 * @param mmio
 * @return true The IWM is actively reading or writing to a drive
 * @return false No drive access
 */
bool clemens_is_drive_io_active(ClemensMMIO *mmio);

/**
 * @brief Forwards input from ths host machine to the ADB
 *
 * Note, mouse movement input can either be relative or absolute.  If absolute,
 * then tracking is enabled - else if relative, deltas are assumed and mouse
 * position is tracked by the emulated firmware
 *
 * @param mmio
 * @param input
 */
void clemens_input(ClemensMMIO *mmio, const struct ClemensInputEvent *input);

/**
 * @brief Forwards state of toggle keys to the emulator
 *
 * @param mmio
 * @param enabled See CLEM_ADB_KEYB_TOGGLE_xxx
 * @param disabled See CLEM_ADB_KEYB_TOGGLE_xxx
 */
void clemens_input_key_toggle(ClemensMMIO *mmio, unsigned enabled);

/**
 * @brief Retrieve the current state of ADB related modifier keys
 *
 * @param mmio
 * @return See CLEM_ADB_GLU_REG2_xxx
 */
unsigned clemens_get_adb_key_modifier_states(ClemensMMIO *mmio);

/**
 * @brief
 *
 * @param input
 * @return const uint8_t*
 */
const uint8_t *clemens_get_ascii_from_a2code(unsigned input);

/**
 * @brief
 *
 * @param mmio
 * @param utf8_str
 * @return const char* Pointer to the next item, utf8_end if there are no more atoms, or equal to
 * utf8_str if the internal queue is full
 */
const char *clemens_clipboard_push_utf8_atom(ClemensMMIO *mmio, const char *utf8_str, const char* utf8_end);

/**
 * @brief
 *
 * @param clem
 * @param seconds_since_1904
 */
void clemens_rtc_set(ClemensMMIO *mmio, uint32_t seconds_since_1904);

/**
 * @brief
 *
 * @param mmio
 */
void clemens_rtc_set_bram_dirty(ClemensMMIO *mmio);

/**
 * @brief This function can be used to persist RTC BRAM data if it has changed
 *
 * @param mmio
 * @param is_dirty
 * @return const uint8_t*
 */
const uint8_t *clemens_rtc_get_bram(ClemensMMIO *mmio, bool *is_dirty);

/**
 * @brief Returns the current monitor settings.
 *
 * This should be used to render the video display on the host.  The return
 * values are tuned to Apple II peculiarities (i.e. Hires, Double Hires).  For
 * example, while ClemensMonitor identifies the display at 'NTSC', 'PAL', and
 * 'color' vs 'monochrome', display resolution is specialized for Apple II
 * scaled (560x384) vs Apple IIgs scaled (640x400).   These resolutions are
 * suggested ones, designed to reflect maximum resolutions and easily
 * downscaled 'lo-res' equivalents.
 *
 * @param monitor
 * @param mmio
 * @return ClemensMonitor*
 */
ClemensMonitor *clemens_get_monitor(ClemensMonitor *monitor, ClemensMMIO *mmio);

/**
 * @brief Returns the current text video data to be displayed by the host
 *
 * The data here is in the form of scanlines and a description of how to
 * interpret the data.  The host must convert this information to visuals.
 *
 * @param video
 * @param mmio
 * @return ClemensVideo*
 */
ClemensVideo *clemens_get_text_video(ClemensVideo *video, ClemensMMIO *mmio);

/**
 * @brief Returns the current graphics video data to be displayed by the host
 *
 * The data here is in the form of scanlines and a description of how to
 * interpret the data.  The host must convert this information to visuals.
 *
 * @param video
 * @param mmio
 * @return ClemensVideo*
 */
ClemensVideo *clemens_get_graphics_video(ClemensVideo *video, ClemensMachine *machine,
                                         ClemensMMIO *mmio);

/**
 * @brief Converts monitor coordinates to the specified video view window coordinates
 *
 * The monitor coordinates must reside from 0 to monitor.width for x, and 0 to
 * monitor.height for y.
 *
 * @param monitor
 * @param video
 * @param vx [out]  Video X
 * @param vy [out]  Video Y
 * @param mx Monitor X
 * @param my Monitor Y
 */
void clemens_monitor_to_video_coordinates(ClemensMonitor *monitor, ClemensVideo *video, int16_t *vx,
                                          int16_t *vy, int16_t mx, int16_t my);

#ifdef __cplusplus
}
#endif

#endif
