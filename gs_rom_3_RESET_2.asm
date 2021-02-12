$ff7600 8f 00 01 01   sta $010100
$ff7604 8f 01 01 01   sta $010101
$ff7608 c2 20         rep #$20
$ff760a a9 76 ff      lda #$ff76
$ff760d 8f 9a 01 e1   sta $e1019a
$ff7611 a9 5c 29      lda #$295c
$ff7614 8f 98 01 e1   sta $e10198
$ff7618 a9 a9 ff      lda #$ffa9
$ff761b 8f 9e 01 e1   sta $e1019e
$ff761f a9 5c aa      lda #$aa5c
$ff7622 8f 9c 01 e1   sta $e1019c
$ff7626 e2 20         sep #$20
$ff7628 6b            rtl
$ff7629 ad 15 c0      lda $c015
$ff762c 48            pha
$ff762d 8d 07 c0      sta $c007
$ff7630 f4 93 76      pea $7693
$ff7633 98            tya
$ff7634 30 30         bmi $ff7666
$ff7636 08            php
$ff7637 c9 05         cmp #$05
$ff7639 b0 04         bcs $ff763f
$ff763b 0a            asl
$ff763c 28            plp
$ff763d 80 17         bra $ff7656
$ff763f 28            plp
$ff7640 ad fb 04      lda $04fb
$ff7643 29 d6         and #$d6
$ff7645 08            php
$ff7646 98            tya
$ff7647 0a            asl
$ff7648 28            plp
$ff7649 d0 0b         bne $ff7656
$ff764b 18            clc
$ff764c 69 1e         adc #$1e
$ff764e 48            pha
$ff764f 20 73 c9      jsr $c973
$ff7652 20 97 cd      jsr $cd97
$ff7655 68            pla
$ff7656 9b            txy
$ff7657 aa            tax
$ff7658 bf 77 76 ff   lda $ff7677,x
$ff765c 48            pha
$ff765d bf 76 76 ff   lda $ff7676,x
$ff7661 48            pha
$ff7662 bb            tyx
$ff7663 a3 0a         lda $0a,sp
$ff7665 60            rts
$ff7666 08            php
$ff7667 0a            asl
$ff7668 28            plp
$ff7669 9b            txy
$ff766a aa            tax
$ff766b bf b3 76 ff   lda $ff76b3,x
$ff766f 48            pha
$ff7670 bf b2 76 ff   lda $ff76b2,x
$ff7674 80 eb         bra $ff7661
$ff7676 4f c8 13 b4   eor $b413c8
$ff767a 0d b4 4a      ora $4ab4
$ff767d b4 44         ldy $44,x
$ff767f b4 74         ldy $74,x
$ff7681 b3 8f         lda ($8f,sp),y
$ff7683 b3 32         lda ($32,sp),y
$ff7685 b4 ca         ldy $ca,x
$ff7687 b3 6f         lda ($6f,sp),y
$ff7689 b4 7a         ldy $7a,x
$ff768b b3 00         lda ($00,sp),y
$ff768d b4 bd         ldy $bd,x
$ff768f b3 2c         lda ($2c,sp),y
$ff7691 b4 ec         ldy $ec,x
$ff7693 b3 83         lda ($83,sp),y
$ff7695 06 68         asl $68
$ff7697 30 03         bmi $ff769c
$ff7699 8d 06 c0      sta $c006
$ff769c 6b            rtl
$ff769d 00 d3         brk $d3
$ff769f b3 83         lda ($83,sp),y
$ff76a1 cb            wai
$ff76a2 3f cc ce b3   and $b3cecc,x
$ff76a6 6f b4 1a cc   adc $cc1ab4
$ff76aa 97 b4         sta [$b4],y
$ff76ac 5f b4 2c b4   eor $b42cb4,x
$ff76b0 e1 b3         sbc ($b3,x)
$ff76b2 03 aa         ora $aa,sp
$ff76b4 7a            ply
$ff76b5 b8            clv
$ff76b6 c3 b4         cmp $b4,sp
$ff76b8 60            rts
$ff76b9 b7 8a         lda [$8a],y
$ff76bb b0 c5         bcs $ff7682
$ff76bd b2 5f         lda ($5f)
$ff76bf 9d 6e 9d      sta $9d6e,x
$ff76c2 14 a7         trb $a7
$ff76c4 d5 a2         cmp $a2,x
$ff76c6 5a            phy
$ff76c7 ab            plb
$ff76c8 40            rti
$ff76c9 b8            clv
$ff76ca a3 b4         lda $b4,sp
$ff76cc be b0 2e      ldx $2eb0,y
$ff76cf a7 1e         lda [$1e]
$ff76d1 a7 51         lda [$51]
$ff76d3 a8            tay
$ff76d4 f3 9b         sbc ($9b,sp),y
$ff76d6 6e 79 08      ror $0879
$ff76d9 a4 6f         ldy $6f
$ff76db b2 ff         lda ($ff)
$ff76dd 9b            txy
$ff76de 36 9b         rol $9b,x
$ff76e0 95 ac         sta $ac,x
$ff76e2 82 a5 2b      brl $ffa28a
$ff76e5 ad 9b ac      lda $ac9b
$ff76e8 9a            txs
$ff76e9 c9 a3         cmp #$a3
$ff76eb c9 be         cmp #$be
$ff76ed 9c 50 9a      stz $9a50
$ff76f0 ef aa 57 9a   sbc $9a57aa
$ff76f4 78            sei
$ff76f5 a6 f7         ldx $f7
$ff76f7 a9 ce         lda #$ce
$ff76f9 ba            tsx
$ff76fa f2 7b         sbc ($7b)
$ff76fc ec a4 01      cpx $01a4
$ff76ff 8f 2d 7a 23   sta $237a2d
$ff7703 7c ff a7      jmp ($a7ff,x)
$ff7706 f3 a7         sbc ($a7,sp),y
$ff7708 d8            cld
$ff7709 7c 0c 77      jmp ($770c,x)
$ff770c 07 78         ora [$78]
$ff770e 17 78         ora [$78],y
$ff7710 b2 77         lda ($77)
$ff7712 23 77         and $77,sp
$ff7714 8d 77 18      sta $1877
$ff7717 77 c2         adc [$c2],y
$ff7719 30 48         bmi $ff7763
$ff771b a2 03         ldx #$03
$ff771d 19 22 00      ora $0022,y
$ff7720 00 e1         brk $e1
$ff7722 6b            rtl
$ff7723 bb            tyx
$ff7724 b0 1f         bcs $ff7745
$ff7726 a0 c4         ldy #$c4
$ff7728 c4 39         cpy $39
$ff772a d0 04         bne $ff7730
$ff772c a4 38         ldy $38
$ff772e f0 15         beq $ff7745
$ff7730 48            pha
$ff7731 da            phx
$ff7732 29 7f         and #$7f
$ff7734 c9 02         cmp #$02
$ff7736 b0 09         bcs $ff7741
$ff7738 08            php
$ff7739 78            sei
$ff773a 20 60 c4      jsr $c460
$ff773d 20 76 c4      jsr $c476
$ff7740 28            plp
$ff7741 fa            plx
$ff7742 68            pla
$ff7743 80 47         bra $ff778c
$ff7745 eb            xba
$ff7746 a3 07         lda $07,sp
$ff7748 a8            tay
$ff7749 eb            xba
$ff774a 91 28         sta ($28),y
$ff774c a0 05         ldy #$05
$ff774e 84 38         sty $38
$ff7750 08            php
$ff7751 78            sei
$ff7752 22 b2 77 ff   jsr $ff77b2
$ff7756 ad 7c 04      lda $047c
$ff7759 ae 7c 05      ldx $057c
$ff775c a0 05         ldy #$05
$ff775e 20 82 78      jsr $7882
$ff7761 ad fc 04      lda $04fc
$ff7764 ae fc 05      ldx $05fc
$ff7767 a0 0c         ldy #$0c
$ff7769 20 82 78      jsr $7882
$ff776c ad 00 c0      lda $c000
$ff776f 0a            asl
$ff7770 08            php
$ff7771 ad 7c 07      lda $077c
$ff7774 2a            rol
$ff7775 2a            rol
$ff7776 2a            rol
$ff7777 29 03         and #$03
$ff7779 49 03         eor #$03
$ff777b 1a            inc
$ff777c 28            plp
$ff777d a2 00         ldx #$00
$ff777f a0 10         ldy #$10
$ff7781 20 93 78      jsr $7893
$ff7784 a9 8d         lda #$8d
$ff7786 8d 11 02      sta $0211
$ff7789 a2 11         ldx #$11
$ff778b 28            plp
$ff778c 6b            rtl
$ff778d a2 04         ldx #$04
$ff778f bf b7 02 e1   lda $e102b7,x
$ff7793 48            pha
$ff7794 ca            dex
$ff7795 d0 f8         bne $ff778f
$ff7797 68            pla
$ff7798 9d 7c 04      sta $047c,x
$ff779b 68            pla
$ff779c 9d 7c 05      sta $057c,x
$ff779f 8a            txa
$ff77a0 30 04         bmi $ff77a6
$ff77a2 a2 80         ldx #$80
$ff77a4 80 f1         bra $ff7797
$ff77a6 c2 30         rep #$30
$ff77a8 a2 03 1a      ldx #$1a03
$ff77ab 22 00 00 e1   jsr $e10000
$ff77af e2 30         sep #$30
$ff77b1 6b            rtl
$ff77b2 08            php
$ff77b3 78            sei
$ff77b4 ad fc 07      lda $07fc
$ff77b7 8f 97 01 e1   sta $e10197
$ff77bb ad 7c 07      lda $077c
$ff77be 8f 96 01 e1   sta $e10196
$ff77c2 ad fc 04      lda $04fc
$ff77c5 8f 91 01 e1   sta $e10191
$ff77c9 ad fc 05      lda $05fc
$ff77cc 8f 93 01 e1   sta $e10193
$ff77d0 ad 7c 04      lda $047c
$ff77d3 8f 90 01 e1   sta $e10190
$ff77d7 ad 7c 05      lda $057c
$ff77da 8f 92 01 e1   sta $e10192
$ff77de c2 30         rep #$30
$ff77e0 48            pha
$ff77e1 48            pha
$ff77e2 48            pha
$ff77e3 a2 03 17      ldx #$1703
$ff77e6 22 00 00 e1   jsr $e10000
$ff77ea e2 30         sep #$30
$ff77ec 68            pla
$ff77ed 8d fc 07      sta $07fc
$ff77f0 68            pla
$ff77f1 8d 7c 07      sta $077c
$ff77f4 68            pla
$ff77f5 8d fc 04      sta $04fc
$ff77f8 68            pla
$ff77f9 8d fc 05      sta $05fc
$ff77fc 68            pla
$ff77fd 8d 7c 04      sta $047c
$ff7800 68            pla
$ff7801 8d 7c 05      sta $057c
$ff7804 28            plp
$ff7805 18            clc
$ff7806 6b            rtl
$ff7807 c2 30         rep #$30
$ff7809 a9 04 00      lda #$0004
$ff780c 48            pha
$ff780d a2 03 18      ldx #$1803
$ff7810 22 00 00 e1   jsr $e10000
$ff7814 e2 30         sep #$30
$ff7816 6b            rtl
$ff7817 48            pha
$ff7818 ad fc 07      lda $07fc
$ff781b 8f 97 01 e1   sta $e10197
$ff781f ad 7c 06      lda $067c
$ff7822 8f 94 01 e1   sta $e10194
$ff7826 68            pla
$ff7827 29 01         and #$01
$ff7829 aa            tax
$ff782a d0 1e         bne $ff784a
$ff782c ad 78 05      lda $0578
$ff782f 48            pha
$ff7830 ad 78 04      lda $0478
$ff7833 48            pha
$ff7834 ad f8 05      lda $05f8
$ff7837 48            pha
$ff7838 ad f8 04      lda $04f8
$ff783b 48            pha
$ff783c c2 30         rep #$30
$ff783e af ba 02 e1   lda $e102ba
$ff7842 48            pha
$ff7843 af be 02 e1   lda $e102be
$ff7847 48            pha
$ff7848 80 1e         bra $ff7868
$ff784a c2 30         rep #$30
$ff784c af b8 02 e1   lda $e102b8
$ff7850 48            pha
$ff7851 af bc 02 e1   lda $e102bc
$ff7855 48            pha
$ff7856 e2 30         sep #$30
$ff7858 ad 78 05      lda $0578
$ff785b 48            pha
$ff785c ad 78 04      lda $0478
$ff785f 48            pha
$ff7860 ad f8 05      lda $05f8
$ff7863 48            pha
$ff7864 ad f8 04      lda $04f8
$ff7867 48            pha
$ff7868 c2 30         rep #$30
$ff786a a2 03 1c      ldx #$1c03
$ff786d 22 00 00 e1   jsr $e10000
$ff7871 e2 30         sep #$30
$ff7873 af 97 01 e1   lda $e10197
$ff7877 8d fc 07      sta $07fc
$ff787a af 94 01 e1   lda $e10194
$ff787e 8d 7c 06      sta $067c
$ff7881 6b            rtl
$ff7882 e0 80         cpx #$80
$ff7884 90 0d         bcc $ff7893
$ff7886 49 ff         eor #$ff
$ff7888 69 00         adc #$00
$ff788a 48            pha
$ff788b 8a            txa
$ff788c 49 ff         eor #$ff
$ff788e 69 00         adc #$00
$ff7890 aa            tax
$ff7891 68            pla
$ff7892 38            sec
$ff7893 8d 21 02      sta $0221
$ff7896 8e 20 02      stx $0220
$ff7899 a9 ab         lda #$ab
$ff789b 90 02         bcc $ff789f
$ff789d a9 ad         lda #$ad
$ff789f 48            pha
$ff78a0 a9 ac         lda #$ac
$ff78a2 99 01 02      sta $0201,y
$ff78a5 20 ad 78      jsr $78ad
$ff78a8 68            pla
$ff78a9 99 00 02      sta $0200,y
$ff78ac 60            rts
$ff78ad a2 11         ldx #$11
$ff78af a9 00         lda #$00
$ff78b1 18            clc
$ff78b2 2a            rol
$ff78b3 c9 0a         cmp #$0a
$ff78b5 90 02         bcc $ff78b9
$ff78b7 e9 0a         sbc #$0a
$ff78b9 2e 21 02      rol $0221
$ff78bc 2e 20 02      rol $0220
$ff78bf ca            dex
$ff78c0 d0 f0         bne $ff78b2
$ff78c2 09 b0         ora #$b0
$ff78c4 99 00 02      sta $0200,y
$ff78c7 88            dey
$ff78c8 f0 08         beq $ff78d2
$ff78ca c0 07         cpy #$07
$ff78cc f0 04         beq $ff78d2
$ff78ce c0 0e         cpy #$0e
$ff78d0 d0 db         bne $ff78ad
$ff78d2 60            rts
$ff78d3 29 03         and #$03
$ff78d5 00 e2         brk $e2
$ff78d7 20 a8 af      jsr $afa8
$ff78da 36 c0         rol $c0,x
$ff78dc 00 29         brk $29
$ff78de 80 18         bra $ff78f8
$ff78e0 2a            rol
$ff78e1 2a            rol
$ff78e2 aa            tax
$ff78e3 98            tya
$ff78e4 f0 15         beq $ff78fb
$ff78e6 c9 01         cmp #$01
$ff78e8 f0 05         beq $ff78ef
$ff78ea c9 03         cmp #$03
$ff78ec f0 17         beq $ff7905
$ff78ee 3a            dec
$ff78ef a9 80         lda #$80
$ff78f1 0f 36 c0 00   ora $00c036
$ff78f5 8f 36 c0 00   sta $00c036
$ff78f9 80 0a         bra $ff7905
$ff78fb af 36 c0 00   lda $00c036
$ff78ff 29 70         and #$70
$ff7901 8f 36 c0 00   sta $00c036
$ff7905 8a            txa
$ff7906 c2 20         rep #$20
$ff7908 29 03 00      and #$0003
$ff790b 6b            rtl
$ff790c 8b            phb
$ff790d 18            clc
$ff790e a8            tay
$ff790f 29 00 03      and #$0300
$ff7912 d0 22         bne $ff7936
$ff7914 98            tya
$ff7915 e2 30         sep #$30
$ff7917 aa            tax
$ff7918 29 07         and #$07
$ff791a f0 19         beq $ff7935
$ff791c c9 03         cmp #$03
$ff791e f0 34         beq $ff7954
$ff7920 a8            tay
$ff7921 af 2d c0 00   lda $00c02d
$ff7925 6a            ror
$ff7926 88            dey
$ff7927 10 fc         bpl $ff7925
$ff7929 8a            txa
$ff792a 29 08         and #$08
$ff792c b0 05         bcs $ff7933
$ff792e d0 05         bne $ff7935
$ff7930 18            clc
$ff7931 80 03         bra $ff7936
$ff7933 d0 fb         bne $ff7930
$ff7935 38            sec
$ff7936 e2 20         sep #$20
$ff7938 08            php
$ff7939 af 2d c0 00   lda $00c02d
$ff793d 48            pha
$ff793e eb            xba
$ff793f 68            pla
$ff7940 28            plp
$ff7941 c2 30         rep #$30
$ff7943 49 ff 00      eor #$00ff
$ff7946 29 fe fe      and #$fefe
$ff7949 aa            tax
$ff794a a9 00 00      lda #$0000
$ff794d 90 03         bcc $ff7952
$ff794f a9 10 00      lda #$0010
$ff7952 ab            plb
$ff7953 6b            rtl
$ff7954 af 17 c0 00   lda $00c017
$ff7958 2a            rol
$ff7959 80 ce         bra $ff7929
$ff795b 6b            rtl
$ff795c a9 01 8f      lda #$8f01
$ff795f 74 01         stz $01,x
$ff7961 e1 c2         sbc ($c2,x)
$ff7963 30 a2         bmi $ff7907
$ff7965 00 08         brk $08
$ff7967 9b            txy
$ff7968 a9 ff 03      lda #$03ff
$ff796b 54 e0 00      mvn $e0,$00
$ff796e 6b            rtl
$ff796f af 7d 01 e1   lda $e1017d
$ff7973 09 80 8f      ora #$8f80
$ff7976 7d 01 e1      adc $e101,x
$ff7979 af 7d 01 e1   lda $e1017d
$ff797d 48            pha
$ff797e 0a            asl
$ff797f 30 15         bmi $ff7996
$ff7981 c2 30         rep #$30
$ff7983 f4 ff 00      pea $00ff
$ff7986 f4 9e 79      pea $799e
$ff7989 a2 05 0f      ldx #$0f05
$ff798c 22 00 00 e1   jsr $e10000
$ff7990 22 04 00 fe   jsr $fe0004
$ff7994 e2 30         sep #$30
$ff7996 68            pla
$ff7997 09 40         ora #$40
$ff7999 8f 7d 01 e1   sta $e1017d
$ff799d 60            rts
$ff799e a2 79         ldx #$79
$ff79a0 ff 00 0d d6   sbc $d60d00,x
$ff79a4 e9 f3         sbc #$f3
$ff79a6 e9 f4         sbc #$f4
$ff79a8 a0 cd         ldy #$cd
$ff79aa ef ee e9 f4   sbc $f4e9ee
$ff79ae ef f2 b8 79   sbc $79b8f2
$ff79b2 ff 00 24 7a   sbc $7a2400,x
$ff79b6 ff 00 e2 30   sbc $30e200,x
$ff79ba 78            sei
$ff79bb f4 00 00      pea $0000
$ff79be ab            plb
$ff79bf ab            plb
$ff79c0 a2 02         ldx #$02
$ff79c2 bd f8 03      lda $03f8,x
$ff79c5 48            pha
$ff79c6 ca            dex
$ff79c7 10 f9         bpl $ff79c2
$ff79c9 a9 4c         lda #$4c
$ff79cb 8d f8 03      sta $03f8
$ff79ce a9 fb         lda #$fb
$ff79d0 8d f9 03      sta $03f9
$ff79d3 9c fa 03      stz $03fa
$ff79d6 a2 08         ldx #$08
$ff79d8 b5 f7         lda $f7,x
$ff79da 48            pha
$ff79db ca            dex
$ff79dc 10 fa         bpl $ff79d8
$ff79de 8b            phb
$ff79df 4b            phk
$ff79e0 ab            plb
$ff79e1 a2 08         ldx #$08
$ff79e3 bd 25 7a      lda $7a25,x
$ff79e6 95 f7         sta $f7,x
$ff79e8 ca            dex
$ff79e9 10 f8         bpl $ff79e3
$ff79eb ab            plb
$ff79ec 4b            phk
$ff79ed 68            pla
$ff79ee 85 ff         sta $ff
$ff79f0 ad 68 c0      lda $c068
$ff79f3 48            pha
$ff79f4 20 ee a9      jsr $a9ee
$ff79f7 a9 f2         lda #$f2
$ff79f9 20 80 c0      jsr $c080
$ff79fc 68            pla
$ff79fd 8d 68 c0      sta $c068
$ff7a00 38            sec
$ff7a01 fb            xce
$ff7a02 5c f7 00 00   jmp $0000f7
$ff7a06 68            pla
$ff7a07 68            pla
$ff7a08 f4 00 00      pea $0000
$ff7a0b ab            plb
$ff7a0c ab            plb
$ff7a0d 18            clc
$ff7a0e fb            xce
$ff7a0f 58            cli
$ff7a10 a2 00         ldx #$00
$ff7a12 68            pla
$ff7a13 95 f7         sta $f7,x
$ff7a15 e8            inx
$ff7a16 e0 09         cpx #$09
$ff7a18 90 f8         bcc $ff7a12
$ff7a1a 68            pla
$ff7a1b 9d ef 03      sta $03ef,x
$ff7a1e e0 0b         cpx #$0b
$ff7a20 90 f3         bcc $ff7a15
$ff7a22 c2 30         rep #$30
$ff7a24 6b            rtl
$ff7a25 58            cli
$ff7a26 4c 69 ff      jmp $ff69
$ff7a29 78            sei
$ff7a2a 5c 06 7a 00   jmp $007a06
$ff7a2e 8b            phb
$ff7a2f 20 82 f8      jsr $f882
$ff7a32 ad 8e 01      lda $018e
$ff7a35 c9 06 90      cmp #$9006
$ff7a38 0c ad 7c      tsb $7cad
$ff7a3b 01 d0         ora ($d0,x)
$ff7a3d 07 a9         ora [$a9]
$ff7a3f 20 0c 35      jsr $350c
$ff7a42 c0 80 05      cpy #$0580
$ff7a45 a9 20 1c      lda #$1c20
$ff7a48 35 c0         and $c0,x
$ff7a4a ab            plb
$ff7a4b 60            rts
$ff7a4c 08            php
$ff7a4d e2 30         sep #$30
$ff7a4f ad 30 01      lda $0130
$ff7a52 0a            asl
$ff7a53 0d 31 01      ora $0131
$ff7a56 0a            asl
$ff7a57 0a            asl
$ff7a58 0a            asl
$ff7a59 0a            asl
$ff7a5a 48            pha
$ff7a5b ad 2a 01      lda $012a
$ff7a5e 29 cf         and #$cf
$ff7a60 03 01         ora $01,sp
$ff7a62 7a            ply
$ff7a63 8d 2a 01      sta $012a
$ff7a66 28            plp
$ff7a67 60            rts
$ff7a68 f4 e1 e1      pea $e1e1
$ff7a6b ab            plb
$ff7a6c ab            plb
$ff7a6d ad 1c c0      lda $c01c
$ff7a70 10 07         bpl $ff7a79
$ff7a72 ad 2d 01      lda $012d
$ff7a75 09 40         ora #$40
$ff7a77 80 05         bra $ff7a7e
$ff7a79 ad 2d 01      lda $012d
$ff7a7c 29 bf         and #$bf
$ff7a7e 8d 68 c0      sta $c068
$ff7a81 ad 2e 01      lda $012e
$ff7a84 0a            asl
$ff7a85 08            php
$ff7a86 ad 36 c0      lda $c036
$ff7a89 0a            asl
$ff7a8a 28            plp
$ff7a8b 6a            ror
$ff7a8c 29 9f         and #$9f
$ff7a8e 8d 36 c0      sta $c036
$ff7a91 ad 2e 01      lda $012e
$ff7a94 29 5f         and #$5f
$ff7a96 8d 35 c0      sta $c035
$ff7a99 ad 32 01      lda $0132
$ff7a9c 6a            ror
$ff7a9d ad 2a 01      lda $012a
$ff7aa0 09 04         ora #$04
$ff7aa2 eb            xba
$ff7aa3 ad 2b 01      lda $012b
$ff7aa6 c2 30         rep #$30
$ff7aa8 48            pha
$ff7aa9 ad 28 01      lda $0128
$ff7aac 5b            tcd
$ff7aad ac 24 01      ldy $0124
$ff7ab0 ae 22 01      ldx $0122
$ff7ab3 ad 20 01      lda $0120
$ff7ab6 ab            plb
$ff7ab7 90 09         bcc $ff7ac2
$ff7ab9 28            plp
$ff7aba 90 03         bcc $ff7abf
$ff7abc 38            sec
$ff7abd 80 07         bra $ff7ac6
$ff7abf 38            sec
$ff7ac0 80 09         bra $ff7acb
$ff7ac2 28            plp
$ff7ac3 90 05         bcc $ff7aca
$ff7ac5 18            clc
$ff7ac6 fb            xce
$ff7ac7 38            sec
$ff7ac8 80 03         bra $ff7acd
$ff7aca 18            clc
$ff7acb fb            xce
$ff7acc 18            clc
$ff7acd 60            rts
$ff7ace 8f 20 01 e1   sta $e10120
$ff7ad2 08            php
$ff7ad3 e2 20         sep #$20
$ff7ad5 68            pla
$ff7ad6 29 fb 8f      and #$8ffb
$ff7ad9 2a            rol
$ff7ada 01 e1         ora ($e1,x)
$ff7adc 18            clc
$ff7add fb            xce
$ff7ade d8            cld
$ff7adf e2 20         sep #$20
$ff7ae1 eb            xba
$ff7ae2 8f 21 01 e1   sta $e10121
$ff7ae6 a9 00 2a      lda #$2a00
$ff7ae9 8f 32 01 e1   sta $e10132
$ff7aed 8b            phb
$ff7aee 68            pla
$ff7aef 8f 2b 01 e1   sta $e1012b
$ff7af3 c2 30         rep #$30
$ff7af5 20 82 f8      jsr $f882
$ff7af8 7b            tdc
$ff7af9 8d 28 01      sta $0128
$ff7afc 8e 22 01      stx $0122
$ff7aff 8c 24 01      sty $0124
$ff7b02 e2 30         sep #$30
$ff7b04 ad 2a 01      lda $012a
$ff7b07 29 30         and #$30
$ff7b09 4a            lsr
$ff7b0a 20 7b f8      jsr $f87b
$ff7b0d 8d 30 01      sta $0130
$ff7b10 a9 00         lda #$00
$ff7b12 2a            rol
$ff7b13 8d 31 01      sta $0131
$ff7b16 ad 68 c0      lda $c068
$ff7b19 8d 2d 01      sta $012d
$ff7b1c 29 04         and #$04
$ff7b1e 4a            lsr
$ff7b1f 4a            lsr
$ff7b20 8d 2f 01      sta $012f
$ff7b23 ad 36 c0      lda $c036
$ff7b26 0a            asl
$ff7b27 08            php
$ff7b28 ad 35 c0      lda $c035
$ff7b2b 0a            asl
$ff7b2c 28            plp
$ff7b2d 6a            ror
$ff7b2e 8d 2e 01      sta $012e
$ff7b31 29 5f         and #$5f
$ff7b33 8d 35 c0      sta $c035
$ff7b36 ad 36 c0      lda $c036
$ff7b39 09 80         ora #$80
$ff7b3b 8d 36 c0      sta $c036
$ff7b3e f4 00 00      pea $0000
$ff7b41 2b            pld
$ff7b42 f4 00 00      pea $0000
$ff7b45 ab            plb
$ff7b46 ab            plb
$ff7b47 60            rts
$ff7b48 af 00 01 01   lda $010100
$ff7b4c 8f cf 01 e1   sta $e101cf
$ff7b50 3b            tsc
$ff7b51 1a            inc
$ff7b52 1a            inc
$ff7b53 8f 00 01 01   sta $010100
$ff7b57 60            rts
$ff7b58 00 00         brk $00
$ff7b5a 00 00         brk $00
$ff7b5c 00 00         brk $00
$ff7b5e 00 00         brk $00
$ff7b60 00 00         brk $00
$ff7b62 00 00         brk $00
$ff7b64 00 00         brk $00
$ff7b66 00 00         brk $00
$ff7b68 00 00         brk $00
$ff7b6a 00 00         brk $00
$ff7b6c 00 00         brk $00
$ff7b6e 00 00         brk $00
$ff7b70 0f a0 c1 f0   ora $f0c1a0
$ff7b74 f0 ec         beq $ff7b62
$ff7b76 e5 a0         sbc $a0
$ff7b78 c9 c9         cmp #$c9
$ff7b7a e7 f3         sbc [$f3]
$ff7b7c 00 43         brk $43
$ff7b7e 6f 70 79 72   adc $727970
$ff7b82 69 67         adc #$67
$ff7b84 68            pla
$ff7b85 74 20         stz $20,x
$ff7b87 41 70         eor ($70,x)
$ff7b89 70 6c         bvs $ff7bf7
$ff7b8b 65 20         adc $20
$ff7b8d 43 6f         eor $6f,sp
$ff7b8f 6d 70 75      adc $7570
$ff7b92 74 65         stz $65,x
$ff7b94 72 2c         adc ($2c)
$ff7b96 20 49 6e      jsr $6e49
$ff7b99 63 2e         adc $2e,sp
$ff7b9b 20 31 39      jsr $3931
$ff7b9e 37 37         and [$37],y
$ff7ba0 2d 31 39      and $3931
$ff7ba3 38            sec
$ff7ba4 b9 00 00      lda $0000,y
$ff7ba7 c6 e5         dec $e5
$ff7ba9 f2 ee         sbc ($ee)
$ff7bab a0 c2         ldy #$c2
$ff7bad e1 e3         sbc ($e3,x)
$ff7baf e8            inx
$ff7bb0 ed e1 ee      sbc $eee1
$ff7bb3 a0 ca         ldy #$ca
$ff7bb5 f2 ae         sbc ($ae)
$ff7bb7 00 00         brk $00
$ff7bb9 b1 b9         lda ($b9),y
$ff7bbb b7 b7         lda [$b7],y
$ff7bbd a0 cd         ldy #$cd
$ff7bbf e9 e3         sbc #$e3
$ff7bc1 f2 ef         sbc ($ef)
$ff7bc3 f3 ef         sbc ($ef,sp),y
$ff7bc5 e6 f4         inc $f4
$ff7bc7 0a            asl
$ff7bc8 a0 c1         ldy #$c1
$ff7bca ec ec a0      cpx $a0ec
$ff7bcd d2 e9         cmp ($e9)
$ff7bcf e7 e8         sbc [$e8]
$ff7bd1 f4 f3 a0      pea $a0f3
$ff7bd4 d2 e5         cmp ($e5)
$ff7bd6 f3 e5         sbc ($e5,sp),y
$ff7bd8 f2 f6         sbc ($f6)
$ff7bda e5 e4         sbc $e4
$ff7bdc ae 00 0d      ldx $0d00
$ff7bdf a0 d2         ldy #$d2
$ff7be1 cf cd a0 d6   cmp $d6a0cd
$ff7be5 e5 f2         sbc $f2
$ff7be7 f3 e9         sbc ($e9,sp),y
$ff7be9 ef ee a0 00   sbc $00a0ee
$ff7bed 00 13         brk $13
$ff7bef 00 00         brk $00
$ff7bf1 15 17         ora $17,x
$ff7bf3 20 2e 9c      jsr $9c2e
$ff7bf6 a2 00         ldx #$00
$ff7bf8 c2 20         rep #$20
$ff7bfa 18            clc
$ff7bfb 8b            phb
$ff7bfc 4b            phk
$ff7bfd ab            plb
$ff7bfe bc ed 7b      ldy $7bed,x
$ff7c01 ab            plb
$ff7c02 8a            txa
$ff7c03 69 f5 00      adc #$00f5
$ff7c06 da            phx
$ff7c07 48            pha
$ff7c08 e2 30         sep #$30
$ff7c0a 20 bb 93      jsr $93bb
$ff7c0d c2 30         rep #$30
$ff7c0f 68            pla
$ff7c10 20 85 c0      jsr $c085
$ff7c13 fa            plx
$ff7c14 e8            inx
$ff7c15 e0 06 90      cpx #$9006
$ff7c18 df e2 30 20   cmp $2030e2,x
$ff7c1c 91 ac         sta ($ac),y
$ff7c1e ad 59 fb      lda $fb59
$ff7c21 4c 99 a9      jmp $a999
$ff7c24 da            phx
$ff7c25 a2 0a bf      ldx #$bf0a
$ff7c28 71 7b         adc ($7b),y
$ff7c2a ff 9f 0e 04   sbc $040e9f,x
$ff7c2e 00 ca         brk $ca
$ff7c30 d0 f5         bne $ff7c27
$ff7c32 fa            plx
$ff7c33 60            rts
$ff7c34 5c 61 b5 ff   jmp $ffb561
$ff7c38 5c d3 78 ff   jmp $ff78d3
$ff7c3c 5c 0c 79 ff   jmp $ff790c
$ff7c40 5c 00 00 ff   jmp $ff0000
$ff7c44 5c 03 00 ff   jmp $ff0003
$ff7c48 5c 06 00 ff   jmp $ff0006
$ff7c4c 5c 18 ba ff   jmp $ffba18
$ff7c50 5c 18 ba ff   jmp $ffba18
$ff7c54 5c 6c bc ff   jmp $ffbc6c
$ff7c58 5c 5e bc ff   jmp $ffbc5e
$ff7c5c 5c 5e bc ff   jmp $ffbc5e
$ff7c60 5c 33 a5 ff   jmp $ffa533
$ff7c64 5c 4e b9 ff   jmp $ffb94e
$ff7c68 5c 70 b9 ff   jmp $ffb970
$ff7c6c 5c 1e ba ff   jmp $ffba1e
$ff7c70 5c da b9 ff   jmp $ffb9da
$ff7c74 5c ac 8f ff   jmp $ff8fac
$ff7c78 5c 7a bb ff   jmp $ffbb7a
$ff7c7c 5c e7 c4 ff   jmp $ffc4e7
$ff7c80 5c eb c4 ff   jmp $ffc4eb
$ff7c84 5c 19 fe 00   jmp $00fe19
$ff7c88 5c 9e 8e ff   jmp $ff8e9e
$ff7c8c 00 a0         brk $a0
$ff7c8e d0 f2         bne $ff7c82
$ff7c90 ef ea e5 e3   sbc $e3e5ea
$ff7c94 f4 a0 cd      pea $cda0
$ff7c97 e1 ee         sbc ($ee,x)
$ff7c99 e1 e7         sbc ($e7,x)
$ff7c9b e5 ed         sbc $ed
$ff7c9d e5 ee         sbc $ee
$ff7c9f f4 02 a0      pea $a002
$ff7ca2 c8            iny
$ff7ca3 e1 f2         sbc ($f2,x)
$ff7ca5 e4 f7         cpx $f7
$ff7ca7 e1 f2         sbc ($f2,x)
$ff7ca9 e5 8d         sbc $8d
$ff7cab 04 a0         tsb $a0
$ff7cad ca            dex
$ff7cae ef f3 e5 e6   sbc $e6e5f3
$ff7cb2 a0 c6 f2      ldy #$f2c6
$ff7cb5 e9 e5 e4      sbc #$e4e5
$ff7cb8 ed e1 ee      sbc $eee1
$ff7cbb 06 a0         asl $a0
$ff7cbd cc e1 f2      cpy $f2e1
$ff7cc0 f2 f9         sbc ($f9)
$ff7cc2 a0 d4 e8      ldy #$e8d4
$ff7cc5 ef ed f0 f3   sbc $f3f0ed
$ff7cc9 ef ee 8d 04   sbc $048dee
$ff7ccd a0 c6 e5      ldy #$e5c6
$ff7cd0 f2 ee         sbc ($ee)
$ff7cd2 a0 c2 e1      ldy #$e1c2
$ff7cd5 e3 e8         sbc $e8,sp
$ff7cd7 ed e1 ee      sbc $eee1
$ff7cda ac ca f2      ldy $f2ca
$ff7cdd ae 04 a0      ldx $a004
$ff7ce0 c4 e1         cpy $e1
$ff7ce2 ee a0 c8      inc $c8a0
$ff7ce5 e9 ec ec      sbc #$ecec
$ff7ce8 ed e1 ee      sbc $eee1
$ff7ceb 8d 04 a0      sta $a004
$ff7cee c1 ee         cmp ($ee,x)
$ff7cf0 ee e5 a0      inc $a0e5
$ff7cf3 c2 e1         rep #$e1
$ff7cf5 e3 e8         sbc $e8,sp
$ff7cf7 f4 ef ec      pea $ecef
$ff7cfa e4 07         cpx $07
$ff7cfc a0 d2 e1      ldy #$e1d2
$ff7cff f9 a0 ca      sbc $caa0,y
$ff7d02 e1 e1         sbc ($e1,x)
$ff7d04 e6 e1         inc $e1
$ff7d06 f2 e9         sbc ($e9)
$ff7d08 8d 04 a0      sta $a004
$ff7d0b ca            dex
$ff7d0c e1 ee         sbc ($ee,x)
$ff7d0e e5 a0         sbc $a0
$ff7d10 ca            dex
$ff7d11 e1 e3         sbc ($e3,x)
$ff7d13 ef e2 f3 ef   sbc $eff3e2
$ff7d17 ee 07 a0      inc $a007
$ff7d1a c7 e1         cmp [$e1]
$ff7d1c f2 f9         sbc ($f9)
$ff7d1e a0 cb ee      ldy #$eecb
$ff7d21 f5 f4         sbc $f4,x
$ff7d23 f3 ef         sbc ($ef,sp),y
$ff7d25 ee 8d 04      inc $048d
$ff7d28 a0 c8 e1      ldy #$e1c8
$ff7d2b f2 f6         sbc ($f6)
$ff7d2d e5 f9         sbc $f9
$ff7d2f a0 cc e5      ldy #$e5cc
$ff7d32 e8            inx
$ff7d33 f4 ed e1      pea $e1ed
$ff7d36 ee 06 a0      inc $a006
$ff7d39 ca            dex
$ff7d3a ef e8 ee a0   sbc $a0eee8
$ff7d3e cd e1 e3      cmp $e3e1
$ff7d41 d0 e8         bne $ff7d2b
$ff7d43 e5 e5         sbc $e5
$ff7d45 8d 04 a0      sta $a004
$ff7d48 c4 ae         cpy $ae
$ff7d4a a0 c4 f2      ldy #$f2c4
$ff7d4d e5 f3         sbc $f3
$ff7d4f e2 e1         sep #$e1
$ff7d51 e3 e8         sbc $e8,sp
$ff7d53 09 a0 d2      ora #$d2a0
$ff7d56 ef e2 a0 cd   sbc $cda0e2
$ff7d5a ef ef f2 e5   sbc $e5f2ef
$ff7d5e 8d 04 a0      sta $a004
$ff7d61 cc ae a0      cpy $a0ae
$ff7d64 cb            wai
$ff7d65 f5 f2         sbc $f2,x
$ff7d67 e9 e8 e1      sbc #$e1e8
$ff7d6a f2 e1         sbc ($e1)
$ff7d6c 09 a0 ca      ora #$caa0
$ff7d6f e1 f9         sbc ($f9,x)
$ff7d71 a0 d2 e9      ldy #$e9d2
$ff7d74 e3 eb         sbc $eb,sp
$ff7d76 e1 f2         sbc ($f2,x)
$ff7d78 e4 8d         cpx $8d
$ff7d7a 18            clc
$ff7d7b a0 d4 ef      ldy #$efd4
$ff7d7e e4 e4         cpx $e4
$ff7d80 a0 d2 f5      ldy #$f5d2
$ff7d83 e4 e9         cpx $e9
$ff7d85 ee 00 00      inc $0000
$ff7d88 c6 e9         dec $e9
$ff7d8a f2 ed         sbc ($ed)
$ff7d8c f7 e1         sbc [$e1],y
$ff7d8e f2 e5         sbc ($e5)
$ff7d90 08            php
$ff7d91 a0 d4 ef      ldy #$efd4
$ff7d94 ef ec f3 08   sbc $08f3ec
$ff7d98 a0 c4 e9      ldy #$e9c4
$ff7d9b e1 e7         sbc ($e7,x)
$ff7d9d ee ef f3      inc $f3ef
$ff7da0 f4 e9 e3      pea $e3e9
$ff7da3 f3 a0         sbc ($a0,sp),y
$ff7da5 cd e9 e3      cmp $e3e9
$ff7da8 e8            inx
$ff7da9 e1 e5         sbc ($e5,x)
$ff7dab ec a0 c1      cpx $c1a0
$ff7dae f3 eb         sbc ($eb,sp),y
$ff7db0 e9 ee f3      sbc #$f3ee
$ff7db3 02 a0         cop $a0
$ff7db5 d3 f4         cmp ($f4,sp),y
$ff7db7 e5 f6         sbc $f6
$ff7db9 e5 ee         sbc $ee
$ff7dbb a0 c7 ec      ldy #$ecc7
$ff7dbe e1 f3         sbc ($f3,x)
$ff7dc0 f3 01         sbc ($01,sp),y
$ff7dc2 a0 c2 ae      ldy #$aec2
$ff7dc5 d3 ae         cmp ($ae,sp),y
$ff7dc7 a0 d3 ef      ldy #$efd3
$ff7dca e8            inx
$ff7dcb 8d a0 c7      sta $c7a0
$ff7dce f5 f3         sbc $f3,x
$ff7dd0 a0 c1 ee      ldy #$eec1
$ff7dd3 e4 f2         cpx $f2
$ff7dd5 e1 e4         sbc ($e4,x)
$ff7dd7 e5 05         sbc $05
$ff7dd9 a0 c5 e1      ldy #$e1c5
$ff7ddc e7 ec         sbc [$ec]
$ff7dde e5 a0         sbc $a0
$ff7de0 c2 e5         rep #$e5
$ff7de2 f2 ee         sbc ($ee)
$ff7de4 f3 02         sbc ($02,sp),y
$ff7de6 a0 d2 e1      ldy #$e1d2
$ff7de9 ee e4 f9      inc $f9e4
$ff7dec a0 c3 e1      ldy #$e1c3
$ff7def f2 f2         sbc ($f2)
$ff7df1 a0 ca ef      ldy #$efca
$ff7df4 e8            inx
$ff7df5 ee a0 c1      inc $c1a0
$ff7df8 f2 eb         sbc ($eb)
$ff7dfa ec e5 f9      cpx $f9e5
$ff7dfd 05 a0         ora $a0
$ff7dff c1 f2         cmp ($f2,x)
$ff7e01 f4 a0 c3      pea $c3a0
$ff7e04 e1 e2         sbc ($e2,x)
$ff7e06 f2 e1         sbc ($e1)
$ff7e08 ec 03 a0      cpx $a003
$ff7e0b cb            wai
$ff7e0c e1 f2         sbc ($f2,x)
$ff7e0e ec a0 c7      cpx $c7a0
$ff7e11 f2 e1         sbc ($e1)
$ff7e13 e2 e5         sep #$e5
$ff7e15 a0 c3 ae      ldy #$aec3
$ff7e18 cb            wai
$ff7e19 ae a0 c2      ldx $c2a0
$ff7e1c ef a0 a8 ca   sbc $caa8a0
$ff7e20 ef e5 a9 03   sbc $03a9e5
$ff7e24 a0 cd e1      ldy #$e1cd
$ff7e27 f4 f4 a0      pea $a0f4
$ff7e2a c4 e5         cpy $e5
$ff7e2c ee ed e1      inc $e1ed
$ff7e2f ee 02 a0      inc $a002
$ff7e32 ca            dex
$ff7e33 a0 d2 e5      ldy #$e5d2
$ff7e36 f9 ee ef      sbc $efee,y
$ff7e39 ec e4 f3      cpx $f3e4
$ff7e3c a0 d0 e5      ldy #$e5d0
$ff7e3f f4 e5 f2      pea $f2e5
$ff7e42 a0 c2 e1      ldy #$e1c2
$ff7e45 f5 ed         sbc $ed,x
$ff7e47 06 a0         asl $a0
$ff7e49 d3 f5         cmp ($f5,sp),y
$ff7e4b e5 a0         sbc $a0
$ff7e4d c4 f5         cpy $f5
$ff7e4f ed ef ee      sbc $eeef
$ff7e52 f4 8d a0      pea $a08d
$ff7e55 d2 ef         cmp ($ef)
$ff7e57 e2 a0         sep #$a0
$ff7e59 d4 f5         pei ($f5)
$ff7e5b f2 ee         sbc ($ee)
$ff7e5d e5 f2         sbc $f2
$ff7e5f a0 a0 a0      ldy #$a0a0
$ff7e62 03 a0         ora $a0,sp
$ff7e64 c3 e8         cmp $e8,sp
$ff7e66 e5 f2         sbc $f2
$ff7e68 f9 ec a0      sbc $a0ec,y
$ff7e6b c5 f7         cmp $f7
$ff7e6d f9 8d a0      sbc $a08d,y
$ff7e70 cc e1 f5      cpy $f5e1
$ff7e73 f2 e1         sbc ($e1)
$ff7e75 a0 c4 f2      ldy #$f2c4
$ff7e78 e9 f4 f4      sbc #$f4f4
$ff7e7b ec e5 f2      cpx $f2e5
$ff7e7e 02 a0         cop $a0
$ff7e80 c4 e1         cpy $e1
$ff7e82 f6 e5         inc $e5,x
$ff7e84 a0 c7 ef      ldy #$efc7
$ff7e87 ef e4 8d a0   sbc $a08de4
$ff7e8b d4 e9         pei ($e9)
$ff7e8d ed a0 c8      sbc $c8a0
$ff7e90 e1 f2         sbc ($f2,x)
$ff7e92 f2 e9         sbc ($e9)
$ff7e94 ee e7 f4      inc $f4e7
$ff7e97 ef ee 02 a0   sbc $a002ee
$ff7e9b cb            wai
$ff7e9c e5 ee         sbc $ee
$ff7e9e f4 ef ee      pea $eeef
$ff7ea1 a0 c8 e1      ldy #$e1c8
$ff7ea4 ee f3 ef      inc $eff3
$ff7ea7 ee 8d a0      inc $a08d
$ff7eaa c5 e4         cmp $e4
$ff7eac a0 cc e1      ldy #$e1cc
$ff7eaf e9 0a a0      sbc #$a00a
$ff7eb2 d3 f5         cmp ($f5,sp),y
$ff7eb4 eb            xba
$ff7eb5 e9 a0 cc      sbc #$cca0
$ff7eb8 e5 e5         sbc $e5
$ff7eba 8d a0 d2      sta $d2a0
$ff7ebd e1 f9         sbc ($f9,x)
$ff7ebf a0 cd ef      ldy #$efcd
$ff7ec2 ee f4 e1      inc $e1f4
$ff7ec5 e7 ee         sbc [$ee]
$ff7ec7 e5 04         sbc $04
$ff7ec9 a0 c3 ec      ldy #$ecc3
$ff7ecc e1 f9         sbc ($f9,x)
$ff7ece f4 ef ee      pea $eeef
$ff7ed1 a0 cc e5      ldy #$e5cc
$ff7ed4 f7 e9         sbc [$e9],y
$ff7ed6 f3 8d         sbc ($8d,sp),y
$ff7ed8 a0 d0 e5      ldy #$e5d0
$ff7edb f4 e5 f2      pea $f2e5
$ff7ede a0 d2 e9      ldy #$e9d2
$ff7ee1 e3 e8         sbc $e8,sp
$ff7ee3 e5 f2         sbc $f2
$ff7ee5 f4 03 a0      pea $a003
$ff7ee8 c2 e5         rep #$e5
$ff7eea ee ee e5      inc $e5ee
$ff7eed f4 a0 cd      pea $cda0
$ff7ef0 e1 f2         sbc ($f2,x)
$ff7ef2 eb            xba
$ff7ef3 f3 8d         sbc ($8d,sp),y
$ff7ef5 a0 c7 f2      ldy #$f2c7
$ff7ef8 e5 e7         sbc $e7
$ff7efa a0 d3 e5      ldy #$e5d3
$ff7efd e9 f4 fa      sbc #$faf4
$ff7f00 06 a0         asl $a0
$ff7f02 c1 ee         cmp ($ee,x)
$ff7f04 e4 f9         cpx $f9
$ff7f06 a0 d3 f4      ldy #$f4d3
$ff7f09 e1 e4         sbc ($e4,x)
$ff7f0b ec e5 f2      cpx $f2e5
$ff7f0e a0 a0 8d      ldy #$8da0
$ff7f11 a0 d2 e9      ldy #$e9d2
$ff7f14 e3 e8         sbc $e8,sp
$ff7f16 a0 d7 e9      ldy #$e9d7
$ff7f19 ec ec e9      cpx $e9ec
$ff7f1c e1 ed         sbc ($ed,x)
$ff7f1e f3 03         sbc ($03,sp),y
$ff7f20 a0 cd e5      ldy #$e5cd
$ff7f23 ee f3 e3      inc $e3f3
$ff7f26 e8            inx
$ff7f27 8d 11 a0      sta $a011
$ff7f2a c4 e1         cpy $e1
$ff7f2c ee a0 cf      inc $cfa0
$ff7f2f ec e9 f6      cpx $f6e9
$ff7f32 e5 f2         sbc $f2
$ff7f34 8d 11 a0      sta $a011
$ff7f37 cb            wai
$ff7f38 ef ee f3 f4   sbc $f4f3ee
$ff7f3c e1 ee         sbc ($ee,x)
$ff7f3e f4 e9 ee      pea $eee9
$ff7f41 a0 cf f4      ldy #$f4cf
$ff7f44 e8            inx
$ff7f45 ed e5 f2      sbc $f2e5
$ff7f48 8d 11 a0      sta $a011
$ff7f4b cf f7 e5 ee   cmp $eee5f7
$ff7f4f a0 d2 f5      ldy #$f5d2
$ff7f52 e2 e9         sep #$e9
$ff7f54 ee 8d 11      inc $118d
$ff7f57 a0 c8 e1      ldy #$e1c8
$ff7f5a f2 f2         sbc ($f2)
$ff7f5c f9 a0 d9      sbc $d9a0,y
$ff7f5f e5 e5         sbc $e5
$ff7f61 00 c2         brk $c2
$ff7f63 10 a2         bpl $ff7f07
$ff7f65 70 17         bvs $ff7f7e
$ff7f67 ca            dex
$ff7f68 f0 09         beq $ff7f73
$ff7f6a ad 26 c0      lda $c026
$ff7f6d ad 27 c0      lda $c027
$ff7f70 6a            ror
$ff7f71 b0 f4         bcs $ff7f67
$ff7f73 a9 07 20      lda #$2007
$ff7f76 72 81         adc ($81)
$ff7f78 ad 26 c0      lda $c026
$ff7f7b 20 f9 80      jsr $80f9
$ff7f7e 20 10 81      jsr $8110
$ff7f81 20 44 81      jsr $8144
$ff7f84 20 63 81      jsr $8163
$ff7f87 9c d7 0f      stz $0fd7
$ff7f8a 9c d6 0f      stz $0fd6
$ff7f8d a9 ff 20      lda #$20ff
$ff7f90 a8            tay
$ff7f91 fc ad 61      jsr ($61ad,x)
$ff7f94 c0 8d 40      cpy #$408d
$ff7f97 01 2a         ora ($2a,x)
$ff7f99 ad 62 c0      lda $c062
$ff7f9c 8d 41 01      sta $0141
$ff7f9f 10 0b         bpl $ff7fac
$ff7fa1 90 09         bcc $ff7fac
$ff7fa3 e2 30         sep #$30
$ff7fa5 20 2e 9c      jsr $9c2e
$ff7fa8 5c 00 64 ff   jmp $ff6400
$ff7fac ad 46 c0      lda $c046
$ff7faf 30 f2         bmi $ff7fa3
$ff7fb1 ad 40 01      lda $0140
$ff7fb4 0d 41 01      ora $0141
$ff7fb7 10 13         bpl $ff7fcc
$ff7fb9 a9 00         lda #$00
$ff7fbb 8f f4 03 00   sta $0003f4
$ff7fbf 8f f3 03 00   sta $0003f3
$ff7fc3 ad 25 c0      lda $c025
$ff7fc6 6a            ror
$ff7fc7 90 03         bcc $ff7fcc
$ff7fc9 9c ff 15      stz $15ff
$ff7fcc a9 73         lda #$73
$ff7fce 20 72 81      jsr $8172
$ff7fd1 c2 30         rep #$30
$ff7fd3 a9 ba ff      lda #$ffba
$ff7fd6 8d 4e 00      sta $004e
$ff7fd9 8d 52 00      sta $0052
$ff7fdc a9 5c 18      lda #$185c
$ff7fdf 8d 4c 00      sta $004c
$ff7fe2 8d 50 00      sta $0050
$ff7fe5 e2 20         sep #$20
$ff7fe7 a9 0d 20      lda #$200d
$ff7fea 72 81         adc ($81)
$ff7fec 20 96 81      jsr $8196
$ff7fef 8d 8e 01      sta $018e
$ff7ff2 c9 06 90      cmp #$9006
$ff7ff5 29 2c 36      and #$362c
$ff7ff8 c0 50 62      cpy #$6250
$ff7ffb 9c a5 01      stz $01a5
$ff7ffe a9 09 20      lda #$2009
$ff8001 72 81         adc ($81)
$ff8003 a9 e8 20      lda #$20e8
$ff8006 72 81         adc ($81)
$ff8008 a9 00 20      lda #$2000
$ff800b 72 81         adc ($81)
$ff800d 20 96 81      jsr $8196
$ff8010 29 c0 8d      and #$8dc0
$ff8013 a5 01         lda $01
$ff8015 a9 40 8d      lda #$8d40
$ff8018 b1 01         lda ($01),y
$ff801a 1c 36 c0      trb $c036
$ff801d 80 2e         bra $ff804d
$ff801f 9c b1 01      stz $01b1
$ff8022 9c a5 01      stz $01a5
$ff8025 a9 09 20      lda #$2009
$ff8028 72 81         adc ($81)
$ff802a a9 51 20      lda #$2051
$ff802d 72 81         adc ($81)
$ff802f a9 00 20      lda #$2000
$ff8032 72 81         adc ($81)
$ff8034 20 96 81      jsr $8196
$ff8037 8d 68 01      sta $0168
$ff803a c9 a5 f0      cmp #$f0a5
$ff803d 1f a9 08 20   ora $2008a9,x
$ff8041 72 81         adc ($81)
$ff8043 a9 51 20      lda #$2051
$ff8046 72 81         adc ($81)
$ff8048 a9 a5 20      lda #$20a5
$ff804b 72 81         adc ($81)
$ff804d 9c ff 15      stz $15ff
$ff8050 9c a6 01      stz $01a6
$ff8053 a9 00 8f      lda #$8f00
$ff8056 f3 03         sbc ($03,sp),y
$ff8058 00 8f         brk $8f
$ff805a f4 03 00      pea $0003
$ff805d a9 06 20      lda #$2006
$ff8060 72 81         adc ($81)
$ff8062 20 10 81      jsr $8110
$ff8065 a0 00 00      ldy #$0000
$ff8068 a9 0a 99      lda #$990a
$ff806b 07 03         ora [$03]
$ff806d 98            tya
$ff806e cd 07 03      cmp $0307
$ff8071 c8            iny
$ff8072 90 f6         bcc $ff806a
$ff8074 a0 00 00      ldy #$0000
$ff8077 a9 08 99      lda #$9908
$ff807a fe 02 98      inc $9802,x
$ff807d cd fe 02      cmp $02fe
$ff8080 c8            iny
$ff8081 90 f6         bcc $ff8079
$ff8083 e2 30         sep #$30
$ff8085 a9 0f         lda #$0f
$ff8087 48            pha
$ff8088 20 e4 a6      jsr $a6e4
$ff808b f4 01 00      pea $0001
$ff808e 4b            phk
$ff808f 62 04 00      per $ff8096
$ff8092 22 14 00 fe   jsr $fe0014
$ff8096 6b            rtl
$ff8097 e2 30         sep #$30
$ff8099 90 07         bcc $ff80a2
$ff809b 68            pla
$ff809c 3a            dec
$ff809d 10 e8         bpl $ff8087
$ff809f 4c b1 81      jmp $81b1
$ff80a2 c2 10         rep #$10
$ff80a4 68            pla
$ff80a5 a9 b3 20      lda #$20b3
$ff80a8 72 81         adc ($81)
$ff80aa a9 03 20      lda #$2003
$ff80ad 72 81         adc ($81)
$ff80af ad f9 02      lda $02f9
$ff80b2 c9 02 b0      cmp #$b002
$ff80b5 04 a9         tsb $a9
$ff80b7 01 80         ora ($80,x)
$ff80b9 02 a9         cop $a9
$ff80bb 02 20         cop $20
$ff80bd 72 81         adc ($81)
$ff80bf a9 05 20      lda #$2005
$ff80c2 72 81         adc ($81)
$ff80c4 a9 ff 20      lda #$20ff
$ff80c7 72 81         adc ($81)
$ff80c9 a9 04 20      lda #$2004
$ff80cc 72 81         adc ($81)
$ff80ce 20 f9 80      jsr $80f9
$ff80d1 a9 06 20      lda #$2006
$ff80d4 72 81         adc ($81)
$ff80d6 20 10 81      jsr $8110
$ff80d9 ad 8e 01      lda $018e
$ff80dc c9 06 90      cmp #$9006
$ff80df 10 a9         bpl $ff808a
$ff80e1 12 20         ora ($20)
$ff80e3 72 81         adc ($81)
$ff80e5 20 44 81      jsr $8144
$ff80e8 a9 13 20      lda #$2013
$ff80eb 72 81         adc ($81)
$ff80ed 20 63 81      jsr $8163
$ff80f0 9c d7 0f      stz $0fd7
$ff80f3 9c d6 0f      stz $0fd6
$ff80f6 e2 30         sep #$30
$ff80f8 60            rts
$ff80f9 a9 00         lda #$00
$ff80fb 0d ef 02      ora $02ef
$ff80fe 0a            asl
$ff80ff 0a            asl
$ff8100 0d eb 02      ora $02eb
$ff8103 0a            asl
$ff8104 0d f1 02      ora $02f1
$ff8107 0a            asl
$ff8108 0d f0 02      ora $02f0
$ff810b 0a            asl
$ff810c 0a            asl
$ff810d 4c 72 81      jmp $8172
$ff8110 a9 32         lda #$32
$ff8112 20 6f 81      jsr $816f
$ff8115 ad e9 02      lda $02e9
$ff8118 0a            asl
$ff8119 0a            asl
$ff811a 0a            asl
$ff811b 0a            asl
$ff811c 0d ea 02      ora $02ea
$ff811f 20 6f 81      jsr $816f
$ff8122 ad ed 02      lda $02ed
$ff8125 0a            asl
$ff8126 0a            asl
$ff8127 0a            asl
$ff8128 0a            asl
$ff8129 48            pha
$ff812a ad 8e 01      lda $018e
$ff812d c9 06         cmp #$06
$ff812f ad ec 02      lda $02ec
$ff8132 b0 08         bcs $ff813c
$ff8134 c9 08         cmp #$08
$ff8136 90 02         bcc $ff813a
$ff8138 a9 07         lda #$07
$ff813a 49 07         eor #$07
$ff813c 03 01         ora $01,sp
$ff813e 83 01         sta $01,sp
$ff8140 68            pla
$ff8141 4c 72 81      jmp $8172
$ff8144 ad fb 02      lda $02fb
$ff8147 0a            asl
$ff8148 0a            asl
$ff8149 0a            asl
$ff814a 0a            asl
$ff814b 0d f9 02      ora $02f9
$ff814e 20 6f 81      jsr $816f
$ff8151 ad fc 02      lda $02fc
$ff8154 0a            asl
$ff8155 0a            asl
$ff8156 0a            asl
$ff8157 0a            asl
$ff8158 0d fd 02      ora $02fd
$ff815b 49 70         eor #$70
$ff815d 38            sec
$ff815e e9 30         sbc #$30
$ff8160 4c 72 81      jmp $8172
$ff8163 ad f2 02      lda $02f2
$ff8166 20 6f 81      jsr $816f
$ff8169 ad f3 02      lda $02f3
$ff816c 4c 72 81      jmp $8172
$ff816f 38            sec
$ff8170 80 01         bra $ff8173
$ff8172 18            clc
$ff8173 08            php
$ff8174 78            sei
$ff8175 aa            tax
$ff8176 a0 0a         ldy #$0a
$ff8178 00 ad         brk $ad
$ff817a 27 c0         and [$c0]
$ff817c 6a            ror
$ff817d 8a            txa
$ff817e b0 11         bcs $ff8191
$ff8180 8d 26 c0      sta $c026
$ff8183 a2 4c         ldx #$4c
$ff8185 1d ca f0      ora $f0ca,x
$ff8188 22 ad 27 c0   jsr $c027ad
$ff818c 6a            ror
$ff818d b0 f7         bcs $ff8186
$ff818f 28            plp
$ff8190 60            rts
$ff8191 88            dey
$ff8192 f0 17         beq $ff81ab
$ff8194 80 e3         bra $ff8179
$ff8196 18            clc
$ff8197 08            php
$ff8198 78            sei
$ff8199 a2 ec         ldx #$ec
$ff819b 2c ca f0      bit $f0ca
$ff819e 0c ad 27      tsb $27ad
$ff81a1 c0 29         cpy #$29
$ff81a3 20 f0 f6      jsr $f6f0
$ff81a6 ad 26 c0      lda $c026
$ff81a9 28            plp
$ff81aa 60            rts
$ff81ab 28            plp
$ff81ac 90 01         bcc $ff81af
$ff81ae fa            plx
$ff81af fa            plx
$ff81b0 fa            plx
$ff81b1 e2 30         sep #$30
$ff81b3 20 2e 9c      jsr $9c2e
$ff81b6 f4 11 09      pea $0911
$ff81b9 20 e4 a6      jsr $a6e4
$ff81bc 8b            phb
$ff81bd 38            sec
$ff81be 5c 33 a5 ff   jmp $ffa533
$ff81c2 20 f9 81      jsr $81f9
$ff81c5 d4 28         pei ($28)
$ff81c7 d4 24         pei ($24)
$ff81c9 a6 00         ldx $00
$ff81cb 30 1a         bmi $ff81e7
$ff81cd bf 2d 83 ff   lda $ff832d,x
$ff81d1 0a            asl
$ff81d2 0a            asl
$ff81d3 10 12         bpl $ff81e7
$ff81d5 9c 7b 04      stz $047b
$ff81d8 a5 32         lda $32
$ff81da 48            pha
$ff81db 20 11 9a      jsr $9a11
$ff81de 20 65 94      jsr $9465
$ff81e1 68            pla
$ff81e2 30 03         bmi $ff81e7
$ff81e4 20 20 9a      jsr $9a20
$ff81e7 c2 30         rep #$30
$ff81e9 68            pla
$ff81ea 85 24         sta $24
$ff81ec 68            pla
$ff81ed 85 28         sta $28
$ff81ef e2 30         sep #$30
$ff81f1 20 fc 81      jsr $81fc
$ff81f4 9c 32 c0      stz $c032
$ff81f7 18            clc
$ff81f8 6b            rtl
$ff81f9 e2 40         sep #$40
$ff81fb 50 b8         bvc $ff81b5
$ff81fd a2 00         ldx #$00
$ff81ff a0 78         ldy #$78
$ff8201 a9 04         lda #$04
$ff8203 84 09         sty $09
$ff8205 85 0a         sta $0a
$ff8207 a0 00         ldy #$00
$ff8209 50 06         bvc $ff8211
$ff820b b1 09         lda ($09),y
$ff820d 95 0b         sta $0b,x
$ff820f 80 04         bra $ff8215
$ff8211 b5 0b         lda $0b,x
$ff8213 91 09         sta ($09),y
$ff8215 e8            inx
$ff8216 a0 80         ldy #$80
$ff8218 50 06         bvc $ff8220
$ff821a b1 09         lda ($09),y
$ff821c 95 0b         sta $0b,x
$ff821e 80 04         bra $ff8224
$ff8220 b5 0b         lda $0b,x
$ff8222 91 09         sta ($09),y
$ff8224 e8            inx
$ff8225 a5 0a         lda $0a
$ff8227 1a            inc
$ff8228 c9 08         cmp #$08
$ff822a 90 d9         bcc $ff8205
$ff822c a0 7b         ldy #$7b
$ff822e c4 09         cpy $09
$ff8230 d0 cf         bne $ff8201
$ff8232 60            rts
$ff8233 58            cli
$ff8234 20 44 9a      jsr $9a44
$ff8237 f0 62         beq $ff829b
$ff8239 20 b1 82      jsr $82b1
$ff823c c9 8b         cmp #$8b
$ff823e f0 05         beq $ff8245
$ff8240 c9 8a         cmp #$8a
$ff8242 f0 3b         beq $ff827f
$ff8244 60            rts
$ff8245 a5 01         lda $01
$ff8247 d0 04         bne $ff824d
$ff8249 a5 04         lda $04
$ff824b 85 01         sta $01
$ff824d c6 01         dec $01
$ff824f a6 01         ldx $01
$ff8251 b5 83         lda $83,x
$ff8253 30 f0         bmi $ff8245
$ff8255 20 44 9a      jsr $9a44
$ff8258 d0 16         bne $ff8270
$ff825a a5 01         lda $01
$ff825c d0 41         bne $ff829f
$ff825e 20 2b 9a      jsr $9a2b
$ff8261 f0 09         beq $ff826c
$ff8263 20 37 9a      jsr $9a37
$ff8266 f0 04         beq $ff826c
$ff8268 a9 0b         lda #$0b
$ff826a 80 31         bra $ff829d
$ff826c a9 04         lda #$04
$ff826e 80 2d         bra $ff829d
$ff8270 20 d5 82      jsr $82d5
$ff8273 b0 2a         bcs $ff829f
$ff8275 a5 01         lda $01
$ff8277 c9 05         cmp #$05
$ff8279 d0 24         bne $ff829f
$ff827b a9 01         lda #$01
$ff827d 80 1e         bra $ff829d
$ff827f a5 01         lda $01
$ff8281 1a            inc
$ff8282 c5 04         cmp $04
$ff8284 90 04         bcc $ff828a
$ff8286 a9 ff         lda #$ff
$ff8288 85 01         sta $01
$ff828a e6 01         inc $01
$ff828c a6 01         ldx $01
$ff828e b5 83         lda $83,x
$ff8290 30 ed         bmi $ff827f
$ff8292 20 44 9a      jsr $9a44
$ff8295 d0 0b         bne $ff82a2
$ff8297 a5 01         lda $01
$ff8299 d0 04         bne $ff829f
$ff829b a9 01         lda #$01
$ff829d 85 01         sta $01
$ff829f a9 00         lda #$00
$ff82a1 60            rts
$ff82a2 20 d5 82      jsr $82d5
$ff82a5 b0 f8         bcs $ff829f
$ff82a7 a5 01         lda $01
$ff82a9 c9 02         cmp #$02
$ff82ab d0 f2         bne $ff829f
$ff82ad a9 06         lda #$06
$ff82af 80 ec         bra $ff829d
$ff82b1 20 74 cf      jsr $cf74
$ff82b4 10 fb         bpl $ff82b1
$ff82b6 c9 9b         cmp #$9b
$ff82b8 f0 19         beq $ff82d3
$ff82ba c9 8d         cmp #$8d
$ff82bc f0 15         beq $ff82d3
$ff82be c9 8b         cmp #$8b
$ff82c0 f0 11         beq $ff82d3
$ff82c2 c9 8a         cmp #$8a
$ff82c4 f0 0d         beq $ff82d3
$ff82c6 c9 88         cmp #$88
$ff82c8 f0 0a         beq $ff82d4
$ff82ca c9 95         cmp #$95
$ff82cc f0 06         beq $ff82d4
$ff82ce 20 51 9a      jsr $9a51
$ff82d1 80 de         bra $ff82b1
$ff82d3 18            clc
$ff82d4 60            rts
$ff82d5 af 8e 01 e1   lda $e1018e
$ff82d9 c9 06         cmp #$06
$ff82db 90 0e         bcc $ff82eb
$ff82dd af a5 01 e1   lda $e101a5
$ff82e1 10 08         bpl $ff82eb
$ff82e3 a5 00         lda $00
$ff82e5 c9 05         cmp #$05
$ff82e7 d0 02         bne $ff82eb
$ff82e9 18            clc
$ff82ea 60            rts
$ff82eb 38            sec
$ff82ec 60            rts
$ff82ed 4f 5f 6b 69   eor $696b5f
$ff82f1 56 61         lsr $61,x
$ff82f3 43 43         eor $43,sp
$ff82f5 00 6c         brk $6c
$ff82f7 89 8f         bit #$8f
$ff82f9 91 af         sta ($af),y
$ff82fb 9a            txs
$ff82fc 92 71         sta ($71)
$ff82fe 71 a7         adc ($a7),y
$ff8300 aa            tax
$ff8301 dd e3 e5      cmp $e5e3,x
$ff8304 f9 ee e6      sbc $e6ee,y
$ff8307 c5 d1         cmp $d1
$ff8309 fb            xce
$ff830a fe d8 de      inc $ded8,x
$ff830d e0 f4         cpx #$f4
$ff830f e9 e1         sbc #$e1
$ff8311 c0 cc         cpy #$cc
$ff8313 f6 f9         inc $f9,x
$ff8315 84 ff         sty $ff
$ff8317 f2 ff         sbc ($ff)
$ff8319 fe ff 10      inc $10ff,x
$ff831c fe c0 f8      inc $f8c0,x
$ff831f 80 fe         bra $ff831f
$ff8321 00 f0         brk $f0
$ff8323 00 f0         brk $f0
$ff8325 fe fe 8c      inc $8cfe,x
$ff8328 ff ff ff 00   sbc $00ffff,x
$ff832c f4 d9 53      pea $53d9
$ff832f 11 f9         ora ($f9),y
$ff8331 1b            tcs
$ff8332 19 1c 1c      ora $1c1c,y
$ff8335 d9 17 00      cmp $0017,y
$ff8338 2c 33 63      bit $6333
$ff833b 3c 51 46      bit $4651,x
$ff833e 3d 27 27      and $2727,x
$ff8341 5a            phy
$ff8342 66 6d         ror $6d
$ff8344 71 76         adc ($76),y
$ff8346 76 76         ror $76,x
$ff8348 76 78         ror $78,x
$ff834a 87 8f         sta [$8f]
$ff834c 76 76         ror $76,x
$ff834e 76 92         ror $92,x
$ff8350 94 96         sty $96,x
$ff8352 96 96         stx $96,y
$ff8354 76 a6         ror $a6,x
$ff8356 a8            tay
$ff8357 a8            tay
$ff8358 76 c8         ror $c8,x
$ff835a d2 d2         cmp ($d2)
$ff835c 76 76         ror $76,x
$ff835e d7 d9         cmp [$d9],y
$ff8360 d9 00 04      cmp $0400,y
$ff8363 08            php
$ff8364 0a            asl
$ff8365 0c 0e 10      tsb $100e
$ff8368 12 e8         ora ($e8)
$ff836a eb            xba
$ff836b d7 d2         cmp [$d2],y
$ff836d d2 d2         cmp ($d2)
$ff836f d2 c8         cmp ($c8)
$ff8371 04 05         tsb $05
$ff8373 02 02         cop $02
$ff8375 02 02         cop $02
$ff8377 0f 08 03 02   ora $020308
$ff837b 02 02         cop $02
$ff837d 04 05         tsb $05
$ff837f 02 02         cop $02
$ff8381 02 02         cop $02
$ff8383 0f 08 03 02   ora $020308
$ff8387 02 02         cop $02
$ff8389 02 02         cop $02
$ff838b 10 10         bpl $ff839d
$ff838d 10 02         bpl $ff8391
$ff838f 0f 0f 02 04   ora $04020f
$ff8393 04 02         tsb $02
$ff8395 02 02         cop $02
$ff8397 02 02         cop $02
$ff8399 0b            phd
$ff839a 80 c0         bra $ff835c
$ff839c 02 0a         cop $0a
$ff839e 05 05         ora $05
$ff83a0 02 02         cop $02
$ff83a2 02 2e         cop $2e
$ff83a4 2e 03 02      rol $0203
$ff83a7 80 02         bra $ff83ab
$ff83a9 80 05         bra $ff83b0
$ff83ab 05 05         ora $05
$ff83ad 05 0a         ora $0a
$ff83af 0c 1f 64      tsb $641f
$ff83b2 03 18         ora $18,sp
$ff83b4 3c 3c 02      bit $023c,x
$ff83b7 1f 1c 1f 1e   ora $1e1f1c,x
$ff83bb 1f 1e 1f 1f   ora $1f1f1e,x
$ff83bf 1e 1f 1e      asl $1e1f,x
$ff83c2 1f 1d 04 00   ora $00041d,x
$ff83c6 00 00         brk $00
$ff83c8 01 00         ora ($00,x)
$ff83ca 00 0d         brk $0d
$ff83cc 06 02         asl $02
$ff83ce 01 01         ora ($01,x)
$ff83d0 00 01         brk $01
$ff83d2 00 00         brk $00
$ff83d4 00 00         brk $00
$ff83d6 00 07         brk $07
$ff83d8 06 02         asl $02
$ff83da 01 01         ora ($01,x)
$ff83dc 00 00         brk $00
$ff83de 00 0f         brk $0f
$ff83e0 06 06         asl $06
$ff83e2 00 05         brk $05
$ff83e4 06 01         asl $01
$ff83e6 00 00         brk $00
$ff83e8 00 00         brk $00
$ff83ea 00 00         brk $00
$ff83ec 01 00         ora ($00,x)
$ff83ee 00 00         brk $00
$ff83f0 00 05         brk $05
$ff83f2 02 02         cop $02
$ff83f4 00 00         brk $00
$ff83f6 00 2d         brk $2d
$ff83f8 2d 00 00      and $0000
$ff83fb 00 00         brk $00
$ff83fd 00 00         brk $00
$ff83ff 02 02         cop $02
$ff8401 02 06         cop $06
$ff8403 08            php
$ff8404 00 01         brk $01
$ff8406 02 03         cop $03
$ff8408 04 05         tsb $05
$ff840a 06 07         asl $07
$ff840c 0a            asl
$ff840d 00 01         brk $01
$ff840f 02 03         cop $03
$ff8411 04 05         tsb $05
$ff8413 06 07         asl $07
$ff8415 08            php
$ff8416 09 0a         ora #$0a
$ff8418 0b            phd
$ff8419 0c 0d 0e      tsb $0e0d
$ff841c 0f 00 00 a0   ora $a00000
$ff8420 a0 a0         ldy #$a0
$ff8422 cd e1 f2      cmp $f2e1
$ff8425 e3 e8         sbc $e8,sp
$ff8427 a0 b1         ldy #$b1
$ff8429 b7 ac         lda [$ac],y
$ff842b a0 b1         ldy #$b1
$ff842d b9 b8 b7      lda $b7b8,y
$ff8430 b7 86         lda [$86],y
$ff8432 c5 86         cmp $86
$ff8434 dc 86 f4      jmp [$f486]
$ff8437 86 6a         stx $6a
$ff8439 86 70         stx $70
$ff843b 86 74         stx $74
$ff843d 86 79         stx $79
$ff843f 86 84         stx $84
$ff8441 86 8e         stx $8e
$ff8443 86 91         stx $91
$ff8445 86 95         stx $95
$ff8447 86 0d         stx $0d
$ff8449 87 25         sta [$25]
$ff844b 88            dey
$ff844c a2 86         ldx #$86
$ff844e b0 86         bcs $ff83d6
$ff8450 2e 87 33      rol $3387
$ff8453 87 39         sta [$39]
$ff8455 87 43         sta [$43]
$ff8457 87 4d         sta [$4d]
$ff8459 87 59         sta [$59]
$ff845b 87 24         sta [$24]
$ff845d 87 29         sta [$29]
$ff845f 87 6e         sta [$6e]
$ff8461 87 95         sta [$95]
$ff8463 87 bc         sta [$bc]
$ff8465 87 4a         sta [$4a]
$ff8467 89 51         bit #$51
$ff8469 89 56         bit #$56
$ff846b 89 62         bit #$62
$ff846d 89 67         bit #$67
$ff846f 89 6f         bit #$6f
$ff8471 89 74         bit #$74
$ff8473 89 80         bit #$80
$ff8475 89 8a         bit #$8a
$ff8477 89 96         bit #$96
$ff8479 89 0b         bit #$0b
$ff847b 8c 92 89      sty $8992
$ff847e 9b            txy
$ff847f 89 ac         bit #$ac
$ff8481 89 b7         bit #$b7
$ff8483 89 cf         bit #$cf
$ff8485 89 de         bit #$de
$ff8487 89 e2         bit #$e2
$ff8489 89 eb         bit #$eb
$ff848b 89 ef         bit #$ef
$ff848d 89 fd         bit #$fd
$ff848f 89 03         bit #$03
$ff8491 8a            txa
$ff8492 10 8a         bpl $ff841e
$ff8494 21 8a         and ($8a,x)
$ff8496 33 8a         and ($8a,sp),y
$ff8498 37 8a         and [$8a],y
$ff849a 5e 87 3e      lsr $3e87,x
$ff849d 8a            txa
$ff849e 42            ???
$ff849f 8a            txa
$ff84a0 4c 8a 52      jmp $528a
$ff84a3 8a            txa
$ff84a4 0b            phd
$ff84a5 8c 04 88      sty $8804
$ff84a8 56 89         lsr $89,x
$ff84aa d8            cld
$ff84ab 8a            txa
$ff84ac df 8a e6 8a   cmp $8ae68a,x
$ff84b0 ed 8a f4      sbc $f48a
$ff84b3 8a            txa
$ff84b4 fb            xce
$ff84b5 8a            txa
$ff84b6 02 8b         cop $8b
$ff84b8 0b            phd
$ff84b9 8c 09 8b      sty $8b09
$ff84bc 10 8b         bpl $ff8449
$ff84be 20 8b 2f      jsr $2f8b
$ff84c1 8b            phb
$ff84c2 41 8b         eor ($8b,x)
$ff84c4 4d 8b 59      eor $598b
$ff84c7 8b            phb
$ff84c8 0b            phd
$ff84c9 8c e3 87      sty $87e3
$ff84cc 65 8b         adc $8b
$ff84ce 79 8b 8f      adc $8f8b,y
$ff84d1 8b            phb
$ff84d2 9e 8b a3      stz $a38b,x
$ff84d5 8b            phb
$ff84d6 a6 8b         ldx $8b
$ff84d8 aa            tax
$ff84d9 8b            phb
$ff84da 0b            phd
$ff84db 8c b0 8b      sty $8bb0
$ff84de b4 8b         ldy $8b,x
$ff84e0 ba            tsx
$ff84e1 8b            phb
$ff84e2 aa            tax
$ff84e3 8b            phb
$ff84e4 c0 8b         cpy #$8b
$ff84e6 0d 88 0b      ora $0b88
$ff84e9 8c f7 87      sty $87f7
$ff84ec d6 8b         dec $8b,x
$ff84ee e7 8b         sbc [$8b]
$ff84f0 fb            xce
$ff84f1 8b            phb
$ff84f2 0b            phd
$ff84f3 8c 0d 8c      sty $8c0d
$ff84f6 1f 8c 0b 8c   ora $8c0b8c,x
$ff84fa 25 8c         and $8c
$ff84fc 2b            pld
$ff84fd 8c 39 8c      sty $8c39
$ff8500 0b            phd
$ff8501 8c 71 8c      sty $8c71
$ff8504 47 8c         eor [$8c]
$ff8506 55 8c         eor $8c,x
$ff8508 63 8c         adc $8c,sp
$ff850a 81 8c         sta ($8c,x)
$ff850c 89 8c         bit #$8c
$ff850e 8f 8c 99 8c   sta $8c998c
$ff8512 a3 8c         lda $8c,sp
$ff8514 ac 8c ae      ldy $ae8c
$ff8517 8c b0 8c      sty $8cb0
$ff851a b2 8c         lda ($8c)
$ff851c b5 8c         lda $8c,x
$ff851e b7 8c         lda [$8c],y
$ff8520 ba            tsx
$ff8521 8c bc 8c      sty $8cbc
$ff8524 be 8c c1      ldx $c18c,y
$ff8527 8c c6 8c      sty $8cc6
$ff852a c9 8c         cmp #$8c
$ff852c cc 8c cf      cpy $cf8c
$ff852f 8c d3 8c      sty $8cd3
$ff8532 d7 8c         cmp [$8c],y
$ff8534 db            stp
$ff8535 8c df 8c      sty $8cdf
$ff8538 e3 8c         sbc $8c,sp
$ff853a e7 8c         sbc [$8c]
$ff853c eb            xba
$ff853d 8c f0 8c      sty $8cf0
$ff8540 f3 8c         sbc ($8c,sp),y
$ff8542 f6 8c         inc $8c,x
$ff8544 f9 8c fc      sbc $fc8c,y
$ff8547 8c ff 8c      sty $8cff
$ff854a 02 8d         cop $8d
$ff854c 05 8d         ora $8d
$ff854e 08            php
$ff854f 8d 0b 8d      sta $8d0b
$ff8552 0f 8d 13 8d   ora $8d138d
$ff8556 18            clc
$ff8557 8d ac 8c      sta $8cac
$ff855a b0 8c         bcs $ff84e8
$ff855c 5b            tcd
$ff855d 8a            txa
$ff855e 60            rts
$ff855f 8a            txa
$ff8560 68            pla
$ff8561 8a            txa
$ff8562 71 8a         adc ($8a),y
$ff8564 77 8a         adc [$8a],y
$ff8566 81 8a         sta ($8a,x)
$ff8568 8a            txa
$ff8569 8a            txa
$ff856a 95 8a         sta $8a,x
$ff856c 9f 8a a4 8a   sta $8aa48a,x
$ff8570 aa            tax
$ff8571 8a            txa
$ff8572 b4 8a         ldy $8a,x
$ff8574 b8            clv
$ff8575 8a            txa
$ff8576 c3 8a         cmp $8a,sp
$ff8578 c9 8a         cmp #$8a
$ff857a d3 8a         cmp ($8a,sp),y
$ff857c 22 8d ba 8c   jsr $8cba8d
$ff8580 24 8d         bit $8d
$ff8582 2a            rol
$ff8583 8d 2e 8d      sta $8d2e
$ff8586 34 8d         bit $8d,x
$ff8588 3a            dec
$ff8589 8d 41 8d      sta $8d41
$ff858c 48            pha
$ff858d 8d 4e 8d      sta $8d4e
$ff8590 55 8d         eor $8d,x
$ff8592 5b            tcd
$ff8593 8d 6a 8d      sta $8d6a
$ff8596 71 8d         adc ($8d),y
$ff8598 77 8d         adc [$8d],y
$ff859a 7f 8d 85 8d   adc $8d858d,x
$ff859e 8a            txa
$ff859f 8d 91 8d      sta $8d91
$ff85a2 98            tya
$ff85a3 8d a2 8d      sta $8da2
$ff85a6 a7 8d         lda [$8d]
$ff85a8 ac 8d ae      ldy $ae8d
$ff85ab 8d b0 8d      sta $8db0
$ff85ae b2 8d         lda ($8d)
$ff85b0 b4 8d         ldy $8d,x
$ff85b2 b6 8d         ldx $8d,y
$ff85b4 b8            clv
$ff85b5 8d ba 8d      sta $8dba
$ff85b8 bc 8d be      ldy $be8d,x
$ff85bb 8d c0 8d      sta $8dc0
$ff85be c2 8d         rep #$8d
$ff85c0 d7 88         cmp [$88],y
$ff85c2 dd 88 e4      cmp $e488,x
$ff85c5 88            dey
$ff85c6 ec 88 f4      cpx $f488
$ff85c9 88            dey
$ff85ca 04 89         tsb $89
$ff85cc 14 89         trb $89
$ff85ce 24 89         bit $89
$ff85d0 34 89         bit $89,x
$ff85d2 43 89         eor $89,sp
$ff85d4 d7 88         cmp [$88],y
$ff85d6 ec 88 0c      cpx $0c88
$ff85d9 89 24         bit #$24
$ff85db 89 43         bit #$43
$ff85dd 89 c4         bit #$c4
$ff85df 8d ca 8d      sta $8dca
$ff85e2 d7 88         cmp [$88],y
$ff85e4 dd 88 e4      cmp $e488,x
$ff85e7 88            dey
$ff85e8 ec 88 f4      cpx $f488
$ff85eb 88            dey
$ff85ec fc 88 04      jsr ($0488,x)
$ff85ef 89 0c         bit #$0c
$ff85f1 89 14         bit #$14
$ff85f3 89 1c         bit #$1c
$ff85f5 89 24         bit #$24
$ff85f7 89 2c         bit #$2c
$ff85f9 89 34         bit #$34
$ff85fb 89 3c         bit #$3c
$ff85fd 89 43         bit #$43
$ff85ff 89 2a         bit #$2a
$ff8601 8e 32 8e      stx $8e32
$ff8604 3a            dec
$ff8605 8e 42 8e      stx $8e42
$ff8608 47 8e         eor [$8e]
$ff860a 4e 8e 55      lsr $558e
$ff860d 8e 6c 8e      stx $8e6c
$ff8610 6f 8e 73 8e   adc $8e738e
$ff8614 7e 8e 87      ror $878e,x
$ff8617 7d 8c 7c      adc $7c8c,x
$ff861a 70 7b         bvs $ff8697
$ff861c 7d 7b a5      adc $a57b,x
$ff861f 7b            tdc
$ff8620 b7 7b         lda [$7b],y
$ff8622 c7 7b         cmp [$7b]
$ff8624 de 7b 00      dec $007b,x
$ff8627 00 00         brk $00
$ff8629 00 00         brk $00
$ff862b 00 00         brk $00
$ff862d 00 00         brk $00
$ff862f 00 d7         brk $d7
$ff8631 8d ce 8d      sta $8dce
$ff8634 15 8e         ora $8e,x
$ff8636 de 8d de      dec $de8d,x
$ff8639 8d ce 8d      sta $8dce
$ff863c 15 8e         ora $8e,x
$ff863e d7 8d         cmp [$8d],y
$ff8640 e3 8d         sbc $8d,sp
$ff8642 ce 8d f8      dec $f88d
$ff8645 8d ce 8d      sta $8dce
$ff8648 02 8e         cop $8e
$ff864a ce 8d 0c      dec $0c8d
$ff864d 8e ce 8d      stx $8dce
$ff8650 15 8e         ora $8e,x
$ff8652 ce 8d 1e      dec $1e8d
$ff8655 8e d8 8a      stx $8ad8
$ff8658 df 8a e6 8a   cmp $8ae68a,x
$ff865c ed 8a f4      sbc $f48a
$ff865f 8a            txa
$ff8660 fb            xce
$ff8661 8a            txa
$ff8662 02 8b         cop $8b
$ff8664 8a            txa
$ff8665 89 22         bit #$22
$ff8667 8e 15 8e      stx $8e15
$ff866a 54 69 6d      mvn $69,$6d
$ff866d 65 3d         adc $3d
$ff866f a0 53         ldy #$53
$ff8671 74 65         stz $65,x
$ff8673 f0 54         beq $ff86c9
$ff8675 72 61         adc ($61)
$ff8677 63 e5         adc $e5,sp
$ff8679 43 62         eor $62,sp
$ff867b 61 6e         adc ($6e,x)
$ff867d 6b            rtl
$ff867e 20 28 30      jsr $3028
$ff8681 2f 31 a9 44   and $44a931
$ff8685 65 63         adc $63
$ff8687 69 6d         adc #$6d
$ff8689 61 6c         adc ($6c,x)
$ff868b 2d 3e a0      and $a03e
$ff868e 48            pha
$ff868f 65 78         adc $78
$ff8691 2d 3e 20      and $203e
$ff8694 a4 54         ldy $54
$ff8696 6f 6f 6c 20   adc $206c6f
$ff869a 65 72         adc $72
$ff869c 72 6f         adc ($6f)
$ff869e 72 2d         adc ($2d)
$ff86a0 3e a0 20      rol $20a0,x
$ff86a3 43 6f         eor $6f,sp
$ff86a5 6e 74 72      ror $7274
$ff86a8 6f 6c 20 50   adc $50206c
$ff86ac 61 6e         adc ($6e,x)
$ff86ae 65 ec         adc $ec
$ff86b0 53 65         eor ($65,sp),y
$ff86b2 6c 65 63      jmp ($6365)
$ff86b5 74 ba         stz $ba,x
$ff86b7 0e a0 c9      asl $c9a0
$ff86ba af cf a0 e5   lda $e5a0cf
$ff86be f2 f2         sbc ($f2)
$ff86c0 ef f2 a1 8d   sbc $8da1f2
$ff86c4 00 0a         brk $0a
$ff86c6 a0 ce         ldy #$ce
$ff86c8 ef f4 a0 e1   sbc $e1a0f4
$ff86cc a0 f3         ldy #$f3
$ff86ce f4 e1 f2      pea $f2e1
$ff86d1 f4 f5 f0      pea $f0f5
$ff86d4 a0 e4         ldy #$e4
$ff86d6 e9 f3         sbc #$f3
$ff86d8 eb            xba
$ff86d9 a1 8d         lda ($8d,x)
$ff86db 00 0a         brk $0a
$ff86dd a0 ce         ldy #$ce
$ff86df ef a0 e4 e5   sbc $e5e4a0
$ff86e3 f6 e9         inc $e9,x
$ff86e5 e3 e5         sbc $e5,sp
$ff86e7 a0 e3         ldy #$e3
$ff86e9 ef ee ee e5   sbc $e5eeee
$ff86ed e3 f4         sbc $f4,sp
$ff86ef e5 e4         sbc $e4
$ff86f1 a1 8d         lda ($8d,x)
$ff86f3 00 0a         brk $0a
$ff86f5 a0 c3         ldy #$c3
$ff86f7 e8            inx
$ff86f8 e5 e3         sbc $e3
$ff86fa eb            xba
$ff86fb a0 f3         ldy #$f3
$ff86fd f4 e1 f2      pea $f2e1
$ff8700 f4 f5 f0      pea $f0f5
$ff8703 a0 e4         ldy #$e4
$ff8705 e5 f6         sbc $f6
$ff8707 e9 e3         sbc #$e3
$ff8709 e5 a1         sbc $a1
$ff870b 8d 00 08      sta $0800
$ff870e a0 c6         ldy #$c6
$ff8710 e1 f4         sbc ($f4,x)
$ff8712 e1 ec         sbc ($ec,x)
$ff8714 a0 f3         ldy #$f3
$ff8716 f9 f3 f4      sbc $f4f3,y
$ff8719 e5 ed         sbc $ed
$ff871b a0 e5         ldy #$e5
$ff871d f2 f2         sbc ($f2)
$ff871f ef f2 ad be   sbc $beadf2
$ff8723 a0 00         ldy #$00
$ff8725 a0 0d         ldy #$0d
$ff8727 df a0 00 a0   cmp $a000a0,x
$ff872b 0d 4c a0      ora $a04c
$ff872e 00 a0         brk $a0
$ff8730 4a            lsr
$ff8731 a0 4b         ldy #$4b
$ff8733 00 a0         brk $a0
$ff8735 48            pha
$ff8736 a0 55         ldy #$55
$ff8738 00 12         brk $12
$ff873a a0 cf         ldy #$cf
$ff873c f0 e5         beq $ff8723
$ff873e ee ba a0      inc $a0ba
$ff8741 4d 00 02      eor $0200
$ff8744 a0 d3         ldy #$d3
$ff8746 e1 f6         sbc ($f6,x)
$ff8748 e5 ba         sbc $ba
$ff874a a0 4d         ldy #$4d
$ff874c 00 02         brk $02
$ff874e a0 c3         ldy #$c3
$ff8750 e1 ee         sbc ($ee,x)
$ff8752 e3 e5         sbc $e5,sp
$ff8754 ec ba c5      cpx $c5ba
$ff8757 f3 e3         sbc ($e3,sp),y
$ff8759 00 5a         brk $5a
$ff875b 0d a0 5f      ora $5fa0
$ff875e 00 53         brk $53
$ff8760 d3 e3         cmp ($e3,sp),y
$ff8762 f2 e5         sbc ($e5)
$ff8764 e5 ee         sbc $ee
$ff8766 a0 c3         ldy #$c3
$ff8768 ef ec ef f2   sbc $f2efec
$ff876c f3 53         sbc ($53,sp),y
$ff876e 00 53         brk $53
$ff8770 a0 a0         ldy #$a0
$ff8772 d7 c1         cmp [$c1],y
$ff8774 d2 ce         cmp ($ce)
$ff8776 c9 ce         cmp #$ce
$ff8778 c7 ba         cmp [$ba]
$ff877a a0 c5         ldy #$c5
$ff877c e9 f4         sbc #$f4
$ff877e e8            inx
$ff877f e5 f2         sbc $f2
$ff8781 a0 d3         ldy #$d3
$ff8783 ec ef f4      cpx $f4ef
$ff8786 a0 b1         ldy #$b1
$ff8788 a0 ef         ldy #$ef
$ff878a f2 a0         sbc ($a0)
$ff878c d3 ec         cmp ($ec,sp),y
$ff878e ef f4 a0 b2   sbc $b2a0f4
$ff8792 a0 a0         ldy #$a0
$ff8794 53 00         eor ($00,sp),y
$ff8796 53 a0         eor ($a0,sp),y
$ff8798 a0 ee         ldy #$ee
$ff879a e5 e5         sbc $e5
$ff879c e4 f3         cpx $f3
$ff879e a0 f4         ldy #$f4
$ff87a0 ef a0 e2 e5   sbc $e5e2a0
$ff87a4 a0 f3         ldy #$f3
$ff87a6 e5 f4         sbc $f4
$ff87a8 a0 f4         ldy #$f4
$ff87aa ef a0 c1 f0   sbc $f0c1a0
$ff87ae f0 ec         beq $ff879c
$ff87b0 e5 d4         sbc $d4
$ff87b2 e1 ec         sbc ($ec,x)
$ff87b4 eb            xba
$ff87b5 a1 a1         lda ($a1,x)
$ff87b7 a1 a0         lda ($a0,x)
$ff87b9 a0 a0         ldy #$a0
$ff87bb 53 00         eor ($00,sp),y
$ff87bd 53 d7         eor ($d7,sp),y
$ff87bf c1 d2         cmp ($d2,x)
$ff87c1 ce c9 ce      dec $cec9
$ff87c4 c7 ba         cmp [$ba]
$ff87c6 a0 a0         ldy #$a0
$ff87c8 c1 f0         cmp ($f0,x)
$ff87ca f0 ec         beq $ff87b8
$ff87cc e5 d4         sbc $d4
$ff87ce e1 ec         sbc ($ec,x)
$ff87d0 eb            xba
$ff87d1 a0 e1         ldy #$e1
$ff87d3 ec f2 e5      cpx $e5f2
$ff87d6 e1 e4         sbc ($e4,x)
$ff87d8 f9 a0 d3      sbc $d3a0,y
$ff87db e5 ec         sbc $ec
$ff87dd e5 e3         sbc $e3
$ff87df f4 e5 e4      pea $e4e5
$ff87e2 53 00         eor ($00,sp),y
$ff87e4 53 c1         eor ($c1,sp),y
$ff87e6 e4 f6         cpx $f6
$ff87e8 e1 ee         sbc ($ee,x)
$ff87ea e3 e5         sbc $e5,sp
$ff87ec e4 a0         cpx $a0
$ff87ee c6 e5         dec $e5
$ff87f0 e1 f4         sbc ($f4,x)
$ff87f2 f5 f2         sbc $f2,x
$ff87f4 e5 f3         sbc $f3
$ff87f6 53 00         eor ($00,sp),y
$ff87f8 53 d2         eor ($d2,sp),y
$ff87fa c1 cd         cmp ($cd,x)
$ff87fc a0 d3         ldy #$d3
$ff87fe f4 e1 f4      pea $f4e1
$ff8801 f5 f3         sbc $f3,x
$ff8803 53 00         eor ($00,sp),y
$ff8805 53 c8         eor ($c8,sp),y
$ff8807 e5 f2         sbc $f2
$ff8809 f4 fa ba      pea $bafa
$ff880c a0 00         ldy #$00
$ff880e 53 cc         eor ($cc,sp),y
$ff8810 e1 f2         sbc ($f2,x)
$ff8812 e7 e5         sbc [$e5]
$ff8814 f3 f4         sbc ($f4,sp),y
$ff8816 a0 d3         ldy #$d3
$ff8818 e5 ec         sbc $ec
$ff881a e5 e3         sbc $e3
$ff881c f4 e1 e2      pea $e2e1
$ff881f ec e5 ba      cpx $bae5
$ff8822 04 a0         tsb $a0
$ff8824 00 02         brk $02
$ff8826 a0 b1         ldy #$b1
$ff8828 bd c5 ee      lda $eec5,x
$ff882b f4 e5 f2      pea $f2e5
$ff882e a0 f4         ldy #$f4
$ff8830 e8            inx
$ff8831 e5 a0         sbc $a0
$ff8833 c3 ef         cmp $ef,sp
$ff8835 ee f4 f2      inc $f2f4
$ff8838 ef ec a0 d0   sbc $d0a0ec
$ff883c e1 ee         sbc ($ee,x)
$ff883e e5 ec         sbc $ec
$ff8840 ae 8d 8d      ldx $8d8d
$ff8843 a0 a0         ldy #$a0
$ff8845 b2 bd         lda ($bd)
$ff8847 d3 e5         cmp ($e5,sp),y
$ff8849 f4 a0 f3      pea $f3a0
$ff884c f9 f3 f4      sbc $f4f3,y
$ff884f e5 ed         sbc $ed
$ff8851 a0 f3         ldy #$f3
$ff8853 f4 e1 ee      pea $eee1
$ff8856 e4 e1         cpx $e1
$ff8858 f2 e4         sbc ($e4)
$ff885a f3 a0         sbc ($a0,sp),y
$ff885c e1 ee         sbc ($ee,x)
$ff885e e4 a0         cpx $a0
$ff8860 b6 b0         ldx $b0,y
$ff8862 a0 e8         ldy #$e8
$ff8864 e5 f2         sbc $f2
$ff8866 f4 fa ae      pea $aefa
$ff8869 8d 8d a0      sta $a08d
$ff886c a0 b3         ldy #$b3
$ff886e bd d3 e5      lda $e5d3,x
$ff8871 f4 a0 f3      pea $f3a0
$ff8874 f9 f3 f4      sbc $f4f3,y
$ff8877 e5 ed         sbc $ed
$ff8879 a0 f3         ldy #$f3
$ff887b f4 e1 ee      pea $eee1
$ff887e e4 e1         cpx $e1
$ff8880 f2 e4         sbc ($e4)
$ff8882 f3 a0         sbc ($a0,sp),y
$ff8884 e1 ee         sbc ($ee,x)
$ff8886 e4 a0         cpx $a0
$ff8888 b5 b0         lda $b0,x
$ff888a a0 e8         ldy #$e8
$ff888c e5 f2         sbc $f2
$ff888e f4 fa ae      pea $aefa
$ff8891 8d 8d a0      sta $a08d
$ff8894 a0 b4         ldy #$b4
$ff8896 bd c3 ef      lda $efc3,x
$ff8899 ee f4 e9      inc $e9f4
$ff889c ee f5 e5      inc $e5f5
$ff889f a0 f2         ldy #$f2
$ff88a1 e5 f3         sbc $f3
$ff88a3 f4 e1 f2      pea $f2e1
$ff88a6 f4 e9 ee      pea $eee9
$ff88a9 e7 a0         sbc [$a0]
$ff88ab f4 e8 e5      pea $e5e8
$ff88ae a0 f3         ldy #$f3
$ff88b0 f9 f3 f4      sbc $f4f3,y
$ff88b3 e5 ed         sbc $ed
$ff88b5 ae 08 8d      ldx $8d08
$ff88b8 05 a0         ora $a0
$ff88ba d0 f2         bne $ff88ae
$ff88bc e5 f3         sbc $f3
$ff88be f3 a0         sbc ($a0,sp),y
$ff88c0 b1 ac         lda ($ac),y
$ff88c2 b2 ac         lda ($ac)
$ff88c4 b3 a0         lda ($a0,sp),y
$ff88c6 ef f2 a0 b4   sbc $b4a0f2
$ff88ca a0 f4         ldy #$f4
$ff88cc ef a0 e3 ef   sbc $efe3a0
$ff88d0 ee f4 e9      inc $e9f4
$ff88d3 ee f5 e5      inc $e5f5
$ff88d6 ae 00 5a      ldx $5a00
$ff88d9 aa            tax
$ff88da 0e 53 5f      asl $5f53
$ff88dd 00 5a         brk $5a
$ff88df 53 aa         eor ($aa,sp),y
$ff88e1 0d 53 5f      ora $5f53
$ff88e4 00 5a         brk $5a
$ff88e6 53 53         eor ($53,sp),y
$ff88e8 aa            tax
$ff88e9 0c 53 5f      tsb $5f53
$ff88ec 00 5a         brk $5a
$ff88ee 03 53         ora $53,sp
$ff88f0 aa            tax
$ff88f1 0b            phd
$ff88f2 53 5f         eor ($5f,sp),y
$ff88f4 00 5a         brk $5a
$ff88f6 04 53         tsb $53
$ff88f8 aa            tax
$ff88f9 0a            asl
$ff88fa 53 5f         eor ($5f,sp),y
$ff88fc 00 5a         brk $5a
$ff88fe 05 53         ora $53
$ff8900 aa            tax
$ff8901 09 53         ora #$53
$ff8903 5f 00 5a 06   eor $065a00,x
$ff8907 53 aa         eor ($aa,sp),y
$ff8909 08            php
$ff890a 53 5f         eor ($5f,sp),y
$ff890c 00 5a         brk $5a
$ff890e 07 53         ora [$53]
$ff8910 aa            tax
$ff8911 07 53         ora [$53]
$ff8913 5f 00 5a 08   eor $085a00,x
$ff8917 53 aa         eor ($aa,sp),y
$ff8919 06 53         asl $53
$ff891b 5f 00 5a 09   eor $095a00,x
$ff891f 53 aa         eor ($aa,sp),y
$ff8921 05 53         ora $53
$ff8923 5f 00 5a 0a   eor $0a5a00,x
$ff8927 53 aa         eor ($aa,sp),y
$ff8929 04 53         tsb $53
$ff892b 5f 00 5a 0b   eor $0b5a00,x
$ff892f 53 aa         eor ($aa,sp),y
$ff8931 03 53         ora $53,sp
$ff8933 5f 00 5a 0c   eor $0c5a00,x
$ff8937 53 aa         eor ($aa,sp),y
$ff8939 53 53         eor ($53,sp),y
$ff893b 5f 00 5a 0d   eor $0d5a00,x
$ff893f 53 aa         eor ($aa,sp),y
$ff8941 53 5f         eor ($5f,sp),y
$ff8943 00 5a         brk $5a
$ff8945 0e 53 aa      asl $aa53
$ff8948 5f 00 44 69   eor $694400,x
$ff894c 73 70         adc ($70,sp),y
$ff894e 6c 61 f9      jmp ($f961)
$ff8951 53 6f         eor ($6f,sp),y
$ff8953 75 6e         adc $6e,x
$ff8955 e4 53         cpx $53
$ff8957 79 73 74      adc $7473,y
$ff895a 65 6d         adc $6d
$ff895c 20 53 70      jsr $7053
$ff895f 65 65         adc $65
$ff8961 e4 43         cpx $43
$ff8963 6c 6f 63      jmp ($636f)
$ff8966 eb            xba
$ff8967 4b            phk
$ff8968 65 79         adc $79
$ff896a 62 6f 61      per $ffeadc
$ff896d 72 e4         adc ($e4)
$ff896f 53 6c         eor ($6c,sp),y
$ff8971 6f 74 f3 50   adc $50f374
$ff8975 72 69         adc ($69)
$ff8977 6e 74 65      ror $6574
$ff897a 72 20         adc ($20)
$ff897c 50 6f         bvc $ff89ed
$ff897e 72 f4         adc ($f4)
$ff8980 4d 6f 64      eor $646f
$ff8983 65 6d         adc $6d
$ff8985 20 50 6f      jsr $6f50
$ff8988 72 f4         adc ($f4)
$ff898a 52 41         eor ($41)
$ff898c 4d 20 44      eor $4420
$ff898f 69 73         adc #$73
$ff8991 eb            xba
$ff8992 51 75         eor ($75),y
$ff8994 69 f4         adc #$f4
$ff8996 4d 6f 75      eor $756f
$ff8999 73 e5         adc ($e5,sp),y
$ff899b 2d 44 65      and $6544
$ff899e 76 69         ror $69,x
$ff89a0 63 65         adc $65,sp
$ff89a2 20 43 6f      jsr $6f43
$ff89a5 6e 6e 65      ror $656e
$ff89a8 63 74         adc $74,sp
$ff89aa 65 e4         adc $e4
$ff89ac 4c 69 6e      jmp $6e69
$ff89af 65 20         adc $20
$ff89b1 4c 65 6e      jmp $6e65
$ff89b4 67 74         adc [$74]
$ff89b6 e8            inx
$ff89b7 44 65 6c      mvp $65,$6c
$ff89ba 65 74         adc $74
$ff89bc 65 20         adc $20
$ff89be 66 69         ror $69
$ff89c0 72 73         adc ($73)
$ff89c2 74 20         stz $20,x
$ff89c4 4c 46 20      jmp $2046
$ff89c7 61 66         adc ($66,x)
$ff89c9 74 65         stz $65,x
$ff89cb 72 20         adc ($20)
$ff89cd 43 d2         eor $d2,sp
$ff89cf 41 64         eor ($64,x)
$ff89d1 64 20         stz $20
$ff89d3 4c 46 20      jmp $2046
$ff89d6 61 66         adc ($66,x)
$ff89d8 74 65         stz $65,x
$ff89da 72 20         adc ($20)
$ff89dc 43 d2         eor $d2,sp
$ff89de 45 63         eor $63
$ff89e0 68            pla
$ff89e1 ef 42 75 66   sbc $667542
$ff89e5 66 65         ror $65
$ff89e7 72 69         adc ($69)
$ff89e9 6e e7 42      ror $42e7
$ff89ec 61 75         adc ($75,x)
$ff89ee e4 44         cpx $44
$ff89f0 61 74         adc ($74,x)
$ff89f2 61 2f         adc ($2f,x)
$ff89f4 53 74         eor ($74,sp),y
$ff89f6 6f 70 20 42   adc $422070
$ff89fa 69 74         adc #$74
$ff89fc f3 50         sbc ($50,sp),y
$ff89fe 61 72         adc ($72,x)
$ff8a00 69 74         adc #$74
$ff8a02 f9 44 43      sbc $4344,y
$ff8a05 44 20 48      mvp $20,$48
$ff8a08 61 6e         adc ($6e,x)
$ff8a0a 64 73         stz $73
$ff8a0c 68            pla
$ff8a0d 61 6b         adc ($6b,x)
$ff8a0f e5 44         sbc $44
$ff8a11 53 52         eor ($52,sp),y
$ff8a13 2f 44 54 52   and $525444
$ff8a17 20 48 61      jsr $6148
$ff8a1a 6e 64 73      ror $7364
$ff8a1d 68            pla
$ff8a1e 61 6b         adc ($6b,x)
$ff8a20 e5 58         sbc $58
$ff8a22 4f 4e 2f 58   eor $582f4e
$ff8a26 4f 46 46 20   eor $204646
$ff8a2a 48            pha
$ff8a2b 61 6e         adc ($6e,x)
$ff8a2d 64 73         stz $73
$ff8a2f 68            pla
$ff8a30 61 6b         adc ($6b,x)
$ff8a32 e5 54         sbc $54
$ff8a34 79 70 e5      adc $e570,y
$ff8a37 43 6f         eor $6f,sp
$ff8a39 6c 75 6d      jmp ($6d75)
$ff8a3c 6e f3 54      ror $54f3
$ff8a3f 65 78         adc $78
$ff8a41 f4 42 61      pea $6142
$ff8a44 63 6b         adc $6b,sp
$ff8a46 67 72         adc [$72]
$ff8a48 6f 75 6e e4   adc $e46e75
$ff8a4c 42            ???
$ff8a4d 6f 72 64 65   adc $656472
$ff8a51 f2 53         sbc ($53)
$ff8a53 74 61         stz $61,x
$ff8a55 6e 64 61      ror $6164
$ff8a58 72 64         adc ($64)
$ff8a5a f3 42         sbc ($42,sp),y
$ff8a5c 6c 61 63      jmp ($6361)
$ff8a5f eb            xba
$ff8a60 44 65 65      mvp $65,$65
$ff8a63 70 20         bvs $ff8a85
$ff8a65 52 65         eor ($65)
$ff8a67 e4 44         cpx $44
$ff8a69 61 72         adc ($72,x)
$ff8a6b 6b            rtl
$ff8a6c 20 42 6c      jsr $6c42
$ff8a6f 75 e5         adc $e5,x
$ff8a71 50 75         bvc $ff8ae8
$ff8a73 72 70         adc ($70)
$ff8a75 6c e5 44      jmp ($44e5)
$ff8a78 61 72         adc ($72,x)
$ff8a7a 6b            rtl
$ff8a7b 20 47 72      jsr $7247
$ff8a7e 65 65         adc $65
$ff8a80 ee 44 61      inc $6144
$ff8a83 72 6b         adc ($6b)
$ff8a85 20 47 72      jsr $7247
$ff8a88 61 f9         adc ($f9,x)
$ff8a8a 4d 65 64      eor $6465
$ff8a8d 69 75         adc #$75
$ff8a8f 6d 20 42      adc $4220
$ff8a92 6c 75 e5      jmp ($e575)
$ff8a95 4c 69 67      jmp $6769
$ff8a98 68            pla
$ff8a99 74 20         stz $20,x
$ff8a9b 42            ???
$ff8a9c 6c 75 e5      jmp ($e575)
$ff8a9f 42            ???
$ff8aa0 72 6f         adc ($6f)
$ff8aa2 77 ee         adc [$ee],y
$ff8aa4 4f 72 61 6e   eor $6e6172
$ff8aa8 67 e5         adc [$e5]
$ff8aaa 4c 69 67      jmp $6769
$ff8aad 68            pla
$ff8aae 74 20         stz $20,x
$ff8ab0 47 72         eor [$72]
$ff8ab2 61 f9         adc ($f9,x)
$ff8ab4 50 69         bvc $ff8b1f
$ff8ab6 6e eb 4c      ror $4ceb
$ff8ab9 69 67         adc #$67
$ff8abb 68            pla
$ff8abc 74 20         stz $20,x
$ff8abe 47 72         eor [$72]
$ff8ac0 65 65         adc $65
$ff8ac2 ee 59 65      inc $6559
$ff8ac5 6c 6c 6f      jmp ($6f6c)
$ff8ac8 f7 41         sbc [$41],y
$ff8aca 71 75         adc ($75),y
$ff8acc 61 6d         adc ($6d,x)
$ff8ace 61 72         adc ($72,x)
$ff8ad0 69 6e         adc #$6e
$ff8ad2 e5 57         sbc $57
$ff8ad4 68            pla
$ff8ad5 69 74         adc #$74
$ff8ad7 e5 53         sbc $53
$ff8ad9 6c 6f 74      jmp ($746f)
$ff8adc 20 b1 00      jsr $00b1
$ff8adf 53 6c         eor ($6c,sp),y
$ff8ae1 6f 74 20 b2   adc $b22074
$ff8ae5 00 53         brk $53
$ff8ae7 6c 6f 74      jmp ($746f)
$ff8aea 20 b3 00      jsr $00b3
$ff8aed 53 6c         eor ($6c,sp),y
$ff8aef 6f 74 20 b4   adc $b42074
$ff8af3 00 53         brk $53
$ff8af5 6c 6f 74      jmp ($746f)
$ff8af8 20 b5 00      jsr $00b5
$ff8afb 53 6c         eor ($6c,sp),y
$ff8afd 6f 74 20 b6   adc $b62074
$ff8b01 00 53         brk $53
$ff8b03 6c 6f 74      jmp ($746f)
$ff8b06 20 b7 00      jsr $00b7
$ff8b09 53 74         eor ($74,sp),y
$ff8b0b 61 72         adc ($72,x)
$ff8b0d 74 55         stz $55,x
$ff8b0f f0 44         beq $ff8b55
$ff8b11 69 73         adc #$73
$ff8b13 70 6c         bvs $ff8b81
$ff8b15 61 79         adc ($79,x)
$ff8b17 20 4c 61      jsr $614c
$ff8b1a 6e 67 75      ror $7567
$ff8b1d 61 67         adc ($67,x)
$ff8b1f e5 4b         sbc $4b
$ff8b21 65 79         adc $79
$ff8b23 62 6f 61      per $ffec95
$ff8b26 72 64         adc ($64)
$ff8b28 20 4c 61      jsr $614c
$ff8b2b 79 6f 75      adc $756f,y
$ff8b2e f4 4b 65      pea $654b
$ff8b31 79 62 6f      adc $6f62,y
$ff8b34 61 72         adc ($72,x)
$ff8b36 64 20         stz $20
$ff8b38 42            ???
$ff8b39 75 66         adc $66,x
$ff8b3b 66 65         ror $65
$ff8b3d 72 69         adc ($69)
$ff8b3f 6e e7 52      ror $52e7
$ff8b42 65 70         adc $70
$ff8b44 65 61         adc $61
$ff8b46 74 20         stz $20,x
$ff8b48 53 70         eor ($70,sp),y
$ff8b4a 65 65         adc $65
$ff8b4c e4 52         cpx $52
$ff8b4e 65 70         adc $70
$ff8b50 65 61         adc $61
$ff8b52 74 20         stz $20,x
$ff8b54 44 65 6c      mvp $65,$6c
$ff8b57 61 f9         adc ($f9,x)
$ff8b59 43 75         eor $75,sp
$ff8b5b 72 73         adc ($73)
$ff8b5d 6f 72 20 46   adc $462072
$ff8b61 6c 61 73      jmp ($7361)
$ff8b64 e8            inx
$ff8b65 53 68         eor ($68,sp),y
$ff8b67 69 66         adc #$66
$ff8b69 74 20         stz $20,x
$ff8b6b 43 61         eor $61,sp
$ff8b6d 70 73         bvs $ff8be2
$ff8b6f 2f 4c 6f 77   and $776f4c
$ff8b73 65 72         adc $72
$ff8b75 63 61         adc $61,sp
$ff8b77 73 e5         adc ($e5,sp),y
$ff8b79 46 61         lsr $61
$ff8b7b 73 74         adc ($74,sp),y
$ff8b7d 20 53 70      jsr $7053
$ff8b80 61 63         adc ($63,x)
$ff8b82 65 2f         adc $2f
$ff8b84 44 65 6c      mvp $65,$6c
$ff8b87 65 74         adc $74
$ff8b89 65 20         adc $20
$ff8b8b 4b            phk
$ff8b8c 65 79         adc $79
$ff8b8e f3 44         sbc ($44,sp),y
$ff8b90 75 61         adc $61,x
$ff8b92 6c 20 53      jmp ($5320)
$ff8b95 70 65         bvs $ff8bfc
$ff8b97 65 64         adc $64
$ff8b99 20 4b 65      jsr $654b
$ff8b9c 79 f3 4d      adc $4df3,y
$ff8b9f 6f 6e 74 e8   adc $e8746e
$ff8ba3 44 61 f9      mvp $61,$f9
$ff8ba6 59 65 61      eor $6165,y
$ff8ba9 f2 46         sbc ($46)
$ff8bab 6f 72 6d 61   adc $616d72
$ff8baf f4 48 6f      pea $6f48
$ff8bb2 75 f2         adc $f2,x
$ff8bb4 4d 69 6e      eor $6e69
$ff8bb7 75 74         adc $74,x
$ff8bb9 e5 53         sbc $53
$ff8bbb 65 63         adc $63
$ff8bbd 6f 6e e4 53   adc $53e46e
$ff8bc1 65 6c         adc $6c
$ff8bc3 65 63         adc $63
$ff8bc5 74 20         stz $20,x
$ff8bc7 52 41         eor ($41)
$ff8bc9 4d 20 20      eor $2020
$ff8bcc 44 69 73      mvp $69,$73
$ff8bcf 6b            rtl
$ff8bd0 20 20 53      jsr $5320
$ff8bd3 69 7a         adc #$7a
$ff8bd5 e5 52         sbc $52
$ff8bd7 41 4d         eor ($4d,x)
$ff8bd9 20 44 69      jsr $6944
$ff8bdc 73 6b         adc ($6b,sp),y
$ff8bde 20 53 69      jsr $6953
$ff8be1 7a            ply
$ff8be2 65 3a         adc $3a
$ff8be4 09 20         ora #$20
$ff8be6 a0 54         ldy #$54
$ff8be8 6f 74 61 6c   adc $6c6174
$ff8bec 20 52 41      jsr $4152
$ff8bef 4d 20 69      eor $6920
$ff8bf2 6e 20 55      ror $5520
$ff8bf5 73 65         adc ($65,sp),y
$ff8bf7 3a            dec
$ff8bf8 06 20         asl $20
$ff8bfa a0 54         ldy #$54
$ff8bfc 6f 74 61 6c   adc $6c6174
$ff8c00 20 46 72      jsr $7246
$ff8c03 65 65         adc $65
$ff8c05 20 52 41      jsr $4152
$ff8c08 4d 3a 08      eor $083a
$ff8c0b 20 a0 52      jsr $52a0
$ff8c0e 65 73         adc $73
$ff8c10 69 7a         adc #$7a
$ff8c12 65 20         adc $20
$ff8c14 61 66         adc ($66,x)
$ff8c16 74 65         stz $65,x
$ff8c18 72 20         adc ($20)
$ff8c1a 52 65         eor ($65)
$ff8c1c 73 65         adc ($65,sp),y
$ff8c1e f4 56 6f      pea $6f56
$ff8c21 6c 75 6d      jmp ($6d75)
$ff8c24 e5 20         sbc $20
$ff8c26 50 69         bvc $ff8c91
$ff8c28 74 63         stz $63,x
$ff8c2a e8            inx
$ff8c2b 4d 6f 75      eor $756f
$ff8c2e 73 65         adc ($65,sp),y
$ff8c30 20 54 72      jsr $7254
$ff8c33 61 63         adc ($63,x)
$ff8c35 6b            rtl
$ff8c36 69 6e         adc #$6e
$ff8c38 e7 44         sbc [$44]
$ff8c3a 6f 75 62 6c   adc $6c6275
$ff8c3e 65 20         adc $20
$ff8c40 43 6c         eor $6c,sp
$ff8c42 69 63         adc #$63
$ff8c44 6b            rtl
$ff8c45 20 a0 44      jsr $44a0
$ff8c48 65 6c         adc $6c
$ff8c4a 61 79         adc ($79,x)
$ff8c4c 2d 74 6f      and $6f74
$ff8c4f 2d 53 74      and $7453
$ff8c52 61 72         adc ($72,x)
$ff8c54 f4 41 63      pea $6341
$ff8c57 63 65         adc $65,sp
$ff8c59 6c 65 72      jmp ($7265)
$ff8c5c 61 74         adc ($74,x)
$ff8c5e 69 6f         adc #$6f
$ff8c60 6e 20 a0      ror $a020
$ff8c63 4d 61 78      eor $7861
$ff8c66 69 6d         adc #$6d
$ff8c68 75 6d         adc $6d,x
$ff8c6a 20 53 70      jsr $7053
$ff8c6d 65 65         adc $65
$ff8c6f 64 a0         stz $a0
$ff8c71 2d 4b 65      and $654b
$ff8c74 79 62 6f      adc $6f62,y
$ff8c77 61 72         adc ($72,x)
$ff8c79 64 20         stz $20
$ff8c7b 4d 6f 75      eor $756f
$ff8c7e 73 65         adc ($65,sp),y
$ff8c80 ad 50 72      lda $7250
$ff8c83 69 6e         adc #$6e
$ff8c85 74 65         stz $65,x
$ff8c87 72 ad         adc ($ad)
$ff8c89 4d 6f 64      eor $646f
$ff8c8c 65 6d         adc $6d
$ff8c8e ad 41 70      lda $7041
$ff8c91 70 6c         bvs $ff8cff
$ff8c93 65 54         adc $54
$ff8c95 61 6c         adc ($6c,x)
$ff8c97 6b            rtl
$ff8c98 ad 59 6f      lda $6f59
$ff8c9b 75 72         adc $72,x
$ff8c9d 20 43 61      jsr $6143
$ff8ca0 72 64         adc ($64)
$ff8ca2 ad 55 6e      lda $6e55
$ff8ca5 6c 69 6d      jmp ($6d69)
$ff8ca8 69 74         adc #$74
$ff8caa 65 e4         adc $e4
$ff8cac 34 b0         bit $b0,x
$ff8cae 37 b2         and [$b2],y
$ff8cb0 38            sec
$ff8cb1 b0 31         bcs $ff8ce4
$ff8cb3 33 b2         and ($b2,sp),y
$ff8cb5 4e ef 59      lsr $59ef
$ff8cb8 65 f3         adc $f3
$ff8cba 35 b0         and $b0,x
$ff8cbc 37 b5         and [$b5],y
$ff8cbe 31 31         and ($31),y
$ff8cc0 b0 31         bcs $ff8cf3
$ff8cc2 33 34         and ($34,sp),y
$ff8cc4 2e b5 31      rol $31b5
$ff8cc7 35 b0         and $b0,x
$ff8cc9 33 30         and ($30,sp),y
$ff8ccb b0 36         bcs $ff8d03
$ff8ccd 30 b0         bmi $ff8c7f
$ff8ccf 31 32         and ($32),y
$ff8cd1 30 b0         bmi $ff8c83
$ff8cd3 31 38         and ($38),y
$ff8cd5 30 b0         bmi $ff8c87
$ff8cd7 32 34         and ($34)
$ff8cd9 30 b0         bmi $ff8c8b
$ff8cdb 33 36         and ($36,sp),y
$ff8cdd 30 b0         bmi $ff8c8f
$ff8cdf 34 38         bit $38,x
$ff8ce1 30 b0         bmi $ff8c93
$ff8ce3 37 32         and [$32],y
$ff8ce5 30 b0         bmi $ff8c97
$ff8ce7 39 36 30      and $3036,y
$ff8cea b0 31         bcs $ff8d1d
$ff8cec 39 32 30      and $3032,y
$ff8cef b0 35         bcs $ff8d26
$ff8cf1 2f b1 35 2f   and $2f35b1
$ff8cf5 b2 36         lda ($36)
$ff8cf7 2f b1 36 2f   and $2f36b1
$ff8cfb b2 37         lda ($37)
$ff8cfd 2f b1 37 2f   and $2f37b1
$ff8d01 b2 38         lda ($38)
$ff8d03 2f b1 38 2f   and $2f38b1
$ff8d07 b2 4f         lda ($4f)
$ff8d09 64 e4         stz $e4
$ff8d0b 45 76         eor $76
$ff8d0d 65 ee         adc $ee
$ff8d0f 4e 6f 6e      lsr $6e6f
$ff8d12 e5 43         sbc $43
$ff8d14 6f 6c 6f f2   adc $f26f6c
$ff8d18 4d 6f 6e      eor $6e6f
$ff8d1b 6f 63 68 72   adc $726863
$ff8d1f 6f 6d e5 36   adc $36e56d
$ff8d23 b0 55         bcs $ff8d7a
$ff8d25 2e 53 2e      rol $2e53
$ff8d28 41 ae         eor ($ae,x)
$ff8d2a 55 2e         eor $2e,x
$ff8d2c 4b            phk
$ff8d2d ae 46 72      ldx $7246
$ff8d30 65 6e         adc $6e
$ff8d32 63 e8         adc $e8,sp
$ff8d34 44 61 6e      mvp $61,$6e
$ff8d37 69 73         adc #$73
$ff8d39 e8            inx
$ff8d3a 53 70         eor ($70,sp),y
$ff8d3c 61 6e         adc ($6e,x)
$ff8d3e 69 73         adc #$73
$ff8d40 e8            inx
$ff8d41 49 74         eor #$74
$ff8d43 61 6c         adc ($6c,x)
$ff8d45 69 61         adc #$61
$ff8d47 ee 47 65      inc $6547
$ff8d4a 72 6d         adc ($6d)
$ff8d4c 61 ee         adc ($ee,x)
$ff8d4e 53 77         eor ($77,sp),y
$ff8d50 65 64         adc $64
$ff8d52 69 73         adc #$73
$ff8d54 e8            inx
$ff8d55 44 76 6f      mvp $76,$6f
$ff8d58 72 61         adc ($61)
$ff8d5a eb            xba
$ff8d5b 46 72         lsr $72
$ff8d5d 65 6e         adc $6e
$ff8d5f 63 68         adc $68,sp
$ff8d61 20 43 61      jsr $6143
$ff8d64 6e 61 64      ror $6461
$ff8d67 69 61         adc #$61
$ff8d69 ee 46 6c      inc $6c46
$ff8d6c 65 6d         adc $6d
$ff8d6e 69 73         adc #$73
$ff8d70 e8            inx
$ff8d71 48            pha
$ff8d72 65 62         adc $62
$ff8d74 72 65         adc ($65)
$ff8d76 f7 4a         sbc [$4a],y
$ff8d78 61 70         adc ($70,x)
$ff8d7a 61 6e         adc ($6e,x)
$ff8d7c 65 73         adc $73
$ff8d7e e5 41         sbc $41
$ff8d80 72 61         adc ($61)
$ff8d82 62 69 e3      per $ff70ee
$ff8d85 47 72         eor [$72]
$ff8d87 65 65         adc $65
$ff8d89 eb            xba
$ff8d8a 54 75 72      mvn $75,$72
$ff8d8d 6b            rtl
$ff8d8e 69 73         adc #$73
$ff8d90 e8            inx
$ff8d91 46 69         lsr $69
$ff8d93 6e 6e 69      ror $696e
$ff8d96 73 e8         adc ($e8,sp),y
$ff8d98 50 6f         bvc $ff8e09
$ff8d9a 72 74         adc ($74)
$ff8d9c 75 67         adc $67,x
$ff8d9e 75 65         adc $65,x
$ff8da0 73 e5         adc ($e5,sp),y
$ff8da2 54 61 6d      mvn $61,$6d
$ff8da5 69 ec         adc #$ec
$ff8da7 48            pha
$ff8da8 69 6e         adc #$6e
$ff8daa 64 f5         stz $f5
$ff8dac 54 b1 54      mvn $b1,$54
$ff8daf b2 54         lda ($54)
$ff8db1 b3 54         lda ($54,sp),y
$ff8db3 b4 54         ldy $54,x
$ff8db5 b5 54         lda $54,x
$ff8db7 b6 4c         ldx $4c,y
$ff8db9 b1 4c         lda ($4c),y
$ff8dbb b2 4c         lda ($4c)
$ff8dbd b3 4c         lda ($4c,sp),y
$ff8dbf b4 4c         ldy $4c,x
$ff8dc1 b5 4c         lda $4c,x
$ff8dc3 b6 4e         ldx $4e,y
$ff8dc5 6f 72 6d 61   adc $616d72
$ff8dc9 ec 46 61      cpx $6146
$ff8dcc 73 f4         adc ($f4,sp),y
$ff8dce 59 6f 75      eor $756f,y
$ff8dd1 72 20         adc ($20)
$ff8dd3 43 61         eor $61,sp
$ff8dd5 72 e4         adc ($e4)
$ff8dd7 50 72         bvc $ff8e4b
$ff8dd9 69 6e         adc #$6e
$ff8ddb 74 65         stz $65,x
$ff8ddd f2 4d         sbc ($4d)
$ff8ddf 6f 64 65 ed   adc $ed6564
$ff8de3 42            ???
$ff8de4 75 69         adc $69,x
$ff8de6 6c 74 2d      jmp ($2d74)
$ff8de9 69 6e         adc #$6e
$ff8deb 20 54 65      jsr $6554
$ff8dee 78            sei
$ff8def 74 20         stz $20,x
$ff8df1 44 69 73      mvp $69,$73
$ff8df4 70 6c         bvs $ff8e62
$ff8df6 61 f9         adc ($f9,x)
$ff8df8 4d 6f 75      eor $756f
$ff8dfb 73 65         adc ($65,sp),y
$ff8dfd 20 50 6f      jsr $6f50
$ff8e00 72 f4         adc ($f4)
$ff8e02 53 6d         eor ($6d,sp),y
$ff8e04 61 72         adc ($72,x)
$ff8e06 74 20         stz $20,x
$ff8e08 50 6f         bvc $ff8e79
$ff8e0a 72 f4         adc ($f4)
$ff8e0c 44 69 73      mvp $69,$73
$ff8e0f 6b            rtl
$ff8e10 20 50 6f      jsr $6f50
$ff8e13 72 f4         adc ($f4)
$ff8e15 41 70         eor ($70,x)
$ff8e17 70 6c         bvs $ff8e85
$ff8e19 65 54         adc $54
$ff8e1b 61 6c         adc ($6c,x)
$ff8e1d eb            xba
$ff8e1e 53 63         eor ($63,sp),y
$ff8e20 61 ee         adc ($ee,x)
$ff8e22 52 4f         eor ($4f)
$ff8e24 4d 20 44      eor $4420
$ff8e27 69 73         adc #$73
$ff8e29 eb            xba
$ff8e2a 4d 4d 2f      eor $2f4d
$ff8e2d 44 44 2f      mvp $44,$2f
$ff8e30 59 d9 44      eor $44d9,y
$ff8e33 44 2f 4d      mvp $2f,$4d
$ff8e36 4d 2f 59      eor $592f
$ff8e39 d9 59 59      cmp $5959,y
$ff8e3c 2f 4d 4d 2f   and $2f4d4d
$ff8e40 44 c4 41      mvp $c4,$41
$ff8e43 4d 2d 50      eor $502d
$ff8e46 cd 32 34      cmp $3432
$ff8e49 20 48 6f      jsr $6f48
$ff8e4c 75 f2         adc $f2,x
$ff8e4e 03 a0         ora $a0,sp
$ff8e50 d4 f5         pei ($f5)
$ff8e52 f2 ee         sbc ($ee)
$ff8e54 00 20         brk $20
$ff8e56 41 6c         eor ($6c,x)
$ff8e58 74 65         stz $65,x
$ff8e5a 72 6e         adc ($6e)
$ff8e5c 61 74         adc ($74,x)
$ff8e5e 65 20         adc $20
$ff8e60 44 69 73      mvp $69,$73
$ff8e63 70 6c         bvs $ff8ed1
$ff8e65 61 79         adc ($79,x)
$ff8e67 20 4d 6f      jsr $6f4d
$ff8e6a 64 e5         stz $e5
$ff8e6c 20 4f ee      jsr $ee4f
$ff8e6f 20 4f 66      jsr $664f
$ff8e72 e6 10         inc $10
$ff8e74 a0 c1         ldy #$c1
$ff8e76 e3 e3         sbc $e3,sp
$ff8e78 e5 f0         sbc $f0
$ff8e7a f4 a0 4d      pea $4da0
$ff8e7d 00 57         brk $57
$ff8e7f 65 6c         adc $6c
$ff8e81 63 6f         adc $6f,sp
$ff8e83 6d 65 2e      adc $2e65
$ff8e86 2e 2e 43      rol $432e
$ff8e89 6f 6e 74 72   adc $72746e
$ff8e8d 6f 6c 2d 59   adc $592d6c
$ff8e91 20 52 65      jsr $6552
$ff8e94 74 75         stz $75,x
$ff8e96 72 6e         adc ($6e)
$ff8e98 20 65 78      jsr $7865
$ff8e9b 69 74         adc #$74
$ff8e9d f3 ad         sbc ($ad,sp),y
$ff8e9f 15 c0         ora $c0,x
$ff8ea1 48            pha
$ff8ea2 ad 36 c0      lda $c036
$ff8ea5 48            pha
$ff8ea6 20 ee a9      jsr $a9ee
$ff8ea9 20 11 9a      jsr $9a11
$ff8eac a2 01         ldx #$01
$ff8eae a0 01         ldy #$01
$ff8eb0 a9 ee         lda #$ee
$ff8eb2 20 d1 93      jsr $93d1
$ff8eb5 20 40 96      jsr $9640
$ff8eb8 a2 01         ldx #$01
$ff8eba a0 0b         ldy #$0b
$ff8ebc a9 ed         lda #$ed
$ff8ebe 20 d1 93      jsr $93d1
$ff8ec1 a9 ee         lda #$ee
$ff8ec3 20 80 c0      jsr $c080
$ff8ec6 af 7c 01 e1   lda $e1017c
$ff8eca d0 04         bne $ff8ed0
$ff8ecc a9 ef         lda #$ef
$ff8ece 80 02         bra $ff8ed2
$ff8ed0 a9 f0         lda #$f0
$ff8ed2 20 80 c0      jsr $c080
$ff8ed5 a2 01         ldx #$01
$ff8ed7 a0 16         ldy #$16
$ff8ed9 a9 14         lda #$14
$ff8edb 20 d1 93      jsr $93d1
$ff8ede a9 f1         lda #$f1
$ff8ee0 20 80 c0      jsr $c080
$ff8ee3 20 2e 96      jsr $962e
$ff8ee6 20 74 cf      jsr $cf74
$ff8ee9 10 fb         bpl $ff8ee6
$ff8eeb c9 8d         cmp #$8d
$ff8eed f0 09         beq $ff8ef8
$ff8eef c9 9b         cmp #$9b
$ff8ef1 f0 08         beq $ff8efb
$ff8ef3 20 51 9a      jsr $9a51
$ff8ef6 80 ee         bra $ff8ee6
$ff8ef8 20 02 8f      jsr $8f02
$ff8efb 68            pla
$ff8efc 8d 36 c0      sta $c036
$ff8eff 4c e5 90      jmp $90e5
$ff8f02 8b            phb
$ff8f03 20 82 f8      jsr $f882
$ff8f06 ad 7c 01      lda $017c
$ff8f09 49 ff         eor #$ff
$ff8f0b 8d 7c 01      sta $017c
$ff8f0e 20 2e 7a      jsr $7a2e
$ff8f11 90 02         bcc $ff8f15
$ff8f13 ab            plb
$ff8f14 60            rts
$ff8f15 ab            plb
$ff8f16 e2 40         sep #$40
$ff8f18 18            clc
$ff8f19 fb            xce
$ff8f1a 08            php
$ff8f1b c2 30         rep #$30
$ff8f1d 50 b8         bvc $ff8ed7
$ff8f1f f4 e1 00      pea $00e1
$ff8f22 f4 70 01      pea $0170
$ff8f25 a2 03 12      ldx #$1203
$ff8f28 70 03         bvs $ff8f2d
$ff8f2a a2 03 13      ldx #$1303
$ff8f2d 22 00 00 e1   jsr $e10000
$ff8f31 b0 eb         bcs $ff8f1e
$ff8f33 28            plp
$ff8f34 a9 08 0c      lda #$0c08
$ff8f37 41 c0         eor ($c0,x)
$ff8f39 fb            xce
$ff8f3a 60            rts
$ff8f3b 78            sei
$ff8f3c a9 10 1c      lda #$1c10
$ff8f3f 27 c0         and [$c0]
$ff8f41 58            cli
$ff8f42 20 2c fa      jsr $fa2c
$ff8f45 f0 03         beq $ff8f4a
$ff8f47 8d 0a c0      sta $c00a
$ff8f4a a9 11 20      lda #$2011
$ff8f4d e9 a9 20      sbc #$20a9
$ff8f50 11 9a         ora ($9a),y
$ff8f52 a9 80 8d      lda #$8d80
$ff8f55 36 c0         rol $c0,x
$ff8f57 8f 38 01 e1   sta $e10138
$ff8f5b 0a            asl
$ff8f5c 9c f3 03      stz $03f3
$ff8f5f 9c f4 03      stz $03f4
$ff8f62 a0 08 a2      ldy #$a208
$ff8f65 00 a9         brk $a9
$ff8f67 0d 20 d1      ora $d120
$ff8f6a 93 20         sta ($20,sp),y
$ff8f6c 2e 96 8d      rol $8d96
$ff8f6f 06 c0         asl $c0
$ff8f71 ad 00 c0      lda $c000
$ff8f74 10 fb         bpl $ff8f71
$ff8f76 8d 10 c0      sta $c010
$ff8f79 c9 b1 f0      cmp #$f0b1
$ff8f7c 24 c9         bit $c9
$ff8f7e b2 f0         lda ($f0)
$ff8f80 0d c9 b3      ora $b3c9
$ff8f83 f0 0d         beq $ff8f92
$ff8f85 c9 b4 f0      cmp #$f0b4
$ff8f88 17 20         ora [$20],y
$ff8f8a 51 9a         eor ($9a),y
$ff8f8c 80 e3         bra $ff8f71
$ff8f8e 5c 25 b9 ff   jmp $ffb925
$ff8f92 22 25 b9 ff   jsr $ffb925
$ff8f96 a9 01 8f      lda #$8f01
$ff8f99 dd 02 e1      cmp $e102,x
$ff8f9c 5c 4e b9 ff   jmp $ffb94e
$ff8fa0 6b            rtl
$ff8fa1 af d9 02 e1   lda $e102d9
$ff8fa5 f0 05         beq $ff8fac
$ff8fa7 a9 1b 20      lda #$201b
$ff8faa e9 a9 ad      sbc #$ada9
$ff8fad 15 c0         ora $c0,x
$ff8faf 48            pha
$ff8fb0 08            php
$ff8fb1 78            sei
$ff8fb2 ad 27 c0      lda $c027
$ff8fb5 85 b0         sta $b0
$ff8fb7 ad 23 c0      lda $c023
$ff8fba 85 b1         sta $b1
$ff8fbc a9 04 1c      lda #$1c04
$ff8fbf 27 c0         and [$c0]
$ff8fc1 0c 23 c0      tsb $c023
$ff8fc4 9c 32 c0      stz $c032
$ff8fc7 c2 20         rep #$20
$ff8fc9 af 54 00 e1   lda $e10054
$ff8fcd 85 b2         sta $b2
$ff8fcf af 56 00 e1   lda $e10056
$ff8fd3 85 b4         sta $b4
$ff8fd5 4b            phk
$ff8fd6 a2 81 da      ldx #$da81
$ff8fd9 68            pla
$ff8fda 8f 56 00 e1   sta $e10056
$ff8fde a9 5c c2      lda #$c25c
$ff8fe1 8f 54 00 e1   sta $e10054
$ff8fe5 e2 30         sep #$30
$ff8fe7 28            plp
$ff8fe8 22 70 b9 ff   jsr $ffb970
$ff8fec 38            sec
$ff8fed 22 94 00 e1   jsr $e10094
$ff8ff1 a2 02         ldx #$02
$ff8ff3 20 e6 a6      jsr $a6e6
$ff8ff6 a2 0d         ldx #$0d
$ff8ff8 0c 22 00      tsb $0022
$ff8ffb 00 e1         brk $e1
$ff8ffd a3 01         lda $01,sp
$ff8fff 85 5c         sta $5c
$ff9001 a3 03         lda $03,sp
$ff9003 85 5e         sta $5e
$ff9005 a2 02         ldx #$02
$ff9007 1d 22 00      ora $0022,x
$ff900a 00 e1         brk $e1
$ff900c a3 01         lda $01,sp
$ff900e 85 50         sta $50
$ff9010 85 58         sta $58
$ff9012 a3 03         lda $03,sp
$ff9014 85 52         sta $52
$ff9016 38            sec
$ff9017 e9 04         sbc #$04
$ff9019 00 85         brk $85
$ff901b 5a            phy
$ff901c d0 0c         bne $ff902a
$ff901e 8f f6 02 e1   sta $e102f6
$ff9022 e2 30         sep #$30
$ff9024 22 80 00 e1   jsr $e10080
$ff9028 c2 30         rep #$30
$ff902a a2 02 1b      ldx #$1b02
$ff902d 22 00 00 e1   jsr $e10000
$ff9031 a3 01         lda $01,sp
$ff9033 85 54         sta $54
$ff9035 a3 03         lda $03,sp
$ff9037 85 56         sta $56
$ff9039 a5 50         lda $50
$ff903b 38            sec
$ff903c e5 54         sbc $54
$ff903e 85 60         sta $60
$ff9040 a5 52         lda $52
$ff9042 e5 56         sbc $56
$ff9044 85 62         sta $62
$ff9046 68            pla
$ff9047 68            pla
$ff9048 e2 30         sep #$30
$ff904a 20 11 9a      jsr $9a11
$ff904d a0 00         ldy #$00
$ff904f 20 95 98      jsr $9895
$ff9052 a2 01         ldx #$01
$ff9054 a0 00         ldy #$00
$ff9056 20 bd 93      jsr $93bd
$ff9059 a9 df         lda #$df
$ff905b 20 c7 94      jsr $94c7
$ff905e a9 0b         lda #$0b
$ff9060 85 00         sta $00
$ff9062 78            sei
$ff9063 a5 00         lda $00
$ff9065 85 01         sta $01
$ff9067 a9 0b         lda #$0b
$ff9069 85 00         sta $00
$ff906b a0 01         ldy #$01
$ff906d 20 95 98      jsr $9895
$ff9070 20 d7 93      jsr $93d7
$ff9073 20 40 96      jsr $9640
$ff9076 20 a8 98      jsr $98a8
$ff9079 20 06 94      jsr $9406
$ff907c a9 04         lda #$04
$ff907e 85 25         sta $25
$ff9080 a9 1b         lda #$1b
$ff9082 48            pha
$ff9083 e6 25         inc $25
$ff9085 38            sec
$ff9086 e9 1b         sbc #$1b
$ff9088 c5 01         cmp $01
$ff908a d0 03         bne $ff908f
$ff908c 20 20 9a      jsr $9a20
$ff908f a4 25         ldy $25
$ff9091 a2 04         ldx #$04
$ff9093 a3 01         lda $01,sp
$ff9095 20 d1 93      jsr $93d1
$ff9098 20 11 9a      jsr $9a11
$ff909b 68            pla
$ff909c 1a            inc
$ff909d c9 27         cmp #$27
$ff909f 90 e1         bcc $ff9082
$ff90a1 20 33 82      jsr $8233
$ff90a4 c9 8d         cmp #$8d
$ff90a6 f0 13         beq $ff90bb
$ff90a8 c9 9b         cmp #$9b
$ff90aa f0 09         beq $ff90b5
$ff90ac c9 00         cmp #$00
$ff90ae f0 cc         beq $ff907c
$ff90b0 20 51 9a      jsr $9a51
$ff90b3 80 c7         bra $ff907c
$ff90b5 a9 0b         lda #$0b
$ff90b7 85 01         sta $01
$ff90b9 80 c1         bra $ff907c
$ff90bb a6 01         ldx $01
$ff90bd 86 00         stx $00
$ff90bf e0 0b         cpx #$0b
$ff90c1 d0 29         bne $ff90ec
$ff90c3 08            php
$ff90c4 78            sei
$ff90c5 a5 b1         lda $b1
$ff90c7 8d 23 c0      sta $c023
$ff90ca a5 b0         lda $b0
$ff90cc 8d 27 c0      sta $c027
$ff90cf c2 30         rep #$30
$ff90d1 a5 b2         lda $b2
$ff90d3 8f 54 00 e1   sta $e10054
$ff90d7 a5 b4         lda $b4
$ff90d9 8f 56 00 e1   sta $e10056
$ff90dd e2 30         sep #$30
$ff90df 28            plp
$ff90e0 38            sec
$ff90e1 22 94 00 e1   jsr $e10094
$ff90e5 68            pla
$ff90e6 30 03         bmi $ff90eb
$ff90e8 8d 06 c0      sta $c006
$ff90eb 6b            rtl
$ff90ec 78            sei
$ff90ed af f8 02 e1   lda $e102f8
$ff90f1 8f f7 02 e1   sta $e102f7
$ff90f5 af 18 03 e1   lda $e10318
$ff90f9 8f e1 02 e1   sta $e102e1
$ff90fd 20 d7 93      jsr $93d7
$ff9100 20 c3 94      jsr $94c3
$ff9103 a2 00         ldx #$00
$ff9105 a0 02         ldy #$02
$ff9107 20 bd 93      jsr $93bd
$ff910a 20 b7 94      jsr $94b7
$ff910d a2 01         ldx #$01
$ff910f a0 04         ldy #$04
$ff9111 20 bb 94      jsr $94bb
$ff9114 a2 01         ldx #$01
$ff9116 a0 03         ldy #$03
$ff9118 20 bd 93      jsr $93bd
$ff911b 20 91 ac      jsr $ac91
$ff911e 18            clc
$ff911f a5 00         lda $00
$ff9121 69 1b         adc #$1b
$ff9123 20 80 c0      jsr $c080
$ff9126 20 bf 94      jsr $94bf
$ff9129 64 01         stz $01
$ff912b a0 04         ldy #$04
$ff912d 20 95 98      jsr $9895
$ff9130 a0 04         ldy #$04
$ff9132 20 48 94      jsr $9448
$ff9135 20 a8 98      jsr $98a8
$ff9138 20 06 94      jsr $9406
$ff913b a2 04         ldx #$04
$ff913d 20 e6 a6      jsr $a6e6
$ff9140 a2 03         ldx #$03
$ff9142 0d 22 00      ora $0022
$ff9145 00 e1         brk $e1
$ff9147 e2 30         sep #$30
$ff9149 68            pla
$ff914a 85 6a         sta $6a
$ff914c 68            pla
$ff914d 85 69         sta $69
$ff914f 68            pla
$ff9150 85 68         sta $68
$ff9152 68            pla
$ff9153 c9 64         cmp #$64
$ff9155 90 02         bcc $ff9159
$ff9157 e9 64         sbc #$64
$ff9159 85 66         sta $66
$ff915b 68            pla
$ff915c 85 65         sta $65
$ff915e 68            pla
$ff915f 85 64         sta $64
$ff9161 68            pla
$ff9162 68            pla
$ff9163 af f4 02 e1   lda $e102f4
$ff9167 85 67         sta $67
$ff9169 af f5 02 e1   lda $e102f5
$ff916d 85 6b         sta $6b
$ff916f 20 44 9a      jsr $9a44
$ff9172 d0 05         bne $ff9179
$ff9174 20 33 82      jsr $8233
$ff9177 80 07         bra $ff9180
$ff9179 64 02         stz $02
$ff917b 20 58 96      jsr $9658
$ff917e 64 01         stz $01
$ff9180 a5 01         lda $01
$ff9182 85 02         sta $02
$ff9184 48            pha
$ff9185 20 58 96      jsr $9658
$ff9188 68            pla
$ff9189 85 01         sta $01
$ff918b 20 33 82      jsr $8233
$ff918e c9 00         cmp #$00
$ff9190 f0 ee         beq $ff9180
$ff9192 c9 9b         cmp #$9b
$ff9194 d0 08         bne $ff919e
$ff9196 38            sec
$ff9197 22 94 00 e1   jsr $e10094
$ff919b 4c 62 90      jmp $9062
$ff919e c9 8d         cmp #$8d
$ff91a0 f0 03         beq $ff91a5
$ff91a2 4c 49 92      jmp $9249
$ff91a5 a5 00         lda $00
$ff91a7 c9 03         cmp #$03
$ff91a9 d0 26         bne $ff91d1
$ff91ab a5 64         lda $64
$ff91ad 48            pha
$ff91ae a5 65         lda $65
$ff91b0 48            pha
$ff91b1 a5 66         lda $66
$ff91b3 c9 28         cmp #$28
$ff91b5 b0 03         bcs $ff91ba
$ff91b7 18            clc
$ff91b8 69 64         adc #$64
$ff91ba 48            pha
$ff91bb a5 68         lda $68
$ff91bd 48            pha
$ff91be a5 69         lda $69
$ff91c0 48            pha
$ff91c1 a5 6a         lda $6a
$ff91c3 48            pha
$ff91c4 c2 30         rep #$30
$ff91c6 a2 03 0e      ldx #$0e03
$ff91c9 22 00 00 e1   jsr $e10000
$ff91cd e2 30         sep #$30
$ff91cf 80 0f         bra $ff91e0
$ff91d1 c9 08         cmp #$08
$ff91d3 d0 0b         bne $ff91e0
$ff91d5 c2 30         rep #$30
$ff91d7 a2 0d 0b      ldx #$0b0d
$ff91da 22 00 00 e1   jsr $e10000
$ff91de e2 30         sep #$30
$ff91e0 20 93 95      jsr $9593
$ff91e3 b0 9b         bcs $ff9180
$ff91e5 af f7 02 e1   lda $e102f7
$ff91e9 8f f8 02 e1   sta $e102f8
$ff91ed f0 06         beq $ff91f5
$ff91ef a9 00         lda #$00
$ff91f1 8f ff 15 e1   sta $e115ff
$ff91f5 af f6 02 e1   lda $e102f6
$ff91f9 8f f7 02 e1   sta $e102f7
$ff91fd af e1 02 e1   lda $e102e1
$ff9201 8f 18 03 e1   sta $e10318
$ff9205 aa            tax
$ff9206 f0 0d         beq $ff9215
$ff9208 3a            dec
$ff9209 f0 07         beq $ff9212
$ff920b 3a            dec
$ff920c f0 05         beq $ff9213
$ff920e 3a            dec
$ff920f f0 03         beq $ff9214
$ff9211 3a            dec
$ff9212 1a            inc
$ff9213 1a            inc
$ff9214 1a            inc
$ff9215 8f c0 02 e1   sta $e102c0
$ff9219 af e2 02 e1   lda $e102e2
$ff921d f0 0c         beq $ff922b
$ff921f 3a            dec
$ff9220 f0 07         beq $ff9229
$ff9222 3a            dec
$ff9223 f0 05         beq $ff922a
$ff9225 3a            dec
$ff9226 f0 04         beq $ff922c
$ff9228 3a            dec
$ff9229 1a            inc
$ff922a 1a            inc
$ff922b 1a            inc
$ff922c 8f cc 02 e1   sta $e102cc
$ff9230 af e7 02 e1   lda $e102e7
$ff9234 d0 0c         bne $ff9242
$ff9236 8a            txa
$ff9237 c9 02         cmp #$02
$ff9239 f0 02         beq $ff923d
$ff923b a9 01         lda #$01
$ff923d 3a            dec
$ff923e 8f e1 02 e1   sta $e102e1
$ff9242 22 80 00 e1   jsr $e10080
$ff9246 4c 96 91      jmp $9196
$ff9249 48            pha
$ff924a 24 1f         bit $1f
$ff924c 70 5b         bvs $ff92a9
$ff924e 20 8a 99      jsr $998a
$ff9251 fa            plx
$ff9252 20 5b 92      jsr $925b
$ff9255 20 93 95      jsr $9593
$ff9258 4c 80 91      jmp $9180
$ff925b e0 88         cpx #$88
$ff925d f0 2a         beq $ff9289
$ff925f aa            tax
$ff9260 20 d5 82      jsr $82d5
$ff9263 b0 0e         bcs $ff9273
$ff9265 a5 01         lda $01
$ff9267 f0 04         beq $ff926d
$ff9269 c9 01         cmp #$01
$ff926b d0 06         bne $ff9273
$ff926d 8a            txa
$ff926e d0 04         bne $ff9274
$ff9270 1a            inc
$ff9271 80 01         bra $ff9274
$ff9273 8a            txa
$ff9274 1a            inc
$ff9275 c5 05         cmp $05
$ff9277 90 02         bcc $ff927b
$ff9279 a9 00         lda #$00
$ff927b 97 ad         sta [$ad],y
$ff927d 18            clc
$ff927e a5 01         lda $01
$ff9280 aa            tax
$ff9281 65 a3         adc $a3
$ff9283 20 80 96      jsr $9680
$ff9286 4c b5 96      jmp $96b5
$ff9289 aa            tax
$ff928a d0 04         bne $ff9290
$ff928c a5 05         lda $05
$ff928e 80 16         bra $ff92a6
$ff9290 20 d5 82      jsr $82d5
$ff9293 b0 10         bcs $ff92a5
$ff9295 a5 01         lda $01
$ff9297 f0 04         beq $ff929d
$ff9299 c9 01         cmp #$01
$ff929b d0 08         bne $ff92a5
$ff929d 8a            txa
$ff929e c9 02         cmp #$02
$ff92a0 d0 04         bne $ff92a6
$ff92a2 3a            dec
$ff92a3 80 01         bra $ff92a6
$ff92a5 8a            txa
$ff92a6 3a            dec
$ff92a7 80 d2         bra $ff927b
$ff92a9 20 8a 99      jsr $998a
$ff92ac a6 00         ldx $00
$ff92ae f0 16         beq $ff92c6
$ff92b0 e0 01         cpx #$01
$ff92b2 f0 5e         beq $ff9312
$ff92b4 e0 03         cpx #$03
$ff92b6 f0 08         beq $ff92c0
$ff92b8 e0 08         cpx #$08
$ff92ba f0 07         beq $ff92c3
$ff92bc 68            pla
$ff92bd 4c 62 90      jmp $9062
$ff92c0 4c 61 93      jmp $9361
$ff92c3 4c 35 93      jmp $9335
$ff92c6 a5 01         lda $01
$ff92c8 c9 06         cmp #$06
$ff92ca d0 10         bne $ff92dc
$ff92cc 68            pla
$ff92cd a2 02         ldx #$02
$ff92cf bf df 83 ff   lda $ff83df,x
$ff92d3 9f da 02 e1   sta $e102da,x
$ff92d7 ca            dex
$ff92d8 10 f5         bpl $ff92cf
$ff92da 80 1b         bra $ff92f7
$ff92dc c9 02         cmp #$02
$ff92de b0 03         bcs $ff92e3
$ff92e0 4c 4e 92      jmp $924e
$ff92e3 20 8a 99      jsr $998a
$ff92e6 fa            plx
$ff92e7 e0 88         cpx #$88
$ff92e9 f0 1a         beq $ff9305
$ff92eb 1a            inc
$ff92ec c5 05         cmp $05
$ff92ee 90 02         bcc $ff92f2
$ff92f0 a9 00         lda #$00
$ff92f2 20 f4 99      jsr $99f4
$ff92f5 b0 f4         bcs $ff92eb
$ff92f7 a5 01         lda $01
$ff92f9 48            pha
$ff92fa a0 05         ldy #$05
$ff92fc 84 25         sty $25
$ff92fe 64 01         stz $01
$ff9300 20 18 97      jsr $9718
$ff9303 80 4a         bra $ff934f
$ff9305 aa            tax
$ff9306 d0 02         bne $ff930a
$ff9308 a5 05         lda $05
$ff930a 3a            dec
$ff930b 20 f4 99      jsr $99f4
$ff930e b0 f5         bcs $ff9305
$ff9310 80 e5         bra $ff92f7
$ff9312 fa            plx
$ff9313 20 5b 92      jsr $925b
$ff9316 ad 3c c0      lda $c03c
$ff9319 29 f0         and #$f0
$ff931b 0f de 02 e1   ora $e102de
$ff931f 8d 3c c0      sta $c03c
$ff9322 29 0f         and #$0f
$ff9324 8f ca 00 e1   sta $e100ca
$ff9328 0a            asl
$ff9329 0a            asl
$ff932a 0a            asl
$ff932b 0a            asl
$ff932c 8f b0 1d e1   sta $e11db0
$ff9330 20 51 9a      jsr $9a51
$ff9333 80 1d         bra $ff9352
$ff9335 fa            plx
$ff9336 e0 88         cpx #$88
$ff9338 f0 1b         beq $ff9355
$ff933a 1a            inc
$ff933b a6 05         ldx $05
$ff933d f0 06         beq $ff9345
$ff933f c5 05         cmp $05
$ff9341 90 02         bcc $ff9345
$ff9343 a9 00         lda #$00
$ff9345 97 ad         sta [$ad],y
$ff9347 a5 01         lda $01
$ff9349 48            pha
$ff934a 64 01         stz $01
$ff934c 20 a3 97      jsr $97a3
$ff934f 68            pla
$ff9350 85 01         sta $01
$ff9352 4c 80 91      jmp $9180
$ff9355 aa            tax
$ff9356 d0 06         bne $ff935e
$ff9358 a6 05         ldx $05
$ff935a f0 02         beq $ff935e
$ff935c a5 05         lda $05
$ff935e 3a            dec
$ff935f 80 e4         bra $ff9345
$ff9361 20 97 93      jsr $9397
$ff9364 b9 64 00      lda $0064,y
$ff9367 fa            plx
$ff9368 e0 88         cpx #$88
$ff936a f0 23         beq $ff938f
$ff936c 1a            inc
$ff936d c5 05         cmp $05
$ff936f 90 02         bcc $ff9373
$ff9371 a9 00         lda #$00
$ff9373 99 64 00      sta $0064,y
$ff9376 a5 67         lda $67
$ff9378 8f f4 02 e1   sta $e102f4
$ff937c a5 6b         lda $6b
$ff937e 8f f5 02 e1   sta $e102f5
$ff9382 a5 01         lda $01
$ff9384 48            pha
$ff9385 64 01         stz $01
$ff9387 20 97 93      jsr $9397
$ff938a 20 fb 97      jsr $97fb
$ff938d 80 c0         bra $ff934f
$ff938f aa            tax
$ff9390 d0 02         bne $ff9394
$ff9392 a5 05         lda $05
$ff9394 3a            dec
$ff9395 80 dc         bra $ff9373
$ff9397 c0 03         cpy #$03
$ff9399 b0 1f         bcs $ff93ba
$ff939b a6 64         ldx $64
$ff939d a5 66         lda $66
$ff939f 29 03         and #$03
$ff93a1 d0 06         bne $ff93a9
$ff93a3 e0 01         cpx #$01
$ff93a5 d0 02         bne $ff93a9
$ff93a7 a2 0c         ldx #$0c
$ff93a9 bf b7 83 ff   lda $ff83b7,x
$ff93ad c0 01         cpy #$01
$ff93af d0 02         bne $ff93b3
$ff93b1 85 05         sta $05
$ff93b3 3a            dec
$ff93b4 c5 65         cmp $65
$ff93b6 b0 02         bcs $ff93ba
$ff93b8 85 65         sta $65
$ff93ba 60            rts
$ff93bb a2 00         ldx #$00
$ff93bd 48            pha
$ff93be 8a            txa
$ff93bf 18            clc
$ff93c0 2c 1f c0      bit $c01f
$ff93c3 10 02         bpl $ff93c7
$ff93c5 69 14         adc #$14
$ff93c7 aa            tax
$ff93c8 20 e0 a9      jsr $a9e0
$ff93cb 68            pla
$ff93cc 60            rts
$ff93cd a9 16         lda #$16
$ff93cf a2 17         ldx #$17
$ff93d1 20 bd 93      jsr $93bd
$ff93d4 4c 80 c0      jmp $c080
$ff93d7 a2 01         ldx #$01
$ff93d9 a0 01         ldy #$01
$ff93db a9 0e         lda #$0e
$ff93dd 80 f2         bra $ff93d1
$ff93df 18            clc
$ff93e0 b7 a4         lda [$a4],y
$ff93e2 77 ad         adc [$ad],y
$ff93e4 80 ee         bra $ff93d4
$ff93e6 a0 01         ldy #$01
$ff93e8 5a            phy
$ff93e9 20 bb 93      jsr $93bb
$ff93ec a9 5a         lda #$5a
$ff93ee 20 d1 a9      jsr $a9d1
$ff93f1 7a            ply
$ff93f2 5a            phy
$ff93f3 a2 27         ldx #$27
$ff93f5 20 bd 93      jsr $93bd
$ff93f8 a9 5f         lda #$5f
$ff93fa 20 d1 a9      jsr $a9d1
$ff93fd 7a            ply
$ff93fe c8            iny
$ff93ff c0 16         cpy #$16
$ff9401 90 e5         bcc $ff93e8
$ff9403 f0 e3         beq $ff93e8
$ff9405 60            rts
$ff9406 20 e6 93      jsr $93e6
$ff9409 a2 02         ldx #$02
$ff940b a0 16         ldy #$16
$ff940d a9 0f         lda #$0f
$ff940f 20 d1 93      jsr $93d1
$ff9412 a5 00         lda $00
$ff9414 c9 0b         cmp #$0b
$ff9416 f0 0b         beq $ff9423
$ff9418 a9 11         lda #$11
$ff941a 20 80 c0      jsr $c080
$ff941d a5 04         lda $04
$ff941f c9 01         cmp #$01
$ff9421 f0 05         beq $ff9428
$ff9423 a9 10         lda #$10
$ff9425 20 80 c0      jsr $c080
$ff9428 a5 1f         lda $1f
$ff942a 29 30         and #$30
$ff942c 0a            asl
$ff942d 0a            asl
$ff942e 0a            asl
$ff942f 08            php
$ff9430 30 04         bmi $ff9436
$ff9432 a9 12         lda #$12
$ff9434 80 07         bra $ff943d
$ff9436 a9 14         lda #$14
$ff9438 20 80 c0      jsr $c080
$ff943b a9 13         lda #$13
$ff943d 20 80 c0      jsr $c080
$ff9440 28            plp
$ff9441 90 03         bcc $ff9446
$ff9443 20 52 94      jsr $9452
$ff9446 a0 17         ldy #$17
$ff9448 a2 01         ldx #$01
$ff944a 20 bd 93      jsr $93bd
$ff944d a9 4c         lda #$4c
$ff944f 4c c7 94      jmp $94c7
$ff9452 a0 05         ldy #$05
$ff9454 20 cd 93      jsr $93cd
$ff9457 a0 06         ldy #$06
$ff9459 a9 15         lda #$15
$ff945b 20 cf 93      jsr $93cf
$ff945e a0 07         ldy #$07
$ff9460 a9 15         lda #$15
$ff9462 20 cf 93      jsr $93cf
$ff9465 08            php
$ff9466 78            sei
$ff9467 c2 30         rep #$30
$ff9469 f4 00 00      pea $0000
$ff946c f4 6f 00      pea $006f
$ff946f a2 03 0f      ldx #$0f03
$ff9472 22 00 00 e1   jsr $e10000
$ff9476 e2 30         sep #$30
$ff9478 a2 19         ldx #$19
$ff947a a0 06         ldy #$06
$ff947c 20 bd 93      jsr $93bd
$ff947f af f5 02 e1   lda $e102f5
$ff9483 f0 03         beq $ff9488
$ff9485 20 91 ac      jsr $ac91
$ff9488 a2 09         ldx #$09
$ff948a bf 6f 00 00   lda $00006f,x
$ff948e da            phx
$ff948f 20 d1 a9      jsr $a9d1
$ff9492 fa            plx
$ff9493 e8            inx
$ff9494 e0 14         cpx #$14
$ff9496 90 f2         bcc $ff948a
$ff9498 a2 1a         ldx #$1a
$ff949a a0 07         ldy #$07
$ff949c 20 bd 93      jsr $93bd
$ff949f a2 00         ldx #$00
$ff94a1 bf 6f 00 00   lda $00006f,x
$ff94a5 da            phx
$ff94a6 20 d1 a9      jsr $a9d1
$ff94a9 fa            plx
$ff94aa e8            inx
$ff94ab e0 08         cpx #$08
$ff94ad 90 f2         bcc $ff94a1
$ff94af 28            plp
$ff94b0 a0 08         ldy #$08
$ff94b2 a9 17         lda #$17
$ff94b4 4c cf 93      jmp $93cf
$ff94b7 a9 5c         lda #$5c
$ff94b9 80 0f         bra $ff94ca
$ff94bb a9 4c         lda #$4c
$ff94bd 80 12         bra $ff94d1
$ff94bf a9 20         lda #$20
$ff94c1 80 07         bra $ff94ca
$ff94c3 a9 a0         lda #$a0
$ff94c5 80 0a         bra $ff94d1
$ff94c7 48            pha
$ff94c8 80 03         bra $ff94cd
$ff94ca 48            pha
$ff94cb a9 5a         lda #$5a
$ff94cd 20 d1 a9      jsr $a9d1
$ff94d0 68            pla
$ff94d1 48            pha
$ff94d2 a5 24         lda $24
$ff94d4 2c 1f c0      bit $c01f
$ff94d7 10 06         bpl $ff94df
$ff94d9 ad 7b 05      lda $057b
$ff94dc 38            sec
$ff94dd e9 14         sbc #$14
$ff94df aa            tax
$ff94e0 68            pla
$ff94e1 e0 27         cpx #$27
$ff94e3 b0 25         bcs $ff950a
$ff94e5 da            phx
$ff94e6 48            pha
$ff94e7 20 d1 a9      jsr $a9d1
$ff94ea 68            pla
$ff94eb fa            plx
$ff94ec e8            inx
$ff94ed 80 f2         bra $ff94e1
$ff94ef 20 8c 99      jsr $998c
$ff94f2 a8            tay
$ff94f3 8a            txa
$ff94f4 f0 27         beq $ff951d
$ff94f6 c2 20         rep #$20
$ff94f8 e0 01 f0      cpx #$f001
$ff94fb 0f e0 04 f0   ora $f004e0
$ff94ff 0f e0 05 f0   ora $f005e0
$ff9503 0f e0 06 f0   ora $f006e0
$ff9507 0f e2 30 60   ora $6030e2
$ff950b a5 59         lda $59
$ff950d 80 0a         bra $ff9519
$ff950f a5 5d         lda $5d
$ff9511 80 06         bra $ff9519
$ff9513 a5 61         lda $61
$ff9515 80 02         bra $ff9519
$ff9517 a5 55         lda $55
$ff9519 4a            lsr
$ff951a 4a            lsr
$ff951b 80 2a         bra $ff9547
$ff951d c2 20         rep #$20
$ff951f a9 00 80      lda #$8000
$ff9522 85 1b         sta $1b
$ff9524 64 1d         stz $1d
$ff9526 f4 00 00      pea $0000
$ff9529 f4 00 00      pea $0000
$ff952c 18            clc
$ff952d 88            dey
$ff952e c0 ff f0      cpy #$f0ff
$ff9531 10 a3         bpl $ff94d6
$ff9533 01 65         ora ($65,x)
$ff9535 1b            tcs
$ff9536 83 01         sta $01,sp
$ff9538 a3 03         lda $03,sp
$ff953a 65 1d         adc $1d
$ff953c 83 03         sta $03,sp
$ff953e 63 01         adc $01,sp
$ff9540 80 eb         bra $ff952d
$ff9542 fa            plx
$ff9543 68            pla
$ff9544 4a            lsr
$ff9545 4a            lsr
$ff9546 fa            plx
$ff9547 e2 30         sep #$30
$ff9549 eb            xba
$ff954a aa            tax
$ff954b eb            xba
$ff954c 18            clc
$ff954d 85 3e         sta $3e
$ff954f 86 3f         stx $3f
$ff9551 66 34         ror $34
$ff9553 a0 05         ldy #$05
$ff9555 a2 11         ldx #$11
$ff9557 a9 00         lda #$00
$ff9559 18            clc
$ff955a 2a            rol
$ff955b c9 0a         cmp #$0a
$ff955d 90 02         bcc $ff9561
$ff955f e9 0a         sbc #$0a
$ff9561 26 3e         rol $3e
$ff9563 26 3f         rol $3f
$ff9565 ca            dex
$ff9566 d0 f2         bne $ff955a
$ff9568 09 b0         ora #$b0
$ff956a 48            pha
$ff956b 88            dey
$ff956c d0 e7         bne $ff9555
$ff956e a0 04         ldy #$04
$ff9570 68            pla
$ff9571 c9 b0         cmp #$b0
$ff9573 d0 15         bne $ff958a
$ff9575 24 34         bit $34
$ff9577 30 0a         bmi $ff9583
$ff9579 bb            tyx
$ff957a f0 0a         beq $ff9586
$ff957c 5a            phy
$ff957d 48            pha
$ff957e 20 91 ac      jsr $ac91
$ff9581 68            pla
$ff9582 7a            ply
$ff9583 88            dey
$ff9584 10 ea         bpl $ff9570
$ff9586 4c d1 a9      jmp $a9d1
$ff9589 68            pla
$ff958a 5a            phy
$ff958b 20 d1 a9      jsr $a9d1
$ff958e 7a            ply
$ff958f 88            dey
$ff9590 10 f7         bpl $ff9589
$ff9592 60            rts
$ff9593 a5 00         lda $00
$ff9595 c9 06         cmp #$06
$ff9597 d0 14         bne $ff95ad
$ff9599 a9 00         lda #$00
$ff959b 8f 43 01 e1   sta $e10143
$ff959f 20 a8 98      jsr $98a8
$ff95a2 af c0 02 e1   lda $e102c0
$ff95a6 c9 02         cmp #$02
$ff95a8 d0 74         bne $ff961e
$ff95aa 20 20 96      jsr $9620
$ff95ad c9 07         cmp #$07
$ff95af d0 14         bne $ff95c5
$ff95b1 a9 00         lda #$00
$ff95b3 8f 44 01 e1   sta $e10144
$ff95b7 20 a8 98      jsr $98a8
$ff95ba af cc 02 e1   lda $e102cc
$ff95be c9 02         cmp #$02
$ff95c0 d0 5c         bne $ff961e
$ff95c2 20 20 96      jsr $9620
$ff95c5 a5 00         lda $00
$ff95c7 c9 05         cmp #$05
$ff95c9 d0 42         bne $ff960d
$ff95cb af e1 02 e1   lda $e102e1
$ff95cf c9 02         cmp #$02
$ff95d1 d0 0c         bne $ff95df
$ff95d3 4f e2 02 e1   eor $e102e2
$ff95d7 d0 06         bne $ff95df
$ff95d9 a9 1a         lda #$1a
$ff95db a0 10         ldy #$10
$ff95dd 80 27         bra $ff9606
$ff95df af e7 02 e1   lda $e102e7
$ff95e3 f0 08         beq $ff95ed
$ff95e5 af e8 02 e1   lda $e102e8
$ff95e9 c9 0a         cmp #$0a
$ff95eb d0 20         bne $ff960d
$ff95ed a9 02         lda #$02
$ff95ef cf e1 02 e1   cmp $e102e1
$ff95f3 f0 18         beq $ff960d
$ff95f5 cf e2 02 e1   cmp $e102e2
$ff95f9 f0 12         beq $ff960d
$ff95fb a9 18         lda #$18
$ff95fd a0 10         ldy #$10
$ff95ff 20 06 96      jsr $9606
$ff9602 a9 19         lda #$19
$ff9604 a0 11         ldy #$11
$ff9606 a2 01         ldx #$01
$ff9608 20 d1 93      jsr $93d1
$ff960b 38            sec
$ff960c 60            rts
$ff960d a0 10         ldy #$10
$ff960f 20 14 96      jsr $9614
$ff9612 a0 11         ldy #$11
$ff9614 a2 01         ldx #$01
$ff9616 20 bd 93      jsr $93bd
$ff9619 a9 a0         lda #$a0
$ff961b 20 c7 94      jsr $94c7
$ff961e 18            clc
$ff961f 60            rts
$ff9620 a0 0a         ldy #$0a
$ff9622 20 95 98      jsr $9895
$ff9625 20 06 94      jsr $9406
$ff9628 60            rts
$ff9629 a9 53         lda #$53
$ff962b 4c d1 a9      jmp $a9d1
$ff962e a2 01         ldx #$01
$ff9630 a0 00         ldy #$00
$ff9632 20 bd 93      jsr $93bd
$ff9635 a9 df         lda #$df
$ff9637 20 c7 94      jsr $94c7
$ff963a 20 e6 93      jsr $93e6
$ff963d 4c 46 94      jmp $9446
$ff9640 20 bf 94      jsr $94bf
$ff9643 a2 01         ldx #$01
$ff9645 a0 02         ldy #$02
$ff9647 20 bd 93      jsr $93bd
$ff964a a9 4c         lda #$4c
$ff964c 4c c7 94      jmp $94c7
$ff964f 4c 18 97      jmp $9718
$ff9652 4c fb 97      jmp $97fb
$ff9655 4c a3 97      jmp $97a3
$ff9658 a0 05         ldy #$05
$ff965a 84 25         sty $25
$ff965c a6 00         ldx $00
$ff965e f0 ef         beq $ff964f
$ff9660 e0 03         cpx #$03
$ff9662 f0 ee         beq $ff9652
$ff9664 e0 08         cpx #$08
$ff9666 f0 ed         beq $ff9655
$ff9668 20 81 99      jsr $9981
$ff966b a2 00         ldx #$00
$ff966d 20 80 96      jsr $9680
$ff9670 34 83         bit $83,x
$ff9672 30 03         bmi $ff9677
$ff9674 20 b5 96      jsr $96b5
$ff9677 e6 25         inc $25
$ff9679 1a            inc
$ff967a e8            inx
$ff967b e4 04         cpx $04
$ff967d 90 ee         bcc $ff966d
$ff967f 60            rts
$ff9680 48            pha
$ff9681 da            phx
$ff9682 b5 83         lda $83,x
$ff9684 08            php
$ff9685 a3 02         lda $02,sp
$ff9687 c5 02         cmp $02
$ff9689 d0 03         bne $ff968e
$ff968b 20 20 9a      jsr $9a20
$ff968e 18            clc
$ff968f a9 05         lda #$05
$ff9691 63 02         adc $02,sp
$ff9693 a8            tay
$ff9694 a2 04         ldx #$04
$ff9696 a3 03         lda $03,sp
$ff9698 20 d1 93      jsr $93d1
$ff969b 28            plp
$ff969c 08            php
$ff969d 30 12         bmi $ff96b1
$ff969f a9 ba         lda #$ba
$ff96a1 20 d1 a9      jsr $a9d1
$ff96a4 20 11 9a      jsr $9a11
$ff96a7 20 91 ac      jsr $ac91
$ff96aa a3 02         lda $02,sp
$ff96ac 85 01         sta $01
$ff96ae 20 4b 99      jsr $994b
$ff96b1 28            plp
$ff96b2 fa            plx
$ff96b3 68            pla
$ff96b4 60            rts
$ff96b5 48            pha
$ff96b6 da            phx
$ff96b7 a6 01         ldx $01
$ff96b9 b4 93         ldy $93,x
$ff96bb b7 ad         lda [$ad],y
$ff96bd a6 00         ldx $00
$ff96bf e0 04         cpx #$04
$ff96c1 d0 13         bne $ff96d6
$ff96c3 bb            tyx
$ff96c4 f0 0b         beq $ff96d1
$ff96c6 c0 02         cpy #$02
$ff96c8 b0 0c         bcs $ff96d6
$ff96ca aa            tax
$ff96cb bf 08 03 e1   lda $e10308,x
$ff96cf 80 05         bra $ff96d6
$ff96d1 aa            tax
$ff96d2 bf ff 02 e1   lda $e102ff,x
$ff96d6 18            clc
$ff96d7 77 a4         adc [$a4],y
$ff96d9 a6 00         ldx $00
$ff96db e0 05         cpx #$05
$ff96dd d0 0c         bne $ff96eb
$ff96df 48            pha
$ff96e0 a9 01         lda #$01
$ff96e2 eb            xba
$ff96e3 68            pla
$ff96e4 c2 30         rep #$30
$ff96e6 20 85 c0      jsr $c085
$ff96e9 80 03         bra $ff96ee
$ff96eb 20 80 c0      jsr $c080
$ff96ee a6 01         ldx $01
$ff96f0 b4 93         ldy $93,x
$ff96f2 20 c3 94      jsr $94c3
$ff96f5 20 fb 96      jsr $96fb
$ff96f8 fa            plx
$ff96f9 68            pla
$ff96fa 60            rts
$ff96fb b7 ad         lda [$ad],y
$ff96fd d7 aa         cmp [$aa],y
$ff96ff 08            php
$ff9700 a2 02 a4      ldx #$a402
$ff9703 25 20         and $20
$ff9705 bd 93 28      lda $2893,x
$ff9708 d0 09         bne $ff9713
$ff970a 20 44 9a      jsr $9a44
$ff970d f0 04         beq $ff9713
$ff970f a9 44 80      lda #$8044
$ff9712 02 a9         cop $a9
$ff9714 a0 4c d1      ldy #$d14c
$ff9717 a9 20 81      lda #$8120
$ff971a 99 a2 00      sta $00a2,y
$ff971d 20 80 96      jsr $9680
$ff9720 48            pha
$ff9721 da            phx
$ff9722 e0 08 d0      cpx #$d008
$ff9725 14 af         trb $af
$ff9727 dd 02 e1      cmp $e102,x
$ff972a d0 04         bne $ff9730
$ff972c a9 a6 80      lda #$80a6
$ff972f 02 a9         cop $a9
$ff9731 78            sei
$ff9732 20 80 c0      jsr $c080
$ff9735 20 29 96      jsr $9629
$ff9738 80 49         bra $ff9783
$ff973a fa            plx
$ff973b 68            pla
$ff973c 34 83         bit $83,x
$ff973e 30 45         bmi $ff9785
$ff9740 48            pha
$ff9741 da            phx
$ff9742 b5 83         lda $83,x
$ff9744 c9 06 d0      cmp #$d006
$ff9747 30 a2         bmi $ff96eb
$ff9749 03 20         ora $20,sp
$ff974b 8c 99 d7      sty $d799
$ff974e aa            tax
$ff974f d0 18         bne $ff9769
$ff9751 a2 04 20      ldx #$2004
$ff9754 8c 99 d7      sty $d799
$ff9757 aa            tax
$ff9758 d0 0f         bne $ff9769
$ff975a a2 05 20      ldx #$2005
$ff975d 8c 99 d7      sty $d799
$ff9760 aa            tax
$ff9761 d0 06         bne $ff9769
$ff9763 a9 77 e2      lda #$e277
$ff9766 02 80         cop $80
$ff9768 02 a9         cop $a9
$ff976a 76 08         ror $08,x
$ff976c 20 80 c0      jsr $c080
$ff976f 20 91 ac      jsr $ac91
$ff9772 28            plp
$ff9773 20 ff 96      jsr $96ff
$ff9776 80 0b         bra $ff9783
$ff9778 c9 02 90      cmp #$9002
$ff977b 04 c9         tsb $c9
$ff977d 06 b0         asl $b0
$ff977f 03 20         ora $20,sp
$ff9781 b5 96         lda $96,x
$ff9783 fa            plx
$ff9784 68            pla
$ff9785 e6 25         inc $25
$ff9787 1a            inc
$ff9788 e8            inx
$ff9789 e4 04         cpx $04
$ff978b 90 90         bcc $ff971d
$ff978d 4c 0e bc      jmp $bc0e
$ff9790 da            phx
$ff9791 a9 41 20      lda #$2041
$ff9794 d1 a9         cmp ($a9),y
$ff9796 fa            plx
$ff9797 ca            dex
$ff9798 10 f6         bpl $ff9790
$ff979a a5 00         lda $00
$ff979c 30 04         bmi $ff97a2
$ff979e 28            plp
$ff979f 4c ff 96      jmp $96ff
$ff97a2 60            rts
$ff97a3 a0 05 84      ldy #$8405
$ff97a6 25 20         and $20
$ff97a8 81 99         sta ($99,x)
$ff97aa a2 00 da      ldx #$da00
$ff97ad 48            pha
$ff97ae 20 80 96      jsr $9680
$ff97b1 20 ef 94      jsr $94ef
$ff97b4 a3 02         lda $02,sp
$ff97b6 c9 02 f0      cmp #$f002
$ff97b9 1a            inc
$ff97ba c9 03 f0      cmp #$f003
$ff97bd 16 c9         asl $c9,x
$ff97bf 07 f0         ora [$f0]
$ff97c1 12 c9         ora ($c9)
$ff97c3 08            php
$ff97c4 f0 26         beq $ff97ec
$ff97c6 a9 cb 20      lda #$20cb
$ff97c9 d1 a9         cmp ($a9),y
$ff97cb a3 02         lda $02,sp
$ff97cd c9 01 d0      cmp #$d001
$ff97d0 03 20         ora $20,sp
$ff97d2 29 96 a3      and #$a396
$ff97d5 02 aa         cop $aa
$ff97d7 a3 01         lda $01,sp
$ff97d9 e0 00 d0      cpx #$d000
$ff97dc 04 9b         tsb $9b
$ff97de 20 fb 96      jsr $96fb
$ff97e1 e6 25         inc $25
$ff97e3 68            pla
$ff97e4 1a            inc
$ff97e5 fa            plx
$ff97e6 e8            inx
$ff97e7 e4 04         cpx $04
$ff97e9 90 c1         bcc $ff97ac
$ff97eb 60            rts
$ff97ec af f7 02 e1   lda $e102f7
$ff97f0 d0 04         bne $ff97f6
$ff97f2 a9 76 80      lda #$8076
$ff97f5 02 a9         cop $a9
$ff97f7 77 4c         adc [$4c],y
$ff97f9 eb            xba
$ff97fa 96 af         stx $af,y
$ff97fc f5 02         sbc $02,x
$ff97fe e1 f0         sbc ($f0,x)
$ff9800 07 a0         ora [$a0]
$ff9802 a0 bb a5      ldy #$a5bb
$ff9805 68            pla
$ff9806 80 17         bra $ff981f
$ff9808 a2 d0 a5      ldx #$a5d0
$ff980b 68            pla
$ff980c c9 0c 90      cmp #$900c
$ff980f 06 f0         asl $f0
$ff9811 0b            phd
$ff9812 e9 0c 80      sbc #$800c
$ff9815 07 aa         ora [$aa]
$ff9817 d0 02         bne $ff981b
$ff9819 a9 0c a2      lda #$a20c
$ff981c c1 a0         cmp ($a0,x)
$ff981e cd 86 6d      cmp $6d86
$ff9821 84 6e         sty $6e
$ff9823 85 6c         sta $6c
$ff9825 a0 05 84      ldy #$8405
$ff9828 25 a6         and $a6
$ff982a 00 20         brk $20
$ff982c 81 99         sta ($99,x)
$ff982e a2 00 48      ldx #$4800
$ff9831 da            phx
$ff9832 20 80 96      jsr $9680
$ff9835 20 8c 99      jsr $998c
$ff9838 e0 03 f0      cpx #$f003
$ff983b 31 e0         and ($e0),y
$ff983d 04 f0         tsb $f0
$ff983f 37 e0         and [$e0],y
$ff9841 08            php
$ff9842 f0 1f         beq $ff9863
$ff9844 a6 01         ldx $01
$ff9846 e0 05 d0      cpx #$d005
$ff9849 02 a0         cop $a0
$ff984b 08            php
$ff984c b9 64 00      lda $0064,y
$ff984f c0 00 f0      cpy #$f000
$ff9852 04 c0         tsb $c0
$ff9854 01 d0         ora ($d0,x)
$ff9856 01 1a         ora ($1a,x)
$ff9858 a2 00 38      ldx #$3800
$ff985b 20 4d 95      jsr $954d
$ff985e 20 91 ac      jsr $ac91
$ff9861 80 14         bra $ff9877
$ff9863 a0 01 20      ldy #$2001
$ff9866 df 93 a0 01   cmp $01a093,x
$ff986a 4c f2 96      jmp $96f2
$ff986d a0 00 20      ldy #$2000
$ff9870 df 93 a0 00   cmp $00a093,x
$ff9874 20 fb 96      jsr $96fb
$ff9877 fa            plx
$ff9878 da            phx
$ff9879 e0 05 d0      cpx #$d005
$ff987c 0d a5 6d      ora $6da5
$ff987f 20 d1 a9      jsr $a9d1
$ff9882 a5 6e         lda $6e
$ff9884 20 d1 a9      jsr $a9d1
$ff9887 20 91 ac      jsr $ac91
$ff988a fa            plx
$ff988b 68            pla
$ff988c 1a            inc
$ff988d e8            inx
$ff988e e6 25         inc $25
$ff9890 e4 04         cpx $04
$ff9892 90 9c         bcc $ff9830
$ff9894 60            rts
$ff9895 a5 22         lda $22
$ff9897 48            pha
$ff9898 64 24         stz $24
$ff989a 84 25         sty $25
$ff989c 84 22         sty $22
$ff989e 20 e4 a9      jsr $a9e4
$ff98a1 20 ee a9      jsr $a9ee
$ff98a4 68            pla
$ff98a5 85 22         sta $22
$ff98a7 60            rts
$ff98a8 a6 00         ldx $00
$ff98aa bf 2d 83 ff   lda $ff832d,x
$ff98ae 85 1f         sta $1f
$ff98b0 29 0f 85      and #$850f
$ff98b3 04 20         tsb $20
$ff98b5 2b            pld
$ff98b6 9a            txs
$ff98b7 f0 05         beq $ff98be
$ff98b9 20 37 9a      jsr $9a37
$ff98bc d0 0b         bne $ff98c9
$ff98be 38            sec
$ff98bf a5 04         lda $04
$ff98c1 e9 07 85      sbc #$8507
$ff98c4 04 38         tsb $38
$ff98c6 08            php
$ff98c7 80 14         bra $ff98dd
$ff98c9 af 8e 01 e1   lda $e1018e
$ff98cd c9 06 08      cmp #$0806
$ff98d0 b0 0b         bcs $ff98dd
$ff98d2 e0 09 d0      cpx #$d009
$ff98d5 07 38         ora [$38]
$ff98d7 a5 04         lda $04
$ff98d9 e9 05 85      sbc #$8505
$ff98dc 04 8a         tsb $8a
$ff98de 0a            asl
$ff98df aa            tax
$ff98e0 bf 16 83 ff   lda $ff8316,x
$ff98e4 eb            xba
$ff98e5 bf 15 83 ff   lda $ff8315,x
$ff98e9 28            plp
$ff98ea b0 0e         bcs $ff98fa
$ff98ec e0 08 d0      cpx #$d008
$ff98ef 04 eb         tsb $eb
$ff98f1 09 18 eb      ora #$eb18
$ff98f4 e0 12 d0      cpx #$d012
$ff98f7 02 09         cop $09
$ff98f9 70 e0         bvs $ff98db
$ff98fb 0c d0 07      tsb $07d0
$ff98fe 48            pha
$ff98ff af c0 02 e1   lda $e102c0
$ff9903 80 09         bra $ff990e
$ff9905 e0 0e d0      cpx #$d00e
$ff9908 13 48         ora ($48,sp),y
$ff990a af cc 02 e1   lda $e102cc
$ff990e c9 02 f0      cmp #$f002
$ff9911 03 68         ora $68,sp
$ff9913 80 07         bra $ff991c
$ff9915 68            pla
$ff9916 09 e0 eb      ora #$ebe0
$ff9919 09 ff eb      ora #$ebff
$ff991c a2 00 20      ldx #$2000
$ff991f 3f 99 e0 08   and $08e099,x
$ff9923 90 f9         bcc $ff991e
$ff9925 eb            xba
$ff9926 20 3f 99      jsr $993f
$ff9929 e0 10 90      cpx #$9010
$ff992c f9 a2 00      sbc $00a2,y
$ff992f 9b            txy
$ff9930 b5 83         lda $83,x
$ff9932 c9 80 94      cmp #$9480
$ff9935 93 e8         sta ($e8,sp),y
$ff9937 b0 01         bcs $ff993a
$ff9939 c8            iny
$ff993a e0 10 90      cpx #$9010
$ff993d f2 60         sbc ($60)
$ff993f 4a            lsr
$ff9940 48            pha
$ff9941 a0 80 b0      ldy #$b080
$ff9944 01 9b         ora ($9b,x)
$ff9946 94 83         sty $83,x
$ff9948 68            pla
$ff9949 e8            inx
$ff994a 60            rts
$ff994b 8b            phb
$ff994c 4b            phk
$ff994d ab            plb
$ff994e a6 00         ldx $00
$ff9950 bd ed 82      lda $82ed,x
$ff9953 85 a4         sta $a4
$ff9955 bd 01 83      lda $8301,x
$ff9958 85 aa         sta $aa
$ff995a bd 0b 83      lda $830b,x
$ff995d 85 ad         sta $ad
$ff995f bd f7 82      lda $82f7,x
$ff9962 85 a7         sta $a7
$ff9964 a9 e1 85      lda #$85e1
$ff9967 af 4b 68 85   lda $85684b
$ff996b a6 85         ldx $85
$ff996d ac 85 a9      ldy $a985
$ff9970 a9 83 85      lda #$8583
$ff9973 a5 85         lda $85
$ff9975 ab            plb
$ff9976 85 a8         sta $a8
$ff9978 a9 02 85      lda #$8502
$ff997b ae 8a 0a      ldx $0a8a
$ff997e aa            tax
$ff997f ab            plb
$ff9980 60            rts
$ff9981 a6 00         ldx $00
$ff9983 bf 39 83 ff   lda $ff8339,x
$ff9987 85 a3         sta $a3
$ff9989 60            rts
$ff998a a6 01         ldx $01
$ff998c 86 01         stx $01
$ff998e 20 4b 99      jsr $994b
$ff9991 a6 01         ldx $01
$ff9993 b4 93         ldy $93,x
$ff9995 b7 a7         lda [$a7],y
$ff9997 10 56         bpl $ff99ef
$ff9999 48            pha
$ff999a a5 00         lda $00
$ff999c c9 03 f0      cmp #$f003
$ff999f 37 c9         and [$c9],y
$ff99a1 04 f0         tsb $f0
$ff99a3 3d 5a a0      and $a05a,x
$ff99a6 ff c2 20 f4   sbc $f420c2,x
$ff99aa 00 00         brk $00
$ff99ac f4 00 80      pea $8000
$ff99af a5 58         lda $58
$ff99b1 85 1b         sta $1b
$ff99b3 a5 5a         lda $5a
$ff99b5 85 1d         sta $1d
$ff99b7 c9 80 00      cmp #$0080
$ff99ba b0 11         bcs $ff99cd
$ff99bc c8            iny
$ff99bd 38            sec
$ff99be c8            iny
$ff99bf a5 1b         lda $1b
$ff99c1 e3 01         sbc $01,sp
$ff99c3 85 1b         sta $1b
$ff99c5 a5 1d         lda $1d
$ff99c7 e3 03         sbc $03,sp
$ff99c9 85 1d         sta $1d
$ff99cb b0 f0         bcs $ff99bd
$ff99cd 84 05         sty $05
$ff99cf 68            pla
$ff99d0 68            pla
$ff99d1 e2 30         sep #$30
$ff99d3 7a            ply
$ff99d4 68            pla
$ff99d5 80 1a         bra $ff99f1
$ff99d7 68            pla
$ff99d8 8b            phb
$ff99d9 4b            phk
$ff99da ab            plb
$ff99db b9 af 83      lda $83af,y
$ff99de ab            plb
$ff99df 80 0e         bra $ff99ef
$ff99e1 68            pla
$ff99e2 0a            asl
$ff99e3 30 06         bmi $ff99eb
$ff99e5 af fe 02 e1   lda $e102fe
$ff99e9 80 04         bra $ff99ef
$ff99eb af 07 03 e1   lda $e10307
$ff99ef 85 05         sta $05
$ff99f1 b7 ad         lda [$ad],y
$ff99f3 60            rts
$ff99f4 48            pha
$ff99f5 97 ad         sta [$ad],y
$ff99f7 a6 01         ldx $01
$ff99f9 e0 03         cpx #$03
$ff99fb f0 07         beq $ff9a04
$ff99fd e0 04         cpx #$04
$ff99ff f0 03         beq $ff9a04
$ff9a01 68            pla
$ff9a02 18            clc
$ff9a03 60            rts
$ff9a04 af da 02 e1   lda $e102da
$ff9a08 cf db 02 e1   cmp $e102db
$ff9a0c d0 f3         bne $ff9a01
$ff9a0e 68            pla
$ff9a0f 38            sec
$ff9a10 60            rts
$ff9a11 48            pha
$ff9a12 da            phx
$ff9a13 5a            phy
$ff9a14 20 84 fe      jsr $fe84
$ff9a17 a9 1b         lda #$1b
$ff9a19 20 d1 a9      jsr $a9d1
$ff9a1c 7a            ply
$ff9a1d fa            plx
$ff9a1e 68            pla
$ff9a1f 60            rts
$ff9a20 48            pha
$ff9a21 da            phx
$ff9a22 5a            phy
$ff9a23 a9 7f         lda #$7f
$ff9a25 85 32         sta $32
$ff9a27 a9 18         lda #$18
$ff9a29 80 ee         bra $ff9a19
$ff9a2b a5 00         lda $00
$ff9a2d c9 06         cmp #$06
$ff9a2f d0 12         bne $ff9a43
$ff9a31 af c0 02 e1   lda $e102c0
$ff9a35 80 0a         bra $ff9a41
$ff9a37 a5 00         lda $00
$ff9a39 c9 07         cmp #$07
$ff9a3b d0 06         bne $ff9a43
$ff9a3d af cc 02 e1   lda $e102cc
$ff9a41 c9 02         cmp #$02
$ff9a43 60            rts
$ff9a44 a5 01         lda $01
$ff9a46 d0 08         bne $ff9a50
$ff9a48 a5 00         lda $00
$ff9a4a c9 06         cmp #$06
$ff9a4c f0 02         beq $ff9a50
$ff9a4e c9 07         cmp #$07
$ff9a50 60            rts
$ff9a51 22 6c 00 e1   jsr $e1006c
$ff9a55 b0 01         bcs $ff9a58
$ff9a57 60            rts
$ff9a58 08            php
$ff9a59 78            sei
$ff9a5a a9 b0         lda #$b0
$ff9a5c 20 a8 fc      jsr $fca8
$ff9a5f ad 36 c0      lda $c036
$ff9a62 48            pha
$ff9a63 29 7f         and #$7f
$ff9a65 8d 36 c0      sta $c036
$ff9a68 da            phx
$ff9a69 8b            phb
$ff9a6a 20 82 f8      jsr $f882
$ff9a6d ad df 02      lda $02df
$ff9a70 49 0f         eor #$0f
$ff9a72 8d 6d 01      sta $016d
$ff9a75 9c 6e 01      stz $016e
$ff9a78 c2 20         rep #$20
$ff9a7a a9 88 88      lda #$8888
$ff9a7d 8d 69 01      sta $0169
$ff9a80 a9 00 20      lda #$2000
$ff9a83 8d 6b 01      sta $016b
$ff9a86 e2 30         sep #$30
$ff9a88 ad ca 00      lda $00ca
$ff9a8b 8d 6f 01      sta $016f
$ff9a8e 48            pha
$ff9a8f 29 0f         and #$0f
$ff9a91 d0 40         bne $ff9ad3
$ff9a93 ad 34 c0      lda $c034
$ff9a96 29 0f         and #$0f
$ff9a98 48            pha
$ff9a99 58            cli
$ff9a9a 20 28 9b      jsr $9b28
$ff9a9d a9 0f         lda #$0f
$ff9a9f 0c 34 c0      tsb $c034
$ff9aa2 20 23 9b      jsr $9b23
$ff9aa5 20 28 9b      jsr $9b28
$ff9aa8 a9 0f         lda #$0f
$ff9aaa 1c 34 c0      trb $c034
$ff9aad 20 23 9b      jsr $9b23
$ff9ab0 20 28 9b      jsr $9b28
$ff9ab3 a9 0f         lda #$0f
$ff9ab5 0c 34 c0      tsb $c034
$ff9ab8 20 23 9b      jsr $9b23
$ff9abb 20 28 9b      jsr $9b28
$ff9abe a9 0f         lda #$0f
$ff9ac0 1c 34 c0      trb $c034
$ff9ac3 20 23 9b      jsr $9b23
$ff9ac6 fa            plx
$ff9ac7 78            sei
$ff9ac8 a9 0f         lda #$0f
$ff9aca 1c 34 c0      trb $c034
$ff9acd 8a            txa
$ff9ace 0c 34 c0      tsb $c034
$ff9ad1 80 3d         bra $ff9b10
$ff9ad3 18            clc
$ff9ad4 c2 30         rep #$30
$ff9ad6 ac 6d 01      ldy $016d
$ff9ad9 a2 50 02      ldx #$0250
$ff9adc 88            dey
$ff9add d0 0b         bne $ff9aea
$ff9adf ac 6d 01      ldy $016d
$ff9ae2 2e 69 01      rol $0169
$ff9ae5 90 03         bcc $ff9aea
$ff9ae7 ad 30 c0      lda $c030
$ff9aea ca            dex
$ff9aeb d0 18         bne $ff9b05
$ff9aed a2 50 02      ldx #$0250
$ff9af0 e2 20         sep #$20
$ff9af2 ad 6f 01      lda $016f
$ff9af5 f0 19         beq $ff9b10
$ff9af7 3a            dec
$ff9af8 8d 6f 01      sta $016f
$ff9afb 8d ca 00      sta $00ca
$ff9afe 8d 3c c0      sta $c03c
$ff9b01 c2 20         rep #$20
$ff9b03 80 06         bra $ff9b0b
$ff9b05 a9 05 00      lda #$0005
$ff9b08 3a            dec
$ff9b09 d0 fd         bne $ff9b08
$ff9b0b ce 6b 01      dec $016b
$ff9b0e d0 cc         bne $ff9adc
$ff9b10 e2 30         sep #$30
$ff9b12 68            pla
$ff9b13 8d ca 00      sta $00ca
$ff9b16 8d 3c c0      sta $c03c
$ff9b19 ab            plb
$ff9b1a fa            plx
$ff9b1b a0 00         ldy #$00
$ff9b1d 68            pla
$ff9b1e 8d 36 c0      sta $c036
$ff9b21 28            plp
$ff9b22 60            rts
$ff9b23 a9 ff         lda #$ff
$ff9b25 4c a8 fc      jmp $fca8
$ff9b28 c2 10         rep #$10
$ff9b2a ad 19 c0      lda $c019
$ff9b2d 30 fb         bmi $ff9b2a
$ff9b2f ad 19 c0      lda $c019
$ff9b32 10 fb         bpl $ff9b2f
$ff9b34 e2 10         sep #$10
$ff9b36 60            rts
$ff9b37 20 a2 9b      jsr $9ba2
$ff9b3a 60            rts
$ff9b3b a9 ff         lda #$ff
$ff9b3d 8f 35 01 e1   sta $e10135
$ff9b41 8b            phb
$ff9b42 a9 e1         lda #$e1
$ff9b44 48            pha
$ff9b45 ab            plb
$ff9b46 a9 40         lda #$40
$ff9b48 8d bb 01      sta $01bb
$ff9b4b a9 6b         lda #$6b
$ff9b4d 8d b2 01      sta $01b2
$ff9b50 9c 37 01      stz $0137
$ff9b53 9c d7 00      stz $00d7
$ff9b56 9c cb 00      stz $00cb
$ff9b59 9c 01 01      stz $0101
$ff9b5c c2 20         rep #$20
$ff9b5e 9c 03 01      stz $0103
$ff9b61 9c 43 01      stz $0143
$ff9b64 9c c3 00      stz $00c3
$ff9b67 9c c5 00      stz $00c5
$ff9b6a a9 01 00      lda #$0001
$ff9b6d 8d 74 01      sta $0174
$ff9b70 a9 5a a5      lda #$a55a
$ff9b73 8d 76 01      sta $0176
$ff9b76 a9 79 ff      lda #$ff79
$ff9b79 8d 7a 01      sta $017a
$ff9b7c a9 5c 5c      lda #$5c5c
$ff9b7f 8d 78 01      sta $0178
$ff9b82 a9 79 ff      lda #$ff79
$ff9b85 8d ab 01      sta $01ab
$ff9b88 a9 5c 5b      lda #$5b5c
$ff9b8b 8d a9 01      sta $01a9
$ff9b8e a9 79 ff      lda #$ff79
$ff9b91 8d af 01      sta $01af
$ff9b94 a9 5c 5b      lda #$5b5c
$ff9b97 8d ad 01      sta $01ad
$ff9b9a e2 30         sep #$30
$ff9b9c 9c 7c 01      stz $017c
$ff9b9f e2 30         sep #$30
$ff9ba1 ab            plb
$ff9ba2 8b            phb
$ff9ba3 8b            phb
$ff9ba4 20 82 f8      jsr $f882
$ff9ba7 ad 68 c0      lda $c068
$ff9baa 29 fe         and #$fe
$ff9bac 8d 2d 01      sta $012d
$ff9baf a3 0b         lda $0b,sp
$ff9bb1 29 80         and #$80
$ff9bb3 0d 35 c0      ora $c035
$ff9bb6 8d 2e 01      sta $012e
$ff9bb9 68            pla
$ff9bba 8d 2b 01      sta $012b
$ff9bbd 8d 2c 01      sta $012c
$ff9bc0 8d 33 01      sta $0133
$ff9bc3 8d 3e 01      sta $013e
$ff9bc6 8d a7 01      sta $01a7
$ff9bc9 c2 30         rep #$30
$ff9bcb 7b            tdc
$ff9bcc 8d 28 01      sta $0128
$ff9bcf 3b            tsc
$ff9bd0 18            clc
$ff9bd1 69 0b 00      adc #$000b
$ff9bd4 8d 26 01      sta $0126
$ff9bd7 a9 01 01      lda #$0101
$ff9bda 8d 2f 01      sta $012f
$ff9bdd 8d 31 01      sta $0131
$ff9be0 e2 30         sep #$30
$ff9be2 a0 05         ldy #$05
$ff9be4 a9 00         lda #$00
$ff9be6 99 20 01      sta $0120,y
$ff9be9 88            dey
$ff9bea 10 fa         bpl $ff9be6
$ff9bec 8d 2a 01      sta $012a
$ff9bef 8c 34 01      sty $0134
$ff9bf2 ab            plb
$ff9bf3 60            rts
$ff9bf4 a9 00         lda #$00
$ff9bf6 a2 02         ldx #$02
$ff9bf8 9f 30 01 e1   sta $e10130,x
$ff9bfc ca            dex
$ff9bfd 10 f9         bpl $ff9bf8
$ff9bff 60            rts
$ff9c00 ad 36 c0      lda $c036
$ff9c03 48            pha
$ff9c04 29 7f         and #$7f
$ff9c06 08            php
$ff9c07 78            sei
$ff9c08 8d 36 c0      sta $c036
$ff9c0b ad 70 c0      lda $c070
$ff9c0e a0 00         ldy #$00
$ff9c10 ea            nop
$ff9c11 ea            nop
$ff9c12 bd 64 c0      lda $c064,x
$ff9c15 10 04         bpl $ff9c1b
$ff9c17 c8            iny
$ff9c18 d0 f8         bne $ff9c12
$ff9c1a 88            dey
$ff9c1b 28            plp
$ff9c1c 68            pla
$ff9c1d 8d 36 c0      sta $c036
$ff9c20 60            rts
$ff9c21 e2 30         sep #$30
$ff9c23 eb            xba
$ff9c24 a9 80         lda #$80
$ff9c26 0c 36 c0      tsb $c036
$ff9c29 eb            xba
$ff9c2a 8d 07 c0      sta $c007
$ff9c2d 60            rts
$ff9c2e 8b            phb
$ff9c2f 20 82 f8      jsr $f882
$ff9c32 a2 27         ldx #$27
$ff9c34 bf 64 7c ff   lda $ff7c64,x
$ff9c38 9d 80 00      sta $0080,x
$ff9c3b bf 54 7c ff   lda $ff7c54,x
$ff9c3f 9d 10 00      sta $0010,x
$ff9c42 e0 20         cpx #$20
$ff9c44 b0 07         bcs $ff9c4d
$ff9c46 bf 34 7c ff   lda $ff7c34,x
$ff9c4a 9d 00 02      sta $0200,x
$ff9c4d ca            dex
$ff9c4e 10 e4         bpl $ff9c34
$ff9c50 a2 17         ldx #$17
$ff9c52 9e a8 00      stz $00a8,x
$ff9c55 ca            dex
$ff9c56 10 fa         bpl $ff9c52
$ff9c58 a9 ff         lda #$ff
$ff9c5a 8d c2 00      sta $00c2
$ff9c5d a9 4c         lda #$4c
$ff9c5f 8f d0 03 00   sta $0003d0
$ff9c63 8f f5 03 00   sta $0003f5
$ff9c67 8f f8 03 00   sta $0003f8
$ff9c6b c2 30         rep #$30
$ff9c6d a9 65 ff      lda #$ff65
$ff9c70 8f fe 03 00   sta $0003fe
$ff9c74 8f d1 03 00   sta $0003d1
$ff9c78 8f f9 03 00   sta $0003f9
$ff9c7c a9 03 e0      lda #$e003
$ff9c7f 8f f6 03 00   sta $0003f6
$ff9c83 a9 30 84      lda #$8430
$ff9c86 8d c0 00      sta $00c0
$ff9c89 a2 5c 00      ldx #$005c
$ff9c8c a0 ba ff      ldy #$ffba
$ff9c8f a9 5c 18      lda #$185c
$ff9c92 9d 20 00      sta $0020,x
$ff9c95 98            tya
$ff9c96 9d 22 00      sta $0022,x
$ff9c99 ca            dex
$ff9c9a ca            dex
$ff9c9b ca            dex
$ff9c9c ca            dex
$ff9c9d 10 ed         bpl $ff9c8c
$ff9c9f a9 c0 ba      lda #$bac0
$ff9ca2 8d 51 00      sta $0051
$ff9ca5 a9 5c f1      lda #$f15c
$ff9ca8 8d 7c 00      sta $007c
$ff9cab a9 a6 ff      lda #$ffa6
$ff9cae 8d 7e 00      sta $007e
$ff9cb1 e2 30         sep #$30
$ff9cb3 a9 ff         lda #$ff
$ff9cb5 8d 53 00      sta $0053
$ff9cb8 9c d6 00      stz $00d6
$ff9cbb ab            plb
$ff9cbc 4c 9b bb      jmp $bb9b
$ff9cbf c2 10         rep #$10
$ff9cc1 a2 ff 07      ldx #$07ff
$ff9cc4 a9 b2 9f      lda #$9fb2
$ff9cc7 00 08         brk $08
$ff9cc9 e0 ca 10      cpx #$10ca
$ff9ccc f9 e2 30      sbc $30e2,y
$ff9ccf ad 68 c0      lda $c068
$ff9cd2 48            pha
$ff9cd3 8d 06 c0      sta $c006
$ff9cd6 a9 c1 85      lda #$85c1
$ff9cd9 01 64         ora ($64,x)
$ff9cdb 00 a2         brk $a2
$ff9cdd 03 bf         ora $bf,sp
$ff9cdf 67 fb         adc [$fb]
$ff9ce1 ff a8 bf 6b   sbc $6bbfa8,x
$ff9ce5 fb            xce
$ff9ce6 ff d1 00 d0   sbc $d000d1,x
$ff9cea 0b            phd
$ff9ceb ca            dex
$ff9cec 10 f0         bpl $ff9cde
$ff9cee a5 01         lda $01
$ff9cf0 29 07 aa      and #$aa07
$ff9cf3 9e f8 07      stz $07f8,x
$ff9cf6 e6 01         inc $01
$ff9cf8 a5 01         lda $01
$ff9cfa c9 c8 90      cmp #$90c8
$ff9cfd de 68 8d      dec $8d68,x
$ff9d00 68            pla
$ff9d01 c0 22 00      cpy #$0022
$ff9d04 00 fe         brk $fe
$ff9d06 90 08         bcc $ff9d10
$ff9d08 a9 00 8f      lda #$8f00
$ff9d0b ff 15 e1 80   sbc $80e115,x
$ff9d0f f2 af         sbc ($af)
$ff9d11 f8            sed
$ff9d12 02 e1         cop $e1
$ff9d14 f0 0a         beq $ff9d20
$ff9d16 a9 00 8f      lda #$8f00
$ff9d19 f8            sed
$ff9d1a 02 e1         cop $e1
$ff9d1c 22 4e b9 ff   jsr $ffb94e
$ff9d20 a9 00 8f      lda #$8f00
$ff9d23 fe 0f e1      inc $e10f,x
$ff9d26 8f ff 0f e1   sta $e10fff
$ff9d2a 22 16 b9 ff   jsr $ffb916
$ff9d2e 20 00 56      jsr $5600
$ff9d31 22 0c 00 fe   jsr $fe000c
$ff9d35 60            rts
$ff9d36 08            php
$ff9d37 e2 30         sep #$30
$ff9d39 af 7d 01 e1   lda $e1017d
$ff9d3d aa            tax
$ff9d3e af 19 03 e1   lda $e10319
$ff9d42 f0 0c         beq $ff9d50
$ff9d44 8a            txa
$ff9d45 29 bf         and #$bf
$ff9d47 8f 7d 01 e1   sta $e1017d
$ff9d4b 20 79 79      jsr $7979
$ff9d4e 28            plp
$ff9d4f 6b            rtl
$ff9d50 8a            txa
$ff9d51 30 f2         bmi $ff9d45
$ff9d53 28            plp
$ff9d54 6b            rtl
$ff9d55 29 0f         and #$0f
$ff9d57 09 b0         ora #$b0
$ff9d59 c9 ba         cmp #$ba
$ff9d5b 90 02         bcc $ff9d5f
$ff9d5d 69 06         adc #$06
$ff9d5f 60            rts
$ff9d60 22 74 00 e1   jsr $e10074
$ff9d64 a9 06         lda #$06
$ff9d66 b0 01         bcs $ff9d69
$ff9d68 60            rts
$ff9d69 20 80 c0      jsr $c080
$ff9d6c 38            sec
$ff9d6d 80 0d         bra $ff9d7c
$ff9d6f 22 78 00 e1   jsr $e10078
$ff9d73 a9 05         lda #$05
$ff9d75 b0 01         bcs $ff9d78
$ff9d77 60            rts
$ff9d78 20 80 c0      jsr $c080
$ff9d7b 18            clc
$ff9d7c 8b            phb
$ff9d7d ad 68 c0      lda $c068
$ff9d80 48            pha
$ff9d81 20 da a7      jsr $a7da
$ff9d84 ae 35 c0      ldx $c035
$ff9d87 da            phx
$ff9d88 3b            tsc
$ff9d89 8d 3c 01      sta $013c
$ff9d8c 08            php
$ff9d8d ad 26 01      lda $0126
$ff9d90 c9 00         cmp #$00
$ff9d92 02 b0         cop $b0
$ff9d94 16 c9         asl $c9,x
$ff9d96 80 01         bra $ff9d99
$ff9d98 90 08         bcc $ff9da2
$ff9d9a a9 7f         lda #$7f
$ff9d9c 01 8d         ora ($8d,x)
$ff9d9e 26 01         rol $01
$ff9da0 80 09         bra $ff9dab
$ff9da2 c9 10         cmp #$10
$ff9da4 01 b0         ora ($b0,x)
$ff9da6 04 da         tsb $da
$ff9da8 4c 2c a2      jmp $a22c
$ff9dab 28            plp
$ff9dac 58            cli
$ff9dad e2 30         sep #$30
$ff9daf 9c ba 01      stz $01ba
$ff9db2 a9 7f         lda #$7f
$ff9db4 48            pha
$ff9db5 b8            clv
$ff9db6 08            php
$ff9db7 f4 00 00      pea $0000
$ff9dba ab            plb
$ff9dbb ab            plb
$ff9dbc 20 2c ad      jsr $ad2c
$ff9dbf af 3e 01 e1   lda $e1013e
$ff9dc3 85 3c         sta $3c
$ff9dc5 20 6b cf      jsr $cf6b
$ff9dc8 10 03         bpl $ff9dcd
$ff9dca 4c 95 9e      jmp $9e95
$ff9dcd 28            plp
$ff9dce 08            php
$ff9dcf 90 1d         bcc $ff9dee
$ff9dd1 50 1b         bvc $ff9dee
$ff9dd3 a0 00         ldy #$00
$ff9dd5 20 8b b4      jsr $b48b
$ff9dd8 c9 60         cmp #$60
$ff9dda f0 08         beq $ff9de4
$ff9ddc c9 40         cmp #$40
$ff9dde f0 04         beq $ff9de4
$ff9de0 c9 6b         cmp #$6b
$ff9de2 d0 0a         bne $ff9dee
$ff9de4 28            plp
$ff9de5 68            pla
$ff9de6 a9 7f         lda #$7f
$ff9de8 48            pha
$ff9de9 b8            clv
$ff9dea 18            clc
$ff9deb 08            php
$ff9dec 80 d7         bra $ff9dc5
$ff9dee af ba 01 e1   lda $e101ba
$ff9df2 10 10         bpl $ff9e04
$ff9df4 a0 00         ldy #$00
$ff9df6 20 8b b4      jsr $b48b
$ff9df9 c9 20         cmp #$20
$ff9dfb f0 04         beq $ff9e01
$ff9dfd c9 22         cmp #$22
$ff9dff d0 03         bne $ff9e04
$ff9e01 4c e7 9e      jmp $9ee7
$ff9e04 28            plp
$ff9e05 08            php
$ff9e06 90 bd         bcc $ff9dc5
$ff9e08 a3 02         lda $02,sp
$ff9e0a 20 a8 fc      jsr $fca8
$ff9e0d 4c 25 a0      jmp $a025
$ff9e10 af ba 01 e1   lda $e101ba
$ff9e14 49 40         eor #$40
$ff9e16 8f ba 01 e1   sta $e101ba
$ff9e1a 80 a9         bra $ff9dc5
$ff9e1c 28            plp
$ff9e1d 68            pla
$ff9e1e a2 00         ldx #$00
$ff9e20 4c 15 a7      jmp $a715
$ff9e23 28            plp
$ff9e24 e2 40         sep #$40
$ff9e26 08            php
$ff9e27 80 69         bra $ff9e92
$ff9e29 af ba 01 e1   lda $e101ba
$ff9e2d 09 80         ora #$80
$ff9e2f 8f ba 01 e1   sta $e101ba
$ff9e33 80 5d         bra $ff9e92
$ff9e35 a9 40         lda #$40
$ff9e37 80 1b         bra $ff9e54
$ff9e39 a9 80         lda #$80
$ff9e3b 1c 29 c0      trb $c029
$ff9e3e ad 50 c0      lda $c050
$ff9e41 ad 5f c0      lda $c05f
$ff9e44 80 4c         bra $ff9e92
$ff9e46 a9 20         lda #$20
$ff9e48 80 0a         bra $ff9e54
$ff9e4a a9 10         lda #$10
$ff9e4c 80 06         bra $ff9e54
$ff9e4e a9 00         lda #$00
$ff9e50 80 02         bra $ff9e54
$ff9e52 a9 80         lda #$80
$ff9e54 8f bb 01 e1   sta $e101bb
$ff9e58 a9 80         lda #$80
$ff9e5a 1c 29 c0      trb $c029
$ff9e5d 8d 5e c0      sta $c05e
$ff9e60 20 11 a8      jsr $a811
$ff9e63 80 2d         bra $ff9e92
$ff9e65 29 df         and #$df
$ff9e67 c9 cf         cmp #$cf
$ff9e69 f0 a5         beq $ff9e10
$ff9e6b c9 ca         cmp #$ca
$ff9e6d f0 ad         beq $ff9e1c
$ff9e6f c9 d2         cmp #$d2
$ff9e71 f0 b0         beq $ff9e23
$ff9e73 c9 d8         cmp #$d8
$ff9e75 f0 b2         beq $ff9e29
$ff9e77 c9 d4         cmp #$d4
$ff9e79 f0 ba         beq $ff9e35
$ff9e7b c9 c4         cmp #$c4
$ff9e7d f0 ba         beq $ff9e39
$ff9e7f c9 cd         cmp #$cd
$ff9e81 f0 c3         beq $ff9e46
$ff9e83 c9 c8         cmp #$c8
$ff9e85 f0 c3         beq $ff9e4a
$ff9e87 c9 cc         cmp #$cc
$ff9e89 f0 c3         beq $ff9e4e
$ff9e8b c9 d3         cmp #$d3
$ff9e8d f0 c3         beq $ff9e52
$ff9e8f 20 51 9a      jsr $9a51
$ff9e92 4c c5 9d      jmp $9dc5
$ff9e95 c9 c1         cmp #$c1
$ff9e97 b0 cc         bcs $ff9e65
$ff9e99 c9 9b         cmp #$9b
$ff9e9b f0 17         beq $ff9eb4
$ff9e9d c9 a0         cmp #$a0
$ff9e9f f0 17         beq $ff9eb8
$ff9ea1 c9 8d         cmp #$8d
$ff9ea3 f0 19         beq $ff9ebe
$ff9ea5 c9 88         cmp #$88
$ff9ea7 f0 21         beq $ff9eca
$ff9ea9 c9 95         cmp #$95
$ff9eab f0 23         beq $ff9ed0
$ff9ead c9 8a         cmp #$8a
$ff9eaf f0 28         beq $ff9ed9
$ff9eb1 4c 8f 9e      jmp $9e8f
$ff9eb4 58            cli
$ff9eb5 4c b6 a7      jmp $a7b6
$ff9eb8 28            plp
$ff9eb9 90 0b         bcc $ff9ec6
$ff9ebb 18            clc
$ff9ebc 80 08         bra $ff9ec6
$ff9ebe 28            plp
$ff9ebf b0 05         bcs $ff9ec6
$ff9ec1 38            sec
$ff9ec2 68            pla
$ff9ec3 a9 7f         lda #$7f
$ff9ec5 48            pha
$ff9ec6 08            php
$ff9ec7 4c 25 a0      jmp $a025
$ff9eca 28            plp
$ff9ecb 68            pla
$ff9ecc a9 ff         lda #$ff
$ff9ece 80 04         bra $ff9ed4
$ff9ed0 28            plp
$ff9ed1 68            pla
$ff9ed2 a9 3f         lda #$3f
$ff9ed4 48            pha
$ff9ed5 08            php
$ff9ed6 4c c5 9d      jmp $9dc5
$ff9ed9 c2 20         rep #$20
$ff9edb 4b            phk
$ff9edc f4 bb 9d      pea $9dbb
$ff9edf 3b            tsc
$ff9ee0 8f 8c 01 e1   sta $e1018c
$ff9ee4 4c 0a a2      jmp $a20a
$ff9ee7 af ba 01 e1   lda $e101ba
$ff9eeb 29 7f 8f      and #$8f7f
$ff9eee ba            tsx
$ff9eef 01 e1         ora ($e1,x)
$ff9ef1 c2 30         rep #$30
$ff9ef3 4b            phk
$ff9ef4 f4 bb 9d      pea $9dbb
$ff9ef7 3b            tsc
$ff9ef8 8f 8c 01 e1   sta $e1018c
$ff9efc 18            clc
$ff9efd e2 30         sep #$30
$ff9eff 08            php
$ff9f00 a4 2f         ldy $2f
$ff9f02 c0 03         cpy #$03
$ff9f04 d0 03         bne $ff9f09
$ff9f06 4c b2 9f      jmp $9fb2
$ff9f09 20 8b b4      jsr $b48b
$ff9f0c c9 bf         cmp #$bf
$ff9f0e d0 11         bne $ff9f21
$ff9f10 88            dey
$ff9f11 20 8b b4      jsr $b48b
$ff9f14 0f 3e 01 e1   ora $e1013e
$ff9f18 d0 07         bne $ff9f21
$ff9f1a a9 05         lda #$05
$ff9f1c 85 2f         sta $2f
$ff9f1e 68            pla
$ff9f1f 80 03         bra $ff9f24
$ff9f21 28            plp
$ff9f22 b0 36         bcs $ff9f5a
$ff9f24 a5 3a         lda $3a
$ff9f26 85 3d         sta $3d
$ff9f28 a5 3b         lda $3b
$ff9f2a 85 3e         sta $3e
$ff9f2c af 2c 01 e1   lda $e1012c
$ff9f30 85 3f         sta $3f
$ff9f32 20 48 7b      jsr $7b48
$ff9f35 c2 30         rep #$30
$ff9f37 af 26 01 e1   lda $e10126
$ff9f3b 1b            tcs
$ff9f3c e2 20         sep #$20
$ff9f3e a5 3f         lda $3f
$ff9f40 c9 f8 90      cmp #$90f8
$ff9f43 34 64         bit $64,x
$ff9f45 3d 64 3e      and $3e64,x
$ff9f48 a0 ff ff      ldy #$ffff
$ff9f4b b7 3d         lda [$3d],y
$ff9f4d c9 6b f0      cmp #$f06b
$ff9f50 0c 88 d0      tsb $d088
$ff9f53 f7 b7         sbc [$b7],y
$ff9f55 3a            dec
$ff9f56 c9 22 f0      cmp #$f022
$ff9f59 7a            ply
$ff9f5a 4c 81 a1      jmp $a181
$ff9f5d 4b            phk
$ff9f5e f4 1c a0      pea $a01c
$ff9f61 88            dey
$ff9f62 5a            phy
$ff9f63 e2 30         sep #$30
$ff9f65 a0 00         ldy #$00
$ff9f67 b7 3a         lda [$3a],y
$ff9f69 c9 22         cmp #$22
$ff9f6b d0 03         bne $ff9f70
$ff9f6d 4c ee 9f      jmp $9fee
$ff9f70 a5 3f         lda $3f
$ff9f72 48            pha
$ff9f73 a0 02         ldy #$02
$ff9f75 4c fd 9f      jmp $9ffd
$ff9f78 e2 30         sep #$30
$ff9f7a a4 2f         ldy $2f
$ff9f7c 20 7b ac      jsr $ac7b
$ff9f7f a0 00         ldy #$00
$ff9f81 20 8b b4      jsr $b48b
$ff9f84 48            pha
$ff9f85 a9 6b         lda #$6b
$ff9f87 97 3a         sta [$3a],y
$ff9f89 4b            phk
$ff9f8a f4 0f a0      pea $a00f
$ff9f8d af 32 01 e1   lda $e10132
$ff9f91 d0 03         bne $ff9f96
$ff9f93 a5 3f         lda $3f
$ff9f95 48            pha
$ff9f96 a5 3e         lda $3e
$ff9f98 48            pha
$ff9f99 a5 3d         lda $3d
$ff9f9b 48            pha
$ff9f9c af 2a 01 e1   lda $e1012a
$ff9fa0 48            pha
$ff9fa1 af 32 01 e1   lda $e10132
$ff9fa5 d0 04         bne $ff9fab
$ff9fa7 20 68 7a      jsr $7a68
$ff9faa 40            rti
$ff9fab 20 68 7a      jsr $7a68
$ff9fae 5c 72 c0 00   jmp $00c072
$ff9fb2 20 8b b4      jsr $b48b
$ff9fb5 c9 e1         cmp #$e1
$ff9fb7 d0 18         bne $ff9fd1
$ff9fb9 88            dey
$ff9fba 20 8b b4      jsr $b48b
$ff9fbd c9 00         cmp #$00
$ff9fbf d0 10         bne $ff9fd1
$ff9fc1 88            dey
$ff9fc2 20 8b b4      jsr $b48b
$ff9fc5 c9 a8         cmp #$a8
$ff9fc7 d0 08         bne $ff9fd1
$ff9fc9 a9 09         lda #$09
$ff9fcb 85 2f         sta $2f
$ff9fcd 68            pla
$ff9fce 4c 24 9f      jmp $9f24
$ff9fd1 28            plp
$ff9fd2 90 0c         bcc $ff9fe0
$ff9fd4 e2 30         sep #$30
$ff9fd6 af 2c 01 e1   lda $e1012c
$ff9fda 20 ae a2      jsr $a2ae
$ff9fdd 4c 81 a1      jmp $a181
$ff9fe0 20 48 7b      jsr $7b48
$ff9fe3 c2 30         rep #$30
$ff9fe5 af 26 01 e1   lda $e10126
$ff9fe9 1b            tcs
$ff9fea 4b            phk
$ff9feb f4 1c a0      pea $a01c
$ff9fee e2 30         sep #$30
$ff9ff0 a0 03         ldy #$03
$ff9ff2 af 32 01 e1   lda $e10132
$ff9ff6 d0 04         bne $ff9ffc
$ff9ff8 20 8b b4      jsr $b48b
$ff9ffb 48            pha
$ff9ffc 88            dey
$ff9ffd 20 8b b4      jsr $b48b
$ffa000 48            pha
$ffa001 88            dey
$ffa002 20 8b b4      jsr $b48b
$ffa005 eb            xba
$ffa006 68            pla
$ffa007 eb            xba
$ffa008 c2 30         rep #$30
$ffa00a 48            pha
$ffa00b e2 30         sep #$30
$ffa00d 4c 9c 9f      jmp $9f9c
$ffa010 20 ce 7a      jsr $7ace
$ffa013 e2 30         sep #$30
$ffa015 68            pla
$ffa016 a0 00         ldy #$00
$ffa018 97 3a         sta [$3a],y
$ffa01a 4c 0f a2      jmp $a20f
$ffa01d 20 ce 7a      jsr $7ace
$ffa020 e2 30         sep #$30
$ffa022 4c 03 a2      jmp $a203
$ffa025 c2 30         rep #$30
$ffa027 4b            phk
$ffa028 f4 bb 9d      pea $9dbb
$ffa02b 3b            tsc
$ffa02c 8f 8c 01 e1   sta $e1018c
$ffa030 e2 30         sep #$30
$ffa032 78            sei
$ffa033 a0 00         ldy #$00
$ffa035 20 8b b4      jsr $b48b
$ffa038 48            pha
$ffa039 a2 0b         ldx #$0b
$ffa03b bf 43 a2 ff   lda $ffa243,x
$ffa03f 9f c3 01 e1   sta $e101c3,x
$ffa043 ca            dex
$ffa044 10 f5         bpl $ffa03b
$ffa046 af 3e 01 e1   lda $e1013e
$ffa04a 85 3c         sta $3c
$ffa04c 8f 2c 01 e1   sta $e1012c
$ffa050 fa            plx
$ffa051 d0 10         bne $ffa063
$ffa053 20 51 9a      jsr $9a51
$ffa056 af ba 01 e1   lda $e101ba
$ffa05a 29 bf         and #$bf
$ffa05c 8f ba 01 e1   sta $e101ba
$ffa060 4c 0f a2      jmp $a20f
$ffa063 e0 60         cpx #$60
$ffa065 f0 0f         beq $ffa076
$ffa067 e0 6b         cpx #$6b
$ffa069 f0 0b         beq $ffa076
$ffa06b e0 40         cpx #$40
$ffa06d d0 37         bne $ffa0a6
$ffa06f 20 8d a2      jsr $a28d
$ffa072 8f 2a 01 e1   sta $e1012a
$ffa076 20 8d a2      jsr $a28d
$ffa079 85 3a         sta $3a
$ffa07b 20 8d a2      jsr $a28d
$ffa07e 85 3b         sta $3b
$ffa080 e0 40         cpx #$40
$ffa082 d0 08         bne $ffa08c
$ffa084 af 32 01 e1   lda $e10132
$ffa088 f0 0c         beq $ffa096
$ffa08a 80 17         bra $ffa0a3
$ffa08c c2 30         rep #$30
$ffa08e e6 3a         inc $3a
$ffa090 e2 30         sep #$30
$ffa092 e0 6b         cpx #$6b
$ffa094 d0 0d         bne $ffa0a3
$ffa096 20 8d a2      jsr $a28d
$ffa099 8f 2c 01 e1   sta $e1012c
$ffa09d 8f 3e 01 e1   sta $e1013e
$ffa0a1 85 3c         sta $3c
$ffa0a3 4c 0f a2      jmp $a20f
$ffa0a6 da            phx
$ffa0a7 a4 2f         ldy $2f
$ffa0a9 20 8b b4      jsr $b48b
$ffa0ac bb            tyx
$ffa0ad 9f c3 01 e1   sta $e101c3,x
$ffa0b1 88            dey
$ffa0b2 10 f5         bpl $ffa0a9
$ffa0b4 fa            plx
$ffa0b5 e0 58         cpx #$58
$ffa0b7 d0 03         bne $ffa0bc
$ffa0b9 4c 0a a2      jmp $a20a
$ffa0bc e0 4c         cpx #$4c
$ffa0be b0 03         bcs $ffa0c3
$ffa0c0 4c 6f a1      jmp $a16f
$ffa0c3 f0 30         beq $ffa0f5
$ffa0c5 e0 dc         cpx #$dc
$ffa0c7 f0 2c         beq $ffa0f5
$ffa0c9 90 03         bcc $ffa0ce
$ffa0cb 4c 6f a1      jmp $a16f
$ffa0ce e0 5c         cpx #$5c
$ffa0d0 f0 23         beq $ffa0f5
$ffa0d2 e0 6c         cpx #$6c
$ffa0d4 f0 1f         beq $ffa0f5
$ffa0d6 e0 7c         cpx #$7c
$ffa0d8 d0 f1         bne $ffa0cb
$ffa0da af 32 01 e1   lda $e10132
$ffa0de d0 08         bne $ffa0e8
$ffa0e0 af 2a 01 e1   lda $e1012a
$ffa0e4 29 10         and #$10
$ffa0e6 f0 0d         beq $ffa0f5
$ffa0e8 af 23 01 e1   lda $e10123
$ffa0ec 48            pha
$ffa0ed a9 00         lda #$00
$ffa0ef 8f 23 01 e1   sta $e10123
$ffa0f3 80 05         bra $ffa0fa
$ffa0f5 af 23 01 e1   lda $e10123
$ffa0f9 48            pha
$ffa0fa af c4 01 e1   lda $e101c4
$ffa0fe 85 3a         sta $3a
$ffa100 af c5 01 e1   lda $e101c5
$ffa104 85 3b         sta $3b
$ffa106 e0 5c         cpx #$5c
$ffa108 d0 0c         bne $ffa116
$ffa10a af c6 01 e1   lda $e101c6
$ffa10e 8f 3e 01 e1   sta $e1013e
$ffa112 8f 2c 01 e1   sta $e1012c
$ffa116 e0 4c         cpx #$4c
$ffa118 f0 4d         beq $ffa167
$ffa11a e0 5c         cpx #$5c
$ffa11c f0 49         beq $ffa167
$ffa11e e0 7c         cpx #$7c
$ffa120 d0 0d         bne $ffa12f
$ffa122 c2 30         rep #$30
$ffa124 af c4 01 e1   lda $e101c4
$ffa128 18            clc
$ffa129 6f 22 01 e1   adc $e10122
$ffa12d 85 3a         sta $3a
$ffa12f e2 30         sep #$30
$ffa131 e0 dc         cpx #$dc
$ffa133 d0 0c         bne $ffa141
$ffa135 a0 02         ldy #$02
$ffa137 b1 3a         lda ($3a),y
$ffa139 8f 3e 01 e1   sta $e1013e
$ffa13d 8f 2c 01 e1   sta $e1012c
$ffa141 e0 fc         cpx #$fc
$ffa143 f0 04         beq $ffa149
$ffa145 e0 7c         cpx #$7c
$ffa147 d0 13         bne $ffa15c
$ffa149 8b            phb
$ffa14a af 2c 01 e1   lda $e1012c
$ffa14e 48            pha
$ffa14f ab            plb
$ffa150 c2 30         rep #$30
$ffa152 a0 00 00      ldy #$0000
$ffa155 b1 3a         lda ($3a),y
$ffa157 85 3a         sta $3a
$ffa159 ab            plb
$ffa15a 80 09         bra $ffa165
$ffa15c c2 30         rep #$30
$ffa15e a0 00 00      ldy #$0000
$ffa161 b1 3a         lda ($3a),y
$ffa163 85 3a         sta $3a
$ffa165 e2 30         sep #$30
$ffa167 68            pla
$ffa168 8f 23 01 e1   sta $e10123
$ffa16c 4c 0f a2      jmp $a20f
$ffa16f 38            sec
$ffa170 08            php
$ffa171 e0 20         cpx #$20
$ffa173 f0 04         beq $ffa179
$ffa175 e0 22         cpx #$22
$ffa177 d0 03         bne $ffa17c
$ffa179 4c 00 9f      jmp $9f00
$ffa17c 28            plp
$ffa17d e0 fc         cpx #$fc
$ffa17f d0 25         bne $ffa1a6
$ffa181 da            phx
$ffa182 c6 2f         dec $2f
$ffa184 20 53 f9      jsr $f953
$ffa187 85 3a         sta $3a
$ffa189 84 3b         sty $3b
$ffa18b 48            pha
$ffa18c 98            tya
$ffa18d 20 ae a2      jsr $a2ae
$ffa190 68            pla
$ffa191 20 ae a2      jsr $a2ae
$ffa194 a2 4c         ldx #$4c
$ffa196 68            pla
$ffa197 c9 20         cmp #$20
$ffa199 f0 08         beq $ffa1a3
$ffa19b a2 5c         ldx #$5c
$ffa19d c9 22         cmp #$22
$ffa19f f0 02         beq $ffa1a3
$ffa1a1 a2 7c         ldx #$7c
$ffa1a3 4c f5 a0      jmp $a0f5
$ffa1a6 e0 82         cpx #$82
$ffa1a8 d0 0a         bne $ffa1b4
$ffa1aa a9 00         lda #$00
$ffa1ac 8f c5 01 e1   sta $e101c5
$ffa1b0 a9 05         lda #$05
$ffa1b2 80 0d         bra $ffa1c1
$ffa1b4 e0 80         cpx #$80
$ffa1b6 f0 07         beq $ffa1bf
$ffa1b8 8a            txa
$ffa1b9 29 1f         and #$1f
$ffa1bb c9 10         cmp #$10
$ffa1bd d0 06         bne $ffa1c5
$ffa1bf a9 06         lda #$06
$ffa1c1 8f c4 01 e1   sta $e101c4
$ffa1c5 e0 62         cpx #$62
$ffa1c7 f0 0d         beq $ffa1d6
$ffa1c9 e0 4b         cpx #$4b
$ffa1cb d0 20         bne $ffa1ed
$ffa1cd af 2c 01 e1   lda $e1012c
$ffa1d1 20 ae a2      jsr $a2ae
$ffa1d4 80 34         bra $ffa20a
$ffa1d6 20 48 7b      jsr $7b48
$ffa1d9 c2 30         rep #$30
$ffa1db af 26 01 e1   lda $e10126
$ffa1df 1b            tcs
$ffa1e0 a5 3a         lda $3a
$ffa1e2 1a            inc
$ffa1e3 1a            inc
$ffa1e4 38            sec
$ffa1e5 6f c4 01 e1   adc $e101c4
$ffa1e9 48            pha
$ffa1ea 80 17         bra $ffa203
$ffa1ec 48            pha
$ffa1ed 20 48 7b      jsr $7b48
$ffa1f0 c2 30         rep #$30
$ffa1f2 af 26 01 e1   lda $e10126
$ffa1f6 1b            tcs
$ffa1f7 e2 30         sep #$30
$ffa1f9 20 68 7a      jsr $7a68
$ffa1fc 5c c3 01 e1   jmp $e101c3
$ffa200 20 ce 7a      jsr $7ace
$ffa203 c2 30         rep #$30
$ffa205 3b            tsc
$ffa206 8f 26 01 e1   sta $e10126
$ffa20a e2 30         sep #$30
$ffa20c 20 7b ac      jsr $ac7b
$ffa20f af cf 01 e1   lda $e101cf
$ffa213 8f 00 01 01   sta $010100
$ffa217 c2 30         rep #$30
$ffa219 af 8c 01 e1   lda $e1018c
$ffa21d 1b            tcs
$ffa21e af 26 01 e1   lda $e10126
$ffa222 c9 00 02      cmp #$0200
$ffa225 b0 0d         bcs $ffa234
$ffa227 c9 0f 01      cmp #$010f
$ffa22a b0 08         bcs $ffa234
$ffa22c e2 30         sep #$30
$ffa22e 20 51 9a      jsr $9a51
$ffa231 4c b4 9e      jmp $9eb4
$ffa234 e2 30         sep #$30
$ffa236 af ba 01 e1   lda $e101ba
$ffa23a 29 40         and #$40
$ffa23c d0 03         bne $ffa241
$ffa23e 20 ed a4      jsr $a4ed
$ffa241 58            cli
$ffa242 6b            rtl
$ffa243 ea            nop
$ffa244 ea            nop
$ffa245 ea            nop
$ffa246 ea            nop
$ffa247 5c 00 a2 ff   jmp $ffa200
$ffa24b 5c 53 a2 ff   jmp $ffa253
$ffa24f 5c 1d a0 ff   jmp $ffa01d
$ffa253 f4 00 00      pea $0000
$ffa256 ab            plb
$ffa257 ab            plb
$ffa258 f4 00 00      pea $0000
$ffa25b 2b            pld
$ffa25c 18            clc
$ffa25d fb            xce
$ffa25e e2 30         sep #$30
$ffa260 a4 2f         ldy $2f
$ffa262 20 8b b4      jsr $b48b
$ffa265 88            dey
$ffa266 f0 15         beq $ffa27d
$ffa268 48            pha
$ffa269 20 8b b4      jsr $b48b
$ffa26c 38            sec
$ffa26d 65 3a         adc $3a
$ffa26f aa            tax
$ffa270 68            pla
$ffa271 65 3b         adc $3b
$ffa273 a8            tay
$ffa274 8a            txa
$ffa275 18            clc
$ffa276 69 02         adc #$02
$ffa278 aa            tax
$ffa279 90 0b         bcc $ffa286
$ffa27b 80 08         bra $ffa285
$ffa27d 38            sec
$ffa27e 20 56 f9      jsr $f956
$ffa281 aa            tax
$ffa282 e8            inx
$ffa283 d0 01         bne $ffa286
$ffa285 c8            iny
$ffa286 86 3a         stx $3a
$ffa288 84 3b         sty $3b
$ffa28a 4c 0f a2      jmp $a20f
$ffa28d 8b            phb
$ffa28e 20 82 f8      jsr $f882
$ffa291 ee 26 01      inc $0126
$ffa294 d0 08         bne $ffa29e
$ffa296 ad 32 01      lda $0132
$ffa299 f0 03         beq $ffa29e
$ffa29b ee 26 01      inc $0126
$ffa29e ad 26 01      lda $0126
$ffa2a1 85 3d         sta $3d
$ffa2a3 ad 27 01      lda $0127
$ffa2a6 85 3e         sta $3e
$ffa2a8 64 3f         stz $3f
$ffa2aa a7 3d         lda [$3d]
$ffa2ac ab            plb
$ffa2ad 60            rts
$ffa2ae 8b            phb
$ffa2af 48            pha
$ffa2b0 20 82 f8      jsr $f882
$ffa2b3 ad 26 01      lda $0126
$ffa2b6 85 3d         sta $3d
$ffa2b8 ad 27 01      lda $0127
$ffa2bb 85 3e         sta $3e
$ffa2bd 68            pla
$ffa2be 64 3f         stz $3f
$ffa2c0 87 3d         sta [$3d]
$ffa2c2 ce 26 01      dec $0126
$ffa2c5 ad 26 01      lda $0126
$ffa2c8 c9 ff         cmp #$ff
$ffa2ca d0 08         bne $ffa2d4
$ffa2cc ad 32 01      lda $0132
$ffa2cf d0 03         bne $ffa2d4
$ffa2d1 ce 27 01      dec $0127
$ffa2d4 ab            plb
$ffa2d5 60            rts
$ffa2d6 a4 34         ldy $34
$ffa2d8 b9 00 02      lda $0200,y
$ffa2db c9 a0         cmp #$a0
$ffa2dd d0 03         bne $ffa2e2
$ffa2df c8            iny
$ffa2e0 d0 f6         bne $ffa2d8
$ffa2e2 c8            iny
$ffa2e3 84 34         sty $34
$ffa2e5 a2 12         ldx #$12
$ffa2e7 df ea c0 ff   cmp $ffc0ea,x
$ffa2eb f0 69         beq $ffa356
$ffa2ed ca            dex
$ffa2ee 10 f7         bpl $ffa2e7
$ffa2f0 88            dey
$ffa2f1 b9 00 02      lda $0200,y
$ffa2f4 c9 a0         cmp #$a0
$ffa2f6 d0 03         bne $ffa2fb
$ffa2f8 c8            iny
$ffa2f9 80 f6         bra $ffa2f1
$ffa2fb e2 40         sep #$40
$ffa2fd c9 ab         cmp #$ab
$ffa2ff f0 04         beq $ffa305
$ffa301 c9 ad         cmp #$ad
$ffa303 d0 01         bne $ffa306
$ffa305 b8            clv
$ffa306 20 e4 a6      jsr $a6e4
$ffa309 e2 30         sep #$30
$ffa30b a9 02         lda #$02
$ffa30d 48            pha
$ffa30e 5a            phy
$ffa30f a2 00         ldx #$00
$ffa311 e8            inx
$ffa312 c8            iny
$ffa313 b9 00 02      lda $0200,y
$ffa316 c9 8d         cmp #$8d
$ffa318 f0 06         beq $ffa320
$ffa31a 49 b0         eor #$b0
$ffa31c c9 0a         cmp #$0a
$ffa31e 90 f1         bcc $ffa311
$ffa320 c2 30         rep #$30
$ffa322 da            phx
$ffa323 50 05         bvc $ffa32a
$ffa325 f4 00 00      pea $0000
$ffa328 80 03         bra $ffa32d
$ffa32a f4 01 00      pea $0001
$ffa32d a2 0b 29      ldx #$290b
$ffa330 22 00 00 e1   jsr $e10000
$ffa334 e2 30         sep #$30
$ffa336 a9 09         lda #$09
$ffa338 20 80 c0      jsr $c080
$ffa33b a3 03         lda $03,sp
$ffa33d aa            tax
$ffa33e a3 04         lda $04,sp
$ffa340 20 9f a9      jsr $a99f
$ffa343 fa            plx
$ffa344 68            pla
$ffa345 20 9f a9      jsr $a99f
$ffa348 68            pla
$ffa349 68            pla
$ffa34a 20 52 ac      jsr $ac52
$ffa34d a2 0a         ldx #$0a
$ffa34f 68            pla
$ffa350 ca            dex
$ffa351 d0 fc         bne $ffa34f
$ffa353 4c db a9      jmp $a9db
$ffa356 e0 05         cpx #$05
$ffa358 90 35         bcc $ffa38f
$ffa35a e0 10         cpx #$10
$ffa35c f0 0a         beq $ffa368
$ffa35e e0 11         cpx #$11
$ffa360 b0 0d         bcs $ffa36f
$ffa362 8a            txa
$ffa363 69 05         adc #$05
$ffa365 aa            tax
$ffa366 80 2c         bra $ffa394
$ffa368 a5 3e         lda $3e
$ffa36a 8f bb 01 e1   sta $e101bb
$ffa36e 60            rts
$ffa36f d0 03         bne $ffa374
$ffa371 4c aa a8      jmp $a8aa
$ffa374 a9 08         lda #$08
$ffa376 20 80 c0      jsr $c080
$ffa379 b8            clv
$ffa37a 20 bb a3      jsr $a3bb
$ffa37d 20 91 ac      jsr $ac91
$ffa380 a9 fb         lda #$fb
$ffa382 20 d1 a9      jsr $a9d1
$ffa385 e2 40         sep #$40
$ffa387 20 bb a3      jsr $a3bb
$ffa38a 20 08 ae      jsr $ae08
$ffa38d 80 bb         bra $ffa34a
$ffa38f 8a            txa
$ffa390 0a            asl
$ffa391 aa            tax
$ffa392 c2 20         rep #$20
$ffa394 a5 3e         lda $3e
$ffa396 e0 0f 90      cpx #$900f
$ffa399 06 e0         asl $e0
$ffa39b 14 f0         trb $f0
$ffa39d 02 29         cop $29
$ffa39f 01 9f         ora ($9f,x)
$ffa3a1 20 01 e1      jsr $e101
$ffa3a4 e2 30         sep #$30
$ffa3a6 a3 07         lda $07,sp
$ffa3a8 29 7f         and #$7f
$ffa3aa 48            pha
$ffa3ab af 2e 01 e1   lda $e1012e
$ffa3af 29 80         and #$80
$ffa3b1 8f 38 01 e1   sta $e10138
$ffa3b5 03 01         ora $01,sp
$ffa3b7 fa            plx
$ffa3b8 83 07         sta $07,sp
$ffa3ba 60            rts
$ffa3bb 08            php
$ffa3bc c2 30         rep #$30
$ffa3be af 40 01 e1   lda $e10140
$ffa3c2 48            pha
$ffa3c3 a5 3e         lda $3e
$ffa3c5 48            pha
$ffa3c6 f4 00 00      pea $0000
$ffa3c9 f4 00 02      pea $0200
$ffa3cc f4 10 00      pea $0010
$ffa3cf a9 01 00      lda #$0001
$ffa3d2 70 01         bvs $ffa3d5
$ffa3d4 3a            dec
$ffa3d5 48            pha
$ffa3d6 a2 0b 27      ldx #$270b
$ffa3d9 22 00 00 e1   jsr $e10000
$ffa3dd 28            plp
$ffa3de a2 ff e8      ldx #$e8ff
$ffa3e1 e0 10 b0      cpx #$b010
$ffa3e4 23 bd         and $bd,sp
$ffa3e6 00 02         brk $02
$ffa3e8 c9 20 f0      cmp #$f020
$ffa3eb f4 50 0e      pea $0e50
$ffa3ee c9 2d f0      cmp #$f02d
$ffa3f1 0a            asl
$ffa3f2 a9 ab da      lda #$daab
$ffa3f5 20 d1 a9      jsr $a9d1
$ffa3f8 fa            plx
$ffa3f9 bd 00 02      lda $0200,x
$ffa3fc 09 80 da      ora #$da80
$ffa3ff 20 d1 a9      jsr $a9d1
$ffa402 fa            plx
$ffa403 e8            inx
$ffa404 e0 10 90      cpx #$9010
$ffa407 f1 60         sbc ($60),y
$ffa409 ad fb 04      lda $04fb
$ffa40c 10 2f         bpl $ffa43d
$ffa40e c9 ff f0      cmp #$f0ff
$ffa411 2b            pld
$ffa412 2c 1f c0      bit $c01f
$ffa415 10 23         bpl $ffa43a
$ffa417 c2 30         rep #$30
$ffa419 a2 05 09      ldx #$0905
$ffa41c 22 00 00 e1   jsr $e10000
$ffa420 e2 30         sep #$30
$ffa422 a9 8d         lda #$8d
$ffa424 20 e9 a9      jsr $a9e9
$ffa427 20 f9 81      jsr $81f9
$ffa42a c2 30         rep #$30
$ffa42c a2 05 0a      ldx #$0a05
$ffa42f 22 00 00 e1   jsr $e10000
$ffa433 e2 30         sep #$30
$ffa435 20 fc 81      jsr $81fc
$ffa438 80 03         bra $ffa43d
$ffa43a 20 eb cc      jsr $cceb
$ffa43d c2 30         rep #$30
$ffa43f af 16 01 e1   lda $e10116
$ffa443 48            pha
$ffa444 3a            dec
$ffa445 3a            dec
$ffa446 85 3a         sta $3a
$ffa448 e2 30         sep #$30
$ffa44a 8b            phb
$ffa44b 20 82 f8      jsr $f882
$ffa44e a2 0b         ldx #$0b
$ffa450 bd 08 01      lda $0108,x
$ffa453 9d 20 01      sta $0120,x
$ffa456 ca            dex
$ffa457 10 f7         bpl $ffa450
$ffa459 8e 8f 01      stx $018f
$ffa45c ad 18 01      lda $0118
$ffa45f 8d 2d 01      sta $012d
$ffa462 29 04         and #$04
$ffa464 4a            lsr
$ffa465 4a            lsr
$ffa466 8d 2f 01      sta $012f
$ffa469 ad 1a 01      lda $011a
$ffa46c 29 80         and #$80
$ffa46e 48            pha
$ffa46f ad 19 01      lda $0119
$ffa472 03 01         ora $01,sp
$ffa474 8d 2e 01      sta $012e
$ffa477 68            pla
$ffa478 ad 15 01      lda $0115
$ffa47b 8d 2c 01      sta $012c
$ffa47e 8d 3e 01      sta $013e
$ffa481 ad 2a 01      lda $012a
$ffa484 29 30         and #$30
$ffa486 4a            lsr
$ffa487 20 7b f8      jsr $f87b
$ffa48a 8d 30 01      sta $0130
$ffa48d a9 00         lda #$00
$ffa48f 2a            rol
$ffa490 8d 31 01      sta $0131
$ffa493 ad 14 01      lda $0114
$ffa496 29 01         and #$01
$ffa498 8d 32 01      sta $0132
$ffa49b 6a            ror
$ffa49c c2 30         rep #$30
$ffa49e ad 26 01      lda $0126
$ffa4a1 1a            inc
$ffa4a2 1a            inc
$ffa4a3 1a            inc
$ffa4a4 b0 01         bcs $ffa4a7
$ffa4a6 1a            inc
$ffa4a7 8d 26 01      sta $0126
$ffa4aa e2 30         sep #$30
$ffa4ac 9c a8 01      stz $01a8
$ffa4af 9c bb 01      stz $01bb
$ffa4b2 a9 00         lda #$00
$ffa4b4 ae 1c c0      ldx $c01c
$ffa4b7 10 02         bpl $ffa4bb
$ffa4b9 09 08         ora #$08
$ffa4bb ae 1d c0      ldx $c01d
$ffa4be 10 02         bpl $ffa4c2
$ffa4c0 09 10         ora #$10
$ffa4c2 ae 1b c0      ldx $c01b
$ffa4c5 10 02         bpl $ffa4c9
$ffa4c7 09 20         ora #$20
$ffa4c9 ae 1a c0      ldx $c01a
$ffa4cc 10 02         bpl $ffa4d0
$ffa4ce 09 40         ora #$40
$ffa4d0 ae 29 c0      ldx $c029
$ffa4d3 10 02         bpl $ffa4d7
$ffa4d5 09 80         ora #$80
$ffa4d7 8d bb 01      sta $01bb
$ffa4da 20 00 a8      jsr $a800
$ffa4dd ab            plb
$ffa4de 20 2c ad      jsr $ad2c
$ffa4e1 a9 00         lda #$00
$ffa4e3 8f 8f 01 e1   sta $e1018f
$ffa4e7 68            pla
$ffa4e8 85 3a         sta $3a
$ffa4ea 68            pla
$ffa4eb 85 3b         sta $3b
$ffa4ed 20 cf a9      jsr $a9cf
$ffa4f0 a2 00         ldx #$00
$ffa4f2 da            phx
$ffa4f3 20 91 ac      jsr $ac91
$ffa4f6 bf ea c0 ff   lda $ffc0ea,x
$ffa4fa 20 d1 a9      jsr $a9d1
$ffa4fd a9 bd         lda #$bd
$ffa4ff 20 d1 a9      jsr $a9d1
$ffa502 fa            plx
$ffa503 da            phx
$ffa504 e0 0a         cpx #$0a
$ffa506 b0 22         bcs $ffa52a
$ffa508 e0 05         cpx #$05
$ffa50a b0 10         bcs $ffa51c
$ffa50c 8a            txa
$ffa50d 0a            asl
$ffa50e aa            tax
$ffa50f bf 21 01 e1   lda $e10121,x
$ffa513 20 a3 a9      jsr $a9a3
$ffa516 bf 20 01 e1   lda $e10120,x
$ffa51a 80 04         bra $ffa520
$ffa51c bf 25 01 e1   lda $e10125,x
$ffa520 20 a3 a9      jsr $a9a3
$ffa523 fa            plx
$ffa524 e8            inx
$ffa525 e0 0e         cpx #$0e
$ffa527 90 c9         bcc $ffa4f2
$ffa529 60            rts
$ffa52a bf 25 01 e1   lda $e10125,x
$ffa52e 20 99 a9      jsr $a999
$ffa531 80 f0         bra $ffa523
$ffa533 78            sei
$ffa534 a9 00         lda #$00
$ffa536 00 8f         brk $8f
$ffa538 ff 00 e1 5b   sbc $5be100,x
$ffa53c 48            pha
$ffa53d ab            plb
$ffa53e ab            plb
$ffa53f a3 04         lda $04,sp
$ffa541 85 fb         sta $fb
$ffa543 a3 06         lda $06,sp
$ffa545 85 fd         sta $fd
$ffa547 a3 08         lda $08,sp
$ffa549 a2 7f         ldx #$7f
$ffa54b 01 9a         ora ($9a,x)
$ffa54d 48            pha
$ffa54e e2 30         sep #$30
$ffa550 08            php
$ffa551 a9 80         lda #$80
$ffa553 8f 38 01 e1   sta $e10138
$ffa557 8d 07 c0      sta $c007
$ffa55a a9 08         lda #$08
$ffa55c 0c 68 c0      tsb $c068
$ffa55f 20 00 a8      jsr $a800
$ffa562 20 eb cc      jsr $cceb
$ffa565 20 ee a9      jsr $a9ee
$ffa568 28            plp
$ffa569 b0 19         bcs $ffa584
$ffa56b a7 fb         lda [$fb]
$ffa56d 1a            inc
$ffa56e 85 ff         sta $ff
$ffa570 a0 01         ldy #$01
$ffa572 b7 fb         lda [$fb],y
$ffa574 09 80         ora #$80
$ffa576 5a            phy
$ffa577 20 d1 a9      jsr $a9d1
$ffa57a 7a            ply
$ffa57b c8            iny
$ffa57c c4 ff         cpy $ff
$ffa57e 90 f2         bcc $ffa572
$ffa580 58            cli
$ffa581 b0 1f         bcs $ffa5a2
$ffa583 18            clc
$ffa584 08            php
$ffa585 20 f3 a9      jsr $a9f3
$ffa588 20 ee a9      jsr $a9ee
$ffa58b a2 00         ldx #$00
$ffa58d a0 05         ldy #$05
$ffa58f 20 e0 a9      jsr $a9e0
$ffa592 28            plp
$ffa593 08            php
$ffa594 b0 04         bcs $ffa59a
$ffa596 a9 03         lda #$03
$ffa598 80 02         bra $ffa59c
$ffa59a a9 0c         lda #$0c
$ffa59c 20 80 c0      jsr $c080
$ffa59f 28            plp
$ffa5a0 90 05         bcc $ffa5a7
$ffa5a2 fa            plx
$ffa5a3 7a            ply
$ffa5a4 20 9e a9      jsr $a99e
$ffa5a7 8d 0f c0      sta $c00f
$ffa5aa 20 51 9a      jsr $9a51
$ffa5ad a9 06         lda #$06
$ffa5af 85 22         sta $22
$ffa5b1 a0 00         ldy #$00
$ffa5b3 f0 05         beq $ffa5ba
$ffa5b5 a9 df         lda #$df
$ffa5b7 99 a8 05      sta $05a8,y
$ffa5ba a9 41         lda #$41
$ffa5bc 99 a9 05      sta $05a9,y
$ffa5bf 20 da a5      jsr $a5da
$ffa5c2 c8            iny
$ffa5c3 c0 26         cpy #$26
$ffa5c5 90 ec         bcc $ffa5b3
$ffa5c7 88            dey
$ffa5c8 a9 4c         lda #$4c
$ffa5ca 99 a9 05      sta $05a9,y
$ffa5cd a9 40         lda #$40
$ffa5cf 99 a8 05      sta $05a8,y
$ffa5d2 20 da a5      jsr $a5da
$ffa5d5 88            dey
$ffa5d6 d0 f0         bne $ffa5c8
$ffa5d8 80 d9         bra $ffa5b3
$ffa5da ad 00 c0      lda $c000
$ffa5dd 8d 10 c0      sta $c010
$ffa5e0 c9 8e         cmp #$8e
$ffa5e2 d0 0d         bne $ffa5f1
$ffa5e4 ad 62 c0      lda $c062
$ffa5e7 10 08         bpl $ffa5f1
$ffa5e9 ad 61 c0      lda $c061
$ffa5ec 10 03         bpl $ffa5f1
$ffa5ee 20 fc a5      jsr $a5fc
$ffa5f1 ad 19 c0      lda $c019
$ffa5f4 10 fb         bpl $ffa5f1
$ffa5f6 ad 19 c0      lda $c019
$ffa5f9 30 fb         bmi $ffa5f6
$ffa5fb 60            rts
$ffa5fc 5a            phy
$ffa5fd 20 ee a9      jsr $a9ee
$ffa600 a9 06         lda #$06
$ffa602 85 25         sta $25
$ffa604 64 24         stz $24
$ffa606 20 e4 a9      jsr $a9e4
$ffa609 22 18 00 fe   jsr $fe0018
$ffa60d a9 f3         lda #$f3
$ffa60f 20 24 a6      jsr $a624
$ffa612 a9 0f         lda #$0f
$ffa614 85 25         sta $25
$ffa616 a9 02         lda #$02
$ffa618 85 24         sta $24
$ffa61a 20 e4 a9      jsr $a9e4
$ffa61d a9 f4         lda #$f4
$ffa61f 20 24 a6      jsr $a624
$ffa622 7a            ply
$ffa623 60            rts
$ffa624 22 98 00 e1   jsr $e10098
$ffa628 a9 80         lda #$80
$ffa62a 1c 36 c0      trb $c036
$ffa62d a2 10         ldx #$10
$ffa62f a9 ff         lda #$ff
$ffa631 20 a8 fc      jsr $fca8
$ffa634 ca            dex
$ffa635 d0 f8         bne $ffa62f
$ffa637 a9 80         lda #$80
$ffa639 0c 36 c0      tsb $c036
$ffa63c 20 ee a9      jsr $a9ee
$ffa63f 60            rts
$ffa640 18            clc
$ffa641 c2 30         rep #$30
$ffa643 af 40 01 e1   lda $e10140
$ffa647 90 03         bcc $ffa64c
$ffa649 49 ff ff      eor #$ffff
$ffa64c 48            pha
$ffa64d a5 3e         lda $3e
$ffa64f 90 03         bcc $ffa654
$ffa651 49 ff ff      eor #$ffff
$ffa654 65 3c         adc $3c
$ffa656 85 3e         sta $3e
$ffa658 af 46 01 e1   lda $e10146
$ffa65c 63 01         adc $01,sp
$ffa65e 83 01         sta $01,sp
$ffa660 e2 30         sep #$30
$ffa662 20 74 a6      jsr $a674
$ffa665 fa            plx
$ffa666 68            pla
$ffa667 20 9f a9      jsr $a99f
$ffa66a a5 3f         lda $3f
$ffa66c a6 3e         ldx $3e
$ffa66e 20 9f a9      jsr $a99f
$ffa671 4c 52 ac      jmp $ac52
$ffa674 a9 0a         lda #$0a
$ffa676 4c 80 c0      jmp $c080
$ffa679 c9 ad         cmp #$ad
$ffa67b f0 c4         beq $ffa641
$ffa67d c9 ab         cmp #$ab
$ffa67f f0 bf         beq $ffa640
$ffa681 c9 aa         cmp #$aa
$ffa683 f0 01         beq $ffa686
$ffa685 18            clc
$ffa686 08            php
$ffa687 a2 04         ldx #$04
$ffa689 20 e6 a6      jsr $a6e6
$ffa68c af 46 01 e1   lda $e10146
$ffa690 48            pha
$ffa691 a5 3c         lda $3c
$ffa693 48            pha
$ffa694 af 40 01 e1   lda $e10140
$ffa698 48            pha
$ffa699 a5 3e         lda $3e
$ffa69b 48            pha
$ffa69c a2 0b         ldx #$0b
$ffa69e 0c b0 03      tsb $03b0
$ffa6a1 a2 0b         ldx #$0b
$ffa6a3 0d 22 00      ora $0022
$ffa6a6 00 e1         brk $e1
$ffa6a8 e2 30         sep #$30
$ffa6aa a2 00         ldx #$00
$ffa6ac 68            pla
$ffa6ad 95 3c         sta $3c,x
$ffa6af e8            inx
$ffa6b0 e0 08         cpx #$08
$ffa6b2 d0 f8         bne $ffa6ac
$ffa6b4 28            plp
$ffa6b5 08            php
$ffa6b6 b0 05         bcs $ffa6bd
$ffa6b8 a9 d2         lda #$d2
$ffa6ba 20 d1 a9      jsr $a9d1
$ffa6bd 20 74 a6      jsr $a674
$ffa6c0 a2 07         ldx #$07
$ffa6c2 b5 3c         lda $3c,x
$ffa6c4 da            phx
$ffa6c5 20 a3 a9      jsr $a9a3
$ffa6c8 e0 04         cpx #$04
$ffa6ca d0 10         bne $ffa6dc
$ffa6cc a3 02         lda $02,sp
$ffa6ce 6a            ror
$ffa6cf b0 0b         bcs $ffa6dc
$ffa6d1 20 c8 a9      jsr $a9c8
$ffa6d4 a9 d1         lda #$d1
$ffa6d6 20 d1 a9      jsr $a9d1
$ffa6d9 20 74 a6      jsr $a674
$ffa6dc fa            plx
$ffa6dd ca            dex
$ffa6de 10 e2         bpl $ffa6c2
$ffa6e0 28            plp
$ffa6e1 80 8e         bra $ffa671
$ffa6e3 60            rts
$ffa6e4 a2 03         ldx #$03
$ffa6e6 c2 30         rep #$30
$ffa6e8 68            pla
$ffa6e9 f4 00 00      pea $0000
$ffa6ec ca            dex
$ffa6ed d0 fa         bne $ffa6e9
$ffa6ef 48            pha
$ffa6f0 60            rts
$ffa6f1 a2 06 00      ldx #$0006
$ffa6f4 bf 00 00 f8   lda $f80000,x
$ffa6f8 df 0d a7 ff   cmp $ffa70d,x
$ffa6fc d0 0d         bne $ffa70b
$ffa6fe ca            dex
$ffa6ff ca            dex
$ffa700 10 f2         bpl $ffa6f4
$ffa702 22 20 00 f8   jsr $f80020
$ffa706 af 08 00 f8   lda $f80008
$ffa70a 6b            rtl
$ffa70b 38            sec
$ffa70c 6b            rtl
$ffa70d d2 cf         cmp ($cf)
$ffa70f cd d4 cf      cmp $cfd4
$ffa712 cf cc d3 da   cmp $dad3cc
$ffa716 20 11 a8      jsr $a811
$ffa719 fa            plx
$ffa71a 20 da a7      jsr $a7da
$ffa71d 80 62         bra $ffa781
$ffa71f 18            clc
$ffa720 af 3e 01 e1   lda $e1013e
$ffa724 f0 13         beq $ffa739
$ffa726 af 32 01 e1   lda $e10132
$ffa72a f0 0d         beq $ffa739
$ffa72c 4c 51 9a      jmp $9a51
$ffa72f 38            sec
$ffa730 af 3e 01 e1   lda $e1013e
$ffa734 f0 03         beq $ffa739
$ffa736 4c 51 9a      jmp $9a51
$ffa739 8b            phb
$ffa73a ad 68 c0      lda $c068
$ffa73d 48            pha
$ffa73e 20 da a7      jsr $a7da
$ffa741 ae 35 c0      ldx $c035
$ffa744 da            phx
$ffa745 ba            tsx
$ffa746 8e 3c 01      stx $013c
$ffa749 08            php
$ffa74a c9 00 02      cmp #$0200
$ffa74d b0 0a         bcs $ffa759
$ffa74f cd 3c 01      cmp $013c
$ffa752 70 05         bvs $ffa759
$ffa754 90 03         bcc $ffa759
$ffa756 ad 3c 01      lda $013c
$ffa759 28            plp
$ffa75a 48            pha
$ffa75b a3 06         lda $06,sp
$ffa75d aa            tax
$ffa75e 68            pla
$ffa75f e2 30         sep #$30
$ffa761 50 03         bvc $ffa766
$ffa763 8d 09 c0      sta $c009
$ffa766 1b            tcs
$ffa767 b0 06         bcs $ffa76f
$ffa769 4b            phk
$ffa76a f4 b5 a7      pea $a7b5
$ffa76d 90 0e         bcc $ffa77d
$ffa76f 4b            phk
$ffa770 f4 b6 a7      pea $a7b6
$ffa773 a2 5c         ldx #$5c
$ffa775 da            phx
$ffa776 f4 18 fb      pea $fb18
$ffa779 c2 20         rep #$20
$ffa77b 3b            tsc
$ffa77c 48            pha
$ffa77d 20 4c 7a      jsr $7a4c
$ffa780 3b            tsc
$ffa781 e2 30         sep #$30
$ffa783 50 03         bvc $ffa788
$ffa785 8d 09 c0      sta $c009
$ffa788 1b            tcs
$ffa789 ad 3c 01      lda $013c
$ffa78c 8f 00 01 01   sta $010100
$ffa790 ac 32 01      ldy $0132
$ffa793 d0 04         bne $ffa799
$ffa795 ad 3e 01      lda $013e
$ffa798 48            pha
$ffa799 c2 20         rep #$20
$ffa79b ad 3a 01      lda $013a
$ffa79e 48            pha
$ffa79f e2 30         sep #$30
$ffa7a1 af 32 01 e1   lda $e10132
$ffa7a5 d0 03         bne $ffa7aa
$ffa7a7 4c 9c 9f      jmp $9f9c
$ffa7aa af 2a 01 e1   lda $e1012a
$ffa7ae 48            pha
$ffa7af 20 68 7a      jsr $7a68
$ffa7b2 5c 72 c0 00   jmp $00c072
$ffa7b6 d8            cld
$ffa7b7 18            clc
$ffa7b8 fb            xce
$ffa7b9 e2 30         sep #$30
$ffa7bb 8d 08 c0      sta $c008
$ffa7be c2 30         rep #$30
$ffa7c0 20 82 f8      jsr $f882
$ffa7c3 ad 3c 01      lda $013c
$ffa7c6 1b            tcs
$ffa7c7 68            pla
$ffa7c8 8d 35 c0      sta $c035
$ffa7cb e2 30         sep #$30
$ffa7cd 20 2e 7a      jsr $7a2e
$ffa7d0 f4 00 00      pea $0000
$ffa7d3 2b            pld
$ffa7d4 68            pla
$ffa7d5 20 33 b8      jsr $b833
$ffa7d8 ab            plb
$ffa7d9 60            rts
$ffa7da 78            sei
$ffa7db 20 62 fe      jsr $fe62
$ffa7de 20 82 f8      jsr $f882
$ffa7e1 e2 40         sep #$40
$ffa7e3 ad 2d 01      lda $012d
$ffa7e6 30 01         bmi $ffa7e9
$ffa7e8 b8            clv
$ffa7e9 c2 30         rep #$30
$ffa7eb a5 3a         lda $3a
$ffa7ed 8d 3a 01      sta $013a
$ffa7f0 ad 26 01      lda $0126
$ffa7f3 60            rts
$ffa7f4 af a8 01 e1   lda $e101a8
$ffa7f8 49 80 8f      eor #$8f80
$ffa7fb a8            tay
$ffa7fc 01 e1         ora ($e1,x)
$ffa7fe 30 11         bmi $ffa811
$ffa800 64 22         stz $22
$ffa802 a9 80 1c      lda #$1c80
$ffa805 29 c0 ad      and #$adc0
$ffa808 51 c0         eor ($c0),y
$ffa80a ad 52 c0      lda $c052
$ffa80d ad 54 c0      lda $c054
$ffa810 60            rts
$ffa811 8b            phb
$ffa812 20 82 f8      jsr $f882
$ffa815 e2 30         sep #$30
$ffa817 ad bb 01      lda $01bb
$ffa81a 48            pha
$ffa81b 29 80         and #$80
$ffa81d 0d 29 c0      ora $c029
$ffa820 8d 29 c0      sta $c029
$ffa823 68            pla
$ffa824 0a            asl
$ffa825 10 05         bpl $ffa82c
$ffa827 8d 51 c0      sta $c051
$ffa82a 80 03         bra $ffa82f
$ffa82c 8d 50 c0      sta $c050
$ffa82f 0a            asl
$ffa830 10 05         bpl $ffa837
$ffa832 8d 53 c0      sta $c053
$ffa835 80 03         bra $ffa83a
$ffa837 8d 52 c0      sta $c052
$ffa83a 0a            asl
$ffa83b 10 05         bpl $ffa842
$ffa83d 8d 57 c0      sta $c057
$ffa840 80 03         bra $ffa845
$ffa842 8d 56 c0      sta $c056
$ffa845 0a            asl
$ffa846 10 05         bpl $ffa84d
$ffa848 8d 55 c0      sta $c055
$ffa84b 80 03         bra $ffa850
$ffa84d 8d 54 c0      sta $c054
$ffa850 ab            plb
$ffa851 60            rts
$ffa852 c2 30         rep #$30
$ffa854 ad 01 02      lda $0201
$ffa857 8f 3c 01 e1   sta $e1013c
$ffa85b e2 30         sep #$30
$ffa85d a8            tay
$ffa85e c8            iny
$ffa85f c8            iny
$ffa860 a2 00         ldx #$00
$ffa862 bd 03 02      lda $0203,x
$ffa865 48            pha
$ffa866 e8            inx
$ffa867 88            dey
$ffa868 d0 f8         bne $ffa862
$ffa86a c2 30         rep #$30
$ffa86c fa            plx
$ffa86d 22 00 00 e1   jsr $e10000
$ffa871 48            pha
$ffa872 48            pha
$ffa873 e2 30         sep #$30
$ffa875 20 cf a9      jsr $a9cf
$ffa878 a9 0b         lda #$0b
$ffa87a 20 80 c0      jsr $c080
$ffa87d fa            plx
$ffa87e 7a            ply
$ffa87f 20 9e a9      jsr $a99e
$ffa882 20 cf a9      jsr $a9cf
$ffa885 c2 30         rep #$30
$ffa887 68            pla
$ffa888 f0 0b         beq $ffa895
$ffa88a c9 10 00      cmp #$0010
$ffa88d b0 06         bcs $ffa895
$ffa88f af 3c 01 e1   lda $e1013c
$ffa893 80 04         bra $ffa899
$ffa895 af 3d 01 e1   lda $e1013d
$ffa899 e2 30         sep #$30
$ffa89b a8            tay
$ffa89c f0 b3         beq $ffa851
$ffa89e 88            dey
$ffa89f 68            pla
$ffa8a0 5a            phy
$ffa8a1 20 a3 a9      jsr $a9a3
$ffa8a4 20 91 ac      jsr $ac91
$ffa8a7 7a            ply
$ffa8a8 80 f2         bra $ffa89c
$ffa8aa 20 1d a9      jsr $a91d
$ffa8ad a4 34         ldy $34
$ffa8af b9 00 02      lda $0200,y
$ffa8b2 c9 bd         cmp #$bd
$ffa8b4 d0 9b         bne $ffa851
$ffa8b6 e6 34         inc $34
$ffa8b8 9c 00 02      stz $0200
$ffa8bb a4 34         ldy $34
$ffa8bd 20 4b b2      jsr $b24b
$ffa8c0 84 34         sty $34
$ffa8c2 c9 c6         cmp #$c6
$ffa8c4 f0 29         beq $ffa8ef
$ffa8c6 20 4c a9      jsr $a94c
$ffa8c9 f0 f0         beq $ffa8bb
$ffa8cb 48            pha
$ffa8cc ad 00 02      lda $0200
$ffa8cf c9 02         cmp #$02
$ffa8d1 b0 03         bcs $ffa8d6
$ffa8d3 fa            plx
$ffa8d4 ca            dex
$ffa8d5 da            phx
$ffa8d6 c9 02         cmp #$02
$ffa8d8 d0 0c         bne $ffa8e6
$ffa8da 68            pla
$ffa8db c9 28         cmp #$28
$ffa8dd b0 03         bcs $ffa8e2
$ffa8df 18            clc
$ffa8e0 69 64         adc #$64
$ffa8e2 48            pha
$ffa8e3 ad 00 02      lda $0200
$ffa8e6 c9 06         cmp #$06
$ffa8e8 b0 22         bcs $ffa90c
$ffa8ea ee 00 02      inc $0200
$ffa8ed 80 cc         bra $ffa8bb
$ffa8ef 20 4c a9      jsr $a94c
$ffa8f2 f0 04         beq $ffa8f8
$ffa8f4 48            pha
$ffa8f5 ee 00 02      inc $0200
$ffa8f8 ae 00 02      ldx $0200
$ffa8fb e0 06         cpx #$06
$ffa8fd b0 0d         bcs $ffa90c
$ffa8ff 8a            txa
$ffa900 f0 04         beq $ffa906
$ffa902 68            pla
$ffa903 ca            dex
$ffa904 80 f9         bra $ffa8ff
$ffa906 20 51 9a      jsr $9a51
$ffa909 4c 4d a3      jmp $a34d
$ffa90c c2 30         rep #$30
$ffa90e a2 03 0e      ldx #$0e03
$ffa911 22 00 00 e1   jsr $e10000
$ffa915 e2 30         sep #$30
$ffa917 20 1d a9      jsr $a91d
$ffa91a 4c 4d a3      jmp $a34d
$ffa91d 20 cf a9      jsr $a9cf
$ffa920 a9 04         lda #$04
$ffa922 20 80 c0      jsr $c080
$ffa925 08            php
$ffa926 78            sei
$ffa927 f4 e1 00      pea $00e1
$ffa92a f4 08 01      pea $0108
$ffa92d c2 30         rep #$30
$ffa92f a2 03 0f      ldx #$0f03
$ffa932 22 00 00 e1   jsr $e10000
$ffa936 e2 30         sep #$30
$ffa938 a2 00         ldx #$00
$ffa93a bf 08 01 e1   lda $e10108,x
$ffa93e da            phx
$ffa93f 20 d1 a9      jsr $a9d1
$ffa942 fa            plx
$ffa943 e8            inx
$ffa944 e0 14         cpx #$14
$ffa946 90 f2         bcc $ffa93a
$ffa948 28            plp
$ffa949 4c cf a9      jmp $a9cf
$ffa94c af 42 01 e1   lda $e10142
$ffa950 08            php
$ffa951 a5 3e         lda $3e
$ffa953 48            pha
$ffa954 29 0f         and #$0f
$ffa956 85 3e         sta $3e
$ffa958 68            pla
$ffa959 20 7b f8      jsr $f87b
$ffa95c aa            tax
$ffa95d 20 64 a9      jsr $a964
$ffa960 a5 3e         lda $3e
$ffa962 28            plp
$ffa963 60            rts
$ffa964 a9 0a         lda #$0a
$ffa966 85 3c         sta $3c
$ffa968 a5 3c         lda $3c
$ffa96a 18            clc
$ffa96b ca            dex
$ffa96c 30 06         bmi $ffa974
$ffa96e 65 3e         adc $3e
$ffa970 85 3e         sta $3e
$ffa972 80 f4         bra $ffa968
$ffa974 60            rts
$ffa975 f4 19 fc      pea $fc19
$ffa978 80 2c         bra $ffa9a6
$ffa97a 20 86 a9      jsr $a986
$ffa97d a6 3c         ldx $3c
$ffa97f a4 3d         ldy $3d
$ffa981 f4 98 fd      pea $fd98
$ffa984 80 20         bra $ffa9a6
$ffa986 20 cf a9      jsr $a9cf
$ffa989 af 3e 01 e1   lda $e1013e
$ffa98d 20 a3 a9      jsr $a9a3
$ffa990 a9 af         lda #$af
$ffa992 80 3d         bra $ffa9d1
$ffa994 f4 66 fd      pea $fd66
$ffa997 80 0d         bra $ffa9a6
$ffa999 f4 e2 fd      pea $fde2
$ffa99c 80 08         bra $ffa9a6
$ffa99e 98            tya
$ffa99f 20 a3 a9      jsr $a9a3
$ffa9a2 8a            txa
$ffa9a3 f4 d9 fd      pea $fdd9
$ffa9a6 5c 9c 01 e1   jmp $e1019c
$ffa9aa eb            xba
$ffa9ab 8a            txa
$ffa9ac c2 30         rep #$30
$ffa9ae fa            plx
$ffa9af 4b            phk
$ffa9b0 f4 20 9c      pea $9c20
$ffa9b3 f4 d3 f8      pea $f8d3
$ffa9b6 da            phx
$ffa9b7 e2 30         sep #$30
$ffa9b9 aa            tax
$ffa9ba eb            xba
$ffa9bb eb            xba
$ffa9bc 20 d6 ff      jsr $ffd6
$ffa9bf eb            xba
$ffa9c0 8b            phb
$ffa9c1 f4 b4 f8      pea $f8b4
$ffa9c4 8d 06 c0      sta $c006
$ffa9c7 6b            rtl
$ffa9c8 a2 03         ldx #$03
$ffa9ca f4 49 f9      pea $f949
$ffa9cd 80 d7         bra $ffa9a6
$ffa9cf a9 8d         lda #$8d
$ffa9d1 f4 ec fd      pea $fdec
$ffa9d4 80 d0         bra $ffa9a6
$ffa9d6 f4 ef fd      pea $fdef
$ffa9d9 80 cb         bra $ffa9a6
$ffa9db f4 6b ff      pea $ff6b
$ffa9de 80 db         bra $ffa9bb
$ffa9e0 86 24         stx $24
$ffa9e2 84 25         sty $25
$ffa9e4 f4 21 fc      pea $fc21
$ffa9e7 80 bd         bra $ffa9a6
$ffa9e9 f4 ff c2      pea $c2ff
$ffa9ec 80 b8         bra $ffa9a6
$ffa9ee f4 57 fc      pea $fc57
$ffa9f1 80 b3         bra $ffa9a6
$ffa9f3 f4 42 fe      pea $fe42
$ffa9f6 80 ae         bra $ffa9a6
$ffa9f8 a9 98         lda #$98
$ffa9fa 20 e9 a9      jsr $a9e9
$ffa9fd a9 00         lda #$00
$ffa9ff 8f 37 01 e1   sta $e10137
$ffaa03 60            rts
$ffaa04 af 3e 01 e1   lda $e1013e
$ffaa08 cf a7 01 e1   cmp $e101a7
$ffaa0c f0 06         beq $ffaa14
$ffaa0e 90 26         bcc $ffaa36
$ffaa10 64 3e         stz $3e
$ffaa12 64 3f         stz $3f
$ffaa14 20 7a a9      jsr $a97a
$ffaa17 a5 3d         lda $3d
$ffaa19 85 43         sta $43
$ffaa1b a5 3c         lda $3c
$ffaa1d 85 42         sta $42
$ffaa1f 20 e7 b7      jsr $b7e7
$ffaa22 20 a3 a9      jsr $a9a3
$ffaa25 20 ba fc      jsr $fcba
$ffaa28 b0 1a         bcs $ffaa44
$ffaa2a 20 75 fe      jsr $fe75
$ffaa2d 25 3c         and $3c
$ffaa2f f0 13         beq $ffaa44
$ffaa31 20 91 ac      jsr $ac91
$ffaa34 80 e9         bra $ffaa1f
$ffaa36 a5 3f         lda $3f
$ffaa38 48            pha
$ffaa39 a5 3e         lda $3e
$ffaa3b 48            pha
$ffaa3c a9 ff         lda #$ff
$ffaa3e 85 3f         sta $3f
$ffaa40 85 3e         sta $3e
$ffaa42 80 d0         bra $ffaa14
$ffaa44 a5 43         lda $43
$ffaa46 85 3d         sta $3d
$ffaa48 a5 42         lda $42
$ffaa4a 85 3c         sta $3c
$ffaa4c a9 ad         lda #$ad
$ffaa4e 20 d1 a9      jsr $a9d1
$ffaa51 20 e9 b7      jsr $b7e9
$ffaa54 09 80         ora #$80
$ffaa56 c9 a0         cmp #$a0
$ffaa58 90 04         bcc $ffaa5e
$ffaa5a c9 ff         cmp #$ff
$ffaa5c d0 02         bne $ffaa60
$ffaa5e a9 ae         lda #$ae
$ffaa60 a8            tay
$ffaa61 a5 24         lda $24
$ffaa63 f0 0f         beq $ffaa74
$ffaa65 c9 27         cmp #$27
$ffaa67 08            php
$ffaa68 98            tya
$ffaa69 20 d1 a9      jsr $a9d1
$ffaa6c 28            plp
$ffaa6d 90 09         bcc $ffaa78
$ffaa6f 20 75 a9      jsr $a975
$ffaa72 80 04         bra $ffaa78
$ffaa74 98            tya
$ffaa75 20 d1 a9      jsr $a9d1
$ffaa78 20 74 cf      jsr $cf74
$ffaa7b 10 22         bpl $ffaa9f
$ffaa7d 29 7f         and #$7f
$ffaa7f c9 18         cmp #$18
$ffaa81 f0 0b         beq $ffaa8e
$ffaa83 c9 13         cmp #$13
$ffaa85 d0 18         bne $ffaa9f
$ffaa87 20 74 cf      jsr $cf74
$ffaa8a 10 fb         bpl $ffaa87
$ffaa8c 80 11         bra $ffaa9f
$ffaa8e af 3e 01 e1   lda $e1013e
$ffaa92 cf a7 01 e1   cmp $e101a7
$ffaa96 f0 06         beq $ffaa9e
$ffaa98 8f a7 01 e1   sta $e101a7
$ffaa9c 68            pla
$ffaa9d 68            pla
$ffaa9e 60            rts
$ffaa9f 20 ba fc      jsr $fcba
$ffaaa2 b0 09         bcs $ffaaad
$ffaaa4 20 75 fe      jsr $fe75
$ffaaa7 25 3c         and $3c
$ffaaa9 d0 a6         bne $ffaa51
$ffaaab 80 95         bra $ffaa42
$ffaaad 8b            phb
$ffaaae 20 82 f8      jsr $f882
$ffaab1 ee 3e 01      inc $013e
$ffaab4 ad 3e 01      lda $013e
$ffaab7 f0 1d         beq $ffaad6
$ffaab9 cd a7 01      cmp $01a7
$ffaabc f0 0f         beq $ffaacd
$ffaabe b0 16         bcs $ffaad6
$ffaac0 ab            plb
$ffaac1 64 3d         stz $3d
$ffaac3 64 3c         stz $3c
$ffaac5 a9 ff         lda #$ff
$ffaac7 85 3f         sta $3f
$ffaac9 85 3e         sta $3e
$ffaacb 80 de         bra $ffaaab
$ffaacd ab            plb
$ffaace 68            pla
$ffaacf 85 3e         sta $3e
$ffaad1 68            pla
$ffaad2 85 3f         sta $3f
$ffaad4 80 d5         bra $ffaaab
$ffaad6 ce 3e 01      dec $013e
$ffaad9 af 3e 01 e1   lda $e1013e
$ffaadd 8f a7 01 e1   sta $e101a7
$ffaae1 ab            plb
$ffaae2 60            rts
$ffaae3 c2 20         rep #$20
$ffaae5 18            clc
$ffaae6 98            tya
$ffaae7 65 3a         adc $3a
$ffaae9 85 40         sta $40
$ffaaeb e2 20         sep #$20
$ffaaed a2 40         ldx #$40
$ffaaef 60            rts
$ffaaf0 48            pha
$ffaaf1 29 03         and #$03
$ffaaf3 a8            tay
$ffaaf4 68            pla
$ffaaf5 4a            lsr
$ffaaf6 4a            lsr
$ffaaf7 aa            tax
$ffaaf8 bf 08 ab ff   lda $ffab08,x
$ffaafc 88            dey
$ffaafd 30 04         bmi $ffab03
$ffaaff 4a            lsr
$ffab00 4a            lsr
$ffab01 80 f9         bra $ffaafc
$ffab03 29 03         and #$03
$ffab05 85 2f         sta $2f
$ffab07 60            rts
$ffab08 04 14         tsb $14
$ffab0a 04 28         tsb $28
$ffab0c 05 14         ora $14
$ffab0e 08            php
$ffab0f 28            plp
$ffab10 06 15         asl $15
$ffab12 04 2a         tsb $2a
$ffab14 05 14         ora $14
$ffab16 08            php
$ffab17 28            plp
$ffab18 04 14         tsb $14
$ffab1a 04 2a         tsb $2a
$ffab1c 05 14         ora $14
$ffab1e 08            php
$ffab1f 28            plp
$ffab20 04 14         tsb $14
$ffab22 04 2a         tsb $2a
$ffab24 05 14         ora $14
$ffab26 08            php
$ffab27 28            plp
$ffab28 04 15         tsb $15
$ffab2a 00 2a         brk $2a
$ffab2c 05 15         ora $15
$ffab2e 08            php
$ffab2f 08            php
$ffab30 15 15         ora $15,x
$ffab32 04 2a         tsb $2a
$ffab34 05 15         ora $15
$ffab36 08            php
$ffab37 2a            rol
$ffab38 05 15         ora $15
$ffab3a 04 2a         tsb $2a
$ffab3c 05 14         ora $14
$ffab3e 08            php
$ffab3f 28            plp
$ffab40 05 15         ora $15
$ffab42 04 2a         tsb $2a
$ffab44 05 14         ora $14
$ffab46 08            php
$ffab47 28            plp
$ffab48 48            pha
$ffab49 20 74 cf      jsr $cf74
$ffab4c 10 0b         bpl $ffab59
$ffab4e 29 7f         and #$7f
$ffab50 c9 13         cmp #$13
$ffab52 d0 05         bne $ffab59
$ffab54 20 74 cf      jsr $cf74
$ffab57 10 fb         bpl $ffab54
$ffab59 68            pla
$ffab5a 60            rts
$ffab5b 20 62 fe      jsr $fe62
$ffab5e af 30 01 e1   lda $e10130
$ffab62 a2 ed         ldx #$ed
$ffab64 20 0d ae      jsr $ae0d
$ffab67 af 31 01 e1   lda $e10131
$ffab6b a2 f8         ldx #$f8
$ffab6d 20 0d ae      jsr $ae0d
$ffab70 af 33 01 e1   lda $e10133
$ffab74 a2 e4         ldx #$e4
$ffab76 20 0d ae      jsr $ae0d
$ffab79 af 2f 01 e1   lda $e1012f
$ffab7d a2 cc         ldx #$cc
$ffab7f 20 0d ae      jsr $ae0d
$ffab82 a9 07         lda #$07
$ffab84 20 80 c0      jsr $c080
$ffab87 20 52 ac      jsr $ac52
$ffab8a 38            sec
$ffab8b a5 23         lda $23
$ffab8d e5 22         sbc $22
$ffab8f 38            sec
$ffab90 e9 04         sbc #$04
$ffab92 10 05         bpl $ffab99
$ffab94 d0 03         bne $ffab99
$ffab96 4c 52 ac      jmp $ac52
$ffab99 20 48 ab      jsr $ab48
$ffab9c 48            pha
$ffab9d a0 00         ldy #$00
$ffab9f 20 8b b4      jsr $b48b
$ffaba2 48            pha
$ffaba3 af 33 01 e1   lda $e10133
$ffaba7 d0 21         bne $ffabca
$ffaba9 68            pla
$ffabaa c9 c2         cmp #$c2
$ffabac f0 50         beq $ffabfe
$ffabae c9 e2         cmp #$e2
$ffabb0 d0 19         bne $ffabcb
$ffabb2 20 8a b4      jsr $b48a
$ffabb5 0a            asl
$ffabb6 0a            asl
$ffabb7 0a            asl
$ffabb8 10 06         bpl $ffabc0
$ffabba a9 01         lda #$01
$ffabbc 8f 31 01 e1   sta $e10131
$ffabc0 90 52         bcc $ffac14
$ffabc2 a9 01         lda #$01
$ffabc4 8f 30 01 e1   sta $e10130
$ffabc8 80 4a         bra $ffac14
$ffabca 68            pla
$ffabcb c9 22         cmp #$22
$ffabcd f0 47         beq $ffac16
$ffabcf c9 20         cmp #$20
$ffabd1 d0 41         bne $ffac14
$ffabd3 20 8a b4      jsr $b48a
$ffabd6 0f 3e 01 e1   ora $e1013e
$ffabda d0 38         bne $ffac14
$ffabdc 20 8a b4      jsr $b48a
$ffabdf c9 bf         cmp #$bf
$ffabe1 d0 31         bne $ffac14
$ffabe3 a3 01         lda $01,sp
$ffabe5 c9 03         cmp #$03
$ffabe7 90 46         bcc $ffac2f
$ffabe9 20 2c ad      jsr $ad2c
$ffabec 20 7b ac      jsr $ac7b
$ffabef 68            pla
$ffabf0 3a            dec
$ffabf1 48            pha
$ffabf2 20 5d ac      jsr $ac5d
$ffabf5 a0 01         ldy #$01
$ffabf7 20 5f ac      jsr $ac5f
$ffabfa 68            pla
$ffabfb 3a            dec
$ffabfc 80 4c         bra $ffac4a
$ffabfe 20 8a b4      jsr $b48a
$ffac01 0a            asl
$ffac02 0a            asl
$ffac03 0a            asl
$ffac04 10 06         bpl $ffac0c
$ffac06 a9 00         lda #$00
$ffac08 8f 31 01 e1   sta $e10131
$ffac0c 90 35         bcc $ffac43
$ffac0e a9 00         lda #$00
$ffac10 8f 30 01 e1   sta $e10130
$ffac14 80 2d         bra $ffac43
$ffac16 20 8a b4      jsr $b48a
$ffac19 c9 a8         cmp #$a8
$ffac1b d0 26         bne $ffac43
$ffac1d 20 8a b4      jsr $b48a
$ffac20 c9 00         cmp #$00
$ffac22 d0 1f         bne $ffac43
$ffac24 20 8a b4      jsr $b48a
$ffac27 c9 e1         cmp #$e1
$ffac29 d0 18         bne $ffac43
$ffac2b a3 01         lda $01,sp
$ffac2d c9 03         cmp #$03
$ffac2f 90 29         bcc $ffac5a
$ffac31 20 2c ad      jsr $ad2c
$ffac34 20 7b ac      jsr $ac7b
$ffac37 68            pla
$ffac38 3a            dec
$ffac39 48            pha
$ffac3a a0 01         ldy #$01
$ffac3c 20 5f ac      jsr $ac5f
$ffac3f a0 03         ldy #$03
$ffac41 80 b4         bra $ffabf7
$ffac43 20 2c ad      jsr $ad2c
$ffac46 20 7b ac      jsr $ac7b
$ffac49 68            pla
$ffac4a 3a            dec
$ffac4b f0 05         beq $ffac52
$ffac4d 30 03         bmi $ffac52
$ffac4f 4c 99 ab      jmp $ab99
$ffac52 20 48 ab      jsr $ab48
$ffac55 20 cf a9      jsr $a9cf
$ffac58 80 37         bra $ffac91
$ffac5a 68            pla
$ffac5b 80 f5         bra $ffac52
$ffac5d a0 00         ldy #$00
$ffac5f 5a            phy
$ffac60 20 87 ac      jsr $ac87
$ffac63 7a            ply
$ffac64 84 2f         sty $2f
$ffac66 20 0d ad      jsr $ad0d
$ffac69 a2 05         ldx #$05
$ffac6b 20 ca a9      jsr $a9ca
$ffac6e a4 2f         ldy $2f
$ffac70 20 8b b4      jsr $b48b
$ffac73 5a            phy
$ffac74 20 a3 a9      jsr $a9a3
$ffac77 7a            ply
$ffac78 88            dey
$ffac79 10 f5         bpl $ffac70
$ffac7b 20 53 f9      jsr $f953
$ffac7e 85 3a         sta $3a
$ffac80 84 3b         sty $3b
$ffac82 85 40         sta $40
$ffac84 84 41         sty $41
$ffac86 60            rts
$ffac87 20 86 a9      jsr $a986
$ffac8a a6 3a         ldx $3a
$ffac8c a4 3b         ldy $3b
$ffac8e 20 81 a9      jsr $a981
$ffac91 a9 a0         lda #$a0
$ffac93 4c d1 a9      jmp $a9d1
$ffac96 20 87 ac      jsr $ac87
$ffac99 20 8b b4      jsr $b48b
$ffac9c 4a            lsr
$ffac9d 85 3f         sta $3f
$ffac9f a8            tay
$ffaca0 08            php
$ffaca1 08            php
$ffaca2 90 04         bcc $ffaca8
$ffaca4 29 0f         and #$0f
$ffaca6 09 80         ora #$80
$ffaca8 4a            lsr
$ffaca9 aa            tax
$ffacaa bf 24 ae ff   lda $ffae24,x
$ffacae 20 79 f8      jsr $f879
$ffacb1 28            plp
$ffacb2 90 02         bcc $ffacb6
$ffacb4 69 07         adc #$07
$ffacb6 85 2e         sta $2e
$ffacb8 aa            tax
$ffacb9 e0 08         cpx #$08
$ffacbb d0 16         bne $ffacd3
$ffacbd a5 3f         lda $3f
$ffacbf 29 04         and #$04
$ffacc1 f0 06         beq $ffacc9
$ffacc3 af 30 01 e1   lda $e10130
$ffacc7 80 04         bra $ffaccd
$ffacc9 af 31 01 e1   lda $e10131
$ffaccd d0 04         bne $ffacd3
$ffaccf a9 02         lda #$02
$ffacd1 80 08         bra $ffacdb
$ffacd3 bf 83 ae ff   lda $ffae83,x
$ffacd7 85 3f         sta $3f
$ffacd9 29 03         and #$03
$ffacdb 85 2f         sta $2f
$ffacdd 98            tya
$ffacde 28            plp
$ffacdf 90 18         bcc $ffacf9
$fface1 29 07         and #$07
$fface3 c9 05         cmp #$05
$fface5 d0 03         bne $ffacea
$fface7 88            dey
$fface8 80 20         bra $ffad0a
$ffacea 98            tya
$ffaceb c9 44         cmp #$44
$ffaced f0 06         beq $ffacf5
$ffacef 29 70         and #$70
$ffacf1 09 09         ora #$09
$ffacf3 80 17         bra $ffad0c
$ffacf5 a9 12         lda #$12
$ffacf7 80 13         bra $ffad0c
$ffacf9 29 03         and #$03
$ffacfb c9 03         cmp #$03
$ffacfd d0 0c         bne $ffad0b
$ffacff 98            tya
$ffad00 c9 4f         cmp #$4f
$ffad02 f0 06         beq $ffad0a
$ffad04 29 70         and #$70
$ffad06 09 07         ora #$07
$ffad08 80 02         bra $ffad0c
$ffad0a 88            dey
$ffad0b 98            tya
$ffad0c 60            rts
$ffad0d a0 00         ldy #$00
$ffad0f 20 8b b4      jsr $b48b
$ffad12 e6 40         inc $40
$ffad14 d0 02         bne $ffad18
$ffad16 e6 41         inc $41
$ffad18 20 a3 a9      jsr $a9a3
$ffad1b a2 01         ldx #$01
$ffad1d 20 ca a9      jsr $a9ca
$ffad20 c4 2f         cpy $2f
$ffad22 c8            iny
$ffad23 90 ea         bcc $ffad0f
$ffad25 a2 03         ldx #$03
$ffad27 c0 04         cpy #$04
$ffad29 90 f2         bcc $ffad1d
$ffad2b 60            rts
$ffad2c 20 96 ac      jsr $ac96
$ffad2f 48            pha
$ffad30 20 0d ad      jsr $ad0d
$ffad33 7a            ply
$ffad34 da            phx
$ffad35 bb            tyx
$ffad36 bf b2 ae ff   lda $ffaeb2,x
$ffad3a 85 2c         sta $2c
$ffad3c bf 32 af ff   lda $ffaf32,x
$ffad40 85 2d         sta $2d
$ffad42 20 91 ac      jsr $ac91
$ffad45 fa            plx
$ffad46 a9 00         lda #$00
$ffad48 a0 05         ldy #$05
$ffad4a 06 2d         asl $2d
$ffad4c 26 2c         rol $2c
$ffad4e 2a            rol
$ffad4f 88            dey
$ffad50 d0 f8         bne $ffad4a
$ffad52 69 bf         adc #$bf
$ffad54 20 d1 a9      jsr $a9d1
$ffad57 ca            dex
$ffad58 d0 ec         bne $ffad46
$ffad5a 20 91 ac      jsr $ac91
$ffad5d a5 2e         lda $2e
$ffad5f aa            tax
$ffad60 bf 83 ae ff   lda $ffae83,x
$ffad64 0a            asl
$ffad65 08            php
$ffad66 bf 6c ae ff   lda $ffae6c,x
$ffad6a 85 2e         sta $2e
$ffad6c a2 08         ldx #$08
$ffad6e a4 2f         ldy $2f
$ffad70 e0 05         cpx #$05
$ffad72 f0 27         beq $ffad9b
$ffad74 06 2e         asl $2e
$ffad76 90 18         bcc $ffad90
$ffad78 bf 99 ae ff   lda $ffae99,x
$ffad7c 28            plp
$ffad7d 08            php
$ffad7e 10 04         bpl $ffad84
$ffad80 bf a9 ae ff   lda $ffaea9,x
$ffad84 20 d1 a9      jsr $a9d1
$ffad87 bf a1 ae ff   lda $ffaea1,x
$ffad8b f0 03         beq $ffad90
$ffad8d 20 d1 a9      jsr $a9d1
$ffad90 ca            dex
$ffad91 d0 dd         bne $ffad70
$ffad93 28            plp
$ffad94 60            rts
$ffad95 88            dey
$ffad96 30 dc         bmi $ffad74
$ffad98 20 a3 a9      jsr $a9a3
$ffad9b a5 2e         lda $2e
$ffad9d c9 f8         cmp #$f8
$ffad9f 20 8b b4      jsr $b48b
$ffada2 85 3f         sta $3f
$ffada4 90 ef         bcc $ffad95
$ffada6 fa            plx
$ffada7 88            dey
$ffada8 5a            phy
$ffada9 f0 16         beq $ffadc1
$ffadab 48            pha
$ffadac 20 8b b4      jsr $b48b
$ffadaf 85 3e         sta $3e
$ffadb1 65 3a         adc $3a
$ffadb3 aa            tax
$ffadb4 68            pla
$ffadb5 65 3b         adc $3b
$ffadb7 a8            tay
$ffadb8 8a            txa
$ffadb9 18            clc
$ffadba 69 02         adc #$02
$ffadbc aa            tax
$ffadbd 90 0b         bcc $ffadca
$ffadbf 80 08         bra $ffadc9
$ffadc1 38            sec
$ffadc2 20 56 f9      jsr $f956
$ffadc5 aa            tax
$ffadc6 e8            inx
$ffadc7 d0 01         bne $ffadca
$ffadc9 c8            iny
$ffadca 20 9e a9      jsr $a99e
$ffadcd 20 91 ac      jsr $ac91
$ffadd0 a9 fb         lda #$fb
$ffadd2 20 d1 a9      jsr $a9d1
$ffadd5 a5 3f         lda $3f
$ffadd7 10 1d         bpl $ffadf6
$ffadd9 a9 ad         lda #$ad
$ffaddb 20 d1 a9      jsr $a9d1
$ffadde 18            clc
$ffaddf 68            pla
$ffade0 48            pha
$ffade1 f0 0a         beq $ffaded
$ffade3 a5 3e         lda $3e
$ffade5 49 ff         eor #$ff
$ffade7 69 01         adc #$01
$ffade9 85 3e         sta $3e
$ffadeb 80 01         bra $ffadee
$ffaded 38            sec
$ffadee a5 3f         lda $3f
$ffadf0 49 ff         eor #$ff
$ffadf2 69 00         adc #$00
$ffadf4 80 07         bra $ffadfd
$ffadf6 a9 ab         lda #$ab
$ffadf8 20 d1 a9      jsr $a9d1
$ffadfb a5 3f         lda $3f
$ffadfd 20 a3 a9      jsr $a9a3
$ffae00 68            pla
$ffae01 f0 05         beq $ffae08
$ffae03 a5 3e         lda $3e
$ffae05 20 a3 a9      jsr $a9a3
$ffae08 a9 fd         lda #$fd
$ffae0a 4c d1 a9      jmp $a9d1
$ffae0d da            phx
$ffae0e 20 99 a9      jsr $a999
$ffae11 a9 bd         lda #$bd
$ffae13 20 d1 a9      jsr $a9d1
$ffae16 68            pla
$ffae17 48            pha
$ffae18 20 d1 a9      jsr $a9d1
$ffae1b 68            pla
$ffae1c c9 cc         cmp #$cc
$ffae1e d0 01         bne $ffae21
$ffae20 60            rts
$ffae21 4c c8 a9      jmp $a9c8
$ffae24 bb            tyx
$ffae25 bb            tyx
$ffae26 cc 99 52      cpy $5299
$ffae29 db            stp
$ffae2a cc e9 a9      cpy $a9e9
$ffae2d bb            tyx
$ffae2e cc 99 52      cpy $5299
$ffae31 dd cc ee      cmp $eecc,x
$ffae34 bc b9 cc      ldy $ccb9,x
$ffae37 99 52 d9      sta $d952,y
$ffae3a cc ea 3c      cpy $3cea
$ffae3d bb            tyx
$ffae3e cc 94 52      cpy $5294
$ffae41 dd cc e6      cmp $e6cc,x
$ffae44 32 bb         and ($bb)
$ffae46 cc 99 52      cpy $5299
$ffae49 1d cc e9      ora $e9cc,x
$ffae4c 88            dey
$ffae4d bb            tyx
$ffae4e cc 99 52      cpy $5299
$ffae51 1d cc fe      ora $fecc,x
$ffae54 08            php
$ffae55 bb            tyx
$ffae56 cc 99 52      cpy $5299
$ffae59 db            stp
$ffae5a cc e4 08      cpy $08e4
$ffae5d bb            tyx
$ffae5e cc 99 52      cpy $5299
$ffae61 d9 cc e6      cmp $e6cc,y
$ffae64 b9 e3 40      lda $40e3,y
$ffae67 21 c8         and ($c8,x)
$ffae69 d5 47         cmp $47,x
$ffae6b a6 80         ldx $80
$ffae6d 10 1f         bpl $ffae8e
$ffae6f 1f 42 42 4a   ora $4a4242,x
$ffae73 42            ???
$ffae74 80 00         bra $ffae76
$ffae76 00 00         brk $00
$ffae78 00 08         brk $08
$ffae7a 08            php
$ffae7b 10 43         bpl $ffaec0
$ffae7d 4a            lsr
$ffae7e 08            php
$ffae7f 04 47         tsb $47
$ffae81 43 42         eor $42,sp
$ffae83 01 01         ora ($01,x)
$ffae85 01 02         ora ($02,x)
$ffae87 02 01         cop $01
$ffae89 02 02         cop $02
$ffae8b 01 02         ora ($02,x)
$ffae8d 03 01         ora $01,sp
$ffae8f 00 01         brk $01
$ffae91 02 02         cop $02
$ffae93 01 01         ora ($01,x)
$ffae95 03 01         ora $01,sp
$ffae97 01 41         ora ($41,x)
$ffae99 41 ac         eor ($ac,x)
$ffae9b a9 ac         lda #$ac
$ffae9d ac ac a4      ldy $a4ac
$ffaea0 a8            tay
$ffaea1 a3 d9         lda $d9,sp
$ffaea3 00 d3         brk $d3
$ffaea5 d8            cld
$ffaea6 d9 00 00      cmp $0000,y
$ffaea9 00 ac         brk $ac
$ffaeab dd ac ac      cmp $acac,x
$ffaeae ac a4 db      ldy $dba4
$ffaeb1 a3 1c         lda $1c,sp
$ffaeb3 24 ad         bit $ad
$ffaeb5 8a            txa
$ffaeb6 8a            txa
$ffaeb7 15 ad         ora $ad,x
$ffaeb9 15 1c         ora $1c,x
$ffaebb 84 ac         sty $ac
$ffaebd a9 23         lda #$23
$ffaebf 53 ac         eor ($ac,sp),y
$ffaec1 00 5d         brk $5d
$ffaec3 5d 1a 8b      eor $8b1a,x
$ffaec6 8b            phb
$ffaec7 9c 1a 9c      stz $9c1a
$ffaeca 1b            tcs
$ffaecb 13 1a         ora ($1a,sp),y
$ffaecd ad a1 29      lda $29a1
$ffaed0 1a            inc
$ffaed1 00 9d         brk $9d
$ffaed3 c1 75         cmp ($75,x)
$ffaed5 8a            txa
$ffaed6 8a            txa
$ffaed7 6d 5b 6d      adc $6d5b
$ffaeda 1d 34 75      ora $7534,x
$ffaedd a9 23         lda #$23
$ffaedf 8a            txa
$ffaee0 5b            tcd
$ffaee1 00 9d         brk $9d
$ffaee3 89 a5         bit #$a5
$ffaee5 9d 8b 9c      sta $9c8b,x
$ffaee8 5b            tcd
$ffaee9 9c 1d 11      stz $111d
$ffaeec a5 a9         lda $a9
$ffaeee a1 8b         lda ($8b,x)
$ffaef0 5b            tcd
$ffaef1 00 1c         brk $1c
$ffaef3 1c a5 8a      trb $8aa5
$ffaef6 29 ae         and #$ae
$ffaef8 a5 a5         lda $a5
$ffaefa 19 a5 a5      ora $a5a5,y
$ffaefd ae ae ae      ldx $aeae
$ffaf00 a5 00         lda $00
$ffaf02 69 69         adc #$69
$ffaf04 69 8b         adc #$8b
$ffaf06 a8            tay
$ffaf07 a8            tay
$ffaf08 69 69         adc #$69
$ffaf0a 19 69 69      ora $6969,y
$ffaf0d ae 23 ad      ldx $ad23
$ffaf10 69 00         adc #$00
$ffaf12 24 99         bit $99
$ffaf14 24 c0         bit $c0
$ffaf16 53 29         eor ($29,sp),y
$ffaf18 24 29         bit $29
$ffaf1a 1b            tcs
$ffaf1b 23 89         and $89,sp
$ffaf1d a5 23         lda $23
$ffaf1f 8a            txa
$ffaf20 5b            tcd
$ffaf21 00 24         brk $24
$ffaf23 a1 24         lda ($24,x)
$ffaf25 c8            iny
$ffaf26 53 7c         eor ($7c,sp),y
$ffaf28 24 53         bit $53
$ffaf2a 19 a0 89      ora $89a0,y
$ffaf2d c9 a1         cmp #$a1
$ffaf2f 8b            phb
$ffaf30 5d 00 d8      eor $d800,x
$ffaf33 22 06 4a 62   jsr $624a06
$ffaf37 1a            inc
$ffaf38 06 1a         asl $1a
$ffaf3a 5a            phy
$ffaf3b c4 c6         cpy $c6
$ffaf3d 28            plp
$ffaf3e 48            pha
$ffaf3f c8            iny
$ffaf40 c6 00         dec $00
$ffaf42 26 1a         rol $1a
$ffaf44 aa            tax
$ffaf45 4a            lsr
$ffaf46 62 1a aa      per $ff5963
$ffaf49 1a            inc
$ffaf4a 94 ca         sty $ca,x
$ffaf4c aa            tax
$ffaf4d 08            php
$ffaf4e 88            dey
$ffaf4f 88            dey
$ffaf50 aa            tax
$ffaf51 00 54         brk $54
$ffaf53 5c e2 58 44   jmp $4458e2
$ffaf57 26 a2         rol $a2
$ffaf59 26 c8         rol $c8
$ffaf5b 26 de         rol $de
$ffaf5d 0a            asl
$ffaf5e 54 74 a2      mvn $74,$a2
$ffaf61 00 68         brk $68
$ffaf63 a6 76         ldx $76
$ffaf65 5a            phy
$ffaf66 44 26 a2      mvp $26,$a2
$ffaf69 26 e8         rol $e8
$ffaf6b 48            pha
$ffaf6c 76 48         ror $48,x
$ffaf6e 94 74         sty $74,x
$ffaf70 a2 00         ldx #$00
$ffaf72 c4 da         cpy $da
$ffaf74 74 46         stz $46,x
$ffaf76 b4 44         ldy $44,x
$ffaf78 74 72         stz $72,x
$ffaf7a 08            php
$ffaf7b 44 74 74      mvp $74,$74
$ffaf7e 84 68         sty $68
$ffaf80 76 00         ror $00,x
$ffaf82 74 72         stz $72,x
$ffaf84 74 46         stz $46,x
$ffaf86 b4 b2         ldy $b2,x
$ffaf88 74 72         stz $72,x
$ffaf8a 28            plp
$ffaf8b 44 74 b2      mvp $74,$b2
$ffaf8e 6e 32 74      ror $7432
$ffaf91 00 74         brk $74
$ffaf93 a2 74         ldx #$74
$ffaf95 94 f4         sty $f4,x
$ffaf97 b2 74         lda ($74)
$ffaf99 88            dey
$ffaf9a cc a2 94      cpy $94a2
$ffaf9d 62 4a 72      per $10021ea
$ffafa0 9a            txs
$ffafa1 00 72         brk $72
$ffafa3 a2 72         ldx #$72
$ffafa5 c4 f2         cpy $f2
$ffafa7 22 72 c8 a4   jsr $a4c872
$ffafab c8            iny
$ffafac 84 0c         sty $0c
$ffafae 8a            txa
$ffafaf 72 26         adc ($26)
$ffafb1 00 af         brk $af
$ffafb3 42            ???
$ffafb4 01 e1         ora ($e1,x)
$ffafb6 c9 03         cmp #$03
$ffafb8 b0 20         bcs $ffafda
$ffafba a5 3d         lda $3d
$ffafbc c9 82         cmp #$82
$ffafbe f0 1d         beq $ffafdd
$ffafc0 c9 62         cmp #$62
$ffafc2 f0 19         beq $ffafdd
$ffafc4 a4 41         ldy $41
$ffafc6 a6 40         ldx $40
$ffafc8 d0 01         bne $ffafcb
$ffafca 88            dey
$ffafcb ca            dex
$ffafcc 8a            txa
$ffafcd 18            clc
$ffafce e5 3a         sbc $3a
$ffafd0 85 40         sta $40
$ffafd2 10 01         bpl $ffafd5
$ffafd4 c8            iny
$ffafd5 98            tya
$ffafd6 e5 3b         sbc $3b
$ffafd8 f0 10         beq $ffafea
$ffafda 4c 55 b0      jmp $b055
$ffafdd c2 30         rep #$30
$ffafdf a5 40         lda $40
$ffafe1 3a            dec
$ffafe2 3a            dec
$ffafe3 18            clc
$ffafe4 e5 3a         sbc $3a
$ffafe6 85 40         sta $40
$ffafe8 e2 30         sep #$30
$ffafea 20 99 b2      jsr $b299
$ffafed 20 c8 a9      jsr $a9c8
$ffaff0 20 75 a9      jsr $a975
$ffaff3 20 75 a9      jsr $a975
$ffaff6 20 2c ad      jsr $ad2c
$ffaff9 20 7b ac      jsr $ac7b
$ffaffc 80 66         bra $ffb064
$ffaffe a5 3d         lda $3d
$ffb000 20 9c ac      jsr $ac9c
$ffb003 da            phx
$ffb004 48            pha
$ffb005 a6 2e         ldx $2e
$ffb007 bf 6c ae ff   lda $ffae6c,x
$ffb00b 85 2e         sta $2e
$ffb00d 68            pla
$ffb00e aa            tax
$ffb00f bf 32 af ff   lda $ffaf32,x
$ffb013 c5 42         cmp $42
$ffb015 d0 35         bne $ffb04c
$ffb017 bf b2 ae ff   lda $ffaeb2,x
$ffb01b c5 43         cmp $43
$ffb01d d0 2d         bne $ffb04c
$ffb01f a4 2e         ldy $2e
$ffb021 c0 1f         cpy #$1f
$ffb023 d0 0a         bne $ffb02f
$ffb025 fa            plx
$ffb026 4c b2 af      jmp $afb2
$ffb029 4c 35 b1      jmp $b135
$ffb02c 4c 1b b1      jmp $b11b
$ffb02f fa            plx
$ffb030 a5 2c         lda $2c
$ffb032 c5 2e         cmp $2e
$ffb034 d0 17         bne $ffb04d
$ffb036 af 42 01 e1   lda $e10142
$ffb03a c5 3f         cmp $3f
$ffb03c f0 ac         beq $ffafea
$ffb03e e0 08         cpx #$08
$ffb040 d0 0b         bne $ffb04d
$ffb042 c9 01         cmp #$01
$ffb044 f0 f6         beq $ffb03c
$ffb046 c9 02         cmp #$02
$ffb048 f0 f2         beq $ffb03c
$ffb04a d0 01         bne $ffb04d
$ffb04c fa            plx
$ffb04d c6 3d         dec $3d
$ffb04f d0 ad         bne $ffaffe
$ffb051 c6 35         dec $35
$ffb053 f0 a9         beq $ffaffe
$ffb055 a4 34         ldy $34
$ffb057 98            tya
$ffb058 aa            tax
$ffb059 20 ca a9      jsr $a9ca
$ffb05c a9 de         lda #$de
$ffb05e 20 d1 a9      jsr $a9d1
$ffb061 20 51 9a      jsr $9a51
$ffb064 20 94 a9      jsr $a994
$ffb067 a9 00         lda #$00
$ffb069 85 31         sta $31
$ffb06b a8            tay
$ffb06c ad 00 02      lda $0200
$ffb06f c9 a0         cmp #$a0
$ffb071 f0 b6         beq $ffb029
$ffb073 c9 ba         cmp #$ba
$ffb075 f0 37         beq $ffb0ae
$ffb077 c9 a2         cmp #$a2
$ffb079 f0 61         beq $ffb0dc
$ffb07b c9 8d         cmp #$8d
$ffb07d d0 ad         bne $ffb02c
$ffb07f c2 30         rep #$30
$ffb081 68            pla
$ffb082 8f 30 01 e1   sta $e10130
$ffb086 e2 30         sep #$30
$ffb088 4c 4d a3      jmp $a34d
$ffb08b c2 30         rep #$30
$ffb08d af 30 01 e1   lda $e10130
$ffb091 48            pha
$ffb092 e2 30         sep #$30
$ffb094 a9 a1         lda #$a1
$ffb096 85 33         sta $33
$ffb098 ad 01 02      lda $0201
$ffb09b c9 8d         cmp #$8d
$ffb09d f0 c5         beq $ffb064
$ffb09f a0 00         ldy #$00
$ffb0a1 b9 01 02      lda $0201,y
$ffb0a4 99 00 02      sta $0200,y
$ffb0a7 c8            iny
$ffb0a8 c0 20         cpy #$20
$ffb0aa d0 f5         bne $ffb0a1
$ffb0ac 80 b9         bra $ffb067
$ffb0ae c8            iny
$ffb0af 84 34         sty $34
$ffb0b1 a4 34         ldy $34
$ffb0b3 20 4b b2      jsr $b24b
$ffb0b6 84 34         sty $34
$ffb0b8 20 d2 b2      jsr $b2d2
$ffb0bb d0 f4         bne $ffb0b1
$ffb0bd 80 a5         bra $ffb064
$ffb0bf a9 ba         lda #$ba
$ffb0c1 85 31         sta $31
$ffb0c3 a4 34         ldy $34
$ffb0c5 a5 40         lda $40
$ffb0c7 85 3a         sta $3a
$ffb0c9 a5 41         lda $41
$ffb0cb 85 3b         sta $3b
$ffb0cd 88            dey
$ffb0ce 20 e1 b0      jsr $b0e1
$ffb0d1 c9 8d         cmp #$8d
$ffb0d3 f0 04         beq $ffb0d9
$ffb0d5 c8            iny
$ffb0d6 84 34         sty $34
$ffb0d8 60            rts
$ffb0d9 4c 4d a3      jmp $a34d
$ffb0dc 20 e1 b0      jsr $b0e1
$ffb0df 80 dc         bra $ffb0bd
$ffb0e1 c8            iny
$ffb0e2 b9 00 02      lda $0200,y
$ffb0e5 c9 8d         cmp #$8d
$ffb0e7 f0 31         beq $ffb11a
$ffb0e9 c9 a2         cmp #$a2
$ffb0eb f0 2d         beq $ffb11a
$ffb0ed 5a            phy
$ffb0ee 2f 34 01 e1   and $e10134
$ffb0f2 48            pha
$ffb0f3 a0 00         ldy #$00
$ffb0f5 20 e3 aa      jsr $aae3
$ffb0f8 68            pla
$ffb0f9 20 ec b7      jsr $b7ec
$ffb0fc 64 2f         stz $2f
$ffb0fe a5 3b         lda $3b
$ffb100 49 ff         eor #$ff
$ffb102 d0 10         bne $ffb114
$ffb104 a5 3a         lda $3a
$ffb106 48            pha
$ffb107 20 7b ac      jsr $ac7b
$ffb10a 68            pla
$ffb10b c5 3a         cmp $3a
$ffb10d 90 08         bcc $ffb117
$ffb10f 20 6b b3      jsr $b36b
$ffb112 80 03         bra $ffb117
$ffb114 20 7b ac      jsr $ac7b
$ffb117 7a            ply
$ffb118 80 c7         bra $ffb0e1
$ffb11a 60            rts
$ffb11b 20 72 b2      jsr $b272
$ffb11e c9 93         cmp #$93
$ffb120 d0 21         bne $ffb143
$ffb122 20 62 fe      jsr $fe62
$ffb125 f0 1c         beq $ffb143
$ffb127 b9 00 02      lda $0200,y
$ffb12a c9 a2         cmp #$a2
$ffb12c f0 ae         beq $ffb0dc
$ffb12e c9 ba         cmp #$ba
$ffb130 d0 03         bne $ffb135
$ffb132 4c ae b0      jmp $b0ae
$ffb135 a9 03         lda #$03
$ffb137 85 3d         sta $3d
$ffb139 20 13 ff      jsr $ff13
$ffb13c 0a            asl
$ffb13d e9 be         sbc #$be
$ffb13f c9 c2         cmp #$c2
$ffb141 b0 03         bcs $ffb146
$ffb143 4c 57 b0      jmp $b057
$ffb146 0a            asl
$ffb147 0a            asl
$ffb148 a2 04         ldx #$04
$ffb14a 0a            asl
$ffb14b 26 42         rol $42
$ffb14d 26 43         rol $43
$ffb14f ca            dex
$ffb150 10 f8         bpl $ffb14a
$ffb152 c6 3d         dec $3d
$ffb154 f0 f4         beq $ffb14a
$ffb156 10 e1         bpl $ffb139
$ffb158 a2 07         ldx #$07
$ffb15a 64 3e         stz $3e
$ffb15c 20 13 ff      jsr $ff13
$ffb15f 84 34         sty $34
$ffb161 df 9a ae ff   cmp $ffae9a,x
$ffb165 f0 0a         beq $ffb171
$ffb167 df aa ae ff   cmp $ffaeaa,x
$ffb16b d0 1c         bne $ffb189
$ffb16d a9 40         lda #$40
$ffb16f 85 3e         sta $3e
$ffb171 c9 a4         cmp #$a4
$ffb173 d0 03         bne $ffb178
$ffb175 c8            iny
$ffb176 80 11         bra $ffb189
$ffb178 20 13 ff      jsr $ff13
$ffb17b df a2 ae ff   cmp $ffaea2,x
$ffb17f f0 0a         beq $ffb18b
$ffb181 bf a2 ae ff   lda $ffaea2,x
$ffb185 f0 03         beq $ffb18a
$ffb187 a4 34         ldy $34
$ffb189 18            clc
$ffb18a 88            dey
$ffb18b 26 2c         rol $2c
$ffb18d e0 05         cpx #$05
$ffb18f d0 36         bne $ffb1c7
$ffb191 a5 3e         lda $3e
$ffb193 48            pha
$ffb194 20 4b b2      jsr $b24b
$ffb197 8b            phb
$ffb198 20 82 f8      jsr $f882
$ffb19b 4e 42 01      lsr $0142
$ffb19e ab            plb
$ffb19f 90 03         bcc $ffb1a4
$ffb1a1 20 bd b2      jsr $b2bd
$ffb1a4 68            pla
$ffb1a5 85 3e         sta $3e
$ffb1a7 af 42 01 e1   lda $e10142
$ffb1ab 29 01         and #$01
$ffb1ad f0 04         beq $ffb1b3
$ffb1af a9 01         lda #$01
$ffb1b1 80 02         bra $ffb1b5
$ffb1b3 a9 00         lda #$00
$ffb1b5 8f 30 01 e1   sta $e10130
$ffb1b9 8f 31 01 e1   sta $e10131
$ffb1bd a5 3f         lda $3f
$ffb1bf f0 01         beq $ffb1c2
$ffb1c1 e8            inx
$ffb1c2 86 35         stx $35
$ffb1c4 a2 05         ldx #$05
$ffb1c6 88            dey
$ffb1c7 86 3d         stx $3d
$ffb1c9 ca            dex
$ffb1ca 10 90         bpl $ffb15c
$ffb1cc a5 3e         lda $3e
$ffb1ce 0f 42 01 e1   ora $e10142
$ffb1d2 8f 42 01 e1   sta $e10142
$ffb1d6 84 34         sty $34
$ffb1d8 b9 00 02      lda $0200,y
$ffb1db c9 bb         cmp #$bb
$ffb1dd f0 07         beq $ffb1e6
$ffb1df c9 8d         cmp #$8d
$ffb1e1 f0 03         beq $ffb1e6
$ffb1e3 4c 20 b1      jmp $b120
$ffb1e6 4c fe af      jmp $affe
$ffb1e9 70 08         bvs $ffb1f3
$ffb1eb b0 25         bcs $ffb212
$ffb1ed c9 a0         cmp #$a0
$ffb1ef d0 20         bne $ffb211
$ffb1f1 e2 40         sep #$40
$ffb1f3 b9 00 02      lda $0200,y
$ffb1f6 c9 a7         cmp #$a7
$ffb1f8 f0 14         beq $ffb20e
$ffb1fa c9 8d         cmp #$8d
$ffb1fc d0 03         bne $ffb201
$ffb1fe b8            clv
$ffb1ff f0 5d         beq $ffb25e
$ffb201 a2 07         ldx #$07
$ffb203 20 bd b2      jsr $b2bd
$ffb206 2f 34 01 e1   and $e10134
$ffb20a c8            iny
$ffb20b d0 0b         bne $ffb218
$ffb20d 60            rts
$ffb20e a9 99         lda #$99
$ffb210 c8            iny
$ffb211 60            rts
$ffb212 a2 03         ldx #$03
$ffb214 0a            asl
$ffb215 0a            asl
$ffb216 0a            asl
$ffb217 0a            asl
$ffb218 8b            phb
$ffb219 20 82 f8      jsr $f882
$ffb21c 0a            asl
$ffb21d c2 30         rep #$30
$ffb21f 26 3e         rol $3e
$ffb221 2e 40 01      rol $0140
$ffb224 e2 30         sep #$30
$ffb226 ca            dex
$ffb227 10 f3         bpl $ffb21c
$ffb229 ab            plb
$ffb22a a5 31         lda $31
$ffb22c d0 13         bne $ffb241
$ffb22e a2 01         ldx #$01
$ffb230 b5 3e         lda $3e,x
$ffb232 95 3c         sta $3c,x
$ffb234 95 40         sta $40,x
$ffb236 bf 40 01 e1   lda $e10140,x
$ffb23a 9f 46 01 e1   sta $e10146,x
$ffb23e ca            dex
$ffb23f f0 ef         beq $ffb230
$ffb241 e8            inx
$ffb242 f0 fd         beq $ffb241
$ffb244 20 bd b2      jsr $b2bd
$ffb247 70 aa         bvs $ffb1f3
$ffb249 80 13         bra $ffb25e
$ffb24b b8            clv
$ffb24c c2 30         rep #$30
$ffb24e a9 00 00      lda #$0000
$ffb251 85 3e         sta $3e
$ffb253 8f 40 01 e1   sta $e10140
$ffb257 e2 30         sep #$30
$ffb259 8f 42 01 e1   sta $e10142
$ffb25d aa            tax
$ffb25e 20 ca fc      jsr $fcca
$ffb261 49 b0         eor #$b0
$ffb263 c9 0a         cmp #$0a
$ffb265 90 ab         bcc $ffb212
$ffb267 08            php
$ffb268 69 88         adc #$88
$ffb26a 28            plp
$ffb26b c9 fa         cmp #$fa
$ffb26d 4c e9 b1      jmp $b1e9
$ffb270 a4 34         ldy $34
$ffb272 20 4b b2      jsr $b24b
$ffb275 c9 a8         cmp #$a8
$ffb277 d0 98         bne $ffb211
$ffb279 a5 31         lda $31
$ffb27b d0 0c         bne $ffb289
$ffb27d a5 3e         lda $3e
$ffb27f 8f 3e 01 e1   sta $e1013e
$ffb283 8f a7 01 e1   sta $e101a7
$ffb287 80 e9         bra $ffb272
$ffb289 c9 ae         cmp #$ae
$ffb28b f0 04         beq $ffb291
$ffb28d c9 ba         cmp #$ba
$ffb28f d0 e1         bne $ffb272
$ffb291 a5 3e         lda $3e
$ffb293 8f a7 01 e1   sta $e101a7
$ffb297 80 d9         bra $ffb272
$ffb299 a5 40         lda $40
$ffb29b 85 3e         sta $3e
$ffb29d a5 41         lda $41
$ffb29f 85 3f         sta $3f
$ffb2a1 a4 2f         ldy $2f
$ffb2a3 c0 03         cpy #$03
$ffb2a5 f0 05         beq $ffb2ac
$ffb2a7 b9 3d 00      lda $003d,y
$ffb2aa 80 04         bra $ffb2b0
$ffb2ac af 40 01 e1   lda $e10140
$ffb2b0 48            pha
$ffb2b1 20 e3 aa      jsr $aae3
$ffb2b4 68            pla
$ffb2b5 20 ec b7      jsr $b7ec
$ffb2b8 c6 2f         dec $2f
$ffb2ba 10 e5         bpl $ffb2a1
$ffb2bc 60            rts
$ffb2bd 8b            phb
$ffb2be 20 82 f8      jsr $f882
$ffb2c1 ee 42 01      inc $0142
$ffb2c4 ab            plb
$ffb2c5 60            rts
$ffb2c6 a2 ba         ldx #$ba
$ffb2c8 86 31         stx $31
$ffb2ca a6 40         ldx $40
$ffb2cc 86 3a         stx $3a
$ffb2ce a6 41         ldx $41
$ffb2d0 86 3b         stx $3b
$ffb2d2 eb            xba
$ffb2d3 af 42 01 e1   lda $e10142
$ffb2d7 4a            lsr
$ffb2d8 90 01         bcc $ffb2db
$ffb2da 1a            inc
$ffb2db c9 05         cmp #$05
$ffb2dd 90 02         bcc $ffb2e1
$ffb2df a9 04         lda #$04
$ffb2e1 3a            dec
$ffb2e2 aa            tax
$ffb2e3 85 2f         sta $2f
$ffb2e5 eb            xba
$ffb2e6 c9 c6         cmp #$c6
$ffb2e8 08            php
$ffb2e9 da            phx
$ffb2ea a5 3e         lda $3e
$ffb2ec 85 3d         sta $3d
$ffb2ee a5 3f         lda $3f
$ffb2f0 85 40         sta $40
$ffb2f2 af 40 01 e1   lda $e10140
$ffb2f6 85 41         sta $41
$ffb2f8 af 41 01 e1   lda $e10141
$ffb2fc 8f 40 01 e1   sta $e10140
$ffb300 c2 20         rep #$20
$ffb302 a5 40         lda $40
$ffb304 85 3e         sta $3e
$ffb306 a5 2f         lda $2f
$ffb308 29 ff 00      and #$00ff
$ffb30b 18            clc
$ffb30c 65 3a         adc $3a
$ffb30e b0 23         bcs $ffb333
$ffb310 e2 20         sep #$20
$ffb312 20 99 b2      jsr $b299
$ffb315 68            pla
$ffb316 85 2f         sta $2f
$ffb318 a5 3b         lda $3b
$ffb31a 49 ff         eor #$ff
$ffb31c d0 10         bne $ffb32e
$ffb31e a5 3a         lda $3a
$ffb320 48            pha
$ffb321 20 7b ac      jsr $ac7b
$ffb324 68            pla
$ffb325 c5 3a         cmp $3a
$ffb327 90 08         bcc $ffb331
$ffb329 20 6b b3      jsr $b36b
$ffb32c 80 03         bra $ffb331
$ffb32e 20 7b ac      jsr $ac7b
$ffb331 28            plp
$ffb332 60            rts
$ffb333 e2 20         sep #$20
$ffb335 af 3e 01 e1   lda $e1013e
$ffb339 48            pha
$ffb33a 68            pla
$ffb33b 48            pha
$ffb33c 8f 3e 01 e1   sta $e1013e
$ffb340 a4 2f         ldy $2f
$ffb342 c0 03         cpy #$03
$ffb344 f0 05         beq $ffb34b
$ffb346 b9 3d 00      lda $003d,y
$ffb349 80 04         bra $ffb34f
$ffb34b af 40 01 e1   lda $e10140
$ffb34f 48            pha
$ffb350 20 e3 aa      jsr $aae3
$ffb353 a5 40         lda $40
$ffb355 29 f0         and #$f0
$ffb357 d0 03         bne $ffb35c
$ffb359 20 6b b3      jsr $b36b
$ffb35c 68            pla
$ffb35d 20 ec b7      jsr $b7ec
$ffb360 c6 2f         dec $2f
$ffb362 10 d6         bpl $ffb33a
$ffb364 68            pla
$ffb365 8f 3e 01 e1   sta $e1013e
$ffb369 80 aa         bra $ffb315
$ffb36b af 3e 01 e1   lda $e1013e
$ffb36f 1a            inc
$ffb370 8f 3e 01 e1   sta $e1013e
$ffb374 60            rts
$ffb375 a5 22         lda $22
$ffb377 85 25         sta $25
$ffb379 64 24         stz $24
$ffb37b a4 24         ldy $24
$ffb37d a5 25         lda $25
$ffb37f 48            pha
$ffb380 20 9c cd      jsr $cd9c
$ffb383 20 35 b4      jsr $b435
$ffb386 a0 00         ldy #$00
$ffb388 68            pla
$ffb389 1a            inc
$ffb38a c5 23         cmp $23
$ffb38c 90 f1         bcc $ffb37f
$ffb38e b0 29         bcs $ffb3b9
$ffb390 a5 22         lda $22
$ffb392 48            pha
$ffb393 20 9c cd      jsr $cd9c
$ffb396 a5 28         lda $28
$ffb398 85 2a         sta $2a
$ffb39a a5 29         lda $29
$ffb39c 85 2b         sta $2b
$ffb39e a4 21         ldy $21
$ffb3a0 88            dey
$ffb3a1 68            pla
$ffb3a2 1a            inc
$ffb3a3 c5 23         cmp $23
$ffb3a5 b0 0d         bcs $ffb3b4
$ffb3a7 48            pha
$ffb3a8 20 9c cd      jsr $cd9c
$ffb3ab b1 28         lda ($28),y
$ffb3ad 91 2a         sta ($2a),y
$ffb3af 88            dey
$ffb3b0 10 f9         bpl $ffb3ab
$ffb3b2 30 e2         bmi $ffb396
$ffb3b4 a0 00         ldy #$00
$ffb3b6 20 35 b4      jsr $b435
$ffb3b9 a5 25         lda $25
$ffb3bb 4c 9c cd      jmp $cd9c
$ffb3be a9 28         lda #$28
$ffb3c0 85 21         sta $21
$ffb3c2 a9 18         lda #$18
$ffb3c4 85 23         sta $23
$ffb3c6 3a            dec
$ffb3c7 85 25         sta $25
$ffb3c9 d0 f0         bne $ffb3bb
$ffb3cb a4 2a         ldy $2a
$ffb3cd 80 66         bra $ffb435
$ffb3cf a4 2a         ldy $2a
$ffb3d1 4c 43 cc      jmp $cc43
$ffb3d4 20 37 cc      jsr $cc37
$ffb3d7 ad 7b 05      lda $057b
$ffb3da 85 24         sta $24
$ffb3dc 8d 7b 04      sta $047b
$ffb3df 4c 97 cd      jmp $cd97
$ffb3e2 b4 00         ldy $00,x
$ffb3e4 f0 0f         beq $ffb3f5
$ffb3e6 c0 1b         cpy #$1b
$ffb3e8 f0 0e         beq $ffb3f8
$ffb3ea 20 19 cd      jsr $cd19
$ffb3ed b4 00         ldy $00,x
$ffb3ef f0 04         beq $ffb3f5
$ffb3f1 a9 fd         lda #$fd
$ffb3f3 95 01         sta $01,x
$ffb3f5 b5 01         lda $01,x
$ffb3f7 60            rts
$ffb3f8 a5 37         lda $37
$ffb3fa c9 c3         cmp #$c3
$ffb3fc d0 f3         bne $ffb3f1
$ffb3fe 4c 32 c8      jmp $c832
$ffb401 a4 24         ldy $24
$ffb403 b1 28         lda ($28),y
$ffb405 48            pha
$ffb406 29 3f         and #$3f
$ffb408 09 40         ora #$40
$ffb40a 91 28         sta ($28),y
$ffb40c 68            pla
$ffb40d 60            rts
$ffb40e a8            tay
$ffb40f a5 28         lda $28
$ffb411 4c 64 ca      jmp $ca64
$ffb414 20 ad cd      jsr $cdad
$ffb417 da            phx
$ffb418 a2 03         ldx #$03
$ffb41a df fa c3 ff   cmp $ffc3fa,x
$ffb41e d0 04         bne $ffb424
$ffb420 bf 29 b4 ff   lda $ffb429,x
$ffb424 ca            dex
$ffb425 10 f3         bpl $ffb41a
$ffb427 fa            plx
$ffb428 60            rts
$ffb429 ca            dex
$ffb42a cb            wai
$ffb42b cd c9 20      cmp $20c9
$ffb42e b8            clv
$ffb42f cd 09 80      cmp $8009
$ffb432 60            rts
$ffb433 a4 24         ldy $24
$ffb435 a9 a0         lda #$a0
$ffb437 2c 1e c0      bit $c01e
$ffb43a 10 06         bpl $ffb442
$ffb43c 24 32         bit $32
$ffb43e 30 02         bmi $ffb442
$ffb440 a9 20         lda #$20
$ffb442 4c 4e cc      jmp $cc4e
$ffb445 a8            tay
$ffb446 a5 28         lda $28
$ffb448 4c 9c cd      jmp $cd9c
$ffb44b 2c 1f c0      bit $c01f
$ffb44e 10 03         bpl $ffb453
$ffb450 4c 54 c8      jmp $c854
$ffb453 a4 24         ldy $24
$ffb455 91 28         sta ($28),y
$ffb457 20 c9 ce      jsr $cec9
$ffb45a 20 fc ce      jsr $cefc
$ffb45d 10 fb         bpl $ffb45a
$ffb45f 60            rts
$ffb460 20 be b3      jsr $b3be
$ffb463 2c 1f c0      bit $c01f
$ffb466 10 02         bpl $ffb46a
$ffb468 06 21         asl $21
$ffb46a a5 25         lda $25
$ffb46c 8d fb 05      sta $05fb
$ffb46f 60            rts
$ffb470 a9 ff         lda #$ff
$ffb472 8d fb 04      sta $04fb
$ffb475 ad 58 c0      lda $c058
$ffb478 ad 5a c0      lda $c05a
$ffb47b ad 5d c0      lda $c05d
$ffb47e ad 5f c0      lda $c05f
$ffb481 20 2e 7a      jsr $7a2e
$ffb484 a9 08         lda #$08
$ffb486 0c 35 c0      tsb $c035
$ffb489 60            rts
$ffb48a c8            iny
$ffb48b 08            php
$ffb48c 5a            phy
$ffb48d da            phx
$ffb48e 20 e3 aa      jsr $aae3
$ffb491 20 e9 b7      jsr $b7e9
$ffb494 fa            plx
$ffb495 7a            ply
$ffb496 28            plp
$ffb497 60            rts
$ffb498 a4 24         ldy $24
$ffb49a b1 28         lda ($28),y
$ffb49c 2c 1f c0      bit $c01f
$ffb49f 30 f6         bmi $ffb497
$ffb4a1 4c d2 ce      jmp $ced2
$ffb4a4 af 3e 01 e1   lda $e1013e
$ffb4a8 8f 3f 01 e1   sta $e1013f
$ffb4ac a4 42         ldy $42
$ffb4ae c2 30         rep #$30
$ffb4b0 a5 3c         lda $3c
$ffb4b2 85 42         sta $42
$ffb4b4 c6 3e         dec $3e
$ffb4b6 e2 30         sep #$30
$ffb4b8 98            tya
$ffb4b9 a2 42         ldx #$42
$ffb4bb 20 ec b7      jsr $b7ec
$ffb4be c2 30         rep #$30
$ffb4c0 e6 42         inc $42
$ffb4c2 e2 30         sep #$30
$ffb4c4 ad 68 c0      lda $c068
$ffb4c7 cd 83 c0      cmp $c083
$ffb4ca cd 83 c0      cmp $c083
$ffb4cd 48            pha
$ffb4ce 8b            phb
$ffb4cf 29 f2         and #$f2
$ffb4d1 48            pha
$ffb4d2 af 2f 01 e1   lda $e1012f
$ffb4d6 0a            asl
$ffb4d7 0a            asl
$ffb4d8 03 01         ora $01,sp
$ffb4da 8d 68 c0      sta $c068
$ffb4dd 68            pla
$ffb4de c2 20         rep #$20
$ffb4e0 8b            phb
$ffb4e1 20 82 f8      jsr $f882
$ffb4e4 a2 00 da      ldx #$da00
$ffb4e7 ac 3e 01      ldy $013e
$ffb4ea 5a            phy
$ffb4eb a5 3c         lda $3c
$ffb4ed 48            pha
$ffb4ee da            phx
$ffb4ef ac 3f 01      ldy $013f
$ffb4f2 5a            phy
$ffb4f3 a5 42         lda $42
$ffb4f5 48            pha
$ffb4f6 ad 3e 01      lda $013e
$ffb4f9 29 ff 00      and #$00ff
$ffb4fc 48            pha
$ffb4fd aa            tax
$ffb4fe ec a7 01      cpx $01a7
$ffb501 f0 0b         beq $ffb50e
$ffb503 90 0f         bcc $ffb514
$ffb505 8e a7 01      stx $01a7
$ffb508 a9 01 00      lda #$0001
$ffb50b 38            sec
$ffb50c 80 0c         bra $ffb51a
$ffb50e a5 3e         lda $3e
$ffb510 c5 3c         cmp $3c
$ffb512 90 f4         bcc $ffb508
$ffb514 38            sec
$ffb515 a5 3e         lda $3e
$ffb517 e5 3c         sbc $3c
$ffb519 1a            inc
$ffb51a 85 3e         sta $3e
$ffb51c d0 03         bne $ffb521
$ffb51e ee a7 01      inc $01a7
$ffb521 ad a7 01      lda $01a7
$ffb524 29 ff 00      and #$00ff
$ffb527 e3 01         sbc $01,sp
$ffb529 29 ff 00      and #$00ff
$ffb52c 83 01         sta $01,sp
$ffb52e a5 3e         lda $3e
$ffb530 48            pha
$ffb531 a9 05 08      lda #$0805
$ffb534 48            pha
$ffb535 c2 30         rep #$30
$ffb537 22 61 b5 ff   jsr $ffb561
$ffb53b ab            plb
$ffb53c e2 30         sep #$30
$ffb53e 4c 30 b8      jmp $b830
$ffb541 07 b6         ora [$b6]
$ffb543 18            clc
$ffb544 b6 35         ldx $35,y
$ffb546 b6 b4         ldx $b4,y
$ffb548 b5 56         lda $56,x
$ffb54a b6 b4         ldx $b4,y
$ffb54c b5 73         lda $73,x
$ffb54e b6 b4         ldx $b4,y
$ffb550 b5 a3         lda $a3,x
$ffb552 b6 c4         ldx $c4,y
$ffb554 b6 b4         ldx $b4,y
$ffb556 b5 b4         lda $b4,x
$ffb558 b5 b4         lda $b4,x
$ffb55a b5 b4         lda $b4,x
$ffb55c b5 b4         lda $b4,x
$ffb55e b5 b4         lda $b4,x
$ffb560 b5 48         lda $48,x
$ffb562 f4 00 6b      pea $6b00
$ffb565 f4 54 00      pea $0054
$ffb568 0b            phd
$ffb569 3b            tsc
$ffb56a 5b            tcd
$ffb56b 8b            phb
$ffb56c a5 18         lda $18
$ffb56e 05 14         ora $14
$ffb570 05 10         ora $10
$ffb572 29 00         and #$00
$ffb574 ff d0 3d a5   sbc $a53dd0,x
$ffb578 0e 05 10      asl $1005
$ffb57b f0 34         beq $ffb5b1
$ffb57d a5 0c         lda $0c
$ffb57f c9 05         cmp #$05
$ffb581 08            php
$ffb582 f0 4e         beq $ffb5d2
$ffb584 c9 0a         cmp #$0a
$ffb586 08            php
$ffb587 d0 03         bne $ffb58c
$ffb589 4c f4 b6      jmp $b6f4
$ffb58c 89 f0         bit #$f0
$ffb58e f7 d0         sbc [$d0],y
$ffb590 23 89         and $89,sp
$ffb592 00 08         brk $08
$ffb594 f0 1e         beq $ffb5b4
$ffb596 29 0f         and #$0f
$ffb598 00 0a         brk $0a
$ffb59a aa            tax
$ffb59b a5 0e         lda $0e
$ffb59d 49 ff         eor #$ff
$ffb59f ff a8 a5 10   sbc $10a5a8,x
$ffb5a3 49 ff         eor #$ff
$ffb5a5 ff c8 d0 01   sbc $01d0c8,x
$ffb5a9 1a            inc
$ffb5aa 84 0e         sty $0e
$ffb5ac 85 10         sta $10
$ffb5ae 7c 41 b5      jmp ($b541,x)
$ffb5b1 18            clc
$ffb5b2 90 01         bcc $ffb5b5
$ffb5b4 38            sec
$ffb5b5 c2 30         rep #$30
$ffb5b7 ab            plb
$ffb5b8 a5 09         lda $09
$ffb5ba 85 17         sta $17
$ffb5bc a5 0a         lda $0a
$ffb5be 85 18         sta $18
$ffb5c0 2b            pld
$ffb5c1 3b            tsc
$ffb5c2 08            php
$ffb5c3 18            clc
$ffb5c4 69 14 00      adc #$0014
$ffb5c7 28            plp
$ffb5c8 1b            tcs
$ffb5c9 a9 00 00      lda #$0000
$ffb5cc 90 03         bcc $ffb5d1
$ffb5ce a9 ff ff      lda #$ffff
$ffb5d1 6b            rtl
$ffb5d2 20 29 b7      jsr $b729
$ffb5d5 80 14         bra $ffb5eb
$ffb5d7 85 10         sta $10
$ffb5d9 a5 04         lda $04
$ffb5db e0 01 00      cpx #$0001
$ffb5de b0 03         bcs $ffb5e3
$ffb5e0 69 00 01      adc #$0100
$ffb5e3 c0 00 00      cpy #$0000
$ffb5e6 d0 01         bne $ffb5e9
$ffb5e8 1a            inc
$ffb5e9 85 04         sta $04
$ffb5eb 8a            txa
$ffb5ec 49 ff ff      eor #$ffff
$ffb5ef c5 07         cmp $07
$ffb5f1 b0 02         bcs $ffb5f5
$ffb5f3 85 07         sta $07
$ffb5f5 98            tya
$ffb5f6 49 ff ff      eor #$ffff
$ffb5f9 c5 07         cmp $07
$ffb5fb b0 02         bcs $ffb5ff
$ffb5fd 85 07         sta $07
$ffb5ff 20 41 b7      jsr $b741
$ffb602 10 d3         bpl $ffb5d7
$ffb604 4c b1 b5      jmp $b5b1
$ffb607 a6 10         ldx $10
$ffb609 e2 20         sep #$20
$ffb60b a7 16         lda [$16]
$ffb60d 87 12         sta [$12]
$ffb60f c8            iny
$ffb610 d0 f9         bne $ffb60b
$ffb612 e8            inx
$ffb613 d0 f6         bne $ffb60b
$ffb615 4c b1 b5      jmp $b5b1
$ffb618 bb            tyx
$ffb619 a0 00 00      ldy #$0000
$ffb61c e2 20         sep #$20
$ffb61e b7 16         lda [$16],y
$ffb620 87 12         sta [$12]
$ffb622 c8            iny
$ffb623 d0 02         bne $ffb627
$ffb625 e6 18         inc $18
$ffb627 e8            inx
$ffb628 d0 f4         bne $ffb61e
$ffb62a e6 10         inc $10
$ffb62c d0 f0         bne $ffb61e
$ffb62e e6 11         inc $11
$ffb630 d0 ec         bne $ffb61e
$ffb632 4c b1 b5      jmp $b5b1
$ffb635 bb            tyx
$ffb636 a4 16         ldy $16
$ffb638 64 16         stz $16
$ffb63a e2 20         sep #$20
$ffb63c b7 16         lda [$16],y
$ffb63e 87 12         sta [$12]
$ffb640 88            dey
$ffb641 c0 ff ff      cpy #$ffff
$ffb644 d0 02         bne $ffb648
$ffb646 c6 18         dec $18
$ffb648 e8            inx
$ffb649 d0 f1         bne $ffb63c
$ffb64b e6 10         inc $10
$ffb64d d0 ed         bne $ffb63c
$ffb64f e6 11         inc $11
$ffb651 d0 e9         bne $ffb63c
$ffb653 4c b1 b5      jmp $b5b1
$ffb656 bb            tyx
$ffb657 a0 00 00      ldy #$0000
$ffb65a e2 20         sep #$20
$ffb65c a7 16         lda [$16]
$ffb65e 97 12         sta [$12],y
$ffb660 c8            iny
$ffb661 d0 02         bne $ffb665
$ffb663 e6 14         inc $14
$ffb665 e8            inx
$ffb666 d0 f4         bne $ffb65c
$ffb668 e6 10         inc $10
$ffb66a d0 f0         bne $ffb65c
$ffb66c e6 11         inc $11
$ffb66e d0 ec         bne $ffb65c
$ffb670 4c b1 b5      jmp $b5b1
$ffb673 a4 16         ldy $16
$ffb675 64 16         stz $16
$ffb677 a2 00 00      ldx #$0000
$ffb67a e2 20         sep #$20
$ffb67c b7 16         lda [$16],y
$ffb67e 5a            phy
$ffb67f 9b            txy
$ffb680 97 12         sta [$12],y
$ffb682 7a            ply
$ffb683 88            dey
$ffb684 c0 ff ff      cpy #$ffff
$ffb687 d0 02         bne $ffb68b
$ffb689 c6 18         dec $18
$ffb68b e8            inx
$ffb68c d0 02         bne $ffb690
$ffb68e e6 14         inc $14
$ffb690 e6 0e         inc $0e
$ffb692 d0 e8         bne $ffb67c
$ffb694 e6 0f         inc $0f
$ffb696 d0 e4         bne $ffb67c
$ffb698 e6 10         inc $10
$ffb69a d0 e0         bne $ffb67c
$ffb69c e6 11         inc $11
$ffb69e d0 dc         bne $ffb67c
$ffb6a0 4c b1 b5      jmp $b5b1
$ffb6a3 bb            tyx
$ffb6a4 a4 12         ldy $12
$ffb6a6 64 12         stz $12
$ffb6a8 e2 20         sep #$20
$ffb6aa a7 16         lda [$16]
$ffb6ac 97 12         sta [$12],y
$ffb6ae 88            dey
$ffb6af c0 ff ff      cpy #$ffff
$ffb6b2 d0 02         bne $ffb6b6
$ffb6b4 c6 14         dec $14
$ffb6b6 e8            inx
$ffb6b7 d0 f1         bne $ffb6aa
$ffb6b9 e6 10         inc $10
$ffb6bb d0 ed         bne $ffb6aa
$ffb6bd e6 11         inc $11
$ffb6bf d0 e9         bne $ffb6aa
$ffb6c1 4c b1 b5      jmp $b5b1
$ffb6c4 a0 00 00      ldy #$0000
$ffb6c7 a6 12         ldx $12
$ffb6c9 64 12         stz $12
$ffb6cb e2 20         sep #$20
$ffb6cd b7 16         lda [$16],y
$ffb6cf 5a            phy
$ffb6d0 9b            txy
$ffb6d1 97 12         sta [$12],y
$ffb6d3 7a            ply
$ffb6d4 c8            iny
$ffb6d5 d0 02         bne $ffb6d9
$ffb6d7 e6 18         inc $18
$ffb6d9 ca            dex
$ffb6da e0 ff ff      cpx #$ffff
$ffb6dd d0 02         bne $ffb6e1
$ffb6df c6 14         dec $14
$ffb6e1 e6 0e         inc $0e
$ffb6e3 d0 e8         bne $ffb6cd
$ffb6e5 e6 0f         inc $0f
$ffb6e7 d0 e4         bne $ffb6cd
$ffb6e9 e6 10         inc $10
$ffb6eb d0 e0         bne $ffb6cd
$ffb6ed e6 11         inc $11
$ffb6ef d0 dc         bne $ffb6cd
$ffb6f1 4c b1 b5      jmp $b5b1
$ffb6f4 a9 44 00      lda #$0044
$ffb6f7 85 03         sta $03
$ffb6f9 20 29 b7      jsr $b729
$ffb6fc 80 15         bra $ffb713
$ffb6fe 85 10         sta $10
$ffb700 a5 04         lda $04
$ffb702 e0 ff ff      cpx #$ffff
$ffb705 d0 04         bne $ffb70b
$ffb707 38            sec
$ffb708 e9 00 01      sbc #$0100
$ffb70b c0 ff ff      cpy #$ffff
$ffb70e d0 01         bne $ffb711
$ffb710 3a            dec
$ffb711 85 04         sta $04
$ffb713 8a            txa
$ffb714 c5 07         cmp $07
$ffb716 b0 02         bcs $ffb71a
$ffb718 85 07         sta $07
$ffb71a 98            tya
$ffb71b c5 07         cmp $07
$ffb71d b0 02         bcs $ffb721
$ffb71f 85 07         sta $07
$ffb721 20 41 b7      jsr $b741
$ffb724 10 d8         bpl $ffb6fe
$ffb726 4c b1 b5      jmp $b5b1
$ffb729 a5 18         lda $18
$ffb72b eb            xba
$ffb72c 05 14         ora $14
$ffb72e 85 04         sta $04
$ffb730 a5 0e         lda $0e
$ffb732 d0 02         bne $ffb736
$ffb734 c6 10         dec $10
$ffb736 c6 0e         dec $0e
$ffb738 a6 16         ldx $16
$ffb73a a4 12         ldy $12
$ffb73c a5 0e         lda $0e
$ffb73e 85 07         sta $07
$ffb740 60            rts
$ffb741 4b            phk
$ffb742 f4 51 b7      pea $b751
$ffb745 f4 00 00      pea $0000
$ffb748 ab            plb
$ffb749 3b            tsc
$ffb74a 18            clc
$ffb74b 69 09 00      adc #$0009
$ffb74e 48            pha
$ffb74f a5 07         lda $07
$ffb751 6b            rtl
$ffb752 a5 0e         lda $0e
$ffb754 18            clc
$ffb755 e5 07         sbc $07
$ffb757 85 0e         sta $0e
$ffb759 85 07         sta $07
$ffb75b a5 10         lda $10
$ffb75d e9 00 00      sbc #$0000
$ffb760 60            rts
$ffb761 af a7 01 e1   lda $e101a7
$ffb765 cf 3e 01 e1   cmp $e1013e
$ffb769 f0 0c         beq $ffb777
$ffb76b 90 0a         bcc $ffb777
$ffb76d a5 3f         lda $3f
$ffb76f 48            pha
$ffb770 48            pha
$ffb771 a9 ff 85      lda #$85ff
$ffb774 3f 85 3e 20   and $203e85,x
$ffb778 e7 b7         sbc [$b7]
$ffb77a 48            pha
$ffb77b a2 42 20      ldx #$2042
$ffb77e e9 b7 85      sbc #$85b7
$ffb781 40            rti
$ffb782 68            pla
$ffb783 c5 40         cmp $40
$ffb785 f0 1a         beq $ffb7a1
$ffb787 48            pha
$ffb788 20 7a a9      jsr $a97a
$ffb78b 68            pla
$ffb78c 20 a3 a9      jsr $a9a3
$ffb78f 20 91 ac      jsr $ac91
$ffb792 a9 a8 20      lda #$20a8
$ffb795 d1 a9         cmp ($a9),y
$ffb797 a5 40         lda $40
$ffb799 20 a3 a9      jsr $a9a3
$ffb79c a9 a9 20      lda #$20a9
$ffb79f d1 a9         cmp ($a9),y
$ffb7a1 20 b4 fc      jsr $fcb4
$ffb7a4 08            php
$ffb7a5 a5 43         lda $43
$ffb7a7 05 42         ora $42
$ffb7a9 d0 09         bne $ffb7b4
$ffb7ab af 3f 01 e1   lda $e1013f
$ffb7af 1a            inc
$ffb7b0 8f 3f 01 e1   sta $e1013f
$ffb7b4 28            plp
$ffb7b5 90 c0         bcc $ffb777
$ffb7b7 af 3e 01 e1   lda $e1013e
$ffb7bb 1a            inc
$ffb7bc 48            pha
$ffb7bd f0 26         beq $ffb7e5
$ffb7bf cf a7 01 e1   cmp $e101a7
$ffb7c3 f0 13         beq $ffb7d8
$ffb7c5 b0 1e         bcs $ffb7e5
$ffb7c7 68            pla
$ffb7c8 8f 3e 01 e1   sta $e1013e
$ffb7cc 64 3d         stz $3d
$ffb7ce 64 3c         stz $3c
$ffb7d0 a9 ff 85      lda #$85ff
$ffb7d3 3f 85 3e 80   and $803e85,x
$ffb7d7 9f 68 8f 3e   sta $3e8f68,x
$ffb7db 01 e1         ora ($e1,x)
$ffb7dd 68            pla
$ffb7de 85 3e         sta $3e
$ffb7e0 68            pla
$ffb7e1 85 3f         sta $3f
$ffb7e3 80 92         bra $ffb777
$ffb7e5 68            pla
$ffb7e6 60            rts
$ffb7e7 a2 3c b8      ldx #$b83c
$ffb7ea 50 02         bvc $ffb7ee
$ffb7ec e2 40         sep #$40
$ffb7ee eb            xba
$ffb7ef ad 68 c0      lda $c068
$ffb7f2 cd 83 c0      cmp $c083
$ffb7f5 cd 83 c0      cmp $c083
$ffb7f8 48            pha
$ffb7f9 8b            phb
$ffb7fa 29 f2 48      and #$48f2
$ffb7fd af 2f 01 e1   lda $e1012f
$ffb801 0a            asl
$ffb802 0a            asl
$ffb803 03 01         ora $01,sp
$ffb805 83 01         sta $01,sp
$ffb807 af 8f 01 e1   lda $e1018f
$ffb80b f0 06         beq $ffb813
$ffb80d af 2d 01 e1   lda $e1012d
$ffb811 83 01         sta $01,sp
$ffb813 68            pla
$ffb814 8d 68 c0      sta $c068
$ffb817 e0 42 90      cpx #$9042
$ffb81a 06 af         asl $af
$ffb81c 3f 01 e1 b0   and $b0e101,x
$ffb820 04 af         tsb $af
$ffb822 3e 01 e1      rol $e101,x
$ffb825 48            pha
$ffb826 ab            plb
$ffb827 eb            xba
$ffb828 50 04         bvc $ffb82e
$ffb82a 81 00         sta ($00,x)
$ffb82c 80 02         bra $ffb830
$ffb82e a1 00         lda ($00,x)
$ffb830 eb            xba
$ffb831 ab            plb
$ffb832 68            pla
$ffb833 29 bf 2c      and #$2cbf
$ffb836 1c c0 10      trb $10c0
$ffb839 02 09         cop $09
$ffb83b 40            rti
$ffb83c 8d 68 c0      sta $c068
$ffb83f eb            xba
$ffb840 60            rts
$ffb841 ad 00 02      lda $0200
$ffb844 f0 1d         beq $ffb863
$ffb846 af 3e 01 e1   lda $e1013e
$ffb84a 8f 8d 01 e1   sta $e1018d
$ffb84e 8b            phb
$ffb84f 68            pla
$ffb850 8f 3e 01 e1   sta $e1013e
$ffb854 9c 00 02      stz $0200
$ffb857 a9 ba 85      lda #$85ba
$ffb85a 31 a9         and ($a9),y
$ffb85c 02 85         cop $85
$ffb85e 41 3a         eor ($3a,x)
$ffb860 85 40         sta $40
$ffb862 60            rts
$ffb863 af 42 01 e1   lda $e10142
$ffb867 f0 03         beq $ffb86c
$ffb869 20 c6 b2      jsr $b2c6
$ffb86c a5 3a         lda $3a
$ffb86e 3a            dec
$ffb86f 8d 00 02      sta $0200
$ffb872 af 8d 01 e1   lda $e1018d
$ffb876 8f 3e 01 e1   sta $e1013e
$ffb87a 60            rts
$ffb87b c2 20         rep #$20
$ffb87d a5 3e         lda $3e
$ffb87f 48            pha
$ffb880 e2 20         sep #$20
$ffb882 af a7 01 e1   lda $e101a7
$ffb886 cf 3e 01 e1   cmp $e1013e
$ffb88a f0 08         beq $ffb894
$ffb88c 90 06         bcc $ffb894
$ffb88e a9 ff 85      lda #$85ff
$ffb891 3e 85 3f      rol $3f85,x
$ffb894 a5 3c         lda $3c
$ffb896 48            pha
$ffb897 a5 3d         lda $3d
$ffb899 48            pha
$ffb89a a0 01 20      ldy #$2001
$ffb89d e7 b7         sbc [$b7]
$ffb89f d9 00 02      cmp $0200,y
$ffb8a2 d0 29         bne $ffb8cd
$ffb8a4 20 ba fc      jsr $fcba
$ffb8a7 b0 35         bcs $ffb8de
$ffb8a9 c8            iny
$ffb8aa cc 00 02      cpy $0200
$ffb8ad 90 ed         bcc $ffb89c
$ffb8af f0 eb         beq $ffb89c
$ffb8b1 68            pla
$ffb8b2 68            pla
$ffb8b3 a5 3d         lda $3d
$ffb8b5 48            pha
$ffb8b6 a5 3c         lda $3c
$ffb8b8 48            pha
$ffb8b9 ed 00 02      sbc $0200
$ffb8bc 85 3c         sta $3c
$ffb8be b0 02         bcs $ffb8c2
$ffb8c0 c6 3d         dec $3d
$ffb8c2 20 7a a9      jsr $a97a
$ffb8c5 68            pla
$ffb8c6 85 3c         sta $3c
$ffb8c8 68            pla
$ffb8c9 85 3d         sta $3d
$ffb8cb 80 c7         bra $ffb894
$ffb8cd 68            pla
$ffb8ce 85 3d         sta $3d
$ffb8d0 68            pla
$ffb8d1 85 3c         sta $3c
$ffb8d3 20 ba fc      jsr $fcba
$ffb8d6 90 f3         bcc $ffb8cb
$ffb8d8 a5 3c         lda $3c
$ffb8da 48            pha
$ffb8db a5 3d         lda $3d
$ffb8dd 48            pha
$ffb8de 8b            phb
$ffb8df 20 82 f8      jsr $f882
$ffb8e2 ee 3e 01      inc $013e
$ffb8e5 af 3e 01 e1   lda $e1013e
$ffb8e9 f0 20         beq $ffb90b
$ffb8eb cf a7 01 e1   cmp $e101a7
$ffb8ef f0 0f         beq $ffb900
$ffb8f1 b0 18         bcs $ffb90b
$ffb8f3 ab            plb
$ffb8f4 64 3c         stz $3c
$ffb8f6 64 3d         stz $3d
$ffb8f8 a9 ff 85      lda #$85ff
$ffb8fb 3f 85 3e 80   and $803e85,x
$ffb8ff 9c ab a3      stz $a3ab
$ffb902 04 85         tsb $85
$ffb904 3f a3 03 85   and $8503a3,x
$ffb908 3e 80 91      rol $9180,x
$ffb90b ce 3e 01      dec $013e
$ffb90e ab            plb
$ffb90f c2 20         rep #$20
$ffb911 68            pla
$ffb912 68            pla
$ffb913 e2 20         sep #$20
$ffb915 60            rts
$ffb916 a9 37 a0      lda #$a037
$ffb919 55 20         eor $20,x
$ffb91b 9e ba a9      stz $a9ba,x
$ffb91e 31 a0         and ($a0),y
$ffb920 00 20         brk $20
$ffb922 9e ba 6b      stz $6bba,x
$ffb925 08            php
$ffb926 78            sei
$ffb927 8b            phb
$ffb928 20 82 f8      jsr $f882
$ffb92b a2 00 bf      ldx #$bf00
$ffb92e c5 83         cmp $83
$ffb930 ff 9d c0 02   sbc $02c09d,x
$ffb934 e8            inx
$ffb935 e0 5a 90      cpx #$905a
$ffb938 f4 a9 ff      pea $ffa9
$ffb93b e0 a1 d0      cpx #$d0a1
$ffb93e 07 ac         ora [$ac]
$ffb940 46 c0         lsr $c0
$ffb942 10 02         bpl $ffb946
$ffb944 a2 c1 9d      ldx #$9dc1
$ffb947 c0 02 e8      cpy #$e802
$ffb94a d0 ef         bne $ffb93b
$ffb94c ab            plb
$ffb94d 28            plp
$ffb94e 08            php
$ffb94f 78            sei
$ffb950 8b            phb
$ffb951 20 82 f8      jsr $f882
$ffb954 20 57 ba      jsr $ba57
$ffb957 8e bc 03      stx $03bc
$ffb95a 8d be 03      sta $03be
$ffb95d e2 30         sep #$30
$ffb95f a2 00         ldx #$00
$ffb961 9b            txy
$ffb962 b8            clv
$ffb963 bd c0 02      lda $02c0,x
$ffb966 20 6f ba      jsr $ba6f
$ffb969 e8            inx
$ffb96a c8            iny
$ffb96b d0 f5         bne $ffb962
$ffb96d ab            plb
$ffb96e 28            plp
$ffb96f 6b            rtl
$ffb970 8b            phb
$ffb971 08            php
$ffb972 78            sei
$ffb973 20 82 f8      jsr $f882
$ffb976 a2 00         ldx #$00
$ffb978 9b            txy
$ffb979 e2 40         sep #$40
$ffb97b 20 6f ba      jsr $ba6f
$ffb97e 9d c0 02      sta $02c0,x
$ffb981 e0 29         cpx #$29
$ffb983 f0 1c         beq $ffb9a1
$ffb985 e0 2a         cpx #$2a
$ffb987 f0 1c         beq $ffb9a5
$ffb989 e0 36         cpx #$36
$ffb98b f0 2c         beq $ffb9b9
$ffb98d e0 37         cpx #$37
$ffb98f f0 28         beq $ffb9b9
$ffb991 e0 3e         cpx #$3e
$ffb993 b0 06         bcs $ffb99b
$ffb995 df 71 83 ff   cmp $ff8371,x
$ffb999 80 1c         bra $ffb9b7
$ffb99b d0 0c         bne $ffb9a9
$ffb99d c9 09         cmp #$09
$ffb99f 80 16         bra $ffb9b7
$ffb9a1 c9 08         cmp #$08
$ffb9a3 80 12         bra $ffb9b7
$ffb9a5 c9 11         cmp #$11
$ffb9a7 80 0e         bra $ffb9b7
$ffb9a9 e0 59         cpx #$59
$ffb9ab b0 0c         bcs $ffb9b9
$ffb9ad e0 58         cpx #$58
$ffb9af 90 04         bcc $ffb9b5
$ffb9b1 c9 04         cmp #$04
$ffb9b3 80 02         bra $ffb9b7
$ffb9b5 c9 20         cmp #$20
$ffb9b7 b0 17         bcs $ffb9d0
$ffb9b9 e8            inx
$ffb9ba c8            iny
$ffb9bb d0 bc         bne $ffb979
$ffb9bd 20 57 ba      jsr $ba57
$ffb9c0 ec bc 03      cpx $03bc
$ffb9c3 d0 0b         bne $ffb9d0
$ffb9c5 cd be 03      cmp $03be
$ffb9c8 d0 06         bne $ffb9d0
$ffb9ca 28            plp
$ffb9cb 18            clc
$ffb9cc e2 30         sep #$30
$ffb9ce ab            plb
$ffb9cf 6b            rtl
$ffb9d0 e2 30         sep #$30
$ffb9d2 22 25 b9 ff   jsr $ffb925
$ffb9d6 28            plp
$ffb9d7 38            sec
$ffb9d8 ab            plb
$ffb9d9 6b            rtl
$ffb9da 08            php
$ffb9db 78            sei
$ffb9dc 8b            phb
$ffb9dd 20 82 f8      jsr $f882
$ffb9e0 9c e0 03      stz $03e0
$ffb9e3 a0 00         ldy #$00
$ffb9e5 ee e0 03      inc $03e0
$ffb9e8 f0 2c         beq $ffba16
$ffb9ea a2 00         ldx #$00
$ffb9ec a9 fd         lda #$fd
$ffb9ee 18            clc
$ffb9ef 69 04         adc #$04
$ffb9f1 48            pha
$ffb9f2 09 80         ora #$80
$ffb9f4 e2 40         sep #$40
$ffb9f6 20 9f ba      jsr $ba9f
$ffb9f9 c0 00         cpy #$00
$ffb9fb d0 05         bne $ffba02
$ffb9fd 9d e1 03      sta $03e1,x
$ffba00 80 08         bra $ffba0a
$ffba02 dd e1 03      cmp $03e1,x
$ffba05 f0 03         beq $ffba0a
$ffba07 68            pla
$ffba08 80 d9         bra $ffb9e3
$ffba0a 68            pla
$ffba0b e8            inx
$ffba0c e0 04         cpx #$04
$ffba0e 90 de         bcc $ffb9ee
$ffba10 bb            tyx
$ffba11 d0 07         bne $ffba1a
$ffba13 c8            iny
$ffba14 80 cf         bra $ffb9e5
$ffba16 ab            plb
$ffba17 28            plp
$ffba18 38            sec
$ffba19 6b            rtl
$ffba1a ab            plb
$ffba1b 28            plp
$ffba1c 18            clc
$ffba1d 6b            rtl
$ffba1e 08            php
$ffba1f 78            sei
$ffba20 a9 00         lda #$00
$ffba22 48            pha
$ffba23 a2 00         ldx #$00
$ffba25 a9 fd         lda #$fd
$ffba27 18            clc
$ffba28 69 04         adc #$04
$ffba2a 48            pha
$ffba2b bf e5 03 e1   lda $e103e5,x
$ffba2f a8            tay
$ffba30 a3 01         lda $01,sp
$ffba32 20 9e ba      jsr $ba9e
$ffba35 68            pla
$ffba36 e8            inx
$ffba37 e0 04         cpx #$04
$ffba39 90 ec         bcc $ffba27
$ffba3b 22 da b9 ff   jsr $ffb9da
$ffba3f a2 03         ldx #$03
$ffba41 bf e1 03 e1   lda $e103e1,x
$ffba45 df e5 03 e1   cmp $e103e5,x
$ffba49 d0 06         bne $ffba51
$ffba4b ca            dex
$ffba4c 10 f3         bpl $ffba41
$ffba4e 68            pla
$ffba4f 80 ca         bra $ffba1b
$ffba51 68            pla
$ffba52 3a            dec
$ffba53 d0 cd         bne $ffba22
$ffba55 80 c0         bra $ffba17
$ffba57 a2 fa         ldx #$fa
$ffba59 c2 20         rep #$20
$ffba5b 18            clc
$ffba5c a9 00 00      lda #$0000
$ffba5f 2a            rol
$ffba60 7d c0 02      adc $02c0,x
$ffba63 ca            dex
$ffba64 e0 ff d0      cpx #$d0ff
$ffba67 f7 c2         sbc [$c2],y
$ffba69 30 aa         bmi $ffba15
$ffba6b 49 aa aa      eor #$aaaa
$ffba6e 60            rts
$ffba6f 48            pha
$ffba70 98            tya
$ffba71 48            pha
$ffba72 29 e0 4a      and #$4ae0
$ffba75 4a            lsr
$ffba76 4a            lsr
$ffba77 4a            lsr
$ffba78 4a            lsr
$ffba79 09 38 50      ora #$5038
$ffba7c 02 09         cop $09
$ffba7e 80 eb         bra $ffba6b
$ffba80 68            pla
$ffba81 29 1f 0a      and #$0a1f
$ffba84 0a            asl
$ffba85 eb            xba
$ffba86 08            php
$ffba87 20 a3 ba      jsr $baa3
$ffba8a eb            xba
$ffba8b 20 a3 ba      jsr $baa3
$ffba8e 28            plp
$ffba8f 68            pla
$ffba90 20 a4 ba      jsr $baa4
$ffba93 48            pha
$ffba94 ad 34 c0      lda $c034
$ffba97 29 df 8d      and #$8ddf
$ffba9a 34 c0         bit $c0,x
$ffba9c 68            pla
$ffba9d 60            rts
$ffba9e b8            clv
$ffba9f 5a            phy
$ffbaa0 08            php
$ffbaa1 80 e8         bra $ffba8b
$ffbaa3 b8            clv
$ffbaa4 8d 33 c0      sta $c033
$ffbaa7 ad 34 c0      lda $c034
$ffbaaa 29 3f 70      and #$703f
$ffbaad 04 09         tsb $09
$ffbaaf a0 80 02      ldy #$0280
$ffbab2 09 e0 8d      ora #$8de0
$ffbab5 34 c0         bit $c0,x
$ffbab7 ad 34 c0      lda $c034
$ffbaba 30 fb         bmi $ffbab7
$ffbabc ad 33 c0      lda $c033
$ffbabf 60            rts
$ffbac0 af 35 01 e1   lda $e10135
$ffbac4 48            pha
$ffbac5 20 cf ba      jsr $bacf
$ffbac8 68            pla
$ffbac9 8f 35 01 e1   sta $e10135
$ffbacd 18            clc
$ffbace 6b            rtl
$ffbacf a9 0b 8d      lda #$8d0b
$ffbad2 39 c0 a9      and $a9c0,y
$ffbad5 d2 8d         cmp ($8d)
$ffbad7 39 c0 20      and $20c0,y
$ffbada 3b            tsc
$ffbadb 9b            txy
$ffbadc 22 70 b9 ff   jsr $ffb970
$ffbae0 8b            phb
$ffbae1 20 82 f8      jsr $f882
$ffbae4 9c c0 01      stz $01c0
$ffbae7 9c c1 01      stz $01c1
$ffbaea 9c c2 01      stz $01c2
$ffbaed ad e2 02      lda $02e2
$ffbaf0 c9 02 f0      cmp #$f002
$ffbaf3 08            php
$ffbaf4 ad 18 03      lda $0318
$ffbaf7 c9 02 d0      cmp #$d002
$ffbafa 0a            asl
$ffbafb 3a            dec
$ffbafc 8d c0 01      sta $01c0
$ffbaff 8d c1 01      sta $01c1
$ffbb02 8d c2 01      sta $01c2
$ffbb05 a2 07 bd      ldx #$bd07
$ffbb08 4c 00 48      jmp $4800
$ffbb0b ca            dex
$ffbb0c 10 f9         bpl $ffbb07
$ffbb0e 20 62 7f      jsr $7f62
$ffbb11 20 62 7f      jsr $7f62
$ffbb14 a2 00 68      ldx #$6800
$ffbb17 9d 4c 00      sta $004c,x
$ffbb1a e8            inx
$ffbb1b e0 08 d0      cpx #$d008
$ffbb1e f7 ab         sbc [$ab],y
$ffbb20 af a6 01 e1   lda $e101a6
$ffbb24 d0 27         bne $ffbb4d
$ffbb26 a9 ff 8f      lda #$8fff
$ffbb29 a6 01         ldx $01
$ffbb2b e1 a9         sbc ($a9,x)
$ffbb2d 00 a2         brk $a2
$ffbb2f 08            php
$ffbb30 ca            dex
$ffbb31 f0 1a         beq $ffbb4d
$ffbb33 9d 78 04      sta $0478,x
$ffbb36 9d f8 04      sta $04f8,x
$ffbb39 9d 78 05      sta $0578,x
$ffbb3c 9d f8 05      sta $05f8,x
$ffbb3f 9d 78 06      sta $0678,x
$ffbb42 9d f8 06      sta $06f8,x
$ffbb45 9d 78 07      sta $0778,x
$ffbb48 9d f8 07      sta $07f8,x
$ffbb4b 80 e3         bra $ffbb30
$ffbb4d 20 71 bb      jsr $bb71
$ffbb50 d0 09         bne $ffbb5b
$ffbb52 af 39 01 e1   lda $e10139
$ffbb56 20 ca bb      jsr $bbca
$ffbb59 38            sec
$ffbb5a 90 18         bcc $ffbb74
$ffbb5c 22 7a bb ff   jsr $ffbb7a
$ffbb60 20 71 bb      jsr $bb71
$ffbb63 d0 0b         bne $ffbb70
$ffbb65 c2 30         rep #$30
$ffbb67 a2 01 05      ldx #$0501
$ffbb6a 22 00 00 e1   jsr $e10000
$ffbb6e e2 30         sep #$30
$ffbb70 60            rts
$ffbb71 ad f3 03      lda $03f3
$ffbb74 49 a5         eor #$a5
$ffbb76 cd f4 03      cmp $03f4
$ffbb79 60            rts
$ffbb7a 08            php
$ffbb7b 78            sei
$ffbb7c ad 68 c0      lda $c068
$ffbb7f 48            pha
$ffbb80 29 fe         and #$fe
$ffbb82 09 08         ora #$08
$ffbb84 8d 68 c0      sta $c068
$ffbb87 08            php
$ffbb88 20 d7 bb      jsr $bbd7
$ffbb8b 28            plp
$ffbb8c b0 03         bcs $ffbb91
$ffbb8e 20 9b bb      jsr $bb9b
$ffbb91 20 e2 f9      jsr $f9e2
$ffbb94 68            pla
$ffbb95 8d 68 c0      sta $c068
$ffbb98 28            plp
$ffbb99 18            clc
$ffbb9a 6b            rtl
$ffbb9b a2 07         ldx #$07
$ffbb9d a9 00         lda #$00
$ffbb9f 1f e0 02 e1   ora $e102e0,x
$ffbba3 0a            asl
$ffbba4 ca            dex
$ffbba5 d0 f8         bne $ffbb9f
$ffbba7 29 f0         and #$f0
$ffbba9 aa            tax
$ffbbaa af e2 02 e1   lda $e102e2
$ffbbae c9 01         cmp #$01
$ffbbb0 d0 04         bne $ffbbb6
$ffbbb2 8a            txa
$ffbbb3 09 04         ora #$04
$ffbbb5 aa            tax
$ffbbb6 af 18 03 e1   lda $e10318
$ffbbba c9 01         cmp #$01
$ffbbbc d0 04         bne $ffbbc2
$ffbbbe 8a            txa
$ffbbbf 09 02         ora #$02
$ffbbc1 aa            tax
$ffbbc2 8a            txa
$ffbbc3 8d 2d c0      sta $c02d
$ffbbc6 8f 39 01 e1   sta $e10139
$ffbbca 8d 0a c0      sta $c00a
$ffbbcd af e3 02 e1   lda $e102e3
$ffbbd1 f0 03         beq $ffbbd6
$ffbbd3 8d 0b c0      sta $c00b
$ffbbd6 60            rts
$ffbbd7 22 70 b9 ff   jsr $ffb970
$ffbbdb 8b            phb
$ffbbdc 20 82 f8      jsr $f882
$ffbbdf 9c 8f 01      stz $018f
$ffbbe2 18            clc
$ffbbe3 ad e0 02      lda $02e0
$ffbbe6 6a            ror
$ffbbe7 6a            ror
$ffbbe8 8d 38 01      sta $0138
$ffbbeb 20 0e bc      jsr $bc0e
$ffbbee ad 3c c0      lda $c03c
$ffbbf1 29 f0         and #$f0
$ffbbf3 0d de 02      ora $02de
$ffbbf6 8d 3c c0      sta $c03c
$ffbbf9 ad de 02      lda $02de
$ffbbfc 8d ca 00      sta $00ca
$ffbbff 0a            asl
$ffbc00 0a            asl
$ffbc01 0a            asl
$ffbc02 0a            asl
$ffbc03 8d b0 1d      sta $1db0
$ffbc06 20 ea c3      jsr $c3ea
$ffbc09 20 83 80      jsr $8083
$ffbc0c ab            plb
$ffbc0d 60            rts
$ffbc0e 8b            phb
$ffbc0f 20 82 f8      jsr $f882
$ffbc12 ad da 02      lda $02da
$ffbc15 0a            asl
$ffbc16 0a            asl
$ffbc17 0a            asl
$ffbc18 0a            asl
$ffbc19 0d db 02      ora $02db
$ffbc1c 8d 22 c0      sta $c022
$ffbc1f ad 34 c0      lda $c034
$ffbc22 29 f0         and #$f0
$ffbc24 0d dc 02      ora $02dc
$ffbc27 8d 34 c0      sta $c034
$ffbc2a a9 f0         lda #$f0
$ffbc2c 1c 2b c0      trb $c02b
$ffbc2f ad e9 02      lda $02e9
$ffbc32 0a            asl
$ffbc33 0d dd 02      ora $02dd
$ffbc36 0a            asl
$ffbc37 0a            asl
$ffbc38 0a            asl
$ffbc39 0a            asl
$ffbc3a 09 08         ora #$08
$ffbc3c 0c 2b c0      tsb $c02b
$ffbc3f a9 20         lda #$20
$ffbc41 1c 29 c0      trb $c029
$ffbc44 18            clc
$ffbc45 ad d8 02      lda $02d8
$ffbc48 6a            ror
$ffbc49 6a            ror
$ffbc4a 8d 21 c0      sta $c021
$ffbc4d 6a            ror
$ffbc4e 6a            ror
$ffbc4f 0c 29 c0      tsb $c029
$ffbc52 ab            plb
$ffbc53 60            rts
$ffbc54 90 04         bcc $ffbc5a
$ffbc56 5c 18 00 e1   jmp $e10018
$ffbc5a 5c 14 00 e1   jmp $e10014
$ffbc5e 18            clc
$ffbc5f fb            xce
$ffbc60 b0 04         bcs $ffbc66
$ffbc62 e2 40         sep #$40
$ffbc64 80 06         bra $ffbc6c
$ffbc66 fb            xce
$ffbc67 b8            clv
$ffbc68 68            pla
$ffbc69 09 10         ora #$10
$ffbc6b 48            pha
$ffbc6c 18            clc
$ffbc6d fb            xce
$ffbc6e c2 30         rep #$30
$ffbc70 08            php
$ffbc71 8b            phb
$ffbc72 f4 e1 e1      pea $e1e1
$ffbc75 ab            plb
$ffbc76 8d 08 01      sta $0108
$ffbc79 ad 35 c0      lda $c035
$ffbc7c 8d 19 01      sta $0119
$ffbc7f 09 00 80      ora #$8000
$ffbc82 29 3e 9f      and #$9f3e
$ffbc85 8d 35 c0      sta $c035
$ffbc88 8e 0a 01      stx $010a
$ffbc8b 8c 0c 01      sty $010c
$ffbc8e 7b            tdc
$ffbc8f 8d 10 01      sta $0110
$ffbc92 a9 00 00      lda #$0000
$ffbc95 5b            tcd
$ffbc96 e2 30         sep #$30
$ffbc98 90 06         bcc $ffbca0
$ffbc9a a3 04         lda $04,sp
$ffbc9c 29 10         and #$10
$ffbc9e 69 70         adc #$70
$ffbca0 70 74         bvs $ffbd16
$ffbca2 a9 03         lda #$03
$ffbca4 8d 39 c0      sta $c039
$ffbca7 ad 39 c0      lda $c039
$ffbcaa 2c 03 01      bit $0103
$ffbcad f0 37         beq $ffbce6
$ffbcaf 48            pha
$ffbcb0 29 07         and #$07
$ffbcb2 d0 12         bne $ffbcc6
$ffbcb4 ad 3b c0      lda $c03b
$ffbcb7 8d 05 01      sta $0105
$ffbcba ad 3b c0      lda $c03b
$ffbcbd 8d 06 01      sta $0106
$ffbcc0 22 20 00 e1   jsr $e10020
$ffbcc4 80 10         bra $ffbcd6
$ffbcc6 ad 3a c0      lda $c03a
$ffbcc9 8d 05 01      sta $0105
$ffbccc ad 3a c0      lda $c03a
$ffbccf 8d 06 01      sta $0106
$ffbcd2 22 20 00 e1   jsr $e10020
$ffbcd6 af 01 01 01   lda $010101
$ffbcda 8d 1c 01      sta $011c
$ffbcdd a9 00         lda #$00
$ffbcdf 6a            ror
$ffbce0 8d 01 01      sta $0101
$ffbce3 68            pla
$ffbce4 80 13         bra $ffbcf9
$ffbce6 48            pha
$ffbce7 af 01 01 01   lda $010101
$ffbceb 8d 1c 01      sta $011c
$ffbcee 9c 01 01      stz $0101
$ffbcf1 22 1c 02 e1   jsr $e1021c
$ffbcf5 68            pla
$ffbcf6 90 14         bcc $ffbd0c
$ffbcf8 18            clc
$ffbcf9 48            pha
$ffbcfa 2d 04 01      and $0104
$ffbcfd f0 13         beq $ffbd12
$ffbcff 22 24 00 e1   jsr $e10024
$ffbd03 6e 01 01      ror $0101
$ffbd06 68            pla
$ffbd07 ad 01 01      lda $0101
$ffbd0a d0 09         bne $ffbd15
$ffbd0c c2 30         rep #$30
$ffbd0e ab            plb
$ffbd0f 4c 58 bf      jmp $bf58
$ffbd12 68            pla
$ffbd13 d0 f2         bne $ffbd07
$ffbd15 b8            clv
$ffbd16 af 01 01 01   lda $010101
$ffbd1a 8d 1c 01      sta $011c
$ffbd1d c2 30         rep #$30
$ffbd1f ab            plb
$ffbd20 68            pla
$ffbd21 8d 13 01      sta $0113
$ffbd24 9c c3 00      stz $00c3
$ffbd27 9c c5 00      stz $00c5
$ffbd2a 50 1e         bvc $ffbd4a
$ffbd2c e2 10         sep #$10
$ffbd2e 7a            ply
$ffbd2f 5a            phy
$ffbd30 8c 12 01      sty $0112
$ffbd33 84 48         sty $48
$ffbd35 a3 02         lda $02,sp
$ffbd37 8d 16 01      sta $0116
$ffbd3a 85 3a         sta $3a
$ffbd3c a0 00 ad      ldy #$ad00
$ffbd3f 14 01         trb $01
$ffbd41 4a            lsr
$ffbd42 b0 03         bcs $ffbd47
$ffbd44 a3 04         lda $04,sp
$ffbd46 a8            tay
$ffbd47 8c 15 01      sty $0115
$ffbd4a 3b            tsc
$ffbd4b 8d 0e 01      sta $010e
$ffbd4e e2 30         sep #$30
$ffbd50 eb            xba
$ffbd51 49 01         eor #$01
$ffbd53 d0 0a         bne $ffbd5f
$ffbd55 ae 16 c0      ldx $c016
$ffbd58 10 0d         bpl $ffbd67
$ffbd5a eb            xba
$ffbd5b 8f 01 01 01   sta $010101
$ffbd5f a9 01         lda #$01
$ffbd61 eb            xba
$ffbd62 af 00 01 01   lda $010100
$ffbd66 1b            tcs
$ffbd67 ad 68 c0      lda $c068
$ffbd6a 8d 18 01      sta $0118
$ffbd6d ae 18 c0      ldx $c018
$ffbd70 10 02         bpl $ffbd74
$ffbd72 29 bf         and #$bf
$ffbd74 29 40         and #$40
$ffbd76 09 08         ora #$08
$ffbd78 8d 68 c0      sta $c068
$ffbd7b af f8 07 00   lda $0007f8
$ffbd7f 8d 1b 01      sta $011b
$ffbd82 50 06         bvc $ffbd8a
$ffbd84 9c c6 00      stz $00c6
$ffbd87 4c 99 bf      jmp $bf99
$ffbd8a ee cb 00      inc $00cb
$ffbd8d ad 01 01      lda $0101
$ffbd90 f0 0a         beq $ffbd9c
$ffbd92 8d c6 00      sta $00c6
$ffbd95 a9 00         lda #$00
$ffbd97 48            pha
$ffbd98 ab            plb
$ffbd99 4c 8f bf      jmp $bf8f
$ffbd9c a9 00         lda #$00
$ffbd9e 48            pha
$ffbd9f ab            plb
$ffbda0 ad 23 c0      lda $c023
$ffbda3 10 0f         bpl $ffbdb4
$ffbda5 29 22         and #$22
$ffbda7 4a            lsr
$ffbda8 4a            lsr
$ffbda9 90 09         bcc $ffbdb4
$ffbdab f0 07         beq $ffbdb4
$ffbdad 22 28 00 e1   jsr $e10028
$ffbdb1 4c fd be      jmp $befd
$ffbdb4 ad 3c c0      lda $c03c
$ffbdb7 29 f0         and #$f0
$ffbdb9 48            pha
$ffbdba c2 30         rep #$30
$ffbdbc ad 3e c0      lda $c03e
$ffbdbf 48            pha
$ffbdc0 e2 30         sep #$30
$ffbdc2 a9 e0         lda #$e0
$ffbdc4 8d 3e c0      sta $c03e
$ffbdc7 ad 3c c0      lda $c03c
$ffbdca 29 90         and #$90
$ffbdcc 0f ca 00 e1   ora $e100ca
$ffbdd0 8d 3c c0      sta $c03c
$ffbdd3 ad 3d c0      lda $c03d
$ffbdd6 ad 3d c0      lda $c03d
$ffbdd9 8f cc 00 e1   sta $e100cc
$ffbddd 30 17         bmi $ffbdf6
$ffbddf 22 2c 00 e1   jsr $e1002c
$ffbde3 c2 30         rep #$30
$ffbde5 68            pla
$ffbde6 8d 3e c0      sta $c03e
$ffbde9 e2 30         sep #$30
$ffbdeb 68            pla
$ffbdec 0f ca 00 e1   ora $e100ca
$ffbdf0 8d 3c c0      sta $c03c
$ffbdf3 4c fd be      jmp $befd
$ffbdf6 c2 30         rep #$30
$ffbdf8 68            pla
$ffbdf9 8d 3e c0      sta $c03e
$ffbdfc e2 30         sep #$30
$ffbdfe 68            pla
$ffbdff 0f ca 00 e1   ora $e100ca
$ffbe03 8d 3c c0      sta $c03c
$ffbe06 ad 46 c0      lda $c046
$ffbe09 2d 41 c0      and $c041
$ffbe0c 8f c3 00 e1   sta $e100c3
$ffbe10 29 18         and #$18
$ffbe12 48            pha
$ffbe13 29 08         and #$08
$ffbe15 f0 1a         beq $ffbe31
$ffbe17 22 30 00 e1   jsr $e10030
$ffbe1b af d7 00 e1   lda $e100d7
$ffbe1f d0 0c         bne $ffbe2d
$ffbe21 90 0e         bcc $ffbe31
$ffbe23 a9 08         lda #$08
$ffbe25 1c 41 c0      trb $c041
$ffbe28 8d 47 c0      sta $c047
$ffbe2b 80 04         bra $ffbe31
$ffbe2d 38            sec
$ffbe2e 20 83 bf      jsr $bf83
$ffbe31 ad 27 c0      lda $c027
$ffbe34 8f c5 00 e1   sta $e100c5
$ffbe38 0a            asl
$ffbe39 10 0c         bpl $ffbe47
$ffbe3b 90 0a         bcc $ffbe47
$ffbe3d 22 34 00 e1   jsr $e10034
$ffbe41 b8            clv
$ffbe42 20 83 bf      jsr $bf83
$ffbe45 80 02         bra $ffbe49
$ffbe47 e2 40         sep #$40
$ffbe49 68            pla
$ffbe4a d0 04         bne $ffbe50
$ffbe4c 50 75         bvc $ffbec3
$ffbe4e 70 0a         bvs $ffbe5a
$ffbe50 29 10         and #$10
$ffbe52 f0 6f         beq $ffbec3
$ffbe54 22 38 00 e1   jsr $e10038
$ffbe58 80 66         bra $ffbec0
$ffbe5a af c5 00 e1   lda $e100c5
$ffbe5e 29 30         and #$30
$ffbe60 0a            asl
$ffbe61 0a            asl
$ffbe62 0a            asl
$ffbe63 10 61         bpl $ffbec6
$ffbe65 90 5f         bcc $ffbec6
$ffbe67 ad 26 c0      lda $c026
$ffbe6a 8f c4 00 e1   sta $e100c4
$ffbe6e 10 07         bpl $ffbe77
$ffbe70 22 40 00 e1   jsr $e10040
$ffbe74 20 83 bf      jsr $bf83
$ffbe77 af c4 00 e1   lda $e100c4
$ffbe7b 29 08         and #$08
$ffbe7d f0 07         beq $ffbe86
$ffbe7f 22 44 00 e1   jsr $e10044
$ffbe83 20 83 bf      jsr $bf83
$ffbe86 af c4 00 e1   lda $e100c4
$ffbe8a 29 20         and #$20
$ffbe8c f0 13         beq $ffbea1
$ffbe8e ad 25 c0      lda $c025
$ffbe91 6a            ror
$ffbe92 90 06         bcc $ffbe9a
$ffbe94 22 08 00 fe   jsr $fe0008
$ffbe98 80 04         bra $ffbe9e
$ffbe9a 22 48 00 e1   jsr $e10048
$ffbe9e 20 83 bf      jsr $bf83
$ffbea1 af c4 00 e1   lda $e100c4
$ffbea5 29 50         and #$50
$ffbea7 0a            asl
$ffbea8 0a            asl
$ffbea9 f0 09         beq $ffbeb4
$ffbeab 90 03         bcc $ffbeb0
$ffbead 8d 10 c0      sta $c010
$ffbeb0 22 4c 00 e1   jsr $e1004c
$ffbeb4 af c4 00 e1   lda $e100c4
$ffbeb8 c9 40         cmp #$40
$ffbeba d0 07         bne $ffbec3
$ffbebc 22 50 00 e1   jsr $e10050
$ffbec0 20 83 bf      jsr $bf83
$ffbec3 4c 8f bf      jmp $bf8f
$ffbec6 af c5 00 e1   lda $e100c5
$ffbeca a8            tay
$ffbecb 29 0c         and #$0c
$ffbecd 4a            lsr
$ffbece 4a            lsr
$ffbecf 4a            lsr
$ffbed0 90 08         bcc $ffbeda
$ffbed2 f0 06         beq $ffbeda
$ffbed4 22 3c 00 e1   jsr $e1003c
$ffbed8 80 23         bra $ffbefd
$ffbeda ad 23 c0      lda $c023
$ffbedd 10 0e         bpl $ffbeed
$ffbedf 29 44         and #$44
$ffbee1 0a            asl
$ffbee2 0a            asl
$ffbee3 f0 08         beq $ffbeed
$ffbee5 90 06         bcc $ffbeed
$ffbee7 22 54 00 e1   jsr $e10054
$ffbeeb 80 10         bra $ffbefd
$ffbeed ad 23 c0      lda $c023
$ffbef0 10 10         bpl $ffbf02
$ffbef2 29 11         and #$11
$ffbef4 4a            lsr
$ffbef5 90 0b         bcc $ffbf02
$ffbef7 f0 09         beq $ffbf02
$ffbef9 22 58 00 e1   jsr $e10058
$ffbefd 90 39         bcc $ffbf38
$ffbeff 4c 98 bf      jmp $bf98
$ffbf02 22 a9 01 e1   jsr $e101a9
$ffbf06 22 5c 00 e1   jsr $e1005c
$ffbf0a 80 f1         bra $ffbefd
$ffbf0c 68            pla
$ffbf0d 18            clc
$ffbf0e fb            xce
$ffbf0f 22 ad 01 e1   jsr $e101ad
$ffbf13 af c6 00 e1   lda $e100c6
$ffbf17 0a            asl
$ffbf18 8f c6 00 e1   sta $e100c6
$ffbf1c d0 7a         bne $ffbf98
$ffbf1e a9 80         lda #$80
$ffbf20 0c 36 c0      tsb $c036
$ffbf23 af 1b 01 e1   lda $e1011b
$ffbf27 8d f8 07      sta $07f8
$ffbf2a c2 10         rep #$10
$ffbf2c eb            xba
$ffbf2d a9 00 aa      lda #$aa00
$ffbf30 2c ff cf      bit $cfff
$ffbf33 bd 00 00      lda $0000,x
$ffbf36 e2 10         sep #$10
$ffbf38 a9 e1         lda #$e1
$ffbf3a 48            pha
$ffbf3b ab            plb
$ffbf3c ad 8b c0      lda $c08b
$ffbf3f ad 8b c0      lda $c08b
$ffbf42 ad 18 01      lda $0118
$ffbf45 8d 68 c0      sta $c068
$ffbf48 ce cb 00      dec $00cb
$ffbf4b c2 30         rep #$30
$ffbf4d 9c c3 00      stz $00c3
$ffbf50 ad 0e 01      lda $010e
$ffbf53 1b            tcs
$ffbf54 ad 13 01      lda $0113
$ffbf57 48            pha
$ffbf58 ad 10 01      lda $0110
$ffbf5b 5b            tcd
$ffbf5c ac 0c 01      ldy $010c
$ffbf5f ae 0a 01      ldx $010a
$ffbf62 ad 19 01      lda $0119
$ffbf65 8d 35 c0      sta $c035
$ffbf68 e2 20         sep #$20
$ffbf6a ad 1c 01      lda $011c
$ffbf6d 8f 01 01 01   sta $010101
$ffbf71 20 2e 7a      jsr $7a2e
$ffbf74 c2 20         rep #$20
$ffbf76 ad 08 01      lda $0108
$ffbf79 ab            plb
$ffbf7a 28            plp
$ffbf7b b0 01         bcs $ffbf7e
$ffbf7d 40            rti
$ffbf7e fb            xce
$ffbf7f 5c 72 c0 00   jmp $00c072
$ffbf83 90 09         bcc $ffbf8e
$ffbf85 af c6 00 e1   lda $e100c6
$ffbf89 6a            ror
$ffbf8a 8f c6 00 e1   sta $e100c6
$ffbf8e 60            rts
$ffbf8f af c6 00 e1   lda $e100c6
$ffbf93 d0 03         bne $ffbf98
$ffbf95 4c 0d bf      jmp $bf0d
$ffbf98 b8            clv
$ffbf99 a9 00 48      lda #$4800
$ffbf9c ab            plb
$ffbf9d ad 68 c0      lda $c068
$ffbfa0 ae 18 c0      ldx $c018
$ffbfa3 10 02         bpl $ffbfa7
$ffbfa5 29 bf 29      and #$29bf
$ffbfa8 40            rti
$ffbfa9 09 08 8d      ora #$8d08
$ffbfac 68            pla
$ffbfad c0 a9 80      cpy #$80a9
$ffbfb0 1c 36 c0      trb $c036
$ffbfb3 50 08         bvc $ffbfbd
$ffbfb5 22 70 00 e1   jsr $e10070
$ffbfb9 90 d4         bcc $ffbf8f
$ffbfbb e2 40         sep #$40
$ffbfbd c2 20         rep #$20
$ffbfbf 3b            tsc
$ffbfc0 a2 6b da      ldx #$da6b
$ffbfc3 4b            phk
$ffbfc4 f4 0b bf      pea $bf0b
$ffbfc7 48            pha
$ffbfc8 38            sec
$ffbfc9 fb            xce
$ffbfca e2 20         sep #$20
$ffbfcc a9 04 48      lda #$4804
$ffbfcf 70 04         bvs $ffbfd5
$ffbfd1 5c 3d fd 00   jmp $00fd3d
$ffbfd5 af 08 01 e1   lda $e10108
$ffbfd9 85 45         sta $45
$ffbfdb af 0a 01 e1   lda $e1010a
$ffbfdf 85 46         sta $46
$ffbfe1 af 0c 01 e1   lda $e1010c
$ffbfe5 85 47         sta $47
$ffbfe7 af 0e 01 e1   lda $e1010e
$ffbfeb 85 49         sta $49
$ffbfed 38            sec
$ffbfee fb            xce
$ffbfef 5c 56 fa 00   jmp $00fa56
$ffbff3 00 00         brk $00
$ffbff5 00 00         brk $00
$ffbff7 00 00         brk $00
$ffbff9 00 00         brk $00
$ffbffb 00 00         brk $00
$ffbffd 00 00         brk $00
$ffbfff 00 00         brk $00
$ffc001 00 00         brk $00
$ffc003 00 00         brk $00
$ffc005 00 00         brk $00
$ffc007 00 00         brk $00
$ffc009 00 00         brk $00
$ffc00b 00 00         brk $00
$ffc00d 00 00         brk $00
$ffc00f 00 00         brk $00
$ffc011 00 00         brk $00
$ffc013 00 00         brk $00
$ffc015 00 00         brk $00
$ffc017 00 00         brk $00
$ffc019 00 00         brk $00
$ffc01b 00 00         brk $00
$ffc01d 00 00         brk $00
$ffc01f 00 00         brk $00
$ffc021 00 00         brk $00
$ffc023 00 00         brk $00
$ffc025 00 00         brk $00
$ffc027 00 00         brk $00
$ffc029 00 00         brk $00
$ffc02b 00 00         brk $00
$ffc02d 00 00         brk $00
$ffc02f 00 00         brk $00
$ffc031 00 00         brk $00
$ffc033 00 00         brk $00
$ffc035 00 00         brk $00
$ffc037 00 00         brk $00
$ffc039 00 00         brk $00
$ffc03b 00 00         brk $00
$ffc03d 00 00         brk $00
$ffc03f 00 00         brk $00
$ffc041 00 00         brk $00
$ffc043 00 00         brk $00
$ffc045 00 00         brk $00
$ffc047 00 00         brk $00
$ffc049 00 00         brk $00
$ffc04b 00 00         brk $00
$ffc04d 00 00         brk $00
$ffc04f 00 00         brk $00
$ffc051 00 00         brk $00
$ffc053 00 00         brk $00
$ffc055 00 00         brk $00
$ffc057 00 00         brk $00
$ffc059 00 00         brk $00
$ffc05b 00 00         brk $00
$ffc05d 00 00         brk $00
$ffc05f 00 00         brk $00
$ffc061 00 00         brk $00
$ffc063 00 00         brk $00
$ffc065 00 00         brk $00
$ffc067 00 00         brk $00
$ffc069 00 00         brk $00
$ffc06b 00 00         brk $00
$ffc06d 00 4c         brk $4c
$ffc06f 36 9d         rol $9d,x
$ffc071 e2 40         sep #$40
$ffc073 50 b8         bvc $ffc02d
$ffc075 5c 10 00 e1   jmp $e10010
$ffc079 38            sec
$ffc07a 90 18         bcc $ffc094
$ffc07c 5c 54 bc ff   jmp $ffbc54
$ffc080 c2 30         rep #$30
$ffc082 29 ff 00      and #$00ff
$ffc085 a4 06         ldy $06
$ffc087 5a            phy
$ffc088 a4 08         ldy $08
$ffc08a 5a            phy
$ffc08b 0a            asl
$ffc08c a8            tay
$ffc08d af c0 00 e1   lda $e100c0
$ffc091 85 06         sta $06
$ffc093 af c1 00 e1   lda $e100c1
$ffc097 85 07         sta $07
$ffc099 b7 06         lda [$06],y
$ffc09b 85 06         sta $06
$ffc09d e2 70         sep #$70
$ffc09f a7 06         lda [$06]
$ffc0a1 f0 18         beq $ffc0bb
$ffc0a3 c9 20         cmp #$20
$ffc0a5 90 2a         bcc $ffc0d1
$ffc0a7 b8            clv
$ffc0a8 a7 06         lda [$06]
$ffc0aa f0 1a         beq $ffc0c6
$ffc0ac c9 20         cmp #$20
$ffc0ae 90 21         bcc $ffc0d1
$ffc0b0 70 04         bvs $ffc0b6
$ffc0b2 30 0f         bmi $ffc0c3
$ffc0b4 09 80         ora #$80
$ffc0b6 08            php
$ffc0b7 20 d1 a9      jsr $a9d1
$ffc0ba 28            plp
$ffc0bb e6 06         inc $06
$ffc0bd d0 e9         bne $ffc0a8
$ffc0bf e6 07         inc $07
$ffc0c1 80 e5         bra $ffc0a8
$ffc0c3 20 d1 a9      jsr $a9d1
$ffc0c6 c2 30         rep #$30
$ffc0c8 68            pla
$ffc0c9 85 08         sta $08
$ffc0cb 68            pla
$ffc0cc 85 06         sta $06
$ffc0ce e2 30         sep #$30
$ffc0d0 60            rts
$ffc0d1 a8            tay
$ffc0d2 e6 06         inc $06
$ffc0d4 d0 02         bne $ffc0d8
$ffc0d6 e6 07         inc $07
$ffc0d8 a7 06         lda [$06]
$ffc0da 5a            phy
$ffc0db 70 02         bvs $ffc0df
$ffc0dd 09 80         ora #$80
$ffc0df 08            php
$ffc0e0 20 d1 a9      jsr $a9d1
$ffc0e3 28            plp
$ffc0e4 7a            ply
$ffc0e5 88            dey
$ffc0e6 d0 f0         bne $ffc0d8
$ffc0e8 80 d1         bra $ffc0bb
$ffc0ea c1 d8         cmp ($d8,x)
$ffc0ec d9 d3 c4      cmp $c4d3,y
$ffc0ef d0 c2         bne $ffc0b3
$ffc0f1 cb            wai
$ffc0f2 cd d1 cc      cmp $ccd1
$ffc0f5 ed f8 e5      sbc $e5f8
$ffc0f8 e4 c6         cpx $c6
$ffc0fa d6 d4         dec $d4,x
$ffc0fc 8d 00 00      sta $0000
$ffc0ff 00 e2         brk $e2
$ffc101 40            rti
$ffc102 70 27         bvs $ffc12b
$ffc104 ea            nop
$ffc105 38            sec
$ffc106 90 18         bcc $ffc120
$ffc108 b8            clv
$ffc109 50 20         bvc $ffc12b
$ffc10b 01 31         ora ($31,x)
$ffc10d 47 48         eor [$48]
$ffc10f 49 4a         eor #$4a
$ffc111 00 14         brk $14
$ffc113 00 48         brk $48
$ffc115 48            pha
$ffc116 20 56 c1      jsr $c156
$ffc119 08            php
$ffc11a 78            sei
$ffc11b eb            xba
$ffc11c a9 c1         lda #$c1
$ffc11e 8d f8 07      sta $07f8
$ffc121 eb            xba
$ffc122 22 f5 0e ff   jsr $ff0ef5
$ffc126 2a            rol
$ffc127 28            plp
$ffc128 4a            lsr
$ffc129 80 0d         bra $ffc138
$ffc12b 48            pha
$ffc12c 48            pha
$ffc12d 20 56 c1      jsr $c156
$ffc130 da            phx
$ffc131 a2 c1         ldx #$c1
$ffc133 22 d5 0e ff   jsr $ff0ed5
$ffc137 fa            plx
$ffc138 eb            xba
$ffc139 68            pla
$ffc13a 8d 36 c0      sta $c036
$ffc13d 68            pla
$ffc13e 8d 68 c0      sta $c068
$ffc141 eb            xba
$ffc142 08            php
$ffc143 38            sec
$ffc144 fb            xce
$ffc145 28            plp
$ffc146 60            rts
$ffc147 c8            iny
$ffc148 c8            iny
$ffc149 c8            iny
$ffc14a c8            iny
$ffc14b 48            pha
$ffc14c 48            pha
$ffc14d 20 56 c1      jsr $c156
$ffc150 22 e5 0e ff   jsr $ff0ee5
$ffc154 80 e2         bra $ffc138
$ffc156 08            php
$ffc157 18            clc
$ffc158 fb            xce
$ffc159 48            pha
$ffc15a ad 36 c0      lda $c036
$ffc15d 83 05         sta $05,sp
$ffc15f 09 80         ora #$80
$ffc161 8d 36 c0      sta $c036
$ffc164 ad 68 c0      lda $c068
$ffc167 83 06         sta $06,sp
$ffc169 29 8f         and #$8f
$ffc16b 8d 68 c0      sta $c068
$ffc16e a9 00         lda #$00
$ffc170 8f 96 15 e1   sta $e11596
$ffc174 68            pla
$ffc175 28            plp
$ffc176 e2 30         sep #$30
$ffc178 60            rts
$ffc179 4d 53 41      eor $4153
$ffc17c bf de 05 ff   lda $ff05de,x
$ffc180 a8            tay
$ffc181 bf d5 14 e1   lda $e114d5,x
$ffc185 10 05         bpl $ffc18c
$ffc187 22 26 10 e1   jsr $e11026
$ffc18b 60            rts
$ffc18c b9 fd bf      lda $bffd,y
$ffc18f 60            rts
$ffc190 00 00         brk $00
$ffc192 00 00         brk $00
$ffc194 00 00         brk $00
$ffc196 22 18 10 e1   jsr $e11018
$ffc19a 60            rts
$ffc19b 22 1c 10 e1   jsr $e1101c
$ffc19f 60            rts
$ffc1a0 fb            xce
$ffc1a1 d4 85         pei ($85)
$ffc1a3 ab            plb
$ffc1a4 ab            plb
$ffc1a5 dc 84 00      jmp [$0084]
$ffc1a8 18            clc
$ffc1a9 fb            xce
$ffc1aa 6b            rtl
$ffc1ab a2 ff         ldx #$ff
$ffc1ad 6b            rtl
$ffc1ae 18            clc
$ffc1af fb            xce
$ffc1b0 22 22 10 e1   jsr $e11022
$ffc1b4 08            php
$ffc1b5 38            sec
$ffc1b6 fb            xce
$ffc1b7 28            plp
$ffc1b8 50 0a         bvc $ffc1c4
$ffc1ba af 21 10 e1   lda $e11021
$ffc1be 48            pha
$ffc1bf af 20 10 e1   lda $e11020
$ffc1c3 48            pha
$ffc1c4 60            rts
$ffc1c5 00 00         brk $00
$ffc1c7 00 00         brk $00
$ffc1c9 00 00         brk $00
$ffc1cb 00 00         brk $00
$ffc1cd 00 00         brk $00
$ffc1cf 00 00         brk $00
$ffc1d1 00 00         brk $00
$ffc1d3 00 00         brk $00
$ffc1d5 00 00         brk $00
$ffc1d7 00 5c         brk $5c
$ffc1d9 00 00         brk $00
$ffc1db ff 5c 03 00   sbc $00035c,x
$ffc1df ff 18 fb c2   sbc $c2fb18,x
$ffc1e3 30 a2         bmi $ffc187
$ffc1e5 00 11         brk $11
$ffc1e7 a0 e1         ldy #$e1
$ffc1e9 00 a9         brk $a9
$ffc1eb 00 06         brk $06
$ffc1ed 8f 00 11 e1   sta $e11100
$ffc1f1 5c 14 10 e1   jmp $e11014
$ffc1f5 00 00         brk $00
$ffc1f7 00 00         brk $00
$ffc1f9 00 00         brk $00
$ffc1fb 00 00         brk $00
$ffc1fd 00 20         brk $20
$ffc1ff 10 e2         bpl $ffc1e3
$ffc201 40            rti
$ffc202 70 27         bvs $ffc22b
$ffc204 ea            nop
$ffc205 38            sec
$ffc206 90 18         bcc $ffc220
$ffc208 b8            clv
$ffc209 50 20         bvc $ffc22b
$ffc20b 01 31         ora ($31,x)
$ffc20d 47 48         eor [$48]
$ffc20f 49 4a         eor #$4a
$ffc211 00 14         brk $14
$ffc213 00 48         brk $48
$ffc215 48            pha
$ffc216 20 56 c2      jsr $c256
$ffc219 08            php
$ffc21a 78            sei
$ffc21b eb            xba
$ffc21c a9 c2         lda #$c2
$ffc21e 8d f8 07      sta $07f8
$ffc221 eb            xba
$ffc222 22 f5 0e ff   jsr $ff0ef5
$ffc226 2a            rol
$ffc227 28            plp
$ffc228 4a            lsr
$ffc229 80 0d         bra $ffc238
$ffc22b 48            pha
$ffc22c 48            pha
$ffc22d 20 56 c2      jsr $c256
$ffc230 da            phx
$ffc231 a2 c2         ldx #$c2
$ffc233 22 d5 0e ff   jsr $ff0ed5
$ffc237 fa            plx
$ffc238 eb            xba
$ffc239 68            pla
$ffc23a 8d 36 c0      sta $c036
$ffc23d 68            pla
$ffc23e 8d 68 c0      sta $c068
$ffc241 eb            xba
$ffc242 08            php
$ffc243 38            sec
$ffc244 fb            xce
$ffc245 28            plp
$ffc246 60            rts
$ffc247 c8            iny
$ffc248 c8            iny
$ffc249 c8            iny
$ffc24a c8            iny
$ffc24b 48            pha
$ffc24c 48            pha
$ffc24d 20 56 c2      jsr $c256
$ffc250 22 e5 0e ff   jsr $ff0ee5
$ffc254 80 e2         bra $ffc238
$ffc256 08            php
$ffc257 18            clc
$ffc258 fb            xce
$ffc259 48            pha
$ffc25a ad 36 c0      lda $c036
$ffc25d 83 05         sta $05,sp
$ffc25f 09 80         ora #$80
$ffc261 8d 36 c0      sta $c036
$ffc264 ad 68 c0      lda $c068
$ffc267 83 06         sta $06,sp
$ffc269 29 8f         and #$8f
$ffc26b 8d 68 c0      sta $c068
$ffc26e a9 00         lda #$00
$ffc270 8f 97 15 e1   sta $e11597
$ffc274 68            pla
$ffc275 28            plp
$ffc276 e2 30         sep #$30
$ffc278 60            rts
$ffc279 4d 53 41      eor $4153
$ffc27c da            phx
$ffc27d 20 30 0d      jsr $0d30
$ffc280 fa            plx
$ffc281 bd b8 05      lda $05b8,x
$ffc284 89 40         bit #$40
$ffc286 f0 03         beq $ffc28b
$ffc288 20 c6 09      jsr $09c6
$ffc28b 60            rts
$ffc28c 00 00         brk $00
$ffc28e 00 00         brk $00
$ffc290 00 00         brk $00
$ffc292 00 00         brk $00
$ffc294 00 00         brk $00
$ffc296 22 18 10 e1   jsr $e11018
$ffc29a 60            rts
$ffc29b 22 1c 10 e1   jsr $e1101c
$ffc29f 60            rts
$ffc2a0 fb            xce
$ffc2a1 d4 85         pei ($85)
$ffc2a3 ab            plb
$ffc2a4 ab            plb
$ffc2a5 dc 84 00      jmp [$0084]
$ffc2a8 18            clc
$ffc2a9 fb            xce
$ffc2aa 6b            rtl
$ffc2ab a2 ff         ldx #$ff
$ffc2ad 6b            rtl
$ffc2ae 18            clc
$ffc2af fb            xce
$ffc2b0 22 22 10 e1   jsr $e11022
$ffc2b4 08            php
$ffc2b5 38            sec
$ffc2b6 fb            xce
$ffc2b7 28            plp
$ffc2b8 50 0a         bvc $ffc2c4
$ffc2ba af 21 10 e1   lda $e11021
$ffc2be 48            pha
$ffc2bf af 20 10 e1   lda $e11020
$ffc2c3 48            pha
$ffc2c4 60            rts
$ffc2c5 00 00         brk $00
$ffc2c7 00 00         brk $00
$ffc2c9 00 00         brk $00
$ffc2cb 00 00         brk $00
$ffc2cd 00 00         brk $00
$ffc2cf 00 00         brk $00
$ffc2d1 00 00         brk $00
$ffc2d3 00 00         brk $00
$ffc2d5 00 00         brk $00
$ffc2d7 00 5c         brk $5c
$ffc2d9 00 00         brk $00
$ffc2db ff 5c 03 00   sbc $00035c,x
$ffc2df ff 18 fb c2   sbc $c2fb18,x
$ffc2e3 30 a2         bmi $ffc287
$ffc2e5 00 11         brk $11
$ffc2e7 a0 e1         ldy #$e1
$ffc2e9 00 a9         brk $a9
$ffc2eb 00 06         brk $06
$ffc2ed 8f 00 11 e1   sta $e11100
$ffc2f1 5c 14 10 e1   jmp $e11014
$ffc2f5 00 00         brk $00
$ffc2f7 00 00         brk $00
$ffc2f9 00 00         brk $00
$ffc2fb 00 00         brk $00
$ffc2fd 00 20         brk $20
$ffc2ff 10 e2         bpl $ffc2e3
$ffc301 40            rti
$ffc302 70 37         bvs $ffc33b
$ffc304 ea            nop
$ffc305 38            sec
$ffc306 90 18         bcc $ffc320
$ffc308 b8            clv
$ffc309 50 30         bvc $ffc33b
$ffc30b 01 88         ora ($88,x)
$ffc30d 69 6f         adc #$6f
$ffc30f 75 7b         adc $7b,x
$ffc311 4c 98 c3      jmp $c398
$ffc314 eb            xba
$ffc315 ad ed 03      lda $03ed
$ffc318 48            pha
$ffc319 ad ee 03      lda $03ee
$ffc31c 48            pha
$ffc31d a9 30         lda #$30
$ffc31f 0c 68 c0      tsb $c068
$ffc322 b0 03         bcs $ffc327
$ffc324 1c 68 c0      trb $c068
$ffc327 68            pla
$ffc328 8d ee 03      sta $03ee
$ffc32b 68            pla
$ffc32c 8d ed 03      sta $03ed
$ffc32f eb            xba
$ffc330 8d 08 c0      sta $c008
$ffc333 50 03         bvc $ffc338
$ffc335 8d 09 c0      sta $c009
$ffc338 6c ed 03      jmp ($03ed)
$ffc33b 8d 7b 06      sta $067b
$ffc33e 5a            phy
$ffc33f da            phx
$ffc340 08            php
$ffc341 2c f8 07      bit $07f8
$ffc344 30 05         bmi $ffc34b
$ffc346 a9 08         lda #$08
$ffc348 0c fb 04      tsb $04fb
$ffc34b 20 8f c3      jsr $c38f
$ffc34e 28            plp
$ffc34f 70 15         bvs $ffc366
$ffc351 90 10         bcc $ffc363
$ffc353 ad fb 04      lda $04fb
$ffc356 10 0b         bpl $ffc363
$ffc358 20 f4 cc      jsr $ccf4
$ffc35b fa            plx
$ffc35c 7a            ply
$ffc35d ad 7b 06      lda $067b
$ffc360 6c 38 00      jmp ($0038)
$ffc363 4c 5a c8      jmp $c85a
$ffc366 4c 03 c8      jmp $c803
$ffc369 20 8f c3      jsr $c38f
$ffc36c 4c b4 c9      jmp $c9b4
$ffc36f 20 8f c3      jsr $c38f
$ffc372 4c d0 c9      jmp $c9d0
$ffc375 20 8f c3      jsr $c38f
$ffc378 4c ea c9      jmp $c9ea
$ffc37b 20 8f c3      jsr $c38f
$ffc37e aa            tax
$ffc37f f0 08         beq $ffc389
$ffc381 ca            dex
$ffc382 d0 07         bne $ffc38b
$ffc384 20 94 cf      jsr $cf94
$ffc387 10 04         bpl $ffc38d
$ffc389 38            sec
$ffc38a 60            rts
$ffc38b a2 03         ldx #$03
$ffc38d 18            clc
$ffc38e 60            rts
$ffc38f a2 c3         ldx #$c3
$ffc391 8e f8 07      stx $07f8
$ffc394 ae ff cf      ldx $cfff
$ffc397 60            rts
$ffc398 8b            phb
$ffc399 48            pha
$ffc39a 5a            phy
$ffc39b da            phx
$ffc39c ad 68 c0      lda $c068
$ffc39f 48            pha
$ffc3a0 29 cf         and #$cf
$ffc3a2 8d 68 c0      sta $c068
$ffc3a5 ad 36 c0      lda $c036
$ffc3a8 48            pha
$ffc3a9 09 80         ora #$80
$ffc3ab 8d 36 c0      sta $c036
$ffc3ae 08            php
$ffc3af 08            php
$ffc3b0 18            clc
$ffc3b1 fb            xce
$ffc3b2 c2 30         rep #$30
$ffc3b4 38            sec
$ffc3b5 a5 3e         lda $3e
$ffc3b7 48            pha
$ffc3b8 e5 3c         sbc $3c
$ffc3ba 48            pha
$ffc3bb a3 05         lda $05,sp
$ffc3bd 6a            ror
$ffc3be a3 01         lda $01,sp
$ffc3c0 a6 3c         ldx $3c
$ffc3c2 a4 42         ldy $42
$ffc3c4 90 05         bcc $ffc3cb
$ffc3c6 54 01 00      mvn $01,$00
$ffc3c9 80 03         bra $ffc3ce
$ffc3cb 54 00 01      mvn $00,$01
$ffc3ce 68            pla
$ffc3cf 38            sec
$ffc3d0 65 42         adc $42
$ffc3d2 85 42         sta $42
$ffc3d4 68            pla
$ffc3d5 1a            inc
$ffc3d6 85 3c         sta $3c
$ffc3d8 68            pla
$ffc3d9 e2 30         sep #$30
$ffc3db 68            pla
$ffc3dc 8d 36 c0      sta $c036
$ffc3df 68            pla
$ffc3e0 8d 68 c0      sta $c068
$ffc3e3 38            sec
$ffc3e4 fb            xce
$ffc3e5 fa            plx
$ffc3e6 7a            ply
$ffc3e7 68            pla
$ffc3e8 ab            plb
$ffc3e9 60            rts
$ffc3ea af ee 02 e1   lda $e102ee
$ffc3ee 49 07         eor #$07
$ffc3f0 38            sec
$ffc3f1 e9 03         sbc #$03
$ffc3f3 0a            asl
$ffc3f4 1a            inc
$ffc3f5 8f 45 01 e1   sta $e10145
$ffc3f9 60            rts
$ffc3fa 88            dey
$ffc3fb 95 8a         sta $8a,x
$ffc3fd 8b            phb
$ffc3fe 86 82         stx $82
$ffc400 80 05         bra $ffc407
$ffc402 c6 c1         dec $c1
$ffc404 c2 38         rep #$38
$ffc406 90 18         bcc $ffc420
$ffc408 80 16         bra $ffc420
$ffc40a ea            nop
$ffc40b 01 20         ora ($20,x)
$ffc40d 72 72         adc ($72)
$ffc40f 72 72         adc ($72)
$ffc411 00 60         brk $60
$ffc413 29 a6 7b      and #$7ba6
$ffc416 99 a1 e0      sta $e0a1,y
$ffc419 76 9f         ror $9f,x
$ffc41b 9f 9f 9f 9f   sta $9f9f9f,x
$ffc41f 9f 5a 9b a2   sta $a29b5a,x
$ffc423 06 20         asl $20
$ffc425 a8            tay
$ffc426 c4 7a         cpy $7a
$ffc428 60            rts
$ffc429 a9 0e 1c      lda #$1c0e
$ffc42c 7c 07 af      jmp ($af07,x)
$ffc42f c3 00         cmp $00,sp
$ffc431 e1 a8         sbc ($a8,x)
$ffc433 29 08 f0      and #$f008
$ffc436 0c 8d 47      tsb $478d
$ffc439 c0 98 29      cpy #$2998
$ffc43c f7 8f         sbc [$8f],y
$ffc43e c3 00         cmp $00,sp
$ffc440 e1 a9         sbc ($a9,x)
$ffc442 08            php
$ffc443 a8            tay
$ffc444 0c 7c 07      tsb $077c
$ffc447 2c 27 c0      bit $c027
$ffc44a 50 08         bvc $ffc454
$ffc44c 10 06         bpl $ffc454
$ffc44e a9 0e a8      lda #$a80e
$ffc451 0c 7c 07      tsb $077c
$ffc454 ad 7c 07      lda $077c
$ffc457 8f 96 01 e1   sta $e10196
$ffc45b 98            tya
$ffc45c f0 16         beq $ffc474
$ffc45e 18            clc
$ffc45f 60            rts
$ffc460 c9 10 b0      cmp #$b010
$ffc463 10 a2         bpl $ffc407
$ffc465 0a            asl
$ffc466 20 a8 c4      jsr $c4a8
$ffc469 af 97 01 e1   lda $e10197
$ffc46d 8d fc 07      sta $07fc
$ffc470 18            clc
$ffc471 60            rts
$ffc472 a2 03 38      ldx #$3803
$ffc475 60            rts
$ffc476 a2 00 20      ldx #$2000
$ffc479 a8            tay
$ffc47a c4 9c         cpy $9c
$ffc47c 7c 05 9c      jmp ($9c05,x)
$ffc47f 7c 04 9c      jmp ($9c04,x)
$ffc482 fc 05 9c      jsr ($9c05,x)
$ffc485 fc 04 9c      jsr ($9c04,x)
$ffc488 7c 07 a2      jmp ($a207,x)
$ffc48b 03 a9         ora $a9,sp
$ffc48d 00 9f         brk $9f
$ffc48f 90 01         bcc $ffc492
$ffc491 e1 ca         sbc ($ca,x)
$ffc493 10 f9         bpl $ffc48e
$ffc495 8f 96 01 e1   sta $e10196
$ffc499 08            php
$ffc49a 78            sei
$ffc49b 20 c6 c4      jsr $c4c6
$ffc49e 28            plp
$ffc49f 18            clc
$ffc4a0 60            rts
$ffc4a1 a2 02 20      ldx #$2002
$ffc4a4 a8            tay
$ffc4a5 c4 a2         cpy $a2
$ffc4a7 04 08         tsb $08
$ffc4a9 18            clc
$ffc4aa fb            xce
$ffc4ab 28            plp
$ffc4ac eb            xba
$ffc4ad ad 36 c0      lda $c036
$ffc4b0 48            pha
$ffc4b1 09 80 8d      ora #$8d80
$ffc4b4 36 c0         rol $c0,x
$ffc4b6 eb            xba
$ffc4b7 22 08 77 ff   jsr $ff7708
$ffc4bb 08            php
$ffc4bc 38            sec
$ffc4bd fb            xce
$ffc4be 28            plp
$ffc4bf eb            xba
$ffc4c0 68            pla
$ffc4c1 8d 36 c0      sta $c036
$ffc4c4 eb            xba
$ffc4c5 60            rts
$ffc4c6 ad 27 c0      lda $c027
$ffc4c9 10 a9         bpl $ffc474
$ffc4cb 6a            ror
$ffc4cc 6a            ror
$ffc4cd ad 24 c0      lda $c024
$ffc4d0 aa            tax
$ffc4d1 b0 a1         bcs $ffc474
$ffc4d3 ad 27 c0      lda $c027
$ffc4d6 29 02 f0      and #$f002
$ffc4d9 9a            txs
$ffc4da ad 24 c0      lda $c024
$ffc4dd a8            tay
$ffc4de 18            clc
$ffc4df 60            rts
$ffc4e0 a2 08 20      ldx #$2008
$ffc4e3 a8            tay
$ffc4e4 c4 80         cpy $80
$ffc4e6 b2 20         lda ($20)
$ffc4e8 80 c0         bra $ffc4aa
$ffc4ea 6b            rtl
$ffc4eb 20 85 c0      jsr $c085
$ffc4ee 6b            rtl
$ffc4ef 00 00         brk $00
$ffc4f1 00 00         brk $00
$ffc4f3 00 00         brk $00
$ffc4f5 00 00         brk $00
$ffc4f7 00 00         brk $00
$ffc4f9 00 00         brk $00
$ffc4fb d6 c6         dec $c6,x
$ffc4fd c1 c2         cmp ($c2,x)
$ffc4ff 01 a2         ora ($a2,x)
$ffc501 20 a2 00      jsr $00a2
$ffc504 a2 03 c9      ldx #$c903
$ffc507 00 b0         brk $b0
$ffc509 0c 38 b0      tsb $b038
$ffc50c 01 18         ora ($18,x)
$ffc50e 6a            ror
$ffc50f 29 80 8f      and #$8f80
$ffc512 b0 0f         bcs $ffc523
$ffc514 e1 18         sbc ($18,x)
$ffc516 90 06         bcc $ffc51e
$ffc518 a9 c0 8f      lda #$8fc0
$ffc51b b0 0f         bcs $ffc52c
$ffc51d e1 fb         sbc ($fb,x)
$ffc51f 18            clc
$ffc520 fb            xce
$ffc521 e2 30         sep #$30
$ffc523 a9 c5         lda #$c5
$ffc525 8d f8 07      sta $07f8
$ffc528 a9 04         lda #$04
$ffc52a 8f cb 0f e1   sta $e10fcb
$ffc52e 08            php
$ffc52f 8b            phb
$ffc530 a9 00         lda #$00
$ffc532 48            pha
$ffc533 ab            plb
$ffc534 ad 36 c0      lda $c036
$ffc537 48            pha
$ffc538 8f cc 0f e1   sta $e10fcc
$ffc53c 29 fb         and #$fb
$ffc53e 09 80         ora #$80
$ffc540 8d 36 c0      sta $c036
$ffc543 a3 03         lda $03,sp
$ffc545 4a            lsr
$ffc546 22 06 56 ff   jsr $ff5606
$ffc54a eb            xba
$ffc54b 68            pla
$ffc54c af cc 0f e1   lda $e10fcc
$ffc550 8d 36 c0      sta $c036
$ffc553 08            php
$ffc554 38            sec
$ffc555 fb            xce
$ffc556 28            plp
$ffc557 ab            plb
$ffc558 68            pla
$ffc559 eb            xba
$ffc55a 48            pha
$ffc55b af b0 0f e1   lda $e10fb0
$ffc55f 29 40         and #$40
$ffc561 d0 02         bne $ffc565
$ffc563 68            pla
$ffc564 60            rts
$ffc565 a9 00         lda #$00
$ffc567 48            pha
$ffc568 ab            plb
$ffc569 a9 08         lda #$08
$ffc56b 8d 35 c0      sta $c035
$ffc56e 68            pla
$ffc56f b0 03         bcs $ffc574
$ffc571 4c 01 08      jmp $0801
$ffc574 a6 00         ldx $00
$ffc576 d0 0a         bne $ffc582
$ffc578 a5 01         lda $01
$ffc57a cd f8 07      cmp $07f8
$ffc57d d0 03         bne $ffc582
$ffc57f 4c ba fa      jmp $faba
$ffc582 4c 00 e0      jmp $e000
$ffc585 20 93 fe      jsr $fe93
$ffc588 20 89 fe      jsr $fe89
$ffc58b 20 39 fb      jsr $fb39
$ffc58e 20 58 fc      jsr $fc58
$ffc591 a9 0a         lda #$0a
$ffc593 85 25         sta $25
$ffc595 64 24         stz $24
$ffc597 20 22 fc      jsr $fc22
$ffc59a 6b            rtl
$ffc59b 03 03         ora $03,sp
$ffc59d 83 01         sta $01,sp
$ffc59f 83 01         sta $01,sp
$ffc5a1 01 01         ora ($01,x)
$ffc5a3 04 84         tsb $84
$ffc5a5 83 03         sta $03,sp
$ffc5a7 84 80         sty $80
$ffc5a9 80 80         bra $ffc52b
$ffc5ab 80 80         bra $ffc52d
$ffc5ad 80 80         bra $ffc52f
$ffc5af 80 00         bra $ffc5b1
$ffc5b1 00 00         brk $00
$ffc5b3 00 00         brk $00
$ffc5b5 00 00         brk $00
$ffc5b7 00 80         brk $80
$ffc5b9 80 80         bra $ffc53b
$ffc5bb 80 00         bra $ffc5bd
$ffc5bd 00 00         brk $00
$ffc5bf 00 80         brk $80
$ffc5c1 80 80         bra $ffc543
$ffc5c3 80 00         bra $ffc5c5
$ffc5c5 00 00         brk $00
$ffc5c7 00 80         brk $80
$ffc5c9 80 00         bra $ffc5cb
$ffc5cb 00 80         brk $80
$ffc5cd 80 00         bra $ffc5cf
$ffc5cf 00 80         brk $80
$ffc5d1 80 00         bra $ffc5d3
$ffc5d3 00 80         brk $80
$ffc5d5 80 00         bra $ffc5d7
$ffc5d7 00 80         brk $80
$ffc5d9 00 80         brk $80
$ffc5db 00 80         brk $80
$ffc5dd 00 80         brk $80
$ffc5df 00 80         brk $80
$ffc5e1 00 80         brk $80
$ffc5e3 00 80         brk $80
$ffc5e5 00 80         brk $80
$ffc5e7 00 c3         brk $c3
$ffc5e9 ff fc f3 cf   sbc $cff3fc,x
$ffc5ed 3f 00 00 00   and $000000,x
$ffc5f1 00 00         brk $00
$ffc5f3 00 00         brk $00
$ffc5f5 00 00         brk $00
$ffc5f7 00 00         brk $00
$ffc5f9 00 00         brk $00
$ffc5fb c0 00         cpy #$00
$ffc5fd 00 bf         brk $bf
$ffc5ff 0a            asl
$ffc600 a2 20         ldx #$20
$ffc602 a0 00         ldy #$00
$ffc604 64 03         stz $03
$ffc606 64 3c         stz $3c
$ffc608 5a            phy
$ffc609 5a            phy
$ffc60a 2b            pld
$ffc60b 4b            phk
$ffc60c ab            plb
$ffc60d a9 c6         lda #$c6
$ffc60f 8d f8 07      sta $07f8
$ffc612 e2 30         sep #$30
$ffc614 08            php
$ffc615 68            pla
$ffc616 8d fe 07      sta $07fe
$ffc619 78            sei
$ffc61a 22 09 56 ff   jsr $ff5609
$ffc61e a2 60         ldx #$60
$ffc620 b0 0c         bcs $ffc62e
$ffc622 64 03         stz $03
$ffc624 78            sei
$ffc625 18            clc
$ffc626 08            php
$ffc627 28            plp
$ffc628 a6 2b         ldx $2b
$ffc62a c6 03         dec $03
$ffc62c d0 28         bne $ffc656
$ffc62e bd 88 c0      lda $c088,x
$ffc631 22 71 56 ff   jsr $ff5671
$ffc635 20 48 c6      jsr $c648
$ffc638 a5 01         lda $01
$ffc63a cd f8 07      cmp $07f8
$ffc63d d0 03         bne $ffc642
$ffc63f 4c ba fa      jmp $faba
$ffc642 4c 00 e0      jmp $e000
$ffc645 9c 35 c0      stz $c035
$ffc648 ad fe 07      lda $07fe
$ffc64b 29 04         and #$04
$ffc64d d0 01         bne $ffc650
$ffc64f 58            cli
$ffc650 a5 3d         lda $3d
$ffc652 60            rts
$ffc653 00 00         brk $00
$ffc655 00 08         brk $08
$ffc657 88            dey
$ffc658 d0 04         bne $ffc65e
$ffc65a f0 cb         beq $ffc627
$ffc65c 80 c4         bra $ffc622
$ffc65e bd 8c c0      lda $c08c,x
$ffc661 10 fb         bpl $ffc65e
$ffc663 49 d5         eor #$d5
$ffc665 d0 f0         bne $ffc657
$ffc667 bd 8c c0      lda $c08c,x
$ffc66a 10 fb         bpl $ffc667
$ffc66c c9 aa         cmp #$aa
$ffc66e d0 f3         bne $ffc663
$ffc670 ea            nop
$ffc671 bd 8c c0      lda $c08c,x
$ffc674 10 fb         bpl $ffc671
$ffc676 c9 96         cmp #$96
$ffc678 f0 09         beq $ffc683
$ffc67a 28            plp
$ffc67b 90 a8         bcc $ffc625
$ffc67d 49 ad         eor #$ad
$ffc67f f0 25         beq $ffc6a6
$ffc681 d0 a2         bne $ffc625
$ffc683 a0 03         ldy #$03
$ffc685 85 40         sta $40
$ffc687 bd 8c c0      lda $c08c,x
$ffc68a 10 fb         bpl $ffc687
$ffc68c 2a            rol
$ffc68d 85 3c         sta $3c
$ffc68f bd 8c c0      lda $c08c,x
$ffc692 10 fb         bpl $ffc68f
$ffc694 25 3c         and $3c
$ffc696 88            dey
$ffc697 d0 ec         bne $ffc685
$ffc699 28            plp
$ffc69a c5 3d         cmp $3d
$ffc69c d0 87         bne $ffc625
$ffc69e a5 40         lda $40
$ffc6a0 c5 41         cmp $41
$ffc6a2 d0 81         bne $ffc625
$ffc6a4 b0 82         bcs $ffc628
$ffc6a6 a0 56         ldy #$56
$ffc6a8 84 3c         sty $3c
$ffc6aa bc 8c c0      ldy $c08c,x
$ffc6ad 10 fb         bpl $ffc6aa
$ffc6af 59 d6 02      eor $02d6,y
$ffc6b2 a4 3c         ldy $3c
$ffc6b4 88            dey
$ffc6b5 99 00 03      sta $0300,y
$ffc6b8 d0 ee         bne $ffc6a8
$ffc6ba 84 3c         sty $3c
$ffc6bc bc 8c c0      ldy $c08c,x
$ffc6bf 10 fb         bpl $ffc6bc
$ffc6c1 59 d6 02      eor $02d6,y
$ffc6c4 a4 3c         ldy $3c
$ffc6c6 91 26         sta ($26),y
$ffc6c8 c8            iny
$ffc6c9 d0 ef         bne $ffc6ba
$ffc6cb bc 8c c0      ldy $c08c,x
$ffc6ce 10 fb         bpl $ffc6cb
$ffc6d0 59 d6 02      eor $02d6,y
$ffc6d3 d0 cd         bne $ffc6a2
$ffc6d5 a0 00         ldy #$00
$ffc6d7 a2 56         ldx #$56
$ffc6d9 ca            dex
$ffc6da 30 fb         bmi $ffc6d7
$ffc6dc b1 26         lda ($26),y
$ffc6de 5e 00 03      lsr $0300,x
$ffc6e1 2a            rol
$ffc6e2 5e 00 03      lsr $0300,x
$ffc6e5 2a            rol
$ffc6e6 91 26         sta ($26),y
$ffc6e8 c8            iny
$ffc6e9 d0 ee         bne $ffc6d9
$ffc6eb e6 27         inc $27
$ffc6ed e6 3d         inc $3d
$ffc6ef a5 3d         lda $3d
$ffc6f1 cd 00 08      cmp $0800
$ffc6f4 a6 4f         ldx $4f
$ffc6f6 90 db         bcc $ffc6d3
$ffc6f8 20 45 c6      jsr $c645
$ffc6fb 4c 01 08      jmp $0801
$ffc6fe 00 00         brk $00
$ffc700 00 00         brk $00
$ffc702 00 00         brk $00
$ffc704 00 00         brk $00
$ffc706 00 00         brk $00
$ffc708 00 00         brk $00
$ffc70a 00 00         brk $00
$ffc70c 00 00         brk $00
$ffc70e 00 00         brk $00
$ffc710 00 00         brk $00
$ffc712 00 00         brk $00
$ffc714 00 00         brk $00
$ffc716 00 00         brk $00
$ffc718 00 00         brk $00
$ffc71a 00 00         brk $00
$ffc71c 00 00         brk $00
$ffc71e 00 00         brk $00
$ffc720 00 00         brk $00
$ffc722 00 00         brk $00
$ffc724 00 00         brk $00
$ffc726 00 00         brk $00
$ffc728 00 00         brk $00
$ffc72a 00 00         brk $00
$ffc72c 00 00         brk $00
$ffc72e 00 00         brk $00
$ffc730 00 00         brk $00
$ffc732 00 00         brk $00
$ffc734 00 00         brk $00
$ffc736 00 00         brk $00
$ffc738 00 00         brk $00
$ffc73a 00 00         brk $00
$ffc73c 00 00         brk $00
$ffc73e 00 00         brk $00
$ffc740 00 00         brk $00
$ffc742 00 00         brk $00
$ffc744 00 00         brk $00
$ffc746 00 00         brk $00
$ffc748 00 00         brk $00
$ffc74a 00 00         brk $00
$ffc74c 00 00         brk $00
$ffc74e 00 00         brk $00
$ffc750 00 00         brk $00
$ffc752 00 00         brk $00
$ffc754 00 00         brk $00
$ffc756 00 00         brk $00
$ffc758 00 00         brk $00
$ffc75a 00 00         brk $00
$ffc75c 00 00         brk $00
$ffc75e 00 00         brk $00
$ffc760 00 00         brk $00
$ffc762 00 00         brk $00
$ffc764 00 00         brk $00
$ffc766 00 00         brk $00
$ffc768 00 00         brk $00
$ffc76a 00 00         brk $00
$ffc76c 00 00         brk $00
$ffc76e 00 00         brk $00
$ffc770 00 00         brk $00
$ffc772 00 00         brk $00
$ffc774 00 00         brk $00
$ffc776 00 00         brk $00
$ffc778 00 00         brk $00
$ffc77a 00 00         brk $00
$ffc77c 00 00         brk $00
$ffc77e 00 00         brk $00
$ffc780 00 00         brk $00
$ffc782 00 00         brk $00
$ffc784 00 00         brk $00
$ffc786 00 00         brk $00
$ffc788 00 00         brk $00
$ffc78a 00 00         brk $00
$ffc78c 00 00         brk $00
$ffc78e 00 00         brk $00
$ffc790 00 00         brk $00
$ffc792 00 00         brk $00
$ffc794 00 00         brk $00
$ffc796 22 18 10 e1   jsr $e11018
$ffc79a 60            rts
$ffc79b 22 1c 10 e1   jsr $e1101c
$ffc79f 60            rts
$ffc7a0 fb            xce
$ffc7a1 d4 85         pei ($85)
$ffc7a3 ab            plb
$ffc7a4 ab            plb
$ffc7a5 dc 84 00      jmp [$0084]
$ffc7a8 18            clc
$ffc7a9 fb            xce
$ffc7aa 6b            rtl
$ffc7ab a2 ff         ldx #$ff
$ffc7ad 6b            rtl
$ffc7ae 18            clc
$ffc7af fb            xce
$ffc7b0 22 22 10 e1   jsr $e11022
$ffc7b4 08            php
$ffc7b5 38            sec
$ffc7b6 fb            xce
$ffc7b7 28            plp
$ffc7b8 50 0a         bvc $ffc7c4
$ffc7ba af 21 10 e1   lda $e11021
$ffc7be 48            pha
$ffc7bf af 20 10 e1   lda $e11020
$ffc7c3 48            pha
$ffc7c4 60            rts
$ffc7c5 00 00         brk $00
$ffc7c7 00 00         brk $00
$ffc7c9 00 00         brk $00
$ffc7cb 00 00         brk $00
$ffc7cd 00 00         brk $00
$ffc7cf 00 00         brk $00
$ffc7d1 00 00         brk $00
$ffc7d3 00 00         brk $00
$ffc7d5 00 00         brk $00
$ffc7d7 00 5c         brk $5c
$ffc7d9 00 00         brk $00
$ffc7db ff 5c 03 00   sbc $00035c,x
$ffc7df ff 00 00 00   sbc $000000,x
$ffc7e3 00 00         brk $00
$ffc7e5 00 00         brk $00
$ffc7e7 00 00         brk $00
$ffc7e9 00 00         brk $00
$ffc7eb 00 00         brk $00
$ffc7ed 00 00         brk $00
$ffc7ef 00 00         brk $00
$ffc7f1 00 00         brk $00
$ffc7f3 00 00         brk $00
$ffc7f5 00 00         brk $00
$ffc7f7 00 00         brk $00
$ffc7f9 41 54         eor ($54,x)
$ffc7fb 4c 4b 00      jmp $004b
$ffc7fe 20 10 4c      jsr $4c10
$ffc801 b0 c9         bcs $ffc7cc
$ffc803 20 89 ce      jsr $ce89
$ffc806 20 2a c8      jsr $c82a
$ffc809 20 cb cc      jsr $cccb
$ffc80c a9 00         lda #$00
$ffc80e 8f 35 01 e1   sta $e10135
$ffc812 a9 01         lda #$01
$ffc814 8d fb 04      sta $04fb
$ffc817 06 21         asl $21
$ffc819 8d 01 c0      sta $c001
$ffc81c 8d 0d c0      sta $c00d
$ffc81f 8d 0f c0      sta $c00f
$ffc822 20 37 cc      jsr $cc37
$ffc825 ac 7b 05      ldy $057b
$ffc828 80 32         bra $ffc85c
$ffc82a a9 07         lda #$07
$ffc82c 85 36         sta $36
$ffc82e a9 c3         lda #$c3
$ffc830 85 37         sta $37
$ffc832 a9 05         lda #$05
$ffc834 85 38         sta $38
$ffc836 a9 c3         lda #$c3
$ffc838 85 39         sta $39
$ffc83a 60            rts
$ffc83b e6 4e         inc $4e
$ffc83d d0 02         bne $ffc841
$ffc83f e6 4f         inc $4f
$ffc841 20 74 cf      jsr $cf74
$ffc844 10 f5         bpl $ffc83b
$ffc846 60            rts
$ffc847 c6 ca         dec $ca
$ffc849 c2 00         rep #$00
$ffc84b 00 00         brk $00
$ffc84d 4c 6f c3      jmp $c36f
$ffc850 a4 35         ldy $35
$ffc852 18            clc
$ffc853 b0 38         bcs $ffc88d
$ffc855 8d 7b 06      sta $067b
$ffc858 5a            phy
$ffc859 da            phx
$ffc85a b0 6e         bcs $ffc8ca
$ffc85c 20 73 c9      jsr $c973
$ffc85f ad 7b 06      lda $067b
$ffc862 c9 8d         cmp #$8d
$ffc864 d0 18         bne $ffc87e
$ffc866 ae 00 c0      ldx $c000
$ffc869 10 13         bpl $ffc87e
$ffc86b e0 93         cpx #$93
$ffc86d d0 0f         bne $ffc87e
$ffc86f 2c 10 c0      bit $c010
$ffc872 ae 00 c0      ldx $c000
$ffc875 10 fb         bpl $ffc872
$ffc877 e0 83         cpx #$83
$ffc879 f0 03         beq $ffc87e
$ffc87b 2c 10 c0      bit $c010
$ffc87e eb            xba
$ffc87f af 37 01 e1   lda $e10137
$ffc883 10 0e         bpl $ffc893
$ffc885 eb            xba
$ffc886 8f 35 01 e1   sta $e10135
$ffc88a a9 01         lda #$01
$ffc88c 8f 37 01 e1   sta $e10137
$ffc890 18            clc
$ffc891 80 1d         bra $ffc8b0
$ffc893 eb            xba
$ffc894 29 7f         and #$7f
$ffc896 c9 20         cmp #$20
$ffc898 b0 05         bcs $ffc89f
$ffc89a 20 7c ca      jsr $ca7c
$ffc89d 80 11         bra $ffc8b0
$ffc89f ad 7b 06      lda $067b
$ffc8a2 20 e7 cd      jsr $cde7
$ffc8a5 c8            iny
$ffc8a6 8c 7b 05      sty $057b
$ffc8a9 c4 21         cpy $21
$ffc8ab 90 03         bcc $ffc8b0
$ffc8ad 20 ea ca      jsr $caea
$ffc8b0 a9 08         lda #$08
$ffc8b2 1c fb 04      trb $04fb
$ffc8b5 ad 7b 05      lda $057b
$ffc8b8 2c 1f c0      bit $c01f
$ffc8bb 10 02         bpl $ffc8bf
$ffc8bd a9 00         lda #$00
$ffc8bf 85 24         sta $24
$ffc8c1 8d 7b 04      sta $047b
$ffc8c4 fa            plx
$ffc8c5 7a            ply
$ffc8c6 ad 7b 06      lda $067b
$ffc8c9 60            rts
$ffc8ca a4 24         ldy $24
$ffc8cc ad 7b 06      lda $067b
$ffc8cf 91 28         sta ($28),y
$ffc8d1 20 73 c9      jsr $c973
$ffc8d4 20 c9 ce      jsr $cec9
$ffc8d7 20 fc ce      jsr $cefc
$ffc8da 10 fb         bpl $ffc8d7
$ffc8dc 8d 7b 06      sta $067b
$ffc8df a8            tay
$ffc8e0 ad fb 04      lda $04fb
$ffc8e3 29 08         and #$08
$ffc8e5 f0 ce         beq $ffc8b5
$ffc8e7 c0 8d         cpy #$8d
$ffc8e9 d0 05         bne $ffc8f0
$ffc8eb a9 08         lda #$08
$ffc8ed 1c fb 04      trb $04fb
$ffc8f0 c0 9b         cpy #$9b
$ffc8f2 f0 0e         beq $ffc902
$ffc8f4 c0 95         cpy #$95
$ffc8f6 d0 bd         bne $ffc8b5
$ffc8f8 20 b8 cd      jsr $cdb8
$ffc8fb 09 80         ora #$80
$ffc8fd 8d 7b 06      sta $067b
$ffc900 d0 b3         bne $ffc8b5
$ffc902 20 37 ce      jsr $ce37
$ffc905 20 3b c8      jsr $c83b
$ffc908 20 49 ce      jsr $ce49
$ffc90b 20 ad cd      jsr $cdad
$ffc90e 29 7f         and #$7f
$ffc910 a0 10         ldy #$10
$ffc912 d9 62 c9      cmp $c962,y
$ffc915 f0 05         beq $ffc91c
$ffc917 88            dey
$ffc918 10 f8         bpl $ffc912
$ffc91a 30 0f         bmi $ffc92b
$ffc91c b9 51 c9      lda $c951,y
$ffc91f 29 7f         and #$7f
$ffc921 20 7f ca      jsr $ca7f
$ffc924 b9 51 c9      lda $c951,y
$ffc927 30 d9         bmi $ffc902
$ffc929 10 a9         bpl $ffc8d4
$ffc92b a8            tay
$ffc92c ad fb 04      lda $04fb
$ffc92f c0 11         cpy #$11
$ffc931 d0 0b         bne $ffc93e
$ffc933 20 e6 cc      jsr $cce6
$ffc936 a9 98         lda #$98
$ffc938 8d 7b 06      sta $067b
$ffc93b 4c b5 c8      jmp $c8b5
$ffc93e c0 05         cpy #$05
$ffc940 d0 07         bne $ffc949
$ffc942 29 df         and #$df
$ffc944 8d fb 04      sta $04fb
$ffc947 80 8b         bra $ffc8d4
$ffc949 c0 04         cpy #$04
$ffc94b d0 fa         bne $ffc947
$ffc94d 09 20         ora #$20
$ffc94f d0 f3         bne $ffc944
$ffc951 0c 1c 08      tsb $081c
$ffc954 0a            asl
$ffc955 1f 1d 0b 9f   ora $9f0b1d,x
$ffc959 88            dey
$ffc95a 9c 8a 11      stz $118a
$ffc95d 12 88         ora ($88)
$ffc95f 8a            txa
$ffc960 9f 9c 40 41   sta $41409c,x
$ffc964 42            ???
$ffc965 43 44         eor $44,sp
$ffc967 45 46         eor $46
$ffc969 49 4a         eor #$4a
$ffc96b 4b            phk
$ffc96c 4d 34 38      eor $3834
$ffc96f 08            php
$ffc970 0a            asl
$ffc971 0b            phd
$ffc972 15 a4         ora $a4,x
$ffc974 25 8c         and $8c
$ffc976 fb            xce
$ffc977 05 a4         ora $a4
$ffc979 24 cc         bit $cc
$ffc97b 7b            tdc
$ffc97c 04 d0         tsb $d0
$ffc97e 03 ac         ora $ac,sp
$ffc980 7b            tdc
$ffc981 05 c4         ora $c4
$ffc983 21 90         and ($90,x)
$ffc985 02 a0         cop $a0
$ffc987 00 8c         brk $8c
$ffc989 7b            tdc
$ffc98a 05 2c         ora $2c
$ffc98c 1f c0 10 02   ora $0210c0,x
$ffc990 a0 00         ldy #$00
$ffc992 84 24         sty $24
$ffc994 8c 7b 04      sty $047b
$ffc997 ac 7b 05      ldy $057b
$ffc99a 60            rts
$ffc99b 20 78 c9      jsr $c978
$ffc99e 88            dey
$ffc99f 20 88 c9      jsr $c988
$ffc9a2 98            tya
$ffc9a3 60            rts
$ffc9a4 3a            dec
$ffc9a5 a8            tay
$ffc9a6 80 f7         bra $ffc99f
$ffc9a8 00 00         brk $00
$ffc9aa ad 7b 06      lda $067b
$ffc9ad 4c 75 c3      jmp $c375
$ffc9b0 a9 83         lda #$83
$ffc9b2 d0 02         bne $ffc9b6
$ffc9b4 a9 81         lda #$81
$ffc9b6 8d fb 04      sta $04fb
$ffc9b9 8d 01 c0      sta $c001
$ffc9bc 8d 0d c0      sta $c00d
$ffc9bf 8d 0f c0      sta $c00f
$ffc9c2 a9 00         lda #$00
$ffc9c4 8f 35 01 e1   sta $e10135
$ffc9c8 20 69 ce      jsr $ce69
$ffc9cb 20 37 cc      jsr $cc37
$ffc9ce 80 47         bra $ffca17
$ffc9d0 20 69 ce      jsr $ce69
$ffc9d3 20 3b c8      jsr $c83b
$ffc9d6 29 7f         and #$7f
$ffc9d8 8d 7b 06      sta $067b
$ffc9db a2 00         ldx #$00
$ffc9dd ad fb 04      lda $04fb
$ffc9e0 29 02         and #$02
$ffc9e2 f0 02         beq $ffc9e6
$ffc9e4 a2 c3         ldx #$c3
$ffc9e6 ad 7b 06      lda $067b
$ffc9e9 60            rts
$ffc9ea 29 7f         and #$7f
$ffc9ec aa            tax
$ffc9ed 20 69 ce      jsr $ce69
$ffc9f0 a9 08         lda #$08
$ffc9f2 2c fb 04      bit $04fb
$ffc9f5 d0 30         bne $ffca27
$ffc9f7 8a            txa
$ffc9f8 2c 26 ca      bit $ca26
$ffc9fb f0 4e         beq $ffca4b
$ffc9fd ac 7b 05      ldy $057b
$ffca00 24 32         bit $32
$ffca02 10 02         bpl $ffca06
$ffca04 09 80         ora #$80
$ffca06 20 f9 cd      jsr $cdf9
$ffca09 c8            iny
$ffca0a 8c 7b 05      sty $057b
$ffca0d c4 21         cpy $21
$ffca0f 90 06         bcc $ffca17
$ffca11 9c 7b 05      stz $057b
$ffca14 20 71 cb      jsr $cb71
$ffca17 a5 28         lda $28
$ffca19 8d 7b 07      sta $077b
$ffca1c a5 29         lda $29
$ffca1e 8d fb 07      sta $07fb
$ffca21 20 53 ce      jsr $ce53
$ffca24 a2 00         ldx #$00
$ffca26 60            rts
$ffca27 20 53 ce      jsr $ce53
$ffca2a 8a            txa
$ffca2b 38            sec
$ffca2c e9 20         sbc #$20
$ffca2e 2c fb 06      bit $06fb
$ffca31 30 2c         bmi $ffca5f
$ffca33 8d fb 05      sta $05fb
$ffca36 85 25         sta $25
$ffca38 20 64 ca      jsr $ca64
$ffca3b ad fb 06      lda $06fb
$ffca3e 8d 7b 05      sta $057b
$ffca41 a9 f7         lda #$f7
$ffca43 2d fb 04      and $04fb
$ffca46 8d fb 04      sta $04fb
$ffca49 d0 cc         bne $ffca17
$ffca4b 20 53 ce      jsr $ce53
$ffca4e 8a            txa
$ffca4f c9 1e         cmp #$1e
$ffca51 f0 05         beq $ffca58
$ffca53 20 7f ca      jsr $ca7f
$ffca56 80 bf         bra $ffca17
$ffca58 a9 08         lda #$08
$ffca5a 0c fb 04      tsb $04fb
$ffca5d a9 ff         lda #$ff
$ffca5f 8d fb 06      sta $06fb
$ffca62 80 bd         bra $ffca21
$ffca64 48            pha
$ffca65 4a            lsr
$ffca66 29 03         and #$03
$ffca68 09 04         ora #$04
$ffca6a 85 29         sta $29
$ffca6c 68            pla
$ffca6d 29 18         and #$18
$ffca6f 90 02         bcc $ffca73
$ffca71 69 7f         adc #$7f
$ffca73 85 28         sta $28
$ffca75 0a            asl
$ffca76 0a            asl
$ffca77 05 28         ora $28
$ffca79 85 28         sta $28
$ffca7b 60            rts
$ffca7c e2 40         sep #$40
$ffca7e 50 b8         bvc $ffca38
$ffca80 8d 7b 07      sta $077b
$ffca83 48            pha
$ffca84 5a            phy
$ffca85 ac 7b 07      ldy $077b
$ffca88 c0 05         cpy #$05
$ffca8a 90 13         bcc $ffca9f
$ffca8c b9 4d cb      lda $cb4d,y
$ffca8f f0 0e         beq $ffca9f
$ffca91 50 12         bvc $ffcaa5
$ffca93 30 10         bmi $ffcaa5
$ffca95 8d 7b 07      sta $077b
$ffca98 ad fb 04      lda $04fb
$ffca9b 29 28         and #$28
$ffca9d f0 03         beq $ffcaa2
$ffca9f 38            sec
$ffcaa0 b0 09         bcs $ffcaab
$ffcaa2 ad 7b 07      lda $077b
$ffcaa5 09 80         ora #$80
$ffcaa7 20 ae ca      jsr $caae
$ffcaaa 18            clc
$ffcaab 7a            ply
$ffcaac 68            pla
$ffcaad 60            rts
$ffcaae 48            pha
$ffcaaf b9 32 cb      lda $cb32,y
$ffcab2 48            pha
$ffcab3 60            rts
$ffcab4 ad fb 04      lda $04fb
$ffcab7 10 05         bpl $ffcabe
$ffcab9 29 ef         and #$ef
$ffcabb 8d fb 04      sta $04fb
$ffcabe 60            rts
$ffcabf ad fb 04      lda $04fb
$ffcac2 10 fa         bpl $ffcabe
$ffcac4 09 10         ora #$10
$ffcac6 d0 f3         bne $ffcabb
$ffcac8 ad 68 c0      lda $c068
$ffcacb 48            pha
$ffcacc 09 08         ora #$08
$ffcace 8d 68 c0      sta $c068
$ffcad1 20 dd fb      jsr $fbdd
$ffcad4 68            pla
$ffcad5 8d 68 c0      sta $c068
$ffcad8 60            rts
$ffcad9 ce 7b 05      dec $057b
$ffcadc 10 0b         bpl $ffcae9
$ffcade a5 21         lda $21
$ffcae0 8d 7b 05      sta $057b
$ffcae3 ce 7b 05      dec $057b
$ffcae6 20 0b cb      jsr $cb0b
$ffcae9 60            rts
$ffcaea 9c 7b 05      stz $057b
$ffcaed ad fb 04      lda $04fb
$ffcaf0 10 7f         bpl $ffcb71
$ffcaf2 60            rts
$ffcaf3 a5 22         lda $22
$ffcaf5 85 25         sta $25
$ffcaf7 9c 7b 05      stz $057b
$ffcafa 4c 97 cd      jmp $cd97
$ffcafd ee 7b 05      inc $057b
$ffcb00 ad 7b 05      lda $057b
$ffcb03 c5 21         cmp $21
$ffcb05 90 03         bcc $ffcb0a
$ffcb07 20 ea ca      jsr $caea
$ffcb0a 60            rts
$ffcb0b a5 22         lda $22
$ffcb0d c5 25         cmp $25
$ffcb0f b0 1e         bcs $ffcb2f
$ffcb11 c6 25         dec $25
$ffcb13 4c 97 cd      jmp $cd97
$ffcb16 ad fb 04      lda $04fb
$ffcb19 10 02         bpl $ffcb1d
$ffcb1b 29 fb         and #$fb
$ffcb1d a0 ff         ldy #$ff
$ffcb1f d0 09         bne $ffcb2a
$ffcb21 ad fb 04      lda $04fb
$ffcb24 10 02         bpl $ffcb28
$ffcb26 09 04         ora #$04
$ffcb28 a0 7f         ldy #$7f
$ffcb2a 8d fb 04      sta $04fb
$ffcb2d 84 32         sty $32
$ffcb2f 60            rts
$ffcb30 a9 81         lda #$81
$ffcb32 8f 37 01 e1   sta $e10137
$ffcb36 60            rts
$ffcb37 b3 be         lda ($be,sp),y
$ffcb39 c7 d8         cmp [$d8]
$ffcb3b 00 70         brk $70
$ffcb3d 1a            inc
$ffcb3e 36 e9         rol $e9,x
$ffcb40 15 20         ora $20,x
$ffcb42 00 8b         brk $8b
$ffcb44 9d 00 00      sta $0000,x
$ffcb47 e5 6c         sbc $6c
$ffcb49 83 d9         sta $d9,sp
$ffcb4b f2 3b         sbc ($3b)
$ffcb4d df fc 3f 2f   cmp $2f3ffc,x
$ffcb51 0a            asl
$ffcb52 4a            lsr
$ffcb53 4a            lsr
$ffcb54 ca            dex
$ffcb55 ca            dex
$ffcb56 00 cb         brk $cb
$ffcb58 4c 4c ca      jmp $ca4c
$ffcb5b 4b            phk
$ffcb5c 4b            phk
$ffcb5d 00 4c         brk $4c
$ffcb5f 4c 00 00      jmp $0000
$ffcb62 4c 4b 4b      jmp $4b4b
$ffcb65 4c 4a 4c      jmp $4c4a
$ffcb68 4c 4a 4c      jmp $4c4a
$ffcb6b cb            wai
$ffcb6c 4b            phk
$ffcb6d a0 00         ldy #$00
$ffcb6f f0 15         beq $ffcb86
$ffcb71 e6 25         inc $25
$ffcb73 a5 25         lda $25
$ffcb75 8d fb 05      sta $05fb
$ffcb78 c5 23         cmp $23
$ffcb7a b0 03         bcs $ffcb7f
$ffcb7c 4c 9c cd      jmp $cd9c
$ffcb7f ce fb 05      dec $05fb
$ffcb82 c6 25         dec $25
$ffcb84 a0 01         ldy #$01
$ffcb86 ad 36 c0      lda $c036
$ffcb89 48            pha
$ffcb8a 09 80         ora #$80
$ffcb8c 8d 36 c0      sta $c036
$ffcb8f 20 97 cb      jsr $cb97
$ffcb92 68            pla
$ffcb93 8d 36 c0      sta $c036
$ffcb96 60            rts
$ffcb97 da            phx
$ffcb98 8c 7b 07      sty $077b
$ffcb9b a5 21         lda $21
$ffcb9d 48            pha
$ffcb9e 2c 1f c0      bit $c01f
$ffcba1 10 1b         bpl $ffcbbe
$ffcba3 8d 01 c0      sta $c001
$ffcba6 4a            lsr
$ffcba7 aa            tax
$ffcba8 a5 20         lda $20
$ffcbaa 4a            lsr
$ffcbab b8            clv
$ffcbac 90 02         bcc $ffcbb0
$ffcbae e2 40         sep #$40
$ffcbb0 2a            rol
$ffcbb1 45 21         eor $21
$ffcbb3 4a            lsr
$ffcbb4 70 03         bvs $ffcbb9
$ffcbb6 b0 01         bcs $ffcbb9
$ffcbb8 ca            dex
$ffcbb9 86 21         stx $21
$ffcbbb ad 1f c0      lda $c01f
$ffcbbe 08            php
$ffcbbf a6 22         ldx $22
$ffcbc1 98            tya
$ffcbc2 d0 03         bne $ffcbc7
$ffcbc4 a6 23         ldx $23
$ffcbc6 ca            dex
$ffcbc7 8a            txa
$ffcbc8 20 9c cd      jsr $cd9c
$ffcbcb a5 28         lda $28
$ffcbcd 85 2a         sta $2a
$ffcbcf a5 29         lda $29
$ffcbd1 85 2b         sta $2b
$ffcbd3 ad 7b 07      lda $077b
$ffcbd6 f0 32         beq $ffcc0a
$ffcbd8 e8            inx
$ffcbd9 e4 23         cpx $23
$ffcbdb b0 32         bcs $ffcc0f
$ffcbdd 8a            txa
$ffcbde 20 9c cd      jsr $cd9c
$ffcbe1 a4 21         ldy $21
$ffcbe3 28            plp
$ffcbe4 08            php
$ffcbe5 10 1e         bpl $ffcc05
$ffcbe7 ad 55 c0      lda $c055
$ffcbea 98            tya
$ffcbeb f0 07         beq $ffcbf4
$ffcbed b1 28         lda ($28),y
$ffcbef 91 2a         sta ($2a),y
$ffcbf1 88            dey
$ffcbf2 d0 f9         bne $ffcbed
$ffcbf4 70 04         bvs $ffcbfa
$ffcbf6 b1 28         lda ($28),y
$ffcbf8 91 2a         sta ($2a),y
$ffcbfa ad 54 c0      lda $c054
$ffcbfd a4 21         ldy $21
$ffcbff b0 04         bcs $ffcc05
$ffcc01 b1 28         lda ($28),y
$ffcc03 91 2a         sta ($2a),y
$ffcc05 88            dey
$ffcc06 10 f9         bpl $ffcc01
$ffcc08 30 c1         bmi $ffcbcb
$ffcc0a ca            dex
$ffcc0b e4 22         cpx $22
$ffcc0d 10 ce         bpl $ffcbdd
$ffcc0f 28            plp
$ffcc10 68            pla
$ffcc11 85 21         sta $21
$ffcc13 20 3c cc      jsr $cc3c
$ffcc16 20 97 cd      jsr $cd97
$ffcc19 fa            plx
$ffcc1a 60            rts
$ffcc1b 20 40 cc      jsr $cc40
$ffcc1e a5 25         lda $25
$ffcc20 48            pha
$ffcc21 10 06         bpl $ffcc29
$ffcc23 20 9c cd      jsr $cd9c
$ffcc26 20 3c cc      jsr $cc3c
$ffcc29 e6 25         inc $25
$ffcc2b a5 25         lda $25
$ffcc2d c5 23         cmp $23
$ffcc2f 90 f2         bcc $ffcc23
$ffcc31 68            pla
$ffcc32 85 25         sta $25
$ffcc34 4c 97 cd      jmp $cd97
$ffcc37 20 f3 ca      jsr $caf3
$ffcc3a 80 df         bra $ffcc1b
$ffcc3c a0 00         ldy #$00
$ffcc3e f0 03         beq $ffcc43
$ffcc40 ac 7b 05      ldy $057b
$ffcc43 a5 32         lda $32
$ffcc45 29 80         and #$80
$ffcc47 09 20         ora #$20
$ffcc49 2c 1f c0      bit $c01f
$ffcc4c 30 13         bmi $ffcc61
$ffcc4e 91 28         sta ($28),y
$ffcc50 c8            iny
$ffcc51 c4 21         cpy $21
$ffcc53 90 f9         bcc $ffcc4e
$ffcc55 60            rts
$ffcc56 da            phx
$ffcc57 a2 d8         ldx #$d8
$ffcc59 a0 14         ldy #$14
$ffcc5b a5 32         lda $32
$ffcc5d 29 a0         and #$a0
$ffcc5f 80 17         bra $ffcc78
$ffcc61 da            phx
$ffcc62 48            pha
$ffcc63 98            tya
$ffcc64 48            pha
$ffcc65 38            sec
$ffcc66 e5 21         sbc $21
$ffcc68 aa            tax
$ffcc69 98            tya
$ffcc6a 4a            lsr
$ffcc6b a8            tay
$ffcc6c 68            pla
$ffcc6d 45 20         eor $20
$ffcc6f 6a            ror
$ffcc70 b0 03         bcs $ffcc75
$ffcc72 10 01         bpl $ffcc75
$ffcc74 c8            iny
$ffcc75 68            pla
$ffcc76 b0 0b         bcs $ffcc83
$ffcc78 2c 55 c0      bit $c055
$ffcc7b 91 28         sta ($28),y
$ffcc7d 2c 54 c0      bit $c054
$ffcc80 e8            inx
$ffcc81 f0 06         beq $ffcc89
$ffcc83 91 28         sta ($28),y
$ffcc85 c8            iny
$ffcc86 e8            inx
$ffcc87 d0 ef         bne $ffcc78
$ffcc89 fa            plx
$ffcc8a 38            sec
$ffcc8b 60            rts
$ffcc8c ad fb 04      lda $04fb
$ffcc8f 30 48         bmi $ffccd9
$ffcc91 20 ce cc      jsr $ccce
$ffcc94 2c 1f c0      bit $c01f
$ffcc97 10 0d         bpl $ffcca6
$ffcc99 20 2e cd      jsr $cd2e
$ffcc9c 90 08         bcc $ffcca6
$ffcc9e 2c 1f c0      bit $c01f
$ffcca1 30 03         bmi $ffcca6
$ffcca3 20 5f cd      jsr $cd5f
$ffcca6 ad 7b 05      lda $057b
$ffcca9 18            clc
$ffccaa 65 20         adc $20
$ffccac 2c 1f c0      bit $c01f
$ffccaf 30 06         bmi $ffccb7
$ffccb1 c9 28         cmp #$28
$ffccb3 90 02         bcc $ffccb7
$ffccb5 a9 27         lda #$27
$ffccb7 8d 7b 05      sta $057b
$ffccba 85 24         sta $24
$ffccbc a5 25         lda $25
$ffccbe 20 64 ca      jsr $ca64
$ffccc1 2c 1f c0      bit $c01f
$ffccc4 10 05         bpl $ffcccb
$ffccc6 20 0a cd      jsr $cd0a
$ffccc9 80 03         bra $ffccce
$ffcccb 20 06 cd      jsr $cd06
$ffccce a9 00         lda #$00
$ffccd0 2c 1a c0      bit $c01a
$ffccd3 30 02         bmi $ffccd7
$ffccd5 a9 14         lda #$14
$ffccd7 85 22         sta $22
$ffccd9 60            rts
$ffccda a9 01         lda #$01
$ffccdc 0c fb 04      tsb $04fb
$ffccdf 60            rts
$ffcce0 a9 01         lda #$01
$ffcce2 1c fb 04      trb $04fb
$ffcce5 60            rts
$ffcce6 ad fb 04      lda $04fb
$ffcce9 30 1a         bmi $ffcd05
$ffcceb 20 cb cc      jsr $cccb
$ffccee 20 19 cd      jsr $cd19
$ffccf1 20 fd cc      jsr $ccfd
$ffccf4 a9 fd         lda #$fd
$ffccf6 85 39         sta $39
$ffccf8 a9 1b         lda #$1b
$ffccfa 85 38         sta $38
$ffccfc 60            rts
$ffccfd a9 fd         lda #$fd
$ffccff 85 37         sta $37
$ffcd01 a9 f0         lda #$f0
$ffcd03 85 36         sta $36
$ffcd05 60            rts
$ffcd06 a9 28         lda #$28
$ffcd08 d0 02         bne $ffcd0c
$ffcd0a a9 50         lda #$50
$ffcd0c 85 21         sta $21
$ffcd0e a9 18         lda #$18
$ffcd10 85 23         sta $23
$ffcd12 64 22         stz $22
$ffcd14 64 20         stz $20
$ffcd16 a9 00         lda #$00
$ffcd18 60            rts
$ffcd19 2c 1f c0      bit $c01f
$ffcd1c 10 03         bpl $ffcd21
$ffcd1e 20 91 cc      jsr $cc91
$ffcd21 8d 0e c0      sta $c00e
$ffcd24 a9 ff         lda #$ff
$ffcd26 8d fb 04      sta $04fb
$ffcd29 8f 35 01 e1   sta $e10135
$ffcd2d 60            rts
$ffcd2e da            phx
$ffcd2f a2 17         ldx #$17
$ffcd31 8d 01 c0      sta $c001
$ffcd34 8a            txa
$ffcd35 20 64 ca      jsr $ca64
$ffcd38 a0 27         ldy #$27
$ffcd3a 84 2a         sty $2a
$ffcd3c 98            tya
$ffcd3d 4a            lsr
$ffcd3e b0 03         bcs $ffcd43
$ffcd40 2c 55 c0      bit $c055
$ffcd43 a8            tay
$ffcd44 b1 28         lda ($28),y
$ffcd46 2c 54 c0      bit $c054
$ffcd49 a4 2a         ldy $2a
$ffcd4b 91 28         sta ($28),y
$ffcd4d 88            dey
$ffcd4e 10 ea         bpl $ffcd3a
$ffcd50 ca            dex
$ffcd51 30 04         bmi $ffcd57
$ffcd53 e4 22         cpx $22
$ffcd55 b0 dd         bcs $ffcd34
$ffcd57 8d 00 c0      sta $c000
$ffcd5a 8d 0c c0      sta $c00c
$ffcd5d 80 33         bra $ffcd92
$ffcd5f da            phx
$ffcd60 a2 17         ldx #$17
$ffcd62 8a            txa
$ffcd63 20 64 ca      jsr $ca64
$ffcd66 a0 00         ldy #$00
$ffcd68 8d 01 c0      sta $c001
$ffcd6b b1 28         lda ($28),y
$ffcd6d 84 2a         sty $2a
$ffcd6f 48            pha
$ffcd70 98            tya
$ffcd71 4a            lsr
$ffcd72 b0 03         bcs $ffcd77
$ffcd74 8d 55 c0      sta $c055
$ffcd77 a8            tay
$ffcd78 68            pla
$ffcd79 91 28         sta ($28),y
$ffcd7b 8d 54 c0      sta $c054
$ffcd7e a4 2a         ldy $2a
$ffcd80 c8            iny
$ffcd81 c0 28         cpy #$28
$ffcd83 90 e6         bcc $ffcd6b
$ffcd85 20 56 cc      jsr $cc56
$ffcd88 ca            dex
$ffcd89 30 04         bmi $ffcd8f
$ffcd8b e4 22         cpx $22
$ffcd8d b0 d3         bcs $ffcd62
$ffcd8f 8d 0d c0      sta $c00d
$ffcd92 20 97 cd      jsr $cd97
$ffcd95 fa            plx
$ffcd96 60            rts
$ffcd97 a5 25         lda $25
$ffcd99 8d fb 05      sta $05fb
$ffcd9c 20 64 ca      jsr $ca64
$ffcd9f a5 20         lda $20
$ffcda1 2c 1f c0      bit $c01f
$ffcda4 10 01         bpl $ffcda7
$ffcda6 4a            lsr
$ffcda7 18            clc
$ffcda8 65 28         adc $28
$ffcdaa 85 28         sta $28
$ffcdac 60            rts
$ffcdad c9 e1         cmp #$e1
$ffcdaf 90 06         bcc $ffcdb7
$ffcdb1 c9 fb         cmp #$fb
$ffcdb3 b0 02         bcs $ffcdb7
$ffcdb5 29 df         and #$df
$ffcdb7 60            rts
$ffcdb8 5a            phy
$ffcdb9 20 78 c9      jsr $c978
$ffcdbc a5 5a         lda $5a
$ffcdbe b1 28         lda ($28),y
$ffcdc0 2c 1f c0      bit $c01f
$ffcdc3 10 15         bpl $ffcdda
$ffcdc5 8d 01 c0      sta $c001
$ffcdc8 98            tya
$ffcdc9 45 20         eor $20
$ffcdcb 6a            ror
$ffcdcc b0 04         bcs $ffcdd2
$ffcdce ad 55 c0      lda $c055
$ffcdd1 c8            iny
$ffcdd2 98            tya
$ffcdd3 4a            lsr
$ffcdd4 a8            tay
$ffcdd5 b1 28         lda ($28),y
$ffcdd7 2c 54 c0      bit $c054
$ffcdda 2c 1e c0      bit $c01e
$ffcddd 10 06         bpl $ffcde5
$ffcddf c9 20         cmp #$20
$ffcde1 b0 02         bcs $ffcde5
$ffcde3 09 40         ora #$40
$ffcde5 7a            ply
$ffcde6 60            rts
$ffcde7 48            pha
$ffcde8 20 ed cd      jsr $cded
$ffcdeb 68            pla
$ffcdec 60            rts
$ffcded 24 32         bit $32
$ffcdef 30 02         bmi $ffcdf3
$ffcdf1 29 7f         and #$7f
$ffcdf3 48            pha
$ffcdf4 20 78 c9      jsr $c978
$ffcdf7 80 01         bra $ffcdfa
$ffcdf9 48            pha
$ffcdfa 29 ff         and #$ff
$ffcdfc 30 15         bmi $ffce13
$ffcdfe ad fb 04      lda $04fb
$ffce01 6a            ror
$ffce02 68            pla
$ffce03 48            pha
$ffce04 90 0d         bcc $ffce13
$ffce06 2c 1e c0      bit $c01e
$ffce09 10 08         bpl $ffce13
$ffce0b 49 40         eor #$40
$ffce0d 89 60         bit #$60
$ffce0f f0 02         beq $ffce13
$ffce11 49 40         eor #$40
$ffce13 2c 1f c0      bit $c01f
$ffce16 10 1b         bpl $ffce33
$ffce18 8d 01 c0      sta $c001
$ffce1b 5a            phy
$ffce1c 48            pha
$ffce1d 98            tya
$ffce1e 45 20         eor $20
$ffce20 4a            lsr
$ffce21 b0 04         bcs $ffce27
$ffce23 ad 55 c0      lda $c055
$ffce26 c8            iny
$ffce27 98            tya
$ffce28 4a            lsr
$ffce29 a8            tay
$ffce2a 68            pla
$ffce2b 91 28         sta ($28),y
$ffce2d ad 54 c0      lda $c054
$ffce30 7a            ply
$ffce31 68            pla
$ffce32 60            rts
$ffce33 91 28         sta ($28),y
$ffce35 68            pla
$ffce36 60            rts
$ffce37 5a            phy
$ffce38 48            pha
$ffce39 20 b8 cd      jsr $cdb8
$ffce3c 8d 7b 06      sta $067b
$ffce3f 29 80         and #$80
$ffce41 49 ab         eor #$ab
$ffce43 20 f3 cd      jsr $cdf3
$ffce46 68            pla
$ffce47 7a            ply
$ffce48 60            rts
$ffce49 5a            phy
$ffce4a 48            pha
$ffce4b ac 7b 05      ldy $057b
$ffce4e ad 7b 06      lda $067b
$ffce51 80 f0         bra $ffce43
$ffce53 ad fb 04      lda $04fb
$ffce56 29 10         and #$10
$ffce58 d0 ee         bne $ffce48
$ffce5a 5a            phy
$ffce5b 48            pha
$ffce5c ac 7b 05      ldy $057b
$ffce5f 20 bd cd      jsr $cdbd
$ffce62 49 80         eor #$80
$ffce64 20 f9 cd      jsr $cdf9
$ffce67 80 dd         bra $ffce46
$ffce69 20 0a cd      jsr $cd0a
$ffce6c a9 ff         lda #$ff
$ffce6e 85 32         sta $32
$ffce70 ad fb 04      lda $04fb
$ffce73 29 04         and #$04
$ffce75 f0 02         beq $ffce79
$ffce77 46 32         lsr $32
$ffce79 ad 7b 07      lda $077b
$ffce7c 85 28         sta $28
$ffce7e ad fb 07      lda $07fb
$ffce81 85 29         sta $29
$ffce83 ad fb 05      lda $05fb
$ffce86 85 25         sta $25
$ffce88 60            rts
$ffce89 2c 12 c0      bit $c012
$ffce8c 10 3a         bpl $ffcec8
$ffce8e a9 06         lda #$06
$ffce90 c5 06         cmp $06
$ffce92 f0 34         beq $ffcec8
$ffce94 a2 03         ldx #$03
$ffce96 2c 11 c0      bit $c011
$ffce99 30 02         bmi $ffce9d
$ffce9b a2 0b         ldx #$0b
$ffce9d 85 06         sta $06
$ffce9f 2c 80 c0      bit $c080
$ffcea2 a5 06         lda $06
$ffcea4 c9 06         cmp #$06
$ffcea6 f0 01         beq $ffcea9
$ffcea8 e8            inx
$ffcea9 2c 81 c0      bit $c081
$ffceac 2c 81 c0      bit $c081
$ffceaf a0 00         ldy #$00
$ffceb1 a9 f8         lda #$f8
$ffceb3 85 37         sta $37
$ffceb5 84 36         sty $36
$ffceb7 b1 36         lda ($36),y
$ffceb9 91 36         sta ($36),y
$ffcebb c8            iny
$ffcebc d0 f9         bne $ffceb7
$ffcebe e6 37         inc $37
$ffcec0 d0 f5         bne $ffceb7
$ffcec2 bd 80 c0      lda $c080,x
$ffcec5 bd 80 c0      lda $c080,x
$ffcec8 60            rts
$ffcec9 48            pha
$ffceca af 35 01 e1   lda $e10135
$ffcece a8            tay
$ffcecf d0 0b         bne $ffcedc
$ffced1 68            pla
$ffced2 5a            phy
$ffced3 20 b8 cd      jsr $cdb8
$ffced6 48            pha
$ffced7 49 80         eor #$80
$ffced9 4c 43 ce      jmp $ce43
$ffcedc 68            pla
$ffcedd 20 b8 cd      jsr $cdb8
$ffcee0 48            pha
$ffcee1 8f 36 01 e1   sta $e10136
$ffcee5 98            tya
$ffcee6 c8            iny
$ffcee7 f0 0e         beq $ffcef7
$ffcee9 7a            ply
$ffceea 5a            phy
$ffceeb 30 0a         bmi $ffcef7
$ffceed ad 1e c0      lda $c01e
$ffcef0 09 7f         ora #$7f
$ffcef2 4a            lsr
$ffcef3 2f 35 01 e1   and $e10135
$ffcef7 20 f3 cd      jsr $cdf3
$ffcefa 68            pla
$ffcefb 60            rts
$ffcefc eb            xba
$ffcefd ad 36 c0      lda $c036
$ffcf00 09 80         ora #$80
$ffcf02 48            pha
$ffcf03 48            pha
$ffcf04 8d 36 c0      sta $c036
$ffcf07 eb            xba
$ffcf08 48            pha
$ffcf09 e6 4e         inc $4e
$ffcf0b d0 2a         bne $ffcf37
$ffcf0d e6 4f         inc $4f
$ffcf0f af 45 01 e1   lda $e10145
$ffcf13 3a            dec
$ffcf14 8f 45 01 e1   sta $e10145
$ffcf18 10 1d         bpl $ffcf37
$ffcf1a 20 ea c3      jsr $c3ea
$ffcf1d af 35 01 e1   lda $e10135
$ffcf21 f0 14         beq $ffcf37
$ffcf23 5a            phy
$ffcf24 20 b8 cd      jsr $cdb8
$ffcf27 48            pha
$ffcf28 af 36 01 e1   lda $e10136
$ffcf2c a8            tay
$ffcf2d 68            pla
$ffcf2e 8f 36 01 e1   sta $e10136
$ffcf32 98            tya
$ffcf33 20 f3 cd      jsr $cdf3
$ffcf36 7a            ply
$ffcf37 68            pla
$ffcf38 20 94 cf      jsr $cf94
$ffcf3b 08            php
$ffcf3c 08            php
$ffcf3d eb            xba
$ffcf3e 68            pla
$ffcf3f 83 03         sta $03,sp
$ffcf41 eb            xba
$ffcf42 28            plp
$ffcf43 10 06         bpl $ffcf4b
$ffcf45 20 f3 cd      jsr $cdf3
$ffcf48 20 74 cf      jsr $cf74
$ffcf4b eb            xba
$ffcf4c 68            pla
$ffcf4d 29 7f         and #$7f
$ffcf4f 0f 38 01 e1   ora $e10138
$ffcf53 8d 36 c0      sta $c036
$ffcf56 eb            xba
$ffcf57 28            plp
$ffcf58 60            rts
$ffcf59 20 2a c8      jsr $c82a
$ffcf5c a9 00         lda #$00
$ffcf5e 8f 35 01 e1   sta $e10135
$ffcf62 a9 f7         lda #$f7
$ffcf64 1c fb 04      trb $04fb
$ffcf67 8d 0f c0      sta $c00f
$ffcf6a 60            rts
$ffcf6b ad 00 c0      lda $c000
$ffcf6e 10 04         bpl $ffcf74
$ffcf70 8d 10 c0      sta $c010
$ffcf73 60            rts
$ffcf74 20 94 cf      jsr $cf94
$ffcf77 10 fa         bpl $ffcf73
$ffcf79 90 f0         bcc $ffcf6b
$ffcf7b 48            pha
$ffcf7c da            phx
$ffcf7d 5a            phy
$ffcf7e 18            clc
$ffcf7f fb            xce
$ffcf80 08            php
$ffcf81 38            sec
$ffcf82 20 e2 cf      jsr $cfe2
$ffcf85 f0 48         beq $ffcfcf
$ffcf87 28            plp
$ffcf88 fb            xce
$ffcf89 7a            ply
$ffcf8a fa            plx
$ffcf8b 68            pla
$ffcf8c af 5a 01 e1   lda $e1015a
$ffcf90 09 80         ora #$80
$ffcf92 38            sec
$ffcf93 60            rts
$ffcf94 48            pha
$ffcf95 da            phx
$ffcf96 5a            phy
$ffcf97 18            clc
$ffcf98 fb            xce
$ffcf99 08            php
$ffcf9a c2 30         rep #$30
$ffcf9c f4 00 00      pea $0000
$ffcf9f a2 06 06      ldx #$0606
$ffcfa2 22 00 00 e1   jsr $e10000
$ffcfa6 68            pla
$ffcfa7 d0 07         bne $ffcfb0
$ffcfa9 28            plp
$ffcfaa fb            xce
$ffcfab 7a            ply
$ffcfac fa            plx
$ffcfad 68            pla
$ffcfae 80 28         bra $ffcfd8
$ffcfb0 e2 30         sep #$30
$ffcfb2 a9 00         lda #$00
$ffcfb4 8f 45 01 e1   sta $e10145
$ffcfb8 18            clc
$ffcfb9 20 e2 cf      jsr $cfe2
$ffcfbc f0 11         beq $ffcfcf
$ffcfbe 28            plp
$ffcfbf fb            xce
$ffcfc0 af 58 01 e1   lda $e10158
$ffcfc4 c9 0a         cmp #$0a
$ffcfc6 f0 15         beq $ffcfdd
$ffcfc8 38            sec
$ffcfc9 7a            ply
$ffcfca fa            plx
$ffcfcb 68            pla
$ffcfcc e2 80         sep #$80
$ffcfce 60            rts
$ffcfcf 28            plp
$ffcfd0 fb            xce
$ffcfd1 38            sec
$ffcfd2 7a            ply
$ffcfd3 fa            plx
$ffcfd4 68            pla
$ffcfd5 c2 80         rep #$80
$ffcfd7 60            rts
$ffcfd8 2c 00 c0      bit $c000
$ffcfdb 18            clc
$ffcfdc 60            rts
$ffcfdd 20 7b cf      jsr $cf7b
$ffcfe0 80 ef         bra $ffcfd1
$ffcfe2 c2 30         rep #$30
$ffcfe4 f4 00 00      pea $0000
$ffcfe7 f4 28 04      pea $0428
$ffcfea f4 e1 00      pea $00e1
$ffcfed f4 58 01      pea $0158
$ffcff0 a2 06 0a      ldx #$0a06
$ffcff3 b0 03         bcs $ffcff8
$ffcff5 a2 06 0b      ldx #$0b06
$ffcff8 22 00 00 e1   jsr $e10000
$ffcffc 68            pla
$ffcffd 60            rts
$ffcffe 00 00         brk $00
$ffd000 6f d8 65 d7   adc $d765d8
$ffd004 f8            sed
$ffd005 dc 94 d9      jmp [$d994]
$ffd008 b1 db         lda ($db),y
$ffd00a 30 f3         bmi $ffcfff
$ffd00c d8            cld
$ffd00d df e1 db 8f   cmp $8fdbe1,x
$ffd011 f3 98         sbc ($98,sp),y
$ffd013 f3 e4         sbc ($e4,sp),y
$ffd015 f1 dd         sbc ($dd),y
$ffd017 f1 d4         sbc ($d4),y
$ffd019 f1 24         sbc ($24),y
$ffd01b f2 31         sbc ($31)
$ffd01d f2 40         sbc ($40)
$ffd01f f2 d7         sbc ($d7)
$ffd021 f3 e1         sbc ($e1,sp),y
$ffd023 f3 e8         sbc ($e8,sp),y
$ffd025 f6 fd         inc $fd,x
$ffd027 f6 68         inc $68,x
$ffd029 f7 6e         sbc [$6e],y
$ffd02b f7 e6         sbc [$e6],y
$ffd02d f7 57         sbc [$57],y
$ffd02f fc 20 f7      jsr ($f720,x)
$ffd032 26 f7         rol $f7
$ffd034 f4 03 6c      pea $6c03
$ffd037 f2 6e         sbc ($6e)
$ffd039 f2 72         sbc ($72)
$ffd03b f2 76         sbc ($76)
$ffd03d f2 7f         sbc ($7f)
$ffd03f f2 4e         sbc ($4e)
$ffd041 f2 6a         sbc ($6a)
$ffd043 d9 55 f2      cmp $f255,y
$ffd046 85 f2         sta $f2
$ffd048 a5 f2         lda $f2
$ffd04a ca            dex
$ffd04b f2 17         sbc ($17)
$ffd04d f3 f4         sbc ($f4,sp),y
$ffd04f 03 f4         ora $f4,sp
$ffd051 03 61         ora $61,sp
$ffd053 f2 45         sbc ($45)
$ffd055 da            phx
$ffd056 3d d9 11      and $11d9,x
$ffd059 d9 c8 d9      cmp $d9c8,y
$ffd05c 48            pha
$ffd05d d8            cld
$ffd05e f4 03 20      pea $2003
$ffd061 d9 6a d9      cmp $d96a,y
$ffd064 db            stp
$ffd065 d9 6d d8      cmp $d86d,y
$ffd068 eb            xba
$ffd069 d9 83 e7      cmp $e783,y
$ffd06c f4 03 f4      pea $f403
$ffd06f 03 12         ora $12,sp
$ffd071 e3 7a         sbc $7a,sp
$ffd073 e7 d4         sbc [$d4]
$ffd075 da            phx
$ffd076 95 d8         sta $d8,x
$ffd078 a4 d6         ldy $d6
$ffd07a 69 d6 9f      adc #$9fd6
$ffd07d db            stp
$ffd07e 48            pha
$ffd07f d6 90         dec $90,x
$ffd081 eb            xba
$ffd082 23 ec         and $ec,sp
$ffd084 af eb 0a 00   lda $000aeb
$ffd088 de e2 12      dec $12e2,x
$ffd08b d4 cd         pei ($cd)
$ffd08d df ff e2 8d   cmp $8de2ff,x
$ffd091 ee ae ef      inc $efae
$ffd094 41 e9         eor ($e9,x)
$ffd096 09 ef ea      ora #$eaef
$ffd099 ef f1 ef 3a   sbc $3aeff1
$ffd09d f0 9e         beq $ffd03d
$ffd09f f0 64         beq $ffd105
$ffd0a1 e7 d6         sbc [$d6]
$ffd0a3 e6 c5         inc $c5
$ffd0a5 e3 07         sbc $07,sp
$ffd0a7 e7 e5         sbc [$e5]
$ffd0a9 e6 46         inc $46
$ffd0ab e6 5a         inc $5a
$ffd0ad e6 86         inc $86
$ffd0af e6 91         inc $91
$ffd0b1 e6 79         inc $79
$ffd0b3 c0 e7 79      cpy #$79e7
$ffd0b6 a9 e7 7b      lda #$7be7
$ffd0b9 81 e9         sta ($e9,x)
$ffd0bb 7b            tdc
$ffd0bc 68            pla
$ffd0bd ea            nop
$ffd0be 7d 96 ee      adc $ee96,x
$ffd0c1 50 54         bvc $ffd117
$ffd0c3 df 46 4e df   cmp $df4e46,x
$ffd0c7 7f cf ee 7f   adc $7feecf,x
$ffd0cb 97 de         sta [$de],y
$ffd0cd 64 64         stz $64
$ffd0cf df 45 4e c4   cmp $c44e45,x
$ffd0d3 46 4f         lsr $4f
$ffd0d5 d2 4e         cmp ($4e)
$ffd0d7 45 58         eor $58
$ffd0d9 d4 44         pei ($44)
$ffd0db 41 54         eor ($54,x)
$ffd0dd c1 49         cmp ($49,x)
$ffd0df 4e 50 55      lsr $5550
$ffd0e2 d4 44         pei ($44)
$ffd0e4 45 cc         eor $cc
$ffd0e6 44 49 cd      mvp $49,$cd
$ffd0e9 52 45         eor ($45)
$ffd0eb 41 c4         eor ($c4,x)
$ffd0ed 47 d2         eor [$d2]
$ffd0ef 54 45 58      mvn $45,$58
$ffd0f2 d4 50         pei ($50)
$ffd0f4 52 a3         eor ($a3)
$ffd0f6 49 4e a3      eor #$a34e
$ffd0f9 43 41         eor $41,sp
$ffd0fb 4c cc 50      jmp $50cc
$ffd0fe 4c 4f d4      jmp $d44f
$ffd101 48            pha
$ffd102 4c 49 ce      jmp $ce49
$ffd105 56 4c         lsr $4c,x
$ffd107 49 ce 48      eor #$48ce
$ffd10a 47 52         eor [$52]
$ffd10c b2 48         lda ($48)
$ffd10e 47 d2         eor [$d2]
$ffd110 48            pha
$ffd111 43 4f         eor $4f,sp
$ffd113 4c 4f 52      jmp $524f
$ffd116 bd 48 50      lda $5048,x
$ffd119 4c 4f d4      jmp $d44f
$ffd11c 44 52 41      mvp $52,$41
$ffd11f d7 58         cmp [$58],y
$ffd121 44 52 41      mvp $52,$41
$ffd124 d7 48         cmp [$48],y
$ffd126 54 41 c2      mvn $41,$c2
$ffd129 48            pha
$ffd12a 4f 4d c5 52   eor $52c54d
$ffd12e 4f 54 bd 53   eor $53bd54
$ffd132 43 41         eor $41,sp
$ffd134 4c 45 bd      jmp $bd45
$ffd137 53 48         eor ($48,sp),y
$ffd139 4c 4f 41      jmp $414f
$ffd13c c4 54         cpy $54
$ffd13e 52 41         eor ($41)
$ffd140 43 c5         eor $c5,sp
$ffd142 4e 4f 54      lsr $544f
$ffd145 52 41         eor ($41)
$ffd147 43 c5         eor $c5,sp
$ffd149 4e 4f 52      lsr $524f
$ffd14c 4d 41 cc      eor $cc41
$ffd14f 49 4e 56      eor #$564e
$ffd152 45 52         eor $52
$ffd154 53 c5         eor ($c5,sp),y
$ffd156 46 4c         lsr $4c
$ffd158 41 53         eor ($53,x)
$ffd15a c8            iny
$ffd15b 43 4f         eor $4f,sp
$ffd15d 4c 4f 52      jmp $524f
$ffd160 bd 50 4f      lda $4f50,x
$ffd163 d0 56         bne $ffd1bb
$ffd165 54 41 c2      mvn $41,$c2
$ffd168 48            pha
$ffd169 49 4d 45      eor #$454d
$ffd16c 4d ba 4c      eor $4cba
$ffd16f 4f 4d 45 4d   eor $4d454d
$ffd173 ba            tsx
$ffd174 4f 4e 45 52   eor $52454e
$ffd178 d2 52         cmp ($52)
$ffd17a 45 53         eor $53
$ffd17c 55 4d         eor $4d,x
$ffd17e c5 52         cmp $52
$ffd180 45 43         eor $43
$ffd182 41 4c         eor ($4c,x)
$ffd184 cc 53 54      cpy $5453
$ffd187 4f 52 c5 53   eor $53c552
$ffd18b 50 45         bvc $ffd1d2
$ffd18d 45 44         eor $44
$ffd18f bd 4c 45      lda $454c,x
$ffd192 d4 47         pei ($47)
$ffd194 4f 54 cf 52   eor $52cf54
$ffd198 55 ce         eor $ce,x
$ffd19a 49 c6 52      eor #$52c6
$ffd19d 45 53         eor $53
$ffd19f 54 4f 52      mvn $4f,$52
$ffd1a2 c5 a6         cmp $a6
$ffd1a4 47 4f         eor [$4f]
$ffd1a6 53 55         eor ($55,sp),y
$ffd1a8 c2 52         rep #$52
$ffd1aa 45 54         eor $54
$ffd1ac 55 52         eor $52,x
$ffd1ae ce 52 45      dec $4552
$ffd1b1 cd 53 54      cmp $5453
$ffd1b4 4f d0 4f ce   eor $ce4fd0
$ffd1b8 57 41         eor [$41],y
$ffd1ba 49 d4 4c      eor #$4cd4
$ffd1bd 4f 41 c4 53   eor $53c441
$ffd1c1 41 56         eor ($56,x)
$ffd1c3 c5 44         cmp $44
$ffd1c5 45 c6         eor $c6
$ffd1c7 50 4f         bvc $ffd218
$ffd1c9 4b            phk
$ffd1ca c5 50         cmp $50
$ffd1cc 52 49         eor ($49)
$ffd1ce 4e d4 43      lsr $43d4
$ffd1d1 4f 4e d4 4c   eor $4cd44e
$ffd1d5 49 53 d4      eor #$d453
$ffd1d8 43 4c         eor $4c,sp
$ffd1da 45 41         eor $41
$ffd1dc d2 47         cmp ($47)
$ffd1de 45 d4         eor $d4
$ffd1e0 4e 45 d7      lsr $d745
$ffd1e3 54 41 42      mvn $41,$42
$ffd1e6 a8            tay
$ffd1e7 54 cf 46      mvn $cf,$46
$ffd1ea ce 53 50      dec $5053
$ffd1ed 43 a8         eor $a8,sp
$ffd1ef 54 48 45      mvn $48,$45
$ffd1f2 ce 41 d4      dec $d441
$ffd1f5 4e 4f d4      lsr $d44f
$ffd1f8 53 54         eor ($54,sp),y
$ffd1fa 45 d0         eor $d0
$ffd1fc ab            plb
$ffd1fd ad aa af      lda $afaa
$ffd200 de 41 4e      dec $4e41,x
$ffd203 c4 4f         cpy $4f
$ffd205 d2 be         cmp ($be)
$ffd207 bd bc 53      lda $53bc,x
$ffd20a 47 ce         eor [$ce]
$ffd20c 49 4e d4      eor #$d44e
$ffd20f 41 42         eor ($42,x)
$ffd211 d3 55         cmp ($55,sp),y
$ffd213 53 d2         eor ($d2,sp),y
$ffd215 46 52         lsr $52
$ffd217 c5 53         cmp $53
$ffd219 43 52         eor $52,sp
$ffd21b 4e a8 50      lsr $50a8
$ffd21e 44 cc 50      mvp $cc,$50
$ffd221 4f d3 53 51   eor $5153d3
$ffd225 d2 52         cmp ($52)
$ffd227 4e c4 4c      lsr $4cc4
$ffd22a 4f c7 45 58   eor $5845c7
$ffd22e d0 43         bne $ffd273
$ffd230 4f d3 53 49   eor $4953d3
$ffd234 ce 54 41      dec $4154
$ffd237 ce 41 54      dec $5441
$ffd23a ce 50 45      dec $4550
$ffd23d 45 cb         eor $cb
$ffd23f 4c 45 ce      jmp $ce45
$ffd242 53 54         eor ($54,sp),y
$ffd244 52 a4         eor ($a4)
$ffd246 56 41         lsr $41,x
$ffd248 cc 41 53      cpy $5341
$ffd24b c3 43         cmp $43,sp
$ffd24d 48            pha
$ffd24e 52 a4         eor ($a4)
$ffd250 4c 45 46      jmp $4645
$ffd253 54 a4 52      mvn $a4,$52
$ffd256 49 47 48      eor #$4847
$ffd259 54 a4 4d      mvn $a4,$4d
$ffd25c 49 44 a4      eor #$a444
$ffd25f 00 4e         brk $4e
$ffd261 45 58         eor $58
$ffd263 54 20 57      mvn $20,$57
$ffd266 49 54 48      eor #$4854
$ffd269 4f 55 54 20   eor $205455
$ffd26d 46 4f         lsr $4f
$ffd26f d2 53         cmp ($53)
$ffd271 59 4e 54      eor $544e,y
$ffd274 41 d8         eor ($d8,x)
$ffd276 52 45         eor ($45)
$ffd278 54 55 52      mvn $55,$52
$ffd27b 4e 20 57      lsr $5720
$ffd27e 49 54 48      eor #$4854
$ffd281 4f 55 54 20   eor $205455
$ffd285 47 4f         eor [$4f]
$ffd287 53 55         eor ($55,sp),y
$ffd289 c2 4f         rep #$4f
$ffd28b 55 54         eor $54,x
$ffd28d 20 4f 46      jsr $464f
$ffd290 20 44 41      jsr $4144
$ffd293 54 c1 49      mvn $c1,$49
$ffd296 4c 4c 45      jmp $454c
$ffd299 47 41         eor [$41]
$ffd29b 4c 20 51      jmp $5120
$ffd29e 55 41         eor $41,x
$ffd2a0 4e 54 49      lsr $4954
$ffd2a3 54 d9 4f      mvn $d9,$4f
$ffd2a6 56 45         lsr $45,x
$ffd2a8 52 46         eor ($46)
$ffd2aa 4c 4f d7      jmp $d74f
$ffd2ad 4f 55 54 20   eor $205455
$ffd2b1 4f 46 20 4d   eor $4d2046
$ffd2b5 45 4d         eor $4d
$ffd2b7 4f 52 d9 55   eor $55d952
$ffd2bb 4e 44 45      lsr $4544
$ffd2be 46 27         lsr $27
$ffd2c0 44 20 53      mvp $20,$53
$ffd2c3 54 41 54      mvn $41,$54
$ffd2c6 45 4d         eor $4d
$ffd2c8 45 4e         eor $4e
$ffd2ca d4 42         pei ($42)
$ffd2cc 41 44         eor ($44,x)
$ffd2ce 20 53 55      jsr $5553
$ffd2d1 42            ???
$ffd2d2 53 43         eor ($43,sp),y
$ffd2d4 52 49         eor ($49)
$ffd2d6 50 d4         bvc $ffd2ac
$ffd2d8 52 45         eor ($45)
$ffd2da 44 49 4d      mvp $49,$4d
$ffd2dd 27 44         and [$44]
$ffd2df 20 41 52      jsr $5241
$ffd2e2 52 41         eor ($41)
$ffd2e4 d9 44 49      cmp $4944,y
$ffd2e7 56 49         lsr $49,x
$ffd2e9 53 49         eor ($49,sp),y
$ffd2eb 4f 4e 20 42   eor $42204e
$ffd2ef 59 20 5a      eor $5a20,y
$ffd2f2 45 52         eor $52
$ffd2f4 cf 49 4c 4c   cmp $4c4c49
$ffd2f8 45 47         eor $47
$ffd2fa 41 4c         eor ($4c,x)
$ffd2fc 20 44 49      jsr $4944
$ffd2ff 52 45         eor ($45)
$ffd301 43 d4         eor $d4,sp
$ffd303 54 59 50      mvn $59,$50
$ffd306 45 20         eor $20
$ffd308 4d 49 53      eor $5349
$ffd30b 4d 41 54      eor $5441
$ffd30e 43 c8         eor $c8,sp
$ffd310 53 54         eor ($54,sp),y
$ffd312 52 49         eor ($49)
$ffd314 4e 47 20      lsr $2047
$ffd317 54 4f 4f      mvn $4f,$4f
$ffd31a 20 4c 4f      jsr $4f4c
$ffd31d 4e c7 46      lsr $46c7
$ffd320 4f 52 4d 55   eor $554d52
$ffd324 4c 41 20      jmp $2041
$ffd327 54 4f 4f      mvn $4f,$4f
$ffd32a 20 43 4f      jsr $4f43
$ffd32d 4d 50 4c      eor $4c50
$ffd330 45 d8         eor $d8
$ffd332 43 41         eor $41,sp
$ffd334 4e 27 54      lsr $5427
$ffd337 20 43 4f      jsr $4f43
$ffd33a 4e 54 49      lsr $4954
$ffd33d 4e 55 c5      lsr $c555
$ffd340 55 4e         eor $4e,x
$ffd342 44 45 46      mvp $45,$46
$ffd345 27 44         and [$44]
$ffd347 20 46 55      jsr $5546
$ffd34a 4e 43 54      lsr $5443
$ffd34d 49 4f ce      eor #$ce4f
$ffd350 20 45 52      jsr $5245
$ffd353 52 4f         eor ($4f)
$ffd355 52 07         eor ($07)
$ffd357 00 20         brk $20
$ffd359 49 4e 20      eor #$204e
$ffd35c 00 0d         brk $0d
$ffd35e 42            ???
$ffd35f 52 45         eor ($45)
$ffd361 41 4b         eor ($4b,x)
$ffd363 07 00         ora [$00]
$ffd365 ba            tsx
$ffd366 e8            inx
$ffd367 e8            inx
$ffd368 e8            inx
$ffd369 e8            inx
$ffd36a bd 01 01      lda $0101,x
$ffd36d c9 81 d0      cmp #$d081
$ffd370 21 a5         and ($a5,x)
$ffd372 86 d0         stx $d0
$ffd374 0a            asl
$ffd375 bd 02 01      lda $0102,x
$ffd378 85 85         sta $85
$ffd37a bd 03 01      lda $0103,x
$ffd37d 85 86         sta $86
$ffd37f dd 03 01      cmp $0103,x
$ffd382 d0 07         bne $ffd38b
$ffd384 a5 85         lda $85
$ffd386 dd 02 01      cmp $0102,x
$ffd389 f0 07         beq $ffd392
$ffd38b 8a            txa
$ffd38c 18            clc
$ffd38d 69 12 aa      adc #$aa12
$ffd390 d0 d8         bne $ffd36a
$ffd392 60            rts
$ffd393 20 e3 d3      jsr $d3e3
$ffd396 85 6d         sta $6d
$ffd398 84 6e         sty $6e
$ffd39a 38            sec
$ffd39b a5 96         lda $96
$ffd39d e5 9b         sbc $9b
$ffd39f 85 5e         sta $5e
$ffd3a1 a8            tay
$ffd3a2 a5 97         lda $97
$ffd3a4 e5 9c         sbc $9c
$ffd3a6 aa            tax
$ffd3a7 e8            inx
$ffd3a8 98            tya
$ffd3a9 f0 23         beq $ffd3ce
$ffd3ab a5 96         lda $96
$ffd3ad 38            sec
$ffd3ae e5 5e         sbc $5e
$ffd3b0 85 96         sta $96
$ffd3b2 b0 03         bcs $ffd3b7
$ffd3b4 c6 97         dec $97
$ffd3b6 38            sec
$ffd3b7 a5 94         lda $94
$ffd3b9 e5 5e         sbc $5e
$ffd3bb 85 94         sta $94
$ffd3bd b0 08         bcs $ffd3c7
$ffd3bf c6 95         dec $95
$ffd3c1 90 04         bcc $ffd3c7
$ffd3c3 b1 96         lda ($96),y
$ffd3c5 91 94         sta ($94),y
$ffd3c7 88            dey
$ffd3c8 d0 f9         bne $ffd3c3
$ffd3ca b1 96         lda ($96),y
$ffd3cc 91 94         sta ($94),y
$ffd3ce c6 97         dec $97
$ffd3d0 c6 95         dec $95
$ffd3d2 ca            dex
$ffd3d3 d0 f2         bne $ffd3c7
$ffd3d5 60            rts
$ffd3d6 0a            asl
$ffd3d7 69 36 b0      adc #$b036
$ffd3da 35 85         and $85,x
$ffd3dc 5e ba e4      lsr $e4ba,x
$ffd3df 5e 90 2e      lsr $2e90,x
$ffd3e2 60            rts
$ffd3e3 c4 70         cpy $70
$ffd3e5 90 28         bcc $ffd40f
$ffd3e7 d0 04         bne $ffd3ed
$ffd3e9 c5 6f         cmp $6f
$ffd3eb 90 22         bcc $ffd40f
$ffd3ed 48            pha
$ffd3ee a2 09 98      ldx #$9809
$ffd3f1 48            pha
$ffd3f2 b5 93         lda $93,x
$ffd3f4 ca            dex
$ffd3f5 10 fa         bpl $ffd3f1
$ffd3f7 20 84 e4      jsr $e484
$ffd3fa a2 f7 68      ldx #$68f7
$ffd3fd 95 9d         sta $9d,x
$ffd3ff e8            inx
$ffd400 30 fa         bmi $ffd3fc
$ffd402 68            pla
$ffd403 a8            tay
$ffd404 68            pla
$ffd405 c4 70         cpy $70
$ffd407 90 06         bcc $ffd40f
$ffd409 d0 05         bne $ffd410
$ffd40b c5 6f         cmp $6f
$ffd40d b0 01         bcs $ffd410
$ffd40f 60            rts
$ffd410 a2 4d 24      ldx #$244d
$ffd413 d8            cld
$ffd414 10 03         bpl $ffd419
$ffd416 4c e9 f2      jmp $f2e9
$ffd419 20 fb da      jsr $dafb
$ffd41c 20 5a db      jsr $db5a
$ffd41f bd 60 d2      lda $d260,x
$ffd422 48            pha
$ffd423 20 5c db      jsr $db5c
$ffd426 e8            inx
$ffd427 68            pla
$ffd428 10 f5         bpl $ffd41f
$ffd42a 20 83 d6      jsr $d683
$ffd42d a9 50 a0      lda #$a050
$ffd430 d3 20         cmp ($20,sp),y
$ffd432 3a            dec
$ffd433 db            stp
$ffd434 a4 76         ldy $76
$ffd436 c8            iny
$ffd437 f0 03         beq $ffd43c
$ffd439 20 19 ed      jsr $ed19
$ffd43c 20 fb da      jsr $dafb
$ffd43f a2 dd 20      ldx #$20dd
$ffd442 2e d5 86      rol $86d5
$ffd445 b8            clv
$ffd446 84 b9         sty $b9
$ffd448 46 d8         lsr $d8
$ffd44a 20 b1 00      jsr $00b1
$ffd44d aa            tax
$ffd44e f0 ec         beq $ffd43c
$ffd450 a2 ff 86      ldx #$86ff
$ffd453 76 90         ror $90,x
$ffd455 06 20         asl $20
$ffd457 59 d5 4c      eor $4cd5,y
$ffd45a 05 d8         ora $d8
$ffd45c a6 af         ldx $af
$ffd45e 86 69         stx $69
$ffd460 a6 b0         ldx $b0
$ffd462 86 6a         stx $6a
$ffd464 20 0c da      jsr $da0c
$ffd467 20 59 d5      jsr $d559
$ffd46a 84 0f         sty $0f
$ffd46c 20 1a d6      jsr $d61a
$ffd46f 90 44         bcc $ffd4b5
$ffd471 a0 01 b1      ldy #$b101
$ffd474 9b            txy
$ffd475 85 5f         sta $5f
$ffd477 a5 69         lda $69
$ffd479 85 5e         sta $5e
$ffd47b a5 9c         lda $9c
$ffd47d 85 61         sta $61
$ffd47f a5 9b         lda $9b
$ffd481 88            dey
$ffd482 f1 9b         sbc ($9b),y
$ffd484 18            clc
$ffd485 65 69         adc $69
$ffd487 85 69         sta $69
$ffd489 85 60         sta $60
$ffd48b a5 6a         lda $6a
$ffd48d 69 ff 85      adc #$85ff
$ffd490 6a            ror
$ffd491 e5 9c         sbc $9c
$ffd493 aa            tax
$ffd494 38            sec
$ffd495 a5 9b         lda $9b
$ffd497 e5 69         sbc $69
$ffd499 a8            tay
$ffd49a b0 03         bcs $ffd49f
$ffd49c e8            inx
$ffd49d c6 61         dec $61
$ffd49f 18            clc
$ffd4a0 65 5e         adc $5e
$ffd4a2 90 03         bcc $ffd4a7
$ffd4a4 c6 5f         dec $5f
$ffd4a6 18            clc
$ffd4a7 b1 5e         lda ($5e),y
$ffd4a9 91 60         sta ($60),y
$ffd4ab c8            iny
$ffd4ac d0 f9         bne $ffd4a7
$ffd4ae e6 5f         inc $5f
$ffd4b0 e6 61         inc $61
$ffd4b2 ca            dex
$ffd4b3 d0 f2         bne $ffd4a7
$ffd4b5 ad 00 02      lda $0200
$ffd4b8 f0 38         beq $ffd4f2
$ffd4ba a5 73         lda $73
$ffd4bc a4 74         ldy $74
$ffd4be 85 6f         sta $6f
$ffd4c0 84 70         sty $70
$ffd4c2 a5 69         lda $69
$ffd4c4 85 96         sta $96
$ffd4c6 65 0f         adc $0f
$ffd4c8 85 94         sta $94
$ffd4ca a4 6a         ldy $6a
$ffd4cc 84 97         sty $97
$ffd4ce 90 01         bcc $ffd4d1
$ffd4d0 c8            iny
$ffd4d1 84 95         sty $95
$ffd4d3 20 93 d3      jsr $d393
$ffd4d6 a5 50         lda $50
$ffd4d8 a4 51         ldy $51
$ffd4da 8d fe 01      sta $01fe
$ffd4dd 8c ff 01      sty $01ff
$ffd4e0 a5 6d         lda $6d
$ffd4e2 a4 6e         ldy $6e
$ffd4e4 85 69         sta $69
$ffd4e6 84 6a         sty $6a
$ffd4e8 a4 0f         ldy $0f
$ffd4ea b9 fb 01      lda $01fb,y
$ffd4ed 88            dey
$ffd4ee 91 9b         sta ($9b),y
$ffd4f0 d0 f8         bne $ffd4ea
$ffd4f2 20 65 d6      jsr $d665
$ffd4f5 a5 67         lda $67
$ffd4f7 a4 68         ldy $68
$ffd4f9 85 5e         sta $5e
$ffd4fb 84 5f         sty $5f
$ffd4fd 18            clc
$ffd4fe a0 01 b1      ldy #$b101
$ffd501 5e d0 0b      lsr $0bd0,x
$ffd504 a5 69         lda $69
$ffd506 85 af         sta $af
$ffd508 a5 6a         lda $6a
$ffd50a 85 b0         sta $b0
$ffd50c 4c 3c d4      jmp $d43c
$ffd50f a0 04 c8      ldy #$c804
$ffd512 b1 5e         lda ($5e),y
$ffd514 d0 fb         bne $ffd511
$ffd516 c8            iny
$ffd517 98            tya
$ffd518 65 5e         adc $5e
$ffd51a aa            tax
$ffd51b a0 00 91      ldy #$9100
$ffd51e 5e a5 5f      lsr $5fa5,x
$ffd521 69 00 c8      adc #$c800
$ffd524 91 5e         sta ($5e),y
$ffd526 86 5e         stx $5e
$ffd528 85 5f         sta $5f
$ffd52a 90 d2         bcc $ffd4fe
$ffd52c a2 80 86      ldx #$8680
$ffd52f 33 20         and ($20,sp),y
$ffd531 6a            ror
$ffd532 fd e0 ef      sbc $efe0,x
$ffd535 90 02         bcc $ffd539
$ffd537 a2 ef a9      ldx #$a9ef
$ffd53a 00 9d         brk $9d
$ffd53c 00 02         brk $02
$ffd53e 8a            txa
$ffd53f f0 0b         beq $ffd54c
$ffd541 bd ff 01      lda $01ff,x
$ffd544 29 7f 9d      and #$9d7f
$ffd547 ff 01 ca d0   sbc $d0ca01,x
$ffd54b f5 a9         sbc $a9,x
$ffd54d 00 a2         brk $a2
$ffd54f ff a0 01 60   sbc $6001a0,x
$ffd553 20 0c fd      jsr $fd0c
$ffd556 29 7f 60      and #$607f
$ffd559 a6 b8         ldx $b8
$ffd55b ca            dex
$ffd55c a0 04 84      ldy #$8404
$ffd55f 13 24         ora ($24,sp),y
$ffd561 d6 10         dec $10,x
$ffd563 08            php
$ffd564 68            pla
$ffd565 68            pla
$ffd566 20 65 d6      jsr $d665
$ffd569 4c d2 d7      jmp $d7d2
$ffd56c e8            inx
$ffd56d 20 b5 d8      jsr $d8b5
$ffd570 24 13         bit $13
$ffd572 70 04         bvs $ffd578
$ffd574 c9 20 f0      cmp #$f020
$ffd577 f4 85 0e      pea $0e85
$ffd57a c9 22 f0      cmp #$f022
$ffd57d 74 70         stz $70,x
$ffd57f 4d c9 3f      eor $3fc9
$ffd582 d0 04         bne $ffd588
$ffd584 a9 ba d0      lda #$d0ba
$ffd587 45 c9         eor $c9
$ffd589 30 90         bmi $ffd51b
$ffd58b 04 c9         tsb $c9
$ffd58d 3c 90 3d      bit $3d90,x
$ffd590 84 ad         sty $ad
$ffd592 a9 d0 85      lda #$85d0
$ffd595 9d a9 cf      sta $cfa9,x
$ffd598 85 9e         sta $9e
$ffd59a a0 00 84      ldy #$8400
$ffd59d 0f 88 86 b8   ora $b88688
$ffd5a1 ca            dex
$ffd5a2 c8            iny
$ffd5a3 d0 02         bne $ffd5a7
$ffd5a5 e6 9e         inc $9e
$ffd5a7 e8            inx
$ffd5a8 20 b5 d8      jsr $d8b5
$ffd5ab c9 20 f0      cmp #$f020
$ffd5ae f8            sed
$ffd5af 38            sec
$ffd5b0 f1 9d         sbc ($9d),y
$ffd5b2 f0 ee         beq $ffd5a2
$ffd5b4 c9 80 d0      cmp #$d080
$ffd5b7 41 05         eor ($05,x)
$ffd5b9 0f c9 c5 d0   ora $d0c5c9
$ffd5bd 0d 20 b0      ora $b020
$ffd5c0 d8            cld
$ffd5c1 c9 4e f0      cmp #$f04e
$ffd5c4 34 c9         bit $c9,x
$ffd5c6 4f f0 30 a9   eor $a930f0
$ffd5ca c5 a4         cmp $a4
$ffd5cc ad e8 c8      lda $c8e8
$ffd5cf 99 fb 01      sta $01fb,y
$ffd5d2 b9 fb 01      lda $01fb,y
$ffd5d5 f0 39         beq $ffd610
$ffd5d7 38            sec
$ffd5d8 e9 3a f0      sbc #$f03a
$ffd5db 04 c9         tsb $c9
$ffd5dd 49 d0 02      eor #$02d0
$ffd5e0 85 13         sta $13
$ffd5e2 38            sec
$ffd5e3 e9 78 d0      sbc #$d078
$ffd5e6 86 85         stx $85
$ffd5e8 0e 20 b5      asl $b520
$ffd5eb d8            cld
$ffd5ec f0 df         beq $ffd5cd
$ffd5ee c5 0e         cmp $0e
$ffd5f0 f0 db         beq $ffd5cd
$ffd5f2 c8            iny
$ffd5f3 99 fb 01      sta $01fb,y
$ffd5f6 e8            inx
$ffd5f7 d0 f0         bne $ffd5e9
$ffd5f9 a6 b8         ldx $b8
$ffd5fb e6 0f         inc $0f
$ffd5fd b1 9d         lda ($9d),y
$ffd5ff c8            iny
$ffd600 d0 02         bne $ffd604
$ffd602 e6 9e         inc $9e
$ffd604 0a            asl
$ffd605 90 f6         bcc $ffd5fd
$ffd607 b1 9d         lda ($9d),y
$ffd609 d0 9d         bne $ffd5a8
$ffd60b 20 c3 d8      jsr $d8c3
$ffd60e 10 bb         bpl $ffd5cb
$ffd610 99 fd 01      sta $01fd,y
$ffd613 c6 b9         dec $b9
$ffd615 a9 ff 85      lda #$85ff
$ffd618 b8            clv
$ffd619 60            rts
$ffd61a a5 67         lda $67
$ffd61c a6 68         ldx $68
$ffd61e a0 01 85      ldy #$8501
$ffd621 9b            txy
$ffd622 86 9c         stx $9c
$ffd624 b1 9b         lda ($9b),y
$ffd626 f0 1f         beq $ffd647
$ffd628 c8            iny
$ffd629 c8            iny
$ffd62a a5 51         lda $51
$ffd62c d1 9b         cmp ($9b),y
$ffd62e 90 18         bcc $ffd648
$ffd630 f0 03         beq $ffd635
$ffd632 88            dey
$ffd633 d0 09         bne $ffd63e
$ffd635 a5 50         lda $50
$ffd637 88            dey
$ffd638 d1 9b         cmp ($9b),y
$ffd63a 90 0c         bcc $ffd648
$ffd63c f0 0a         beq $ffd648
$ffd63e 88            dey
$ffd63f b1 9b         lda ($9b),y
$ffd641 aa            tax
$ffd642 88            dey
$ffd643 b1 9b         lda ($9b),y
$ffd645 b0 d7         bcs $ffd61e
$ffd647 18            clc
$ffd648 60            rts
$ffd649 d0 fd         bne $ffd648
$ffd64b a9 00 85      lda #$8500
$ffd64e d6 a8         dec $a8,x
$ffd650 91 67         sta ($67),y
$ffd652 c8            iny
$ffd653 91 67         sta ($67),y
$ffd655 a5 67         lda $67
$ffd657 69 02 85      adc #$8502
$ffd65a 69 85 af      adc #$af85
$ffd65d a5 68         lda $68
$ffd65f 69 00 85      adc #$8500
$ffd662 6a            ror
$ffd663 85 b0         sta $b0
$ffd665 20 97 d6      jsr $d697
$ffd668 a9 00 d0      lda #$d000
$ffd66b 2a            rol
$ffd66c a5 73         lda $73
$ffd66e a4 74         ldy $74
$ffd670 85 6f         sta $6f
$ffd672 84 70         sty $70
$ffd674 a5 69         lda $69
$ffd676 a4 6a         ldy $6a
$ffd678 85 6b         sta $6b
$ffd67a 84 6c         sty $6c
$ffd67c 85 6d         sta $6d
$ffd67e 84 6e         sty $6e
$ffd680 20 49 d8      jsr $d849
$ffd683 a2 55 86      ldx #$8655
$ffd686 52 68         eor ($68)
$ffd688 a8            tay
$ffd689 68            pla
$ffd68a a2 f8 9a      ldx #$9af8
$ffd68d 48            pha
$ffd68e 98            tya
$ffd68f 48            pha
$ffd690 a9 00 85      lda #$8500
$ffd693 7a            ply
$ffd694 85 14         sta $14
$ffd696 60            rts
$ffd697 18            clc
$ffd698 a5 67         lda $67
$ffd69a 69 ff 85      adc #$85ff
$ffd69d b8            clv
$ffd69e a5 68         lda $68
$ffd6a0 69 ff 85      adc #$85ff
$ffd6a3 b9 60 90      lda $9060,y
$ffd6a6 0a            asl
$ffd6a7 f0 08         beq $ffd6b1
$ffd6a9 c9 c9 f0      cmp #$f0c9
$ffd6ac 04 c9         tsb $c9
$ffd6ae 2c d0 e5      bit $e5d0
$ffd6b1 20 0c da      jsr $da0c
$ffd6b4 20 1a d6      jsr $d61a
$ffd6b7 20 b7 00      jsr $00b7
$ffd6ba f0 10         beq $ffd6cc
$ffd6bc c9 c9 f0      cmp #$f0c9
$ffd6bf 04 c9         tsb $c9
$ffd6c1 2c d0 84      bit $84d0
$ffd6c4 20 b1 00      jsr $00b1
$ffd6c7 20 0c da      jsr $da0c
$ffd6ca d0 ca         bne $ffd696
$ffd6cc 68            pla
$ffd6cd 68            pla
$ffd6ce a5 50         lda $50
$ffd6d0 05 51         ora $51
$ffd6d2 d0 06         bne $ffd6da
$ffd6d4 a9 ff 85      lda #$85ff
$ffd6d7 50 85         bvc $ffd65e
$ffd6d9 51 a0         eor ($a0),y
$ffd6db 01 b1         ora ($b1,x)
$ffd6dd 9b            txy
$ffd6de f0 44         beq $ffd724
$ffd6e0 20 58 d8      jsr $d858
$ffd6e3 20 fb da      jsr $dafb
$ffd6e6 c8            iny
$ffd6e7 b1 9b         lda ($9b),y
$ffd6e9 aa            tax
$ffd6ea c8            iny
$ffd6eb b1 9b         lda ($9b),y
$ffd6ed c5 51         cmp $51
$ffd6ef d0 04         bne $ffd6f5
$ffd6f1 e4 50         cpx $50
$ffd6f3 f0 02         beq $ffd6f7
$ffd6f5 b0 2d         bcs $ffd724
$ffd6f7 84 85         sty $85
$ffd6f9 20 d3 d8      jsr $d8d3
$ffd6fc a9 20 a4      lda #$a420
$ffd6ff 85 29         sta $29
$ffd701 7f 20 5c db   adc $db5c20,x
$ffd705 20 dd d8      jsr $d8dd
$ffd708 ea            nop
$ffd709 90 07         bcc $ffd712
$ffd70b 20 fb da      jsr $dafb
$ffd70e a9 05 85      lda #$8505
$ffd711 24 c8         bit $c8
$ffd713 b1 9b         lda ($9b),y
$ffd715 d0 1d         bne $ffd734
$ffd717 a8            tay
$ffd718 b1 9b         lda ($9b),y
$ffd71a aa            tax
$ffd71b c8            iny
$ffd71c b1 9b         lda ($9b),y
$ffd71e 86 9b         stx $9b
$ffd720 85 9c         sta $9c
$ffd722 d0 b6         bne $ffd6da
$ffd724 a9 0d 20      lda #$200d
$ffd727 5c db 4c d2   jmp $d24cdb
$ffd72b d7 c8         cmp [$c8],y
$ffd72d d0 02         bne $ffd731
$ffd72f e6 9e         inc $9e
$ffd731 b1 9d         lda ($9d),y
$ffd733 60            rts
$ffd734 10 cc         bpl $ffd702
$ffd736 38            sec
$ffd737 e9 7f aa      sbc #$aa7f
$ffd73a 84 85         sty $85
$ffd73c a0 d0 84      ldy #$84d0
$ffd73f 9d a0 cf      sta $cfa0,x
$ffd742 84 9e         sty $9e
$ffd744 a0 ff ca      ldy #$caff
$ffd747 f0 07         beq $ffd750
$ffd749 20 2c d7      jsr $d72c
$ffd74c 10 fb         bpl $ffd749
$ffd74e 30 f6         bmi $ffd746
$ffd750 a9 20 20      lda #$2020
$ffd753 5c db 20 2c   jmp $2c20db
$ffd757 d7 30         cmp [$30],y
$ffd759 05 20         ora $20
$ffd75b 5c db d0 f6   jmp $f6d0db
$ffd75f 20 5c db      jsr $db5c
$ffd762 a9 20 d0      lda #$d020
$ffd765 98            tya
$ffd766 a9 80 85      lda #$8580
$ffd769 14 20         trb $20
$ffd76b 46 da         lsr $da
$ffd76d 20 65 d3      jsr $d365
$ffd770 d0 05         bne $ffd777
$ffd772 8a            txa
$ffd773 69 0f aa      adc #$aa0f
$ffd776 9a            txs
$ffd777 68            pla
$ffd778 68            pla
$ffd779 a9 09 20      lda #$2009
$ffd77c d6 d3         dec $d3,x
$ffd77e 20 a3 d9      jsr $d9a3
$ffd781 18            clc
$ffd782 98            tya
$ffd783 65 b8         adc $b8
$ffd785 48            pha
$ffd786 a5 b9         lda $b9
$ffd788 69 00 48      adc #$4800
$ffd78b a5 76         lda $76
$ffd78d 48            pha
$ffd78e a5 75         lda $75
$ffd790 48            pha
$ffd791 a9 c1 20      lda #$20c1
$ffd794 c0 de 20      cpy #$20de
$ffd797 6a            ror
$ffd798 dd 20 67      cmp $6720,x
$ffd79b dd a5 a2      cmp $a2a5,x
$ffd79e 09 7f 25      ora #$257f
$ffd7a1 9e 85 9e      stz $9e85,x
$ffd7a4 a9 af a0      lda #$a0af
$ffd7a7 d7 85         cmp [$85],y
$ffd7a9 5e 84 5f      lsr $5f84,x
$ffd7ac 4c 20 de      jmp $de20
$ffd7af a9 13 a0      lda #$a013
$ffd7b2 e9 20 f9      sbc #$f920
$ffd7b5 ea            nop
$ffd7b6 20 b7 00      jsr $00b7
$ffd7b9 c9 c7 d0      cmp #$d0c7
$ffd7bc 06 20         asl $20
$ffd7be b1 00         lda ($00),y
$ffd7c0 20 67 dd      jsr $dd67
$ffd7c3 20 82 eb      jsr $eb82
$ffd7c6 20 15 de      jsr $de15
$ffd7c9 a5 86         lda $86
$ffd7cb 48            pha
$ffd7cc a5 85         lda $85
$ffd7ce 48            pha
$ffd7cf a9 81 48      lda #$4881
$ffd7d2 ba            tsx
$ffd7d3 86 f8         stx $f8
$ffd7d5 20 58 d8      jsr $d858
$ffd7d8 a5 b8         lda $b8
$ffd7da a4 b9         ldy $b9
$ffd7dc a6 76         ldx $76
$ffd7de e8            inx
$ffd7df f0 04         beq $ffd7e5
$ffd7e1 85 79         sta $79
$ffd7e3 84 7a         sty $7a
$ffd7e5 a0 00 b1      ldy #$b100
$ffd7e8 b8            clv
$ffd7e9 d0 57         bne $ffd842
$ffd7eb a0 02 b1      ldy #$b102
$ffd7ee b8            clv
$ffd7ef 18            clc
$ffd7f0 f0 34         beq $ffd826
$ffd7f2 c8            iny
$ffd7f3 b1 b8         lda ($b8),y
$ffd7f5 85 75         sta $75
$ffd7f7 c8            iny
$ffd7f8 b1 b8         lda ($b8),y
$ffd7fa 85 76         sta $76
$ffd7fc 98            tya
$ffd7fd 65 b8         adc $b8
$ffd7ff 85 b8         sta $b8
$ffd801 90 02         bcc $ffd805
$ffd803 e6 b9         inc $b9
$ffd805 24 f2         bit $f2
$ffd807 10 14         bpl $ffd81d
$ffd809 a6 76         ldx $76
$ffd80b e8            inx
$ffd80c f0 0f         beq $ffd81d
$ffd80e a9 23 20      lda #$2023
$ffd811 5c db a6 75   jmp $75a6db
$ffd815 a5 76         lda $76
$ffd817 20 24 ed      jsr $ed24
$ffd81a 20 57 db      jsr $db57
$ffd81d 20 b1 00      jsr $00b1
$ffd820 20 28 d8      jsr $d828
$ffd823 4c d2 d7      jmp $d7d2
$ffd826 f0 62         beq $ffd88a
$ffd828 f0 2d         beq $ffd857
$ffd82a e9 80 90      sbc #$9080
$ffd82d 11 c9         ora ($c9),y
$ffd82f 40            rti
$ffd830 b0 14         bcs $ffd846
$ffd832 0a            asl
$ffd833 a8            tay
$ffd834 b9 01 d0      lda $d001,y
$ffd837 48            pha
$ffd838 b9 00 d0      lda $d000,y
$ffd83b 48            pha
$ffd83c 4c b1 00      jmp $00b1
$ffd83f 4c 46 da      jmp $da46
$ffd842 c9 3a f0      cmp #$f03a
$ffd845 bf 4c c9 de   lda $dec94c,x
$ffd849 38            sec
$ffd84a a5 67         lda $67
$ffd84c e9 01 a4      sbc #$a401
$ffd84f 68            pla
$ffd850 b0 01         bcs $ffd853
$ffd852 88            dey
$ffd853 85 7d         sta $7d
$ffd855 84 7e         sty $7e
$ffd857 60            rts
$ffd858 ad 00 c0      lda $c000
$ffd85b c9 83 f0      cmp #$f083
$ffd85e 01 60         ora ($60,x)
$ffd860 20 53 d5      jsr $d553
$ffd863 a2 ff 24      ldx #$24ff
$ffd866 d8            cld
$ffd867 10 03         bpl $ffd86c
$ffd869 4c e9 f2      jmp $f2e9
$ffd86c c9 03 b0      cmp #$b003
$ffd86f 01 18         ora ($18,x)
$ffd871 d0 3c         bne $ffd8af
$ffd873 a5 b8         lda $b8
$ffd875 a4 b9         ldy $b9
$ffd877 a6 76         ldx $76
$ffd879 e8            inx
$ffd87a f0 0c         beq $ffd888
$ffd87c 85 79         sta $79
$ffd87e 84 7a         sty $7a
$ffd880 a5 75         lda $75
$ffd882 a4 76         ldy $76
$ffd884 85 77         sta $77
$ffd886 84 78         sty $78
$ffd888 68            pla
$ffd889 68            pla
$ffd88a a9 5d a0      lda #$a05d
$ffd88d d3 90         cmp ($90,sp),y
$ffd88f 03 4c         ora $4c,sp
$ffd891 31 d4         and ($d4),y
$ffd893 4c 3c d4      jmp $d43c
$ffd896 d0 17         bne $ffd8af
$ffd898 a2 d2 a4      ldx #$a4d2
$ffd89b 7a            ply
$ffd89c d0 03         bne $ffd8a1
$ffd89e 4c 12 d4      jmp $d412
$ffd8a1 a5 79         lda $79
$ffd8a3 85 b8         sta $b8
$ffd8a5 84 b9         sty $b9
$ffd8a7 a5 77         lda $77
$ffd8a9 a4 78         ldy $78
$ffd8ab 85 75         sta $75
$ffd8ad 84 76         sty $76
$ffd8af 60            rts
$ffd8b0 bd 01 02      lda $0201,x
$ffd8b3 10 11         bpl $ffd8c6
$ffd8b5 a5 0e         lda $0e
$ffd8b7 f0 16         beq $ffd8cf
$ffd8b9 c9 22 f0      cmp #$f022
$ffd8bc 12 a5         ora ($a5)
$ffd8be 13 c9         ora ($c9,sp),y
$ffd8c0 49 f0 0c      eor #$0cf0
$ffd8c3 bd 00 02      lda $0200,x
$ffd8c6 08            php
$ffd8c7 c9 61 90      cmp #$9061
$ffd8ca 02 29         cop $29
$ffd8cc 5f 28 60 bd   eor $bd6028,x
$ffd8d0 00 02         brk $02
$ffd8d2 60            rts
$ffd8d3 48            pha
$ffd8d4 a9 20 20      lda #$2020
$ffd8d7 5c db 68 4c   jmp $4c68db
$ffd8db 24 ed         bit $ed
$ffd8dd a5 24         lda $24
$ffd8df c9 21 2c      cmp #$2c21
$ffd8e2 1f c0 10 05   ora $0510c0,x
$ffd8e6 ad 7b 05      lda $057b
$ffd8e9 c9 49 60      cmp #$6049
$ffd8ec ad 50 c0      lda $c050
$ffd8ef 20 f7 d8      jsr $d8f7
$ffd8f2 a9 14 4c      lda #$4c14
$ffd8f5 4b            phk
$ffd8f6 fb            xce
$ffd8f7 a0 27 84      ldy #$8427
$ffd8fa 2d 20 cb      and $cb20
$ffd8fd f3 a9         sbc ($a9,sp),y
$ffd8ff 27 90         and [$90]
$ffd901 01 2a         ora ($2a,x)
$ffd903 a8            tay
$ffd904 a9 00 85      lda #$8500
$ffd907 30 20         bmi $ffd929
$ffd909 8b            phb
$ffd90a f7 88         sbc [$88],y
$ffd90c 10 f6         bpl $ffd904
$ffd90e 60            rts
$ffd90f 00 00         brk $00
$ffd911 00 08         brk $08
$ffd913 c6 76         dec $76
$ffd915 28            plp
$ffd916 d0 03         bne $ffd91b
$ffd918 4c 65 d6      jmp $d665
$ffd91b 20 6c d6      jsr $d66c
$ffd91e 4c 35 d9      jmp $d935
$ffd921 a9 03 20      lda #$2003
$ffd924 d6 d3         dec $d3,x
$ffd926 a5 b9         lda $b9
$ffd928 48            pha
$ffd929 a5 b8         lda $b8
$ffd92b 48            pha
$ffd92c a5 76         lda $76
$ffd92e 48            pha
$ffd92f a5 75         lda $75
$ffd931 48            pha
$ffd932 a9 b0 48      lda #$48b0
$ffd935 20 b7 00      jsr $00b7
$ffd938 20 3e d9      jsr $d93e
$ffd93b 4c d2 d7      jmp $d7d2
$ffd93e 20 0c da      jsr $da0c
$ffd941 20 a6 d9      jsr $d9a6
$ffd944 a5 76         lda $76
$ffd946 c5 51         cmp $51
$ffd948 b0 0b         bcs $ffd955
$ffd94a 98            tya
$ffd94b 38            sec
$ffd94c 65 b8         adc $b8
$ffd94e a6 b9         ldx $b9
$ffd950 90 07         bcc $ffd959
$ffd952 e8            inx
$ffd953 b0 04         bcs $ffd959
$ffd955 a5 67         lda $67
$ffd957 a6 68         ldx $68
$ffd959 20 1e d6      jsr $d61e
$ffd95c 90 1e         bcc $ffd97c
$ffd95e a5 9b         lda $9b
$ffd960 e9 01 85      sbc #$8501
$ffd963 b8            clv
$ffd964 a5 9c         lda $9c
$ffd966 e9 00 85      sbc #$8500
$ffd969 b9 60 d0      lda $d060,y
$ffd96c fd a9 ff      sbc $ffa9,x
$ffd96f 85 85         sta $85
$ffd971 20 65 d3      jsr $d365
$ffd974 9a            txs
$ffd975 c9 b0 f0      cmp #$f0b0
$ffd978 0b            phd
$ffd979 a2 16 2c      ldx #$2c16
$ffd97c a2 5a 4c      ldx #$4c5a
$ffd97f 12 d4         ora ($d4)
$ffd981 4c c9 de      jmp $dec9
$ffd984 68            pla
$ffd985 68            pla
$ffd986 c0 42 f0      cpy #$f042
$ffd989 3b            tsc
$ffd98a 85 75         sta $75
$ffd98c 68            pla
$ffd98d 85 76         sta $76
$ffd98f 68            pla
$ffd990 85 b8         sta $b8
$ffd992 68            pla
$ffd993 85 b9         sta $b9
$ffd995 20 a3 d9      jsr $d9a3
$ffd998 98            tya
$ffd999 18            clc
$ffd99a 65 b8         adc $b8
$ffd99c 85 b8         sta $b8
$ffd99e 90 02         bcc $ffd9a2
$ffd9a0 e6 b9         inc $b9
$ffd9a2 60            rts
$ffd9a3 a2 3a 2c      ldx #$2c3a
$ffd9a6 a2 00 86      ldx #$8600
$ffd9a9 0d a0 00      ora $00a0
$ffd9ac 84 0e         sty $0e
$ffd9ae a5 0e         lda $0e
$ffd9b0 a6 0d         ldx $0d
$ffd9b2 85 0d         sta $0d
$ffd9b4 86 0e         stx $0e
$ffd9b6 b1 b8         lda ($b8),y
$ffd9b8 f0 e8         beq $ffd9a2
$ffd9ba c5 0e         cmp $0e
$ffd9bc f0 e4         beq $ffd9a2
$ffd9be c8            iny
$ffd9bf c9 22 d0      cmp #$d022
$ffd9c2 f3 f0         sbc ($f0,sp),y
$ffd9c4 e9 68 68      sbc #$6868
$ffd9c7 68            pla
$ffd9c8 60            rts
$ffd9c9 20 7b dd      jsr $dd7b
$ffd9cc 20 b7 00      jsr $00b7
$ffd9cf c9 ab f0      cmp #$f0ab
$ffd9d2 05 a9         ora $a9
$ffd9d4 c4 20         cpy $20
$ffd9d6 c0 de a5      cpy #$a5de
$ffd9d9 9d d0 05      sta $05d0,x
$ffd9dc 20 a6 d9      jsr $d9a6
$ffd9df f0 b7         beq $ffd998
$ffd9e1 20 b7 00      jsr $00b7
$ffd9e4 b0 03         bcs $ffd9e9
$ffd9e6 4c 3e d9      jmp $d93e
$ffd9e9 4c 28 d8      jmp $d828
$ffd9ec 20 f8 e6      jsr $e6f8
$ffd9ef 48            pha
$ffd9f0 c9 b0 f0      cmp #$f0b0
$ffd9f3 04 c9         tsb $c9
$ffd9f5 ab            plb
$ffd9f6 d0 89         bne $ffd981
$ffd9f8 c6 a1         dec $a1
$ffd9fa d0 04         bne $ffda00
$ffd9fc 68            pla
$ffd9fd 4c 2a d8      jmp $d82a
$ffda00 20 b1 00      jsr $00b1
$ffda03 20 0c da      jsr $da0c
$ffda06 c9 2c f0      cmp #$f02c
$ffda09 ee 68 60      inc $6068
$ffda0c a2 00 86      ldx #$8600
$ffda0f 50 86         bvc $ffd997
$ffda11 51 b0         eor ($b0),y
$ffda13 f7 e9         sbc [$e9],y
$ffda15 2f 85 0d a5   and $a50d85
$ffda19 51 85         eor ($85),y
$ffda1b 5e c9 19      lsr $19c9,x
$ffda1e b0 d4         bcs $ffd9f4
$ffda20 a5 50         lda $50
$ffda22 0a            asl
$ffda23 26 5e         rol $5e
$ffda25 0a            asl
$ffda26 26 5e         rol $5e
$ffda28 65 50         adc $50
$ffda2a 85 50         sta $50
$ffda2c a5 5e         lda $5e
$ffda2e 65 51         adc $51
$ffda30 85 51         sta $51
$ffda32 06 50         asl $50
$ffda34 26 51         rol $51
$ffda36 a5 50         lda $50
$ffda38 65 0d         adc $0d
$ffda3a 85 50         sta $50
$ffda3c 90 02         bcc $ffda40
$ffda3e e6 51         inc $51
$ffda40 20 b1 00      jsr $00b1
$ffda43 4c 12 da      jmp $da12
$ffda46 20 e3 df      jsr $dfe3
$ffda49 85 85         sta $85
$ffda4b 84 86         sty $86
$ffda4d a9 d0 20      lda #$20d0
$ffda50 c0 de a5      cpy #$a5de
$ffda53 12 48         ora ($48)
$ffda55 a5 11         lda $11
$ffda57 48            pha
$ffda58 20 7b dd      jsr $dd7b
$ffda5b 68            pla
$ffda5c 2a            rol
$ffda5d 20 6d dd      jsr $dd6d
$ffda60 d0 18         bne $ffda7a
$ffda62 68            pla
$ffda63 10 12         bpl $ffda77
$ffda65 20 72 eb      jsr $eb72
$ffda68 20 0c e1      jsr $e10c
$ffda6b a0 00 a5      ldy #$a500
$ffda6e a0 91 85      ldy #$8591
$ffda71 c8            iny
$ffda72 a5 a1         lda $a1
$ffda74 91 85         sta ($85),y
$ffda76 60            rts
$ffda77 4c 27 eb      jmp $eb27
$ffda7a 68            pla
$ffda7b a0 02 b1      ldy #$b102
$ffda7e a0 c5 70      ldy #$70c5
$ffda81 90 17         bcc $ffda9a
$ffda83 d0 07         bne $ffda8c
$ffda85 88            dey
$ffda86 b1 a0         lda ($a0),y
$ffda88 c5 6f         cmp $6f
$ffda8a 90 0e         bcc $ffda9a
$ffda8c a4 a1         ldy $a1
$ffda8e c4 6a         cpy $6a
$ffda90 90 08         bcc $ffda9a
$ffda92 d0 0d         bne $ffdaa1
$ffda94 a5 a0         lda $a0
$ffda96 c5 69         cmp $69
$ffda98 b0 07         bcs $ffdaa1
$ffda9a a5 a0         lda $a0
$ffda9c a4 a1         ldy $a1
$ffda9e 4c b7 da      jmp $dab7
$ffdaa1 a0 00 b1      ldy #$b100
$ffdaa4 a0 20 d5      ldy #$d520
$ffdaa7 e3 a5         sbc $a5,sp
$ffdaa9 8c a4 8d      sty $8da4
$ffdaac 85 ab         sta $ab
$ffdaae 84 ac         sty $ac
$ffdab0 20 d4 e5      jsr $e5d4
$ffdab3 a9 9d a0      lda #$a09d
$ffdab6 00 85         brk $85
$ffdab8 8c 84 8d      sty $8d84
$ffdabb 20 35 e6      jsr $e635
$ffdabe a0 00 b1      ldy #$b100
$ffdac1 8c 91 85      sty $8591
$ffdac4 c8            iny
$ffdac5 b1 8c         lda ($8c),y
$ffdac7 91 85         sta ($85),y
$ffdac9 c8            iny
$ffdaca b1 8c         lda ($8c),y
$ffdacc 91 85         sta ($85),y
$ffdace 60            rts
$ffdacf 20 3d db      jsr $db3d
$ffdad2 20 b7 00      jsr $00b7
$ffdad5 f0 24         beq $ffdafb
$ffdad7 f0 29         beq $ffdb02
$ffdad9 c9 c0 f0      cmp #$f0c0
$ffdadc 3c c9 c3      bit $c3c9,x
$ffdadf 18            clc
$ffdae0 f0 37         beq $ffdb19
$ffdae2 c9 2c 18      cmp #$182c
$ffdae5 f0 1c         beq $ffdb03
$ffdae7 c9 3b f0      cmp #$f03b
$ffdaea 44 20 7b      mvp $20,$7b
$ffdaed dd 24 11      cmp $1124,x
$ffdaf0 30 dd         bmi $ffdacf
$ffdaf2 20 34 ed      jsr $ed34
$ffdaf5 20 e7 e3      jsr $e3e7
$ffdaf8 4c cf da      jmp $dacf
$ffdafb a9 0d 20      lda #$200d
$ffdafe 5c db 49 ff   jmp $ff49db
$ffdb02 60            rts
$ffdb03 20 dd d8      jsr $d8dd
$ffdb06 30 09         bmi $ffdb11
$ffdb08 c9 18 90      cmp #$9018
$ffdb0b 05 20         ora $20
$ffdb0d fb            xce
$ffdb0e da            phx
$ffdb0f d0 1e         bne $ffdb2f
$ffdb11 69 10 29      adc #$2910
$ffdb14 f0 aa         beq $ffdac0
$ffdb16 38            sec
$ffdb17 b0 0c         bcs $ffdb25
$ffdb19 08            php
$ffdb1a 20 f5 e6      jsr $e6f5
$ffdb1d c9 29 d0      cmp #$d029
$ffdb20 62 28 90      per $ff6b4b
$ffdb23 07 ca         ora [$ca]
$ffdb25 20 cb f7      jsr $f7cb
$ffdb28 90 05         bcc $ffdb2f
$ffdb2a aa            tax
$ffdb2b e8            inx
$ffdb2c ca            dex
$ffdb2d d0 06         bne $ffdb35
$ffdb2f 20 b1 00      jsr $00b1
$ffdb32 4c d7 da      jmp $dad7
$ffdb35 20 57 db      jsr $db57
$ffdb38 d0 f2         bne $ffdb2c
$ffdb3a 20 e7 e3      jsr $e3e7
$ffdb3d 20 00 e6      jsr $e600
$ffdb40 aa            tax
$ffdb41 a0 00 e8      ldy #$e800
$ffdb44 ca            dex
$ffdb45 f0 bb         beq $ffdb02
$ffdb47 b1 5e         lda ($5e),y
$ffdb49 20 5c db      jsr $db5c
$ffdb4c c8            iny
$ffdb4d c9 0d d0      cmp #$d00d
$ffdb50 f3 20         sbc ($20,sp),y
$ffdb52 00 db         brk $db
$ffdb54 4c 44 db      jmp $db44
$ffdb57 a9 20 2c      lda #$2c20
$ffdb5a a9 3f 09      lda #$093f
$ffdb5d 80 c9         bra $ffdb28
$ffdb5f a0 90 02      ldy #$0290
$ffdb62 05 f3         ora $f3
$ffdb64 20 ed fd      jsr $fded
$ffdb67 29 7f 48      and #$487f
$ffdb6a a5 f1         lda $f1
$ffdb6c 20 a8 fc      jsr $fca8
$ffdb6f 68            pla
$ffdb70 60            rts
$ffdb71 a5 15         lda $15
$ffdb73 f0 12         beq $ffdb87
$ffdb75 30 04         bmi $ffdb7b
$ffdb77 a0 ff d0      ldy #$d0ff
$ffdb7a 04 a5         tsb $a5
$ffdb7c 7b            tdc
$ffdb7d a4 7c         ldy $7c
$ffdb7f 85 75         sta $75
$ffdb81 84 76         sty $76
$ffdb83 4c c9 de      jmp $dec9
$ffdb86 68            pla
$ffdb87 24 d8         bit $d8
$ffdb89 10 05         bpl $ffdb90
$ffdb8b a2 fe 4c      ldx #$4cfe
$ffdb8e e9 f2 a9      sbc #$a9f2
$ffdb91 ef a0 dc 20   sbc $20dca0
$ffdb95 3a            dec
$ffdb96 db            stp
$ffdb97 a5 79         lda $79
$ffdb99 a4 7a         ldy $7a
$ffdb9b 85 b8         sta $b8
$ffdb9d 84 b9         sty $b9
$ffdb9f 60            rts
$ffdba0 20 06 e3      jsr $e306
$ffdba3 a2 01 a0      ldx #$a001
$ffdba6 02 a9         cop $a9
$ffdba8 00 8d         brk $8d
$ffdbaa 01 02         ora ($02,x)
$ffdbac a9 40 20      lda #$2040
$ffdbaf eb            xba
$ffdbb0 db            stp
$ffdbb1 60            rts
$ffdbb2 c9 22 d0      cmp #$d022
$ffdbb5 0e 20 81      asl $8120
$ffdbb8 de a9 3b      dec $3ba9,x
$ffdbbb 20 c0 de      jsr $dec0
$ffdbbe 20 3d db      jsr $db3d
$ffdbc1 4c c7 db      jmp $dbc7
$ffdbc4 20 5a db      jsr $db5a
$ffdbc7 20 06 e3      jsr $e306
$ffdbca a9 2c 8d      lda #$8d2c
$ffdbcd ff 01 20 2c   sbc $2c2001,x
$ffdbd1 d5 ad         cmp $ad,x
$ffdbd3 00 02         brk $02
$ffdbd5 c9 03 d0      cmp #$d003
$ffdbd8 10 4c         bpl $ffdc26
$ffdbda 63 d8         adc $d8,sp
$ffdbdc 20 5a db      jsr $db5a
$ffdbdf 4c 2c d5      jmp $d52c
$ffdbe2 a6 7d         ldx $7d
$ffdbe4 a4 7e         ldy $7e
$ffdbe6 a9 98 2c      lda #$2c98
$ffdbe9 a9 00 85      lda #$8500
$ffdbec 15 86         ora $86,x
$ffdbee 7f 84 80 20   adc $208084,x
$ffdbf2 e3 df         sbc $df,sp
$ffdbf4 85 85         sta $85
$ffdbf6 84 86         sty $86
$ffdbf8 a5 b8         lda $b8
$ffdbfa a4 b9         ldy $b9
$ffdbfc 85 87         sta $87
$ffdbfe 84 88         sty $88
$ffdc00 a6 7f         ldx $7f
$ffdc02 a4 80         ldy $80
$ffdc04 86 b8         stx $b8
$ffdc06 84 b9         sty $b9
$ffdc08 20 b7 00      jsr $00b7
$ffdc0b d0 1e         bne $ffdc2b
$ffdc0d 24 15         bit $15
$ffdc0f 50 0e         bvc $ffdc1f
$ffdc11 20 0c fd      jsr $fd0c
$ffdc14 29 7f 8d      and #$8d7f
$ffdc17 00 02         brk $02
$ffdc19 a2 ff a0      ldx #$a0ff
$ffdc1c 01 d0         ora ($d0,x)
$ffdc1e 08            php
$ffdc1f 30 7f         bmi $ffdca0
$ffdc21 20 5a db      jsr $db5a
$ffdc24 20 dc db      jsr $dbdc
$ffdc27 86 b8         stx $b8
$ffdc29 84 b9         sty $b9
$ffdc2b 20 b1 00      jsr $00b1
$ffdc2e 24 11         bit $11
$ffdc30 10 31         bpl $ffdc63
$ffdc32 24 15         bit $15
$ffdc34 50 09         bvc $ffdc3f
$ffdc36 e8            inx
$ffdc37 86 b8         stx $b8
$ffdc39 a9 00 85      lda #$8500
$ffdc3c 0d f0 0c      ora $0cf0
$ffdc3f 85 0d         sta $0d
$ffdc41 c9 22 f0      cmp #$f022
$ffdc44 07 a9         ora [$a9]
$ffdc46 3a            dec
$ffdc47 85 0d         sta $0d
$ffdc49 a9 2c 18      lda #$182c
$ffdc4c 85 0e         sta $0e
$ffdc4e a5 b8         lda $b8
$ffdc50 a4 b9         ldy $b9
$ffdc52 69 00 90      adc #$9000
$ffdc55 01 c8         ora ($c8,x)
$ffdc57 20 ed e3      jsr $e3ed
$ffdc5a 20 3d e7      jsr $e73d
$ffdc5d 20 7b da      jsr $da7b
$ffdc60 4c 72 dc      jmp $dc72
$ffdc63 48            pha
$ffdc64 ad 00 02      lda $0200
$ffdc67 f0 30         beq $ffdc99
$ffdc69 68            pla
$ffdc6a 20 4a ec      jsr $ec4a
$ffdc6d a5 12         lda $12
$ffdc6f 20 63 da      jsr $da63
$ffdc72 20 b7 00      jsr $00b7
$ffdc75 f0 07         beq $ffdc7e
$ffdc77 c9 2c f0      cmp #$f02c
$ffdc7a 03 4c         ora $4c,sp
$ffdc7c 71 db         adc ($db),y
$ffdc7e a5 b8         lda $b8
$ffdc80 a4 b9         ldy $b9
$ffdc82 85 7f         sta $7f
$ffdc84 84 80         sty $80
$ffdc86 a5 87         lda $87
$ffdc88 a4 88         ldy $88
$ffdc8a 85 b8         sta $b8
$ffdc8c 84 b9         sty $b9
$ffdc8e 20 b7 00      jsr $00b7
$ffdc91 f0 33         beq $ffdcc6
$ffdc93 20 be de      jsr $debe
$ffdc96 4c f1 db      jmp $dbf1
$ffdc99 a5 15         lda $15
$ffdc9b d0 cc         bne $ffdc69
$ffdc9d 4c 86 db      jmp $db86
$ffdca0 20 a3 d9      jsr $d9a3
$ffdca3 c8            iny
$ffdca4 aa            tax
$ffdca5 d0 12         bne $ffdcb9
$ffdca7 a2 2a c8      ldx #$c82a
$ffdcaa b1 b8         lda ($b8),y
$ffdcac f0 5f         beq $ffdd0d
$ffdcae c8            iny
$ffdcaf b1 b8         lda ($b8),y
$ffdcb1 85 7b         sta $7b
$ffdcb3 c8            iny
$ffdcb4 b1 b8         lda ($b8),y
$ffdcb6 c8            iny
$ffdcb7 85 7c         sta $7c
$ffdcb9 b1 b8         lda ($b8),y
$ffdcbb aa            tax
$ffdcbc 20 98 d9      jsr $d998
$ffdcbf e0 83 d0      cpx #$d083
$ffdcc2 dd 4c 2b      cmp $2b4c,x
$ffdcc5 dc a5 7f      jmp [$7fa5]
$ffdcc8 a4 80         ldy $80
$ffdcca a6 15         ldx $15
$ffdccc 10 03         bpl $ffdcd1
$ffdcce 4c 53 d8      jmp $d853
$ffdcd1 a0 00 b1      ldy #$b100
$ffdcd4 7f f0 07 a9   adc $a907f0,x
$ffdcd8 df a0 dc 4c   cmp $4cdca0,x
$ffdcdc 3a            dec
$ffdcdd db            stp
$ffdcde 60            rts
$ffdcdf 3f 45 58 54   and $545845,x
$ffdce3 52 41         eor ($41)
$ffdce5 20 49 47      jsr $4749
$ffdce8 4e 4f 52      lsr $524f
$ffdceb 45 44         eor $44
$ffdced 0d 00 3f      ora $3f00
$ffdcf0 52 45         eor ($45)
$ffdcf2 45 4e         eor $4e
$ffdcf4 54 45 52      mvn $45,$52
$ffdcf7 0d 00 d0      ora $d000
$ffdcfa 04 a0         tsb $a0
$ffdcfc 00 f0         brk $f0
$ffdcfe 03 20         ora $20,sp
$ffdd00 e3 df         sbc $df,sp
$ffdd02 85 85         sta $85
$ffdd04 84 86         sty $86
$ffdd06 20 65 d3      jsr $d365
$ffdd09 f0 04         beq $ffdd0f
$ffdd0b a2 00 f0      ldx #$f000
$ffdd0e 69 9a e8      adc #$e89a
$ffdd11 e8            inx
$ffdd12 e8            inx
$ffdd13 e8            inx
$ffdd14 8a            txa
$ffdd15 e8            inx
$ffdd16 e8            inx
$ffdd17 e8            inx
$ffdd18 e8            inx
$ffdd19 e8            inx
$ffdd1a e8            inx
$ffdd1b 86 60         stx $60
$ffdd1d a0 01 20      ldy #$2001
$ffdd20 f9 ea ba      sbc $baea,y
$ffdd23 bd 09 01      lda $0109,x
$ffdd26 85 a2         sta $a2
$ffdd28 a5 85         lda $85
$ffdd2a a4 86         ldy $86
$ffdd2c 20 be e7      jsr $e7be
$ffdd2f 20 27 eb      jsr $eb27
$ffdd32 a0 01 20      ldy #$2001
$ffdd35 b4 eb         ldy $eb,x
$ffdd37 ba            tsx
$ffdd38 38            sec
$ffdd39 fd 09 01      sbc $0109,x
$ffdd3c f0 17         beq $ffdd55
$ffdd3e bd 0f 01      lda $010f,x
$ffdd41 85 75         sta $75
$ffdd43 bd 10 01      lda $0110,x
$ffdd46 85 76         sta $76
$ffdd48 bd 12 01      lda $0112,x
$ffdd4b 85 b8         sta $b8
$ffdd4d bd 11 01      lda $0111,x
$ffdd50 85 b9         sta $b9
$ffdd52 4c d2 d7      jmp $d7d2
$ffdd55 8a            txa
$ffdd56 69 11 aa      adc #$aa11
$ffdd59 9a            txs
$ffdd5a 20 b7 00      jsr $00b7
$ffdd5d c9 2c d0      cmp #$d02c
$ffdd60 f1 20         sbc ($20),y
$ffdd62 b1 00         lda ($00),y
$ffdd64 20 ff dc      jsr $dcff
$ffdd67 20 7b dd      jsr $dd7b
$ffdd6a 18            clc
$ffdd6b 24 38         bit $38
$ffdd6d 24 11         bit $11
$ffdd6f 30 03         bmi $ffdd74
$ffdd71 b0 03         bcs $ffdd76
$ffdd73 60            rts
$ffdd74 b0 fd         bcs $ffdd73
$ffdd76 a2 a3 4c      ldx #$4ca3
$ffdd79 12 d4         ora ($d4)
$ffdd7b a6 b8         ldx $b8
$ffdd7d d0 02         bne $ffdd81
$ffdd7f c6 b9         dec $b9
$ffdd81 c6 b8         dec $b8
$ffdd83 a2 00 24      ldx #$2400
$ffdd86 48            pha
$ffdd87 8a            txa
$ffdd88 48            pha
$ffdd89 a9 01 20      lda #$2001
$ffdd8c d6 d3         dec $d3,x
$ffdd8e 20 60 de      jsr $de60
$ffdd91 a9 00 85      lda #$8500
$ffdd94 89 20 b7      bit #$b720
$ffdd97 00 38         brk $38
$ffdd99 e9 cf 90      sbc #$90cf
$ffdd9c 17 c9         ora [$c9],y
$ffdd9e 03 b0         ora $b0,sp
$ffdda0 13 c9         ora ($c9,sp),y
$ffdda2 01 2a         ora ($2a,x)
$ffdda4 49 01 45      eor #$4501
$ffdda7 89 c5 89      bit #$89c5
$ffddaa 90 61         bcc $ffde0d
$ffddac 85 89         sta $89
$ffddae 20 b1 00      jsr $00b1
$ffddb1 4c 98 dd      jmp $dd98
$ffddb4 a6 89         ldx $89
$ffddb6 d0 2c         bne $ffdde4
$ffddb8 b0 7b         bcs $ffde35
$ffddba 69 07 90      adc #$9007
$ffddbd 77 65         adc [$65],y
$ffddbf 11 d0         ora ($d0),y
$ffddc1 03 4c         ora $4c,sp
$ffddc3 97 e5         sta [$e5],y
$ffddc5 69 ff 85      adc #$85ff
$ffddc8 5e 0a 65      lsr $650a,x
$ffddcb 5e a8 68      lsr $68a8,x
$ffddce d9 b2 d0      cmp $d0b2,y
$ffddd1 b0 67         bcs $ffde3a
$ffddd3 20 6a dd      jsr $dd6a
$ffddd6 48            pha
$ffddd7 20 fd dd      jsr $ddfd
$ffddda 68            pla
$ffdddb a4 87         ldy $87
$ffdddd 10 17         bpl $ffddf6
$ffdddf aa            tax
$ffdde0 f0 56         beq $ffde38
$ffdde2 d0 5f         bne $ffde43
$ffdde4 46 11         lsr $11
$ffdde6 8a            txa
$ffdde7 2a            rol
$ffdde8 a6 b8         ldx $b8
$ffddea d0 02         bne $ffddee
$ffddec c6 b9         dec $b9
$ffddee c6 b8         dec $b8
$ffddf0 a0 1b 85      ldy #$851b
$ffddf3 89 d0 d7      bit #$d7d0
$ffddf6 d9 b2 d0      cmp $d0b2,y
$ffddf9 b0 48         bcs $ffde43
$ffddfb 90 d9         bcc $ffddd6
$ffddfd b9 b4 d0      lda $d0b4,y
$ffde00 48            pha
$ffde01 b9 b3 d0      lda $d0b3,y
$ffde04 48            pha
$ffde05 20 10 de      jsr $de10
$ffde08 a5 89         lda $89
$ffde0a 4c 86 dd      jmp $dd86
$ffde0d 4c c9 de      jmp $dec9
$ffde10 a5 a2         lda $a2
$ffde12 be b2 d0      ldx $d0b2,y
$ffde15 a8            tay
$ffde16 68            pla
$ffde17 85 5e         sta $5e
$ffde19 e6 5e         inc $5e
$ffde1b 68            pla
$ffde1c 85 5f         sta $5f
$ffde1e 98            tya
$ffde1f 48            pha
$ffde20 20 72 eb      jsr $eb72
$ffde23 a5 a1         lda $a1
$ffde25 48            pha
$ffde26 a5 a0         lda $a0
$ffde28 48            pha
$ffde29 a5 9f         lda $9f
$ffde2b 48            pha
$ffde2c a5 9e         lda $9e
$ffde2e 48            pha
$ffde2f a5 9d         lda $9d
$ffde31 48            pha
$ffde32 6c 5e 00      jmp ($005e)
$ffde35 a0 ff 68      ldy #$68ff
$ffde38 f0 23         beq $ffde5d
$ffde3a c9 64 f0      cmp #$f064
$ffde3d 03 20         ora $20,sp
$ffde3f 6a            ror
$ffde40 dd 84 87      cmp $8784,x
$ffde43 68            pla
$ffde44 4a            lsr
$ffde45 85 16         sta $16
$ffde47 68            pla
$ffde48 85 a5         sta $a5
$ffde4a 68            pla
$ffde4b 85 a6         sta $a6
$ffde4d 68            pla
$ffde4e 85 a7         sta $a7
$ffde50 68            pla
$ffde51 85 a8         sta $a8
$ffde53 68            pla
$ffde54 85 a9         sta $a9
$ffde56 68            pla
$ffde57 85 aa         sta $aa
$ffde59 45 a2         eor $a2
$ffde5b 85 ab         sta $ab
$ffde5d a5 9d         lda $9d
$ffde5f 60            rts
$ffde60 a9 00 85      lda #$8500
$ffde63 11 20         ora ($20),y
$ffde65 b1 00         lda ($00),y
$ffde67 b0 03         bcs $ffde6c
$ffde69 4c 4a ec      jmp $ec4a
$ffde6c 20 7d e0      jsr $e07d
$ffde6f b0 64         bcs $ffded5
$ffde71 c9 2e f0      cmp #$f02e
$ffde74 f4 c9 c9      pea $c9c9
$ffde77 f0 55         beq $ffdece
$ffde79 c9 c8 f0      cmp #$f0c8
$ffde7c e7 c9         sbc [$c9]
$ffde7e 22 d0 0f a5   jsr $a50fd0
$ffde82 b8            clv
$ffde83 a4 b9         ldy $b9
$ffde85 69 00 90      adc #$9000
$ffde88 01 c8         ora ($c8,x)
$ffde8a 20 e7 e3      jsr $e3e7
$ffde8d 4c 3d e7      jmp $e73d
$ffde90 c9 c6 d0      cmp #$d0c6
$ffde93 10 a0         bpl $ffde35
$ffde95 18            clc
$ffde96 d0 38         bne $ffded0
$ffde98 a5 9d         lda $9d
$ffde9a d0 03         bne $ffde9f
$ffde9c a0 01 2c      ldy #$2c01
$ffde9f a0 00 4c      ldy #$4c00
$ffdea2 01 e3         ora ($e3,x)
$ffdea4 c9 c2 d0      cmp #$d0c2
$ffdea7 03 4c         ora $4c,sp
$ffdea9 54 e3 c9      mvn $e3,$c9
$ffdeac d2 90         cmp ($90)
$ffdeae 03 4c         ora $4c,sp
$ffdeb0 0c df 20      tsb $20df
$ffdeb3 bb            tyx
$ffdeb4 de 20 7b      dec $7b20,x
$ffdeb7 dd a9 29      cmp $29a9,x
$ffdeba 2c a9 28      bit $28a9
$ffdebd 2c a9 2c      bit $2ca9
$ffdec0 a0 00 d1      ldy #$d100
$ffdec3 b8            clv
$ffdec4 d0 03         bne $ffdec9
$ffdec6 4c b1 00      jmp $00b1
$ffdec9 a2 10 4c      ldx #$4c10
$ffdecc 12 d4         ora ($d4)
$ffdece a0 15 68      ldy #$6815
$ffded1 68            pla
$ffded2 4c d7 dd      jmp $ddd7
$ffded5 20 e3 df      jsr $dfe3
$ffded8 85 a0         sta $a0
$ffdeda 84 a1         sty $a1
$ffdedc a6 11         ldx $11
$ffdede f0 05         beq $ffdee5
$ffdee0 a2 00 86      ldx #$8600
$ffdee3 ac 60 a6      ldy $a660
$ffdee6 12 10         ora ($10)
$ffdee8 0d a0 00      ora $00a0
$ffdeeb b1 a0         lda ($a0),y
$ffdeed aa            tax
$ffdeee c8            iny
$ffdeef b1 a0         lda ($a0),y
$ffdef1 a8            tay
$ffdef2 8a            txa
$ffdef3 4c f2 e2      jmp $e2f2
$ffdef6 4c f9 ea      jmp $eaf9
$ffdef9 20 b1 00      jsr $00b1
$ffdefc 20 ec f1      jsr $f1ec
$ffdeff 8a            txa
$ffdf00 a4 f0         ldy $f0
$ffdf02 20 a6 f7      jsr $f7a6
$ffdf05 a8            tay
$ffdf06 20 01 e3      jsr $e301
$ffdf09 4c b8 de      jmp $deb8
$ffdf0c c9 d7 f0      cmp #$f0d7
$ffdf0f e9 0a 48      sbc #$480a
$ffdf12 aa            tax
$ffdf13 20 b1 00      jsr $00b1
$ffdf16 e0 cf 90      cpx #$90cf
$ffdf19 20 20 bb      jsr $bb20
$ffdf1c de 20 7b      dec $7b20,x
$ffdf1f dd 20 be      cmp $be20,x
$ffdf22 de 20 6c      dec $6c20,x
$ffdf25 dd 68 aa      cmp $aa68,x
$ffdf28 a5 a1         lda $a1
$ffdf2a 48            pha
$ffdf2b a5 a0         lda $a0
$ffdf2d 48            pha
$ffdf2e 8a            txa
$ffdf2f 48            pha
$ffdf30 20 f8 e6      jsr $e6f8
$ffdf33 68            pla
$ffdf34 a8            tay
$ffdf35 8a            txa
$ffdf36 48            pha
$ffdf37 4c 3f df      jmp $df3f
$ffdf3a 20 b2 de      jsr $deb2
$ffdf3d 68            pla
$ffdf3e a8            tay
$ffdf3f b9 dc cf      lda $cfdc,y
$ffdf42 85 91         sta $91
$ffdf44 b9 dd cf      lda $cfdd,y
$ffdf47 85 92         sta $92
$ffdf49 20 90 00      jsr $0090
$ffdf4c 4c 6a dd      jmp $dd6a
$ffdf4f a5 a5         lda $a5
$ffdf51 05 9d         ora $9d
$ffdf53 d0 0b         bne $ffdf60
$ffdf55 a5 a5         lda $a5
$ffdf57 f0 04         beq $ffdf5d
$ffdf59 a5 9d         lda $9d
$ffdf5b d0 03         bne $ffdf60
$ffdf5d a0 00 2c      ldy #$2c00
$ffdf60 a0 01 4c      ldy #$4c01
$ffdf63 01 e3         ora ($e3,x)
$ffdf65 20 6d dd      jsr $dd6d
$ffdf68 b0 13         bcs $ffdf7d
$ffdf6a a5 aa         lda $aa
$ffdf6c 09 7f 25      ora #$257f
$ffdf6f a6 85         ldx $85
$ffdf71 a6 a9         ldx $a9
$ffdf73 a5 a0         lda $a0
$ffdf75 00 20         brk $20
$ffdf77 b2 eb         lda ($eb)
$ffdf79 aa            tax
$ffdf7a 4c b0 df      jmp $dfb0
$ffdf7d a9 00 85      lda #$8500
$ffdf80 11 c6         ora ($c6),y
$ffdf82 89 20 00      bit #$0020
$ffdf85 e6 85         inc $85
$ffdf87 9d 86 9e      sta $9e86,x
$ffdf8a 84 9f         sty $9f
$ffdf8c a5 a8         lda $a8
$ffdf8e a4 a9         ldy $a9
$ffdf90 20 04 e6      jsr $e604
$ffdf93 86 a8         stx $a8
$ffdf95 84 a9         sty $a9
$ffdf97 aa            tax
$ffdf98 38            sec
$ffdf99 e5 9d         sbc $9d
$ffdf9b f0 08         beq $ffdfa5
$ffdf9d a9 01 90      lda #$9001
$ffdfa0 04 a6         tsb $a6
$ffdfa2 9d a9 ff      sta $ffa9,x
$ffdfa5 85 a2         sta $a2
$ffdfa7 a0 ff e8      ldy #$e8ff
$ffdfaa c8            iny
$ffdfab ca            dex
$ffdfac d0 07         bne $ffdfb5
$ffdfae a6 a2         ldx $a2
$ffdfb0 30 0f         bmi $ffdfc1
$ffdfb2 18            clc
$ffdfb3 90 0c         bcc $ffdfc1
$ffdfb5 b1 a8         lda ($a8),y
$ffdfb7 d1 9e         cmp ($9e),y
$ffdfb9 f0 ef         beq $ffdfaa
$ffdfbb a2 ff b0      ldx #$b0ff
$ffdfbe 02 a2         cop $a2
$ffdfc0 01 e8         ora ($e8,x)
$ffdfc2 8a            txa
$ffdfc3 2a            rol
$ffdfc4 25 16         and $16
$ffdfc6 f0 02         beq $ffdfca
$ffdfc8 a9 01 4c      lda #$4c01
$ffdfcb 93 eb         sta ($eb,sp),y
$ffdfcd 20 fb e6      jsr $e6fb
$ffdfd0 20 1e fb      jsr $fb1e
$ffdfd3 4c 01 e3      jmp $e301
$ffdfd6 20 be de      jsr $debe
$ffdfd9 aa            tax
$ffdfda 20 e8 df      jsr $dfe8
$ffdfdd 20 b7 00      jsr $00b7
$ffdfe0 d0 f4         bne $ffdfd6
$ffdfe2 60            rts
$ffdfe3 a2 00 20      ldx #$2000
$ffdfe6 b7 00         lda [$00],y
$ffdfe8 86 10         stx $10
$ffdfea 85 81         sta $81
$ffdfec 20 b7 00      jsr $00b7
$ffdfef 20 7d e0      jsr $e07d
$ffdff2 b0 03         bcs $ffdff7
$ffdff4 4c c9 de      jmp $dec9
$ffdff7 a2 00 86      ldx #$8600
$ffdffa 11 86         ora ($86),y
$ffdffc 12 4c         ora ($4c)
$ffdffe 07 e0         ora [$e0]
$ffe000 4c 28 f1      jmp $f128
$ffe003 4c 3c d4      jmp $d43c
$ffe006 00 20         brk $20
$ffe008 b1 00         lda ($00),y
$ffe00a 90 05         bcc $ffe011
$ffe00c 20 7d e0      jsr $e07d
$ffe00f 90 0b         bcc $ffe01c
$ffe011 aa            tax
$ffe012 20 b1 00      jsr $00b1
$ffe015 90 fb         bcc $ffe012
$ffe017 20 7d e0      jsr $e07d
$ffe01a b0 f6         bcs $ffe012
$ffe01c c9 24 d0      cmp #$d024
$ffe01f 06 a9         asl $a9
$ffe021 ff 85 11 d0   sbc $d01185,x
$ffe025 10 c9         bpl $ffdff0
$ffe027 25 d0         and $d0
$ffe029 13 a5         ora ($a5,sp),y
$ffe02b 14 30         trb $30
$ffe02d c6 a9         dec $a9
$ffe02f 80 85         bra $ffdfb6
$ffe031 12 05         ora ($05)
$ffe033 81 85         sta ($85,x)
$ffe035 81 8a         sta ($8a,x)
$ffe037 09 80 aa      ora #$aa80
$ffe03a 20 b1 00      jsr $00b1
$ffe03d 86 82         stx $82
$ffe03f 38            sec
$ffe040 05 14         ora $14
$ffe042 e9 28 d0      sbc #$d028
$ffe045 03 4c         ora $4c,sp
$ffe047 1e e1 24      asl $24e1,x
$ffe04a 14 30         trb $30
$ffe04c 02 70         cop $70
$ffe04e f7 a9         sbc [$a9],y
$ffe050 00 85         brk $85
$ffe052 14 a5         trb $a5
$ffe054 69 a6 6a      adc #$6aa6
$ffe057 a0 00 86      ldy #$8600
$ffe05a 9c 85 9b      stz $9b85
$ffe05d e4 6c         cpx $6c
$ffe05f d0 04         bne $ffe065
$ffe061 c5 6b         cmp $6b
$ffe063 f0 22         beq $ffe087
$ffe065 a5 81         lda $81
$ffe067 d1 9b         cmp ($9b),y
$ffe069 d0 08         bne $ffe073
$ffe06b a5 82         lda $82
$ffe06d c8            iny
$ffe06e d1 9b         cmp ($9b),y
$ffe070 f0 6c         beq $ffe0de
$ffe072 88            dey
$ffe073 18            clc
$ffe074 a5 9b         lda $9b
$ffe076 69 07 90      adc #$9007
$ffe079 e1 e8         sbc ($e8,x)
$ffe07b d0 dc         bne $ffe059
$ffe07d c9 41 90      cmp #$9041
$ffe080 05 e9         ora $e9
$ffe082 5b            tcd
$ffe083 38            sec
$ffe084 e9 a5 60      sbc #$60a5
$ffe087 68            pla
$ffe088 48            pha
$ffe089 c9 d7 d0      cmp #$d0d7
$ffe08c 0f ba bd 02   ora $02bdba
$ffe090 01 c9         ora ($c9,x)
$ffe092 de d0 07      dec $07d0,x
$ffe095 a9 9a a0      lda #$a09a
$ffe098 e0 60 00      cpx #$0060
$ffe09b 00 a5         brk $a5
$ffe09d 6b            rtl
$ffe09e a4 6c         ldy $6c
$ffe0a0 85 9b         sta $9b
$ffe0a2 84 9c         sty $9c
$ffe0a4 a5 6d         lda $6d
$ffe0a6 a4 6e         ldy $6e
$ffe0a8 85 96         sta $96
$ffe0aa 84 97         sty $97
$ffe0ac 18            clc
$ffe0ad 69 07 90      adc #$9007
$ffe0b0 01 c8         ora ($c8,x)
$ffe0b2 85 94         sta $94
$ffe0b4 84 95         sty $95
$ffe0b6 20 93 d3      jsr $d393
$ffe0b9 a5 94         lda $94
$ffe0bb a4 95         ldy $95
$ffe0bd c8            iny
$ffe0be 85 6b         sta $6b
$ffe0c0 84 6c         sty $6c
$ffe0c2 a0 00 a5      ldy #$a500
$ffe0c5 81 91         sta ($91,x)
$ffe0c7 9b            txy
$ffe0c8 c8            iny
$ffe0c9 a5 82         lda $82
$ffe0cb 91 9b         sta ($9b),y
$ffe0cd a9 00 c8      lda #$c800
$ffe0d0 91 9b         sta ($9b),y
$ffe0d2 c8            iny
$ffe0d3 91 9b         sta ($9b),y
$ffe0d5 c8            iny
$ffe0d6 91 9b         sta ($9b),y
$ffe0d8 c8            iny
$ffe0d9 91 9b         sta ($9b),y
$ffe0db c8            iny
$ffe0dc 91 9b         sta ($9b),y
$ffe0de a5 9b         lda $9b
$ffe0e0 18            clc
$ffe0e1 69 02 a4      adc #$a402
$ffe0e4 9c 90 01      stz $0190
$ffe0e7 c8            iny
$ffe0e8 85 83         sta $83
$ffe0ea 84 84         sty $84
$ffe0ec 60            rts
$ffe0ed a5 0f         lda $0f
$ffe0ef 0a            asl
$ffe0f0 69 05 65      adc #$6505
$ffe0f3 9b            txy
$ffe0f4 a4 9c         ldy $9c
$ffe0f6 90 01         bcc $ffe0f9
$ffe0f8 c8            iny
$ffe0f9 85 94         sta $94
$ffe0fb 84 95         sty $95
$ffe0fd 60            rts
$ffe0fe 90 80         bcc $ffe080
$ffe100 00 00         brk $00
$ffe102 20 b1 00      jsr $00b1
$ffe105 20 67 dd      jsr $dd67
$ffe108 a5 a2         lda $a2
$ffe10a 30 0d         bmi $ffe119
$ffe10c a5 9d         lda $9d
$ffe10e c9 90 90      cmp #$9090
$ffe111 09 a9 fe      ora #$fea9
$ffe114 a0 e0 20      ldy #$20e0
$ffe117 b2 eb         lda ($eb)
$ffe119 d0 7e         bne $ffe199
$ffe11b 4c f2 eb      jmp $ebf2
$ffe11e a5 14         lda $14
$ffe120 d0 47         bne $ffe169
$ffe122 a5 10         lda $10
$ffe124 05 12         ora $12
$ffe126 48            pha
$ffe127 a5 11         lda $11
$ffe129 48            pha
$ffe12a a0 00 98      ldy #$9800
$ffe12d 48            pha
$ffe12e a5 82         lda $82
$ffe130 48            pha
$ffe131 a5 81         lda $81
$ffe133 48            pha
$ffe134 20 02 e1      jsr $e102
$ffe137 68            pla
$ffe138 85 81         sta $81
$ffe13a 68            pla
$ffe13b 85 82         sta $82
$ffe13d 68            pla
$ffe13e a8            tay
$ffe13f ba            tsx
$ffe140 bd 02 01      lda $0102,x
$ffe143 48            pha
$ffe144 bd 01 01      lda $0101,x
$ffe147 48            pha
$ffe148 a5 a0         lda $a0
$ffe14a 9d 02 01      sta $0102,x
$ffe14d a5 a1         lda $a1
$ffe14f 9d 01 01      sta $0101,x
$ffe152 c8            iny
$ffe153 20 b7 00      jsr $00b7
$ffe156 c9 2c f0      cmp #$f02c
$ffe159 d2 84         cmp ($84)
$ffe15b 0f 20 b8 de   ora $deb820
$ffe15f 68            pla
$ffe160 85 11         sta $11
$ffe162 68            pla
$ffe163 85 12         sta $12
$ffe165 29 7f 85      and #$857f
$ffe168 10 a6         bpl $ffe110
$ffe16a 6b            rtl
$ffe16b a5 6c         lda $6c
$ffe16d 86 9b         stx $9b
$ffe16f 85 9c         sta $9c
$ffe171 c5 6e         cmp $6e
$ffe173 d0 04         bne $ffe179
$ffe175 e4 6d         cpx $6d
$ffe177 f0 3f         beq $ffe1b8
$ffe179 a0 00 b1      ldy #$b100
$ffe17c 9b            txy
$ffe17d c8            iny
$ffe17e c5 81         cmp $81
$ffe180 d0 06         bne $ffe188
$ffe182 a5 82         lda $82
$ffe184 d1 9b         cmp ($9b),y
$ffe186 f0 16         beq $ffe19e
$ffe188 c8            iny
$ffe189 b1 9b         lda ($9b),y
$ffe18b 18            clc
$ffe18c 65 9b         adc $9b
$ffe18e aa            tax
$ffe18f c8            iny
$ffe190 b1 9b         lda ($9b),y
$ffe192 65 9c         adc $9c
$ffe194 90 d7         bcc $ffe16d
$ffe196 a2 6b 2c      ldx #$2c6b
$ffe199 a2 35 4c      ldx #$4c35
$ffe19c 12 d4         ora ($d4)
$ffe19e a2 78 a5      ldx #$a578
$ffe1a1 10 d0         bpl $ffe173
$ffe1a3 f7 a5         sbc [$a5],y
$ffe1a5 14 f0         trb $f0
$ffe1a7 02 38         cop $38
$ffe1a9 60            rts
$ffe1aa 20 ed e0      jsr $e0ed
$ffe1ad a5 0f         lda $0f
$ffe1af a0 04 d1      ldy #$d104
$ffe1b2 9b            txy
$ffe1b3 d0 e1         bne $ffe196
$ffe1b5 4c 4b e2      jmp $e24b
$ffe1b8 a5 14         lda $14
$ffe1ba f0 05         beq $ffe1c1
$ffe1bc a2 2a 4c      ldx #$4c2a
$ffe1bf 12 d4         ora ($d4)
$ffe1c1 20 ed e0      jsr $e0ed
$ffe1c4 20 e3 d3      jsr $d3e3
$ffe1c7 a9 00 a8      lda #$a800
$ffe1ca 85 ae         sta $ae
$ffe1cc a2 05 a5      ldx #$a505
$ffe1cf 81 91         sta ($91,x)
$ffe1d1 9b            txy
$ffe1d2 10 01         bpl $ffe1d5
$ffe1d4 ca            dex
$ffe1d5 c8            iny
$ffe1d6 a5 82         lda $82
$ffe1d8 91 9b         sta ($9b),y
$ffe1da 10 02         bpl $ffe1de
$ffe1dc ca            dex
$ffe1dd ca            dex
$ffe1de 86 ad         stx $ad
$ffe1e0 a5 0f         lda $0f
$ffe1e2 c8            iny
$ffe1e3 c8            iny
$ffe1e4 c8            iny
$ffe1e5 91 9b         sta ($9b),y
$ffe1e7 a2 0b a9      ldx #$a90b
$ffe1ea 00 24         brk $24
$ffe1ec 10 50         bpl $ffe23e
$ffe1ee 08            php
$ffe1ef 68            pla
$ffe1f0 18            clc
$ffe1f1 69 01 aa      adc #$aa01
$ffe1f4 68            pla
$ffe1f5 69 00 c8      adc #$c800
$ffe1f8 91 9b         sta ($9b),y
$ffe1fa c8            iny
$ffe1fb 8a            txa
$ffe1fc 91 9b         sta ($9b),y
$ffe1fe 20 ad e2      jsr $e2ad
$ffe201 86 ad         stx $ad
$ffe203 85 ae         sta $ae
$ffe205 a4 5e         ldy $5e
$ffe207 c6 0f         dec $0f
$ffe209 d0 dc         bne $ffe1e7
$ffe20b 65 95         adc $95
$ffe20d b0 5d         bcs $ffe26c
$ffe20f 85 95         sta $95
$ffe211 a8            tay
$ffe212 8a            txa
$ffe213 65 94         adc $94
$ffe215 90 03         bcc $ffe21a
$ffe217 c8            iny
$ffe218 f0 52         beq $ffe26c
$ffe21a 20 e3 d3      jsr $d3e3
$ffe21d 85 6d         sta $6d
$ffe21f 84 6e         sty $6e
$ffe221 a9 00 e6      lda #$e600
$ffe224 ae a4 ad      ldx $ada4
$ffe227 f0 05         beq $ffe22e
$ffe229 88            dey
$ffe22a 91 94         sta ($94),y
$ffe22c d0 fb         bne $ffe229
$ffe22e c6 95         dec $95
$ffe230 c6 ae         dec $ae
$ffe232 d0 f5         bne $ffe229
$ffe234 e6 95         inc $95
$ffe236 38            sec
$ffe237 a5 6d         lda $6d
$ffe239 e5 9b         sbc $9b
$ffe23b a0 02 91      ldy #$9102
$ffe23e 9b            txy
$ffe23f a5 6e         lda $6e
$ffe241 c8            iny
$ffe242 e5 9c         sbc $9c
$ffe244 91 9b         sta ($9b),y
$ffe246 a5 10         lda $10
$ffe248 d0 62         bne $ffe2ac
$ffe24a c8            iny
$ffe24b b1 9b         lda ($9b),y
$ffe24d 85 0f         sta $0f
$ffe24f a9 00 85      lda #$8500
$ffe252 ad 85 ae      lda $ae85
$ffe255 c8            iny
$ffe256 68            pla
$ffe257 aa            tax
$ffe258 85 a0         sta $a0
$ffe25a 68            pla
$ffe25b 85 a1         sta $a1
$ffe25d d1 9b         cmp ($9b),y
$ffe25f 90 0e         bcc $ffe26f
$ffe261 d0 06         bne $ffe269
$ffe263 c8            iny
$ffe264 8a            txa
$ffe265 d1 9b         cmp ($9b),y
$ffe267 90 07         bcc $ffe270
$ffe269 4c 96 e1      jmp $e196
$ffe26c 4c 10 d4      jmp $d410
$ffe26f c8            iny
$ffe270 a5 ae         lda $ae
$ffe272 05 ad         ora $ad
$ffe274 18            clc
$ffe275 f0 0a         beq $ffe281
$ffe277 20 ad e2      jsr $e2ad
$ffe27a 8a            txa
$ffe27b 65 a0         adc $a0
$ffe27d aa            tax
$ffe27e 98            tya
$ffe27f a4 5e         ldy $5e
$ffe281 65 a1         adc $a1
$ffe283 86 ad         stx $ad
$ffe285 c6 0f         dec $0f
$ffe287 d0 ca         bne $ffe253
$ffe289 85 ae         sta $ae
$ffe28b a2 05 a5      ldx #$a505
$ffe28e 81 10         sta ($10,x)
$ffe290 01 ca         ora ($ca,x)
$ffe292 a5 82         lda $82
$ffe294 10 02         bpl $ffe298
$ffe296 ca            dex
$ffe297 ca            dex
$ffe298 86 64         stx $64
$ffe29a a9 00 20      lda #$2000
$ffe29d b6 e2         ldx $e2,y
$ffe29f 8a            txa
$ffe2a0 65 94         adc $94
$ffe2a2 85 83         sta $83
$ffe2a4 98            tya
$ffe2a5 65 95         adc $95
$ffe2a7 85 84         sta $84
$ffe2a9 a8            tay
$ffe2aa a5 83         lda $83
$ffe2ac 60            rts
$ffe2ad 84 5e         sty $5e
$ffe2af b1 9b         lda ($9b),y
$ffe2b1 85 64         sta $64
$ffe2b3 88            dey
$ffe2b4 b1 9b         lda ($9b),y
$ffe2b6 85 65         sta $65
$ffe2b8 a9 10 85      lda #$8510
$ffe2bb 99 a2 00      sta $00a2,y
$ffe2be a0 00 8a      ldy #$8a00
$ffe2c1 0a            asl
$ffe2c2 aa            tax
$ffe2c3 98            tya
$ffe2c4 2a            rol
$ffe2c5 a8            tay
$ffe2c6 b0 a4         bcs $ffe26c
$ffe2c8 06 ad         asl $ad
$ffe2ca 26 ae         rol $ae
$ffe2cc 90 0b         bcc $ffe2d9
$ffe2ce 18            clc
$ffe2cf 8a            txa
$ffe2d0 65 64         adc $64
$ffe2d2 aa            tax
$ffe2d3 98            tya
$ffe2d4 65 65         adc $65
$ffe2d6 a8            tay
$ffe2d7 b0 93         bcs $ffe26c
$ffe2d9 c6 99         dec $99
$ffe2db d0 e3         bne $ffe2c0
$ffe2dd 60            rts
$ffe2de a5 11         lda $11
$ffe2e0 f0 03         beq $ffe2e5
$ffe2e2 20 00 e6      jsr $e600
$ffe2e5 20 84 e4      jsr $e484
$ffe2e8 38            sec
$ffe2e9 a5 6f         lda $6f
$ffe2eb e5 6d         sbc $6d
$ffe2ed a8            tay
$ffe2ee a5 70         lda $70
$ffe2f0 e5 6e         sbc $6e
$ffe2f2 a2 00 86      ldx #$8600
$ffe2f5 11 85         ora ($85),y
$ffe2f7 9e 84 9f      stz $9f84,x
$ffe2fa a2 90 4c      ldx #$4c90
$ffe2fd 9b            txy
$ffe2fe eb            xba
$ffe2ff a4 24         ldy $24
$ffe301 a9 00 38      lda #$3800
$ffe304 f0 ec         beq $ffe2f2
$ffe306 a6 76         ldx $76
$ffe308 e8            inx
$ffe309 d0 a1         bne $ffe2ac
$ffe30b a2 95 2c      ldx #$2c95
$ffe30e a2 e0 4c      ldx #$4ce0
$ffe311 12 d4         ora ($d4)
$ffe313 20 41 e3      jsr $e341
$ffe316 20 06 e3      jsr $e306
$ffe319 20 bb de      jsr $debb
$ffe31c a9 80 85      lda #$8580
$ffe31f 14 20         trb $20
$ffe321 e3 df         sbc $df,sp
$ffe323 20 6a dd      jsr $dd6a
$ffe326 20 b8 de      jsr $deb8
$ffe329 a9 d0 20      lda #$20d0
$ffe32c c0 de 48      cpy #$48de
$ffe32f a5 84         lda $84
$ffe331 48            pha
$ffe332 a5 83         lda $83
$ffe334 48            pha
$ffe335 a5 b9         lda $b9
$ffe337 48            pha
$ffe338 a5 b8         lda $b8
$ffe33a 48            pha
$ffe33b 20 95 d9      jsr $d995
$ffe33e 4c af e3      jmp $e3af
$ffe341 a9 c2 20      lda #$20c2
$ffe344 c0 de 09      cpy #$09de
$ffe347 80 85         bra $ffe2ce
$ffe349 14 20         trb $20
$ffe34b ea            nop
$ffe34c df 85 8a 84   cmp $848a85,x
$ffe350 8b            phb
$ffe351 4c 6a dd      jmp $dd6a
$ffe354 20 41 e3      jsr $e341
$ffe357 a5 8b         lda $8b
$ffe359 48            pha
$ffe35a a5 8a         lda $8a
$ffe35c 48            pha
$ffe35d 20 b2 de      jsr $deb2
$ffe360 20 6a dd      jsr $dd6a
$ffe363 68            pla
$ffe364 85 8a         sta $8a
$ffe366 68            pla
$ffe367 85 8b         sta $8b
$ffe369 a0 02 b1      ldy #$b102
$ffe36c 8a            txa
$ffe36d 85 83         sta $83
$ffe36f aa            tax
$ffe370 c8            iny
$ffe371 b1 8a         lda ($8a),y
$ffe373 f0 99         beq $ffe30e
$ffe375 85 84         sta $84
$ffe377 c8            iny
$ffe378 b1 83         lda ($83),y
$ffe37a 48            pha
$ffe37b 88            dey
$ffe37c 10 fa         bpl $ffe378
$ffe37e a4 84         ldy $84
$ffe380 20 2b eb      jsr $eb2b
$ffe383 a5 b9         lda $b9
$ffe385 48            pha
$ffe386 a5 b8         lda $b8
$ffe388 48            pha
$ffe389 b1 8a         lda ($8a),y
$ffe38b 85 b8         sta $b8
$ffe38d c8            iny
$ffe38e b1 8a         lda ($8a),y
$ffe390 85 b9         sta $b9
$ffe392 a5 84         lda $84
$ffe394 48            pha
$ffe395 a5 83         lda $83
$ffe397 48            pha
$ffe398 20 67 dd      jsr $dd67
$ffe39b 68            pla
$ffe39c 85 8a         sta $8a
$ffe39e 68            pla
$ffe39f 85 8b         sta $8b
$ffe3a1 20 b7 00      jsr $00b7
$ffe3a4 f0 03         beq $ffe3a9
$ffe3a6 4c c9 de      jmp $dec9
$ffe3a9 68            pla
$ffe3aa 85 b8         sta $b8
$ffe3ac 68            pla
$ffe3ad 85 b9         sta $b9
$ffe3af a0 00 68      ldy #$6800
$ffe3b2 91 8a         sta ($8a),y
$ffe3b4 68            pla
$ffe3b5 c8            iny
$ffe3b6 91 8a         sta ($8a),y
$ffe3b8 68            pla
$ffe3b9 c8            iny
$ffe3ba 91 8a         sta ($8a),y
$ffe3bc 68            pla
$ffe3bd c8            iny
$ffe3be 91 8a         sta ($8a),y
$ffe3c0 68            pla
$ffe3c1 c8            iny
$ffe3c2 91 8a         sta ($8a),y
$ffe3c4 60            rts
$ffe3c5 20 6a dd      jsr $dd6a
$ffe3c8 a0 00 20      ldy #$2000
$ffe3cb 36 ed         rol $ed,x
$ffe3cd 68            pla
$ffe3ce 68            pla
$ffe3cf a9 ff a0      lda #$a0ff
$ffe3d2 00 f0         brk $f0
$ffe3d4 12 a6         ora ($a6)
$ffe3d6 a0 a4 a1      ldy #$a1a4
$ffe3d9 86 8c         stx $8c
$ffe3db 84 8d         sty $8d
$ffe3dd 20 52 e4      jsr $e452
$ffe3e0 86 9e         stx $9e
$ffe3e2 84 9f         sty $9f
$ffe3e4 85 9d         sta $9d
$ffe3e6 60            rts
$ffe3e7 a2 22 86      ldx #$8622
$ffe3ea 0d 86 0e      ora $0e86
$ffe3ed 85 ab         sta $ab
$ffe3ef 84 ac         sty $ac
$ffe3f1 85 9e         sta $9e
$ffe3f3 84 9f         sty $9f
$ffe3f5 a0 ff c8      ldy #$c8ff
$ffe3f8 b1 ab         lda ($ab),y
$ffe3fa f0 0c         beq $ffe408
$ffe3fc c5 0d         cmp $0d
$ffe3fe f0 04         beq $ffe404
$ffe400 c5 0e         cmp $0e
$ffe402 d0 f3         bne $ffe3f7
$ffe404 c9 22 f0      cmp #$f022
$ffe407 01 18         ora ($18,x)
$ffe409 84 9d         sty $9d
$ffe40b 98            tya
$ffe40c 65 ab         adc $ab
$ffe40e 85 ad         sta $ad
$ffe410 a6 ac         ldx $ac
$ffe412 90 01         bcc $ffe415
$ffe414 e8            inx
$ffe415 86 ae         stx $ae
$ffe417 a5 ac         lda $ac
$ffe419 f0 04         beq $ffe41f
$ffe41b c9 02 d0      cmp #$d002
$ffe41e 0b            phd
$ffe41f 98            tya
$ffe420 20 d5 e3      jsr $e3d5
$ffe423 a6 ab         ldx $ab
$ffe425 a4 ac         ldy $ac
$ffe427 20 e2 e5      jsr $e5e2
$ffe42a a6 52         ldx $52
$ffe42c e0 5e d0      cpx #$d05e
$ffe42f 05 a2         ora $a2
$ffe431 bf 4c 12 d4   lda $d4124c,x
$ffe435 a5 9d         lda $9d
$ffe437 95 00         sta $00,x
$ffe439 a5 9e         lda $9e
$ffe43b 95 01         sta $01,x
$ffe43d a5 9f         lda $9f
$ffe43f 95 02         sta $02,x
$ffe441 a0 00 86      ldy #$8600
$ffe444 a0 84 a1      ldy #$a184
$ffe447 88            dey
$ffe448 84 11         sty $11
$ffe44a 86 53         stx $53
$ffe44c e8            inx
$ffe44d e8            inx
$ffe44e e8            inx
$ffe44f 86 52         stx $52
$ffe451 60            rts
$ffe452 46 13         lsr $13
$ffe454 48            pha
$ffe455 49 ff 38      eor #$38ff
$ffe458 65 6f         adc $6f
$ffe45a a4 70         ldy $70
$ffe45c b0 01         bcs $ffe45f
$ffe45e 88            dey
$ffe45f c4 6e         cpy $6e
$ffe461 90 11         bcc $ffe474
$ffe463 d0 04         bne $ffe469
$ffe465 c5 6d         cmp $6d
$ffe467 90 0b         bcc $ffe474
$ffe469 85 6f         sta $6f
$ffe46b 84 70         sty $70
$ffe46d 85 71         sta $71
$ffe46f 84 72         sty $72
$ffe471 aa            tax
$ffe472 68            pla
$ffe473 60            rts
$ffe474 a2 4d a5      ldx #$a54d
$ffe477 13 30         ora ($30,sp),y
$ffe479 b8            clv
$ffe47a 20 84 e4      jsr $e484
$ffe47d a9 80 85      lda #$8580
$ffe480 13 68         ora ($68,sp),y
$ffe482 d0 d0         bne $ffe454
$ffe484 a6 73         ldx $73
$ffe486 a5 74         lda $74
$ffe488 86 6f         stx $6f
$ffe48a 85 70         sta $70
$ffe48c a0 00 84      ldy #$8400
$ffe48f 8b            phb
$ffe490 a5 6d         lda $6d
$ffe492 a6 6e         ldx $6e
$ffe494 85 9b         sta $9b
$ffe496 86 9c         stx $9c
$ffe498 a9 55 a2      lda #$a255
$ffe49b 00 85         brk $85
$ffe49d 5e 86 5f      lsr $5f86,x
$ffe4a0 c5 52         cmp $52
$ffe4a2 f0 05         beq $ffe4a9
$ffe4a4 20 23 e5      jsr $e523
$ffe4a7 f0 f7         beq $ffe4a0
$ffe4a9 a9 07 85      lda #$8507
$ffe4ac 8f a5 69 a6   sta $a669a5
$ffe4b0 6a            ror
$ffe4b1 85 5e         sta $5e
$ffe4b3 86 5f         stx $5f
$ffe4b5 e4 6c         cpx $6c
$ffe4b7 d0 04         bne $ffe4bd
$ffe4b9 c5 6b         cmp $6b
$ffe4bb f0 05         beq $ffe4c2
$ffe4bd 20 19 e5      jsr $e519
$ffe4c0 f0 f3         beq $ffe4b5
$ffe4c2 85 94         sta $94
$ffe4c4 86 95         stx $95
$ffe4c6 a9 03 85      lda #$8503
$ffe4c9 8f a5 94 a6   sta $a694a5
$ffe4cd 95 e4         sta $e4,x
$ffe4cf 6e d0 07      ror $07d0
$ffe4d2 c5 6d         cmp $6d
$ffe4d4 d0 03         bne $ffe4d9
$ffe4d6 4c 62 e5      jmp $e562
$ffe4d9 85 5e         sta $5e
$ffe4db 86 5f         stx $5f
$ffe4dd a0 00 b1      ldy #$b100
$ffe4e0 5e aa c8      lsr $c8aa,x
$ffe4e3 b1 5e         lda ($5e),y
$ffe4e5 08            php
$ffe4e6 c8            iny
$ffe4e7 b1 5e         lda ($5e),y
$ffe4e9 65 94         adc $94
$ffe4eb 85 94         sta $94
$ffe4ed c8            iny
$ffe4ee b1 5e         lda ($5e),y
$ffe4f0 65 95         adc $95
$ffe4f2 85 95         sta $95
$ffe4f4 28            plp
$ffe4f5 10 d3         bpl $ffe4ca
$ffe4f7 8a            txa
$ffe4f8 30 d0         bmi $ffe4ca
$ffe4fa c8            iny
$ffe4fb b1 5e         lda ($5e),y
$ffe4fd a0 00 0a      ldy #$0a00
$ffe500 69 05 65      adc #$6505
$ffe503 5e 85 5e      lsr $5e85,x
$ffe506 90 02         bcc $ffe50a
$ffe508 e6 5f         inc $5f
$ffe50a a6 5f         ldx $5f
$ffe50c e4 95         cpx $95
$ffe50e d0 04         bne $ffe514
$ffe510 c5 94         cmp $94
$ffe512 f0 ba         beq $ffe4ce
$ffe514 20 23 e5      jsr $e523
$ffe517 f0 f3         beq $ffe50c
$ffe519 b1 5e         lda ($5e),y
$ffe51b 30 35         bmi $ffe552
$ffe51d c8            iny
$ffe51e b1 5e         lda ($5e),y
$ffe520 10 30         bpl $ffe552
$ffe522 c8            iny
$ffe523 b1 5e         lda ($5e),y
$ffe525 f0 2b         beq $ffe552
$ffe527 c8            iny
$ffe528 b1 5e         lda ($5e),y
$ffe52a aa            tax
$ffe52b c8            iny
$ffe52c b1 5e         lda ($5e),y
$ffe52e c5 70         cmp $70
$ffe530 90 06         bcc $ffe538
$ffe532 d0 1e         bne $ffe552
$ffe534 e4 6f         cpx $6f
$ffe536 b0 1a         bcs $ffe552
$ffe538 c5 9c         cmp $9c
$ffe53a 90 16         bcc $ffe552
$ffe53c d0 04         bne $ffe542
$ffe53e e4 9b         cpx $9b
$ffe540 90 10         bcc $ffe552
$ffe542 86 9b         stx $9b
$ffe544 85 9c         sta $9c
$ffe546 a5 5e         lda $5e
$ffe548 a6 5f         ldx $5f
$ffe54a 85 8a         sta $8a
$ffe54c 86 8b         stx $8b
$ffe54e a5 8f         lda $8f
$ffe550 85 91         sta $91
$ffe552 a5 8f         lda $8f
$ffe554 18            clc
$ffe555 65 5e         adc $5e
$ffe557 85 5e         sta $5e
$ffe559 90 02         bcc $ffe55d
$ffe55b e6 5f         inc $5f
$ffe55d a6 5f         ldx $5f
$ffe55f a0 00 60      ldy #$6000
$ffe562 a6 8b         ldx $8b
$ffe564 f0 f7         beq $ffe55d
$ffe566 a5 91         lda $91
$ffe568 29 04 4a      and #$4a04
$ffe56b a8            tay
$ffe56c 85 91         sta $91
$ffe56e b1 8a         lda ($8a),y
$ffe570 65 9b         adc $9b
$ffe572 85 96         sta $96
$ffe574 a5 9c         lda $9c
$ffe576 69 00 85      adc #$8500
$ffe579 97 a5         sta [$a5],y
$ffe57b 6f a6 70 85   adc $8570a6
$ffe57f 94 86         sty $86,x
$ffe581 95 20         sta $20,x
$ffe583 9a            txs
$ffe584 d3 a4         cmp ($a4,sp),y
$ffe586 91 c8         sta ($c8),y
$ffe588 a5 94         lda $94
$ffe58a 91 8a         sta ($8a),y
$ffe58c aa            tax
$ffe58d e6 95         inc $95
$ffe58f a5 95         lda $95
$ffe591 c8            iny
$ffe592 91 8a         sta ($8a),y
$ffe594 4c 88 e4      jmp $e488
$ffe597 a5 a1         lda $a1
$ffe599 48            pha
$ffe59a a5 a0         lda $a0
$ffe59c 48            pha
$ffe59d 20 60 de      jsr $de60
$ffe5a0 20 6c dd      jsr $dd6c
$ffe5a3 68            pla
$ffe5a4 85 ab         sta $ab
$ffe5a6 68            pla
$ffe5a7 85 ac         sta $ac
$ffe5a9 a0 00 b1      ldy #$b100
$ffe5ac ab            plb
$ffe5ad 18            clc
$ffe5ae 71 a0         adc ($a0),y
$ffe5b0 90 05         bcc $ffe5b7
$ffe5b2 a2 b0 4c      ldx #$4cb0
$ffe5b5 12 d4         ora ($d4)
$ffe5b7 20 d5 e3      jsr $e3d5
$ffe5ba 20 d4 e5      jsr $e5d4
$ffe5bd a5 8c         lda $8c
$ffe5bf a4 8d         ldy $8d
$ffe5c1 20 04 e6      jsr $e604
$ffe5c4 20 e6 e5      jsr $e5e6
$ffe5c7 a5 ab         lda $ab
$ffe5c9 a4 ac         ldy $ac
$ffe5cb 20 04 e6      jsr $e604
$ffe5ce 20 2a e4      jsr $e42a
$ffe5d1 4c 95 dd      jmp $dd95
$ffe5d4 a0 00 b1      ldy #$b100
$ffe5d7 ab            plb
$ffe5d8 48            pha
$ffe5d9 c8            iny
$ffe5da b1 ab         lda ($ab),y
$ffe5dc aa            tax
$ffe5dd c8            iny
$ffe5de b1 ab         lda ($ab),y
$ffe5e0 a8            tay
$ffe5e1 68            pla
$ffe5e2 86 5e         stx $5e
$ffe5e4 84 5f         sty $5f
$ffe5e6 a8            tay
$ffe5e7 f0 0a         beq $ffe5f3
$ffe5e9 48            pha
$ffe5ea 88            dey
$ffe5eb b1 5e         lda ($5e),y
$ffe5ed 91 71         sta ($71),y
$ffe5ef 98            tya
$ffe5f0 d0 f8         bne $ffe5ea
$ffe5f2 68            pla
$ffe5f3 18            clc
$ffe5f4 65 71         adc $71
$ffe5f6 85 71         sta $71
$ffe5f8 90 02         bcc $ffe5fc
$ffe5fa e6 72         inc $72
$ffe5fc 60            rts
$ffe5fd 20 6c dd      jsr $dd6c
$ffe600 a5 a0         lda $a0
$ffe602 a4 a1         ldy $a1
$ffe604 85 5e         sta $5e
$ffe606 84 5f         sty $5f
$ffe608 20 35 e6      jsr $e635
$ffe60b 08            php
$ffe60c a0 00 b1      ldy #$b100
$ffe60f 5e 48 c8      lsr $c848,x
$ffe612 b1 5e         lda ($5e),y
$ffe614 aa            tax
$ffe615 c8            iny
$ffe616 b1 5e         lda ($5e),y
$ffe618 a8            tay
$ffe619 68            pla
$ffe61a 28            plp
$ffe61b d0 13         bne $ffe630
$ffe61d c4 70         cpy $70
$ffe61f d0 0f         bne $ffe630
$ffe621 e4 6f         cpx $6f
$ffe623 d0 0b         bne $ffe630
$ffe625 48            pha
$ffe626 18            clc
$ffe627 65 6f         adc $6f
$ffe629 85 6f         sta $6f
$ffe62b 90 02         bcc $ffe62f
$ffe62d e6 70         inc $70
$ffe62f 68            pla
$ffe630 86 5e         stx $5e
$ffe632 84 5f         sty $5f
$ffe634 60            rts
$ffe635 c4 54         cpy $54
$ffe637 d0 0c         bne $ffe645
$ffe639 c5 53         cmp $53
$ffe63b d0 08         bne $ffe645
$ffe63d 85 52         sta $52
$ffe63f e9 03 85      sbc #$8503
$ffe642 53 a0         eor ($a0,sp),y
$ffe644 00 60         brk $60
$ffe646 20 fb e6      jsr $e6fb
$ffe649 8a            txa
$ffe64a 48            pha
$ffe64b a9 01 20      lda #$2001
$ffe64e dd e3 68      cmp $68e3,x
$ffe651 a0 00 91      ldy #$9100
$ffe654 9e 68 68      stz $6868,x
$ffe657 4c 2a e4      jmp $e42a
$ffe65a 20 b9 e6      jsr $e6b9
$ffe65d d1 8c         cmp ($8c),y
$ffe65f 98            tya
$ffe660 90 04         bcc $ffe666
$ffe662 b1 8c         lda ($8c),y
$ffe664 aa            tax
$ffe665 98            tya
$ffe666 48            pha
$ffe667 8a            txa
$ffe668 48            pha
$ffe669 20 dd e3      jsr $e3dd
$ffe66c a5 8c         lda $8c
$ffe66e a4 8d         ldy $8d
$ffe670 20 04 e6      jsr $e604
$ffe673 68            pla
$ffe674 a8            tay
$ffe675 68            pla
$ffe676 18            clc
$ffe677 65 5e         adc $5e
$ffe679 85 5e         sta $5e
$ffe67b 90 02         bcc $ffe67f
$ffe67d e6 5f         inc $5f
$ffe67f 98            tya
$ffe680 20 e6 e5      jsr $e5e6
$ffe683 4c 2a e4      jmp $e42a
$ffe686 20 b9 e6      jsr $e6b9
$ffe689 18            clc
$ffe68a f1 8c         sbc ($8c),y
$ffe68c 49 ff 4c      eor #$4cff
$ffe68f 60            rts
$ffe690 e6 a9         inc $a9
$ffe692 ff 85 a1 20   sbc $20a185,x
$ffe696 b7 00         lda [$00],y
$ffe698 c9 29 f0      cmp #$f029
$ffe69b 06 20         asl $20
$ffe69d be de 20      ldx $20de,y
$ffe6a0 f8            sed
$ffe6a1 e6 20         inc $20
$ffe6a3 b9 e6 ca      lda $cae6,y
$ffe6a6 8a            txa
$ffe6a7 48            pha
$ffe6a8 18            clc
$ffe6a9 a2 00 f1      ldx #$f100
$ffe6ac 8c b0 b8      sty $b8b0
$ffe6af 49 ff c5      eor #$c5ff
$ffe6b2 a1 90         lda ($90,x)
$ffe6b4 b3 a5         lda ($a5,sp),y
$ffe6b6 a1 b0         lda ($b0,x)
$ffe6b8 af 20 b8 de   lda $deb820
$ffe6bc 68            pla
$ffe6bd a8            tay
$ffe6be 68            pla
$ffe6bf 85 91         sta $91
$ffe6c1 68            pla
$ffe6c2 68            pla
$ffe6c3 68            pla
$ffe6c4 aa            tax
$ffe6c5 68            pla
$ffe6c6 85 8c         sta $8c
$ffe6c8 68            pla
$ffe6c9 85 8d         sta $8d
$ffe6cb a5 91         lda $91
$ffe6cd 48            pha
$ffe6ce 98            tya
$ffe6cf 48            pha
$ffe6d0 a0 00 8a      ldy #$8a00
$ffe6d3 f0 1d         beq $ffe6f2
$ffe6d5 60            rts
$ffe6d6 20 dc e6      jsr $e6dc
$ffe6d9 4c 01 e3      jmp $e301
$ffe6dc 20 fd e5      jsr $e5fd
$ffe6df a2 00 86      ldx #$8600
$ffe6e2 11 a8         ora ($a8),y
$ffe6e4 60            rts
$ffe6e5 20 dc e6      jsr $e6dc
$ffe6e8 f0 08         beq $ffe6f2
$ffe6ea a0 00 b1      ldy #$b100
$ffe6ed 5e a8 4c      lsr $4ca8,x
$ffe6f0 01 e3         ora ($e3,x)
$ffe6f2 4c 99 e1      jmp $e199
$ffe6f5 20 b1 00      jsr $00b1
$ffe6f8 20 67 dd      jsr $dd67
$ffe6fb 20 08 e1      jsr $e108
$ffe6fe a6 a0         ldx $a0
$ffe700 d0 f0         bne $ffe6f2
$ffe702 a6 a1         ldx $a1
$ffe704 4c b7 00      jmp $00b7
$ffe707 20 dc e6      jsr $e6dc
$ffe70a d0 03         bne $ffe70f
$ffe70c 4c 4e e8      jmp $e84e
$ffe70f a6 b8         ldx $b8
$ffe711 a4 b9         ldy $b9
$ffe713 86 ad         stx $ad
$ffe715 84 ae         sty $ae
$ffe717 a6 5e         ldx $5e
$ffe719 86 b8         stx $b8
$ffe71b 18            clc
$ffe71c 65 5e         adc $5e
$ffe71e 85 60         sta $60
$ffe720 a6 5f         ldx $5f
$ffe722 86 b9         stx $b9
$ffe724 90 01         bcc $ffe727
$ffe726 e8            inx
$ffe727 86 61         stx $61
$ffe729 a0 00 b1      ldy #$b100
$ffe72c 60            rts
$ffe72d 48            pha
$ffe72e a9 00 91      lda #$9100
$ffe731 60            rts
$ffe732 20 b7 00      jsr $00b7
$ffe735 20 4a ec      jsr $ec4a
$ffe738 68            pla
$ffe739 a0 00 91      ldy #$9100
$ffe73c 60            rts
$ffe73d a6 ad         ldx $ad
$ffe73f a4 ae         ldy $ae
$ffe741 86 b8         stx $b8
$ffe743 84 b9         sty $b9
$ffe745 60            rts
$ffe746 20 67 dd      jsr $dd67
$ffe749 20 52 e7      jsr $e752
$ffe74c 20 be de      jsr $debe
$ffe74f 4c f8 e6      jmp $e6f8
$ffe752 a5 9d         lda $9d
$ffe754 c9 91 b0      cmp #$b091
$ffe757 9a            txs
$ffe758 20 f2 eb      jsr $ebf2
$ffe75b a5 a0         lda $a0
$ffe75d a4 a1         ldy $a1
$ffe75f 84 50         sty $50
$ffe761 85 51         sta $51
$ffe763 60            rts
$ffe764 a5 50         lda $50
$ffe766 48            pha
$ffe767 a5 51         lda $51
$ffe769 48            pha
$ffe76a 20 52 e7      jsr $e752
$ffe76d a0 00 b1      ldy #$b100
$ffe770 50 a8         bvc $ffe71a
$ffe772 68            pla
$ffe773 85 51         sta $51
$ffe775 68            pla
$ffe776 85 50         sta $50
$ffe778 4c 01 e3      jmp $e301
$ffe77b 20 46 e7      jsr $e746
$ffe77e 8a            txa
$ffe77f a0 00 91      ldy #$9100
$ffe782 50 60         bvc $ffe7e4
$ffe784 20 46 e7      jsr $e746
$ffe787 86 85         stx $85
$ffe789 a2 00 20      ldx #$2000
$ffe78c b7 00         lda [$00],y
$ffe78e f0 03         beq $ffe793
$ffe790 20 4c e7      jsr $e74c
$ffe793 86 86         stx $86
$ffe795 a0 00 b1      ldy #$b100
$ffe798 50 45         bvc $ffe7df
$ffe79a 86 25         stx $25
$ffe79c 85 f0         sta $f0
$ffe79e f8            sed
$ffe79f 60            rts
$ffe7a0 a9 64 a0      lda #$a064
$ffe7a3 ee 4c be      inc $be4c
$ffe7a6 e7 20         sbc [$20]
$ffe7a8 e3 e9         sbc $e9,sp
$ffe7aa a5 a2         lda $a2
$ffe7ac 49 ff 85      eor #$85ff
$ffe7af a2 45 aa      ldx #$aa45
$ffe7b2 85 ab         sta $ab
$ffe7b4 a5 9d         lda $9d
$ffe7b6 4c c1 e7      jmp $e7c1
$ffe7b9 20 f0 e8      jsr $e8f0
$ffe7bc 90 3c         bcc $ffe7fa
$ffe7be 20 e3 e9      jsr $e9e3
$ffe7c1 d0 03         bne $ffe7c6
$ffe7c3 4c 53 eb      jmp $eb53
$ffe7c6 a6 ac         ldx $ac
$ffe7c8 86 92         stx $92
$ffe7ca a2 a5 a5      ldx #$a5a5
$ffe7cd a5 a8         lda $a8
$ffe7cf f0 ce         beq $ffe79f
$ffe7d1 38            sec
$ffe7d2 e5 9d         sbc $9d
$ffe7d4 f0 24         beq $ffe7fa
$ffe7d6 90 12         bcc $ffe7ea
$ffe7d8 84 9d         sty $9d
$ffe7da a4 aa         ldy $aa
$ffe7dc 84 a2         sty $a2
$ffe7de 49 ff 69      eor #$69ff
$ffe7e1 00 a0         brk $a0
$ffe7e3 00 84         brk $84
$ffe7e5 92 a2         sta ($a2)
$ffe7e7 9d d0 04      sta $04d0,x
$ffe7ea a0 00 84      ldy #$8400
$ffe7ed ac c9 f9      ldy $f9c9
$ffe7f0 30 c7         bmi $ffe7b9
$ffe7f2 a8            tay
$ffe7f3 a5 ac         lda $ac
$ffe7f5 56 01         lsr $01,x
$ffe7f7 20 07 e9      jsr $e907
$ffe7fa 24 ab         bit $ab
$ffe7fc 10 57         bpl $ffe855
$ffe7fe a0 9d e0      ldy #$e09d
$ffe801 a5 f0         lda $f0
$ffe803 02 a0         cop $a0
$ffe805 a5 38         lda $38
$ffe807 49 ff 65      eor #$65ff
$ffe80a 92 85         sta ($85)
$ffe80c ac b9 04      ldy $04b9
$ffe80f 00 f5         brk $f5
$ffe811 04 85         tsb $85
$ffe813 a1 b9         lda ($b9,x)
$ffe815 03 00         ora $00,sp
$ffe817 f5 03         sbc $03,x
$ffe819 85 a0         sta $a0
$ffe81b b9 02 00      lda $0002,y
$ffe81e f5 02         sbc $02,x
$ffe820 85 9f         sta $9f
$ffe822 b9 01 00      lda $0001,y
$ffe825 f5 01         sbc $01,x
$ffe827 85 9e         sta $9e
$ffe829 b0 03         bcs $ffe82e
$ffe82b 20 9e e8      jsr $e89e
$ffe82e a0 00 98      ldy #$9800
$ffe831 18            clc
$ffe832 a6 9e         ldx $9e
$ffe834 d0 4a         bne $ffe880
$ffe836 a6 9f         ldx $9f
$ffe838 86 9e         stx $9e
$ffe83a a6 a0         ldx $a0
$ffe83c 86 9f         stx $9f
$ffe83e a6 a1         ldx $a1
$ffe840 86 a0         stx $a0
$ffe842 a6 ac         ldx $ac
$ffe844 86 a1         stx $a1
$ffe846 84 ac         sty $ac
$ffe848 69 08 c9      adc #$c908
$ffe84b 20 d0 e4      jsr $e4d0
$ffe84e a9 00 85      lda #$8500
$ffe851 9d 85 a2      sta $a285,x
$ffe854 60            rts
$ffe855 65 92         adc $92
$ffe857 85 ac         sta $ac
$ffe859 a5 a1         lda $a1
$ffe85b 65 a9         adc $a9
$ffe85d 85 a1         sta $a1
$ffe85f a5 a0         lda $a0
$ffe861 65 a8         adc $a8
$ffe863 85 a0         sta $a0
$ffe865 a5 9f         lda $9f
$ffe867 65 a7         adc $a7
$ffe869 85 9f         sta $9f
$ffe86b a5 9e         lda $9e
$ffe86d 65 a6         adc $a6
$ffe86f 85 9e         sta $9e
$ffe871 4c 8d e8      jmp $e88d
$ffe874 69 01 06      adc #$0601
$ffe877 ac 26 a1      ldy $a126
$ffe87a 26 a0         rol $a0
$ffe87c 26 9f         rol $9f
$ffe87e 26 9e         rol $9e
$ffe880 10 f2         bpl $ffe874
$ffe882 38            sec
$ffe883 e5 9d         sbc $9d
$ffe885 b0 c7         bcs $ffe84e
$ffe887 49 ff 69      eor #$69ff
$ffe88a 01 85         ora ($85,x)
$ffe88c 9d 90 0e      sta $0e90,x
$ffe88f e6 9d         inc $9d
$ffe891 f0 42         beq $ffe8d5
$ffe893 66 9e         ror $9e
$ffe895 66 9f         ror $9f
$ffe897 66 a0         ror $a0
$ffe899 66 a1         ror $a1
$ffe89b 66 ac         ror $ac
$ffe89d 60            rts
$ffe89e a5 a2         lda $a2
$ffe8a0 49 ff 85      eor #$85ff
$ffe8a3 a2 a5 9e      ldx #$9ea5
$ffe8a6 49 ff 85      eor #$85ff
$ffe8a9 9e a5 9f      stz $9fa5,x
$ffe8ac 49 ff 85      eor #$85ff
$ffe8af 9f a5 a0 49   sta $49a0a5,x
$ffe8b3 ff 85 a0 a5   sbc $a5a085,x
$ffe8b7 a1 49         lda ($49,x)
$ffe8b9 ff 85 a1 a5   sbc $a5a185,x
$ffe8bd ac 49 ff      ldy $ff49
$ffe8c0 85 ac         sta $ac
$ffe8c2 e6 ac         inc $ac
$ffe8c4 d0 0e         bne $ffe8d4
$ffe8c6 e6 a1         inc $a1
$ffe8c8 d0 0a         bne $ffe8d4
$ffe8ca e6 a0         inc $a0
$ffe8cc d0 06         bne $ffe8d4
$ffe8ce e6 9f         inc $9f
$ffe8d0 d0 02         bne $ffe8d4
$ffe8d2 e6 9e         inc $9e
$ffe8d4 60            rts
$ffe8d5 a2 45 4c      ldx #$4c45
$ffe8d8 12 d4         ora ($d4)
$ffe8da a2 61 b4      ldx #$b461
$ffe8dd 04 84         tsb $84
$ffe8df ac b4 03      ldy $03b4
$ffe8e2 94 04         sty $04,x
$ffe8e4 b4 02         ldy $02,x
$ffe8e6 94 03         sty $03,x
$ffe8e8 b4 01         ldy $01,x
$ffe8ea 94 02         sty $02,x
$ffe8ec a4 a4         ldy $a4
$ffe8ee 94 01         sty $01,x
$ffe8f0 69 08 30      adc #$3008
$ffe8f3 e8            inx
$ffe8f4 f0 e6         beq $ffe8dc
$ffe8f6 e9 08 a8      sbc #$a808
$ffe8f9 a5 ac         lda $ac
$ffe8fb b0 14         bcs $ffe911
$ffe8fd 16 01         asl $01,x
$ffe8ff 90 02         bcc $ffe903
$ffe901 f6 01         inc $01,x
$ffe903 76 01         ror $01,x
$ffe905 76 01         ror $01,x
$ffe907 76 02         ror $02,x
$ffe909 76 03         ror $03,x
$ffe90b 76 04         ror $04,x
$ffe90d 6a            ror
$ffe90e c8            iny
$ffe90f d0 ec         bne $ffe8fd
$ffe911 18            clc
$ffe912 60            rts
$ffe913 81 00         sta ($00,x)
$ffe915 00 00         brk $00
$ffe917 00 03         brk $03
$ffe919 7f 5e 56 cb   adc $cb565e,x
$ffe91d 79 80 13      adc $1380,y
$ffe920 9b            txy
$ffe921 0b            phd
$ffe922 64 80         stz $80
$ffe924 76 38         ror $38,x
$ffe926 93 16         sta ($16,sp),y
$ffe928 82 38 aa      brl $ff9363
$ffe92b 3b            tsc
$ffe92c 20 80 35      jsr $3580
$ffe92f 04 f3         tsb $f3
$ffe931 34 81         bit $81,x
$ffe933 35 04         and $04,x
$ffe935 f3 34         sbc ($34,sp),y
$ffe937 80 80         bra $ffe8b9
$ffe939 00 00         brk $00
$ffe93b 00 80         brk $80
$ffe93d 31 72         and ($72),y
$ffe93f 17 f8         ora [$f8],y
$ffe941 20 82 eb      jsr $eb82
$ffe944 f0 02         beq $ffe948
$ffe946 10 03         bpl $ffe94b
$ffe948 4c 99 e1      jmp $e199
$ffe94b a5 9d         lda $9d
$ffe94d e9 7f 48      sbc #$487f
$ffe950 a9 80 85      lda #$8580
$ffe953 9d a9 2d      sta $2da9,x
$ffe956 a0 e9 20      ldy #$20e9
$ffe959 be e7 a9      ldx $a9e7,y
$ffe95c 32 a0         and ($a0)
$ffe95e e9 20 66      sbc #$6620
$ffe961 ea            nop
$ffe962 a9 13 a0      lda #$a013
$ffe965 e9 20 a7      sbc #$a720
$ffe968 e7 a9         sbc [$a9]
$ffe96a 18            clc
$ffe96b a0 e9 20      ldy #$20e9
$ffe96e 5c ef a9 37   jmp $37a9ef
$ffe972 a0 e9 20      ldy #$20e9
$ffe975 be e7 68      ldx $68e7,y
$ffe978 20 d5 ec      jsr $ecd5
$ffe97b a9 3c a0      lda #$a03c
$ffe97e e9 20 e3      sbc #$e320
$ffe981 e9 d0 03      sbc #$03d0
$ffe984 4c e2 e9      jmp $e9e2
$ffe987 20 0e ea      jsr $ea0e
$ffe98a a9 00 85      lda #$8500
$ffe98d 62 85 63      per $1004d15
$ffe990 85 64         sta $64
$ffe992 85 65         sta $65
$ffe994 a5 ac         lda $ac
$ffe996 20 b0 e9      jsr $e9b0
$ffe999 a5 a1         lda $a1
$ffe99b 20 b0 e9      jsr $e9b0
$ffe99e a5 a0         lda $a0
$ffe9a0 20 b0 e9      jsr $e9b0
$ffe9a3 a5 9f         lda $9f
$ffe9a5 20 b0 e9      jsr $e9b0
$ffe9a8 a5 9e         lda $9e
$ffe9aa 20 b5 e9      jsr $e9b5
$ffe9ad 4c e6 ea      jmp $eae6
$ffe9b0 d0 03         bne $ffe9b5
$ffe9b2 4c da e8      jmp $e8da
$ffe9b5 4a            lsr
$ffe9b6 09 80 a8      ora #$a880
$ffe9b9 90 19         bcc $ffe9d4
$ffe9bb 18            clc
$ffe9bc a5 65         lda $65
$ffe9be 65 a9         adc $a9
$ffe9c0 85 65         sta $65
$ffe9c2 a5 64         lda $64
$ffe9c4 65 a8         adc $a8
$ffe9c6 85 64         sta $64
$ffe9c8 a5 63         lda $63
$ffe9ca 65 a7         adc $a7
$ffe9cc 85 63         sta $63
$ffe9ce a5 62         lda $62
$ffe9d0 65 a6         adc $a6
$ffe9d2 85 62         sta $62
$ffe9d4 66 62         ror $62
$ffe9d6 66 63         ror $63
$ffe9d8 66 64         ror $64
$ffe9da 66 65         ror $65
$ffe9dc 66 ac         ror $ac
$ffe9de 98            tya
$ffe9df 4a            lsr
$ffe9e0 d0 d6         bne $ffe9b8
$ffe9e2 60            rts
$ffe9e3 85 5e         sta $5e
$ffe9e5 84 5f         sty $5f
$ffe9e7 a0 04 b1      ldy #$b104
$ffe9ea 5e 85 a9      lsr $a985,x
$ffe9ed 88            dey
$ffe9ee b1 5e         lda ($5e),y
$ffe9f0 85 a8         sta $a8
$ffe9f2 88            dey
$ffe9f3 b1 5e         lda ($5e),y
$ffe9f5 85 a7         sta $a7
$ffe9f7 88            dey
$ffe9f8 b1 5e         lda ($5e),y
$ffe9fa 85 aa         sta $aa
$ffe9fc 45 a2         eor $a2
$ffe9fe 85 ab         sta $ab
$ffea00 a5 aa         lda $aa
$ffea02 09 80 85      ora #$8580
$ffea05 a6 88         ldx $88
$ffea07 b1 5e         lda ($5e),y
$ffea09 85 a5         sta $a5
$ffea0b a5 9d         lda $9d
$ffea0d 60            rts
$ffea0e a5 a5         lda $a5
$ffea10 f0 1f         beq $ffea31
$ffea12 18            clc
$ffea13 65 9d         adc $9d
$ffea15 90 04         bcc $ffea1b
$ffea17 30 1d         bmi $ffea36
$ffea19 18            clc
$ffea1a 2c 10 14      bit $1410
$ffea1d 69 80 85      adc #$8580
$ffea20 9d d0 03      sta $03d0,x
$ffea23 4c 52 e8      jmp $e852
$ffea26 a5 ab         lda $ab
$ffea28 85 a2         sta $a2
$ffea2a 60            rts
$ffea2b a5 a2         lda $a2
$ffea2d 49 ff 30      eor #$30ff
$ffea30 05 68         ora $68
$ffea32 68            pla
$ffea33 4c 4e e8      jmp $e84e
$ffea36 4c d5 e8      jmp $e8d5
$ffea39 20 63 eb      jsr $eb63
$ffea3c aa            tax
$ffea3d f0 10         beq $ffea4f
$ffea3f 18            clc
$ffea40 69 02 b0      adc #$b002
$ffea43 f2 a2         sbc ($a2)
$ffea45 00 86         brk $86
$ffea47 ab            plb
$ffea48 20 ce e7      jsr $e7ce
$ffea4b e6 9d         inc $9d
$ffea4d f0 e7         beq $ffea36
$ffea4f 60            rts
$ffea50 84 20         sty $20
$ffea52 00 00         brk $00
$ffea54 00 20         brk $20
$ffea56 63 eb         adc $eb,sp
$ffea58 a9 50 a0      lda #$a050
$ffea5b ea            nop
$ffea5c a2 00 86      ldx #$8600
$ffea5f ab            plb
$ffea60 20 f9 ea      jsr $eaf9
$ffea63 4c 69 ea      jmp $ea69
$ffea66 20 e3 e9      jsr $e9e3
$ffea69 f0 76         beq $ffeae1
$ffea6b 20 72 eb      jsr $eb72
$ffea6e a9 00 38      lda #$3800
$ffea71 e5 9d         sbc $9d
$ffea73 85 9d         sta $9d
$ffea75 20 0e ea      jsr $ea0e
$ffea78 e6 9d         inc $9d
$ffea7a f0 ba         beq $ffea36
$ffea7c a2 fc a9      ldx #$a9fc
$ffea7f 01 a4         ora ($a4,x)
$ffea81 a6 c4         ldx $c4
$ffea83 9e d0 10      stz $10d0,x
$ffea86 a4 a7         ldy $a7
$ffea88 c4 9f         cpy $9f
$ffea8a d0 0a         bne $ffea96
$ffea8c a4 a8         ldy $a8
$ffea8e c4 a0         cpy $a0
$ffea90 d0 04         bne $ffea96
$ffea92 a4 a9         ldy $a9
$ffea94 c4 a1         cpy $a1
$ffea96 08            php
$ffea97 2a            rol
$ffea98 90 09         bcc $ffeaa3
$ffea9a e8            inx
$ffea9b 95 65         sta $65,x
$ffea9d f0 32         beq $ffead1
$ffea9f 10 34         bpl $ffead5
$ffeaa1 a9 01 28      lda #$2801
$ffeaa4 b0 0e         bcs $ffeab4
$ffeaa6 06 a9         asl $a9
$ffeaa8 26 a8         rol $a8
$ffeaaa 26 a7         rol $a7
$ffeaac 26 a6         rol $a6
$ffeaae b0 e6         bcs $ffea96
$ffeab0 30 ce         bmi $ffea80
$ffeab2 10 e2         bpl $ffea96
$ffeab4 a8            tay
$ffeab5 a5 a9         lda $a9
$ffeab7 e5 a1         sbc $a1
$ffeab9 85 a9         sta $a9
$ffeabb a5 a8         lda $a8
$ffeabd e5 a0         sbc $a0
$ffeabf 85 a8         sta $a8
$ffeac1 a5 a7         lda $a7
$ffeac3 e5 9f         sbc $9f
$ffeac5 85 a7         sta $a7
$ffeac7 a5 a6         lda $a6
$ffeac9 e5 9e         sbc $9e
$ffeacb 85 a6         sta $a6
$ffeacd 98            tya
$ffeace 4c a6 ea      jmp $eaa6
$ffead1 a9 40 d0      lda #$d040
$ffead4 ce 0a 0a      dec $0a0a
$ffead7 0a            asl
$ffead8 0a            asl
$ffead9 0a            asl
$ffeada 0a            asl
$ffeadb 85 ac         sta $ac
$ffeadd 28            plp
$ffeade 4c e6 ea      jmp $eae6
$ffeae1 a2 85 4c      ldx #$4c85
$ffeae4 12 d4         ora ($d4)
$ffeae6 a5 62         lda $62
$ffeae8 85 9e         sta $9e
$ffeaea a5 63         lda $63
$ffeaec 85 9f         sta $9f
$ffeaee a5 64         lda $64
$ffeaf0 85 a0         sta $a0
$ffeaf2 a5 65         lda $65
$ffeaf4 85 a1         sta $a1
$ffeaf6 4c 2e e8      jmp $e82e
$ffeaf9 85 5e         sta $5e
$ffeafb 84 5f         sty $5f
$ffeafd a0 04 b1      ldy #$b104
$ffeb00 5e 85 a1      lsr $a185,x
$ffeb03 88            dey
$ffeb04 b1 5e         lda ($5e),y
$ffeb06 85 a0         sta $a0
$ffeb08 88            dey
$ffeb09 b1 5e         lda ($5e),y
$ffeb0b 85 9f         sta $9f
$ffeb0d 88            dey
$ffeb0e b1 5e         lda ($5e),y
$ffeb10 85 a2         sta $a2
$ffeb12 09 80 85      ora #$8580
$ffeb15 9e 88 b1      stz $b188,x
$ffeb18 5e 85 9d      lsr $9d85,x
$ffeb1b 84 ac         sty $ac
$ffeb1d 60            rts
$ffeb1e a2 98 2c      ldx #$2c98
$ffeb21 a2 93 a0      ldx #$a093
$ffeb24 00 f0         brk $f0
$ffeb26 04 a6         tsb $a6
$ffeb28 85 a4         sta $a4
$ffeb2a 86 20         stx $20
$ffeb2c 72 eb         adc ($eb)
$ffeb2e 86 5e         stx $5e
$ffeb30 84 5f         sty $5f
$ffeb32 a0 04 a5      ldy #$a504
$ffeb35 a1 91         lda ($91,x)
$ffeb37 5e 88 a5      lsr $a588,x
$ffeb3a a0 91 5e      ldy #$5e91
$ffeb3d 88            dey
$ffeb3e a5 9f         lda $9f
$ffeb40 91 5e         sta ($5e),y
$ffeb42 88            dey
$ffeb43 a5 a2         lda $a2
$ffeb45 09 7f 25      ora #$257f
$ffeb48 9e 91 5e      stz $5e91,x
$ffeb4b 88            dey
$ffeb4c a5 9d         lda $9d
$ffeb4e 91 5e         sta ($5e),y
$ffeb50 84 ac         sty $ac
$ffeb52 60            rts
$ffeb53 a5 aa         lda $aa
$ffeb55 85 a2         sta $a2
$ffeb57 a2 05 b5      ldx #$b505
$ffeb5a a4 95         ldy $95
$ffeb5c 9c ca d0      stz $d0ca
$ffeb5f f9 86 ac      sbc $ac86,y
$ffeb62 60            rts
$ffeb63 20 72 eb      jsr $eb72
$ffeb66 a2 06 b5      ldx #$b506
$ffeb69 9c 95 a4      stz $a495
$ffeb6c ca            dex
$ffeb6d d0 f9         bne $ffeb68
$ffeb6f 86 ac         stx $ac
$ffeb71 60            rts
$ffeb72 a5 9d         lda $9d
$ffeb74 f0 fb         beq $ffeb71
$ffeb76 06 ac         asl $ac
$ffeb78 90 f7         bcc $ffeb71
$ffeb7a 20 c6 e8      jsr $e8c6
$ffeb7d d0 f2         bne $ffeb71
$ffeb7f 4c 8f e8      jmp $e88f
$ffeb82 a5 9d         lda $9d
$ffeb84 f0 09         beq $ffeb8f
$ffeb86 a5 a2         lda $a2
$ffeb88 2a            rol
$ffeb89 a9 ff b0      lda #$b0ff
$ffeb8c 02 a9         cop $a9
$ffeb8e 01 60         ora ($60,x)
$ffeb90 20 82 eb      jsr $eb82
$ffeb93 85 9e         sta $9e
$ffeb95 a9 00 85      lda #$8500
$ffeb98 9f a2 88 a5   sta $a588a2,x
$ffeb9c 9e 49 ff      stz $ff49,x
$ffeb9f 2a            rol
$ffeba0 a9 00 85      lda #$8500
$ffeba3 a1 85         lda ($85,x)
$ffeba5 a0 86 9d      ldy #$9d86
$ffeba8 85 ac         sta $ac
$ffebaa 85 a2         sta $a2
$ffebac 4c 29 e8      jmp $e829
$ffebaf 46 a2         lsr $a2
$ffebb1 60            rts
$ffebb2 85 60         sta $60
$ffebb4 84 61         sty $61
$ffebb6 a0 00 b1      ldy #$b100
$ffebb9 60            rts
$ffebba c8            iny
$ffebbb aa            tax
$ffebbc f0 c4         beq $ffeb82
$ffebbe b1 60         lda ($60),y
$ffebc0 45 a2         eor $a2
$ffebc2 30 c2         bmi $ffeb86
$ffebc4 e4 9d         cpx $9d
$ffebc6 d0 21         bne $ffebe9
$ffebc8 b1 60         lda ($60),y
$ffebca 09 80 c5      ora #$c580
$ffebcd 9e d0 19      stz $19d0,x
$ffebd0 c8            iny
$ffebd1 b1 60         lda ($60),y
$ffebd3 c5 9f         cmp $9f
$ffebd5 d0 12         bne $ffebe9
$ffebd7 c8            iny
$ffebd8 b1 60         lda ($60),y
$ffebda c5 a0         cmp $a0
$ffebdc d0 0b         bne $ffebe9
$ffebde c8            iny
$ffebdf a9 7f c5      lda #$c57f
$ffebe2 ac b1 60      ldy $60b1
$ffebe5 e5 a1         sbc $a1
$ffebe7 f0 28         beq $ffec11
$ffebe9 a5 a2         lda $a2
$ffebeb 90 02         bcc $ffebef
$ffebed 49 ff 4c      eor #$4cff
$ffebf0 88            dey
$ffebf1 eb            xba
$ffebf2 a5 9d         lda $9d
$ffebf4 f0 4a         beq $ffec40
$ffebf6 38            sec
$ffebf7 e9 a0 24      sbc #$24a0
$ffebfa a2 10 09      ldx #$0910
$ffebfd aa            tax
$ffebfe a9 ff 85      lda #$85ff
$ffec01 a4 20         ldy $20
$ffec03 a4 e8         ldy $e8
$ffec05 8a            txa
$ffec06 a2 9d c9      ldx #$c99d
$ffec09 f9 10 06      sbc $0610,y
$ffec0c 20 f0 e8      jsr $e8f0
$ffec0f 84 a4         sty $a4
$ffec11 60            rts
$ffec12 a8            tay
$ffec13 a5 a2         lda $a2
$ffec15 29 80 46      and #$4680
$ffec18 9e 05 9e      stz $9e05,x
$ffec1b 85 9e         sta $9e
$ffec1d 20 07 e9      jsr $e907
$ffec20 84 a4         sty $a4
$ffec22 60            rts
$ffec23 a5 9d         lda $9d
$ffec25 c9 a0 b0      cmp #$b0a0
$ffec28 20 20 f2      jsr $f220
$ffec2b eb            xba
$ffec2c 84 ac         sty $ac
$ffec2e a5 a2         lda $a2
$ffec30 84 a2         sty $a2
$ffec32 49 80 2a      eor #$2a80
$ffec35 a9 a0 85      lda #$85a0
$ffec38 9d a5 a1      sta $a1a5,x
$ffec3b 85 0d         sta $0d
$ffec3d 4c 29 e8      jmp $e829
$ffec40 85 9e         sta $9e
$ffec42 85 9f         sta $9f
$ffec44 85 a0         sta $a0
$ffec46 85 a1         sta $a1
$ffec48 a8            tay
$ffec49 60            rts
$ffec4a a0 00 a2      ldy #$a200
$ffec4d 0a            asl
$ffec4e 94 99         sty $99,x
$ffec50 ca            dex
$ffec51 10 fb         bpl $ffec4e
$ffec53 90 0f         bcc $ffec64
$ffec55 c9 2d d0      cmp #$d02d
$ffec58 04 86         tsb $86
$ffec5a a3 f0         lda $f0,sp
$ffec5c 04 c9         tsb $c9
$ffec5e 2b            pld
$ffec5f d0 05         bne $ffec66
$ffec61 20 b1 00      jsr $00b1
$ffec64 90 5b         bcc $ffecc1
$ffec66 c9 2e f0      cmp #$f02e
$ffec69 2e c9 45      rol $45c9
$ffec6c d0 30         bne $ffec9e
$ffec6e 20 b1 00      jsr $00b1
$ffec71 90 17         bcc $ffec8a
$ffec73 c9 c9 f0      cmp #$f0c9
$ffec76 0e c9 2d      asl $2dc9
$ffec79 f0 0a         beq $ffec85
$ffec7b c9 c8 f0      cmp #$f0c8
$ffec7e 08            php
$ffec7f c9 2b f0      cmp #$f02b
$ffec82 04 d0         tsb $d0
$ffec84 07 66         ora [$66]
$ffec86 9c 20 b1      stz $b120
$ffec89 00 90         brk $90
$ffec8b 5c 24 9c 10   jmp $109c24
$ffec8f 0e a9 00      asl $00a9
$ffec92 38            sec
$ffec93 e5 9a         sbc $9a
$ffec95 4c a0 ec      jmp $eca0
$ffec98 66 9b         ror $9b
$ffec9a 24 9b         bit $9b
$ffec9c 50 c3         bvc $ffec61
$ffec9e a5 9a         lda $9a
$ffeca0 38            sec
$ffeca1 e5 99         sbc $99
$ffeca3 85 9a         sta $9a
$ffeca5 f0 12         beq $ffecb9
$ffeca7 10 09         bpl $ffecb2
$ffeca9 20 55 ea      jsr $ea55
$ffecac e6 9a         inc $9a
$ffecae d0 f9         bne $ffeca9
$ffecb0 f0 07         beq $ffecb9
$ffecb2 20 39 ea      jsr $ea39
$ffecb5 c6 9a         dec $9a
$ffecb7 d0 f9         bne $ffecb2
$ffecb9 a5 a3         lda $a3
$ffecbb 30 01         bmi $ffecbe
$ffecbd 60            rts
$ffecbe 4c d0 ee      jmp $eed0
$ffecc1 48            pha
$ffecc2 24 9b         bit $9b
$ffecc4 10 02         bpl $ffecc8
$ffecc6 e6 99         inc $99
$ffecc8 20 39 ea      jsr $ea39
$ffeccb 68            pla
$ffeccc 38            sec
$ffeccd e9 30 20      sbc #$2030
$ffecd0 d5 ec         cmp $ec,x
$ffecd2 4c 61 ec      jmp $ec61
$ffecd5 48            pha
$ffecd6 20 63 eb      jsr $eb63
$ffecd9 68            pla
$ffecda 20 93 eb      jsr $eb93
$ffecdd a5 aa         lda $aa
$ffecdf 45 a2         eor $a2
$ffece1 85 ab         sta $ab
$ffece3 a6 9d         ldx $9d
$ffece5 4c c1 e7      jmp $e7c1
$ffece8 a5 9a         lda $9a
$ffecea c9 0a 90      cmp #$900a
$ffeced 09 a9 64      ora #$64a9
$ffecf0 24 9c         bit $9c
$ffecf2 30 11         bmi $ffed05
$ffecf4 4c d5 e8      jmp $e8d5
$ffecf7 0a            asl
$ffecf8 0a            asl
$ffecf9 18            clc
$ffecfa 65 9a         adc $9a
$ffecfc 0a            asl
$ffecfd 18            clc
$ffecfe a0 00 71      ldy #$7100
$ffed01 b8            clv
$ffed02 38            sec
$ffed03 e9 30 85      sbc #$8530
$ffed06 9a            txs
$ffed07 4c 87 ec      jmp $ec87
$ffed0a 9b            txy
$ffed0b 3e bc 1f      rol $1fbc,x
$ffed0e fd 9e 6e      sbc $6e9e,x
$ffed11 6b            rtl
$ffed12 27 fd         and [$fd]
$ffed14 9e 6e 6b      stz $6b6e,x
$ffed17 28            plp
$ffed18 00 a9         brk $a9
$ffed1a 58            cli
$ffed1b a0 d3 20      ldy #$20d3
$ffed1e 31 ed         and ($ed),y
$ffed20 a5 76         lda $76
$ffed22 a6 75         ldx $75
$ffed24 85 9e         sta $9e
$ffed26 86 9f         stx $9f
$ffed28 a2 90 38      ldx #$3890
$ffed2b 20 a0 eb      jsr $eba0
$ffed2e 20 34 ed      jsr $ed34
$ffed31 4c 3a db      jmp $db3a
$ffed34 a0 01 a9      ldy #$a901
$ffed37 2d 88 24      and $2488
$ffed3a a2 10 04      ldx #$0410
$ffed3d c8            iny
$ffed3e 99 ff 00      sta $00ff,y
$ffed41 85 a2         sta $a2
$ffed43 84 ad         sty $ad
$ffed45 c8            iny
$ffed46 a9 30 a6      lda #$a630
$ffed49 9d d0 03      sta $03d0,x
$ffed4c 4c 57 ee      jmp $ee57
$ffed4f a9 00 e0      lda #$e000
$ffed52 80 f0         bra $ffed44
$ffed54 02 b0         cop $b0
$ffed56 09 a9 14      ora #$14a9
$ffed59 a0 ed 20      ldy #$20ed
$ffed5c 7f e9 a9 f7   adc $f7a9e9,x
$ffed60 85 99         sta $99
$ffed62 a9 0f a0      lda #$a00f
$ffed65 ed 20 b2      sbc $b220
$ffed68 eb            xba
$ffed69 f0 1e         beq $ffed89
$ffed6b 10 12         bpl $ffed7f
$ffed6d a9 0a a0      lda #$a00a
$ffed70 ed 20 b2      sbc $b220
$ffed73 eb            xba
$ffed74 f0 02         beq $ffed78
$ffed76 10 0e         bpl $ffed86
$ffed78 20 39 ea      jsr $ea39
$ffed7b c6 99         dec $99
$ffed7d d0 ee         bne $ffed6d
$ffed7f 20 55 ea      jsr $ea55
$ffed82 e6 99         inc $99
$ffed84 d0 dc         bne $ffed62
$ffed86 20 a0 e7      jsr $e7a0
$ffed89 20 f2 eb      jsr $ebf2
$ffed8c a2 01 a5      ldx #$a501
$ffed8f 99 18 69      sta $6918,y
$ffed92 0a            asl
$ffed93 30 09         bmi $ffed9e
$ffed95 c9 0b b0      cmp #$b00b
$ffed98 06 69         asl $69
$ffed9a ff aa a9 02   sbc $02a9aa,x
$ffed9e 38            sec
$ffed9f e9 02 85      sbc #$8502
$ffeda2 9a            txs
$ffeda3 86 99         stx $99
$ffeda5 8a            txa
$ffeda6 f0 02         beq $ffedaa
$ffeda8 10 13         bpl $ffedbd
$ffedaa a4 ad         ldy $ad
$ffedac a9 2e c8      lda #$c82e
$ffedaf 99 ff 00      sta $00ff,y
$ffedb2 8a            txa
$ffedb3 f0 06         beq $ffedbb
$ffedb5 a9 30 c8      lda #$c830
$ffedb8 99 ff 00      sta $00ff,y
$ffedbb 84 ad         sty $ad
$ffedbd a0 00 a2      ldy #$a200
$ffedc0 80 a5         bra $ffed67
$ffedc2 a1 18         lda ($18,x)
$ffedc4 79 6c ee      adc $ee6c,y
$ffedc7 85 a1         sta $a1
$ffedc9 a5 a0         lda $a0
$ffedcb 79 6b ee      adc $ee6b,y
$ffedce 85 a0         sta $a0
$ffedd0 a5 9f         lda $9f
$ffedd2 79 6a ee      adc $ee6a,y
$ffedd5 85 9f         sta $9f
$ffedd7 a5 9e         lda $9e
$ffedd9 79 69 ee      adc $ee69,y
$ffeddc 85 9e         sta $9e
$ffedde e8            inx
$ffeddf b0 04         bcs $ffede5
$ffede1 10 de         bpl $ffedc1
$ffede3 30 02         bmi $ffede7
$ffede5 30 da         bmi $ffedc1
$ffede7 8a            txa
$ffede8 90 04         bcc $ffedee
$ffedea 49 ff 69      eor #$69ff
$ffeded 0a            asl
$ffedee 69 2f c8      adc #$c82f
$ffedf1 c8            iny
$ffedf2 c8            iny
$ffedf3 c8            iny
$ffedf4 84 83         sty $83
$ffedf6 a4 ad         ldy $ad
$ffedf8 c8            iny
$ffedf9 aa            tax
$ffedfa 29 7f 99      and #$997f
$ffedfd ff 00 c6 99   sbc $99c600,x
$ffee01 d0 06         bne $ffee09
$ffee03 a9 2e c8      lda #$c82e
$ffee06 99 ff 00      sta $00ff,y
$ffee09 84 ad         sty $ad
$ffee0b a4 83         ldy $83
$ffee0d 8a            txa
$ffee0e 49 ff 29      eor #$29ff
$ffee11 80 aa         bra $ffedbd
$ffee13 c0 24 d0      cpy #$d024
$ffee16 aa            tax
$ffee17 a4 ad         ldy $ad
$ffee19 b9 ff 00      lda $00ff,y
$ffee1c 88            dey
$ffee1d c9 30 f0      cmp #$f030
$ffee20 f8            sed
$ffee21 c9 2e f0      cmp #$f02e
$ffee24 01 c8         ora ($c8,x)
$ffee26 a9 2b a6      lda #$a62b
$ffee29 9a            txs
$ffee2a f0 2e         beq $ffee5a
$ffee2c 10 08         bpl $ffee36
$ffee2e a9 00 38      lda #$3800
$ffee31 e5 9a         sbc $9a
$ffee33 aa            tax
$ffee34 a9 2d 99      lda #$992d
$ffee37 01 01         ora ($01,x)
$ffee39 a9 45 99      lda #$9945
$ffee3c 00 01         brk $01
$ffee3e 8a            txa
$ffee3f a2 2f 38      ldx #$382f
$ffee42 e8            inx
$ffee43 e9 0a b0      sbc #$b00a
$ffee46 fb            xce
$ffee47 69 3a 99      adc #$993a
$ffee4a 03 01         ora $01,sp
$ffee4c 8a            txa
$ffee4d 99 02 01      sta $0102,y
$ffee50 a9 00 99      lda #$9900
$ffee53 04 01         tsb $01
$ffee55 f0 08         beq $ffee5f
$ffee57 99 ff 00      sta $00ff,y
$ffee5a a9 00 99      lda #$9900
$ffee5d 00 01         brk $01
$ffee5f a9 00 a0      lda #$a000
$ffee62 01 60         ora ($60,x)
$ffee64 80 00         bra $ffee66
$ffee66 00 00         brk $00
$ffee68 00 fa         brk $fa
$ffee6a 0a            asl
$ffee6b 1f 00 00 98   ora $980000,x
$ffee6f 96 80         stx $80,y
$ffee71 ff f0 bd c0   sbc $c0bdf0,x
$ffee75 00 01         brk $01
$ffee77 86 a0         stx $a0
$ffee79 ff ff d8 f0   sbc $f0d8ff,x
$ffee7d 00 00         brk $00
$ffee7f 03 e8         ora $e8,sp
$ffee81 ff ff ff 9c   sbc $9cffff,x
$ffee85 00 00         brk $00
$ffee87 00 0a         brk $0a
$ffee89 ff ff ff ff   sbc $ffffff,x
$ffee8d 20 63 eb      jsr $eb63
$ffee90 a9 64 a0      lda #$a064
$ffee93 ee 20 f9      inc $f920
$ffee96 ea            nop
$ffee97 f0 70         beq $ffef09
$ffee99 a5 a5         lda $a5
$ffee9b d0 03         bne $ffeea0
$ffee9d 4c 50 e8      jmp $e850
$ffeea0 a2 8a a0      ldx #$a08a
$ffeea3 00 20         brk $20
$ffeea5 2b            pld
$ffeea6 eb            xba
$ffeea7 a5 aa         lda $aa
$ffeea9 10 0f         bpl $ffeeba
$ffeeab 20 23 ec      jsr $ec23
$ffeeae a9 8a a0      lda #$a08a
$ffeeb1 00 20         brk $20
$ffeeb3 b2 eb         lda ($eb)
$ffeeb5 d0 03         bne $ffeeba
$ffeeb7 98            tya
$ffeeb8 a4 0d         ldy $0d
$ffeeba 20 55 eb      jsr $eb55
$ffeebd 98            tya
$ffeebe 48            pha
$ffeebf 20 41 e9      jsr $e941
$ffeec2 a9 8a a0      lda #$a08a
$ffeec5 00 20         brk $20
$ffeec7 7f e9 20 09   adc $0920e9,x
$ffeecb ef 68 4a 90   sbc $904a68
$ffeecf 0a            asl
$ffeed0 a5 9d         lda $9d
$ffeed2 f0 06         beq $ffeeda
$ffeed4 a5 a2         lda $a2
$ffeed6 49 ff 85      eor #$85ff
$ffeed9 a2 60 81      ldx #$8160
$ffeedc 38            sec
$ffeedd aa            tax
$ffeede 3b            tsc
$ffeedf 29 07 71      and #$7107
$ffeee2 34 58         bit $58,x
$ffeee4 3e 56 74      rol $7456,x
$ffeee7 16 7e         asl $7e,x
$ffeee9 b3 1b         lda ($1b,sp),y
$ffeeeb 77 2f         adc [$2f],y
$ffeeed ee e3 85      inc $85e3
$ffeef0 7a            ply
$ffeef1 1d 84 1c      ora $1c84,x
$ffeef4 2a            rol
$ffeef5 7c 63 59      jmp ($5963,x)
$ffeef8 58            cli
$ffeef9 0a            asl
$ffeefa 7e 75 fd      ror $fd75,x
$ffeefd e7 c6         sbc [$c6]
$ffeeff 80 31         bra $ffef32
$ffef01 72 18         adc ($18)
$ffef03 10 81         bpl $ffee86
$ffef05 00 00         brk $00
$ffef07 00 00         brk $00
$ffef09 a9 db a0      lda #$a0db
$ffef0c ee 20 7f      inc $7f20
$ffef0f e9 a5 ac      sbc #$aca5
$ffef12 69 50 90      adc #$9050
$ffef15 03 20         ora $20,sp
$ffef17 7a            ply
$ffef18 eb            xba
$ffef19 85 92         sta $92
$ffef1b 20 66 eb      jsr $eb66
$ffef1e a5 9d         lda $9d
$ffef20 c9 88 90      cmp #$9088
$ffef23 03 20         ora $20,sp
$ffef25 2b            pld
$ffef26 ea            nop
$ffef27 20 23 ec      jsr $ec23
$ffef2a a5 0d         lda $0d
$ffef2c 18            clc
$ffef2d 69 81 f0      adc #$f081
$ffef30 f3 38         sbc ($38,sp),y
$ffef32 e9 01 48      sbc #$4801
$ffef35 a2 05 b5      ldx #$b505
$ffef38 a5 b4         lda $b4
$ffef3a 9d 95 9d      sta $9d95,x
$ffef3d 94 a5         sty $a5,x
$ffef3f ca            dex
$ffef40 10 f5         bpl $ffef37
$ffef42 a5 92         lda $92
$ffef44 85 ac         sta $ac
$ffef46 20 aa e7      jsr $e7aa
$ffef49 20 d0 ee      jsr $eed0
$ffef4c a9 e0 a0      lda #$a0e0
$ffef4f ee 20 72      inc $7220
$ffef52 ef a9 00 85   sbc $8500a9
$ffef56 ab            plb
$ffef57 68            pla
$ffef58 20 10 ea      jsr $ea10
$ffef5b 60            rts
$ffef5c 85 ad         sta $ad
$ffef5e 84 ae         sty $ae
$ffef60 20 21 eb      jsr $eb21
$ffef63 a9 93 20      lda #$2093
$ffef66 7f e9 20 76   adc $7620e9,x
$ffef6a ef a9 93 a0   sbc $a093a9
$ffef6e 00 4c         brk $4c
$ffef70 7f e9 85 ad   adc $ad85e9,x
$ffef74 84 ae         sty $ae
$ffef76 20 1e eb      jsr $eb1e
$ffef79 b1 ad         lda ($ad),y
$ffef7b 85 a3         sta $a3
$ffef7d a4 ad         ldy $ad
$ffef7f c8            iny
$ffef80 98            tya
$ffef81 d0 02         bne $ffef85
$ffef83 e6 ae         inc $ae
$ffef85 85 ad         sta $ad
$ffef87 a4 ae         ldy $ae
$ffef89 20 7f e9      jsr $e97f
$ffef8c a5 ad         lda $ad
$ffef8e a4 ae         ldy $ae
$ffef90 18            clc
$ffef91 69 05 90      adc #$9005
$ffef94 01 c8         ora ($c8,x)
$ffef96 85 ad         sta $ad
$ffef98 84 ae         sty $ae
$ffef9a 20 be e7      jsr $e7be
$ffef9d a9 98 a0      lda #$a098
$ffefa0 00 c6         brk $c6
$ffefa2 a3 d0         lda $d0,sp
$ffefa4 e4 60         cpx $60
$ffefa6 98            tya
$ffefa7 35 44         and $44,x
$ffefa9 7a            ply
$ffefaa 68            pla
$ffefab 28            plp
$ffefac b1 46         lda ($46),y
$ffefae 20 82 eb      jsr $eb82
$ffefb1 aa            tax
$ffefb2 30 18         bmi $ffefcc
$ffefb4 a9 c9 a0      lda #$a0c9
$ffefb7 00 20         brk $20
$ffefb9 f9 ea 8a      sbc $8aea,y
$ffefbc f0 e7         beq $ffefa5
$ffefbe a9 a6 a0      lda #$a0a6
$ffefc1 ef 20 7f e9   sbc $e97f20
$ffefc5 a9 aa a0      lda #$a0aa
$ffefc8 ef 20 be e7   sbc $e7be20
$ffefcc a6 a1         ldx $a1
$ffefce a5 9e         lda $9e
$ffefd0 85 a1         sta $a1
$ffefd2 86 9e         stx $9e
$ffefd4 a9 00 85      lda #$8500
$ffefd7 a2 a5 9d      ldx #$9da5
$ffefda 85 ac         sta $ac
$ffefdc a9 80 85      lda #$8580
$ffefdf 9d 20 2e      sta $2e20,x
$ffefe2 e8            inx
$ffefe3 a2 c9 a0      ldx #$a0c9
$ffefe6 00 4c         brk $4c
$ffefe8 2b            pld
$ffefe9 eb            xba
$ffefea a9 66 a0      lda #$a066
$ffefed f0 20         beq $fff00f
$ffefef be e7 20      ldx $20e7,y
$ffeff2 63 eb         adc $eb,sp
$ffeff4 a9 6b a0      lda #$a06b
$ffeff7 f0 a6         beq $ffef9f
$ffeff9 aa            tax
$ffeffa 20 5e ea      jsr $ea5e
$ffeffd 20 63 eb      jsr $eb63
$fff000 20 23 ec      jsr $ec23
$fff003 a9 00 85      lda #$8500
$fff006 ab            plb
$fff007 20 aa e7      jsr $e7aa
$fff00a a9 70 a0      lda #$a070
$fff00d f0 20         beq $fff02f
$fff00f a7 e7         lda [$e7]
$fff011 a5 a2         lda $a2
$fff013 48            pha
$fff014 10 0d         bpl $fff023
$fff016 20 a0 e7      jsr $e7a0
$fff019 a5 a2         lda $a2
$fff01b 30 09         bmi $fff026
$fff01d a5 16         lda $16
$fff01f 49 ff 85      eor #$85ff
$fff022 16 20         asl $20,x
$fff024 d0 ee         bne $fff014
$fff026 a9 70 a0      lda #$a070
$fff029 f0 20         beq $fff04b
$fff02b be e7 68      ldx $68e7,y
$fff02e 10 03         bpl $fff033
$fff030 20 d0 ee      jsr $eed0
$fff033 a9 75 a0      lda #$a075
$fff036 f0 4c         beq $fff084
$fff038 5c ef 20 21   jmp $2120ef
$fff03c eb            xba
$fff03d a9 00 85      lda #$8500
$fff040 16 20         asl $20,x
$fff042 f1 ef         sbc ($ef),y
$fff044 a2 8a a0      ldx #$a08a
$fff047 00 20         brk $20
$fff049 e7 ef         sbc [$ef]
$fff04b a9 93 a0      lda #$a093
$fff04e 00 20         brk $20
$fff050 f9 ea a9      sbc $a9ea,y
$fff053 00 85         brk $85
$fff055 a2 a5 16      ldx #$16a5
$fff058 20 62 f0      jsr $f062
$fff05b a9 8a a0      lda #$a08a
$fff05e 00 4c         brk $4c
$fff060 66 ea         ror $ea
$fff062 48            pha
$fff063 4c 23 f0      jmp $f023
$fff066 81 49         sta ($49,x)
$fff068 0f da a2 83   ora $83a2da
$fff06c 49 0f da      eor #$da0f
$fff06f a2 7f 00      ldx #$007f
$fff072 00 00         brk $00
$fff074 00 05         brk $05
$fff076 84 e6         sty $e6
$fff078 1a            inc
$fff079 2d 1b 86      and $861b
$fff07c 28            plp
$fff07d 07 fb         ora [$fb]
$fff07f f8            sed
$fff080 87 99         sta [$99]
$fff082 68            pla
$fff083 89 01 87      bit #$8701
$fff086 23 35         and $35,sp
$fff088 df e1 86 a5   cmp $a586e1,x
$fff08c 5d e7 28      eor $28e7,x
$fff08f 83 49         sta $49,sp
$fff091 0f da a2 a6   ora $a6a2da
$fff095 d3 c1         cmp ($c1,sp),y
$fff097 c8            iny
$fff098 d4 c8         pei ($c8)
$fff09a d5 c4         cmp $c4,x
$fff09c ce ca a5      dec $a5ca
$fff09f a2 48 10      ldx #$1048
$fff0a2 03 20         ora $20,sp
$fff0a4 d0 ee         bne $fff094
$fff0a6 a5 9d         lda $9d
$fff0a8 48            pha
$fff0a9 c9 81 90      cmp #$9081
$fff0ac 07 a9         ora [$a9]
$fff0ae 13 a0         ora ($a0,sp),y
$fff0b0 e9 20 66      sbc #$6620
$fff0b3 ea            nop
$fff0b4 a9 ce a0      lda #$a0ce
$fff0b7 f0 20         beq $fff0d9
$fff0b9 5c ef 68 c9   jmp $c968ef
$fff0bd 81 90         sta ($90,x)
$fff0bf 07 a9         ora [$a9]
$fff0c1 66 a0         ror $a0
$fff0c3 f0 20         beq $fff0e5
$fff0c5 a7 e7         lda [$e7]
$fff0c7 68            pla
$fff0c8 10 03         bpl $fff0cd
$fff0ca 4c d0 ee      jmp $eed0
$fff0cd 60            rts
$fff0ce 0b            phd
$fff0cf 76 b3         ror $b3,x
$fff0d1 83 bd         sta $bd,sp
$fff0d3 d3 79         cmp ($79,sp),y
$fff0d5 1e f4 a6      asl $a6f4,x
$fff0d8 f5 7b         sbc $7b,x
$fff0da 83 fc         sta $fc,sp
$fff0dc b0 10         bcs $fff0ee
$fff0de 7c 0c 1f      jmp ($1f0c,x)
$fff0e1 67 ca         adc [$ca]
$fff0e3 7c de 53      jmp ($53de,x)
$fff0e6 cb            wai
$fff0e7 c1 7d         cmp ($7d,x)
$fff0e9 14 64         trb $64
$fff0eb 70 4c         bvs $fff139
$fff0ed 7d b7 ea      adc $eab7,x
$fff0f0 51 7a         eor ($7a),y
$fff0f2 7d 63 30      adc $3063,x
$fff0f5 88            dey
$fff0f6 7e 7e 92      ror $927e,x
$fff0f9 44 99 3a      mvp $99,$3a
$fff0fc 7e 4c cc      ror $cc4c,x
$fff0ff 91 c7         sta ($c7),y
$fff101 7f aa aa aa   adc $aaaaaa,x
$fff105 13 81         ora ($81,sp),y
$fff107 00 00         brk $00
$fff109 00 00         brk $00
$fff10b e6 b8         inc $b8
$fff10d d0 02         bne $fff111
$fff10f e6 b9         inc $b9
$fff111 ad 60 ea      lda $ea60
$fff114 c9 3a b0      cmp #$b03a
$fff117 0a            asl
$fff118 c9 20 f0      cmp #$f020
$fff11b ef 38 e9 30   sbc $30e938
$fff11f 38            sec
$fff120 e9 d0 60      sbc #$60d0
$fff123 80 4f         bra $fff174
$fff125 c7 52         cmp [$52]
$fff127 58            cli
$fff128 a2 ff 86      ldx #$86ff
$fff12b 76 a2         ror $a2,x
$fff12d fb            xce
$fff12e 9a            txs
$fff12f a9 28 a0      lda #$a028
$fff132 f1 85         sbc ($85),y
$fff134 01 84         ora ($84,x)
$fff136 02 85         cop $85
$fff138 04 84         tsb $84
$fff13a 05 20         ora $20
$fff13c 73 f2         adc ($f2,sp),y
$fff13e a9 4c 85      lda #$854c
$fff141 00 85         brk $85
$fff143 03 85         ora $85,sp
$fff145 90 85         bcc $fff0cc
$fff147 0a            asl
$fff148 a9 99 a0      lda #$a099
$fff14b e1 85         sbc ($85,x)
$fff14d 0b            phd
$fff14e 84 0c         sty $0c
$fff150 a2 1c bd      ldx #$bd1c
$fff153 0a            asl
$fff154 f1 95         sbc ($95),y
$fff156 b0 86         bcs $fff0de
$fff158 f1 ca         sbc ($ca),y
$fff15a d0 f6         bne $fff152
$fff15c 86 f2         stx $f2
$fff15e 8a            txa
$fff15f 85 a4         sta $a4
$fff161 85 54         sta $54
$fff163 48            pha
$fff164 a9 03 85      lda #$8503
$fff167 8f 20 fb da   sta $dafb20
$fff16b a9 01 8d      lda #$8d01
$fff16e fd 01 8d      sbc $8d01,x
$fff171 fc 01 a2      jsr ($a201,x)
$fff174 55 86         eor $86,x
$fff176 52 a9         eor ($a9)
$fff178 00 a0         brk $a0
$fff17a 08            php
$fff17b 85 50         sta $50
$fff17d 84 51         sty $51
$fff17f a0 00 e6      ldy #$e600
$fff182 51 b1         eor ($b1),y
$fff184 50 49         bvc $fff1cf
$fff186 ff 91 50 d1   sbc $d15091,x
$fff18a 50 d0         bvc $fff15c
$fff18c 08            php
$fff18d 49 ff 91      eor #$91ff
$fff190 50 d1         bvc $fff163
$fff192 50 f0         bvc $fff184
$fff194 ec a4 50      cpx $50a4
$fff197 a5 51         lda $51
$fff199 29 f0 84      and #$84f0
$fff19c 73 85         adc ($85,sp),y
$fff19e 74 84         stz $84,x
$fff1a0 6f 85 70 a2   adc $a27085
$fff1a4 00 a0         brk $a0
$fff1a6 08            php
$fff1a7 86 67         stx $67
$fff1a9 84 68         sty $68
$fff1ab a0 00 84      ldy #$8400
$fff1ae d6 98         dec $98,x
$fff1b0 91 67         sta ($67),y
$fff1b2 e6 67         inc $67
$fff1b4 d0 02         bne $fff1b8
$fff1b6 e6 68         inc $68
$fff1b8 a5 67         lda $67
$fff1ba a4 68         ldy $68
$fff1bc 20 e3 d3      jsr $d3e3
$fff1bf 20 4b d6      jsr $d64b
$fff1c2 a9 3a a0      lda #$a03a
$fff1c5 db            stp
$fff1c6 85 04         sta $04
$fff1c8 84 05         sty $05
$fff1ca a9 3c a0      lda #$a03c
$fff1cd d4 85         pei ($85)
$fff1cf 01 84         ora ($84,x)
$fff1d1 02 6c         cop $6c
$fff1d3 01 00         ora ($00,x)
$fff1d5 20 67 dd      jsr $dd67
$fff1d8 20 52 e7      jsr $e752
$fff1db 6c 50 00      jmp ($0050)
$fff1de 20 f8 e6      jsr $e6f8
$fff1e1 8a            txa
$fff1e2 4c 8b fe      jmp $fe8b
$fff1e5 20 f8 e6      jsr $e6f8
$fff1e8 8a            txa
$fff1e9 4c 95 fe      jmp $fe95
$fff1ec 20 f8 e6      jsr $e6f8
$fff1ef e0 50 b0      cpx #$b050
$fff1f2 13 86         ora ($86,sp),y
$fff1f4 f0 a9         beq $fff19f
$fff1f6 2c 20 c0      bit $c020
$fff1f9 de 20 f8      dec $f820,x
$fff1fc e6 e0         inc $e0
$fff1fe 30 b0         bmi $fff1b0
$fff200 05 86         ora $86
$fff202 2c 86 2d      bit $2d86
$fff205 60            rts
$fff206 4c 99 e1      jmp $e199
$fff209 20 ec f1      jsr $f1ec
$fff20c e4 f0         cpx $f0
$fff20e b0 08         bcs $fff218
$fff210 a5 f0         lda $f0
$fff212 85 2c         sta $2c
$fff214 85 2d         sta $2d
$fff216 86 f0         stx $f0
$fff218 a9 c5 20      lda #$20c5
$fff21b c0 de 20      cpy #$20de
$fff21e f8            sed
$fff21f e6 e0         inc $e0
$fff221 50 b0         bvc $fff1d3
$fff223 e2 60         sep #$60
$fff225 20 ec f1      jsr $f1ec
$fff228 a4 f0         ldy $f0
$fff22a 20 75 f7      jsr $f775
$fff22d 8a            txa
$fff22e 4c 9f f3      jmp $f39f
$fff231 00 20         brk $20
$fff233 d9 f8 a4      cmp $a4f8,y
$fff236 2c 20 75      bit $7520
$fff239 f7 e0         sbc [$e0],y
$fff23b 30 b0         bmi $fff1ed
$fff23d c8            iny
$fff23e 4c 96 f7      jmp $f796
$fff241 20 09 f2      jsr $f209
$fff244 8a            txa
$fff245 a8            tay
$fff246 20 75 f7      jsr $f775
$fff249 a5 f0         lda $f0
$fff24b 4c 83 f7      jmp $f783
$fff24e 00 20         brk $20
$fff250 f8            sed
$fff251 e6 8a         inc $8a
$fff253 4c 64 f8      jmp $f864
$fff256 20 f8 e6      jsr $e6f8
$fff259 ca            dex
$fff25a 8a            txa
$fff25b c9 18 b0      cmp #$b018
$fff25e a7 4c         lda [$4c]
$fff260 5b            tcd
$fff261 fb            xce
$fff262 20 f8 e6      jsr $e6f8
$fff265 8a            txa
$fff266 49 ff aa      eor #$aaff
$fff269 e8            inx
$fff26a 86 f1         stx $f1
$fff26c 60            rts
$fff26d 38            sec
$fff26e 90 18         bcc $fff288
$fff270 66 f2         ror $f2
$fff272 60            rts
$fff273 a9 ff d0      lda #$d0ff
$fff276 02 a9         cop $a9
$fff278 3f a2 00 85   and $8500a2,x
$fff27c 32 86         and ($86)
$fff27e f3 60         sbc ($60,sp),y
$fff280 a9 7f a2      lda #$a27f
$fff283 40            rti
$fff284 d0 f5         bne $fff27b
$fff286 20 67 dd      jsr $dd67
$fff289 20 52 e7      jsr $e752
$fff28c a5 50         lda $50
$fff28e c5 6d         cmp $6d
$fff290 a5 51         lda $51
$fff292 e5 6e         sbc $6e
$fff294 b0 03         bcs $fff299
$fff296 4c 10 d4      jmp $d410
$fff299 a5 50         lda $50
$fff29b 85 73         sta $73
$fff29d 85 6f         sta $6f
$fff29f a5 51         lda $51
$fff2a1 85 74         sta $74
$fff2a3 85 70         sta $70
$fff2a5 60            rts
$fff2a6 20 67 dd      jsr $dd67
$fff2a9 20 52 e7      jsr $e752
$fff2ac a5 50         lda $50
$fff2ae c5 73         cmp $73
$fff2b0 a5 51         lda $51
$fff2b2 e5 74         sbc $74
$fff2b4 b0 e0         bcs $fff296
$fff2b6 a5 50         lda $50
$fff2b8 c5 69         cmp $69
$fff2ba a5 51         lda $51
$fff2bc e5 6a         sbc $6a
$fff2be 90 d6         bcc $fff296
$fff2c0 a5 50         lda $50
$fff2c2 85 69         sta $69
$fff2c4 a5 51         lda $51
$fff2c6 85 6a         sta $6a
$fff2c8 4c 6c d6      jmp $d66c
$fff2cb a9 ab 20      lda #$20ab
$fff2ce c0 de a5      cpy #$a5de
$fff2d1 b8            clv
$fff2d2 85 f4         sta $f4
$fff2d4 a5 b9         lda $b9
$fff2d6 85 f5         sta $f5
$fff2d8 38            sec
$fff2d9 66 d8         ror $d8
$fff2db a5 75         lda $75
$fff2dd 85 f6         sta $f6
$fff2df a5 76         lda $76
$fff2e1 85 f7         sta $f7
$fff2e3 20 a6 d9      jsr $d9a6
$fff2e6 4c 98 d9      jmp $d998
$fff2e9 86 de         stx $de
$fff2eb a6 f8         ldx $f8
$fff2ed 86 df         stx $df
$fff2ef a5 75         lda $75
$fff2f1 85 da         sta $da
$fff2f3 a5 76         lda $76
$fff2f5 85 db         sta $db
$fff2f7 a5 79         lda $79
$fff2f9 85 dc         sta $dc
$fff2fb a5 7a         lda $7a
$fff2fd 85 dd         sta $dd
$fff2ff a5 f4         lda $f4
$fff301 85 b8         sta $b8
$fff303 a5 f5         lda $f5
$fff305 85 b9         sta $b9
$fff307 a5 f6         lda $f6
$fff309 85 75         sta $75
$fff30b a5 f7         lda $f7
$fff30d 85 76         sta $76
$fff30f 20 b7 00      jsr $00b7
$fff312 20 3e d9      jsr $d93e
$fff315 4c d2 d7      jmp $d7d2
$fff318 a5 da         lda $da
$fff31a 85 75         sta $75
$fff31c a5 db         lda $db
$fff31e 85 76         sta $76
$fff320 a5 dc         lda $dc
$fff322 85 b8         sta $b8
$fff324 a5 dd         lda $dd
$fff326 85 b9         sta $b9
$fff328 a6 df         ldx $df
$fff32a 9a            txs
$fff32b 4c d2 d7      jmp $d7d2
$fff32e 4c c9 de      jmp $dec9
$fff331 b0 fb         bcs $fff32e
$fff333 a6 af         ldx $af
$fff335 86 69         stx $69
$fff337 a6 b0         ldx $b0
$fff339 86 6a         stx $6a
$fff33b 20 0c da      jsr $da0c
$fff33e 20 1a d6      jsr $d61a
$fff341 a5 9b         lda $9b
$fff343 85 60         sta $60
$fff345 a5 9c         lda $9c
$fff347 85 61         sta $61
$fff349 a9 2c 20      lda #$202c
$fff34c c0 de 20      cpy #$20de
$fff34f 0c da e6      tsb $e6da
$fff352 50 d0         bvc $fff324
$fff354 02 e6         cop $e6
$fff356 51 20         eor ($20),y
$fff358 1a            inc
$fff359 d6 a5         dec $a5,x
$fff35b 9b            txy
$fff35c c5 60         cmp $60
$fff35e a5 9c         lda $9c
$fff360 e5 61         sbc $61
$fff362 b0 01         bcs $fff365
$fff364 60            rts
$fff365 a0 00 b1      ldy #$b100
$fff368 9b            txy
$fff369 91 60         sta ($60),y
$fff36b e6 9b         inc $9b
$fff36d d0 02         bne $fff371
$fff36f e6 9c         inc $9c
$fff371 e6 60         inc $60
$fff373 d0 02         bne $fff377
$fff375 e6 61         inc $61
$fff377 a5 69         lda $69
$fff379 c5 9b         cmp $9b
$fff37b a5 6a         lda $6a
$fff37d e5 9c         sbc $9c
$fff37f b0 e6         bcs $fff367
$fff381 a6 61         ldx $61
$fff383 a4 60         ldy $60
$fff385 d0 01         bne $fff388
$fff387 ca            dex
$fff388 88            dey
$fff389 86 6a         stx $6a
$fff38b 84 69         sty $69
$fff38d 4c f2 d4      jmp $d4f2
$fff390 ad 56 c0      lda $c056
$fff393 ad 53 c0      lda $c053
$fff396 4c ec d8      jmp $d8ec
$fff399 ad 54 c0      lda $c054
$fff39c 4c 39 fb      jmp $fb39
$fff39f 4a            lsr
$fff3a0 08            php
$fff3a1 20 47 f8      jsr $f847
$fff3a4 28            plp
$fff3a5 a9 0f 90      lda #$900f
$fff3a8 02 69         cop $69
$fff3aa e0 85 2e      cpx #$2e85
$fff3ad 5a            phy
$fff3ae 20 bb f7      jsr $f7bb
$fff3b1 90 0a         bcc $fff3bd
$fff3b3 da            phx
$fff3b4 a5 30         lda $30
$fff3b6 aa            tax
$fff3b7 4a            lsr
$fff3b8 8a            txa
$fff3b9 6a            ror
$fff3ba 38            sec
$fff3bb 85 30         sta $30
$fff3bd 20 0e f8      jsr $f80e
$fff3c0 90 07         bcc $fff3c9
$fff3c2 ad 54 c0      lda $c054
$fff3c5 86 30         stx $30
$fff3c7 fa            plx
$fff3c8 18            clc
$fff3c9 7a            ply
$fff3ca 60            rts
$fff3cb 20 3a f9      jsr $f93a
$fff3ce 49 80 2d      eor #$2d80
$fff3d1 18            clc
$fff3d2 c0 2d 1f      cpy #$1f2d
$fff3d5 c0 0a 60      cpy #$600a
$fff3d8 2c 55 c0      bit $c055
$fff3db 2c 52 c0      bit $c052
$fff3de a9 40 d0      lda #$d040
$fff3e1 08            php
$fff3e2 a9 20 2c      lda #$2c20
$fff3e5 54 c0 2c      mvn $c0,$2c
$fff3e8 53 c0         eor ($c0,sp),y
$fff3ea 85 e6         sta $e6
$fff3ec ad 57 c0      lda $c057
$fff3ef ad 50 c0      lda $c050
$fff3f2 a9 00 85      lda #$8500
$fff3f5 1c a5 e6      trb $e6a5
$fff3f8 85 1b         sta $1b
$fff3fa a0 00 84      ldy #$8400
$fff3fd 1a            inc
$fff3fe a5 1c         lda $1c
$fff400 91 1a         sta ($1a),y
$fff402 20 7e f4      jsr $f47e
$fff405 c8            iny
$fff406 d0 f6         bne $fff3fe
$fff408 e6 1b         inc $1b
$fff40a a5 1b         lda $1b
$fff40c 29 1f d0      and #$d01f
$fff40f ee 60 85      inc $8560
$fff412 e2 86         sep #$86
$fff414 e0 84 e1      cpx #$e184
$fff417 48            pha
$fff418 29 c0 85      and #$85c0
$fff41b 26 4a         rol $4a
$fff41d 4a            lsr
$fff41e 05 26         ora $26
$fff420 85 26         sta $26
$fff422 68            pla
$fff423 85 27         sta $27
$fff425 0a            asl
$fff426 0a            asl
$fff427 0a            asl
$fff428 26 27         rol $27
$fff42a 0a            asl
$fff42b 26 27         rol $27
$fff42d 0a            asl
$fff42e 66 26         ror $26
$fff430 a5 27         lda $27
$fff432 29 1f 05      and #$051f
$fff435 e6 85         inc $85
$fff437 27 8a         and [$8a]
$fff439 c0 00 f0      cpy #$f000
$fff43c 05 a0         ora $a0
$fff43e 23 69         and $69,sp
$fff440 04 c8         tsb $c8
$fff442 e9 07 b0      sbc #$b007
$fff445 fb            xce
$fff446 84 e5         sty $e5
$fff448 aa            tax
$fff449 bd b9 f4      lda $f4b9,x
$fff44c 85 30         sta $30
$fff44e 98            tya
$fff44f 4a            lsr
$fff450 a5 e4         lda $e4
$fff452 85 1c         sta $1c
$fff454 b0 28         bcs $fff47e
$fff456 60            rts
$fff457 20 11 f4      jsr $f411
$fff45a a5 1c         lda $1c
$fff45c 51 26         eor ($26),y
$fff45e 25 30         and $30
$fff460 51 26         eor ($26),y
$fff462 91 26         sta ($26),y
$fff464 60            rts
$fff465 10 23         bpl $fff48a
$fff467 a5 30         lda $30
$fff469 4a            lsr
$fff46a b0 05         bcs $fff471
$fff46c 49 c0 85      eor #$85c0
$fff46f 30 60         bmi $fff4d1
$fff471 88            dey
$fff472 10 02         bpl $fff476
$fff474 a0 27 a9      ldy #$a927
$fff477 c0 85 30      cpy #$3085
$fff47a 84 e5         sty $e5
$fff47c a5 1c         lda $1c
$fff47e 0a            asl
$fff47f c9 c0 10      cmp #$10c0
$fff482 06 a5         asl $a5
$fff484 1c 49 7f      trb $7f49
$fff487 85 1c         sta $1c
$fff489 60            rts
$fff48a a5 30         lda $30
$fff48c 0a            asl
$fff48d 49 80 30      eor #$3080
$fff490 dd a9 81      cmp $81a9,x
$fff493 c8            iny
$fff494 c0 28 90      cpy #$9028
$fff497 e0 a0 00      cpx #$00a0
$fff49a b0 dc         bcs $fff478
$fff49c 18            clc
$fff49d a5 d1         lda $d1
$fff49f 29 04 f0      and #$f004
$fff4a2 25 a9         and $a9
$fff4a4 7f 25 30 31   adc $313025,x
$fff4a8 26 d0         rol $d0
$fff4aa 19 e6 ea      ora $eae6,y
$fff4ad a9 7f 25      lda #$257f
$fff4b0 30 10         bmi $fff4c2
$fff4b2 11 18         ora ($18),y
$fff4b4 a5 d1         lda $d1
$fff4b6 29 04 f0      and #$f004
$fff4b9 0e b1 26      asl $26b1
$fff4bc 45 1c         eor $1c
$fff4be 25 30         and $30
$fff4c0 d0 02         bne $fff4c4
$fff4c2 e6 ea         inc $ea
$fff4c4 51 26         eor ($26),y
$fff4c6 91 26         sta ($26),y
$fff4c8 a5 d1         lda $d1
$fff4ca 65 d3         adc $d3
$fff4cc 29 03 c9      and #$c903
$fff4cf 02 6a         cop $6a
$fff4d1 b0 92         bcs $fff465
$fff4d3 30 30         bmi $fff505
$fff4d5 18            clc
$fff4d6 a5 27         lda $27
$fff4d8 2c b9 f5      bit $f5b9
$fff4db d0 22         bne $fff4ff
$fff4dd 06 26         asl $26
$fff4df b0 1a         bcs $fff4fb
$fff4e1 2c cd f4      bit $f4cd
$fff4e4 f0 05         beq $fff4eb
$fff4e6 69 1f 38      adc #$381f
$fff4e9 b0 12         bcs $fff4fd
$fff4eb 69 23 48      adc #$4823
$fff4ee a5 26         lda $26
$fff4f0 69 b0 b0      adc #$b0b0
$fff4f3 02 69         cop $69
$fff4f5 f0 85         beq $fff47c
$fff4f7 26 68         rol $68
$fff4f9 b0 02         bcs $fff4fd
$fff4fb 69 1f 66      adc #$661f
$fff4fe 26 69         rol $69
$fff500 fc 85 27      jsr ($2785,x)
$fff503 60            rts
$fff504 18            clc
$fff505 a5 27         lda $27
$fff507 69 04 2c      adc #$2c04
$fff50a b9 f5 d0      lda $d0f5,y
$fff50d f3 06         sbc ($06,sp),y
$fff50f 26 90         rol $90
$fff511 18            clc
$fff512 69 e0 18      adc #$18e0
$fff515 2c 08 f5      bit $f508
$fff518 f0 12         beq $fff52c
$fff51a a5 26         lda $26
$fff51c 69 50 49      adc #$4950
$fff51f f0 f0         beq $fff511
$fff521 02 49         cop $49
$fff523 f0 85         beq $fff4aa
$fff525 26 a5         rol $a5
$fff527 e6 90         inc $90
$fff529 02 69         cop $69
$fff52b e0 66 26      cpx #$2666
$fff52e 90 d1         bcc $fff501
$fff530 48            pha
$fff531 a9 00 85      lda #$8500
$fff534 e0 85 e1      cpx #$e185
$fff537 85 e2         sta $e2
$fff539 68            pla
$fff53a 48            pha
$fff53b 38            sec
$fff53c e5 e0         sbc $e0
$fff53e 48            pha
$fff53f 8a            txa
$fff540 e5 e1         sbc $e1
$fff542 85 d3         sta $d3
$fff544 b0 0a         bcs $fff550
$fff546 68            pla
$fff547 49 ff 69      eor #$69ff
$fff54a 01 48         ora ($48,x)
$fff54c a9 00 e5      lda #$e500
$fff54f d3 85         cmp ($85,sp),y
$fff551 d1 85         cmp ($85),y
$fff553 d5 68         cmp $68,x
$fff555 85 d0         sta $d0
$fff557 85 d4         sta $d4
$fff559 68            pla
$fff55a 85 e0         sta $e0
$fff55c 86 e1         stx $e1
$fff55e 98            tya
$fff55f 18            clc
$fff560 e5 e2         sbc $e2
$fff562 90 04         bcc $fff568
$fff564 49 ff 69      eor #$69ff
$fff567 fe 85 d2      inc $d285,x
$fff56a 84 e2         sty $e2
$fff56c 66 d3         ror $d3
$fff56e 38            sec
$fff56f e5 d0         sbc $d0
$fff571 aa            tax
$fff572 a9 ff e5      lda #$e5ff
$fff575 d1 85         cmp ($85),y
$fff577 1d a4 e5      ora $e5a4,x
$fff57a b0 05         bcs $fff581
$fff57c 0a            asl
$fff57d 20 65 f4      jsr $f465
$fff580 38            sec
$fff581 a5 d4         lda $d4
$fff583 65 d2         adc $d2
$fff585 85 d4         sta $d4
$fff587 a5 d5         lda $d5
$fff589 e9 00 85      sbc #$8500
$fff58c d5 b1         cmp $b1,x
$fff58e 26 45         rol $45
$fff590 1c 25 30      trb $3025
$fff593 51 26         eor ($26),y
$fff595 91 26         sta ($26),y
$fff597 e8            inx
$fff598 d0 04         bne $fff59e
$fff59a e6 1d         inc $1d
$fff59c f0 62         beq $fff600
$fff59e a5 d3         lda $d3
$fff5a0 b0 da         bcs $fff57c
$fff5a2 20 d3 f4      jsr $f4d3
$fff5a5 18            clc
$fff5a6 a5 d4         lda $d4
$fff5a8 65 d0         adc $d0
$fff5aa 85 d4         sta $d4
$fff5ac a5 d5         lda $d5
$fff5ae 65 d1         adc $d1
$fff5b0 50 d9         bvc $fff58b
$fff5b2 81 82         sta ($82,x)
$fff5b4 84 88         sty $88
$fff5b6 90 a0         bcc $fff558
$fff5b8 c0 1c ff      cpy #$ff1c
$fff5bb fe fa f4      inc $f4fa,x
$fff5be ec e1 d4      cpx $d4e1
$fff5c1 c5 b4         cmp $b4
$fff5c3 a1 8d         lda ($8d,x)
$fff5c5 78            sei
$fff5c6 61 49         adc ($49,x)
$fff5c8 31 18         and ($18),y
$fff5ca ff a5 26 0a   sbc $0a26a5,x
$fff5ce a5 27         lda $27
$fff5d0 29 03 2a      and #$2a03
$fff5d3 05 26         ora $26
$fff5d5 0a            asl
$fff5d6 0a            asl
$fff5d7 0a            asl
$fff5d8 85 e2         sta $e2
$fff5da a5 27         lda $27
$fff5dc 4a            lsr
$fff5dd 4a            lsr
$fff5de 29 07 05      and #$0507
$fff5e1 e2 85         sep #$85
$fff5e3 e2 a5         sep #$a5
$fff5e5 e5 0a         sbc $0a
$fff5e7 65 e5         adc $e5
$fff5e9 0a            asl
$fff5ea aa            tax
$fff5eb ca            dex
$fff5ec a5 30         lda $30
$fff5ee 29 7f e8      and #$e87f
$fff5f1 4a            lsr
$fff5f2 d0 fc         bne $fff5f0
$fff5f4 85 e1         sta $e1
$fff5f6 8a            txa
$fff5f7 18            clc
$fff5f8 65 e5         adc $e5
$fff5fa 90 02         bcc $fff5fe
$fff5fc e6 e1         inc $e1
$fff5fe 85 e0         sta $e0
$fff600 60            rts
$fff601 86 1a         stx $1a
$fff603 84 1b         sty $1b
$fff605 aa            tax
$fff606 4a            lsr
$fff607 4a            lsr
$fff608 4a            lsr
$fff609 4a            lsr
$fff60a 85 d3         sta $d3
$fff60c 8a            txa
$fff60d 29 0f aa      and #$aa0f
$fff610 bc ba f5      ldy $f5ba,x
$fff613 84 d0         sty $d0
$fff615 49 0f aa      eor #$aa0f
$fff618 bc bb f5      ldy $f5bb,x
$fff61b c8            iny
$fff61c 84 d2         sty $d2
$fff61e a4 e5         ldy $e5
$fff620 a2 00 86      ldx #$8600
$fff623 ea            nop
$fff624 a1 1a         lda ($1a,x)
$fff626 85 d1         sta $d1
$fff628 a2 80 86      ldx #$8680
$fff62b d4 86         pei ($86)
$fff62d d5 a6         cmp $a6,x
$fff62f e7 a5         sbc [$a5]
$fff631 d4 38         pei ($38)
$fff633 65 d0         adc $d0
$fff635 85 d4         sta $d4
$fff637 90 04         bcc $fff63d
$fff639 20 b3 f4      jsr $f4b3
$fff63c 18            clc
$fff63d a5 d5         lda $d5
$fff63f 65 d2         adc $d2
$fff641 85 d5         sta $d5
$fff643 90 03         bcc $fff648
$fff645 20 b4 f4      jsr $f4b4
$fff648 ca            dex
$fff649 d0 e5         bne $fff630
$fff64b a5 d1         lda $d1
$fff64d 4a            lsr
$fff64e 4a            lsr
$fff64f 4a            lsr
$fff650 d0 d4         bne $fff626
$fff652 e6 1a         inc $1a
$fff654 d0 02         bne $fff658
$fff656 e6 1b         inc $1b
$fff658 a1 1a         lda ($1a,x)
$fff65a d0 ca         bne $fff626
$fff65c 60            rts
$fff65d 86 1a         stx $1a
$fff65f 84 1b         sty $1b
$fff661 aa            tax
$fff662 4a            lsr
$fff663 4a            lsr
$fff664 4a            lsr
$fff665 4a            lsr
$fff666 85 d3         sta $d3
$fff668 8a            txa
$fff669 29 0f aa      and #$aa0f
$fff66c bc ba f5      ldy $f5ba,x
$fff66f 84 d0         sty $d0
$fff671 49 0f aa      eor #$aa0f
$fff674 bc bb f5      ldy $f5bb,x
$fff677 c8            iny
$fff678 84 d2         sty $d2
$fff67a a4 e5         ldy $e5
$fff67c a2 00 86      ldx #$8600
$fff67f ea            nop
$fff680 a1 1a         lda ($1a,x)
$fff682 85 d1         sta $d1
$fff684 a2 80 86      ldx #$8680
$fff687 d4 86         pei ($86)
$fff689 d5 a6         cmp $a6,x
$fff68b e7 a5         sbc [$a5]
$fff68d d4 38         pei ($38)
$fff68f 65 d0         adc $d0
$fff691 85 d4         sta $d4
$fff693 90 04         bcc $fff699
$fff695 20 9c f4      jsr $f49c
$fff698 18            clc
$fff699 a5 d5         lda $d5
$fff69b 65 d2         adc $d2
$fff69d 85 d5         sta $d5
$fff69f 90 03         bcc $fff6a4
$fff6a1 20 9d f4      jsr $f49d
$fff6a4 ca            dex
$fff6a5 d0 e5         bne $fff68c
$fff6a7 a5 d1         lda $d1
$fff6a9 4a            lsr
$fff6aa 4a            lsr
$fff6ab 4a            lsr
$fff6ac d0 d4         bne $fff682
$fff6ae e6 1a         inc $1a
$fff6b0 d0 02         bne $fff6b4
$fff6b2 e6 1b         inc $1b
$fff6b4 a1 1a         lda ($1a,x)
$fff6b6 d0 ca         bne $fff682
$fff6b8 60            rts
$fff6b9 20 67 dd      jsr $dd67
$fff6bc 20 52 e7      jsr $e752
$fff6bf a4 51         ldy $51
$fff6c1 a6 50         ldx $50
$fff6c3 c0 01 90      cpy #$9001
$fff6c6 06 d0         asl $d0
$fff6c8 1d e0 18      ora $18e0,x
$fff6cb b0 19         bcs $fff6e6
$fff6cd 8a            txa
$fff6ce 48            pha
$fff6cf 98            tya
$fff6d0 48            pha
$fff6d1 a9 2c 20      lda #$202c
$fff6d4 c0 de 20      cpy #$20de
$fff6d7 f8            sed
$fff6d8 e6 e0         inc $e0
$fff6da c0 b0 09      cpy #$09b0
$fff6dd 86 9d         stx $9d
$fff6df 68            pla
$fff6e0 a8            tay
$fff6e1 68            pla
$fff6e2 aa            tax
$fff6e3 a5 9d         lda $9d
$fff6e5 60            rts
$fff6e6 4c 06 f2      jmp $f206
$fff6e9 20 f8 e6      jsr $e6f8
$fff6ec e0 08 b0      cpx #$b008
$fff6ef f6 bd         inc $bd,x
$fff6f1 f6 f6         inc $f6,x
$fff6f3 85 e4         sta $e4
$fff6f5 60            rts
$fff6f6 00 2a         brk $2a
$fff6f8 55 7f         eor $7f,x
$fff6fa 80 aa         bra $fff6a6
$fff6fc d5 ff         cmp $ff,x
$fff6fe c9 c1 f0      cmp #$f0c1
$fff701 0d 20 b9      ora $b920
$fff704 f6 20         inc $20,x
$fff706 57 f4         eor [$f4],y
$fff708 20 b7 00      jsr $00b7
$fff70b c9 c1 d0      cmp #$d0c1
$fff70e e6 20         inc $20
$fff710 c0 de 20      cpy #$20de
$fff713 b9 f6 84      lda $84f6,y
$fff716 9d a8 8a      sta $8aa8,x
$fff719 a6 9d         ldx $9d
$fff71b 20 3a f5      jsr $f53a
$fff71e 4c 08 f7      jmp $f708
$fff721 20 f8 e6      jsr $e6f8
$fff724 86 f9         stx $f9
$fff726 60            rts
$fff727 20 f8 e6      jsr $e6f8
$fff72a 86 e7         stx $e7
$fff72c 60            rts
$fff72d 20 f8 e6      jsr $e6f8
$fff730 a5 e8         lda $e8
$fff732 85 1a         sta $1a
$fff734 a5 e9         lda $e9
$fff736 85 1b         sta $1b
$fff738 8a            txa
$fff739 a2 00 c1      ldx #$c100
$fff73c 1a            inc
$fff73d f0 02         beq $fff741
$fff73f b0 a5         bcs $fff6e6
$fff741 0a            asl
$fff742 90 03         bcc $fff747
$fff744 e6 1b         inc $1b
$fff746 18            clc
$fff747 a8            tay
$fff748 b1 1a         lda ($1a),y
$fff74a 65 1a         adc $1a
$fff74c aa            tax
$fff74d c8            iny
$fff74e b1 1a         lda ($1a),y
$fff750 65 e9         adc $e9
$fff752 85 1b         sta $1b
$fff754 86 1a         stx $1a
$fff756 20 b7 00      jsr $00b7
$fff759 c9 c5 d0      cmp #$d0c5
$fff75c 09 20 c0      ora #$c020
$fff75f de 20 b9      dec $b920,x
$fff762 f6 20         inc $20,x
$fff764 11 f4         ora ($f4),y
$fff766 a5 f9         lda $f9
$fff768 60            rts
$fff769 20 2d f7      jsr $f72d
$fff76c 4c 05 f6      jmp $f605
$fff76f 20 2d f7      jsr $f72d
$fff772 4c 61 f6      jmp $f661
$fff775 20 cb f3      jsr $f3cb
$fff778 b0 04         bcs $fff77e
$fff77a c0 28 b0      cpy #$b028
$fff77d c1 c0         cmp ($c0,x)
$fff77f 50 b0         bvc $fff731
$fff781 bd 60 48      lda $4860,x
$fff784 a5 2d         lda $2d
$fff786 c9 30 68      cmp #$6830
$fff789 b0 b4         bcs $fff73f
$fff78b 48            pha
$fff78c 20 9f f3      jsr $f39f
$fff78f 68            pla
$fff790 c5 2d         cmp $2d
$fff792 1a            inc
$fff793 90 f6         bcc $fff78b
$fff795 60            rts
$fff796 8a            txa
$fff797 a4 f0         ldy $f0
$fff799 20 9f f3      jsr $f39f
$fff79c c4 2c         cpy $2c
$fff79e b0 f5         bcs $fff795
$fff7a0 c8            iny
$fff7a1 20 ad f3      jsr $f3ad
$fff7a4 80 f6         bra $fff79c
$fff7a6 48            pha
$fff7a7 20 bb f7      jsr $f7bb
$fff7aa 68            pla
$fff7ab 08            php
$fff7ac 20 71 f8      jsr $f871
$fff7af 28            plp
$fff7b0 90 08         bcc $fff7ba
$fff7b2 8d 54 c0      sta $c054
$fff7b5 c9 08 0a      cmp #$0a08
$fff7b8 29 0f 60      and #$600f
$fff7bb 20 cb f3      jsr $f3cb
$fff7be 90 0a         bcc $fff7ca
$fff7c0 98            tya
$fff7c1 49 01 4a      eor #$4a01
$fff7c4 a8            tay
$fff7c5 90 03         bcc $fff7ca
$fff7c7 ad 55 c0      lda $c055
$fff7ca 60            rts
$fff7cb 8a            txa
$fff7cc 2c 1f c0      bit $c01f
$fff7cf 30 12         bmi $fff7e3
$fff7d1 2c 85 24      bit $2485
$fff7d4 38            sec
$fff7d5 8a            txa
$fff7d6 e5 24         sbc $24
$fff7d8 60            rts
$fff7d9 a9 40 85      lda #$8540
$fff7dc 14 20         trb $20
$fff7de e3 df         sbc $df,sp
$fff7e0 64 14         stz $14
$fff7e2 60            rts
$fff7e3 ed 7b 05      sbc $057b
$fff7e6 60            rts
$fff7e7 20 f8 e6      jsr $e6f8
$fff7ea ca            dex
$fff7eb a9 28 c5      lda #$c528
$fff7ee 21 b0         and ($b0,x)
$fff7f0 02 a5         cop $a5
$fff7f2 21 20         and ($20,x)
$fff7f4 d2 f7         cmp ($f7)
$fff7f6 86 24         stx $24
$fff7f8 90 de         bcc $fff7d8
$fff7fa aa            tax
$fff7fb 20 fb da      jsr $dafb
$fff7fe 80 eb         bra $fff7eb
$fff800 4a            lsr
$fff801 08            php
$fff802 20 47 f8      jsr $f847
$fff805 28            plp
$fff806 a9 0f 90      lda #$900f
$fff809 02 69         cop $69
$fff80b e0 85 2e      cpx #$2e85
$fff80e b1 26         lda ($26),y
$fff810 45 30         eor $30
$fff812 25 2e         and $2e
$fff814 51 26         eor ($26),y
$fff816 91 26         sta ($26),y
$fff818 60            rts
$fff819 20 00 f8      jsr $f800
$fff81c c4 2c         cpy $2c
$fff81e b0 11         bcs $fff831
$fff820 c8            iny
$fff821 20 0e f8      jsr $f80e
$fff824 90 f6         bcc $fff81c
$fff826 69 01 48      adc #$4801
$fff829 20 00 f8      jsr $f800
$fff82c 68            pla
$fff82d c5 2d         cmp $2d
$fff82f 90 f5         bcc $fff826
$fff831 60            rts
$fff832 a0 2f d0      ldy #$d02f
$fff835 02 a0         cop $a0
$fff837 27 84         and [$84]
$fff839 2d a0 27      and $27a0
$fff83c a9 00 85      lda #$8500
$fff83f 30 20         bmi $fff861
$fff841 28            plp
$fff842 f8            sed
$fff843 88            dey
$fff844 10 f6         bpl $fff83c
$fff846 60            rts
$fff847 48            pha
$fff848 4a            lsr
$fff849 29 03 09      and #$0903
$fff84c 04 85         tsb $85
$fff84e 27 68         and [$68]
$fff850 29 18 90      and #$9018
$fff853 02 69         cop $69
$fff855 7f 85 26 0a   adc $0a2685,x
$fff859 0a            asl
$fff85a 05 26         ora $26
$fff85c 85 26         sta $26
$fff85e 60            rts
$fff85f a5 30         lda $30
$fff861 18            clc
$fff862 69 03 29      adc #$2903
$fff865 0f 85 30 0a   ora $0a3085
$fff869 0a            asl
$fff86a 0a            asl
$fff86b 0a            asl
$fff86c 05 30         ora $30
$fff86e 85 30         sta $30
$fff870 60            rts
$fff871 4a            lsr
$fff872 08            php
$fff873 20 47 f8      jsr $f847
$fff876 b1 26         lda ($26),y
$fff878 28            plp
$fff879 90 04         bcc $fff87f
$fff87b 4a            lsr
$fff87c 4a            lsr
$fff87d 4a            lsr
$fff87e 4a            lsr
$fff87f 29 0f 60      and #$600f
$fff882 f4 e1 e1      pea $e1e1
$fff885 ab            plb
$fff886 ab            plb
$fff887 60            rts
$fff888 a0 94 80      ldy #$8094
$fff88b 10 a1         bpl $fff82e
$fff88d 3a            dec
$fff88e 38            sec
$fff88f 90 18         bcc $fff8a9
$fff891 4c cc ff      jmp $ffcc
$fff894 4c d1 a9      jmp $a9d1
$fff897 4c d6 a9      jmp $a9d6
$fff89a a0 a0 08      ldy #$08a0
$fff89d 18            clc
$fff89e fb            xce
$fff89f 28            plp
$fff8a0 e2 30         sep #$30
$fff8a2 48            pha
$fff8a3 ad 36 c0      lda $c036
$fff8a6 48            pha
$fff8a7 09 80         ora #$80
$fff8a9 8d 36 c0      sta $c036
$fff8ac 22 98 01 e1   jsr $e10198
$fff8b0 68            pla
$fff8b1 8d 36 c0      sta $c036
$fff8b4 68            pla
$fff8b5 08            php
$fff8b6 38            sec
$fff8b7 fb            xce
$fff8b8 28            plp
$fff8b9 60            rts
$fff8ba 9c 00 08      stz $0800
$fff8bd 18            clc
$fff8be fb            xce
$fff8bf c2 30         rep #$30
$fff8c1 a2 00 08      ldx #$0800
$fff8c4 9b            txy
$fff8c5 c8            iny
$fff8c6 a9 fe b7      lda #$b7fe
$fff8c9 54 00 00      mvn $00,$00
$fff8cc 80 e7         bra $fff8b5
$fff8ce 00 00         brk $00
$fff8d0 a0 99 80      ldy #$8099
$fff8d3 c8            iny
$fff8d4 08            php
$fff8d5 18            clc
$fff8d6 fb            xce
$fff8d7 28            plp
$fff8d8 6b            rtl
$fff8d9 20 f8 e6      jsr $e6f8
$fff8dc e0 50 b0      cpx #$b050
$fff8df 11 86         ora ($86),y
$fff8e1 f0 20         beq $fff903
$fff8e3 be de 20      ldx $20de,y
$fff8e6 f8            sed
$fff8e7 e6 e0         inc $e0
$fff8e9 50 86         bvc $fff871
$fff8eb 2c 86 2d      bit $2d86
$fff8ee 4c 0c f2      jmp $f20c
$fff8f1 4c 06 f2      jmp $f206
$fff8f4 20 ba f8      jsr $f8ba
$fff8f7 20 58 fc      jsr $fc58
$fff8fa a0 a4 20      ldy #$20a4
$fff8fd 9c f8 a0      stz $a0f8
$fff900 9d 20 9c      sta $9c20,x
$fff903 f8            sed
$fff904 20 db f9      jsr $f9db
$fff907 af 41 01 e1   lda $e10141
$fff90b 30 01         bmi $fff90e
$fff90d 60            rts
$fff90e af a5 01 e1   lda $e101a5
$fff912 0a            asl
$fff913 30 f8         bmi $fff90d
$fff915 18            clc
$fff916 fb            xce
$fff917 22 3b 8f ff   jsr $ff8f3b
$fff91b 38            sec
$fff91c fb            xce
$fff91d 4c 62 fa      jmp $fa62
$fff920 af c1 01 e1   lda $e101c1
$fff924 f0 0a         beq $fff930
$fff926 c9 02 d0      cmp #$d002
$fff929 03 4c         ora $4c,sp
$fff92b e0 c2 4c      cpx #$4cc2
$fff92e e0 c1 4c      cpx #$4cc1
$fff931 f8            sed
$fff932 fa            plx
$fff933 00 00         brk $00
$fff935 00 00         brk $00
$fff937 00 00         brk $00
$fff939 00 ad         brk $ad
$fff93b 46 c0         lsr $c0
$fff93d 0a            asl
$fff93e 0a            asl
$fff93f 60            rts
$fff940 98            tya
$fff941 20 da fd      jsr $fdda
$fff944 8a            txa
$fff945 4c da fd      jmp $fdda
$fff948 a2 03 a9      ldx #$a903
$fff94b a0 20 ed      ldy #$ed20
$fff94e fd ca d0      sbc $d0ca,x
$fff951 f8            sed
$fff952 60            rts
$fff953 38            sec
$fff954 a5 2f         lda $2f
$fff956 a4 3b         ldy $3b
$fff958 aa            tax
$fff959 10 01         bpl $fff95c
$fff95b 88            dey
$fff95c 65 3a         adc $3a
$fff95e 90 01         bcc $fff961
$fff960 c8            iny
$fff961 60            rts
$fff962 a0 a6 4c      ldy #$4ca6
$fff965 9c f8 a0      stz $a0f8
$fff968 a7 4c         lda [$4c]
$fff96a 9c f8 5a      stz $5af8
$fff96d a0 9a 90      ldy #$909a
$fff970 02 a0         cop $a0
$fff972 9f 20 9c f8   sta $f89c20,x
$fff976 7a            ply
$fff977 60            rts
$fff978 eb            xba
$fff979 af 37 01 e1   lda $e10137
$fff97d 4a            lsr
$fff97e 90 06         bcc $fff986
$fff980 d0 04         bne $fff986
$fff982 8f 37 01 e1   sta $e10137
$fff986 eb            xba
$fff987 60            rts
$fff988 bc b2 be      ldy $beb2,x
$fff98b 9a            txs
$fff98c ef c4 e9 a9   sbc $a9e9c4
$fff990 bb            tyx
$fff991 a6 a4         ldx $a4
$fff993 06 95         asl $95
$fff995 07 02         ora [$02]
$fff997 05 00         ora $00
$fff999 f1 eb         sbc ($eb),y
$fff99b 93 a7         sta ($a7,sp),y
$fff99d c6 99         dec $99
$fff99f 96 ed         stx $ed,y
$fff9a1 ec 9b f5      cpx $f59b
$fff9a4 f3 ea         sbc ($ea,sp),y
$fff9a6 ee ab ad      inc $adab
$fff9a9 a3 f8         lda $f8,sp
$fff9ab 9c c7 ac      stz $acc7
$fff9ae 12 c9         ora ($c9)
$fff9b0 da            phx
$fff9b1 db            stp
$fff9b2 8c dd 96      sty $96dd
$fff9b5 eb            xba
$fff9b6 0a            asl
$fff9b7 0a            asl
$fff9b8 dc fd 83      jmp [$83fd]
$fff9bb 7f d4 d0 cf   adc $cfd0d4,x
$fff9bf d6 0a         dec $0a,x
$fff9c1 0a            asl
$fff9c2 f5 00         sbc $00,x
$fff9c4 d5 d8         cmp $d8,x
$fff9c6 d7 d1         cmp [$d1],y
$fff9c8 d3 d2         cmp ($d2,sp),y
$fff9ca be ce e0      ldx $e0ce,y
$fff9cd 52 0a         eor ($0a)
$fff9cf 0a            asl
$fff9d0 b3 cd         lda ($cd,sp),y
$fff9d2 18            clc
$fff9d3 fb            xce
$fff9d4 08            php
$fff9d5 22 10 00 fe   jsr $fe0010
$fff9d9 28            plp
$fff9da fb            xce
$fff9db 78            sei
$fff9dc a9 10 0c      lda #$0c10
$fff9df 27 c0         and [$c0]
$fff9e1 58            cli
$fff9e2 af 39 01 e1   lda $e10139
$fff9e6 8d 2d c0      sta $c02d
$fff9e9 a4 00         ldy $00
$fff9eb 5a            phy
$fff9ec a4 01         ldy $01
$fff9ee 5a            phy
$fff9ef a9 00 48      lda #$4800
$fff9f2 a9 c8 64      lda #$64c8
$fff9f5 00 85         brk $85
$fff9f7 01 18         ora ($18,x)
$fff9f9 68            pla
$fff9fa 2a            rol
$fff9fb 48            pha
$fff9fc a0 05 c6      ldy #$c605
$fff9ff 01 a5         ora ($a5,x)
$fffa01 01 c9         ora ($c9,x)
$fffa03 c4 90         cpy $90
$fffa05 14 b1         trb $b1
$fffa07 00 d9         brk $d9
$fffa09 01 fb         ora ($fb,x)
$fffa0b d0 eb         bne $fff9f8
$fffa0d 88            dey
$fffa0e 88            dey
$fffa0f 10 f5         bpl $fffa06
$fffa11 b1 00         lda ($00),y
$fffa13 f0 e4         beq $fff9f9
$fffa15 1a            inc
$fffa16 d0 e0         bne $fff9f8
$fffa18 f0 df         beq $fff9f9
$fffa1a 68            pla
$fffa1b 7a            ply
$fffa1c 84 01         sty $01
$fffa1e 7a            ply
$fffa1f 84 00         sty $00
$fffa21 4c db ff      jmp $ffdb
$fffa24 af d9 02 e1   lda $e102d9
$fffa28 d0 02         bne $fffa2c
$fffa2a 1a            inc
$fffa2b 60            rts
$fffa2c a2 00 ad      ldx #$ad00
$fffa2f 05 c3         ora $c3
$fffa31 c9 38 d0      cmp #$d038
$fffa34 0a            asl
$fffa35 ad 07 c3      lda $c307
$fffa38 c9 18 d0      cmp #$d018
$fffa3b 03 e8         ora $e8,sp
$fffa3d d0 ef         bne $fffa2e
$fffa3f 60            rts
$fffa40 4c 74 c0      jmp $c074
$fffa43 20 3a ff      jsr $ff3a
$fffa46 20 db f9      jsr $f9db
$fffa49 4c 6c ff      jmp $ff6c
$fffa4c 28            plp
$fffa4d 20 4c ff      jsr $ff4c
$fffa50 68            pla
$fffa51 85 3a         sta $3a
$fffa53 68            pla
$fffa54 85 3b         sta $3b
$fffa56 6c f0 03      jmp ($03f0)
$fffa59 a0 93 20      ldy #$2093
$fffa5c 9c f8 80      stz $80f8
$fffa5f e3 00         sbc $00,sp
$fffa61 00 a9         brk $a9
$fffa63 01 0c         ora ($0c,x)
$fffa65 29 c0 a9      and #$a9c0
$fffa68 fb            xce
$fffa69 78            sei
$fffa6a d8            cld
$fffa6b 1b            tcs
$fffa6c 20 36 fe      jsr $fe36
$fffa6f a9 0c 8d      lda #$8d0c
$fffa72 68            pla
$fffa73 c0 a0 09      cpy #$09a0
$fffa76 20 9c f8      jsr $f89c
$fffa79 ad ff cf      lda $cfff
$fffa7c 2c 10 c0      bit $c010
$fffa7f 20 24 fa      jsr $fa24
$fffa82 d0 03         bne $fffa87
$fffa84 20 12 fd      jsr $fd12
$fffa87 20 9a f8      jsr $f89a
$fffa8a ad f3 03      lda $03f3
$fffa8d 49 a5 cd      eor #$cda5
$fffa90 f4 03 d0      pea $d003
$fffa93 12 ad         ora ($ad)
$fffa95 f2 03         sbc ($03)
$fffa97 d0 42         bne $fffadb
$fffa99 a9 e0 cd      lda #$cde0
$fffa9c f3 03         sbc ($03,sp),y
$fffa9e d0 3b         bne $fffadb
$fffaa0 a0 03 4c      ldy #$4c03
$fffaa3 e6 fe         inc $fe
$fffaa5 00 20         brk $20
$fffaa7 f4 f8 a2      pea $a2f8
$fffaaa 05 bd         ora $bd
$fffaac fc fa 9d      jsr ($9dfa,x)
$fffaaf ef 03 ca d0   sbc $d0ca03
$fffab3 f7 80         sbc [$80],y
$fffab5 2b            pld
$fffab6 85 01         sta $01
$fffab8 64 00         stz $00
$fffaba a0 05 c6      ldy #$c605
$fffabd 01 a9         ora ($a9,x)
$fffabf 00 a5         brk $a5
$fffac1 01 c9         ora ($c9,x)
$fffac3 c0 f0 32      cpy #$32f0
$fffac6 8d f8 07      sta $07f8
$fffac9 b1 00         lda ($00),y
$fffacb d9 01 fb      cmp $fb01,y
$ffface d0 ea         bne $fffaba
$fffad0 88            dey
$fffad1 88            dey
$fffad2 10 f5         bpl $fffac9
$fffad4 6c 00 00      jmp ($0000)
$fffad7 a0 a5 80      ldy #$80a5
$fffada 1f 20 d2 f9   ora $f9d220,x
$fffade 6c f2 03      jmp ($03f2)
$fffae1 af e8 02 e1   lda $e102e8
$fffae5 f0 0d         beq $fffaf4
$fffae7 c9 08 90      cmp #$9008
$fffaea 04 80         tsb $80
$fffaec 37 a9         and [$a9],y
$fffaee 05 09         ora $09
$fffaf0 c0 1a 80      cpy #$801a
$fffaf3 c2 a9         rep #$a9
$fffaf5 c8            iny
$fffaf6 80 be         bra $fffab6
$fffaf8 a0 98 4c      ldy #$4c98
$fffafb 9c f8 59      stz $59f8
$fffafe fa            plx
$fffaff 00 e0         brk $e0
$fffb01 45 20         eor $20
$fffb03 ff 00 ff 03   sbc $03ff00,x
$fffb07 ff 3c c1 f0   sbc $f0c13c,x
$fffb0b f0 ec         beq $fffaf9
$fffb0d e5 a0         sbc $a0
$fffb0f dd db c4      cmp $c4db,x
$fffb12 c2 c1         rep #$c1
$fffb14 ff c3 ff ff   sbc $ffffc3,x
$fffb18 cd c1 d8      cmp $d8c1
$fffb1b d9 d0 d3      cmp $d3d0,y
$fffb1e 4c eb fb      jmp $fbeb
$fffb21 4c e6 fb      jmp $fbe6
$fffb24 c9 0a d0      cmp #$d00a
$fffb27 c5 4c         cmp $4c
$fffb29 20 f9 4c      jsr $4cf9
$fffb2c f3 a9         sbc ($a9,sp),y
$fffb2e 00 a9         brk $a9
$fffb30 00 85         brk $85
$fffb32 48            pha
$fffb33 ad 56 c0      lda $c056
$fffb36 ad 54 c0      lda $c054
$fffb39 ad 51 c0      lda $c051
$fffb3c a9 00 f0      lda #$f000
$fffb3f 0b            phd
$fffb40 ad 50 c0      lda $c050
$fffb43 ad 53 c0      lda $c053
$fffb46 20 36 f8      jsr $f836
$fffb49 a9 14 85      lda #$8514
$fffb4c 22 a9 00 85   jsr $8500a9
$fffb50 20 a0 0c      jsr $0ca0
$fffb53 80 a5         bra $fffafa
$fffb55 7f 00 00 00   adc $000000,x
$fffb59 03 00         ora $00,sp
$fffb5b 85 25         sta $25
$fffb5d 4c 22 fc      jmp $fc22
$fffb60 20 58 fc      jsr $fc58
$fffb63 a0 a8 80      ldy #$80a8
$fffb66 93 05         sta ($05,sp),y
$fffb68 07 0b         ora [$0b]
$fffb6a fb            xce
$fffb6b 38            sec
$fffb6c 18            clc
$fffb6d 01 d6         ora ($d6,x)
$fffb6f ad f3 03      lda $03f3
$fffb72 49 a5 8d      eor #$8da5
$fffb75 f4 03 60      pea $6003
$fffb78 c9 8d d0      cmp #$d08d
$fffb7b 18            clc
$fffb7c ac 00 c0      ldy $c000
$fffb7f 10 13         bpl $fffb94
$fffb81 c0 93 d0      cpy #$d093
$fffb84 0f 2c 10 c0   ora $c0102c
$fffb88 ac 00 c0      ldy $c000
$fffb8b 10 fb         bpl $fffb88
$fffb8d c0 83 f0      cpy #$f083
$fffb90 03 2c         ora $2c,sp
$fffb92 10 c0         bpl $fffb54
$fffb94 80 67         bra $fffbfd
$fffb96 a8            tay
$fffb97 b9 48 fa      lda $fa48,y
$fffb9a 20 ac fb      jsr $fbac
$fffb9d 20 02 fd      jsr $fd02
$fffba0 c9 ce b0      cmp #$b0ce
$fffba3 08            php
$fffba4 c9 c9 90      cmp #$90c9
$fffba7 04 c9         tsb $c9
$fffba9 cc d0 ea      cpy $ead0
$fffbac c9 b8 38      cmp #$38b8
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
$fffbc3 29 03 09      and #$0903
$fffbc6 04 85         tsb $85
$fffbc8 29 68 29      and #$2968
$fffbcb 18            clc
$fffbcc 90 02         bcc $fffbd0
$fffbce 69 7f 85      adc #$857f
$fffbd1 28            plp
$fffbd2 0a            asl
$fffbd3 0a            asl
$fffbd4 05 28         ora $28
$fffbd6 85 28         sta $28
$fffbd8 60            rts
$fffbd9 c9 87 d0      cmp #$d087
$fffbdc 1f a0 9e 80   ora $809ea0,x
$fffbe0 48            pha
$fffbe1 ea            nop
$fffbe2 80 f9         bra $fffbdd
$fffbe4 80 f7         bra $fffbdd
$fffbe6 bd 64 c0      lda $c064,x
$fffbe9 10 fb         bpl $fffbe6
$fffbeb a0 95 80      ldy #$8095
$fffbee 3a            dec
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
$fffc04 c9 8d f0      cmp #$f08d
$fffc07 5a            phy
$fffc08 c9 8a f0      cmp #$f08a
$fffc0b 5a            phy
$fffc0c c9 88 d0      cmp #$d088
$fffc0f 75 5a         adc $5a,x
$fffc11 a0 9b 20      ldy #$209b
$fffc14 9c f8 30      stz $30f8
$fffc17 63 7a         adc $7a,sp
$fffc19 60            rts
$fffc1a a5 22         lda $22
$fffc1c c5 25         cmp $25
$fffc1e b0 dc         bcs $fffbfc
$fffc20 c6 25         dec $25
$fffc22 a5 25         lda $25
$fffc24 85 28         sta $28
$fffc26 98            tya
$fffc27 a0 04 4c      ldy #$4c04
$fffc2a 9c f8 49      stz $49f8
$fffc2d c0 f0 28      cpy #$28f0
$fffc30 69 fd 90      adc #$90fd
$fffc33 c0 f0 da      cpy #$daf0
$fffc36 69 fd 90      adc #$90fd
$fffc39 2c f0 de      bit $def0
$fffc3c 69 fd 90      adc #$90fd
$fffc3f 5c d0 ba a0   jmp $a0bad0
$fffc43 0a            asl
$fffc44 d0 e3         bne $fffc29
$fffc46 2c 1f c0      bit $c01f
$fffc49 10 04         bpl $fffc4f
$fffc4b a0 00 80      ldy #$8000
$fffc4e da            phx
$fffc4f 98            tya
$fffc50 48            pha
$fffc51 20 78 fb      jsr $fb78
$fffc54 68            pla
$fffc55 a4 35         ldy $35
$fffc57 60            rts
$fffc58 a0 05 80      ldy #$8005
$fffc5b cd eb 4c      cmp $4ceb
$fffc5e eb            xba
$fffc5f fc 00 00      jsr ($0000,x)
$fffc62 a9 00 85      lda #$8500
$fffc65 24 e6         bit $e6
$fffc67 25 a5         and $a5
$fffc69 25 c5         and $c5
$fffc6b 23 90         and $90,sp
$fffc6d b6 c6         ldx $c6,y
$fffc6f 25 a0         and $a0
$fffc71 06 80         asl $80
$fffc73 b5 90         lda $90,x
$fffc75 02 25         cop $25
$fffc77 32 4c         and ($4c)
$fffc79 f7 fd         sbc [$fd],y
$fffc7b a5 21         lda $21
$fffc7d a0 9c 20      ldy #$209c
$fffc80 9c f8 7a      stz $7af8
$fffc83 80 95         bra $fffc1a
$fffc85 c9 9e f0      cmp #$f09e
$fffc88 03 4c         ora $4c,sp
$fffc8a d9 fb a9      cmp $a9fb,y
$fffc8d 81 80         sta ($80,x)
$fffc8f 02 a9         cop $a9
$fffc91 40            rti
$fffc92 8f 37 01 e1   sta $e10137
$fffc96 60            rts
$fffc97 a0 97 80      ldy #$8097
$fffc9a 8e 00 38      stx $3800
$fffc9d 90 18         bcc $fffcb7
$fffc9f 84 2a         sty $2a
$fffca1 a0 07 b0      ldy #$b007
$fffca4 84 c8         sty $c8
$fffca6 80 81         bra $fffc29
$fffca8 48            pha
$fffca9 ad 36 c0      lda $c036
$fffcac eb            xba
$fffcad a9 80 1c      lda #$1c80
$fffcb0 36 c0         rol $c0,x
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
$fffcce c9 e1 90      cmp #$90e1
$fffcd1 06 c9         asl $c9
$fffcd3 fb            xce
$fffcd4 b0 02         bcs $fffcd8
$fffcd6 29 df 60      and #$60df
$fffcd9 68            pla
$fffcda 38            sec
$fffcdb 48            pha
$fffcdc e9 01 d0      sbc #$d001
$fffcdf fc 68 e9      jsr ($e968,x)
$fffce2 01 d0         ora ($d0,x)
$fffce4 f6 eb         inc $eb,x
$fffce6 8d 36 c0      sta $c036
$fffce9 eb            xba
$fffcea 60            rts
$fffceb af 37 01 e1   lda $e10137
$fffcef 0a            asl
$fffcf0 30 20         bmi $fffd12
$fffcf2 eb            xba
$fffcf3 90 08         bcc $fffcfd
$fffcf5 8f 35 01 e1   sta $e10135
$fffcf9 a9 01 80      lda #$8001
$fffcfc 95 c9         sta $c9,x
$fffcfe a0 4c ff      ldy #$ff4c
$fffd01 fb            xce
$fffd02 20 0c fd      jsr $fd0c
$fffd05 a0 01 80      ldy #$8001
$fffd08 90 4e         bcc $fffd58
$fffd0a f8            sed
$fffd0b 07 a0         ora [$a0]
$fffd0d 0b            phd
$fffd0e d0 0f         bne $fffd1f
$fffd10 80 06         bra $fffd18
$fffd12 a0 a2 80      ldy #$80a2
$fffd15 83 00         sta $00,sp
$fffd17 00 6c         brk $6c
$fffd19 38            sec
$fffd1a 00 a0         brk $a0
$fffd1c 03 80         ora $80,sp
$fffd1e e8            inx
$fffd1f 20 9c f8      jsr $f89c
$fffd22 80 ec         bra $fffd10
$fffd24 20 78 f9      jsr $f978
$fffd27 b0 4c         bcs $fffd75
$fffd29 c9 88 80      cmp #$8088
$fffd2c 25 00         and $00
$fffd2e 00 20         brk $20
$fffd30 02 fd         cop $fd
$fffd32 20 a0 fb      jsr $fba0
$fffd35 20 09 fd      jsr $fd09
$fffd38 c9 9b f0      cmp #$f09b
$fffd3b f3 60         sbc ($60,sp),y
$fffd3d 6c fe 03      jmp ($03fe)
$fffd40 a0 0d 20      ldy #$200d
$fffd43 9c f8 a4      stz $a4f8
$fffd46 24 9d         bit $9d
$fffd48 00 02         brk $02
$fffd4a 20 ed fd      jsr $fded
$fffd4d bd 00 02      lda $0200,x
$fffd50 80 d2         bra $fffd24
$fffd52 f0 1d         beq $fffd71
$fffd54 c9 98 f0      cmp #$f098
$fffd57 0a            asl
$fffd58 e0 f8 90      cpx #$90f8
$fffd5b 03 20         ora $20,sp
$fffd5d 3a            dec
$fffd5e ff e8 d0 13   sbc $13d0e8,x
$fffd62 a9 dc 20      lda #$20dc
$fffd65 ed fd 20      sbc $20fd
$fffd68 8e fd a5      stx $a5fd
$fffd6b 33 20         and ($20,sp),y
$fffd6d ed fd a2      sbc $a2fd
$fffd70 01 8a         ora ($8a,x)
$fffd72 f0 f3         beq $fffd67
$fffd74 ca            dex
$fffd75 20 35 fd      jsr $fd35
$fffd78 c9 95 d0      cmp #$d095
$fffd7b 08            php
$fffd7c b1 28         lda ($28),y
$fffd7e 2c 1f c0      bit $c01f
$fffd81 30 bd         bmi $fffd40
$fffd83 ea            nop
$fffd84 20 be fd      jsr $fdbe
$fffd87 c9 8d d0      cmp #$d08d
$fffd8a bf 20 9c fc   lda $fc9c20,x
$fffd8e a9 8d 80      lda #$808d
$fffd91 5b            tcd
$fffd92 a4 3d         ldy $3d
$fffd94 a6 3c         ldx $3c
$fffd96 20 8e fd      jsr $fd8e
$fffd99 20 40 f9      jsr $f940
$fffd9c a0 00 a9      ldy #$a900
$fffd9f ba            tsx
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
$fffdb4 c9 ae f0      cmp #$f0ae
$fffdb7 f5 5a         sbc $5a,x
$fffdb9 a0 a1 4c      ldy #$4ca1
$fffdbc 73 f9         adc ($f9,sp),y
$fffdbe c9 ff d0      cmp #$d0ff
$fffdc1 11 eb         ora ($eb),y
$fffdc3 af 37 01 e1   lda $e10137
$fffdc7 0a            asl
$fffdc8 eb            xba
$fffdc9 90 06         bcc $fffdd1
$fffdcb 2c 1f c0      bit $c01f
$fffdce 10 03         bpl $fffdd3
$fffdd0 1a            inc
$fffdd1 29 88 9d      and #$9d88
$fffdd4 00 02         brk $02
$fffdd6 60            rts
$fffdd7 4c 46 fc      jmp $fc46
$fffdda 48            pha
$fffddb 4a            lsr
$fffddc 4a            lsr
$fffddd 4a            lsr
$fffdde 4a            lsr
$fffddf 20 e5 fd      jsr $fde5
$fffde2 68            pla
$fffde3 29 0f 09      and #$090f
$fffde6 b0 c9         bcs $fffdb1
$fffde8 ba            tsx
$fffde9 90 02         bcc $fffded
$fffdeb 69 06 6c      adc #$6c06
$fffdee 36 00         rol $00,x
$fffdf0 48            pha
$fffdf1 c9 a0 4c      cmp #$4ca0
$fffdf4 74 fc         stz $fc,x
$fffdf6 48            pha
$fffdf7 84 35         sty $35
$fffdf9 a8            tay
$fffdfa 68            pla
$fffdfb 80 da         bra $fffdd7
$fffdfd c6 34         dec $34
$fffdff f0 a1         beq $fffda2
$fffe01 ca            dex
$fffe02 d0 0c         bne $fffe10
$fffe04 c9 ba d0      cmp #$d0ba
$fffe07 a8            tay
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
$fffe3e a0 a3 20      ldy #$20a3
$fffe41 9c f8 20      stz $20f8
$fffe44 84 fe         sty $fe
$fffe46 20 2f fb      jsr $fb2f
$fffe49 20 93 fe      jsr $fe93
$fffe4c 80 3b         bra $fffe89
$fffe4e a0 a9 4c      ldy #$4ca9
$fffe51 9c f8 a0      stz $a0f8
$fffe54 aa            tax
$fffe55 4c 9c f8      jmp $f89c
$fffe58 00 00         brk $00
$fffe5a 00 00         brk $00
$fffe5c 00 00         brk $00
$fffe5e a0 83 80      ldy #$8083
$fffe61 11 8a         ora ($8a),y
$fffe63 f0 07         beq $fffe6c
$fffe65 b5 3c         lda $3c,x
$fffe67 95 3a         sta $3a,x
$fffe69 ca            dex
$fffe6a 10 f9         bpl $fffe65
$fffe6c 60            rts
$fffe6d 48            pha
$fffe6e 98            tya
$fffe6f 09 80 a8      ora #$a880
$fffe72 68            pla
$fffe73 80 36         bra $fffeab
$fffe75 a5 21         lda $21
$fffe77 c9 48 a9      cmp #$a948
$fffe7a 0f b0 02 a9   ora $a902b0
$fffe7e 07 60         ora [$60]
$fffe80 a0 3f d0      ldy #$d03f
$fffe83 02 a0         cop $a0
$fffe85 ff 84 32 60   sbc $603284,x
$fffe89 a9 00 85      lda #$8500
$fffe8c 3e a2 38      rol $38a2,x
$fffe8f a0 1b d0      ldy #$d01b
$fffe92 08            php
$fffe93 a9 00 85      lda #$8500
$fffe96 3e a2 36      rol $36a2,x
$fffe99 a0 f0 a5      ldy #$a5f0
$fffe9c 3e 29 0f      rol $0f29,x
$fffe9f f0 04         beq $fffea5
$fffea1 09 c0 a0      ora #$a0c0
$fffea4 00 94         brk $94
$fffea6 00 95         brk $95
$fffea8 01 a0         ora ($a0,x)
$fffeaa 0e 80 36      asl $3680
$fffead 4c 03 e0      jmp $e003
$fffeb0 ad 54 c0      lda $c054
$fffeb3 60            rts
$fffeb4 80 5a         bra $ffff10
$fffeb6 20 62 fe      jsr $fe62
$fffeb9 20 3f ff      jsr $ff3f
$fffebc 6c 3a 00      jmp ($003a)
$fffebf ad d0 03      lda $03d0
$fffec2 c9 4c f0      cmp #$f04c
$fffec5 01 60         ora ($60,x)
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
$fffee1 a0 96 4c      ldy #$4c96
$fffee4 9c f8 8c      stz $8cf8
$fffee7 f2 03         sbc ($03)
$fffee9 20 d2 f9      jsr $f9d2
$fffeec 4c 00 e0      jmp $e000
$fffeef 68            pla
$fffef0 68            pla
$fffef1 a9 98 4c      lda #$4c98
$fffef4 ed fd 20      sbc $20fd
$fffef7 fd fd 68      sbc $68fd,x
$fffefa 68            pla
$fffefb 80 6f         bra $ffff6c
$fffefd 60            rts
$fffefe af 3e 01 e1   lda $e1013e
$ffff02 8f 3f 01 e1   sta $e1013f
$ffff06 a2 01 b5      ldx #$b501
$ffff09 3e 95 42      rol $4295,x
$ffff0c ca            dex
$ffff0d 10 f9         bpl $ffff08
$ffff0f 60            rts
$ffff10 c8            iny
$ffff11 80 bb         bra $fffece
$ffff13 20 ca fc      jsr $fcca
$ffff16 c9 a0 f0      cmp #$f0a0
$ffff19 f9 60 b0      sbc $b060,y
$ffff1c 6d c9 a0      adc $a0c9
$ffff1f d0 28         bne $ffff49
$ffff21 b9 00 02      lda $0200,y
$ffff24 a2 07 c9      ldx #$c907
$ffff27 8d f0 7d      sta $7df0
$ffff2a c8            iny
$ffff2b d0 63         bne $ffff90
$ffff2d a9 c5 20      lda #$20c5
$ffff30 ed fd a9      sbc $a9fd
$ffff33 d2 20         cmp ($20)
$ffff35 ed fd 20      sbc $20fd
$ffff38 ed fd a9      sbc $a9fd
$ffff3b 87 4c         sta [$4c]
$ffff3d ed fd a5      sbc $a5fd
$ffff40 48            pha
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
$ffff5e a9 aa 85      lda #$85aa
$ffff61 33 4c         and ($4c,sp),y
$ffff63 67 fd         adc [$fd]
$ffff65 d8            cld
$ffff66 4c 43 fa      jmp $fa43
$ffff69 20 e1 fe      jsr $fee1
$ffff6c ea            nop
$ffff6d 20 5e ff      jsr $ff5e
$ffff70 20 d1 ff      jsr $ffd1
$ffff73 20 88 f8      jsr $f888
$ffff76 84 34         sty $34
$ffff78 a0 25 88      ldy #$8825
$ffff7b 30 e8         bmi $ffff65
$ffff7d d9 88 f9      cmp $f988,y
$ffff80 d0 f8         bne $ffff7a
$ffff82 20 be ff      jsr $ffbe
$ffff85 a4 34         ldy $34
$ffff87 4c 73 ff      jmp $ff73
$ffff8a a2 03 0a      ldx #$0a03
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
$ffffa7 a2 00 86      ldx #$8600
$ffffaa 3e 86 3f      rol $3f86,x
$ffffad 20 ca fc      jsr $fcca
$ffffb0 ea            nop
$ffffb1 49 b0 c9      eor #$c9b0
$ffffb4 0a            asl
$ffffb5 90 d3         bcc $ffff8a
$ffffb7 69 88 c9      adc #$c988
$ffffba fa            plx
$ffffbb 4c 1b ff      jmp $ff1b
$ffffbe a9 fe 48      lda #$48fe
$ffffc1 b9 ad f9      lda $f9ad,y
$ffffc4 48            pha
$ffffc5 a5 31         lda $31
$ffffc7 a0 00 84      ldy #$8400
$ffffca 31 60         and ($60),y
$ffffcc a0 00 80      ldy #$8000
$ffffcf 20 ef 64      jsr $64ef
$ffffd2 34 20         bit $20,x
$ffffd4 c7 ff         cmp [$ff]
$ffffd6 ad 36 c0      lda $c036
$ffffd9 29 7f 0f      and #$0f7f
$ffffdc 38            sec
$ffffdd 01 e1         ora ($e1,x)
$ffffdf 8d 36 c0      sta $c036
$ffffe2 60            rts
$ffffe3 00 7b         brk $7b
$ffffe5 c0 71 c0      cpy #$c071
$ffffe8 79 c0 fb      adc $fbc0,y
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
