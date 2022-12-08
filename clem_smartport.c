#include "clem_smartport.h"
#include "clem_defs.h"

/*
   Bus Reset starts the unit assignment process from the host.

   Each device will force PH30 on its *output* phase, which is passed to the
   next device on the daisy-chained disk port.

   The host will reenable the bus and assign a unit ID to the next available
   device in the chain. See do_enable() for details on subsequent events.

   To achieve this, on do_reset(), each device maintains an 'available' flag
   which is TRUE once it receives its ID assignment.  When 'available', the
   device no longer forces PH3 to 0 on its output and the next device will
   pick up on the PH3 as set by the host.
*/
bool clem_smartport_do_reset(struct ClemensSmartPortUnit *unit, unsigned unit_count,
                             unsigned *io_flags, unsigned *out_phase, unsigned delta_ns) {
    unsigned select_bits = *out_phase;
    if (select_bits != (1 + 4)) {
        return false;
    }
    return true;
}

/*
  Bus Enable marks that all 'available' bus residents can handle SmartPort
  commands.  See do_reset() for details on how a device becomes available.

  Much of the implementation follows the SmartPort protocol diagrams in
  Chapter 7 of the Apple IIgs firmware reference.

  Each device may have its own command handler.  They share common bus
  negotation logic with the host, which is provided below.
*/
bool clem_smartport_do_enable(struct ClemensSmartPortUnit *unit, unsigned unit_count,
                              unsigned *io_flags, unsigned *out_phase, unsigned delta_ns) {
    unsigned select_bits = *out_phase;
    if (!(select_bits & 2) || !(select_bits & 8)) {
        return false;
    }

    //  ACK is HIGH for now
    *io_flags |= CLEM_IWM_FLAG_WRPROTECT_SENSE;

    return true;
}
