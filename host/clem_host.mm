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
#include "sokol/sokol_gfx_ext.h"
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
  NSMenuItem *powerMenuItem;
  NSMenuItem *rebootMenuItem;
  NSMenuItem *shutdownMenuItem;
  NSMenuItem *fullscreenMenuItem;
  NSMenuItem *debuggerMenuItem;
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
  hostInterop->action = ClemensHostInterop::About;
}

- (void)menuPasteText:(id)sender {
  hostInterop->action = ClemensHostInterop::PasteFromClipboard;
}

- (void)menuLoadSnapshot:(id)sender {
  hostInterop->action = ClemensHostInterop::LoadSnapshot;
}

- (void)menuSaveSnapshot:(id)sender {
  hostInterop->action = ClemensHostInterop::SaveSnapshot;
}

- (void)menuPower:(id)sender {
  hostInterop->action = ClemensHostInterop::Power;
}

- (void)menuReboot:(id)sender {
  hostInterop->action = ClemensHostInterop::Reboot;
}

- (void)menuShutdown:(id)sender {
  hostInterop->action = ClemensHostInterop::Shutdown;
}

- (void)menuShortcutsHelp:(id)sender {
  hostInterop->action = ClemensHostInterop::Help;
}

- (void)menuDiskHelp:(id)sender {
  hostInterop->action = ClemensHostInterop::DiskHelp;
}

- (void)menuFullscreenToggle:(id)sender {
  if (hostInterop->view == ClemensHostInterop::Windowed) {
    hostInterop->view = ClemensHostInterop::Fullscreen;
  } else {
    hostInterop->view = ClemensHostInterop::Windowed;
  }
}

- (void)menuDebugger:(id)sender {
  if (hostInterop->debuggerOn) {
    hostInterop->action = ClemensHostInterop::Standard;
  } else {
    hostInterop->action = ClemensHostInterop::Debugger;
  }
}

//  TODO: localized strings!
- (void)buildMenus {
  NSMenu *menubar = [NSMenu new];

  // Application Menu
  NSMenu *appmenu = [NSMenu new];
  NSMenuItem *menuItem = [[NSMenuItem alloc] initWithTitle:@"About Clemens IIGS"
                                                    action:@selector(menuAbout:)
                                             keyEquivalent:@""];
  [menuItem setTarget:self];
  [appmenu addItem:menuItem];

  [appmenu addItem:[NSMenuItem separatorItem]];

  menuItem = [[NSMenuItem alloc] initWithTitle:@"Quit"
                                        action:@selector(menuQuit:)
                                 keyEquivalent:@""];
  [menuItem setTarget:self];
  [appmenu addItem:menuItem];

  NSMenuItem *appMenubarItem = [NSMenuItem new];
  [appMenubarItem setSubmenu:appmenu];
  [menubar addItem:appMenubarItem];

  // File Menu
  NSMenu *filemenu = [[NSMenu alloc] initWithTitle:@"File"];
  menuItem = [[NSMenuItem alloc] initWithTitle:@"Load Snapshot"
                                        action:@selector(menuLoadSnapshot:)
                                 keyEquivalent:@""];
  [menuItem setTarget:self];
  [filemenu addItem:menuItem];
  menuItem = [[NSMenuItem alloc] initWithTitle:@"Save Snapshot"
                                        action:@selector(menuSaveSnapshot:)
                                 keyEquivalent:@""];
  [menuItem setTarget:self];
  [filemenu addItem:menuItem];

  NSMenuItem *fileMenuItem = [menubar addItemWithTitle:@""
                                                action:nil
                                         keyEquivalent:@""];
  [menubar setSubmenu:filemenu forItem:fileMenuItem];

  // Edit Menu
  NSMenu *editmenu = [[NSMenu alloc] initWithTitle:@"Edit"];
  menuItem = [[NSMenuItem alloc] initWithTitle:@"Paste Text Input"
                                        action:@selector(menuPasteText:)
                                 keyEquivalent:@""];
  [menuItem setTarget:self];
  [editmenu addItem:menuItem];

  NSMenuItem *editMenuItem = [menubar addItemWithTitle:@""
                                                action:nil
                                         keyEquivalent:@""];
  [menubar setSubmenu:editmenu forItem:editMenuItem];

  // View Menu
  NSMenu *viewmenu = [[NSMenu alloc] initWithTitle:@"View"];
  fullscreenMenuItem =
      [[NSMenuItem alloc] initWithTitle:@"Enter Fullscreen"
                                 action:@selector(menuFullscreenToggle:)
                          keyEquivalent:@""];
  [fullscreenMenuItem setTarget:self];
  [viewmenu addItem:fullscreenMenuItem];

  debuggerMenuItem = [[NSMenuItem alloc] initWithTitle:@"Debugger View"
                                                action:@selector(menuDebugger:)
                                         keyEquivalent:@""];
  [debuggerMenuItem setTarget:self];
  [viewmenu addItem:debuggerMenuItem];

  NSMenuItem *viewMenuItem = [menubar addItemWithTitle:@""
                                                action:nil
                                         keyEquivalent:@""];
  [menubar setSubmenu:viewmenu forItem:viewMenuItem];

  // Machine Menu
  NSMenu *machmenu = [[NSMenu alloc] initWithTitle:@"Machine"];
  powerMenuItem = [[NSMenuItem alloc] initWithTitle:@"Power"
                                             action:@selector(menuPower:)
                                      keyEquivalent:@""];
  [powerMenuItem setTarget:self];
  [machmenu addItem:powerMenuItem];

  rebootMenuItem = [[NSMenuItem alloc] initWithTitle:@"Reboot"
                                              action:@selector(menuReboot:)
                                       keyEquivalent:@""];
  [rebootMenuItem setTarget:self];
  [machmenu addItem:rebootMenuItem];

  shutdownMenuItem = [[NSMenuItem alloc] initWithTitle:@"Shutdown"
                                                action:@selector(menuShutdown:)
                                         keyEquivalent:@""];
  [shutdownMenuItem setTarget:self];
  [machmenu addItem:shutdownMenuItem];

  NSMenuItem *machMenuItem = [menubar addItemWithTitle:@""
                                                action:nil
                                         keyEquivalent:@""];
  [menubar setSubmenu:machmenu forItem:machMenuItem];

  // Help Menu
  NSMenu *helpmenu = [[NSMenu alloc] initWithTitle:@"Help"];
  menuItem = [[NSMenuItem alloc] initWithTitle:@"Keyboard Shortcuts"
                                        action:@selector(menuShortcutsHelp:)
                                 keyEquivalent:@""];
  [menuItem setTarget:self];
  [helpmenu addItem:menuItem];

  menuItem = [[NSMenuItem alloc] initWithTitle:@"Disk Selection"
                                        action:@selector(menuDiskHelp:)
                                 keyEquivalent:@""];
  [menuItem setTarget:self];
  [helpmenu addItem:menuItem];

  NSMenuItem *helpMenuItem = [menubar addItemWithTitle:@""
                                                action:nil
                                         keyEquivalent:@""];
  [menubar setSubmenu:helpmenu forItem:helpMenuItem];

  // View Menu
  //    Pause or Run
  //    Debugger or Main Display
  //    Enter/Exit Fullscreen

  // Help Menu

  // Setup
  // Main Menu
  [NSApp setMainMenu:menubar];
}

- (void)update {
  if (hostInterop->poweredOn) {
    [rebootMenuItem setEnabled:TRUE];
    [rebootMenuItem setAction:@selector(menuReboot:)];
    [shutdownMenuItem setEnabled:TRUE];
    [shutdownMenuItem setAction:@selector(menuShutdown:)];
    [powerMenuItem setEnabled:FALSE];
    [powerMenuItem setAction:nil];
  } else {
    [rebootMenuItem setEnabled:FALSE];
    [rebootMenuItem setAction:nil];
    [shutdownMenuItem setEnabled:FALSE];
    [shutdownMenuItem setAction:nil];
    [powerMenuItem setEnabled:TRUE];
    [powerMenuItem setAction:@selector(menuPower:)];
  }
  if (hostInterop->view != ClemensHostInterop::Fullscreen) {
    [fullscreenMenuItem setTitle:@"Enter Fullscreen"];
  } else {
    [fullscreenMenuItem setTitle:@"Exit Fullscreen"];
  }
  if (hostInterop->debuggerOn) {
    [debuggerMenuItem setTitle:@"User Mode"];
  } else {
    [debuggerMenuItem setTitle:@"Debugger Mode"];
  }
}

@end

void clemens_host_init(ClemensHostInterop *interop) {
  //  create a menu and attach to the current application
  [[ClemensHostSystem instance] setInterop:interop];
  [[ClemensHostSystem instance] buildMenus];
  interop->nativeMenu = true;
}

void clemens_host_update() { [[ClemensHostSystem instance] update]; }

void clemens_host_terminate() {}
