#if defined(CK3D_BACKEND_METAL)
#define SOKOL_METAL
#elif defined(CK3D_BACKEND_GL)
#define SOKOL_GLCORE33
#endif

#include "imgui.h"

#define SOKOL_IMPL
#include "sokol/sokol_app.h"
#include "sokol/sokol_audio.h"
#include "sokol/sokol_gfx.h"
#include "sokol/sokol_glue.h"
#include "sokol/sokol_imgui.h"
#include "sokol/sokol_time.h"

#include "clem_host.hpp"

#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>

@interface ClemensHostSystem : NSObject
@end

@implementation ClemensHostSystem {
  struct ClemensHostInterop *hostInterop;
}

+ (id)instance {
  static ClemensHostSystem *system = nil;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    system = [[self alloc] init];
  });
  return system;
}

- (void)setInterop:(ClemensHostInterop *)interop {
  self->hostInterop = interop;
}

- (void)menuQuit:(id)sender {
  hostInterop->exitApp = true;
}

- (void)menuAbout:(id)sender {
  // TODO!
}

//  TODO: localized strings!
- (void)buildMenus {
  // Application Menu
  NSMenu *appmenu = [NSMenu new];
  NSMenuItem *aboutItem =
      [[NSMenuItem alloc] initWithTitle:@"About Clemens IIGS"
                                 action:@selector(menuAbout:)
                          keyEquivalent:@""];
  [aboutItem setTarget:self];
  NSMenuItem *quitItem = [[NSMenuItem alloc] initWithTitle:@"Quit"
                                                    action:@selector(menuQuit:)
                                             keyEquivalent:@""];
  [quitItem setTarget:self];
  [appmenu addItem:aboutItem];
  [appmenu addItem:[NSMenuItem separatorItem]];
  [appmenu addItem:quitItem];

  // File Menu

  // Edit Menu

  // View Menu
  //    Pause or Run
  //    Debugger or Main Display
  //    Enter/Exit Fullscreen 

  // Help Menu

  // Main Menu
  NSMenuItem *menubarItem = [NSMenuItem new];
  [menubarItem setSubmenu:appmenu];

  // Setup
  NSMenu *menubar = [NSMenu new];
  [menubar addItem:menubarItem];
  [NSApp setMainMenu:menubar];
}

@end

void clemens_host_init(ClemensHostInterop *interop) {
  //  create a menu and attach to the current application
  [[ClemensHostSystem instance] setInterop:interop];
  [[ClemensHostSystem instance] buildMenus];
}

void clemens_host_terminate() {}
