import XCTest
import Foundation
@testable import Compiler

// Unit-level coverage of the TypeKey storage and the Component ⇄ VM/Ctx binding
// machinery in NativeCodeGenerationManager. Integration behavior (annotation
// parsing, full-pipeline attach step) is verified by the swift-package tests
// plus the end-to-end bzl smoke build; this file locks the primitives in
// isolation.

final class ComponentBindingTests: XCTestCase {

    // Registering a @ViewModel makes the TypeKey lookup positive and seeds the
    // URL sidecar for backward-compat sugar.
    func testRegisterViewModelIsDiscoverableByTypeKeyAndURL() {
        let mgr = NativeCodeGenerationManager(logger: NullLogger(), globalIosImport: nil, rootURL: URL(fileURLWithPath: "/"))
        let src = URL(fileURLWithPath: "/proj/vm.ts")

        mgr.addViewModelSymbol(sourceURL: src, compilationPath: "/proj/vm.ts", symbol: "MyVM")

        let key = TSSymbolKey.make(fileName: "/proj/vm.ts", symbolName: "MyVM")
        XCTAssertTrue(mgr.isRegisteredViewModel(key: key))
        XCTAssertFalse(mgr.isRegisteredContext(key: key))
    }

    // Two @ViewModel interfaces in one file both register at their own TypeKeys
    // (the old "once per file" guard is gone). Each is discoverable independently.
    func testMultipleViewModelsInOneFileCoexist() {
        let mgr = NativeCodeGenerationManager(logger: NullLogger(), globalIosImport: nil, rootURL: URL(fileURLWithPath: "/"))
        let src = URL(fileURLWithPath: "/proj/vms.ts")

        mgr.addViewModelSymbol(sourceURL: src, compilationPath: "/proj/vms.ts", symbol: "A")
        mgr.addViewModelSymbol(sourceURL: src, compilationPath: "/proj/vms.ts", symbol: "B")

        XCTAssertTrue(mgr.isRegisteredViewModel(key: TSSymbolKey.make(fileName: "/proj/vms.ts", symbolName: "A")))
        XCTAssertTrue(mgr.isRegisteredViewModel(key: TSSymbolKey.make(fileName: "/proj/vms.ts", symbolName: "B")))
    }

    // Registering a Component binding wires up the reverse VM/Ctx → Component lookup
    // used by the attach step at code-generation time to route models to the right
    // document even when VM/Ctx are in a different file from the Component.
    func testRegisterComponentBindingPopulatesReverseLookups() {
        let mgr = NativeCodeGenerationManager(logger: NullLogger(), globalIosImport: nil, rootURL: URL(fileURLWithPath: "/"))
        let componentURL = URL(fileURLWithPath: "/proj/comp.tsx")
        let vmKey = TSSymbolKey.make(fileName: "/proj/vm.ts", symbolName: "MyVM")
        let ctxKey = TSSymbolKey.make(fileName: "/proj/ctx.ts", symbolName: "MyCtx")

        mgr.registerComponentBinding(ComponentBindingInfo(
            componentSourceURL: componentURL,
            componentSymbolName: "MyComp",
            viewModelKey: vmKey,
            contextKey: ctxKey))

        XCTAssertEqual(mgr.componentInfos(forViewModelKey: vmKey).map { $0.componentSourceURL }, [componentURL])
        XCTAssertEqual(mgr.componentInfos(forContextKey: ctxKey).map { $0.componentSourceURL }, [componentURL])
    }

    // The reverse VM lookup must return *every* Component that binds a VM, not just
    // the last one to register. A shared VM (e.g. GreetingCardViewModel used by both
    // HelloCard and GoodbyeCard) needs its model attached to each Component's document.
    func testSharedViewModelRegistersMultipleComponents() {
        let mgr = NativeCodeGenerationManager(logger: NullLogger(), globalIosImport: nil, rootURL: URL(fileURLWithPath: "/"))
        let vmKey = TSSymbolKey.make(fileName: "/proj/vm.ts", symbolName: "SharedVM")
        let helloURL = URL(fileURLWithPath: "/proj/hello.tsx")
        let goodbyeURL = URL(fileURLWithPath: "/proj/goodbye.tsx")

        mgr.registerComponentBinding(ComponentBindingInfo(
            componentSourceURL: helloURL,
            componentSymbolName: "HelloCard",
            viewModelKey: vmKey,
            contextKey: nil))
        mgr.registerComponentBinding(ComponentBindingInfo(
            componentSourceURL: goodbyeURL,
            componentSymbolName: "GoodbyeCard",
            viewModelKey: vmKey,
            contextKey: nil))

        let attached = mgr.componentInfos(forViewModelKey: vmKey).map { $0.componentSourceURL }
        XCTAssertEqual(attached.count, 2, "shared VM should route to every consuming Component")
        XCTAssertTrue(attached.contains(helloURL))
        XCTAssertTrue(attached.contains(goodbyeURL))
    }

    // Components without a context (e.g. `Component<VM>` and any equivalent that
    // sugar-fallback resolves to no ctxSlot) don't populate the ctx reverse map.
    func testComponentWithoutContextOnlyRegistersVM() {
        let mgr = NativeCodeGenerationManager(logger: NullLogger(), globalIosImport: nil, rootURL: URL(fileURLWithPath: "/"))
        let componentURL = URL(fileURLWithPath: "/proj/comp.tsx")
        let vmKey = TSSymbolKey.make(fileName: "/proj/vm.ts", symbolName: "MyVM")

        mgr.registerComponentBinding(ComponentBindingInfo(
            componentSourceURL: componentURL,
            componentSymbolName: "MyComp",
            viewModelKey: vmKey,
            contextKey: nil))

        XCTAssertEqual(mgr.componentInfos(forViewModelKey: vmKey).count, 1)
        let unrelated = TSSymbolKey.make(fileName: "/proj/whatever.ts", symbolName: "Nope")
        XCTAssertTrue(mgr.componentInfos(forContextKey: unrelated).isEmpty)
    }

    // The sugar table exposes only the two canonical valdi_core bases. Custom subclasses
    // (`PresentedStatefulComponent`, `NavigationPageComponent`, etc.) must use explicit
    // `viewModel:` / `context:` annotation params — the registry does not enumerate them.
    func testSugarRegistryContainsOnlyCanonicalBases() {
        XCTAssertNotNil(ComponentBaseRegistry.slots["Component"])
        XCTAssertNotNil(ComponentBaseRegistry.slots["StatefulComponent"])
        XCTAssertNil(ComponentBaseRegistry.slots["NavigationPageComponent"])
        XCTAssertNil(ComponentBaseRegistry.slots["PresentedStatefulComponent"])

        XCTAssertEqual(ComponentBaseRegistry.slots["Component"]?.vmSlot, 0)
        XCTAssertEqual(ComponentBaseRegistry.slots["Component"]?.ctxSlot, 1)
        XCTAssertEqual(ComponentBaseRegistry.slots["StatefulComponent"]?.vmSlot, 0)
        XCTAssertEqual(ComponentBaseRegistry.slots["StatefulComponent"]?.ctxSlot, 2)
    }

    // `clear()` wipes everything the annotation processor pushed in. This matters
    // because the pipeline calls `clear()` between compilations.
    func testClearWipesAllBindingState() {
        let mgr = NativeCodeGenerationManager(logger: NullLogger(), globalIosImport: nil, rootURL: URL(fileURLWithPath: "/"))
        let src = URL(fileURLWithPath: "/proj/vm.ts")

        mgr.addViewModelSymbol(sourceURL: src, compilationPath: "/proj/vm.ts", symbol: "MyVM")
        mgr.registerComponentBinding(ComponentBindingInfo(
            componentSourceURL: src,
            componentSymbolName: "MyComp",
            viewModelKey: TSSymbolKey.make(fileName: "/proj/vm.ts", symbolName: "MyVM"),
            contextKey: nil))

        mgr.clear()

        XCTAssertFalse(mgr.isRegisteredViewModel(key: TSSymbolKey.make(fileName: "/proj/vm.ts", symbolName: "MyVM")))
        XCTAssertTrue(mgr.componentInfos(forViewModelKey: TSSymbolKey.make(fileName: "/proj/vm.ts", symbolName: "MyVM")).isEmpty)
    }

    // TypeKey normalization must strip TypeScript extensions so that keys built from
    // TS-companion filenames match keys built from compiler-side compilation paths.
    func testTypeKeyNormalizationStripsTypeScriptExtensions() {
        let fromCompanion = TSSymbolKey.make(fileName: "/proj/foo.ts", symbolName: "X")
        let fromCompilation = TSSymbolKey.make(fileName: "/proj/foo", symbolName: "X")
        let alsoValid = TSSymbolKey.make(fileName: "/proj/foo.tsx", symbolName: "X")
        XCTAssertEqual(fromCompanion, fromCompilation)
        XCTAssertEqual(fromCompanion, alsoValid)
    }
}

// Minimal ILogger stub for tests that construct NativeCodeGenerationManager directly.
private final class NullLogger: ILogger {
    var minLevel: LogLevel = .info
    var duplicateStderrToStdout: Bool = false
    var interceptor: LoggerInterceptor? = nil
    var emittedLogsCount: Int = 0

    func log(level: LogLevel, _ message: () -> String, functionStr: StaticString) {}
    func flush() {}
}
