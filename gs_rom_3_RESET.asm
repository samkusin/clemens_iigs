$fffa62 a9 01         lda #$01
$fffa64 0c 29 c0      tsb $c029
$fffa67 a9 fb         lda #$fb
$fffa69 78            sei
$fffa6a d8            cld
$fffa6b 1b            tcs
$fffa6c 20 36 fe      jsr $fe36
$fffa6f a9 0c         lda #$0c
$fffa71 8d 68 c0      sta $c068
$fffa74 a0 09         ldy #$09
$fffa76 20 9c f8      jsr $f89c
$fffa79 ad ff cf      lda $cfff
$fffa7c 2c 10 c0      bit $c010
$fffa7f 20 24 fa      jsr $fa24
$fffa82 d0 03         bne $fffa87
$fffa84 20 12 fd      jsr $fd12
$fffa87 20 9a f8      jsr $f89a
$fffa8a ad f3 03      lda $03f3
$fffa8d 49 a5         eor #$a5
$fffa8f cd f4 03      cmp $03f4
$fffa92 d0 12         bne $fffaa6
$fffa94 ad f2 03      lda $03f2
$fffa97 d0 42         bne $fffadb
$fffa99 a9 e0         lda #$e0
$fffa9b cd f3 03      cmp $03f3
$fffa9e d0 3b         bne $fffadb
$fffaa0 a0 03         ldy #$03
$fffaa2 4c e6 fe      jmp $fee6
$fffaa5 00 20         brk $20
$fffaa7 f4 f8 a2      pea $a2f8
$fffaaa 05 bd         ora $bd
$fffaac fc fa 9d      jsr ($9dfa,x)
$fffaaf ef 03 ca d0   sbc $d0ca03
$fffab3 f7 80         sbc [$80],y
$fffab5 2b            pld
$fffab6 85 01         sta $01
$fffab8 64 00         stz $00
$fffaba a0 05         ldy #$05
$fffabc c6 01         dec $01
$fffabe a9 00         lda #$00
$fffac0 a5 01         lda $01
$fffac2 c9 c0         cmp #$c0
$fffac4 f0 32         beq $fffaf8
$fffac6 8d f8 07      sta $07f8
$fffac9 b1 00         lda ($00),y
$fffacb d9 01 fb      cmp $fb01,y
$ffface d0 ea         bne $fffaba
$fffad0 88            dey
$fffad1 88            dey
$fffad2 10 f5         bpl $fffac9
$fffad4 6c 00 00      jmp ($0000)
$fffad7 a0 a5         ldy #$a5
$fffad9 80 1f         bra $fffafa
$fffadb 20 d2 f9      jsr $f9d2
$fffade 6c f2 03      jmp ($03f2)
$fffae1 af e8 02 e1   lda $e102e8
$fffae5 f0 0d         beq $fffaf4
$fffae7 c9 08         cmp #$08
$fffae9 90 04         bcc $fffaef
$fffaeb 80 37         bra $fffb24
$fffaed a9 05         lda #$05
$fffaef 09 c0         ora #$c0
$fffaf1 1a            inc
$fffaf2 80 c2         bra $fffab6
$fffaf4 a9 c8         lda #$c8
$fffaf6 80 be         bra $fffab6
$fffaf8 a0 98         ldy #$98
$fffafa 4c 9c f8      jmp $f89c
$fffafd 59 fa 00      eor $00fa,y
$fffb00 e0 45         cpx #$45
$fffb02 20 ff 00      jsr $00ff
$fffb05 ff 03 ff 3c   sbc $3cff03,x
$fffb09 c1 f0         cmp ($f0,x)
$fffb0b f0 ec         beq $fffaf9
$fffb0d e5 a0         sbc $a0
$fffb0f dd db c4      cmp $c4db,x
$fffb12 c2 c1         rep #$c1
$fffb14 ff c3 ff ff   sbc $ffffc3,x
$fffb18 cd c1 d8      cmp $d8c1
$fffb1b d9 d0 d3      cmp $d3d0,y
$fffb1e 4c eb fb      jmp $fbeb
$fffb21 4c e6 fb      jmp $fbe6
$fffb24 c9 0a         cmp #$0a
$fffb26 d0 c5         bne $fffaed
$fffb28 4c 20 f9      jmp $f920
$fffb2b 4c f3 a9      jmp $a9f3
$fffb2e 00 a9         brk $a9
$fffb30 00 85         brk $85
$fffb32 48            pha
$fffb33 ad 56 c0      lda $c056
$fffb36 ad 54 c0      lda $c054
$fffb39 ad 51 c0      lda $c051
$fffb3c a9 00         lda #$00
$fffb3e f0 0b         beq $fffb4b
$fffb40 ad 50 c0      lda $c050
$fffb43 ad 53 c0      lda $c053
$fffb46 20 36 f8      jsr $f836
$fffb49 a9 14         lda #$14
$fffb4b 85 22         sta $22
$fffb4d a9 00         lda #$00
$fffb4f 85 20         sta $20
$fffb51 a0 0c         ldy #$0c
$fffb53 80 a5         bra $fffafa
$fffb55 7f 00 00 00   adc $000000,x
$fffb59 03 00         ora $00,sp
$fffb5b 85 25         sta $25
$fffb5d 4c 22 fc      jmp $fc22
$fffb60 20 58 fc      jsr $fc58
$fffb63 a0 a8         ldy #$a8
$fffb65 80 93         bra $fffafa
$fffb67 05 07         ora $07
$fffb69 0b            phd
$fffb6a fb            xce
$fffb6b 38            sec
$fffb6c 18            clc
$fffb6d 01 d6         ora ($d6,x)
$fffb6f ad f3 03      lda $03f3
$fffb72 49 a5         eor #$a5
$fffb74 8d f4 03      sta $03f4
$fffb77 60            rts
$fffb78 c9 8d         cmp #$8d
$fffb7a d0 18         bne $fffb94
$fffb7c ac 00 c0      ldy $c000
$fffb7f 10 13         bpl $fffb94
$fffb81 c0 93         cpy #$93
$fffb83 d0 0f         bne $fffb94
$fffb85 2c 10 c0      bit $c010
$fffb88 ac 00 c0      ldy $c000
$fffb8b 10 fb         bpl $fffb88
$fffb8d c0 83         cpy #$83
$fffb8f f0 03         beq $fffb94
$fffb91 2c 10 c0      bit $c010
$fffb94 80 67         bra $fffbfd
$fffb96 a8            tay
$fffb97 b9 48 fa      lda $fa48,y
$fffb9a 20 ac fb      jsr $fbac
$fffb9d 20 02 fd      jsr $fd02
$fffba0 c9 ce         cmp #$ce
$fffba2 b0 08         bcs $fffbac
$fffba4 c9 c9         cmp #$c9
$fffba6 90 04         bcc $fffbac
$fffba8 c9 cc         cmp #$cc
$fffbaa d0 ea         bne $fffb96
$fffbac c9 b8         cmp #$b8
$fffbae 38            sec
$fffbaf d0 7b         bne $fffc2c
$fffbb1 80 01         bra $fffbb4
$fffbb3 06 20         asl $20
$fffbb5 2c fa d0      bit $d0fa
$fffbb8 73 20         adc ($20,sp),y
$fffbba 90 fc         bcc $fffbb8
$fffbbc 4c ef fe      jmp $feef
$fffbbf 00 e0         brk $e0
$fffbc1 48            pha
$fffbc2 4a            lsr
$fffbc3 29 03         and #$03
$fffbc5 09 04         ora #$04
$fffbc7 85 29         sta $29
$fffbc9 68            pla
$fffbca 29 18         and #$18
$fffbcc 90 02         bcc $fffbd0
$fffbce 69 7f         adc #$7f
$fffbd0 85 28         sta $28
$fffbd2 0a            asl
$fffbd3 0a            asl
$fffbd4 05 28         ora $28
$fffbd6 85 28         sta $28
$fffbd8 60            rts
$fffbd9 c9 87         cmp #$87
$fffbdb d0 1f         bne $fffbfc
$fffbdd a0 9e         ldy #$9e
$fffbdf 80 48         bra $fffc29
$fffbe1 ea            nop
$fffbe2 80 f9         bra $fffbdd
$fffbe4 80 f7         bra $fffbdd
$fffbe6 bd 64 c0      lda $c064,x
$fffbe9 10 fb         bpl $fffbe6
$fffbeb a0 95         ldy #$95
$fffbed 80 3a         bra $fffc29
$fffbef 00 a4         brk $a4
$fffbf1 24 91         bit $91
$fffbf3 28            plp
$fffbf4 e6 24         inc $24
$fffbf6 a5 24         lda $24
$fffbf8 c5 21         cmp $21
$fffbfa b0 66         bcs $fffc62
$fffbfc 60            rts
$fffbfd 80 5d         bra $fffc5c
$fffbff b0 ef         bcs $fffbf0
$fffc01 a8            tay
$fffc02 10 ec         bpl $fffbf0
$fffc04 c9 8d         cmp #$8d
$fffc06 f0 5a         beq $fffc62
$fffc08 c9 8a         cmp #$8a
$fffc0a f0 5a         beq $fffc66
$fffc0c c9 88         cmp #$88
$fffc0e d0 75         bne $fffc85
$fffc10 5a            phy
$fffc11 a0 9b         ldy #$9b
$fffc13 20 9c f8      jsr $f89c
$fffc16 30 63         bmi $fffc7b
$fffc18 7a            ply
$fffc19 60            rts
$fffc1a a5 22         lda $22
$fffc1c c5 25         cmp $25
$fffc1e b0 dc         bcs $fffbfc
$fffc20 c6 25         dec $25
$fffc22 a5 25         lda $25
$fffc24 85 28         sta $28
$fffc26 98            tya
$fffc27 a0 04         ldy #$04
$fffc29 4c 9c f8      jmp $f89c
$fffc2c 49 c0         eor #$c0
$fffc2e f0 28         beq $fffc58
$fffc30 69 fd         adc #$fd
$fffc32 90 c0         bcc $fffbf4
$fffc34 f0 da         beq $fffc10
$fffc36 69 fd         adc #$fd
$fffc38 90 2c         bcc $fffc66
$fffc3a f0 de         beq $fffc1a
$fffc3c 69 fd         adc #$fd
$fffc3e 90 5c         bcc $fffc9c
$fffc40 d0 ba         bne $fffbfc
$fffc42 a0 0a         ldy #$0a
$fffc44 d0 e3         bne $fffc29
$fffc46 2c 1f c0      bit $c01f
$fffc49 10 04         bpl $fffc4f
$fffc4b a0 00         ldy #$00
$fffc4d 80 da         bra $fffc29
$fffc4f 98            tya
$fffc50 48            pha
$fffc51 20 78 fb      jsr $fb78
$fffc54 68            pla
$fffc55 a4 35         ldy $35
$fffc57 60            rts
$fffc58 a0 05         ldy #$05
$fffc5a 80 cd         bra $fffc29
$fffc5c eb            xba
$fffc5d 4c eb fc      jmp $fceb
$fffc60 00 00         brk $00
$fffc62 a9 00         lda #$00
$fffc64 85 24         sta $24
$fffc66 e6 25         inc $25
$fffc68 a5 25         lda $25
$fffc6a c5 23         cmp $23
$fffc6c 90 b6         bcc $fffc24
$fffc6e c6 25         dec $25
$fffc70 a0 06         ldy #$06
$fffc72 80 b5         bra $fffc29
$fffc74 90 02         bcc $fffc78
$fffc76 25 32         and $32
$fffc78 4c f7 fd      jmp $fdf7
$fffc7b a5 21         lda $21
$fffc7d a0 9c         ldy #$9c
$fffc7f 20 9c f8      jsr $f89c
$fffc82 7a            ply
$fffc83 80 95         bra $fffc1a
$fffc85 c9 9e         cmp #$9e
$fffc87 f0 03         beq $fffc8c
$fffc89 4c d9 fb      jmp $fbd9
$fffc8c a9 81         lda #$81
$fffc8e 80 02         bra $fffc92
$fffc90 a9 40         lda #$40
$fffc92 8f 37 01 e1   sta $e10137
$fffc96 60            rts
$fffc97 a0 97         ldy #$97
$fffc99 80 8e         bra $fffc29
$fffc9b 00 38         brk $38
$fffc9d 90 18         bcc $fffcb7
$fffc9f 84 2a         sty $2a
$fffca1 a0 07         ldy #$07
$fffca3 b0 84         bcs $fffc29
$fffca5 c8            iny
$fffca6 80 81         bra $fffc29
$fffca8 48            pha
$fffca9 ad 36 c0      lda $c036
$fffcac eb            xba
$fffcad a9 80         lda #$80
$fffcaf 1c 36 c0      trb $c036
$fffcb2 80 25         bra $fffcd9
$fffcb4 e6 42         inc $42
$fffcb6 d0 02         bne $fffcba
$fffcb8 e6 43         inc $43
$fffcba a5 3c         lda $3c
$fffcbc c5 3e         cmp $3e
$fffcbe a5 3d         lda $3d
$fffcc0 e5 3f         sbc $3f
$fffcc2 e6 3c         inc $3c
$fffcc4 d0 02         bne $fffcc8
$fffcc6 e6 3d         inc $3d
$fffcc8 60            rts
$fffcc9 60            rts
$fffcca b9 00 02      lda $0200,y
$fffccd c8            iny
$fffcce c9 e1         cmp #$e1
$fffcd0 90 06         bcc $fffcd8
$fffcd2 c9 fb         cmp #$fb
$fffcd4 b0 02         bcs $fffcd8
$fffcd6 29 df         and #$df
$fffcd8 60            rts
$fffcd9 68            pla
$fffcda 38            sec
$fffcdb 48            pha
$fffcdc e9 01         sbc #$01
$fffcde d0 fc         bne $fffcdc
$fffce0 68            pla
$fffce1 e9 01         sbc #$01
$fffce3 d0 f6         bne $fffcdb
$fffce5 eb            xba
$fffce6 8d 36 c0      sta $c036
$fffce9 eb            xba
$fffcea 60            rts
$fffceb af 37 01 e1   lda $e10137
$fffcef 0a            asl
$fffcf0 30 20         bmi $fffd12
$fffcf2 eb            xba
$fffcf3 90 08         bcc $fffcfd
$fffcf5 8f 35 01 e1   sta $e10135
$fffcf9 a9 01         lda #$01
$fffcfb 80 95         bra $fffc92
$fffcfd c9 a0         cmp #$a0
$fffcff 4c ff fb      jmp $fbff
$fffd02 20 0c fd      jsr $fd0c
$fffd05 a0 01         ldy #$01
$fffd07 80 90         bra $fffc99
$fffd09 4e f8 07      lsr $07f8
$fffd0c a0 0b         ldy #$0b
$fffd0e d0 0f         bne $fffd1f
$fffd10 80 06         bra $fffd18
$fffd12 a0 a2         ldy #$a2
$fffd14 80 83         bra $fffc99
$fffd16 00 00         brk $00
$fffd18 6c 38 00      jmp ($0038)
$fffd1b a0 03         ldy #$03
$fffd1d 80 e8         bra $fffd07
$fffd1f 20 9c f8      jsr $f89c
$fffd22 80 ec         bra $fffd10
$fffd24 20 78 f9      jsr $f978
$fffd27 b0 4c         bcs $fffd75
$fffd29 c9 88         cmp #$88
$fffd2b 80 25         bra $fffd52
$fffd2d 00 00         brk $00
$fffd2f 20 02 fd      jsr $fd02
$fffd32 20 a0 fb      jsr $fba0
$fffd35 20 09 fd      jsr $fd09
$fffd38 c9 9b         cmp #$9b
$fffd3a f0 f3         beq $fffd2f
$fffd3c 60            rts
$fffd3d 6c fe 03      jmp ($03fe)
$fffd40 a0 0d         ldy #$0d
$fffd42 20 9c f8      jsr $f89c
$fffd45 a4 24         ldy $24
$fffd47 9d 00 02      sta $0200,x
$fffd4a 20 ed fd      jsr $fded
$fffd4d bd 00 02      lda $0200,x
$fffd50 80 d2         bra $fffd24
$fffd52 f0 1d         beq $fffd71
$fffd54 c9 98         cmp #$98
$fffd56 f0 0a         beq $fffd62
$fffd58 e0 f8         cpx #$f8
$fffd5a 90 03         bcc $fffd5f
$fffd5c 20 3a ff      jsr $ff3a
$fffd5f e8            inx
$fffd60 d0 13         bne $fffd75
$fffd62 a9 dc         lda #$dc
$fffd64 20 ed fd      jsr $fded
$fffd67 20 8e fd      jsr $fd8e
$fffd6a a5 33         lda $33
$fffd6c 20 ed fd      jsr $fded
$fffd6f a2 01         ldx #$01
$fffd71 8a            txa
$fffd72 f0 f3         beq $fffd67
$fffd74 ca            dex
$fffd75 20 35 fd      jsr $fd35
$fffd78 c9 95         cmp #$95
$fffd7a d0 08         bne $fffd84
$fffd7c b1 28         lda ($28),y
$fffd7e 2c 1f c0      bit $c01f
$fffd81 30 bd         bmi $fffd40
$fffd83 ea            nop
$fffd84 20 be fd      jsr $fdbe
$fffd87 c9 8d         cmp #$8d
$fffd89 d0 bf         bne $fffd4a
$fffd8b 20 9c fc      jsr $fc9c
$fffd8e a9 8d         lda #$8d
$fffd90 80 5b         bra $fffded
$fffd92 a4 3d         ldy $3d
$fffd94 a6 3c         ldx $3c
$fffd96 20 8e fd      jsr $fd8e
$fffd99 20 40 f9      jsr $f940
$fffd9c a0 00         ldy #$00
$fffd9e a9 ba         lda #$ba
$fffda0 80 4b         bra $fffded
$fffda2 20 75 fe      jsr $fe75
$fffda5 05 3c         ora $3c
$fffda7 85 3e         sta $3e
$fffda9 a5 3d         lda $3d
$fffdab 85 3f         sta $3f
$fffdad 4c 6d fe      jmp $fe6d
$fffdb0 48            pha
$fffdb1 68            pla
$fffdb2 f0 f9         beq $fffdad
$fffdb4 c9 ae         cmp #$ae
$fffdb6 f0 f5         beq $fffdad
$fffdb8 5a            phy
$fffdb9 a0 a1         ldy #$a1
$fffdbb 4c 73 f9      jmp $f973
$fffdbe c9 ff         cmp #$ff
$fffdc0 d0 11         bne $fffdd3
$fffdc2 eb            xba
$fffdc3 af 37 01 e1   lda $e10137
$fffdc7 0a            asl
$fffdc8 eb            xba
$fffdc9 90 06         bcc $fffdd1
$fffdcb 2c 1f c0      bit $c01f
$fffdce 10 03         bpl $fffdd3
$fffdd0 1a            inc
$fffdd1 29 88         and #$88
$fffdd3 9d 00 02      sta $0200,x
$fffdd6 60            rts
$fffdd7 4c 46 fc      jmp $fc46
$fffdda 48            pha
$fffddb 4a            lsr
$fffddc 4a            lsr
$fffddd 4a            lsr
$fffdde 4a            lsr
$fffddf 20 e5 fd      jsr $fde5
$fffde2 68            pla
$fffde3 29 0f         and #$0f
$fffde5 09 b0         ora #$b0
$fffde7 c9 ba         cmp #$ba
$fffde9 90 02         bcc $fffded
$fffdeb 69 06         adc #$06
$fffded 6c 36 00      jmp ($0036)
$fffdf0 48            pha
$fffdf1 c9 a0         cmp #$a0
$fffdf3 4c 74 fc      jmp $fc74
$fffdf6 48            pha
$fffdf7 84 35         sty $35
$fffdf9 a8            tay
$fffdfa 68            pla
$fffdfb 80 da         bra $fffdd7
$fffdfd c6 34         dec $34
$fffdff f0 a1         beq $fffda2
$fffe01 ca            dex
$fffe02 d0 0c         bne $fffe10
$fffe04 c9 ba         cmp #$ba
$fffe06 d0 a8         bne $fffdb0
$fffe08 4c da fe      jmp $feda
$fffe0b a4 34         ldy $34
$fffe0d b9 ff 01      lda $01ff,y
$fffe10 85 31         sta $31
$fffe12 60            rts
$fffe13 18            clc
$fffe14 fb            xce
$fffe15 5c a0 00 e1   jmp $e100a0
$fffe19 38            sec
$fffe1a fb            xce
$fffe1b 4c f8 03      jmp $03f8
$fffe1e 00 ad         brk $ad
$fffe20 55 fb         eor $fb,x
$fffe22 ae 57 fb      ldx $fb57
$fffe25 ac 59 fb      ldy $fb59
$fffe28 18            clc
$fffe29 60            rts
$fffe2a 00 00         brk $00
$fffe2c b1 3c         lda ($3c),y
$fffe2e 91 42         sta ($42),y
$fffe30 20 b4 fc      jsr $fcb4
$fffe33 90 f7         bcc $fffe2c
$fffe35 60            rts
$fffe36 18            clc
$fffe37 fb            xce
$fffe38 22 00 76 ff   jsr $ff7600
$fffe3c 38            sec
$fffe3d fb            xce
$fffe3e a0 a3         ldy #$a3
$fffe40 20 9c f8      jsr $f89c
$fffe43 20 84 fe      jsr $fe84
$fffe46 20 2f fb      jsr $fb2f
$fffe49 20 93 fe      jsr $fe93
$fffe4c 80 3b         bra $fffe89
$fffe4e a0 a9         ldy #$a9
$fffe50 4c 9c f8      jmp $f89c
$fffe53 a0 aa         ldy #$aa
$fffe55 4c 9c f8      jmp $f89c
$fffe58 00 00         brk $00
$fffe5a 00 00         brk $00
$fffe5c 00 00         brk $00
$fffe5e a0 83         ldy #$83
$fffe60 80 11         bra $fffe73
$fffe62 8a            txa
$fffe63 f0 07         beq $fffe6c
$fffe65 b5 3c         lda $3c,x
$fffe67 95 3a         sta $3a,x
$fffe69 ca            dex
$fffe6a 10 f9         bpl $fffe65
$fffe6c 60            rts
$fffe6d 48            pha
$fffe6e 98            tya
$fffe6f 09 80         ora #$80
$fffe71 a8            tay
$fffe72 68            pla
$fffe73 80 36         bra $fffeab
$fffe75 a5 21         lda $21
$fffe77 c9 48         cmp #$48
$fffe79 a9 0f         lda #$0f
$fffe7b b0 02         bcs $fffe7f
$fffe7d a9 07         lda #$07
$fffe7f 60            rts
$fffe80 a0 3f         ldy #$3f
$fffe82 d0 02         bne $fffe86
$fffe84 a0 ff         ldy #$ff
$fffe86 84 32         sty $32
$fffe88 60            rts
$fffe89 a9 00         lda #$00
$fffe8b 85 3e         sta $3e
$fffe8d a2 38         ldx #$38
$fffe8f a0 1b         ldy #$1b
$fffe91 d0 08         bne $fffe9b
$fffe93 a9 00         lda #$00
$fffe95 85 3e         sta $3e
$fffe97 a2 36         ldx #$36
$fffe99 a0 f0         ldy #$f0
$fffe9b a5 3e         lda $3e
$fffe9d 29 0f         and #$0f
$fffe9f f0 04         beq $fffea5
$fffea1 09 c0         ora #$c0
$fffea3 a0 00         ldy #$00
$fffea5 94 00         sty $00,x
$fffea7 95 01         sta $01,x
$fffea9 a0 0e         ldy #$0e
$fffeab 80 36         bra $fffee3
$fffead 4c 03 e0      jmp $e003
$fffeb0 ad 54 c0      lda $c054
$fffeb3 60            rts
$fffeb4 80 5a         bra $ffff10
$fffeb6 20 62 fe      jsr $fe62
$fffeb9 20 3f ff      jsr $ff3f
$fffebc 6c 3a 00      jmp ($003a)
$fffebf ad d0 03      lda $03d0
$fffec2 c9 4c         cmp #$4c
$fffec4 f0 01         beq $fffec7
$fffec6 60            rts
$fffec7 4c d0 03      jmp $03d0
$fffeca 4c d7 fa      jmp $fad7
$fffecd 60            rts
$fffece c8            iny
$fffecf c8            iny
$fffed0 c8            iny
$fffed1 c8            iny
$fffed2 c8            iny
$fffed3 c8            iny
$fffed4 c8            iny
$fffed5 c8            iny
$fffed6 c8            iny
$fffed7 c8            iny
$fffed8 c8            iny
$fffed9 c8            iny
$fffeda c8            iny
$fffedb c8            iny
$fffedc c8            iny
$fffedd c8            iny
$fffede c8            iny
$fffedf 80 8c         bra $fffe6d
$fffee1 a0 96         ldy #$96
$fffee3 4c 9c f8      jmp $f89c
$fffee6 8c f2 03      sty $03f2
$fffee9 20 d2 f9      jsr $f9d2
$fffeec 4c 00 e0      jmp $e000
$fffeef 68            pla
$fffef0 68            pla
$fffef1 a9 98         lda #$98
$fffef3 4c ed fd      jmp $fded
$fffef6 20 fd fd      jsr $fdfd
$fffef9 68            pla
$fffefa 68            pla
$fffefb 80 6f         bra $ffff6c
$fffefd 60            rts
$fffefe af 3e 01 e1   lda $e1013e
$ffff02 8f 3f 01 e1   sta $e1013f
$ffff06 a2 01         ldx #$01
$ffff08 b5 3e         lda $3e,x
$ffff0a 95 42         sta $42,x
$ffff0c ca            dex
$ffff0d 10 f9         bpl $ffff08
$ffff0f 60            rts
$ffff10 c8            iny
$ffff11 80 bb         bra $fffece
$ffff13 20 ca fc      jsr $fcca
$ffff16 c9 a0         cmp #$a0
$ffff18 f0 f9         beq $ffff13
$ffff1a 60            rts
$ffff1b b0 6d         bcs $ffff8a
$ffff1d c9 a0         cmp #$a0
$ffff1f d0 28         bne $ffff49
$ffff21 b9 00 02      lda $0200,y
$ffff24 a2 07         ldx #$07
$ffff26 c9 8d         cmp #$8d
$ffff28 f0 7d         beq $ffffa7
$ffff2a c8            iny
$ffff2b d0 63         bne $ffff90
$ffff2d a9 c5         lda #$c5
$ffff2f 20 ed fd      jsr $fded
$ffff32 a9 d2         lda #$d2
$ffff34 20 ed fd      jsr $fded
$ffff37 20 ed fd      jsr $fded
$ffff3a a9 87         lda #$87
$ffff3c 4c ed fd      jmp $fded
$ffff3f a5 48         lda $48
$ffff41 48            pha
$ffff42 a5 45         lda $45
$ffff44 a6 46         ldx $46
$ffff46 a4 47         ldy $47
$ffff48 28            plp
$ffff49 60            rts
$ffff4a 85 45         sta $45
$ffff4c 86 46         stx $46
$ffff4e 84 47         sty $47
$ffff50 08            php
$ffff51 68            pla
$ffff52 85 48         sta $48
$ffff54 ba            tsx
$ffff55 86 49         stx $49
$ffff57 d8            cld
$ffff58 60            rts
$ffff59 20 43 fe      jsr $fe43
$ffff5c 80 07         bra $ffff65
$ffff5e a9 aa         lda #$aa
$ffff60 85 33         sta $33
$ffff62 4c 67 fd      jmp $fd67
$ffff65 d8            cld
$ffff66 4c 43 fa      jmp $fa43
$ffff69 20 e1 fe      jsr $fee1
$ffff6c ea            nop
$ffff6d 20 5e ff      jsr $ff5e
$ffff70 20 d1 ff      jsr $ffd1
$ffff73 20 88 f8      jsr $f888
$ffff76 84 34         sty $34
$ffff78 a0 25         ldy #$25
$ffff7a 88            dey
$ffff7b 30 e8         bmi $ffff65
$ffff7d d9 88 f9      cmp $f988,y
$ffff80 d0 f8         bne $ffff7a
$ffff82 20 be ff      jsr $ffbe
$ffff85 a4 34         ldy $34
$ffff87 4c 73 ff      jmp $ff73
$ffff8a a2 03         ldx #$03
$ffff8c 0a            asl
$ffff8d 0a            asl
$ffff8e 0a            asl
$ffff8f 0a            asl
$ffff90 0a            asl
$ffff91 26 3e         rol $3e
$ffff93 26 3f         rol $3f
$ffff95 ca            dex
$ffff96 10 f8         bpl $ffff90
$ffff98 a5 31         lda $31
$ffff9a d0 06         bne $ffffa2
$ffff9c b5 3f         lda $3f,x
$ffff9e 95 3d         sta $3d,x
$ffffa0 95 41         sta $41,x
$ffffa2 e8            inx
$ffffa3 f0 f3         beq $ffff98
$ffffa5 d0 06         bne $ffffad
$ffffa7 a2 00         ldx #$00
$ffffa9 86 3e         stx $3e
$ffffab 86 3f         stx $3f
$ffffad 20 ca fc      jsr $fcca
$ffffb0 ea            nop
$ffffb1 49 b0         eor #$b0
$ffffb3 c9 0a         cmp #$0a
$ffffb5 90 d3         bcc $ffff8a
$ffffb7 69 88         adc #$88
$ffffb9 c9 fa         cmp #$fa
$ffffbb 4c 1b ff      jmp $ff1b
$ffffbe a9 fe         lda #$fe
$ffffc0 48            pha
$ffffc1 b9 ad f9      lda $f9ad,y
$ffffc4 48            pha
$ffffc5 a5 31         lda $31
$ffffc7 a0 00         ldy #$00
$ffffc9 84 31         sty $31
$ffffcb 60            rts
$ffffcc a0 00         ldy #$00
$ffffce 80 20         bra $fffff0
$ffffd0 ef 64 34 20   sbc $203464
$ffffd4 c7 ff         cmp [$ff]
$ffffd6 ad 36 c0      lda $c036
$ffffd9 29 7f         and #$7f
$ffffdb 0f 38 01 e1   ora $e10138
$ffffdf 8d 36 c0      sta $c036
$ffffe2 60            rts
$ffffe3 00 7b         brk $7b
$ffffe5 c0 71         cpy #$71
$ffffe7 c0 79         cpy #$79
$ffffe9 c0 fb         cpy #$fb
$ffffeb 03 e6         ora $e6,sp
$ffffed e2 74         sep #$74
$ffffef c0 4c         cpy #$4c
$fffff1 6c f9 00      jmp ($00f9)
$fffff4 7b            tdc
$fffff5 c0 50         cpy #$50
$fffff7 62 79 c0      per $ffc073
$fffffa fb            xce
$fffffb 03 62         ora $62,sp
$fffffd fa            plx
$fffffe 74 c0         stz $c0,x
