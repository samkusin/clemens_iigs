## References
* [Official diagnostics document](
https://mirrors.apple2.org.za/Apple%20II%20Documentation%20Project/Computers/Apple%20II/Apple%20IIGS/Documentation/Apple%20IIGS%20Diagnostics.pdf)

## ROM 3

### Self Test 5

### 01 (Speed Slow Stuck)

Ref: Page 11, Speed Section.

Version 0.5 and lower fails.

X should output either 19 or 1A to pass.
X outputs 1E.  Investigating VERTCNT and underlying implementation.

Timing is everything - Mega2 access might not be "slow" enough here to extend the time of the loop.
https://youtu.be/LP5k15g_0AM?t=2946

```
11237664 | FF | 6AF1 | ( 2)  LDX #$00       | PC=6AF3, PBR=FF, DBR=00, S=01FA, D=0000, e=0, A=80, X=00, Y=04, 00110111
11237665 | FF | 6AF3 | ( 5)  LDA $00C02E    | PC=6AF7, PBR=FF, DBR=00, S=01FA, D=0000, e=0, A=9F, X=00, Y=04, 10110101
11237722 | FF | 6AF7 | ( 5)  CMP $00C02E    | PC=6AFB, PBR=FF, DBR=00, S=01FA, D=0000, e=0, A=9F, X=00, Y=04, 10110100
11237723 | FF | 6AFB | ( 2)  BEQ $FA (-6)   | PC=6AFD, PBR=FF, DBR=00, S=01FA, D=0000, e=0, A=9F, X=00, Y=04, 10110100
11237724 | FF | 6AFD | ( 5)  LDA $00C02E    | PC=6B01, PBR=FF, DBR=00, S=01FA, D=0000, e=0, A=A0, X=00, Y=04, 10110100
11237812 | FF | 6B01 | ( 2)  INX            | PC=6B02, PBR=FF, DBR=00, S=01FA, D=0000, e=0, A=A0, X=1E, Y=04, 00110101
11237813 | FF | 6B02 | ( 5)  CMP $00C02E    | PC=6B06, PBR=FF, DBR=00, S=01FA, D=0000, e=0, A=A0, X=1E, Y=04, 10110100
11237814 | FF | 6B06 | ( 2)  BEQ $F9 (-7)   | PC=6B08, PBR=FF, DBR=00, S=01FA, D=0000, e=0, A=A0, X=1E, Y=04, 10110100
11237815 | FF | 6B08 | ( 6)  RTS            | PC=6ACF, PBR=FF, DBR=00, S=01FC, D=0000, e=0, A=A0, X=1E, Y=04, 10110100
```

Solution:

