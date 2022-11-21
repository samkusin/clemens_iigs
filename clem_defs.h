#ifndef CLEMENS_DEFS_H
#define CLEMENS_DEFS_H

#include "clem_shared.h"

/* The emulator ID as defined when reading c04f */
#define CLEM_EMULATOR_ID  0xce
#define CLEM_EMULATOR_VER 0x01

#define CLEM_MEGA2_CYCLES_PER_60TH (CLEM_MEGA2_CYCLES_PER_SECOND / 60)
#define CLEM_MEGA2_TIMER_1SEC_US   1000000
#define CLEM_MEGA2_TIMER_QSEC_US   266667

#define CLEM_1SEC_NS 1000000000
#define CLEM_1MS_NS  1000000

#define CLEM_MONITOR_SIGNAL_NTSC 0
#define CLEM_MONITOR_SIGNAL_PAL  1

#define CLEM_MONITOR_COLOR_RGB  0
#define CLEM_MONITOR_COLOR_MONO 1

/** Emulated duration of every 'step' iwm_glu_sync runs. 1.023 / 2 ~ 0.511 */
#define CLEM_IWM_SYNC_FRAME_NS           (CLEM_MEGA2_CYCLE_NS / 2)
#define CLEM_IWM_SYNC_FRAME_NS_FAST      (CLEM_MEGA2_CYCLE_NS / 4)
#define CLEM_IWM_SYNC_DISK_FRAME_NS      500
#define CLEM_IWM_SYNC_DISK_FRAME_NS_FAST 250
#define CLEM_IWM_DRIVE_RANDOM_BYTES      16
#define CLEM_IWM_DEBUG_BUFFER_SIZE       256

/* NTSC scanlines start at counter 7 and end at 198 (192 lines)
   VBL begins at 199 (scanline 192)
   see technote 39, 40 and clem_vgc.c for links
*/
#define CLEM_VGC_HORIZ_SCAN_TIME_NS   63700
#define CLEM_VGC_NTSC_SCANLINE_COUNT  262
#define CLEM_VGC_NTSC_SCAN_TIME_NS    (CLEM_VGC_HORIZ_SCAN_TIME_NS * CLEM_VGC_NTSC_SCANLINE_COUNT)
#define CLEM_VGC_VBL_NTSC_LOWER_BOUND 199
#define CLEM_VGC_VBL_NTSC_UPPER_BOUND (CLEM_VGC_NTSC_SCANLINE_COUNT - 1)
#define CLEM_VGC_PAL_SCANLINE_COUNT   312
#define CLEM_VGC_PAL_SCAN_TIME_NS     (CLEM_VGC_HORIZ_SCAN_TIME_NS * CLEM_VGC_PAL_SCAN_TIME_NS)

#define CLEM_VGC_FIRST_VISIBLE_SCANLINE_CNTR 7

#define CLEM_RTC_BRAM_SIZE 256

#define CLEM_DEBUG_LOG_BUFFER_SIZE      256
#define CLEM_DEBUG_BREAK_UNIMPL_IOREAD  1U
#define CLEM_DEBUG_BREAK_UNIMPL_IOWRITE 2U

#define CLEM_DEBUG_LOG_DEBUG  0
#define CLEM_DEBUG_LOG_INFO   1
#define CLEM_DEBUG_LOG_WARN   2
#define CLEM_DEBUG_LOG_UNIMPL 3
#define CLEM_DEBUG_LOG_FATAL  4

#define CLEM_DEBUG_TOOLBOX_MMGR 1

/** Setting: ADB keyboard buffer size - this doesn't need to be large since
 *  Apple II apps typically expect to consume events via ISR or prompt polling
 *  of IO registers.  Our host should have the opportunity to send input to the
 *  emulator at a decent frequency (30-60hz) for us not to lose events
 */
#define CLEM_ADB_KEYB_BUFFER_LIMIT     8
#define CLEM_ADB_KEYB_TOGGLE_CAPS_LOCK 0x0000001

/** Gameport support - note that paddle axis values range from 0 to 1023, and
 *  there's support for up to 8 buttons.  Of course the Apple 2 only supports
 *  two buttons (possibly more with extended gameport support).  A host can
 *  supply states for up to 8 buttons, and the emulator can tread buttons 0, 2,
 *  4, and so on as 'button 0' and 1, 3, 5, ... as 'button 1' as an option
 */
#define CLEM_GAMEPORT_BUTTON_MASK_JOYSTICK_0 0x0000000
#define CLEM_GAMEPORT_BUTTON_MASK_JOYSTICK_1 0x8000000
#define CLEM_GAMEPORT_BUTTON_MASK_BUTTONS    0x00000ff
#define CLEM_GAMEPORT_PADDLE_AXIS_VALUE_MIN  0
#define CLEM_GAMEPORT_PADDLE_AXIS_VALUE_MAX  1023

/** General Machine Settings */
#define CLEM_IIGS_PAGE_SIZE               256
#define CLEM_IIGS_BANK_SIZE               (64 * 1024)
#define CLEM_IIGS_ROM3_SIZE               (CLEM_IIGS_BANK_SIZE * 4)
#define CLEM_IIGS_EXPANSION_ROM_SIZE      2048
#define CLEM_IIGS_FPI_MAIN_RAM_BANK_COUNT 16
#define CLEM_IIGS_EMPTY_RAM_BANK          0x81

/** Vector addresses */
#define CLEM_6502_COP_VECTOR_LO_ADDR    (0xFFF4U)
#define CLEM_6502_COP_VECTOR_HI_ADDR    (0xFFF5U)
#define CLEM_6502_NMI_VECTOR_LO_ADDR    (0xFFFAU)
#define CLEM_6502_NMI_VECTOR_HI_ADDR    (0xFFFBU)
#define CLEM_6502_RESET_VECTOR_LO_ADDR  (0xFFFCU)
#define CLEM_6502_RESET_VECTOR_HI_ADDR  (0xFFFDU)
#define CLEM_6502_IRQBRK_VECTOR_LO_ADDR (0xFFFEU)
#define CLEM_6502_IRQBRK_VECTOR_HI_ADDR (0xFFFFU)

#define CLEM_65816_COP_VECTOR_LO_ADDR  (0xFFE4U)
#define CLEM_65816_COP_VECTOR_HI_ADDR  (0xFFE5U)
#define CLEM_65816_NMI_VECTOR_LO_ADDR  (0xFFEAU)
#define CLEM_65816_NMI_VECTOR_HI_ADDR  (0xFFEBU)
#define CLEM_65816_BRK_VECTOR_LO_ADDR  (0xFFE6U)
#define CLEM_65816_BRK_VECTOR_HI_ADDR  (0xFFE7U)
#define CLEM_65816_IRQB_VECTOR_LO_ADDR (0xFFEEU)
#define CLEM_65816_IRQB_VECTOR_HI_ADDR (0xFFEFU)

/** IRQ line masks (0xfff000000 are reserved for slot IRQs + NMIs*/
#define CLEM_IRQ_VGC_SCAN_LINE  (0x00000001)
#define CLEM_IRQ_VGC_BLANK      (0x00000002)
#define CLEM_IRQ_VGC_MASK       (0x0000000f)
#define CLEM_IRQ_TIMER_QSEC     (0x00000010)
#define CLEM_IRQ_TIMER_RTC_1SEC (0x00000020)
#define CLEM_IRQ_TIMER_MASK     (0x000000f0)
#define CLEM_IRQ_ADB_KEYB_SRQ   (0x00000100)
#define CLEM_IRQ_ADB_MOUSE_SRQ  (0x00000200) /* IIgs Unsupported */
#define CLEM_IRQ_ADB_MOUSE_EVT  (0x00000400)
#define CLEM_IRQ_ADB_DATA       (0x00000800)
#define CLEM_IRQ_ADB_MASK       (0x00000f00)
#define CLEM_IRQ_AUDIO_OSC      (0x00001000)
#define CLEM_IRQ_SLOT_1         (0x00100000)
#define CLEM_IRQ_SLOT_2         (0x00200000)
#define CLEM_IRQ_SLOT_3         (0x00400000)
#define CLEM_IRQ_SLOT_4         (0x00800000)
#define CLEM_IRQ_SLOT_5         (0x01000000)
#define CLEM_IRQ_SLOT_6         (0x02000000)
#define CLEM_IRQ_SLOT_7         (0x04000000)
//#define CLEM_CARD_NMI (0x40000000)
//#define CLEM_CARD_IRQ (0x80000000)

/** NMI line masks for card slot triggers */
#define CLEM_NMI_CARD_MASK (0x000000ff)

/** Opcodes! */
#define CLEM_OPC_ADC_IMM                    (0x69)
#define CLEM_OPC_ADC_ABS                    (0x6D)
#define CLEM_OPC_ADC_ABSL                   (0x6F)
#define CLEM_OPC_BRK                        (0x00)
#define CLEM_OPC_ADC_DP                     (0x65)
#define CLEM_OPC_ADC_DP_INDIRECT            (0x72)
#define CLEM_OPC_ADC_DP_INDIRECTL           (0x67)
#define CLEM_OPC_ADC_ABS_IDX                (0x7D)
#define CLEM_OPC_ADC_ABSL_IDX               (0x7F)
#define CLEM_OPC_ADC_ABS_IDY                (0x79)
#define CLEM_OPC_ADC_DP_IDX                 (0x75)
#define CLEM_OPC_ADC_DP_IDX_INDIRECT        (0x61)
#define CLEM_OPC_ADC_DP_INDIRECT_IDY        (0x71)
#define CLEM_OPC_ADC_DP_INDIRECTL_IDY       (0x77)
#define CLEM_OPC_ADC_STACK_REL              (0x63)
#define CLEM_OPC_ADC_STACK_REL_INDIRECT_IDY (0x73)
#define CLEM_OPC_AND_IMM                    (0x29)
#define CLEM_OPC_AND_ABS                    (0x2D)
#define CLEM_OPC_AND_ABSL                   (0x2F)
#define CLEM_OPC_AND_DP                     (0x25)
#define CLEM_OPC_AND_DP_INDIRECT            (0x32)
#define CLEM_OPC_AND_DP_INDIRECTL           (0x27)
#define CLEM_OPC_AND_ABS_IDX                (0x3D)
#define CLEM_OPC_AND_ABSL_IDX               (0x3F)
#define CLEM_OPC_AND_ABS_IDY                (0x39)
#define CLEM_OPC_AND_DP_IDX                 (0x35)
#define CLEM_OPC_AND_DP_IDX_INDIRECT        (0x21)
#define CLEM_OPC_AND_DP_INDIRECT_IDY        (0x31)
#define CLEM_OPC_AND_DP_INDIRECTL_IDY       (0x37)
#define CLEM_OPC_AND_STACK_REL              (0x23)
#define CLEM_OPC_AND_STACK_REL_INDIRECT_IDY (0x33)
#define CLEM_OPC_ASL_A                      (0x0A)
#define CLEM_OPC_ASL_ABS                    (0x0E)
#define CLEM_OPC_ASL_DP                     (0x06)
#define CLEM_OPC_ASL_ABS_IDX                (0x1E)
#define CLEM_OPC_ASL_ABS_DP_IDX             (0x16)
#define CLEM_OPC_BCC                        (0x90)
#define CLEM_OPC_BCS                        (0xB0)
#define CLEM_OPC_BEQ                        (0xF0)
#define CLEM_OPC_BIT_IMM                    (0x89)
#define CLEM_OPC_BIT_ABS                    (0x2C)
#define CLEM_OPC_BIT_DP                     (0x24)
#define CLEM_OPC_BIT_ABS_IDX                (0x3C)
#define CLEM_OPC_BIT_DP_IDX                 (0x34)
#define CLEM_OPC_BMI                        (0x30)
#define CLEM_OPC_BNE                        (0xD0)
#define CLEM_OPC_BPL                        (0x10)
#define CLEM_OPC_BRA                        (0x80)
#define CLEM_OPC_BRL                        (0x82)
#define CLEM_OPC_BVC                        (0x50)
#define CLEM_OPC_BVS                        (0x70)
#define CLEM_OPC_CLC                        (0x18)
#define CLEM_OPC_CLD                        (0xD8)
#define CLEM_OPC_CLI                        (0x58)
#define CLEM_OPC_CLV                        (0xB8)
#define CLEM_OPC_CMP_IMM                    (0xC9)
#define CLEM_OPC_CMP_ABS                    (0xCD)
#define CLEM_OPC_CMP_ABSL                   (0xCF)
#define CLEM_OPC_CMP_DP                     (0xC5)
#define CLEM_OPC_CMP_DP_INDIRECT            (0xD2)
#define CLEM_OPC_CMP_DP_INDIRECTL           (0xC7)
#define CLEM_OPC_CMP_ABS_IDX                (0xDD)
#define CLEM_OPC_CMP_ABSL_IDX               (0xDF)
#define CLEM_OPC_CMP_ABS_IDY                (0xD9)
#define CLEM_OPC_CMP_DP_IDX                 (0xD5)
#define CLEM_OPC_CMP_DP_IDX_INDIRECT        (0xC1)
#define CLEM_OPC_CMP_DP_INDIRECT_IDY        (0xD1)
#define CLEM_OPC_CMP_DP_INDIRECTL_IDY       (0xD7)
#define CLEM_OPC_CMP_STACK_REL              (0xC3)
#define CLEM_OPC_CMP_STACK_REL_INDIRECT_IDY (0xD3)
#define CLEM_OPC_COP                        (0x02)
#define CLEM_OPC_CPX_IMM                    (0xE0)
#define CLEM_OPC_CPX_ABS                    (0xEC)
#define CLEM_OPC_CPX_DP                     (0xE4)
#define CLEM_OPC_CPY_IMM                    (0xC0)
#define CLEM_OPC_CPY_ABS                    (0xCC)
#define CLEM_OPC_CPY_DP                     (0xC4)
#define CLEM_OPC_DEC_A                      (0x3A)
#define CLEM_OPC_DEC_ABS                    (0xCE)
#define CLEM_OPC_DEC_DP                     (0xC6)
#define CLEM_OPC_DEC_ABS_IDX                (0xDE)
#define CLEM_OPC_DEC_ABS_DP_IDX             (0xD6)
#define CLEM_OPC_DEX                        (0xCA)
#define CLEM_OPC_DEY                        (0x88)
#define CLEM_OPC_EOR_IMM                    (0x49)
#define CLEM_OPC_EOR_ABS                    (0x4D)
#define CLEM_OPC_EOR_ABSL                   (0x4F)
#define CLEM_OPC_EOR_DP                     (0x45)
#define CLEM_OPC_EOR_DP_INDIRECT            (0x52)
#define CLEM_OPC_EOR_DP_INDIRECTL           (0x47)
#define CLEM_OPC_EOR_ABS_IDX                (0x5D)
#define CLEM_OPC_EOR_ABSL_IDX               (0x5F)
#define CLEM_OPC_EOR_ABS_IDY                (0x59)
#define CLEM_OPC_EOR_DP_IDX                 (0x55)
#define CLEM_OPC_EOR_DP_IDX_INDIRECT        (0x41)
#define CLEM_OPC_EOR_DP_INDIRECT_IDY        (0x51)
#define CLEM_OPC_EOR_DP_INDIRECTL_IDY       (0x57)
#define CLEM_OPC_EOR_STACK_REL              (0x43)
#define CLEM_OPC_EOR_STACK_REL_INDIRECT_IDY (0x53)
#define CLEM_OPC_INC_A                      (0x1A)
#define CLEM_OPC_INC_ABS                    (0xEE)
#define CLEM_OPC_INC_DP                     (0xE6)
#define CLEM_OPC_INC_ABS_IDX                (0xFE)
#define CLEM_OPC_INC_ABS_DP_IDX             (0xF6)
#define CLEM_OPC_INX                        (0xE8)
#define CLEM_OPC_INY                        (0xC8)
#define CLEM_OPC_JMP_ABS                    (0x4C)
#define CLEM_OPC_JMP_INDIRECT               (0x6C)
#define CLEM_OPC_JMP_INDIRECT_IDX           (0x7C)
#define CLEM_OPC_JMP_ABSL                   (0x5C)
#define CLEM_OPC_JMP_ABSL_INDIRECT          (0xDC)
#define CLEM_OPC_JSL                        (0x22)
#define CLEM_OPC_JSR                        (0x20)
#define CLEM_OPC_JSR_INDIRECT_IDX           (0xFC)
#define CLEM_OPC_LDA_IMM                    (0xA9)
#define CLEM_OPC_LDA_ABS                    (0xAD)
#define CLEM_OPC_LDA_ABSL                   (0xAF)
#define CLEM_OPC_LDA_DP                     (0xA5)
#define CLEM_OPC_LDA_DP_INDIRECT            (0xB2)
#define CLEM_OPC_LDA_DP_INDIRECTL           (0xA7)
#define CLEM_OPC_LDA_ABS_IDX                (0xBD)
#define CLEM_OPC_LDA_ABSL_IDX               (0xBF)
#define CLEM_OPC_LDA_ABS_IDY                (0xB9)
#define CLEM_OPC_LDA_DP_IDX                 (0xB5)
#define CLEM_OPC_LDA_DP_IDX_INDIRECT        (0xA1)
#define CLEM_OPC_LDA_DP_INDIRECT_IDY        (0xB1)
#define CLEM_OPC_LDA_DP_INDIRECTL_IDY       (0xB7)
#define CLEM_OPC_LDA_STACK_REL              (0xA3)
#define CLEM_OPC_LDA_STACK_REL_INDIRECT_IDY (0xB3)
#define CLEM_OPC_LDX_IMM                    (0xA2)
#define CLEM_OPC_LDX_ABS                    (0xAE)
#define CLEM_OPC_LDX_DP                     (0xA6)
#define CLEM_OPC_LDX_ABS_IDY                (0xBE)
#define CLEM_OPC_LDX_DP_IDY                 (0xB6)
#define CLEM_OPC_LDY_IMM                    (0xA0)
#define CLEM_OPC_LDY_ABS                    (0xAC)
#define CLEM_OPC_LDY_DP                     (0xA4)
#define CLEM_OPC_LDY_ABS_IDX                (0xBC)
#define CLEM_OPC_LDY_DP_IDX                 (0xB4)
#define CLEM_OPC_LSR_A                      (0x4A)
#define CLEM_OPC_LSR_ABS                    (0x4E)
#define CLEM_OPC_LSR_DP                     (0x46)
#define CLEM_OPC_LSR_ABS_IDX                (0x5E)
#define CLEM_OPC_LSR_ABS_DP_IDX             (0x56)
#define CLEM_OPC_MVN                        (0x54)
#define CLEM_OPC_MVP                        (0x44)
#define CLEM_OPC_NOP                        (0xEA)
#define CLEM_OPC_ORA_IMM                    (0x09)
#define CLEM_OPC_ORA_ABS                    (0x0D)
#define CLEM_OPC_ORA_ABSL                   (0x0F)
#define CLEM_OPC_ORA_DP                     (0x05)
#define CLEM_OPC_ORA_DP_INDIRECT            (0x12)
#define CLEM_OPC_ORA_DP_INDIRECTL           (0x07)
#define CLEM_OPC_ORA_ABS_IDX                (0x1D)
#define CLEM_OPC_ORA_ABSL_IDX               (0x1F)
#define CLEM_OPC_ORA_ABS_IDY                (0x19)
#define CLEM_OPC_ORA_DP_IDX                 (0x15)
#define CLEM_OPC_ORA_DP_IDX_INDIRECT        (0x01)
#define CLEM_OPC_ORA_DP_INDIRECT_IDY        (0x11)
#define CLEM_OPC_ORA_DP_INDIRECTL_IDY       (0x17)
#define CLEM_OPC_ORA_STACK_REL              (0x03)
#define CLEM_OPC_ORA_STACK_REL_INDIRECT_IDY (0x13)
#define CLEM_OPC_PEA_ABS                    (0xF4)
#define CLEM_OPC_PEI_DP_INDIRECT            (0xD4)
#define CLEM_OPC_PER                        (0x62)
#define CLEM_OPC_PHA                        (0x48)
#define CLEM_OPC_PHB                        (0x8B)
#define CLEM_OPC_PHD                        (0x0B)
#define CLEM_OPC_PHK                        (0x4B)
#define CLEM_OPC_PHP                        (0x08)
#define CLEM_OPC_PHX                        (0xDA)
#define CLEM_OPC_PHY                        (0x5A)
#define CLEM_OPC_PLA                        (0x68)
#define CLEM_OPC_PLB                        (0xAB)
#define CLEM_OPC_PLD                        (0x2B)
#define CLEM_OPC_PLP                        (0x28)
#define CLEM_OPC_PLX                        (0xFA)
#define CLEM_OPC_PLY                        (0x7A)
#define CLEM_OPC_REP                        (0xC2)
#define CLEM_OPC_ROL_A                      (0x2A)
#define CLEM_OPC_ROL_ABS                    (0x2E)
#define CLEM_OPC_ROL_DP                     (0x26)
#define CLEM_OPC_ROL_ABS_IDX                (0x3E)
#define CLEM_OPC_ROL_ABS_DP_IDX             (0x36)
#define CLEM_OPC_ROR_A                      (0x6A)
#define CLEM_OPC_ROR_ABS                    (0x6E)
#define CLEM_OPC_ROR_DP                     (0x66)
#define CLEM_OPC_ROR_ABS_IDX                (0x7E)
#define CLEM_OPC_ROR_ABS_DP_IDX             (0x76)
#define CLEM_OPC_RTI                        (0x40)
#define CLEM_OPC_RTL                        (0x6B)
#define CLEM_OPC_RTS                        (0x60)
#define CLEM_OPC_SBC_IMM                    (0xE9)
#define CLEM_OPC_SBC_ABS                    (0xED)
#define CLEM_OPC_SBC_ABSL                   (0xEF)
#define CLEM_OPC_SBC_DP                     (0xE5)
#define CLEM_OPC_SBC_DP_INDIRECT            (0xF2)
#define CLEM_OPC_SBC_DP_INDIRECTL           (0xE7)
#define CLEM_OPC_SBC_ABS_IDX                (0xFD)
#define CLEM_OPC_SBC_ABSL_IDX               (0xFF)
#define CLEM_OPC_SBC_ABS_IDY                (0xF9)
#define CLEM_OPC_SBC_DP_IDX                 (0xF5)
#define CLEM_OPC_SBC_DP_IDX_INDIRECT        (0xE1)
#define CLEM_OPC_SBC_DP_INDIRECT_IDY        (0xF1)
#define CLEM_OPC_SBC_DP_INDIRECTL_IDY       (0xF7)
#define CLEM_OPC_SBC_STACK_REL              (0xE3)
#define CLEM_OPC_SBC_STACK_REL_INDIRECT_IDY (0xF3)
#define CLEM_OPC_SEC                        (0x38)
#define CLEM_OPC_SED                        (0xF8)
#define CLEM_OPC_SEI                        (0x78)
#define CLEM_OPC_SEP                        (0xE2)
#define CLEM_OPC_STA_ABS                    (0x8D)
#define CLEM_OPC_STA_ABSL                   (0x8F)
#define CLEM_OPC_STA_DP                     (0x85)
#define CLEM_OPC_STA_DP_INDIRECT            (0x92)
#define CLEM_OPC_STA_DP_INDIRECTL           (0x87)
#define CLEM_OPC_STA_ABS_IDX                (0x9D)
#define CLEM_OPC_STA_ABSL_IDX               (0x9F)
#define CLEM_OPC_STA_ABS_IDY                (0x99)
#define CLEM_OPC_STA_DP_IDX                 (0x95)
#define CLEM_OPC_STA_DP_IDX_INDIRECT        (0x81)
#define CLEM_OPC_STA_DP_INDIRECT_IDY        (0x91)
#define CLEM_OPC_STA_DP_INDIRECTL_IDY       (0x97)
#define CLEM_OPC_STA_STACK_REL              (0x83)
#define CLEM_OPC_STA_STACK_REL_INDIRECT_IDY (0x93)
#define CLEM_OPC_STP                        (0xDB)
#define CLEM_OPC_STX_ABS                    (0x8E)
#define CLEM_OPC_STX_DP                     (0x86)
#define CLEM_OPC_STX_DP_IDY                 (0x96)
#define CLEM_OPC_STY_ABS                    (0x8C)
#define CLEM_OPC_STY_DP                     (0x84)
#define CLEM_OPC_STY_DP_IDX                 (0x94)
#define CLEM_OPC_STZ_ABS                    (0x9C)
#define CLEM_OPC_STZ_DP                     (0x64)
#define CLEM_OPC_STZ_ABS_IDX                (0x9E)
#define CLEM_OPC_STZ_DP_IDX                 (0x74)
#define CLEM_OPC_TAX                        (0xAA)
#define CLEM_OPC_TAY                        (0xA8)
#define CLEM_OPC_TCS                        (0x1B)
#define CLEM_OPC_TCD                        (0x5B)
#define CLEM_OPC_TDC                        (0x7B)
#define CLEM_OPC_TRB_ABS                    (0x1C)
#define CLEM_OPC_TRB_DP                     (0x14)
#define CLEM_OPC_TSB_ABS                    (0x0C)
#define CLEM_OPC_TSB_DP                     (0x04)
#define CLEM_OPC_TSC                        (0x3B)
#define CLEM_OPC_TSX                        (0xBA)
#define CLEM_OPC_TXA                        (0x8A)
#define CLEM_OPC_TXS                        (0x9A)
#define CLEM_OPC_TXY                        (0x9B)
#define CLEM_OPC_TYA                        (0x98)
#define CLEM_OPC_TYX                        (0xBB)
#define CLEM_OPC_WAI                        (0xCB)
#define CLEM_OPC_WDM                        (0x42)
#define CLEM_OPC_XBA                        (0xEB)
#define CLEM_OPC_XCE                        (0xFB)

#define CLEM_UTIL_set16_lo(_v16_, _v8_) (((_v16_)&0xff00) | ((_v8_)&0xff))

#define CLEM_UTIL_CROSSED_PAGE_BOUNDARY(_adr0_, _adr1_) (((_adr0_) ^ (_adr1_)) & 0xff00)

/* Key codes */
#define CLEM_ADB_KEY_A 0x00
#define CLEM_ADB_KEY_S 0x01
#define CLEM_ADB_KEY_D 0x02
#define CLEM_ADB_KEY_F 0x03
#define CLEM_ADB_KEY_H 0x04
#define CLEM_ADB_KEY_G 0x05
#define CLEM_ADB_KEY_Z 0x06
#define CLEM_ADB_KEY_X 0x07
#define CLEM_ADB_KEY_C 0x08
#define CLEM_ADB_KEY_V 0x09
/* Skipped 0x0A */
#define CLEM_ADB_KEY_B          0x0B
#define CLEM_ADB_KEY_Q          0x0C
#define CLEM_ADB_KEY_W          0x0D
#define CLEM_ADB_KEY_E          0x0E
#define CLEM_ADB_KEY_R          0x0F
#define CLEM_ADB_KEY_T          0x10
#define CLEM_ADB_KEY_Y          0x11
#define CLEM_ADB_KEY_1          0x12
#define CLEM_ADB_KEY_2          0x13
#define CLEM_ADB_KEY_3          0x14
#define CLEM_ADB_KEY_4          0x15
#define CLEM_ADB_KEY_6          0x16
#define CLEM_ADB_KEY_5          0x17
#define CLEM_ADB_KEY_EQUALS     0x18
#define CLEM_ADB_KEY_9          0x19
#define CLEM_ADB_KEY_7          0x1A
#define CLEM_ADB_KEY_MINUS      0x1B
#define CLEM_ADB_KEY_8          0x1C
#define CLEM_ADB_KEY_0          0x1D
#define CLEM_ADB_KEY_RBRACKET   0x1E
#define CLEM_ADB_KEY_O          0x1F
#define CLEM_ADB_KEY_U          0x20
#define CLEM_ADB_KEY_LBRACKET   0x21
#define CLEM_ADB_KEY_I          0x22
#define CLEM_ADB_KEY_P          0x23
#define CLEM_ADB_KEY_RETURN     0x24
#define CLEM_ADB_KEY_L          0x25
#define CLEM_ADB_KEY_J          0x26
#define CLEM_ADB_KEY_APOSTRAPHE 0x27
#define CLEM_ADB_KEY_K          0x28
#define CLEM_ADB_KEY_SEMICOLON  0x29
#define CLEM_ADB_KEY_BACKSLASH  0x2A
#define CLEM_ADB_KEY_COMMA      0x2B
#define CLEM_ADB_KEY_FWDSLASH   0x2C
#define CLEM_ADB_KEY_N          0x2D
#define CLEM_ADB_KEY_M          0x2E
#define CLEM_ADB_KEY_PERIOD     0x2F
#define CLEM_ADB_KEY_TAB        0x30
#define CLEM_ADB_KEY_SPACE      0x31
#define CLEM_ADB_KEY_BACKQUOTE  0x32
#define CLEM_ADB_KEY_DELETE     0x33
/* Skipped 0x34 */
#define CLEM_ADB_KEY_ESCAPE        0x35
#define CLEM_ADB_KEY_LCTRL         0x36
#define CLEM_ADB_KEY_COMMAND_APPLE 0x37
#define CLEM_ADB_KEY_LSHIFT        0x38
#define CLEM_ADB_KEY_CAPSLOCK      0x39
#define CLEM_ADB_KEY_OPTION        0x3A
#define CLEM_ADB_KEY_LEFT          0x3B
#define CLEM_ADB_KEY_RIGHT         0x3C
#define CLEM_ADB_KEY_DOWN          0x3D
#define CLEM_ADB_KEY_UP            0x3E
/* Skipped 0x3F */
/* Skipped 0x40 */
#define CLEM_ADB_KEY_PAD_DECIMAL 0x41
/* Skipped 0x42 */
#define CLEM_ADB_KEY_PAD_MULTIPLY 0x43
/* Skipped 0x44 */
#define CLEM_ADB_KEY_PAD_PLUS 0x45
/* Skipped 0x46 */
#define CLEM_ADB_KEY_PAD_CLEAR_NUMLOCK 0x47
/* Skipped 0x48 */
/* Skipped 0x49 */
/* Skipped 0x4A */
#define CLEM_ADB_KEY_PAD_DIVIDE 0x4B
#define CLEM_ADB_KEY_PAD_ENTER  0x4C
/* Skipped 0x4D */
#define CLEM_ADB_KEY_PAD_MINUS 0x4E
/* Skipped 0x4F */
/* Skipped 0x50 */
#define CLEM_ADB_KEY_PAD_EQUALS 0x51
#define CLEM_ADB_KEY_PAD_0      0x52
#define CLEM_ADB_KEY_PAD_1      0x53
#define CLEM_ADB_KEY_PAD_2      0x54
#define CLEM_ADB_KEY_PAD_3      0x55
#define CLEM_ADB_KEY_PAD_4      0x56
#define CLEM_ADB_KEY_PAD_5      0x57
#define CLEM_ADB_KEY_PAD_6      0x58
#define CLEM_ADB_KEY_PAD_7      0x59
/* Skipped 0x5A */
#define CLEM_ADB_KEY_PAD_8 0x5B
#define CLEM_ADB_KEY_PAD_9 0x5C
/* Skipped 0x5D */
/* Skipped 0x5E */
/* Skipped 0x5F */
#define CLEM_ADB_KEY_F5 0x60
#define CLEM_ADB_KEY_F6 0x61
#define CLEM_ADB_KEY_F7 0x62
#define CLEM_ADB_KEY_F3 0x63
#define CLEM_ADB_KEY_F8 0x64
#define CLEM_ADB_KEY_F9 0x65
/* Skipped 0x66 */
#define CLEM_ADB_KEY_F11 0x67
/* Skipped 0x68 */
#define CLEM_ADB_KEY_F13 0x69
/* Skipped 0x6A */
#define CLEM_ADB_KEY_F14 0x6B
/* Skipped 0x6C */
#define CLEM_ADB_KEY_F10 0x6D
/* Skipped 0x6E */
#define CLEM_ADB_KEY_F12 0x6F
/* Skipped 0x70 */
#define CLEM_ADB_KEY_F15         0x71
#define CLEM_ADB_KEY_HELP_INSERT 0x72
#define CLEM_ADB_KEY_HOME        0x73
#define CLEM_ADB_KEY_PAGEUP      0x74
#define CLEM_ADB_KEY_PAD_DELETE  0x75
#define CLEM_ADB_KEY_F4          0x76
#define CLEM_ADB_KEY_END         0x77
#define CLEM_ADB_KEY_F2          0x78
#define CLEM_ADB_KEY_PAGEDOWN    0x79
#define CLEM_ADB_KEY_F1          0x7A
#define CLEM_ADB_KEY_RSHIFT      0x7B
#define CLEM_ADB_KEY_ROPTION     0x7C
#define CLEM_ADB_KEY_RCTRL       0x7D
/* Skipped 0x7E */
#define CLEM_ADB_KEY_RESET 0x7F

#define CLEM_ADB_KEY_CODE_LIMIT 0x80

/*  Enable 3.5 drive seris */
#define CLEM_IWM_FLAG_DRIVE_35 0x00000001
/*  Drive system is active - in tandem with drive index selected */
#define CLEM_IWM_FLAG_DRIVE_ON 0x00000002
/*  Drive 1 selected - note IWM only allows one drive at a time, but the
    disk port has two pins for drive, so emulating that aspect */
#define CLEM_IWM_FLAG_DRIVE_1 0x00000004
/*  Drive 2 selected */
#define CLEM_IWM_FLAG_DRIVE_2 0x00000008
/*  Congolmerate mask for any-drive selected */
#define CLEM_IWM_FLAG_DRIVE_ANY (CLEM_IWM_FLAG_DRIVE_1 + CLEM_IWM_FLAG_DRIVE_2)
/* device flag, 3.5" side 2 (not used for 5.25")
   note, this really is used for 3.5" drive controller actions:
   https://llx.com/Neil/a2/disk
*/
#define CLEM_IWM_FLAG_HEAD_SEL 0x00000010
/* Places drive in 'write' mode */
#define CLEM_IWM_FLAG_WRITE_REQUEST 0x00000040
/*  Write protect for disk for 5.25", and the sense input bit for 3.5" drives*/
#define CLEM_IWM_FLAG_WRPROTECT_SENSE 0x00000080
/*  Read pulse from the disk/drive bitstream is on */
#define CLEM_IWM_FLAG_READ_DATA 0x00000100
/*  Write pulse input to drive */
#define CLEM_IWM_FLAG_WRITE_DATA 0x00000200
/*  For debugging only */
#define CLEM_IWM_FLAG_READ_DATA_FAKE 0x00004000
#define CLEM_IWM_FLAG_PULSE_HIGH     0x00008000

#define CLEM_VGC_TEXT_SCANLINE_COUNT 24
#define CLEM_VGC_HGR_SCANLINE_COUNT  192
#define CLEM_VGC_SHGR_SCANLINE_COUNT 200

/* Text colors */
#define CLEM_VGC_COLOR_BLACK       0x00
#define CLEM_VGC_COLOR_DEEP_RED    0x01
#define CLEM_VGC_COLOR_DARK_BLUE   0x02
#define CLEM_VGC_COLOR_PURPLE      0x03
#define CLEM_VGC_COLOR_DARK_GREEN  0x04
#define CLEM_VGC_COLOR_DARK_GRAY   0x05
#define CLEM_VGC_COLOR_MEDIUM_BLUE 0x06
#define CLEM_VGC_COLOR_LIGHT_BLUE  0x07
#define CLEM_VGC_COLOR_BROWN       0x08
#define CLEM_VGC_COLOR_ORANGE      0x09
#define CLEM_VGC_COLOR_LIGHT_GRAY  0x0A
#define CLEM_VGC_COLOR_PINK        0x0B
#define CLEM_VGC_COLOR_GREEN       0x0C
#define CLEM_VGC_COLOR_YELLOW      0x0D
#define CLEM_VGC_COLOR_AQUAMARINE  0x0E
#define CLEM_VGC_COLOR_WHITE       0x0F

#define CLEM_VGC_GRAPHICS_MODE   0x00000001
#define CLEM_VGC_MIXED_TEXT      0x00000002
#define CLEM_VGC_80COLUMN_TEXT   0x00000004
#define CLEM_VGC_LORES           0x00000010
#define CLEM_VGC_HIRES           0x00000020
#define CLEM_VGC_RESOLUTION_MASK 0x000000F0
#define CLEM_VGC_SUPER_HIRES     0x00000100
#define CLEM_VGC_ALTCHARSET      0x00010000
#define CLEM_VGC_MONOCHROME      0x00020000
#define CLEM_VGC_PAL             0x00040000
#define CLEM_VGC_LANGUAGE        0x00080000
#define CLEM_VGC_ENABLE_VBL_IRQ  0x00100000
#define CLEM_VGC_DISABLE_AN3     0x00200000
#define CLEM_VGC_DBLRES_MASK     0x00200005
#define CLEM_VGC_INIT            0x80000000

#define CLEM_VGC_SCANLINE_CONTROL_640_MODE   (0x80)
#define CLEM_VGC_SCANLINE_CONTROL_INTERRUPT  (0x40)
#define CLEM_VGC_SCANLINE_COLORFILL_MODE     (0x20)
#define CLEM_VGC_SCANLINE_PALETTE_INDEX_MASK (0x0f)

#define CLEM_SCC_PORT_DTR        0x01
#define CLEM_SCC_PORT_HSKI       0x02
#define CLEM_SCC_PORT_TX_DATA_LO 0x04
#define CLEM_SCC_PORT_TX_DATA_HI 0x08
#define CLEM_SCC_PORT_RX_DATA_LO 0x10
#define CLEM_SCC_PORT_RX_DATA_HI 0x20
#define CLEM_SCC_PORT_GPI        0x40

#define CLEM_ENSONIQ_OSC_CTL_FREE_MODE 0x00
#define CLEM_ENSONIQ_OSC_CTL_M0        0x02
#define CLEM_ENSONIQ_OSC_CTL_SYNC      0x04
#define CLEM_ENSONIQ_OSC_CTL_SWAP      0x06
#define CLEM_ENSONIQ_OSC_CTL_HALT      0x01
#define CLEM_ENSONIQ_OSC_CTL_IE        0x08

#define CLEM_ENSONIQ_REG_OSC_FCLOW  0x00
#define CLEM_ENSONIQ_REG_OSC_FCHI   0x20
#define CLEM_ENSONIQ_REG_OSC_VOLUME 0x40
#define CLEM_ENSONIQ_REG_OSC_DATA   0x60
#define CLEM_ENSONIQ_REG_OSC_PTR    0x80
#define CLEM_ENSONIQ_REG_OSC_CTRL   0xa0
#define CLEM_ENSONIQ_REG_OSC_SIZE   0xc0
#define CLEM_ENSONIQ_REG_OSC_OIR    0xe0
#define CLEM_ENSONIQ_REG_OSC_ENABLE 0xe1
#define CLEM_ENSONIQ_REG_OSC_ADC    0xe2

//  see doc->osc_flags
#define CLEM_ENSONIQ_OSC_FLAG_IRQ 0x01

#define CLEM_PI      (3.14159265f)
#define CLEM_PI_2    (6.28318531f)
#define CLEM_HALF_PI (1.570796325)

/* enable/disable certain compile time diagnostics */

#define CLEM_AUDIO_DIAGNOSTICS 0

//  support read or write operations
#define CLEM_MEM_PAGE_WRITEOK_FLAG 0x00000001
//  use the original bank register
#define CLEM_MEM_PAGE_DIRECT_FLAG 0x10000000
//  use a mask of the requested bank and the 17th address bit of the read/write
//  bank
#define CLEM_MEM_PAGE_MAINAUX_FLAG 0x20000000
//  use card memory vs internal
#define CLEM_MEM_PAGE_CARDMEM_FLAG 0x40000000
//  redirects to Mega2 I/O registers
#define CLEM_MEM_PAGE_IOADDR_FLAG 0x80000000

//  convenience flags to identify source of memory
#define CLEM_MEM_PAGE_BANK_MASK 0x30000000
#define CLEM_MEM_IO_MEMORY_MASK 0xC0000000
#define CLEM_MEM_PAGE_TYPE_MASK 0xF0000000

#endif
