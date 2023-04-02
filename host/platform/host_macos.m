#include "clem_host_platform.h"

#include <assert.h>
#include <mach-o/dyld.h>
#include <uuid/uuid.h>

#import <AppKit/NSWorkspace.h>
#import <Foundation/Foundation.h>
#import <GameController/GameController.h>

//  Objective-C note - These functions are called from the sokol application
//  framework
//                     and otherwise are wrapped already in an autoreleasepool
//                     there. It is the caller's responsibility to ensure that
//                     in other situations, these calls are wrapped in
//                     @autoreleasepool { }

unsigned clem_host_get_processor_number() {
  //  There seems to be no easy way to do this on macOS and especially since the
  //  introduction of Apple silicon.
  //  Since this is only for debugging/diagnostic purposes, it's not essential
  //  to implement this function.
  return 0;
}

void clem_host_uuid_gen(ClemensHostUUID *uuid) {
  assert(sizeof(uuid_t) <= sizeof(uuid->data));
  uuid_generate(uuid->data);
}

char *get_process_executable_path(char *outpath, size_t *outpath_size) {
  uint32_t actual_size = (uint32_t)(*outpath_size & 0xffffffff);
  if (_NSGetExecutablePath(outpath, &actual_size) < 0) {
    *outpath_size = actual_size;
    return NULL;
  }
  return outpath;
}

char *get_local_user_data_directory(char *outpath, size_t outpath_size,
                                    const char *company_name,
                                    const char *app_name) {
  NSArray *paths = NSSearchPathForDirectoriesInDomains(
      NSApplicationSupportDirectory, NSUserDomainMask, YES);
  NSString *application_support_dir = [paths firstObject];
  NSLog(@"User data directory: %@", application_support_dir);

  const char *app_dir_cstr = [application_support_dir UTF8String];
  snprintf(outpath, outpath_size - 1, "%s/%s/%s", app_dir_cstr, company_name,
           app_name);
  outpath[outpath_size - 1] = '\0';
  return outpath;
}

void open_system_folder_view(const char *folder_path) {
  NSString *folder_path_string =
      [[NSString alloc] initWithCString:folder_path
                               encoding:NSUTF8StringEncoding];
  NSURL *folder_url = [NSURL fileURLWithPath:folder_path_string];
  [[NSWorkspace sharedWorkspace] openURL:folder_url];
}

///////////////////////////////////////////////////////////////////////////////

void clem_host_platform_init() {}

void clem_host_platform_terminate() {}

@interface ClemensGameControllerDispatch : NSObject {
  NSPointerArray *controllers;
}

@property(readonly) unsigned controllerCount;

- (BOOL)queryInputsAtIndex:(NSUInteger)atIndex
              withJoystick:(ClemensHostJoystick *)inputs;

@end

@implementation ClemensGameControllerDispatch

- (unsigned)controllerCount {
  return controllers.count;
}

- (instancetype)init {
  self = [super init];
  controllers = [[NSPointerArray alloc] init];
  return self;
}

- (void)onControllerConnected:(NSNotification *)note {
  GCController *controller = note.object;
  unsigned index;
  for (index = 0; index < controllers.count; ++index) {
    if ([controllers pointerAtIndex:index] == nil) {
      [controllers replacePointerAtIndex:index withPointer:controller];
      break;
    }
  }
  if (index >= controllers.count) {
    [controllers addPointer:controller];
  }
  GCExtendedGamepad *gamepad = [controller extendedGamepad];
  GCMicroGamepad *micro = [controller microGamepad];
  if (gamepad != nil && micro != nil) {
    NSLog(@"[%u] ATTACHED: %s and has extendedGamepad=%s, microGamepad=%s",
          index, controller.vendorName.UTF8String,
          gamepad != nil ? "YES" : "NO", micro != nil ? "YES" : "NO");
  } else {
    NSLog(@"[%u] ATTACHED: %s with unknown profile", index,
          controller.vendorName.UTF8String);
  }
}

- (void)onControllerDisconnected:(NSNotification *)note {
  GCController *controller = note.object;
  unsigned index;
  for (index = 0; index < controllers.count; ++index) {
    if ([controllers pointerAtIndex:index] == controller) {
      NSLog(@"[%u] DISCONNECTED: %s", index, controller.vendorName.UTF8String);
      [controllers replacePointerAtIndex:index withPointer:nil];
      break;
    }
  }
}

- (BOOL)queryInputsAtIndex:(NSUInteger)index
              withJoystick:(ClemensHostJoystick *)inputs {
  if (index >= controllers.count)
    return NO;
  GCController *controller = [controllers pointerAtIndex:index];
  inputs->isConnected = false;

  if (controller == nil) {
    return NO;
  }
  //  Apple Joysticks are at most two axis and two button - but check for all
  //  supported apple gamepad types by complexity order (extended first, then
  //  fallback to microGamepad) gamepad profile.
  GCExtendedGamepad *gamepad = [controller extendedGamepad];
  GCMicroGamepad *micro = [controller microGamepad];
  if (gamepad == nil && micro == nil)
    return NO;

  inputs->isConnected = true;
  inputs->buttons = 0;
  if (gamepad) {
    inputs->x[0] =
        gamepad.leftThumbstick.xAxis.value * CLEM_HOST_JOYSTICK_AXIS_DELTA;
    inputs->x[1] =
        gamepad.rightThumbstick.xAxis.value * CLEM_HOST_JOYSTICK_AXIS_DELTA;
    inputs->y[0] =
        -gamepad.leftThumbstick.yAxis.value * CLEM_HOST_JOYSTICK_AXIS_DELTA;
    inputs->y[1] =
        -gamepad.rightThumbstick.yAxis.value * CLEM_HOST_JOYSTICK_AXIS_DELTA;
    ;
    if (gamepad.buttonA.pressed) {
      inputs->buttons |= 0x1;
    }
    if (gamepad.buttonX.pressed) {
      inputs->buttons |= 0x2;
    }
    if (gamepad.buttonB.pressed) {
      inputs->buttons |= 0x4;
    }
    if (gamepad.buttonY.pressed) {
      inputs->buttons |= 0x8;
    }
  } else if (micro) {
    inputs->x[0] = micro.dpad.xAxis.value * CLEM_HOST_JOYSTICK_AXIS_DELTA;
    inputs->x[1] = 0;
    inputs->y[0] = -micro.dpad.yAxis.value * CLEM_HOST_JOYSTICK_AXIS_DELTA;
    inputs->y[1] = 0;
    if (micro.buttonA.pressed) {
      inputs->buttons |= 0x1;
    }
    if (micro.buttonX.pressed) {
      inputs->buttons |= 0x2;
    }
  }
  return YES;
}
@end

static ClemensGameControllerDispatch *controllerDispatch_ = nil;

static void _open_gamecontrollers() {
  controllerDispatch_ = [[ClemensGameControllerDispatch alloc] init];
  NSNotificationCenter *notificationCenter =
      [NSNotificationCenter defaultCenter];
  [notificationCenter addObserver:controllerDispatch_
                         selector:@selector(onControllerConnected:)
                             name:GCControllerDidConnectNotification
                           object:nil];
  [notificationCenter addObserver:controllerDispatch_
                         selector:@selector(onControllerDisconnected:)
                             name:GCControllerDidDisconnectNotification
                           object:nil];
}

void clem_joystick_open_devices(const char *provider) {
  clem_joystick_close_devices();
  if (strncmp(provider, CLEM_HOST_JOYSTICK_PROVIDER_GAMECONTROLLER, 32) == 0) {
    _open_gamecontrollers();
  }
}

unsigned clem_joystick_poll(ClemensHostJoystick *joysticks) {
  unsigned index = 0;
  if (controllerDispatch_) {
    for (; index < controllerDispatch_.controllerCount &&
           index < CLEM_HOST_JOYSTICK_LIMIT;
         ++index) {
      if ([controllerDispatch_ queryInputsAtIndex:index
                                     withJoystick:&joysticks[index]] == NO) {
        joysticks[index].isConnected = false;
      }
    }
  }
  return index;
}

void clem_joystick_close_devices() {
  if (controllerDispatch_ != nil) {
    NSNotificationCenter *notificationCenter =
        [NSNotificationCenter defaultCenter];
    [notificationCenter removeObserver:controllerDispatch_
                                  name:GCControllerDidDisconnectNotification
                                object:nil];
    [notificationCenter removeObserver:controllerDispatch_
                                  name:GCControllerDidConnectNotification
                                object:nil];
    controllerDispatch_ = nil;
  }
}
