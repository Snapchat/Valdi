//
//  SCValdiResourceLoader.m
//  valdi-ios
//
//  Created by Simon Corsin on 10/1/19.
//

#import <Foundation/Foundation.h>
#import <sys/stat.h>
#import "valdi_core/SCValdiObjCConversionUtils.h"
#import "valdi/ios/Resources/SCValdiResourceLoader.h"
#import "valdi_core/cpp/Utils/DiskUtils.hpp"
#import "valdi_core/cpp/Utils/PathUtils.hpp"
#import "valdi_core/SCValdiImage.h"
#import "valdi_core/cpp/Utils/Format.hpp"
#import "valdi_core/SCValdiCustomModuleProvider.h"

namespace ValdiIOS {

// Resolves resourceName under resourceRoot and returns it only if a regular file exists AND the
// name cannot escape resourceRoot. Module resource names are flat leaf filenames, never paths:
// every one is <bundleName> plus a literal extension (".valdimodule" in getArchiveForModule,
// ".map.json" in populateSourceMap), and bundleName is a single normalized path component by
// construction — JavaScriptPathResolver's pathToResourceId assigns it getFirstComponent() of the
// normalized import path. So containment is guaranteed by rejecting any name containing a path
// separator or ".." up front — pure in-memory string checks. resourceName is treated as literal
// path text (never percent-decoded), so sequences like %2F stay literal. This
// deliberately avoids stringByStandardizingPath: it resolves symlinks against the live filesystem
// (the app bundle sits under symlinked /private/var container paths), and two such calls per
// module lookup on the cold-render module-load path cost ~8.7ms P50 of AddFriends cold render
// (PREVIEW-31440). Rejecting ".." anywhere in the name is stricter than strictly required; real
// module names never contain it. -[NSBundle URLForResource:] never traversed out of the bundle;
// this preserves that guarantee.
static NSURL *containedResourceUrl(NSURL *resourceRoot, NSString *resourceName) {
    if ([resourceName containsString:@"/"] || [resourceName containsString:@".."]) {
        return nil;
    }
    NSString *rootPath = resourceRoot.path;
    if (rootPath.length == 0) {
        return nil;
    }
    NSString *candidatePath = [rootPath stringByAppendingPathComponent:resourceName];
    // Require a regular file: a directory whose name matches a module would pass a plain existence
    // check and then fail confusingly later in DiskUtils::load. URLForResource: only returns files,
    // so this keeps parity. stat() rather than -[NSFileManager fileExistsAtPath:isDirectory:] to
    // reach the same syscall without the NSFileManager layer above it, and S_ISREG rather than
    // !S_ISDIR so sockets and fifos are rejected too.
    struct stat candidateStat;
    if (stat(candidatePath.fileSystemRepresentation, &candidateStat) != 0 || !S_ISREG(candidateStat.st_mode)) {
        return nil;
    }
    // isDirectory:NO is not just a hint: +[NSURL fileURLWithPath:] without it stats the path again to
    // decide the URL's trailing-slash form, which the S_ISREG check above already settled.
    return [NSURL fileURLWithPath:candidatePath isDirectory:NO];
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
