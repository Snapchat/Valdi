//
//  ContextBase.hpp
//  ValdiRuntime
//
//  Created by Simon Corsin on 11/12/23.
//  Copyright © 2023 Snap Inc. All rights reserved.
//

#pragma once

#include "valdi_core/cpp/Context/Attribution.hpp"
#include "valdi_core/cpp/Context/ComponentPath.hpp"
#include "valdi_core/cpp/Context/ContextId.hpp"

#include "valdi_core/cpp/Utils/ValdiObject.hpp"

namespace Valdi {

class ContextBase : public ValdiObject {
public:
    ContextBase(ContextId contextId, const Attribution& attribution, const ComponentPath& path,
                const StringBox& scopeName = StringBox());
    ~ContextBase() override;

    ContextId getContextId() const;

    const ComponentPath& getPath() const;

    /**
     * Returns the scope name for this context (set by native code when creating scoped runtimes).
     */
    const StringBox& getScopeName() const;

    std::string getIdAndPathString() const;

    static ContextBase* current();
    static void setCurrent(ContextBase* currentContext);

    void withAttribution(const AttributionFunctionCallback& handler) const;
    const Attribution& getAttribution() const;

    RefCountable* getWeakReferenceTable() const;
    void setWeakReferenceTable(const Ref<RefCountable>& weakReferenceTable);

    RefCountable* getStrongReferenceTable() const;
    void setStrongReferenceTable(const Ref<RefCountable>& strongReferenceTable);

    VALDI_CLASS_HEADER(ContextBase)

private:
    ContextId _contextId;
    Attribution _attribution;
    ComponentPath _path;
    StringBox _scopeName;
    Ref<RefCountable> _weakReferenceTable;
    Ref<RefCountable> _strongReferenceTable;
};

} // namespace Valdi
