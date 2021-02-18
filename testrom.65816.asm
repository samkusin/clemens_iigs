*           = $fc0000
            .word $dead
*           = $fd0000
            .word $beef
*           = $fe0000
            .word $dead
*           = $ff0000
            .byte $00, $01, $02, $03, $04, $05, $06, $07, $08, $09, $0A
*           = $fffa00

start       .block
            .databank $00
            .as
            clc
            lda #$10
            sta $002000
            ldx #10
loop        adc $ff0000, X
            dex
            bne loop
            brk
end         .bend
*           = $fffe00
            brk
*           = $fffffc
            .word $fa00
            .word $fe00
