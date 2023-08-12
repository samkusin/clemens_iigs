                org   $C700

SCRNHOLE6       equ  $0778
SCRNHOLE7       equ  $07f8
ACTIVE_PERIP_SLOT equ   $07f8
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
* Some of this is 'inspired' by the existing ROM at $C500.
*
* 'SlotEntry' - boot cycle
* ':proDOSEntry'- device driver entry point
*           - $42 - command (0 = status, 1 = read, 2 = write,
*                            3 = format)
*           - $43 - unit
*           - $44, $45 - pointer to data buffer (in/out)
*           - $46, $47 - block index
*           (See Table 6.4 Beneath Apple ProDOS)
* ':SPortEntry' - Smartport interface
*           - SP + 1   - cmdnum (standard or extended)
*           - SP + 2,3 - standard command pointer to parameter list
***************************************************************
SlotEntry       equ   *
                lda   #$20
                lda   #$00
                lda   #$03
                cmp   #$00          ; #$00 would be a smartport device
                bcs   :bootEntry    ; c=1 for boot

* Some 'clever' use of the carry flag to account for where the different
* entry points reside in the firmware.  Only a few bytes are available
* so we leverage processor status and the stack to persist information
*
* Carry flag is used to differentiate between boot and device driver entry
* points.  But we also need to differentiate between ProDOS and SmartPort calls
* all contained within our fixed entry points.  So the Accumulator is set
* following the CommonEntry point to 0 for ProDOS and 1 for Smarport, reflecting
* the carry flag set in the those two entry points respectively.
*
:proDOSEntry    clc                 ; c=0 for ProDOS
                bcc   :commonEntry
:SPortEntry     sec
:commonEntry    php                 ; Save ProDOS vs SmartPort settings
                pla                 ; A0: 1 = SmartPort, 0 = ProDOS Driver
                clc                 ; carry will now be 0 for a entry call
                eor  #1             ; A0: 1 = ProDOS Driver, 0 = SmartPort
:bootEntry      and  #1             ; carry will be 1 for boot, A=1 for ProDOS
                phb                 ; restore data bank on exit
                phk
                plb                 ; ensure bank 00 data access
                sta   SCRNHOLE6+7   ; Store SmartPort/ProDOS bool
                php                 ; carry flag saved along with pre-call state
                lda   :slotHiAddr
                sta   ACTIVE_PERIP_SLOT
                pla                 ; save carry for boot vs entrypoint exit
                sta   SCRNHOLE7+7
                bcs   :bootstrap    ; see carry notes above
                lda   SCRNHOLE6+7   ; 1 = ProDOS, 0 = SmartPort
                bra   :setupCall
*
* ProDOS command for READ into $0800 here
*
:bootstrap      ldx   #5
]next           lda   :bootCommand,x
                sta   ZP_PRODOS_CMD,x
                dex
                bpl   ]next
                lda   #1            ; Force ProDOS Call flow
*
* Once this emulator specific call finishes, we have two paths to deal with
* If Boot, then process the boot flow
* If EntryPoint call, then we exit, restoring processor state, result code, carry
*   indicating error or not.
*
* Contents on stack at this point: DBR, RTS
*
* Control and Command protocol
* CTL: write $00 to start a command (this will cancel any current command)
* CTL: read until bit 7 is lo (ready, idle)
* CMD: write command bytes
* CTL: when done, Write $80 to control fire the command
* CTL: Read where bit 7 is hi for being in progress
*                 bit 0 is hi if error
* CMD: Read the error code until CTL is set back $00

* A = 0 (SmartPort), 1 (ProDOS)
:setupCall      sei
                ldx   :IOOffset
                cmp   #1
                bne   :smartCall
:prodosCall     stz   IO_CONTROL,x  ; start the command
]retry          lda   IO_CONTROL,x  ; wait until ready
                bmi   ]retry
                ldy   #0            ; copy command bytes
]next           lda   ZP_PRODOS_CMD,y
                sta   IO_COMMAND,x
                iny
                cpy   #6
                bne   ]next
                bra   :execCommand
:smartCall      lda   #1
                sta   IO_CONTROL,x  ; Setup a SmartPort call
]retry          lda   IO_CONTROL,x
                bmi   ]retry
* ZP_PRODOS_CMD ($42,$43) will be used for accessing the CMDLIST
                lda   ZP_PRODOS_CMD
                pha
                lda   ZP_PRODOS_CMD+1
                pha
* S = DBR,$42,$43 (+1) from stack containing the return address
                ldy   #1
                lda   (4,S),y           ; command num at (return address + 1)+y
                sta   IO_COMMAND,x
                iny
                lda   (4,S),y           ; cmdlist (lo)
                sta   ZP_PRODOS_CMD
                iny
                lda   (4,S),y           ; cmdlist (hi)
                sta   ZP_PRODOS_CMD+1
                ldy   #0
]next           lda   (ZP_PRODOS_CMD),y ; read command list
                sta   IO_COMMAND,x      ; write command bytes
                iny
                lda   IO_CONTROL,x      ; bit 7 means command is not done
                bmi   ]next
                lda   4,S               ; +3 to return address to skip params
                clc
                adc   #3                ; carry set, then add to hi byte
                sta   4,S
                lda   #0
                adc   5,S
                sta   5,S
                pla
                sta   ZP_PRODOS_CMD+1
                pla
                sta   ZP_PRODOS_CMD
* common fire and wait for result
:execCommand    lda   #$80          ; fire the command
                sta   IO_CONTROL,x
]retry          lda   IO_CONTROL,x  ; wait until ready
                bmi   ]retry
:checkResult    tay
                lda   IO_COMMAND,x  ; error code
                pha                 ; store code onto stack
                lda   SCRNHOLE7+7
                and   #1            ; boot?
                bne   :bootCheck
                tya                 ; result from last IO op shifted to carry
                ror
                txy
                lda   IO_RESULT0,y
                tax                 ; store aux result into X
                lda   IO_RESULT1,y
                tay                 ; store aux result into Y
                lda   SCRNHOLE7+7
                and   #4            ; interrupt flag ON upon entry, then skip CLI
                bne   :exit1
                cli
:exit1          pla                 ; obtain error code again
                plb
                rts
*
* Boot validation
*   Carry set, then error reading boot sector
*   If sucessful, check byte at $800 - if 00, then ignore and treat as empty
*                 else jump to $801
*   On failure, bring up error screen
*
:bootCheck      tya
                ror                 ; carry <- saved return status from IO op
                pla                 ; obtain prodos result from stack
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
:bootCommand    hex   01,70,00,08,00,00 ; READ BLK 0 S7,D1 TO $800
                ds    $C7F0-*
:slotHiAddr     db    $C7
:IOOffset       db    $70
                ds    $C7FA-*
                db    $00
                db    $00               ; SmartPort ID type Standard
                ds    $C7FE-*
                db    $97               ; 2 volumes, STATUS,READ,WRITE,REMOVEABLE
                db    :proDOSEntry-SlotEntry
