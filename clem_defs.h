#ifndef CLEMENS_DEFS_H
#define CLEMENS_DEFS_H

/** Setting: Mega2 tick interval for polling the ADB (effectively microseconds)
 */
#define CLEM_MEGA2_CYCLES_PER_MS            1000
#define CLEM_MEGA2_TIMER_1SEC_MS            1000
#define CLEM_MEGA2_TIMER_QSEC_MS            266

/** Setting: ADB keyboard buffer size
 */
#define CLEM_ADB_KEYB_BUFFER_LIMIT          8

/** General Machine Settings */
#define CLEM_IIGS_BANK_SIZE                 (64 * 1024)
#define CLEM_IIGS_ROM3_SIZE                 (CLEM_IIGS_BANK_SIZE * 4)

/** Vector addresses */
#define CLEM_6502_COP_VECTOR_LO_ADDR        (0xFFF4)
#define CLEM_6502_COP_VECTOR_HI_ADDR        (0xFFF5)
#define CLEM_6502_RESET_VECTOR_LO_ADDR      (0xFFFC)
#define CLEM_6502_RESET_VECTOR_HI_ADDR      (0xFFFD)
#define CLEM_6502_IRQBRK_VECTOR_LO_ADDR     (0xFFFE)
#define CLEM_6502_IRQBRK_VECTOR_HI_ADDR     (0xFFFF)

#define CLEM_65816_COP_VECTOR_LO_ADDR       (0xFFE4)
#define CLEM_65816_COP_VECTOR_HI_ADDR       (0xFFE5)
#define CLEM_65816_BRK_VECTOR_LO_ADDR       (0xFFE6)
#define CLEM_65816_BRK_VECTOR_HI_ADDR       (0xFFE7)
#define CLEM_65816_IRQB_VECTOR_LO_ADDR      (0xFFEE)
#define CLEM_65816_IRQB_VECTOR_HI_ADDR      (0xFFEF)

/** IRQ line masks */
#define CLEM_IRQ_VGC_SCAN_LINE              (0x00000010)
#define CLEM_IRQ_TIMER_QSEC                 (0x00001000)
#define CLEM_IRQ_ADB_SRQ                    (0x00010000)
#define CLEM_IRQ_TIMER_RTC_1SEC             (0x00100000)

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


/* Attempt to mimic VDA and VPA per memory access */
#define CLEM_MEM_FLAG_OPCODE_FETCH          (0x3)
#define CLEM_MEM_FLAG_DATA                  (0x2)
#define CLEM_MEM_FLAG_PROGRAM               (0x1)
#define CLEM_MEM_FLAG_NULL                  (0x0)

#define CLEM_UTIL_set16_lo(_v16_, _v8_) \
    (((_v16_) & 0xff00) | ((_v8_) & 0xff))

#define CLEM_UTIL_CROSSED_PAGE_BOUNDARY(_adr0_, _adr1_) \
    (((_adr0_) ^ (_adr1_)) & 0xff00)


/* Key codes */
#define CLEM_ADB_KEY_A                      0x00
#define CLEM_ADB_KEY_S                      0x01
#define CLEM_ADB_KEY_D                      0x02
#define CLEM_ADB_KEY_F                      0x03
#define CLEM_ADB_KEY_H                      0x04
#define CLEM_ADB_KEY_G                      0x05
#define CLEM_ADB_KEY_Z                      0x06
#define CLEM_ADB_KEY_X                      0x07
#define CLEM_ADB_KEY_C                      0x08
#define CLEM_ADB_KEY_V                      0x09
/* Skipped 0x0A */
#define CLEM_ADB_KEY_B                      0x0B
#define CLEM_ADB_KEY_Q                      0x0C
#define CLEM_ADB_KEY_W                      0x0D
#define CLEM_ADB_KEY_E                      0x0E
#define CLEM_ADB_KEY_R                      0x0F
#define CLEM_ADB_KEY_T                      0x10
#define CLEM_ADB_KEY_Y                      0x11
#define CLEM_ADB_KEY_1                      0x12
#define CLEM_ADB_KEY_2                      0x13
#define CLEM_ADB_KEY_3                      0x14
#define CLEM_ADB_KEY_4                      0x15
#define CLEM_ADB_KEY_6                      0x16
#define CLEM_ADB_KEY_5                      0x17
#define CLEM_ADB_KEY_EQUALS                 0x18
#define CLEM_ADB_KEY_9                      0x19
#define CLEM_ADB_KEY_7                      0x1A
#define CLEM_ADB_KEY_MINUS                  0x1B
#define CLEM_ADB_KEY_8                      0x1C
#define CLEM_ADB_KEY_0                      0x1D
#define CLEM_ADB_KEY_RBRACKET               0x1E
#define CLEM_ADB_KEY_O                      0x1F
#define CLEM_ADB_KEY_U                      0x20
#define CLEM_ADB_KEY_LBRACKET               0x21
#define CLEM_ADB_KEY_I                      0x22
#define CLEM_ADB_KEY_P                      0x23
#define CLEM_ADB_KEY_RETURN                 0x24
#define CLEM_ADB_KEY_L                      0x25
#define CLEM_ADB_KEY_J                      0x26
#define CLEM_ADB_KEY_APOSTRAPHE             0x27
#define CLEM_ADB_KEY_K                      0x28
#define CLEM_ADB_KEY_SEMICOLON              0x29
#define CLEM_ADB_KEY_BACKSLASH              0x2A
#define CLEM_ADB_KEY_COMMA                  0x2B
#define CLEM_ADB_KEY_FWDSLASH               0x2C
#define CLEM_ADB_KEY_N                      0x2D
#define CLEM_ADB_KEY_M                      0x2E
#define CLEM_ADB_KEY_PERIOD                 0x2F
#define CLEM_ADB_KEY_TAB                    0x30
#define CLEM_ADB_KEY_SPACE                  0x31
#define CLEM_ADB_KEY_BACKQUOTE              0x32
#define CLEM_ADB_KEY_DELETE                 0x33
/* Skipped 0x34 */
#define CLEM_ADB_KEY_ESCAPE                 0x35
#define CLEM_ADB_KEY_LCTRL                  0x36
#define CLEM_ADB_KEY_COMMAND_APPLE          0x37
#define CLEM_ADB_KEY_LSHIFT                 0x38
#define CLEM_ADB_KEY_CAPSLOCK               0x39
#define CLEM_ADB_KEY_OPTION                 0x3A
#define CLEM_ADB_KEY_LEFT                   0x3B
#define CLEM_ADB_KEY_RIGHT                  0x3C
#define CLEM_ADB_KEY_DOWN                   0x3D
#define CLEM_ADB_KEY_UP                     0x3E
/* Skipped 0x3F */
/* Skipped 0x40 */
#define CLEM_ADB_KEY_PAD_DECIMAL            0x41
/* Skipped 0x42 */
#define CLEM_ADB_KEY_PAD_MULTIPLY           0x43
/* Skipped 0x44 */
#define CLEM_ADB_KEY_PAD_PLUS               0x45
/* Skipped 0x46 */
#define CLEM_ADB_KEY_PAD_CLEAR_NUMLOCK      0x47
/* Skipped 0x48 */
/* Skipped 0x49 */
/* Skipped 0x4A */
#define CLEM_ADB_KEY_PAD_DIVIDE             0x4B
#define CLEM_ADB_KEY_PAD_ENTER              0x4C
/* Skipped 0x4D */
#define CLEM_ADB_KEY_PAD_MINUS              0x4E
/* Skipped 0x4F */
/* Skipped 0x50 */
#define CLEM_ADB_KEY_PAD_EQUALS             0x51
#define CLEM_ADB_KEY_PAD_0                  0x52
#define CLEM_ADB_KEY_PAD_1                  0x53
#define CLEM_ADB_KEY_PAD_2                  0x54
#define CLEM_ADB_KEY_PAD_3                  0x55
#define CLEM_ADB_KEY_PAD_4                  0x56
#define CLEM_ADB_KEY_PAD_5                  0x57
#define CLEM_ADB_KEY_PAD_6                  0x58
#define CLEM_ADB_KEY_PAD_7                  0x59
/* Skipped 0x5A */
#define CLEM_ADB_KEY_PAD_8                  0x5B
#define CLEM_ADB_KEY_PAD_9                  0x5C
/* Skipped 0x5D */
/* Skipped 0x5E */
/* Skipped 0x5F */
#define CLEM_ADB_KEY_F5                     0x60
#define CLEM_ADB_KEY_F6                     0x61
#define CLEM_ADB_KEY_F7                     0x62
#define CLEM_ADB_KEY_F3                     0x63
#define CLEM_ADB_KEY_F8                     0x64
#define CLEM_ADB_KEY_F9                     0x65
/* Skipped 0x66 */
#define CLEM_ADB_KEY_F11                    0x67
/* Skipped 0x68 */
#define CLEM_ADB_KEY_F13                    0x69
/* Skipped 0x6A */
#define CLEM_ADB_KEY_F14                    0x6B
/* Skipped 0x6C */
#define CLEM_ADB_KEY_F10                    0x6D
/* Skipped 0x6E */
#define CLEM_ADB_KEY_F12                    0x6F
/* Skipped 0x70 */
#define CLEM_ADB_KEY_F15                    0x71
#define CLEM_ADB_KEY_HELP_INSERT            0x72
#define CLEM_ADB_KEY_HOME                   0x73
#define CLEM_ADB_KEY_PAGEUP                 0x74
#define CLEM_ADB_KEY_PAD_DELETE             0x75
#define CLEM_ADB_KEY_F4                     0x76
#define CLEM_ADB_KEY_END                    0x77
#define CLEM_ADB_KEY_F2                     0x78
#define CLEM_ADB_KEY_PAGEDOWN               0x79
#define CLEM_ADB_KEY_F1                     0x7A
#define CLEM_ADB_KEY_RSHIFT                 0x7B
#define CLEM_ADB_KEY_ROPTION                0x7C
#define CLEM_ADB_KEY_RCTRL                  0x7D
/* Skipped 0x7E */
#define CLEM_ADB_KEY_RESET                  0x7F


#endif
