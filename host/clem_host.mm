
#include "clem_host.hpp"

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

#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>

@interface ClemensHostSystem : NSObject
@end

@implementation ClemensHostSystem {
  struct ClemensHostInterop *hostInterop;
  NSMenuItem *powerMenuItem;
  NSMenuItem *rebootMenuItem;
  NSMenuItem *shutdownMenuItem;
  NSMenuItem *debuggerMenuItem;
  NSMenuItem *joysickMenuItem;
  NSMenuItem *lockMouseItem;
  NSMenuItem *pauseItem;
  NSMenuItem *fastMode;
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

- (void)menuConfigureJoystick:(id)sender {
  hostInterop->action = ClemensHostInterop::JoystickConfig;
}

- (void)menuShortcutsHelp:(id)sender {
  hostInterop->action = ClemensHostInterop::Help;
}

- (void)menuDiskHelp:(id)sender {
  hostInterop->action = ClemensHostInterop::DiskHelp;
}

- (void)menuDebugger:(id)sender {
  if (hostInterop->debuggerOn) {
    hostInterop->action = ClemensHostInterop::Standard;
  } else {
    hostInterop->action = ClemensHostInterop::Debugger;
  }
}

- (void)menuLockMouse:(id)sender {
  if (hostInterop->mouseLock) {
    hostInterop->action = ClemensHostInterop::MouseUnlock;
  } else {
    hostInterop->action = ClemensHostInterop::MouseLock;
  }
}

- (void)menuPauseEmulator:(id)sender {
  if (!hostInterop->poweredOn)
    return;
  if (hostInterop->running) {
    hostInterop->action = ClemensHostInterop::PauseExecution;
  } else {
    hostInterop->action = ClemensHostInterop::ContinueExecution;
  }
}

- (void)menuFastMode:(id)sender {
  if (hostInterop->fastMode) {
    hostInterop->action = ClemensHostInterop::DisableFastMode;
  } else {
    hostInterop->action = ClemensHostInterop::EnableFastMode;
  }
}

//  TODO: localized strings!
- (void)buildMenus {
  unichar keyEquivalentChar = 0;
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

  debuggerMenuItem = [[NSMenuItem alloc] initWithTitle:@"Debugger View"
                                                action:@selector(menuDebugger:)
                                         keyEquivalent:@""];
  [debuggerMenuItem setTarget:self];
  keyEquivalentChar = NSF11FunctionKey;
  [debuggerMenuItem setKeyEquivalentModifierMask:NSEventModifierFlagControl |
                                                 NSEventModifierFlagOption |
                                                 NSEventModifierFlagFunction];
  [debuggerMenuItem
      setKeyEquivalent:[NSString stringWithCharacters:&keyEquivalentChar
                                               length:1]];
  [viewmenu addItem:debuggerMenuItem];

  lockMouseItem = [[NSMenuItem alloc] initWithTitle:@"Lock Mouse"
                                             action:@selector(menuLockMouse:)
                                      keyEquivalent:@""];
  [lockMouseItem setTarget:self];
  keyEquivalentChar = NSF10FunctionKey;
  [lockMouseItem setKeyEquivalentModifierMask:NSEventModifierFlagControl |
                                              NSEventModifierFlagOption |
                                              NSEventModifierFlagFunction];
  [lockMouseItem
      setKeyEquivalent:[NSString stringWithCharacters:&keyEquivalentChar
                                               length:1]];
  [viewmenu addItem:lockMouseItem];

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

  pauseItem = [[NSMenuItem alloc] initWithTitle:@"Pause"
                                         action:@selector(menuPauseEmulator::)
                                  keyEquivalent:@""];
  [pauseItem setTarget:self];
  keyEquivalentChar = NSF5FunctionKey;
  [pauseItem setKeyEquivalentModifierMask:NSEventModifierFlagControl |
                                          NSEventModifierFlagOption |
                                          NSEventModifierFlagFunction];
  [pauseItem setKeyEquivalent:[NSString stringWithCharacters:&keyEquivalentChar
                                                      length:1]];
  [machmenu addItem:pauseItem];

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

  [machmenu addItem:[NSMenuItem separatorItem]];

  fastMode = [[NSMenuItem alloc] initWithTitle:@"Fast Mode"
                                        action:@selector(menuFastMode:)
                                 keyEquivalent:@""];
  [fastMode setTarget:self];
  keyEquivalentChar = NSF8FunctionKey;
  [fastMode setKeyEquivalentModifierMask:NSEventModifierFlagControl |
                                                 NSEventModifierFlagOption |
                                                 NSEventModifierFlagFunction];
  [fastMode
      setKeyEquivalent:[NSString stringWithCharacters:&keyEquivalentChar
                                               length:1]];
  [machmenu addItem:fastMode];

  joysickMenuItem =
      [[NSMenuItem alloc] initWithTitle:@"Configure Joystick"
                                 action:@selector(menuConfigureJoystick:)
                          keyEquivalent:@""];
  [joysickMenuItem setTarget:self];
  [machmenu addItem:joysickMenuItem];

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
    [lockMouseItem setEnabled:TRUE];
    [lockMouseItem setAction:@selector(menuLockMouse:)];
    [pauseItem setEnabled:TRUE];
    [pauseItem setAction:@selector(menuPauseEmulator:)];
    [fastMode setEnabled:TRUE];
    [fastMode setAction:@selector(menuFastMode:)];
    [fastMode setState: hostInterop->fastMode ? NSControlStateValueOn : NSControlStateValueOff];
  } else {
    [rebootMenuItem setEnabled:FALSE];
    [rebootMenuItem setAction:nil];
    [shutdownMenuItem setEnabled:FALSE];
    [shutdownMenuItem setAction:nil];
    [powerMenuItem setEnabled:TRUE];
    [powerMenuItem setAction:@selector(menuPower:)];
    [lockMouseItem setEnabled:FALSE];
    [lockMouseItem setAction:nil];
    [pauseItem setEnabled:FALSE];
    [pauseItem setAction:nil];
    [fastMode setEnabled:FALSE];
    [fastMode setAction:nil];
  }
  if (hostInterop->debuggerOn) {
    [debuggerMenuItem setTitle:@"User Mode"];
  } else {
    [debuggerMenuItem setTitle:@"Debugger Mode"];
  }
  if (hostInterop->mouseLock) {
    [lockMouseItem setTitle:@"Unlock Mouse"];
  } else {
    [lockMouseItem setTitle:@"Lock Mouse"];
  }
  if (hostInterop->running) {
    [pauseItem setTitle:@"Pause"];
  } else {
    [pauseItem setTitle:@"Continue"];
  }
  if (hostInterop->allowConfigureJoystick) {
    [joysickMenuItem setEnabled:TRUE];
    [joysickMenuItem setAction:@selector(menuConfigureJoystick:)];
  } else {
    [joysickMenuItem setEnabled:FALSE];
    [joysickMenuItem setAction:nil];
  }
  NSSize minSize;
  minSize.width = hostInterop->minWindowWidth;
  minSize.height = hostInterop->minWindowHeight;
  NSWindow *mainWindow = [NSApp mainWindow];
  mainWindow.contentMinSize = minSize;
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
