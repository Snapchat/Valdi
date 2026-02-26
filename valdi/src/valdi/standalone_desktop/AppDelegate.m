//
//  AppDelegate.m
//  MacOSTest
//
//  Created by Simon Corsin on 6/27/20.
//  Copyright © 2020 Snap. All rights reserved.
//

#import "AppDelegate.h"
#import "valdi/macos/SCValdiSnapDrawingNSView.h"
#import "valdi/macos/SCValdiRuntime.h"
#import "NativeModules.h"
#import "valdi/standalone_desktop/ValdiDesktopModuleInit.h"

const NSString *kUseTemporaryCacheDirectoryArgument = @"--use_temporary_caches_directory";

@implementation AppDelegate {
    SCValdiRuntime *_valdiRuntime;
    NSWindow *_window;
}

- (void)applicationDidFinishLaunching:(NSNotification *)notification {
    // Configure whether to use the temporary caches directory.
    NSMutableArray *launchArguments = [[SCValdiRuntime getLaunchArguments] mutableCopy];
    BOOL useTemporaryCachesDirectory = [launchArguments containsObject:kUseTemporaryCacheDirectoryArgument];
    if (useTemporaryCachesDirectory) {
        // Remove the arg from what we pass to Valdi, since it doesn't know about it.
        [launchArguments removeObject:kUseTemporaryCacheDirectoryArgument];
    }
    
    _valdiRuntime = [[SCValdiRuntime alloc] initWithUsingTemporaryCachesDirectory:useTemporaryCachesDirectory];
    SCValdiNativeModulesRegister(_valdiRuntime);
    ValdiRunDesktopModuleInits((__bridge void *)_valdiRuntime);

    [_valdiRuntime waitUntilReadyWithCompletion:^{
        id componentContext = SCValdiGetDesktopComponentContext(_valdiRuntime);
        SCValdiSnapDrawingNSView *rootView = [[SCValdiSnapDrawingNSView alloc] initWithValdiRuntime:_valdiRuntime arguments:launchArguments componentContext:componentContext];
        
        _window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 0, 0) styleMask:NSWindowStyleMaskClosable | NSWindowStyleMaskTitled | NSWindowStyleMaskResizable backing:NSBackingStoreBuffered defer:NO];
        _window.title = @"Valdi Desktop";
        _window.contentView = rootView;
        [_window makeKeyAndOrderFront:nil];
        
        [self _setupMainMenu];

        [NSApp activateIgnoringOtherApps:YES];
    }];
}

- (void)applicationWillTerminate:(NSNotification *)aNotification {
    // Insert code here to tear down your application
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender
{
    return YES;
}

#pragma mark - Private

- (void)_setupMainMenu {
    NSMenu *mainMenu = [[NSMenu alloc] init];
    NSMenuItem *valdiMenuItem = [[NSMenuItem alloc] init];
    [mainMenu addItem:valdiMenuItem];
    NSMenu *valdiMenu = [[NSMenu alloc] init];
    [mainMenu setSubmenu:valdiMenu forItem:valdiMenuItem];
    NSMenuItem *quitMenuItem = [[NSMenuItem alloc] initWithTitle:@"Quit" action:@selector(_quitApplication) keyEquivalent:@"q"];
    [valdiMenu addItem:quitMenuItem];
    NSApplication.sharedApplication.mainMenu = mainMenu;
}

- (void)_quitApplication {
    [NSApp terminate:self];
}

@end
