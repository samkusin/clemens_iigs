; The demo system file (designed to run under ProDOS 8)
;
; Relocoate main system
; Jump to relocated code

        .include "apple2.mac"

; System addresses
SOFTEV          := $3F2
PWREDUP         := $3F4

PRODOS_MLI      := $BF00
PRODOS_NODEV    := $BF10
PRODOS_RAMSLOT  := $BF26
PRODOS_DEVCNT   := $BF31
PRODOS_DEVLST   := $BF32

PRODOS_BITMAP   := $BF58
PRODOS_BITMAP_SIZE = 24

IO_CLR80COL     := $C000
IO_CLR80VID     := $C00C
IO_CLRALTCHAR   := $C00E
IO_ROMIN2       := $C082

INIT            := $FB2F
VERSION         := $FBB3
WAIT            := $FCA8
CROUT1          := $FD8B
CROUT           := $FD8E
PRBYTE          := $FDDA
COUT            := $FDED
IDROUTINE       := $FE1F
SETNORM         := $FE84
SETKBD          := $FE89
SETVID          := $FE93

;   MLI Functions
MLI_QUIT        = $65

;   Application Constants
ZP_SCRATCH_PTR   = $FE
START_PATH_SIZE  = 32

                .setcpu "6502"
                .feature string_escapes

.macro CallProdosMLI op, params_addr
                jsr PRODOS_MLI
                .byte op
                .addr params_addr
.endmacro


;   Main System Bootstrap and Program
                sys_main_start := $B000

                .org    $2000          ; ProDOS system code start point
                jmp     relocate_main
                .byte   $EE, $EE
start_pathname: .byte   START_PATH_SIZE
                .res    START_PATH_SIZE

;   Borrowed from a2stuff
.proc relocate_main
                src := sys_orig_start
                dest := sys_main_start
                ldx     #(sys_orig_end - sys_orig_end + $ff) / $100
                ldy     #0
copy:           lda     src, y
                copy_in_hi := *-1
                sta     dest, y
                copy_out_hi := *-1
                iny
                bne     copy
                inc     copy_in_hi
                inc     copy_out_hi
                dex
                bne     copy

                jmp     main
.endproc

; The relocatable test framework which handles loading tests into $800 - $1FFF
; A test "kernel" is also defined here for tests to execute common utility
; logic
                .setcpu "65816"
                sys_orig_start := *
                .org sys_main_start
.proc entrypoint

.endproc

gs_rom_version: .byte $00
ramd_vec:       .word $0000


.proc main
                cld

            ; Reset Stack Pointer per tech ref 5.2.1
                sei
                ldx #$ff
                txs
                cli

            ; Identify machine
                lda VERSION
                cmp #$06
                beq verify_gs
                brk

            ; Tech Note #7 method of detecting IIgs
verify_gs:      sec
                bcc confirm_gs
                jsr IDROUTINE
                brk

confirm_gs:     sty gs_rom_version

            ; Setup reset vector and power byte
                bit IO_ROMIN2
                lda #<quit_system
                sta SOFTEV
                lda #>quit_system
                sta SOFTEV+1
                eor #$A5
                sta PWREDUP

            ; Reset screen and I/O
                sta IO_CLR80VID
                sta IO_CLR80COL
                sta IO_CLRALTCHAR
                jsr SETNORM
                jsr INIT
                jsr SETVID
                jsr SETKBD

            ; Reserve ZP, Stack, Text Page, $B000 - $BF00 (relocated + system)
                ldx #0
                lda #%11001111
                sta PRODOS_BITMAP, x
                inx
@zero_bitmap:   stz PRODOS_BITMAP, x
                    cpx #PRODOS_BITMAP_SIZE-2
                    bne @zero_bitmap
                lda #%11111111
                sta PRODOS_BITMAP, x
                inx
                sta PRODOS_BITMAP, x

            ; Disconnect /RAM since at this point we know we are running on
            ; an Apple IIgs
                lda PRODOS_RAMSLOT
                cmp PRODOS_NODEV
                bne ramd_check
                lda PRODOS_RAMSLOT+1
                cmp PRODOS_NODEV+1
                beq ramd_done

                ; remove the ramdisk from the prodos device list and store off
                ; old vector, shifting contents of the device list left - at
                ; this point we know there's a device where the /RAM shoule be
                ; and we confirm that here as well
ramd_check:     ldx PRODOS_DEVCNT
@dev_next:      lda PRODOS_DEVLST, x
                    and #$70              ; slot in upper nibble, is it slot 3?
                    cmp #$30
                    beq @shrink_list
                    dex
                    bpl @dev_next
                bra ramd_done
                ;   compress list
@shrink_list:   lda PRODOS_DEVLST+1, x
                    sta PRODOS_DEVLST, x
                    inx
                    cpx PRODOS_DEVCNT
                    bne @shrink_list
                dec PRODOS_DEVCNT
                stz PRODOS_DEVLST, x
                ;   save out vector
                lda PRODOS_RAMSLOT
                sta ramd_vec
                lda PRODOS_RAMSLOT+1
                sta ramd_vec+1
                lda PRODOS_NODEV
                sta PRODOS_RAMSLOT
                lda PRODOS_NODEV+1
                sta PRODOS_RAMSLOT+1
                ; saved off /RAM vector or none was found
ramd_done:      ; NOW we can run our actual test program(s)
                lda #<status_string
                ldy #>status_string
                jsr print_string
                jmp quit_system

status_string:
                scrcode "DONE!"
                .byte   0
.endproc

.proc print_string
                pha
                phy
                sta ZP_SCRATCH_PTR
                sty ZP_SCRATCH_PTR+1
                ldy #0
@loop:          lda (ZP_SCRATCH_PTR), y
                beq @end
                jsr COUT
                iny
                bpl @loop              ; limit string output branching
@end:           jsr CROUT1
                ply
                pla
                rts
.endproc

;   Reset points to this procedure as well - quit the system program here
.proc quit_system
                CallProdosMLI MLI_QUIT, quit_params
                brk
quit_params:    .byte $04
                .byte $00
                .word $0000
                .byte $00
                .word $0000
.endproc
                sys_orig_end := *
