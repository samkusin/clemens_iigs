.P816
.ORG $fc0000
.WORD $dead
.ORG $fd0000
.WORD $beef
.ORG $fe0000
.WORD $dead
.ORG $ff0000
.WORD $beef
.ORG $fffa00
.I8
 Start:         clc
                lda #$10
                sta $2000
                ldx #$10
 @Adder:        adc #$02
                sta $3009, X
                dex
                bne @Adder
End:            brk
.ORG $fffe00
                brk
.ORG $fffffc
.WORD $fa00
.WORD $fe00
.End
