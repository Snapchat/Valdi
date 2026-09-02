#pragma once

#include "valdi/runtime/Interfaces/IDiskCache.hpp"
#include "valdi_modules/client_sql/client_sql.hpp"
#include "valdi_core/cpp/Threading/IDispatchQueue.hpp"
#include "valdi_core/cpp/Utils/Shared.hpp"

#include <chrono>

namespace Valdi {

class ClientSQLNativeModuleFactory : public snap::valdi_modules::client_sql::ClientSQLNativeModuleFactory {
public:
    ClientSQLNativeModuleFactory();
    ClientSQLNativeModuleFactory(const Ref<IDiskCache>& diskCache, const Ref<IDispatchQueue>& workerQueue);
    ClientSQLNativeModuleFactory(const Ref<IDiskCache>& diskCache,
                                 const Ref<IDispatchQueue>& workerQueue,
                                 std::chrono::milliseconds transactionTimeout);
    ClientSQLNativeModuleFactory(const Ref<IDiskCache>& diskCache,
                                 const Ref<IDispatchQueue>& workerQueue,
                                 std::chrono::milliseconds transactionTimeout,
                                 const Ref<IDispatchQueue>& resourceReleaseQueue);
    ~ClientSQLNativeModuleFactory() override;

protected:
    Ref<snap::valdi_modules::client_sql::ClientSQLNativeModule> onLoadModule() override;

private:
    Ref<IDiskCache> _diskCache;
    Ref<IDispatchQueue> _workerQueue;
    Ref<IDispatchQueue> _resourceReleaseQueue;
    std::chrono::milliseconds _transactionTimeout;
};

} // namespace Valdi
