# Apple IO registers

## Essential

### $C029 - NEWVIDEO

Bit 0 - always assume it's going to be '1' for now - meaning the 17th
        address bit is ignored - if it ever changes to '0' in code, decipher
        the code to see what it's trying to do

### $C068 - STATEREG
