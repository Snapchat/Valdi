//
//  SCValdiResourceLoader.m
//  valdi-ios
//
//  Created by Simon Corsin on 10/1/19.
//

#import <Foundation/Foundation.h>
#import "valdi_core/SCValdiObjCConversionUtils.h"
#import "valdi/ios/Resources/SCValdiResourceLoader.h"
#import "valdi_core/cpp/Utils/DiskUtils.hpp"
#import "valdi_core/cpp/Utils/PathUtils.hpp"
#import "valdi_core/SCValdiImage.h"
#import "valdi_core/cpp/Utils/Format.hpp"
#import "valdi_core/SCValdiCustomModuleProvider.h"

namespace ValdiIOS {

// Resolves resourceName under resourceRoot and returns it only if a regular file exists AND stays
// within resourceRoot. Containment is checked on the standardized *filesystem path string* (not a
// URL): resourceName is treated as literal path text, so percent sequences like %2F stay literal
// instead of decoding into a separator, and any ".." is collapsed by stringByStandardizingPath
// before the prefix check — so "../../Documents/x", encoded or not, cannot escape the bundle.
// (Standardizing a URL happens before percent-decoding, which misses ".." hidden behind %2F.)
// -[NSBundle URLForResource:] never traversed out of the bundle; this preserves that guarantee.
static NSURL *containedResourceUrl(NSURL *resourceRoot, NSString *resourceName) {
    NSString *rootPath = resourceRoot.path.stringByStandardizingPath;
    if (rootPath.length == 0) {
        return nil;
    }
    NSString *candidatePath = [rootPath stringByAppendingPathComponent:resourceName].stringByStandardizingPath;
    if (![candidatePath isEqualToString:rootPath] &&
        ![candidatePath hasPrefix:[rootPath stringByAppendingString:@"/"]]) {
        return nil;
    }
    // Require a regular file, not a directory: a directory whose name matches a module would pass a
    // plain existence check and then fail confusingly later in DiskUtils::load. URLForResource: only
    // returns files, so this keeps parity.
    BOOL isDirectory = NO;
    if ([[NSFileManager defaultManager] fileExistsAtPath:candidatePath isDirectory:&isDirectory] &&
        !isDirectory) {
        return [NSURL fileURLWithPath:candidatePath];
    }
    return nil;
}

// Direct filesystem lookup of a resource relative to a bundle's resource directory. Unlike
// -[NSBundle URLForResource:withExtension:], this does not go through _CFBundleCopyFindResources,
// which enumerates the bundle's entire resource directory and builds a resource index that
// CoreFoundation retains for the bundle's lifetime — the dominant live-CFString source in a
// SnapEditor Allocations trace, made worse by the allBundles loop below building an index for
// every scanned bundle. This is a plain direct-name lookup: placements it can't resolve — device
// modifiers (~ipad/~iphone) and localized subdirectories — fall through to the URLForResource:
// fallback, which resolves them correctly (Valdi modules are flat and don't ship device variants,
// so the fast path covers the real cases). Returns nil if the file is not present.
static NSURL *directResourceUrlInBundle(NSBundle *bundle, NSString *resourceName) {
    NSURL *resourceRoot = bundle.resourceURL;
    if (!resourceRoot) {
        return nil;
    }
    return containedResourceUrl(resourceRoot, resourceName);
}

static NSURL *getResourceUrlForResourceName(NSString *resourceName) {
    // NSStringFromString(module) returns nil for invalid UTF-8 and @"" for an empty module name.
    // Guard both: URLByAppendingPathComponent: throws on nil, and an empty component resolves to the
    // bundle's resource directory (which fileExistsAtPath: accepts), so we'd return a bogus dir URL.
    if (resourceName.length == 0) {
        return nil;
    }

    NSBundle *mainBundle = [NSBundle mainBundle];
    NSBundle *imageBundle = [NSBundle bundleForClass:SCValdiImage.class];

    // Resolve by direct filesystem lookup, which avoids constructing the retained per-bundle
    // resource index. Order: main bundle, SCValdiImage bundle, then all other bundles.
    NSURL *url = directResourceUrlInBundle(mainBundle, resourceName);
    if (!url) {
        url = directResourceUrlInBundle(imageBundle, resourceName);
    }
    if (!url) {
        for (NSBundle *bundle in [NSBundle allBundles]) {
            url = directResourceUrlInBundle(bundle, resourceName);
            if (url) {
                break;
            }
        }
    }
    // No URLForResource: fallback. Every Valdi module ships as a flat <module>.valdimodule at a
    // bundle's resource root (verified: none localized or device-variant), so the direct lookup
    // above resolves every real module. URLForResource: would only add cost here — rebuilding the
    // retained per-bundle resource index while sweeping allBundles for a module that does not exist.
    // A nil result means "not found", exactly as before.
    return url;
}

ResourceLoader::ResourceLoader(id<SCValdiCustomModuleProvider> moduleProvider): _moduleProvider(moduleProvider) {}

ResourceLoader::~ResourceLoader() = default;

Valdi::Result<Valdi::BytesView> ResourceLoader::loadModuleContent(const Valdi::StringBox &module) {
    NSString *resourceName = ValdiIOS::NSStringFromString(module);
    if (_moduleProvider) {
        NSError *error = nil;
        NSData *data = [_moduleProvider customModuleDataForPath:resourceName error:&error];
        if (data) {
            return ValdiIOS::BufferFromNSData(data);
        }

        if (error) {
            return Valdi::Error(ValdiIOS::InternedStringFromNSString(error.localizedDescription));
        }
    }

    NSURL *url = getResourceUrlForResourceName(resourceName);
    if (!url) {
        return Valdi::Error("Could not find module");
    }

    auto cppPath = ValdiIOS::StringFromNSString(url.path);
    Valdi::Path path(cppPath.toStringView());

    return Valdi::DiskUtils::load(path);
}

Valdi::StringBox ResourceLoader::resolveLocalAssetURL(const Valdi::StringBox &moduleName, const Valdi::StringBox &resourcePath) {
    NSString *objcModuleName = ValdiIOS::NSStringFromString(moduleName);
    NSString *objcResourcePath = ValdiIOS::NSStringFromString(resourcePath);

    SCValdiImage *image = [SCValdiImage imageWithModuleName:objcModuleName resourcePath:objcResourcePath];
    if (!image) {
        return Valdi::StringBox();
    }

    return STRING_FORMAT("valdi-res://{}/{}", moduleName, resourcePath);
}

}
