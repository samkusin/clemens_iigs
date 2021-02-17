.target "65816"
.format "bin"
.org $fc0000
.word $dead
.org $fd0000
.word $beef
.org $fe0000
.word $dead
.org $ff0000
.word $beef
.org $fffa00
                clc
                lda #$10
                sta $2000
                /* Add with no carry - immediate 8-bit */
                adc #$12
                sta $2001
                /* Add with no carry - absolute 8-bit */
                adc $2000
                sta $3000
                /* Add with no carry - absolute - indexed 8-bit */
                lda #$20
                ldx #$00
                adc $3000, X
                ldx #$01
                adc $3000, X
                ldx #$02
                adc $3000, X
                brk
.org $fffe00
                brk
.org $fffffc
.word $fa00
.word $fe00
.end
