#import <Cocoa/Cocoa.h>

/**
 * A helper NSApplicationDelegate implementation that helps bootstrap a Valdi application.
 */
@interface SCValdiBootstrappingNSAppDelegate : NSObject <NSApplicationDelegate>

- (instancetype)initWithRootValdiComponentPath:(NSString*)rootValdiComponentPath
                                         title:(NSString*)title
                                   windowWidth:(int)windowWidth
                                  windowHeight:(int)windowHeight
                               windowResizable:(bool)windowResizable;

- (instancetype)initWithRootValdiComponentPath:(NSString*)rootValdiComponentPath
                                         title:(NSString*)title
                                   windowWidth:(int)windowWidth
                                  windowHeight:(int)windowHeight
                               windowResizable:(bool)windowResizable
                               useHermesEngine:(bool)useHermesEngine;

@end
