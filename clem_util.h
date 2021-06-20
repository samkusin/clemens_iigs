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
    clem_clocks_duration_t clocks_step,
    clem_clocks_duration_t clocks_step_reference
) {
    return  (uint32_t)(
        CLEM_MEGA2_CYCLE_NS * (uint64_t)clocks_step / clocks_step_reference);
}

static inline clem_clocks_duration_t _clem_calc_clocks_step_from_ns(
    unsigned ns,
    clem_clocks_duration_t clocks_step_reference
) {
    return  (clem_clocks_duration_t)(ns * clocks_step_reference) / (
        CLEM_MEGA2_CYCLE_NS);
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

static inline unsigned clem_util_timer_decrement(
    unsigned timer_ns,
    unsigned dt_ns
) {
    return (timer_ns - dt_ns < timer_ns) ? (timer_ns - dt_ns) : 0;
}

static inline unsigned clem_util_timer_increment(
    unsigned timer_ns,
    unsigned timer_max_ns,
    unsigned dt_ns
) {
    if (timer_ns + dt_ns > timer_ns) {
        timer_ns += dt_ns;
    }
    if (timer_ns + dt_ns < timer_ns) {
        timer_ns = UINT32_MAX;
    } else {
        timer_ns += dt_ns;
    }
    if (timer_ns > timer_max_ns) {
        timer_ns = timer_max_ns;
    }
    return timer_ns;
}
