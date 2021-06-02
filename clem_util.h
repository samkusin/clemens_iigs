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

static inline uint32_t _clem_calc_ns_step_from_clocks(
    ClemensMachine* clem,
    clem_clocks_duration_t clocks_step
) {
    return  (uint32_t)(
        CLEM_MEGA2_CYCLE_NS * clocks_step / clem->clocks_step_mega2);
}

static inline uint32_t _clem_calc_cycles_diff(
    uint32_t cycles_a,
    uint32_t cycles_b
) {
    uint32_t diff = cycles_b - cycles_a;
    if (cycles_b > cycles_a) {
        return cycles_b - cycles_a;
    } else {
        return cycles_b + (UINT32_MAX - cycles_a) + 1;
    }
}
