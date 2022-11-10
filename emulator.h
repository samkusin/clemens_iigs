#ifndef CLEM_EMULATOR_H
#define CLEM_EMULATOR_H

#include "clem_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief
 *
 * @param machine
 * @param speed_factor
 * @param clocks_step
 * @param rom
 * @param romSize
 * @param e0bank
 * @param e1bank
 * @param fpiRAM
 * @param fpiRAMBankCount
 * @return int
 */
int clemens_init(ClemensMachine *machine, uint32_t speed_factor, uint32_t clocks_step, void *rom,
                 size_t romSize, void *e0bank, void *e1bank, void *fpiRAM, void *slotExpansionROM,
                 unsigned int fpiRAMBankCount);

/**
 * @brief initializes an empty machine that acts as a 65816 with just RAM
 *
 * Useful for test purposes (i.e. Klaus2m5's test suite, etc).  It is up to
 * the host to implement memory mapped I/O and supply a memory map of the first
 * 64K so that the 65816 can boot/reset in emulation mode.  As such, calling
 * any audio/video/disk functions here will not work as those functions rely on
 * a full Apple IIgs ROM and I/O implementation.
 *
 * @param machine
 * @param speed_factor
 * @param clocks_step
 * @param fpiRAM
 * @param fpiRAMBankCount
 */
void clemens_simple_init(ClemensMachine *machine, uint32_t speed_factor, uint32_t clocks_step,
                         void *fpiRAM, unsigned int fpiRAMBankCount);

/**
 * @brief
 *
 * @param machine
 */
void clemens_debug_context(ClemensMachine *machine);

/**
 * @brief Verify the machine is initialized/ready for emulation
 *
 * The emulator does not ever allocate memory.  This method checks if the
 * machine was initialized via clemens_init with valid parameters.
 *
 * @param clem The machine
 * @return true The machine has ROM/RAM and clock settings
 * @return false
 */
bool clemens_is_initialized(const ClemensMachine *clem);

/**
 * @brief Trivial validation function (one conditional)
 *
 * Better used for polling every frame vs a more comprehensive check via
 * clemens_is_initialized
 *
 * @param clem The machine
 * @return true Passes a trivial check
 * @return false Fails a trivial check
 */
bool clemens_is_initialized_simple(const ClemensMachine *clem);

/**
 * @brief Emulate the 65816 CPU only
 *
 * Usually this call is paired with another API to handle memory-mapped I/O (MMIO).
 * For Apple IIgs emulation, call clemens_emulate_mmio() following this call.  Unit
 * tests can also test CPU only functions when calling this function
 *
 * If emulating a 65816 machine other than the Apple IIgs, calling this function
 * by itself will suffice (provided that the ClemensMachine was initialized in
 * its 'simple' mode.)
 *
 * @param clem
 */
void clemens_emulate_cpu(ClemensMachine *clem);

/**
 * @brief Emulate the I/O portion of an Apple IIgs
 *
 * This should be paired with calls to clemens_emulate_cpu().  The calls are
 * separated out from the original clemens_emulate() function to: facilitate
 * unit testing, and to allow emulation of other 65816 devices.
 *
 * @param clem
 */
void clemens_emulate_mmio(ClemensMachine *clem);

/**
 * @brief Defines the logger function and machine specific context.
 *
 * This must be called before using other debug functionality.  It is mandatory
 * for logging.
 *
 * For generalized debugging using the clem_debug device, see
 * clemens_debug_context().
 *
 * @param clem
 * @param logger
 * @param debug_user_ptr
 */
void clemens_host_setup(ClemensMachine *clem, LoggerFn logger, void *debug_user_ptr);

/**
 * @brief
 *
 * Must call clemens_host_setup() before defining a callback tp ensure context
 * is delivered to the callback.
 *
 * @param clem
 * @param callback
 */
void clemens_opcode_callback(ClemensMachine *clem, ClemensOpcodeCallback callback);

/**
 * @brief
 *
 * @param page
 * @param page_idx
 * @param bank_read_idx
 * @param bank_write_idx
 */
void clemens_create_page_mapping(struct ClemensMemoryPageInfo *page, uint8_t page_idx,
                                 uint8_t bank_read_idx, uint8_t bank_write_idx);

/**
 * @brief Upload hex data directly into machine memory
 *
 * Data is uploaded directly by bank and address, bypassing any memory maps
 * defined.
 *
 * @param clem
 * @param hex
 * @param hex_end
 * @param bank  This is ignored if the incoming hex points to a 32-bit address
 * @return bool
 */
bool clemens_load_hex(ClemensMachine *clem, const char *hex, const char *hex_end, unsigned bank);

/**
 * @brief
 *
 * If writing to an intel hex file, the maximum out_byte_limit is 255 * 2
 *
 * Absolute limit is 256 bytes of output otherwise.
 *
 * @param clem
 * @param hex
 * @param out_byte_limit The number of hex digits that can be written out with
 *  with room for a null terminator
 * @param bank
 * @param adr
 */
unsigned clemens_out_hex_data_body(const ClemensMachine *clem, char *hex,
                                   unsigned out_hex_byte_limit, unsigned bank, unsigned adr);

/**
 * @brief
 *
 * @param hex
 * @param memory
 * @param out_hex_byte_limit
 * @param adr
 * @return unsigned
 */
unsigned clemens_out_hex_data_from_memory(char *hex, const uint8_t *memory,
                                          unsigned out_hex_byte_limit, unsigned adr);

/**
 * @brief Writes memory from the machine to the output buffer in raw bytes
 *
 * Note, adr is almost always 0 unless copying a portial bank.   In that case
 * if adr + out_byte_cnt > 64K, this API will copy the data into the out buffer
 * sequentially (out = data at adr + out_byte_cnt, and the data in the bank
 * preceeding the data at adr will be copied after that - essentially wrapping
 * around the bank boundary*)
 *
 * @param clem
 * @param out
 * @param out_byte_cnt
 * @param bank
 * @param adr
 */
void clemens_out_bin_data(const ClemensMachine *clem, uint8_t *out, unsigned out_byte_cnt,
                          uint8_t bank, uint16_t adr);

/**
 * @brief
 *
 * @param clem
 * @param is_slow_speed
 * @return uint64_t
 */
uint64_t clemens_clocks_per_second(ClemensMachine *clem, bool *is_slow_speed);

/**
 * @brief Trivial validation that the emulator memory interface has been
 *        initialized following a reset.
 *
 * @param machine The machine
 * @return true  MMIO is initialized (requires a reset)
 * @return false MMIO not initialized
 */
bool clemens_is_mmio_initialized(const ClemensMachine *machine);

/**
 * @brief Sets the audio buffer used by the clemens audio system for mixing
 * Ensoniq and Apple II speaker output
 *
 * @param clem
 * @param mix_buffer
 */
void clemens_assign_audio_mix_buffer(ClemensMachine *clem,
                                     struct ClemensAudioMixBuffer *mix_buffer);

/**
 * @brief Return the current audio buffer, and advances the read head to the
 * current write head
 *
 * @param audio
 * @param clem
 * @return ClemensAudio*
 */
ClemensAudio *clemens_get_audio(ClemensAudio *audio, ClemensMachine *clem);

/**
 * @brief After the host is done with the audio frame, call this.
 *
 * @param clem
 * @param consumed
 */
void clemens_audio_next_frame(ClemensMachine *clem, unsigned consumed);

/**
 * @brief
 *
 * @param clem
 * @param drive_type
 * @return struct ClemensDrive*
 */
struct ClemensDrive *clemens_drive_get(ClemensMachine *clem, enum ClemensDriveType drive_type);

/**
 * @brief
 *
 * @param clem
 * @param drive_type
 * @param disk
 * @return true
 * @return false
 */
bool clemens_assign_disk(ClemensMachine *clem, enum ClemensDriveType drive_type,
                         struct ClemensNibbleDisk *disk);

/**
 * @brief
 *
 * @param clem
 * @param drive_type
 * @param disk  The output disk
 */
void clemens_eject_disk(ClemensMachine *clem, enum ClemensDriveType drive_type,
                        struct ClemensNibbleDisk *disk);

/**
 * @brief Performs a device specific eject of a disk (3.5" disks mainly.)
 *
 * The eject sequence can take a second or so if exact Apple Drive 3.5"
 * emulation is desired.
 *
 * @param clem
 * @param drive_type
 * @param disk  The final nibbilised disk object
 * @return true Disk has ejected
 * @return false Disk is still ejecting
 */
bool clemens_eject_disk_async(ClemensMachine *clem, enum ClemensDriveType drive_type,
                              struct ClemensNibbleDisk *disk);

/**
 * @brief Forwards input from ths host machine to the ADB
 *
 * @param clem
 * @param input
 */
void clemens_input(ClemensMachine *clem, const struct ClemensInputEvent *input);

/**
 * @brief Forwards state of toggle keys to the emulator
 *
 * @param clem
 * @param enabled See CLEM_ADB_KEYB_TOGGLE_xxx
 * @param disabled See CLEM_ADB_KEYB_TOGGLE_xxx
 */
void clemens_input_key_toggle(ClemensMachine *clem, unsigned enabled);

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
void clemens_rtc_set(ClemensMachine *clem, uint32_t seconds_since_1904);

/**
 * @brief
 *
 * @param clem
 */
void clemens_rtc_set_bram_dirty(ClemensMachine *clem);

/**
 * @brief This function can be used to persist RTC BRAM data if it has changed
 *
 * @param clem
 * @param is_dirty
 * @return const uint8_t*
 */
const uint8_t *clemens_rtc_get_bram(ClemensMachine *clem, bool *is_dirty);

/**
 * @brief
 *
 * @param clem
 */
void clemens_debug_status(ClemensMachine *clem);

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
 * @param clem
 * @return ClemensMonitor*
 */
ClemensMonitor *clemens_get_monitor(ClemensMonitor *monitor, ClemensMachine *clem);

/**
 * @brief Returns the current text video data to be displayed by the host
 *
 * The data here is in the form of scanlines and a description of how to
 * interpret the data.  The host must convert this information to visuals.
 *
 * @param video
 * @param clem
 * @return ClemensVideo*
 */
ClemensVideo *clemens_get_text_video(ClemensVideo *video, ClemensMachine *clem);

/**
 * @brief Returns the current graphics video data to be displayed by the host
 *
 * The data here is in the form of scanlines and a description of how to
 * interpret the data.  The host must convert this information to visuals.
 *
 * @param video
 * @param clem
 * @return ClemensVideo*
 */
ClemensVideo *clemens_get_graphics_video(ClemensVideo *video, ClemensMachine *clem);

#ifdef __cplusplus
}
#endif

#endif
