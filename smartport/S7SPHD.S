                org   $C700

ACTIVE_PERIP_SLOT equ   $07f8
SCRNHOLE7       equ  $07f8
BASIC           equ  $E000
SLOOP           equ  $FABA          ; Disk controller slot search loop entry

LOC0            equ  $00
LOC1            equ  $01

*******************************************************************************
* Entry point for the Boot cycle of this slot
* Bytes 01, 03, 05, have the appropriate ProDOS signature bytes
* Some of this is 'inspired' by the existing ROM at $C500, but will not support
* SmartPort features on this slot for now...
*
* 'SlotEntry' - boot cycle
* 'ProDOSEntry'- device driver entry point
*           - $42 - command (0 = status, 1 = read, 2 = write, 3 = format)
*           - $43 - unit
*           - $44, $45 - pointer to data buffer (in/out)
*           - $46, $47 - block index
*           (See Table 6.4 Beneath Apple ProDOS)
*******************************************************************************
SlotEntry       equ   *
                lda   #$20
                lda   #$00
                lda   #$03
                cmp   #$03          ; #$00 would be a smartport device
                bcs   :bootEntry
*
* ProDOS entry point will set carry to 0 indicating a ProDOS call based on
* contents of $42, etc.
*
* Boot entry point will set carry to 1 indicating a preset ProDOS call to
* read sector 0 into $800
*
DriverEntry     clc
:bootEntry      phb                 ; restore data bank on exit
                phk
                plb                 ; ensure bank 00 data access
                php                 ; carry flag saved along with pre-call state
                lda   #$c7
                sta   ACTIVE_PERIP_SLOT
                pla                 ; save carry state for boot vs prodos exit
                sta   SCRNHOLE7+7
                ror                 ; check carry
                bcc   :prodosCall
*
* Boot mode if carry flag was set initially - which means initialize the
* ProDOS command for READ into $0800 here
*

*
* Once this emulator specific call finishes, we have two paths to deal with
* If Boot, then process the boot flow
* If ProDOS call, then we exit, restoring processor state, result code, carry
*   indicating error or not.
*
* Contents on stack: DBR, RTS
:prodosCall     sei
                wdm   #$02
                asc   'CLEM'        ; signature for the emulator, otherwise NOP
                pha                 ; result code saved for later
                lda   SCRNHOLE7+7   ; original carry status = 1 for boot flow
                and   #$01
                bne   :bootCheck
                pla                 ; restore prodos device result
                plb
                cli
                rts
*
* Boot validation
*   Carry set, then error reading boot sector
*   If sucessful, check byte at $800 - if 00, then ignore and treat as empty
*                 else jump to $801
*   On failure, bring up error screen
*
:bootCheck      pla                 ; restore prodos device result
                plb
                cli
                bcs   :bootNone
                lda   $0800         ; Boot sector if first byte 0, empty
                beq   :bootNone
                pea   $0000         ; main bank boot
                plb
                plb                 ; restore data bank
                jmp   $0801

:bootNone       lda   LOC1
                cmp   ACTIVE_PERIP_SLOT
                bne   :bootExit
                jmp   SLOOP
:bootExit       jmp   BASIC
                ds    $C7FF-*
                db   :DriverEntry-SLOT_ENTRY
