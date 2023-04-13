#include "clem_disk.h"

unsigned clem_disk_calculate_nib_storage_size(unsigned disk_type) {
    unsigned size = 0;
    switch (disk_type) {
    case CLEM_DISK_TYPE_5_25:
        size = CLEM_DISK_525_MAX_DATA_SIZE;
        break;
    case CLEM_DISK_TYPE_3_5:
        size = CLEM_DISK_35_MAX_DATA_SIZE;
        break;
    default:
        break;
    }
    return size;
}
