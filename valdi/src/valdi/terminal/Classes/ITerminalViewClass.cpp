#include "valdi/terminal/Classes/ITerminalViewClass.hpp"

namespace Valdi {

ITerminalViewClass::ITerminalViewClass(const char* iosClassName,
                                       const char* androidClassName,
                                       const Ref<ITerminalViewClass>& parentClass,
                                       bool canBeMeasured)
    : _iosClassName(iosClassName),
      _androidClassName(androidClassName),
      _parentClass(parentClass),
      _canBeMeasured(canBeMeasured) {}

ITerminalViewClass::~ITerminalViewClass() = default;

const char* ITerminalViewClass::getIOSClassName() const {
    return _iosClassName;
}

const char* ITerminalViewClass::getAndroidClassName() const {
    return _androidClassName;
}

const Ref<ITerminalViewClass>& ITerminalViewClass::getParent() const {
    return _parentClass;
}

bool ITerminalViewClass::canBeMeasured() const {
    return _canBeMeasured;
}

Ref<TerminalView> ITerminalViewClass::instantiate(const Ref<ITerminalViewClass>& self,
                                                  snap::valdi_core::Platform platform) {
    return makeShared<TerminalView>(self, platform);
}

void ITerminalViewClass::bindAttributes(AttributesBindingContext& /*binder*/) {}

Size ITerminalViewClass::onMeasure(const Ref<ValueMap>& /*attributes*/,
                                   float /*width*/,
                                   MeasureMode /*widthMode*/,
                                   float /*height*/,
                                   MeasureMode /*heightMode*/,
                                   bool /*isRightToLeft*/) {
    return Size();
}

} // namespace Valdi
