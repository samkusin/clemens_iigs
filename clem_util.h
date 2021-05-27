/**
 * @brief Returns a pointer to the memory bank contents and if its a mega2 bank
 *
 * NOTE: the mega2 flag is only changed if the bank is a mega2 bank (and left
 * unchanged if not)
 *
 * @param clem
 * @param bank
 * @param mega2
 * @return uint8_t*
 */
static inline uint8_t* _clem_get_memory_bank(
    ClemensMachine* clem,
    uint8_t bank,
    bool* mega2
) {
    if (bank == 0xe0 || bank == 0xe1) {
        *mega2 = true;
        return clem->mega2_bank_map[bank & 0x1];
    }
    return clem->fpi_bank_map[bank];
}
