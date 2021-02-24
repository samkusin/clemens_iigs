*           = $fc0000
            .word $dead
*           = $fd0000
            .word $beef
*           = $fe0000
            .word $dead
*           = $ff0000
            .byte $00, $01, $02, $03, $04, $05, $06, $07, $08, $09, $0A
*           = $fffa00
;
;   adc sanity tests
start       .block
            .databank $00
            .as
            ; adc without carry, lda, sta, carry should be true
            clc
            lda #$01
            adc #$09            ; A = 0x000A
            sta $002000         ; $002000 = 0x0A
            adc $002000         ; A = 0x0014
            sta $002001         ; $002001 = 0x14
            adc #$f0            ; A = 0x0004, c = 1, v = 0?
            sta $002002         ; $002002 = 0x04
            php                 ; Store status register
            pla                 ; A = status register
            sta $002100         ; $002200 = carry, irqdisable, brk=1
            ; use carry flag, lda 127 adc 0
            lda #$7f
            adc #$0
            sta $002003         ; $002003 = 0x80
            php
            pla
            sta $002101         ; $002201 = irqdisable, brk, overflow = 1
            brk
end         .bend
*           = $fffe00
            ; our custom break handler
            ; wdm, $01 = memory dump 3 pages from $002000 to stdout
            .byte $42, $01
            .byte $02
            .byte $00
            .byte $20
            stp
*           = $fffffc
            .word $fa00
            .word $fe00
