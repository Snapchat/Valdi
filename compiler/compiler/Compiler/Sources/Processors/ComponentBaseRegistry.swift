//
//  ComponentBaseRegistry.swift
//  Compiler
//
//  Sugar-fallback slot map for the common Valdi Component base classes. Consumed by
//  `ApplyTypeScriptAnnotationsProcessor` when a `@Component` declaration doesn't
//  carry explicit `viewModel:` / `context:` annotation parameters — the processor
//  falls back to pulling those type keys from the base class's generic-argument
//  slots as listed here.
//
//  Only the two canonical base classes shipped by valdi_core are enumerated. Custom
//  Component subclasses (dozens across composer_modules) opt into explicit params:
//  `/** @Component({viewModel: 'MyVM', context: 'MyCtx'}) */`. Doing this by
//  hardcoding every subclass would be brittle and lookups would break across module
//  boundaries anyway (dependency modules aren't part of the current compilation's
//  dumped-symbol set).
//

import Foundation

/// Which positional type argument on a Component base class carries the ViewModel
/// vs the Context, after normalizing to `Component<VM, Ctx>`.
struct ComponentBaseSlots {
    let vmSlot: Int
    /// Some bases (or intentionally simple Components) omit the context parameter.
    let ctxSlot: Int?
}

enum ComponentBaseRegistry {
    /// Post-substitution slot map. Additions ship in valdi_core releases; add a row
    /// only for canonical base classes that live in valdi_core.
    static let slots: [String: ComponentBaseSlots] = [
        "Component":         ComponentBaseSlots(vmSlot: 0, ctxSlot: 1),
        "StatefulComponent": ComponentBaseSlots(vmSlot: 0, ctxSlot: 2),
    ]
}
