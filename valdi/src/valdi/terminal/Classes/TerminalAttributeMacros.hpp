#pragma once

#include "valdi/runtime/Attributes/AttributeHandlerDelegate.hpp"
#include "valdi/runtime/Attributes/AttributesBindingContext.hpp"
#include "valdi/terminal/TerminalView.hpp"
#include "valdi_core/cpp/Utils/Function.hpp"
#include "valdi_core/cpp/Utils/Result.hpp"
#include "valdi_core/cpp/Utils/StringCache.hpp"
#include "valdi_core/cpp/Utils/Value.hpp"

namespace Valdi::TerminalAttributes {

using ViewApplier = Function<Result<Void>(TerminalView& view, const Value& value)>;
using ViewResetter = Function<void(TerminalView& view)>;

/**
 * Creates an attribute handler that runs the callbacks against the TerminalView the attribute is
 * applied to, and requests a repaint afterwards. Views of other kinds are ignored.
 */
Ref<AttributeHandlerDelegate> makeDelegate(ViewApplier&& applier, ViewResetter&& resetter);

/** A handler for attributes the terminal backend has no use for; accepts and discards them. */
Ref<AttributeHandlerDelegate> makeIgnoringDelegate();

} // namespace Valdi::TerminalAttributes

/**
 * Binds `name` as an attribute of kind `type` (String, Double, Boolean, Int, Color, Text or
 * Untyped) on `binder`. `applyFn` is `Result<Void>(TerminalView&, const Value&)` and `resetFn` is
 * `void(TerminalView&)`.
 */
#define TERMINAL_BIND_ATTRIBUTE(binder, type, name, invalidateLayout, applyFn, resetFn)                                \
    (binder).bind##type##Attribute(Valdi::StringCache::getGlobal().makeStringFromLiteral(name),                        \
                                   (invalidateLayout),                                                                 \
                                   Valdi::TerminalAttributes::makeDelegate((applyFn), (resetFn)))
