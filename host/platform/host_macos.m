#include "clem_host_platform.h"

#include <assert.h>
#include <mach-o/dyld.h>
#include <uuid/uuid.h>

#import <Foundation/Foundation.h>

//  Objective-C note - These functions are called from the sokol application framework
//                     and otherwise are wrapped already in an autoreleasepool there.
//                     It is the caller's responsibility to ensure that in other 
//                     situations, these calls are wrapped in @autoreleasepool { }

unsigned clem_host_get_processor_number() {
    //  There seems to be no easy way to do this on macOS and especially since the
    //  introduction of Apple silicon.
    //  Since this is only for debugging/diagnostic purposes, it's not essential to
    //  implement this function.
    return 0;
}

void clem_host_uuid_gen(ClemensHostUUID *uuid) {
    assert(sizeof(uuid_t) <= sizeof(uuid->data));
    uuid_generate(uuid->data);
}

char *get_process_executable_path(char *outpath, size_t* outpath_size) {
    uint32_t actual_size = (uint32_t)(*outpath_size & 0xffffffff);
    if (_NSGetExecutablePath(outpath, &actual_size) < 0) {
        *outpath_size = actual_size;
        return NULL;
    }
    return outpath;
}

char *get_local_user_data_directory(char *outpath, size_t outpath_size, const char *company_name,
                                    const char *app_name) {
    NSArray* paths = NSSearchPathForDirectoriesInDomains(NSApplicationSupportDirectory, 
                                                         NSUserDomainMask, YES);
    NSString* application_support_dir = [paths firstObject];
    NSLog(@"User data directory: %@", application_support_dir);

    const char* app_dir_cstr = [application_support_dir UTF8String];
    snprintf(outpath, outpath_size - 1, "%s/%s/%s", app_dir_cstr, company_name, app_name);
    outpath[outpath_size - 1] = '\0';
    return outpath;
}

void clem_joystick_open_devices(const char *provider) {

}

unsigned clem_joystick_poll(ClemensHostJoystick *joysticks) {
    return 0;
}

void clem_joystick_close_devices() {

}
