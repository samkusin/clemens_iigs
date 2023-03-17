//  clock step fast or slow
//      clocks_spent is our reference clock in units
//      fast cycles simply increment the clock by CLEM_CLOCKS_PHI2_FAST_CYCLE
//      slow cycles must synchronize with the PHI0 clock
//          this clock runs 64 cycles of CLEM_CLOCKS_PHI0_CYCLE clocks
//              + 1 CLEM_CLOCKS_7MHZ_CYCLE known as the stretch (NTSC at leaast)
//          synchronize to the next cycle (either PHI0 or PHI0 stretch) if needed
//              for consecutive PHI0 cycles, this step will be skipped
//          advance one PHI0 or PHI0 stretch
//

static inline void _clem_timespec_init(struct ClemensTimeSpec *tspec,
                                       clem_clocks_duration_t clocks_step) {
    tspec->clocks_step = clocks_step;
    tspec->clocks_step_fast = clocks_step;
    tspec->clocks_spent = 0;

    //  Initialize the PHI0 synchronization value
    //  TODO: change when supporting PAL
    tspec->phi0_clocks_stretch = CLEM_CLOCKS_7MHZ_CYCLE;
    tspec->phi0_current_step = CLEM_CLOCKS_PHI0_CYCLE;
    tspec->clocks_next_phi0 = CLEM_CLOCKS_PHI0_CYCLE;
    tspec->mega2_scanline_ctr = 0;
}

static inline void _clem_timespec_next_step(struct ClemensTimeSpec *tspec,
                                            clem_clocks_duration_t clocks) {
    tspec->clocks_spent += clocks;
    //  next phi0 edge calculated below, accounting for the stretch cycle
    if (tspec->clocks_spent >= tspec->clocks_next_phi0) {
        tspec->mega2_scanline_ctr = (tspec->mega2_scanline_ctr + 1) % 65;
        tspec->phi0_current_step = CLEM_CLOCKS_PHI0_CYCLE;
        if (tspec->mega2_scanline_ctr == 64) {
            tspec->phi0_current_step += tspec->phi0_clocks_stretch;
        }
        tspec->clocks_next_phi0 += tspec->phi0_current_step;
    }
}

static inline void _clem_timespec_cycle(struct ClemensTimeSpec *tspec, bool m2sel) {
    if (m2sel) {
        //  synchronize with the PHI0 clock (sync_clocks = 0 infers clock already in sync)
        clem_clocks_duration_t sync_clocks =
            (tspec->clocks_next_phi0 - tspec->clocks_spent) % tspec->phi0_current_step;
        _clem_timespec_next_step(tspec, sync_clocks);
        _clem_timespec_next_step(tspec, tspec->phi0_current_step);
    } else {
        _clem_timespec_next_step(tspec, tspec->clocks_step);
    }
}

static inline void _clem_cycle(struct ClemensMachine *clem) {
    _clem_timespec_cycle(&clem->tspec, clem->tspec.clocks_step == CLEM_CLOCKS_PHI0_CYCLE);
    ++clem->cpu.cycles_spent;
}
static inline void _clem_cycle_2(struct ClemensMachine *clem) {
    _clem_timespec_cycle(&clem->tspec, clem->tspec.clocks_step == CLEM_CLOCKS_PHI0_CYCLE);
    _clem_timespec_cycle(&clem->tspec, clem->tspec.clocks_step == CLEM_CLOCKS_PHI0_CYCLE);
    clem->cpu.cycles_spent += 2;
}

static inline void _clem_mem_cycle(ClemensMachine *clem, bool mega2_access) {
    _clem_timespec_cycle(&clem->tspec,
                         clem->tspec.clocks_step == CLEM_CLOCKS_PHI0_CYCLE || mega2_access);
    ++clem->cpu.cycles_spent;
}
