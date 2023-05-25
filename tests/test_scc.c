#include "emulator.h"
#include "unity.h"

#include "clem_device.h"
#include "clem_mmio_defs.h"

#include <stdio.h>
#include <string.h>



static struct ClemensDeviceSCC scc_dev;

void setUp(void) {
    memset(&scc_dev, 0, sizeof(scc_dev));
    clem_scc_reset(&scc_dev);
}

void tearDown(void) {}

void test_scc_tdd_null_modem(void) {
    /*  An attempt at a simple null-modem  connection between the 'test' and
        the SCC to iterate on the initial functionality.  RTS/CTS/Tx/Rx
        functionality will be tested
       
        Send bytes from the test bit generator to the SCC
        Send bytes from the SCC to the test bit generator

        DTE/SCC    PORT                             DCE/Peer
        ============================================================
        1. Peer -> SCC tranmission
            RTS        TX_D_HI  --------------------->  CTS                                                    
            RxD        RX_D_LO  <---------------------  TxD
            .                                           .
            .                                           .


            CTS        HSKI     <------------------     RTS 
            DTR        DTR      --------------------->  CD
            DCD        GPI      <---------------------  DTR
    */
    
    //  mini_db8 = 0
    //  db9 = 0
    //  clem_comms_peer_init(&serial_peer);
    //  clem_comms_peer_queue_send_text(&serial_peer, "Hello World");
    //  while clem_comms_peer_can_send(&serial_peer):
    //      db9 = clem_comms_peer_communicate(&serial_peer, mini_db8_to_db9(mini_db8))
    //      mini_db8 = clem_scc_serial_communicate(&scc, db9_to_mini_db8(db9))

}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_scc_tdd_null_modem);
    return UNITY_END();
}
