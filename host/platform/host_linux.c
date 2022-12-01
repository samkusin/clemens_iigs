#include "clem_host_platform.h"

#include <X11/XKBlib.h>
#include <X11/keysym.h>

#include <assert.h>

#define _GNU_SOURCE
#include <dirent.h>
#include <fcntl.h>
#include <linux/input.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>
#include <uuid/uuid.h>

//  seems to be a reliable way to call getcpu() regardless of glibc/distribution
static inline unsigned local_getcpu() {
#ifdef SYS_getcpu
    int cpu, status;
    status = syscall(SYS_getcpu, &cpu, NULL, NULL);
    return (status == -1) ? status : cpu;
#else
    return UINT32_MAX; // unavailable
#endif
}

unsigned clem_host_get_processor_number() { return local_getcpu(); }

void clem_host_uuid_gen(ClemensHostUUID *uuid) {
    assert(sizeof(uuid_t) <= sizeof(uuid->data));
    uuid_generate(uuid->data);
}

//  evdev implementation
//  using https://fossies.org/linux/stella/src/tools/evdev-joystick/evdev-joystick.c
//  as an education of evdev and joystick input.
//
#define CLEM_HOST_EVDEV_DIR    "/dev/input/"
#define CLEM_HOST_EVDEV_PREFIX "event"

//  this is overkill but defined to simplify lookup of input axis information
//  during polls (versus remapping input codes to our own codes)
#define CLEM_HOST_EVDEV_AXIS_LIMIT 32

struct ClemensEvdevAxis {
    int min_value;
    int max_value;
    int deadzone;
};

//  support only X and Y axis - for gamepads or devices with two sticks, RX, RY
//  will map to those
struct ClemensHostJoystickInfo {
    int device_id;
    char name[NAME_MAX];
    int fd;
    unsigned avail_axis;
    struct ClemensEvdevAxis axis_info[CLEM_HOST_EVDEV_AXIS_LIMIT];
    bool connected;
};

static struct ClemensHostJoystickInfo s_joysticks[CLEM_HOST_JOYSTICK_LIMIT];

static unsigned s_axis_types[] = {ABS_X, ABS_Y, ABS_RX, ABS_RY, UINT32_MAX};

static int _clem_joystick_evdev_assign_device(int device_index) {
    //  device must have at least one axis (i.e. paddle style)
    //  assumption that the device_id starts with CLEM_HOST_EVDEV_PREFIX since
    //  this should only be called with that assumption in mind (see
    //  _clem_joystick_evdev_enum_devices)
    //
    char path[NAME_MAX + 32];
    unsigned avail_index, axis_index, axis_count, avail_axis;
    struct input_absinfo axis_info;
    int fd, i;
    bool has_deadzones = false;

    for (avail_index = 0; avail_index < CLEM_HOST_JOYSTICK_LIMIT; ++avail_index) {
        if (!s_joysticks[avail_index].connected)
            break;
    }
    if (avail_index >= CLEM_HOST_JOYSTICK_LIMIT) {
        printf("host_linux: no available joystick slots\n");
        return -1;
    }

    snprintf(path, sizeof(path), CLEM_HOST_EVDEV_DIR "event%u", device_index);
    fd = open(path, O_RDWR | O_NONBLOCK);
    if (fd == -1) {
        printf("host_linux: could not open device at %s\n", path);
        return -1;
    }

    ioctl(fd, EVIOCGNAME(sizeof(s_joysticks[avail_index].name)), s_joysticks[avail_index].name);

    axis_index = 0;
    axis_count = 0;
    avail_axis = 0;
    while (s_axis_types[axis_index] != UINT32_MAX) {
        unsigned axis_type = s_axis_types[axis_index];
        if (ioctl(fd, EVIOCGABS(axis_type), &axis_info) != -1) {
            avail_axis |= (1 << axis_type);
            s_joysticks[avail_index].axis_info[axis_type].min_value = axis_info.minimum;
            s_joysticks[avail_index].axis_info[axis_type].max_value = axis_info.maximum;
            s_joysticks[avail_index].axis_info[axis_type].deadzone = axis_info.flat;
            has_deadzones = has_deadzones || axis_info.flat > 0;
            ++axis_count;
        }
        ++axis_index;
    }

    /*
    if (ioctl(fd, EVIOCGKEY(sizeof(result)), &result) == -1) {
        printf("Has no key\n");
    } else {
        printf("Has key (%d)\n", result);
    }
    if (ioctl(fd, EVIOCGBIT(BTN_JOYSTICK, sizeof(result)), &result) == -1) {
        printf("Has no button 0\n");
    } else {
        printf("Has button 0 (%d)\n", result);
    }
    */
    if (axis_count > 0 && has_deadzones) {
        printf("host_linux: evdev joystick %d: %s detected.\n", avail_index,
               s_joysticks[avail_index].name);
        for (i = 0; i < CLEM_HOST_EVDEV_AXIS_LIMIT; ++i) {
            if (avail_axis & (1 << i)) {
                printf("            axis %u: min: %d, max: %d, deadzone: %d\n", i,
                       s_joysticks[avail_index].axis_info[i].min_value,
                       s_joysticks[avail_index].axis_info[i].max_value,
                       s_joysticks[avail_index].axis_info[i].deadzone);
            }
        }
        printf("\n");
    } else {
        if (axis_count > 0) {
            printf("host_linux: evdev device %d: %s\n", avail_index, s_joysticks[avail_index].name);
            printf("            Has absolute axis values but no deadzone.\n"
                   "            Assumption is this is not a real joystick, ignoring.\n");
            printf("\n");
        }
        //  not a joystick (most likely since joysticks are devices that return absolute
        //  axis values versus a mouse)
        close(fd);
        return -1;
    }

    s_joysticks[avail_index].avail_axis = avail_axis;
    s_joysticks[avail_index].connected = true;
    s_joysticks[avail_index].fd = fd;
    s_joysticks[avail_index].device_id = device_index;

    return avail_index;
}

static void _clem_joystick_evdev_clear_devices() {
    unsigned i;
    for (i = 0; i < CLEM_HOST_JOYSTICK_LIMIT; ++i) {
        s_joysticks[i].device_id = -1;
        if (s_joysticks[i].fd >= 0) {
            close(s_joysticks[i].fd);
        }
        s_joysticks[i].connected = false;
        s_joysticks[i].fd = -1;
    }
}

//  This function may be called twice via recursion if the user requested a
//  specific device and the device enumeration couldn't find that device.
//  If so, then this function is called again to enumerate all valid devices
//
void _clem_joystick_evdev_enum_devices() {
    DIR *dir;
    struct dirent *entry;
    size_t prefix_len = strlen(CLEM_HOST_EVDEV_PREFIX);
    size_t device_id_len;
    size_t suffix_offset;
    unsigned found_device_count = 0;

    //  somewhat cribbed from evdev-joystick mostly for reference/instruction
    //  since enumeration 'by-id' is usually available to root, we'll need to
    //  enumerate all devices in /dev/input/ that are named 'event*'
    dir = opendir(CLEM_HOST_EVDEV_DIR);
    if (!dir) {
        printf("host_linux: could not enumerate " CLEM_HOST_EVDEV_DIR "\n");
        _clem_joystick_evdev_clear_devices();
        return;
    }

    while ((entry = readdir(dir)) != NULL) {
        int device_index;
        if (found_device_count == CLEM_HOST_JOYSTICK_LIMIT)
            break;
        device_id_len = strnlen(entry->d_name, 255);
        if (device_id_len <= prefix_len)
            continue;
        //  actual device id is the prefix of the filtered filename
        if (strncmp(entry->d_name, CLEM_HOST_EVDEV_PREFIX, prefix_len) != 0)
            continue;
        suffix_offset = prefix_len;
        device_index = (int)strtol(entry->d_name + suffix_offset, NULL, 10);
        if (_clem_joystick_evdev_assign_device(device_index) >= 0) {
            ++found_device_count;
        }
    }
    closedir(dir);
}

void clem_joystick_open_devices(const char *provider) {
    (void)provider;
    printf("host_linux: enumerating joystick devices with evdev\n");
    _clem_joystick_evdev_enum_devices();
}

unsigned clem_joystick_poll(ClemensHostJoystick *joysticks) { return 0; }

void clem_joystick_close_devices() {}
