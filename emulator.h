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
int clemens_init(
    ClemensMachine* machine,
    uint32_t speed_factor,
    uint32_t clocks_step,
    void* rom,
    size_t romSize,
    void* e0bank,
    void* e1bank,
    void* fpiRAM,
    void* slotROM,
    void* slotExpansionROM,
    unsigned int fpiRAMBankCount
);

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
bool clemens_is_initialized(const ClemensMachine* clem);

/**
 * @brief
 *
 * @param clem
 * @param drive_type
 * @param disk
 * @return true
 * @return false
 */
bool clemens_assign_disk(ClemensMachine* clem,
                         enum ClemensDriveType drive_type,
                         struct ClemensWOZDisk* disk);

/**
 * @brief
 *
 * @param clem
 * @param drive_type
 * @return true
 * @return false
 */
bool clemens_has_disk(ClemensMachine* clem, enum ClemensDriveType drive_type);

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
bool clemens_is_initialized_simple(const ClemensMachine* clem);

/**
 * @brief Trivial validation that the emulator memory interface has been
 *        initialized following a reset.
 *
 * @param machine The machine
 * @return true  MMIO is initialized (requires a reset)
 * @return false MMIO not initialized
 */
bool clemens_is_mmio_initialized(const ClemensMachine* machine);

/**
 * @brief
 *
 * @param clem
 * @param callback
 * @param callback_ptr
 */
void clemens_opcode_callback(ClemensMachine* clem,
                             ClemensOpcodeCallback callback,
                             void* callback_ptr);

/**
 * @brief
 *
 * @param clem
 * @param is_slow_speed
 * @return uint64_t
 */
uint64_t clemens_clocks_per_second(ClemensMachine* clem, bool* is_slow_speed);

/**
 * @brief Forwards input from ths host machine to the ADB
 *
 * @param clem
 * @param input
 */
void clemens_input(ClemensMachine* clem,
                   const struct ClemensInputEvent* input);

/**
 * @brief
 *
 * @param clem
 */
void clemens_emulate(ClemensMachine* clem);

/**
 * @brief
 *
 * @param clem
 */
void clemens_debug_status(ClemensMachine* clem);


/**
 * @brief Returns the current video data to be displayed by the host
 *
 * The data here is in the form of scanlines and a description of how to
 * interpret the data.  The host must convert this information to visuals.
 *
 * @param clem
 * @return ClemensVideo*
 */

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
ClemensVideo* clemens_get_text_video(ClemensVideo* video,
                                     ClemensMachine* clem);

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
ClemensVideo* clemens_get_graphics_video(ClemensVideo* video,
                                         ClemensMachine* clem);

#ifdef __cplusplus
}
#endif

#endif
