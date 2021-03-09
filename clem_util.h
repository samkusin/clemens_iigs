
static inline uint8_t* _clem_get_memory_bank(
    ClemensMachine* clem,
    uint8_t bank,
    uint32_t* clocks_spent
) {
    if (bank == 0xe0 || bank == 0xe1) {
        *clocks_spent = clem->clocks_step_mega2;
        return clem->mega2_bank_map[bank & 0x1];
    }
    *clocks_spent = clem->clocks_step;
    return clem->fpi_bank_map[bank];
}
