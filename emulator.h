#ifndef CLEM_EMULATOR_H
#define CLEM_EMULATOR_H

#include "clem_types.h"

#define clemens_is_irq_enabled(_clem_) (!((_clem_)->cpu.P & kClemensCPUStatus_IRQDisable))

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief
 *
 */
void clemens_register();

/**
 * @brief
 *
 * @param machine
 * @param speed_factor
 * @param clocks_step
 * @param rom
 * @param fpiROMBankCount
 * @param e0bank
 * @param e1bank
 * @param fpiRAM
 * @param fpiRAMBankCount
 * @return int
 */
int clemens_init(ClemensMachine *machine, uint32_t speed_factor, uint32_t clocks_step, void *rom,
                 unsigned int fpiROMBankCount, void *e0bank, void *e1bank, void *fpiRAM,
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
 * @brief Is the CPU in reset mode (resbIn is false and first cycle when true)
 *
 * @param clem
 * @return true
 * @return false
 */
bool clemens_is_resetting(const ClemensMachine *clem);

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
void clemens_host_setup(ClemensMachine *clem, ClemensLoggerFn logger, void *debug_user_ptr);

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
 * @brief Reads memory from the machine and copies to the output buffer in raw bytes
 *
 * This copies memory directly from the RAM bank, bypassing bank mapping (i.e)
 * bank 0 reads the physical bank 0 and ignores settings such as RAMRD on the
 * //gs.
 *
 * Note, adr is almost always 0 unless copying a partial bank.   In that case
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

#ifdef __cplusplus
}
#endif

#endif
