#ifndef CLEM_EMULATOR_MMIO_H
#define CLEM_EMULATOR_MMIO_H

#include "clem_mmio.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Emulate the I/O portion of an Apple IIgs
 *
 * This should be paired with calls to clemens_emulate_cpu().  The calls are
 * separated out from the original clemens_emulate() function to: facilitate
 * unit testing, and to allow emulation of other 65816 devices.
 *
 * @param clem
 */
void clemens_emulate_mmio(ClemensMachine *clem, ClemensMMIO *mmio);

/**
 * @brief
 *
 * @param clem
 * @param is_slow_speed
 * @return uint64_t
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
 * @param disk  The output disk
 */
void clemens_eject_disk(ClemensMMIO *mmio, enum ClemensDriveType drive_type,
                        struct ClemensNibbleDisk *disk);

/**
 * @brief Performs a device specific eject of a disk (3.5" disks mainly.)
 *
 * The eject sequence can take a second or so if exact Apple Drive 3.5"
 * emulation is desired.
 *
 * @param mmio
 * @param drive_type
 * @param disk  The final nibbilised disk object
 * @return true Disk has ejected
 * @return false Disk is still ejecting
 */
bool clemens_eject_disk_async(ClemensMMIO *mmio, enum ClemensDriveType drive_type,
                              struct ClemensNibbleDisk *disk);

/**
 * @brief Forwards input from ths host machine to the ADB
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
 * @brief
 *
 * @param input
 * @return const uint8_t*
 */
const uint8_t *clemens_get_ascii_from_a2code(unsigned input);

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

#ifdef __cplusplus
}
#endif

#endif
