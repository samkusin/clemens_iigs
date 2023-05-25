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

void test_scc_tdd(void) {
    /* An attempt at a simple 'null-modem' like connection between the 'test' 
       and the SCC to iterate on the initial functionality
       
       - A method to send a byte from SCC       (tx)
       - A method to recv a byte from the Test  (rx)

       - Order of Operations
        - Transmit data to test
        - Test receives data and transmits a response        
    */
    

}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_scc_tdd);
    return UNITY_END();
}
