/*
    Tests main/aux bank softswitches (not ALTZP, which is covered in lcbank)

    Cases:
      - 0x2000-0xBFFF area
      - RAMRD
      - Shadowed writes to aux using RAMRD (validate with contents of
        banks E0/E1)
*/
