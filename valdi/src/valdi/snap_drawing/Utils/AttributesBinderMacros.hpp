//
//  AttributesBinderMacros.hpp
//  valdi-desktop-apple
//
//  Created by Simon Corsin on 8/27/20.
//

#pragma once

namespace snap::drawing {
struct AttributeContext;
}

#define __GET_ATTRIBUTE_NAME(__attribute__) k##__attribute__
#define __MAKE_ATTRIBUTE_CONSTANT(__attribute__)                                                                       \
    static auto __GET_ATTRIBUTE_NAME(__attribute__) = STRING_LITERAL(STRINGIFY(__attribute__))

#define __MAKE_SIMPLE_ATTRIBUTE__(__viewClass__, __attribute__, __factory__)                                           \
    __factory__(                                                                                                       \
        [=](const auto& view, auto value, const auto& context) -> Valdi::Result<Valdi::Void> {                         \
            auto castedView = Valdi::castOrNull<__viewClass__>(view);                                                  \
            if (castedView == nullptr)                                                                                 \
                return Valdi::Void();                                                                                  \
            return this->apply_##__attribute__(*castedView, value, context);                                           \
        },                                                                                                             \
        [=](const auto& view, const auto& context) {                                                                   \
            auto castedView = Valdi::castOrNull<__viewClass__>(view);                                                  \
            if (castedView == nullptr)                                                                                 \
                return;                                                                                                \
            return this->reset_##__attribute__(*castedView, context);                                                  \
        })

#define __BIND_ATTRIBUTE__(__viewClass__, __attribute__, invalidateLayoutOnChange, __type__)                           \
    __MAKE_ATTRIBUTE_CONSTANT(__attribute__);                                                                          \
    binder.bind##__type__##Attribute(                                                                                  \
        __GET_ATTRIBUTE_NAME(__attribute__),                                                                           \
        invalidateLayoutOnChange,                                                                                      \
        __MAKE_SIMPLE_ATTRIBUTE__(__viewClass__, __attribute__, make##__type__##Attribute))

#define BIND_STRING_ATTRIBUTE(__viewClass__, __attribute__, invalidateLayoutOnChange)                                  \
    __BIND_ATTRIBUTE__(__viewClass__, __attribute__, invalidateLayoutOnChange, String)
#define BIND_DOUBLE_ATTRIBUTE(__viewClass__, __attribute__, invalidateLayoutOnChange)                                  \
    __BIND_ATTRIBUTE__(__viewClass__, __attribute__, invalidateLayoutOnChange, Double)
#define BIND_BOOLEAN_ATTRIBUTE(__viewClass__, __attribute__, invalidateLayoutOnChange)                                 \
    __BIND_ATTRIBUTE__(__viewClass__, __attribute__, invalidateLayoutOnChange, Boolean)
#define BIND_COLOR_ATTRIBUTE(__viewClass__, __attribute__, invalidateLayoutOnChange)                                   \
    __BIND_ATTRIBUTE__(__viewClass__, __attribute__, invalidateLayoutOnChange, Color)
#define BIND_INT_ATTRIBUTE(__viewClass__, __attribute__, invalidateLayoutOnChange)                                     \
    __BIND_ATTRIBUTE__(__viewClass__, __attribute__, invalidateLayoutOnChange, Int)
#define BIND_TEXT_ATTRIBUTE(__viewClass__, __attribute__, invalidateLayoutOnChange)                                    \
    __BIND_ATTRIBUTE__(__viewClass__, __attribute__, invalidateLayoutOnChange, Text)
#define BIND_UNTYPED_ATTRIBUTE(__viewClass__, __attribute__, invalidateLayoutOnChange)                                 \
    __BIND_ATTRIBUTE__(__viewClass__, __attribute__, invalidateLayoutOnChange, Untyped)
#define BIND_COMPOSITE_ATTRIBUTE(__viewClass__, __attribute__, __parts__)                                              \
    __MAKE_ATTRIBUTE_CONSTANT(__attribute__);                                                                          \
    binder.bindCompositeAttribute(__GET_ATTRIBUTE_NAME(__attribute__),                                                 \
                                  __parts__,                                                                           \
                                  __MAKE_SIMPLE_ATTRIBUTE__(__viewClass__, __attribute__, makeUntypedAttribute))

#define BIND_TRANSFORM_ATTRIBUTES(__viewClass__, __attribute__)                                                        \
    __MAKE_ATTRIBUTE_CONSTANT(__attribute__);                                                                          \
    binder.bindTransformAttributes(__MAKE_SIMPLE_ATTRIBUTE__(__viewClass__, __attribute__, makeUntypedAttribute))

#define BIND_BORDER_ATTRIBUTE(__viewClass__, __attribute__, invalidateLayoutOnChange)                                  \
    __BIND_ATTRIBUTE__(__viewClass__, __attribute__, invalidateLayoutOnChange, Border)

#define BIND_FUNCTION_ATTRIBUTE(__viewClass__, __attribute__)                                                          \
    __MAKE_ATTRIBUTE_CONSTANT(__attribute__);                                                                          \
    binder.bindUntypedAttribute(__GET_ATTRIBUTE_NAME(__attribute__),                                                   \
                                false,                                                                                 \
                                __MAKE_SIMPLE_ATTRIBUTE__(__viewClass__, __attribute__, makeFunctionAttribute))

#define REGISTER_PREPROCESSOR(__attribute__, __enableCache__)                                                          \
    binder.registerPreprocessor(STRING_LITERAL(STRINGIFY(__attribute__)),                                              \
                                __enableCache__,                                                                       \
                                [=](const Valdi::Value& value) -> Valdi::Result<Valdi::Value> {                        \
                                    return this->preprocess_##__attribute__(value);                                    \
                                })

#define __DECLARE_ATTRIBUTE(__viewClass__, __attribute__, __type__)                                                    \
    Valdi::Result<Valdi::Void> apply_##                                                                                \
        __attribute__(__viewClass__ & view, __type__ value, const AttributeContext& context); /* NOLINT */             \
    void reset_##__attribute__(__viewClass__ & view, const AttributeContext& context);        /* NOLINT */

#define DECLARE_STRING_ATTRIBUTE(__viewClass__, __attribute__)                                                         \
    __DECLARE_ATTRIBUTE(__viewClass__, __attribute__, const String&)
#define DECLARE_DOUBLE_ATTRIBUTE(__viewClass__, __attribute__) __DECLARE_ATTRIBUTE(__viewClass__, __attribute__, double)
#define DECLARE_BOOLEAN_ATTRIBUTE(__viewClass__, __attribute__) __DECLARE_ATTRIBUTE(__viewClass__, __attribute__, bool)
#define DECLARE_COLOR_ATTRIBUTE(__viewClass__, __attribute__) __DECLARE_ATTRIBUTE(__viewClass__, __attribute__, int64_t)
#define DECLARE_INT_ATTRIBUTE(__viewClass__, __attribute__) __DECLARE_ATTRIBUTE(__viewClass__, __attribute__, int64_t)
#define DECLARE_TEXT_ATTRIBUTE(__viewClass__, __attribute__)                                                           \
    __DECLARE_ATTRIBUTE(__viewClass__, __attribute__, const Valdi::Value&)
#define DECLARE_UNTYPED_ATTRIBUTE(__viewClass__, __attribute__)                                                        \
    __DECLARE_ATTRIBUTE(__viewClass__, __attribute__, const Valdi::Value&)

#define __IMPLEMENT_ATTRIBUTE(__viewClass__, __attribute__, __type__, __onApply__, __onReset__)                        \
    Valdi::Result<Valdi::Void> __viewClass__##Class::apply_##                                                          \
        __attribute__(__viewClass__ & view, __type__ value, const AttributeContext& context) /* NOLINT */              \
        __onApply__ void __viewClass__##Class::reset_##                                      /* NOLINT */              \
        __attribute__(__viewClass__ & view, const AttributeContext& context) __onReset__     /* NOLINT */

#define IMPLEMENT_STRING_ATTRIBUTE(__viewClass__, __attribute__, __onApply__, __onReset__)                             \
    __IMPLEMENT_ATTRIBUTE(__viewClass__, __attribute__, const String&, __onApply__, __onReset__)
#define IMPLEMENT_DOUBLE_ATTRIBUTE(__viewClass__, __attribute__, __onApply__, __onReset__)                             \
    __IMPLEMENT_ATTRIBUTE(__viewClass__, __attribute__, double, __onApply__, __onReset__)
#define IMPLEMENT_BOOLEAN_ATTRIBUTE(__viewClass__, __attribute__, __onApply__, __onReset__)                            \
    __IMPLEMENT_ATTRIBUTE(__viewClass__, __attribute__, bool, __onApply__, __onReset__)
#define IMPLEMENT_COLOR_ATTRIBUTE(__viewClass__, __attribute__, __onApply__, __onReset__)                              \
    __IMPLEMENT_ATTRIBUTE(__viewClass__, __attribute__, int64_t, __onApply__, __onReset__)
#define IMPLEMENT_INT_ATTRIBUTE(__viewClass__, __attribute__, __onApply__, __onReset__)                                \
    __IMPLEMENT_ATTRIBUTE(__viewClass__, __attribute__, int64_t, __onApply__, __onReset__)
#define IMPLEMENT_TEXT_ATTRIBUTE(__viewClass__, __attribute__, __onApply__, __onReset__)                               \
    __IMPLEMENT_ATTRIBUTE(__viewClass__, __attribute__, const Valdi::Value&, __onApply__, __onReset__)
#define IMPLEMENT_UNTYPED_ATTRIBUTE(__viewClass__, __attribute__, __onApply__, __onReset__)                            \
    __IMPLEMENT_ATTRIBUTE(__viewClass__, __attribute__, const Valdi::Value&, __onApply__, __onReset__)
