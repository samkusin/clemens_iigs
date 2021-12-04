#ifndef CLEM_CARD_MOCKINGBOARD_H
#define CLEM_CARD_MOCKINGBOARD_H

#include "clem_shared.h"


/* these are here for references - the actual functions are determined
   by which bits in the address register are set on io_read and io_write
*/
#define CLEM_CARD_MOCKINGBOARD_ORB1     0x00
#define CLEM_CARD_MOCKINGBOARD_ORA1     0x01
#define CLEM_CARD_MOCKINGBOARD_DDRB1    0x02
#define CLEM_CARD_MOCKINGBOARD_DDRA1    0x03
#define CLEM_CARD_MOCKINGBOARD_ORB2     0x80
#define CLEM_CARD_MOCKINGBOARD_ORA2     0x81
#define CLEM_CARD_MOCKINGBOARD_DDRB2    0x82
#define CLEM_CARD_MOCKINGBOARD_DDRA2    0x83

#ifdef __cplusplus
extern "C" {
#endif

void clem_card_mockingboard_initialize(ClemensCard* card);
void clem_card_mockingboard_uninitialize(ClemensCard* card);

#ifdef __cplusplus
}
#endif

#endif
