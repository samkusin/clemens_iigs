#ifndef CLEM_CARD_MOCKINGBOARD_H
#define CLEM_CARD_MOCKINGBOARD_H

#include "clem_shared.h"

typedef struct mpack_writer_t mpack_writer_t;
typedef struct mpack_reader_t mpack_reader_t;


/* these are here for references - the actual functions are determined
   by which bits in the address register are set on io_read and io_write
*/
#define CLEM_CARD_MOCKINGBOARD_ORB1 0x00
#define CLEM_CARD_MOCKINGBOARD_ORA1 0x01
#define CLEM_CARD_MOCKINGBOARD_DDRB1 0x02
#define CLEM_CARD_MOCKINGBOARD_DDRA1 0x03
#define CLEM_CARD_MOCKINGBOARD_ORB2 0x80
#define CLEM_CARD_MOCKINGBOARD_ORA2 0x81
#define CLEM_CARD_MOCKINGBOARD_DDRB2 0x82
#define CLEM_CARD_MOCKINGBOARD_DDRA2 0x83

#ifdef __cplusplus
extern "C" {
#endif

void clem_card_mockingboard_initialize(ClemensCard *card);
void clem_card_mockingboard_uninitialize(ClemensCard *card);
void clem_card_mockingboard_serialize(mpack_writer_t* writer, ClemensCard* card);
void clem_card_mockingboard_unserialize(mpack_reader_t* reader, ClemensCard* card,
                                        ClemensSerializerAllocateCb alloc_cb,
                                        void* context);
unsigned clem_card_ay3_render(ClemensCard *card, float *samples_out,
                              unsigned sample_limit, unsigned samples_per_frame,
                              unsigned samples_per_second);

#ifdef __cplusplus
}
#endif

#endif
