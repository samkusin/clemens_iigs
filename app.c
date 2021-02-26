#include <stdio.h>
#include <stdlib.h>

#include "emulator.h"

/**
 *  The Apple //gs Clements Emulator
 *
 *  CPU
 *  Mega II emulation
 *  Memory
 *    ROM
 *    RAM
 *  I/O
 *    IWM
 *    ADB (keyboard + mouse)
 *    Ports 1-7
 *    Ensoniq
 *
 * Approach:
 *
 */
int main(int argc, char* argv[])
{
    ClemensMachine machine;

    /*  ROM 3 only */
    FILE* fp = fopen("gs_rom_3.rom", "rb");
    //FILE* fp = fopen("testrom.rom", "rb");
    void* rom = NULL;
    if (fp == NULL) {
        fprintf(stderr, "No ROM\n");
        return 1;
    }
    rom = malloc(CLEM_IIGS_ROM3_SIZE);
    if (fread(rom, 1, CLEM_IIGS_ROM3_SIZE, fp) != CLEM_IIGS_ROM3_SIZE) {
        fprintf(stderr, "Bad ROM\n");
        return 1;
    }
    fclose(fp);

    memset(&machine, 0, sizeof(machine));
    clemens_init(&machine, 1000, 1000, rom, 256 * 1024,
                 malloc(CLEM_IIGS_BANK_SIZE),
                 malloc(CLEM_IIGS_BANK_SIZE),
                 malloc(CLEM_IIGS_BANK_SIZE * 16),
                 16);

    machine.cpu.pins.resbIn = false;
    clemens_emulate(&machine);
    machine.cpu.pins.resbIn = true;

    while (machine.cpu.cycles_spent < 1024) {
        clemens_emulate(&machine);
    }

    return 0;
}
