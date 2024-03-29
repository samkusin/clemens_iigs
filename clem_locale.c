#include "clem_locale.h"

//  matrix of characters to keyboard types with pairs of modifier + adb code

static uint16_t s_xlat_latin_to_adb[256][CLEM_ADB_LOCALE_KEYB_COUNT] = {
    {0x0000}, // chr$(0)
    {0x0200}, // chr$(1)
    {0x020b}, // chr$(2)
    {0x0208}, // chr$(3)
    {0x0202}, // chr$(4)
    {0x020e}, // chr$(5)
    {0x0203}, // chr$(6)
    {0x0205}, // chr$(7)
    {0x0204}, // chr$(8)
    {0x0222}, // chr$(9)
    {0x0024}, // chr$(10)
    {0x0228}, // chr$(11)
    {0x0225}, // chr$(12)
    {0x0024}, // chr$(13)
    {0x022d}, // chr$(14)
    {0x021f}, // chr$(15)
    {0x0223}, // chr$(16)
    {0x020c}, // chr$(17)
    {0x020f}, // chr$(18)
    {0x0201}, // chr$(19)
    {0x0210}, // chr$(20)
    {0x0220}, // chr$(21)
    {0x0209}, // chr$(22)
    {0x020d}, // chr$(23)
    {0x0207}, // chr$(24)
    {0x0211}, // chr$(25)
    {0x0206}, // chr$(26)
    {0x0221}, // chr$(27)
    {0x022a}, // chr$(28)
    {0x021e}, // chr$(29)
    {0x0216}, // chr$(30)
    {0x021b}, // chr$(31)
    {0x0031}, // ' '
    {0x0112}, // !
    {0x0127}, // "
    {0x0114}, // #
    {0x0115}, // $
    {0x0117}, // %
    {0x011a}, // &
    {0x0027}, // '
    {0x0119}, // (
    {0x011d}, // )
    {0x011c}, // *
    {0x0118}, // +
    {0x002b}, // ,
    {0x001b}, // -
    {0x002f}, // .
    {0x002c}, // /
    {0x001d}, // 0
    {0x0012}, // 1
    {0x0013}, // 2
    {0x0014}, // 3
    {0x0015}, // 4
    {0x0017}, // 5
    {0x0016}, // 6
    {0x001a}, // 7
    {0x001c}, // 8
    {0x0019}, // 9
    {0x0129}, // :
    {0x0029}, // ;
    {0x012b}, // <
    {0x0018}, // =
    {0x012f}, // >
    {0x012c}, // ?
    {0x0113}, // @
    {0x0100}, // A
    {0x010b}, // B
    {0x0108}, // C
    {0x0102}, // D
    {0x010e}, // E
    {0x0103}, // F
    {0x0105}, // G
    {0x0104}, // H
    {0x0122}, // I
    {0x0126}, // J
    {0x0128}, // K
    {0x0125}, // L
    {0x012e}, // M
    {0x012d}, // N
    {0x011f}, // O
    {0x0123}, // P
    {0x010c}, // Q
    {0x010f}, // R
    {0x0101}, // S
    {0x0110}, // T
    {0x0120}, // U
    {0x0109}, // V
    {0x010d}, // W
    {0x0107}, // X
    {0x0111}, // Y
    {0x0106}, // Z
    {0x0021}, // [
    {0x002a}, // '\'
    {0x001e}, // ]
    {0x0116}, // ^
    {0x011b}, // _
    {0x0032}, // `
    {0x0000}, // a
    {0x000b}, // b
    {0x0008}, // c
    {0x0002}, // d
    {0x000e}, // e
    {0x0003}, // f
    {0x0005}, // g
    {0x0004}, // h
    {0x0022}, // i
    {0x0026}, // j
    {0x0028}, // k
    {0x0025}, // l
    {0x002e}, // m
    {0x002d}, // n
    {0x001f}, // o
    {0x0023}, // p
    {0x000c}, // q
    {0x000f}, // r
    {0x0001}, // s
    {0x0010}, // t
    {0x0020}, // u
    {0x0009}, // v
    {0x000d}, // w
    {0x0007}, // x
    {0x0011}, // y
    {0x0006}, // z
    {0x0121}, // {
    {0x012a}, // |
    {0x011e}, // }
    {0x0132}, // ~
    {0x0033}, // chr$(127)
    {0x0000}, // chr$(128)
    {0x0000}, // chr$(129)
    {0x0000}, // chr$(130)
    {0x0000}, // chr$(131)
    {0x0000}, // chr$(132)
    {0x0000}, // chr$(133)
    {0x0000}, // chr$(134)
    {0x0000}, // chr$(135)
    {0x0000}, // chr$(136)
    {0x0000}, // chr$(137)
    {0x0000}, // chr$(138)
    {0x0000}, // chr$(139)
    {0x0000}, // chr$(140)
    {0x0000}, // chr$(141)
    {0x0000}, // chr$(142)
    {0x0000}, // chr$(143)
    {0x0000}, // chr$(144)
    {0x0000}, // chr$(145)
    {0x0000}, // chr$(146)
    {0x0000}, // chr$(147)
    {0x0000}, // chr$(148)
    {0x0000}, // chr$(149)
    {0x0000}, // chr$(150)
    {0x0000}, // chr$(151)
    {0x0000}, // chr$(152)
    {0x0000}, // chr$(153)
    {0x0000}, // chr$(154)
    {0x0000}, // chr$(155)
    {0x0000}, // chr$(156)
    {0x0000}, // chr$(157)
    {0x0000}, // chr$(158)
    {0x0000}, // chr$(159)
    {0x0000}, // chr$(160)
    {0x0000}, // chr$(161)
    {0x0000}, // chr$(162)
    {0x0000}, // chr$(163)
    {0x0000}, // chr$(164)
    {0x0000}, // chr$(165)
    {0x0000}, // chr$(166)
    {0x0000}, // chr$(167)
    {0x0000}, // chr$(168)
    {0x0000}, // chr$(169)
    {0x0000}, // chr$(170)
    {0x0000}, // chr$(171)
    {0x0000}, // chr$(172)
    {0x0000}, // chr$(173)
    {0x0000}, // chr$(174)
    {0x0000}, // chr$(175)
    {0x0000}, // chr$(176)
    {0x0000}, // chr$(177)
    {0x0000}, // chr$(178)
    {0x0000}, // chr$(179)
    {0x0000}, // chr$(180)
    {0x0000}, // chr$(181)
    {0x0000}, // chr$(182)
    {0x0000}, // chr$(183)
    {0x0000}, // chr$(184)
    {0x0000}, // chr$(185)
    {0x0000}, // chr$(186)
    {0x0000}, // chr$(187)
    {0x0000}, // chr$(188)
    {0x0000}, // chr$(189)
    {0x0000}, // chr$(190)
    {0x0000}, // chr$(191)
    {0x0000}, // chr$(192)
    {0x0000}, // chr$(193)
    {0x0000}, // chr$(194)
    {0x0000}, // chr$(195)
    {0x0000}, // chr$(196)
    {0x0000}, // chr$(197)
    {0x0000}, // chr$(198)
    {0x0000}, // chr$(199)
    {0x0000}, // chr$(200)
    {0x0000}, // chr$(201)
    {0x0000}, // chr$(202)
    {0x0000}, // chr$(203)
    {0x0000}, // chr$(204)
    {0x0000}, // chr$(205)
    {0x0000}, // chr$(206)
    {0x0000}, // chr$(207)
    {0x0000}, // chr$(208)
    {0x0000}, // chr$(209)
    {0x0000}, // chr$(210)
    {0x0000}, // chr$(211)
    {0x0000}, // chr$(212)
    {0x0000}, // chr$(213)
    {0x0000}, // chr$(214)
    {0x0000}, // chr$(215)
    {0x0000}, // chr$(216)
    {0x0000}, // chr$(217)
    {0x0000}, // chr$(218)
    {0x0000}, // chr$(219)
    {0x0000}, // chr$(220)
    {0x0000}, // chr$(221)
    {0x0000}, // chr$(222)
    {0x0000}, // chr$(223)
    {0x0000}, // chr$(224)
    {0x0000}, // chr$(225)
    {0x0000}, // chr$(226)
    {0x0000}, // chr$(227)
    {0x0000}, // chr$(228)
    {0x0000}, // chr$(229)
    {0x0000}, // chr$(230)
    {0x0000}, // chr$(231)
    {0x0000}, // chr$(232)
    {0x0000}, // chr$(233)
    {0x0000}, // chr$(234)
    {0x0000}, // chr$(235)
    {0x0000}, // chr$(236)
    {0x0000}, // chr$(237)
    {0x0000}, // chr$(238)
    {0x0000}, // chr$(239)
    {0x0000}, // chr$(240)
    {0x0000}, // chr$(241)
    {0x0000}, // chr$(242)
    {0x0000}, // chr$(243)
    {0x0000}, // chr$(244)
    {0x0000}, // chr$(245)
    {0x0000}, // chr$(246)
    {0x0000}, // chr$(247)
    {0x0000}, // chr$(248)
    {0x0000}, // chr$(249)
    {0x0000}, // chr$(250)
    {0x0000}, // chr$(251)
    {0x0000}, // chr$(252)
    {0x0000}, // chr$(253)
    {0x0000}, // chr$(254)
    {0x0000}  // chr$(255)
};

uint16_t clem_iso_latin_1_to_adb_key_and_modifier(unsigned char ch, int keyb_type) {
    return s_xlat_latin_to_adb[ch][keyb_type];
}
