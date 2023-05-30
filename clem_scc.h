#ifndef CLEM_SCC_H
#define CLEM_SCC_H

#include "clem_mmio_types.h"

#ifdef __cplusplus
extern "C" {
#endif

//  Receiver options WR3
#define CLEM_SCC_RX_ENABLE         0x01
#define CLEM_SCC_TX_RX_AUTO_ENABLE 0x20

//  Misc Data Format/Rate WR4
#define CLEM_SCC_PARITY_ENABLED 0x01
#define CLEM_SCC_PARITY_EVEN    0x02
#define CLEM_SCC_STOP_SYNC_MODE 0x00
#define CLEM_SCC_STOP_BIT_1     0x04
#define CLEM_SCC_STOP_BIT_1_5   0x08
#define CLEM_SCC_STOP_BIT_2     0x0c
#define CLEM_SCC_MONO_SYNC_MODE 0x00
#define CLEM_SCC_BI_SYNC_MODE   0x10
//#define CLEM_SCC_SDLC_SYNC_MODE      0x20
#define CLEM_SCC_EXT_SYNC_MODE 0x30
#define CLEM_SCC_CLOCK_X1      0x00
#define CLEM_SCC_CLOCK_X16     0x40
#define CLEM_SCC_CLOCK_X32     0x80
#define CLEM_SCC_CLOCK_X64     0xc0

//  Transmit protocol WR5
#define CLEM_SCC_TX_RTS            0x02
#define CLEM_SCC_TX_ENABLE         0x08
#define CLEM_SCC_TX_SEND_BREAK     0x10
#define CLEM_SCC_TX_BITS_5_OR_LESS 0x00
#define CLEM_SCC_TX_BITS_7         0x20
#define CLEM_SCC_TX_BITS_6         0x40
#define CLEM_SCC_TX_BITS_8         0x60
#define CLEM_SCC_TX_DTR            0x80

//  WR10
#define CLEM_SCC_SYNC_SIZE_8BIT 0x00
#define CLEM_SCC_SYNC_SIZE_6BIT 0x01

//  Clock Mode (WR11)
#define CLEM_SCC_CLK_TRxC_OUT_XTAL 0x00
#define CLEM_SCC_CLK_TRxC_OUT_XMIT 0x01
#define CLEM_SCC_CLK_TRxC_OUT_BRG  0x02
#define CLEM_SCC_CLK_TRxC_OUT_DPLL 0x03
#define CLEM_SCC_TRxC_OUT_ENABLE   0x04
#define CLEM_SCC_CLK_SOURCE_RTxC   0x00
#define CLEM_SCC_CLK_SOURCE_TRxC   0x08
#define CLEM_SCC_CLK_SOURCE_BRG    0x10
#define CLEM_SCC_CLK_SOURCE_DPLL   0x18

//  RR0
#define CLEM_SCC_IN_CTS 0x20

//  Port settings
void clem_scc_channel_port_lo(struct ClemensDeviceSCCChannel *channel, uint8_t flags);
void clem_scc_channel_port_hi(struct ClemensDeviceSCCChannel *channel, uint8_t flags);

#ifdef __cplusplus
}
#endif

#endif
