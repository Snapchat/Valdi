//
//  LoadedAsset.hpp
//  valdi
//
//  Created by Simon Corsin on 8/12/21.
//

#pragma once

#include "valdi_core/cpp/Utils/Bytes.hpp"
#include "valdi_core/cpp/Utils/Result.hpp"
#include "valdi_core/cpp/Utils/ValdiObject.hpp"
#include "valdi_core/cpp/Utils/Value.hpp"

namespace Valdi {

class LoadedAsset : public ValdiObject {
public:
    virtual Result<BytesView> getBytesContent();

    virtual Value getMetadata() const;
};

} // namespace Valdi
