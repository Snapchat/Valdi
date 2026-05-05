//
//  LoadedAsset.cpp
//  valdi
//
//  Created by Simon Corsin on 8/12/21.
//

#include "valdi_core/cpp/Resources/LoadedAsset.hpp"

namespace Valdi {

Result<BytesView> LoadedAsset::getBytesContent() {
    return Error("Not implemented");
}

Value LoadedAsset::getMetadata() const {
    return Value::undefined();
}

} // namespace Valdi
