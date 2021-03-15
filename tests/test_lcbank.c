#include "munit/munit.h"

/*
 Prerequisites:
 * An initialized ClemensMachine structure with a trivial ROM
 * MMIO has been initialized
 * Reset cycle has completed

 Approach:
 * Invoke clem_read/clem_write as needed to trigger the LCBANK2 softswitches
 * Perform a read or write action appropriate to the test
 * Verify read/write worked by checking raw memory

 */
