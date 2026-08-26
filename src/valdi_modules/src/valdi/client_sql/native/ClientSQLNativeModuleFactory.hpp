#pragma once

#include "valdi/runtime/Interfaces/IDiskCache.hpp"
#include "valdi_modules/client_sql/client_sql.hpp"
#include "valdi_core/cpp/Threading/IDispatchQueue.hpp"
#include "valdi_core/cpp/Utils/Shared.hpp"

namespace Valdi {

class ClientSQLNativeModuleFactory : public snap::valdi_modules::client_sql::ClientSQLNativeModuleFactory {
public:
    ClientSQLNativeModuleFactory();
    ClientSQLNativeModuleFactory(const Ref<IDiskCache>& diskCache, const Ref<IDispatchQueue>& workerQueue);
    ~ClientSQLNativeModuleFactory() override;

protected:
    Ref<snap::valdi_modules::client_sql::ClientSQLNativeModule> onLoadModule() override;

private:
    Ref<IDiskCache> _diskCache;
    Ref<IDispatchQueue> _workerQueue;
};

} // namespace Valdi
