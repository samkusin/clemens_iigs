                org   $C700

ACTIVE_PERIP_SLOT equ   $07f8
SCRNHOLE7       equ  $07f8
BASIC           equ  $E000
SLOOP           equ  $FABA          ; Disk controller slot search loop entry

ZP_LOC0         equ  $00
ZP_LOC1         equ  $01
ZP_PRODOS_CMD   equ  $42

IO_CONTROL      equ  $C080          ; Write control, Read Handshake
IO_COMMAND      equ  $C081          ; Write command bytes
IO_RESULT0      equ  $C082          ; results for status = X
IO_RESULT1      equ  $C083          ; results for status = Y

***************************************************************
* Entry point for the Boot cycle of this slot
* Bytes 01, 03, 05, have the appropriate ProDOS signature bytes
* Some of this is 'inspired' by the existing ROM at $C500, but 
* will not support SmartPort features on this slot for now...
*
* 'SlotEntry' - boot cycle
* 'ProDOSEntry'- device driver entry point
*           - $42 - command (0 = status, 1 = read, 2 = write, 
*                            3 = format)
*           - $43 - unit
*           - $44, $45 - pointer to data buffer (in/out)
*           - $46, $47 - block index
*           (See Table 6.4 Beneath Apple ProDOS)
***************************************************************
SlotEntry       equ   *
                lda   #$20
                lda   #$00
                lda   #$03
                cmp   #$03          ; #$00 would be a smartport device
                bcs   :bootEntry
*
* ProDOS entry point will set carry to 0 indicating a ProDOS 
* call based on contents of $42, etc.
*
* Boot entry point will set carry to 1 indicating a preset
* ProDOS call to read sector 0 into $800
*
DriverEntry     clc
:bootEntry      phb                 ; restore data bank on exit
                phk
                plb                 ; ensure bank 00 data access
                php                 ; carry flag saved along with pre-call state
                lda   SlotHiAddr
                sta   ACTIVE_PERIP_SLOT
                pla                 ; save carry state for boot vs prodos exit
                sta   SCRNHOLE7+7   ; TODO, offset by ACTIVE_PERIP_SLOT & 0xf0
                ror                 ; check carry
                bcc   :prodosCall
*
* Boot mode if carry flag was set initially - which means initialize the
* ProDOS command for READ into $0800 here
*
                ldx   #5
]next           lda   BootCommand,x
                sta   ZP_PRODOS_CMD,x
                dex
                bpl   ]next
*
* Once this emulator specific call finishes, we have two paths to deal with
* If Boot, then process the boot flow
* If ProDOS call, then we exit, restoring processor state, result code, carry
*   indicating error or not.
*
* Contents on stack: DBR, RTS
* 
* Control and Command protocol
* CTL: write $00 to start a command (this will cancel any current command)
* CTL: read until bit 7 is lo (ready, idle)
* CMD: write command bytes
* CTL: when done, Write $80 to control fire the command
* CTL: Read where bit 7 is hi for being in progress
*                 bit 0 is hi if error
* CMD: Read the error code until CTL is set back $00
:prodosCall     sei
                ldx   IOOffset
                stz   IO_CONTROL,x  ; start the command
]retry          lda   IO_CONTROL,x  ; wait until ready      
                bmi   ]retry
                ldy   #0            ; copy command bytes
]next           lda   ZP_PRODOS_CMD,y
                sta   IO_COMMAND,x
                iny
                cpy   #6
                bne   ]next
                lda   #$80          ; fire the command
                sta   IO_CONTROL,x
]retry          lda   IO_CONTROL,x  ; wait until ready
                bmi   ]retry                
                ror                 ; carry contains error if any
                lda   IO_COMMAND,x  ; error code
                pha                 ; store code onto stack
                lda   SCRNHOLE7+7
                and   #1            ; carry bit from saved processor status
                bne   :bootCheck
                txy
                lda   IO_RESULT0,y
                tax                 ; store aux result into X
                lda   IO_RESULT1,y
                tay                 ; store aux result into Y
                pla                 ; obtain prodos result again
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
:bootCheck      pla                 ; obtain prodos result from stack
                plb
                cli
                bcs   :bootNone
                lda   $0800         ; Boot sector if first byte 0, empty
                beq   :bootNone
                pea   $0000         ; main bank boot
                plb
                plb                 ; restore data bank
                jmp   $0801

:bootNone       lda   ZP_LOC1
                cmp   ACTIVE_PERIP_SLOT
                bne   :bootExit
                jmp   SLOOP
:bootExit       jmp   BASIC
BootCommand     hex   01,70,00,08,00,00 ; READ BLK 0 S7,D1 TO $800
                ds    $C7F0-*
SlotHiAddr      db    $C7
IOOffset        db    $70
                ds    $C7FE-*
                db    $87               ; STATUS,READ,WRITE,REMOVEABLE
                db    DriverEntry-SlotEntry
