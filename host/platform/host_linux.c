#include "clem_host_platform.h"

#include <X11/XKBlib.h>
#include <X11/keysym.h>

#include <assert.h>

#define _GNU_SOURCE
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <pwd.h>
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
static inline unsigned local_getcpu(void) {
#ifdef SYS_getcpu
    int cpu, status;
    status = syscall(SYS_getcpu, &cpu, NULL, NULL);
    return (status == -1) ? status : cpu;
#else
    return UINT32_MAX; // unavailable
#endif
}

unsigned clem_host_get_processor_number(void) { return local_getcpu(); }

void clem_host_uuid_gen(ClemensHostUUID *uuid) {
    assert(sizeof(uuid_t) <= sizeof(uuid->data));
    uuid_generate(uuid->data);
}

char *get_process_executable_path(char *outpath, size_t *outpath_size) {
    //   TODO: /proc/self/exe
    struct stat file_stat;
    ssize_t bufsz;

    bufsz = readlink("/proc/self/exe", outpath, *outpath_size - 1);
    if (bufsz < 0)
        return NULL;
    outpath[bufsz] = '\0';

    printf("get_process_exectuable_path = %s\n", outpath);

    //  verify that the result is actually a file (to ensure that our output path
    //  is a complete link vs clipped by the buffer size)
    if (stat(outpath, &file_stat) < 0) {
        return NULL;
    }
    if (!S_ISREG(file_stat.st_mode)) {
        return NULL;
    }
    while (--bufsz >= 0) {
        if (outpath[bufsz] == '/') {
            outpath[bufsz] = '\0';
            break;
        }
    }
    return outpath;
}

char *get_local_user_data_directory(char *outpath, size_t outpath_size, const char *company_name,
                                    const char *app_name) {
    //  generate ~/.var/com.<company_name>.<app_name>/data
    const char *user_home_dir = getenv("HOME");
    if (user_home_dir == NULL) {
        user_home_dir = getpwuid(getuid())->pw_dir;
    }
    snprintf(outpath, outpath_size - 1, "%s/.var/com.%s.%s/data", user_home_dir, company_name,
             app_name);
    outpath[outpath_size - 1] = '\0';
    return outpath;
}

void open_system_folder_view(const char *folder_path) { (void)folder_path; }

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
    int value;
};

//  support only X and Y axis - for gamepads or devices with two sticks, RX, RY
//  will map to those
struct ClemensHostJoystickInfo {
    int device_id;
    char name[NAME_MAX];
    int fd;
    unsigned avail_axis;
    struct ClemensEvdevAxis axis_info[CLEM_HOST_EVDEV_AXIS_LIMIT];
    unsigned buttons;
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
            s_joysticks[avail_index].axis_info[axis_type].value = 0;
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
        printf("host_linux: evdev joystick %d: %s detected.\n", device_index,
               s_joysticks[avail_index].name);
        for (i = 0; i < CLEM_HOST_EVDEV_AXIS_LIMIT; ++i) {
            if (avail_axis & (1 << i)) {
                printf("            axis %u: min: %d, max: %d, deadzone: %d\n", i,
                       s_joysticks[avail_index].axis_info[i].min_value,
                       s_joysticks[avail_index].axis_info[i].max_value,
                       s_joysticks[avail_index].axis_info[i].deadzone);
            }
        }
        printf("host_linux: assigning device slot %d\n", device_index);
        printf("\n");
    } else {
        if (axis_count > 0) {
            printf("host_linux: evdev device %d: %s\n", device_index,
                   s_joysticks[avail_index].name);
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
    s_joysticks[avail_index].buttons = 0;
    s_joysticks[avail_index].connected = true;
    s_joysticks[avail_index].fd = fd;
    s_joysticks[avail_index].device_id = device_index;

    return avail_index;
}

static void _clem_joystick_evdev_close(struct ClemensHostJoystickInfo *device) {
    if (device->device_id < 0)
        return;

    if (device->fd >= 0) {
        close(device->fd);
    }
    device->connected = false;
    device->fd = -1;
}

static int _clem_joystick_evdev_normalize_value(int value, struct ClemensEvdevAxis *axis) {
    float scalar = 2.0f * (value - axis->min_value) / (axis->max_value - axis->min_value) - 1.0f;
    return (int)(scalar * CLEM_HOST_JOYSTICK_AXIS_DELTA);
}

static void _clem_joystick_evdev_clear_devices(void) {
    unsigned i;
    for (i = 0; i < CLEM_HOST_JOYSTICK_LIMIT; ++i) {
        _clem_joystick_evdev_close(&s_joysticks[i]);
        s_joysticks[i].device_id = -1;
    }
}

//  This function may be called twice via recursion if the user requested a
//  specific device and the device enumeration couldn't find that device.
//  If so, then this function is called again to enumerate all valid devices
//
void _clem_joystick_evdev_enum_devices(void) {
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

bool _clem_joystick_poll_one(ClemensHostJoystick *joystick,
                             struct ClemensHostJoystickInfo *device) {
    struct input_event event;
    int result;
    unsigned short ev_code_base;

    if (device->connected) {

        while ((result = read(device->fd, &event, sizeof(event))) > 0) {
            switch (event.type) {
            case EV_ABS:
                if (event.code < ABS_TOOL_WIDTH) {
                    if (event.code == ABS_X) {
                        device->axis_info[ABS_X].value = _clem_joystick_evdev_normalize_value(
                            event.value, &device->axis_info[ABS_X]);
                    } else if (event.code == ABS_Y) {
                        device->axis_info[ABS_Y].value = _clem_joystick_evdev_normalize_value(
                            event.value, &device->axis_info[ABS_Y]);
                    } else if (event.code == ABS_RX) {
                        device->axis_info[ABS_RX].value = _clem_joystick_evdev_normalize_value(
                            event.value, &device->axis_info[ABS_RX]);
                    } else if (event.code == ABS_RY) {
                        device->axis_info[ABS_RY].value = _clem_joystick_evdev_normalize_value(
                            event.value, &device->axis_info[ABS_RY]);
                    }
                }
                break;
            case EV_KEY:
                if (event.code >= BTN_JOYSTICK && event.code <= BTN_THUMBR) {
                    if (event.code >= BTN_GAMEPAD) {
                        ev_code_base = BTN_GAMEPAD;
                    } else {
                        ev_code_base = BTN_JOYSTICK;
                    }
                    if (event.value) {
                        device->buttons |= (1 << (event.code - ev_code_base));
                    } else {
                        device->buttons &= ~(1 << (event.code - ev_code_base));
                    }
                }
                break;
            }
        }
        if (result == 0 || errno == EAGAIN) {
            joystick->buttons = device->buttons;
            joystick->x[0] = (int16_t)device->axis_info[ABS_X].value;
            joystick->y[0] = (int16_t)device->axis_info[ABS_Y].value;
            joystick->x[1] = (int16_t)device->axis_info[ABS_RX].value;
            joystick->y[1] = (int16_t)device->axis_info[ABS_RY].value;
        } else if (result < 0) {
            printf("host_linux: DISCONNECTED %d - evice failed with error %d ", device->device_id,
                   errno);
            _clem_joystick_evdev_close(device);
        }
    } else {
        //  TODO: retry connection by opening the device again
    }
    return device->connected;
}

void clem_joystick_open_devices(const char *provider) {
    (void)provider;
    printf("host_linux: enumerating joystick devices with evdev\n");
    _clem_joystick_evdev_clear_devices();
    _clem_joystick_evdev_enum_devices();
}

unsigned clem_joystick_poll(ClemensHostJoystick *joysticks) {
    unsigned device_index;

    for (device_index = 0; device_index < CLEM_HOST_JOYSTICK_LIMIT; ++device_index) {
        struct ClemensHostJoystickInfo *joystick_info = &s_joysticks[device_index];
        if (joystick_info->device_id == -1) {
            joysticks[device_index].isConnected = false;
            continue;
        }
        if (joystick_info->connected) {
            joysticks[device_index].isConnected =
                _clem_joystick_poll_one(&joysticks[device_index], joystick_info);
        } else {
            joysticks[device_index].isConnected = false;
        }
    }

    return CLEM_HOST_JOYSTICK_LIMIT;
}

void clem_joystick_close_devices(void) { _clem_joystick_evdev_clear_devices(); }
