#include "ClientSQLNativeModuleFactory.hpp"

#include "valdi/runtime/Runtime.hpp"
#include "valdi_core/cpp/JavaScript/ModuleFactoryRegistry.hpp"
#include "valdi_core/cpp/Threading/DispatchQueue.hpp"
#include "valdi_core/cpp/Utils/DiskUtils.hpp"
#include "valdi_core/cpp/Utils/Exception.hpp"
#include "valdi_core/cpp/Utils/Format.hpp"
#include "valdi_core/cpp/Utils/StringCache.hpp"
#include "valdi_core/cpp/Utils/ValueArray.hpp"
#include "valdi_core/cpp/Utils/ValueMap.hpp"
#include "valdi_core/cpp/Utils/ValueTypedArray.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <deque>
#include <exception>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#if defined(VALDI_CLIENTSQL_USE_BUNDLED_SQLITE)
#include "sqlite3.h"
#else
#include <sqlite3.h>
#endif

namespace Valdi {
namespace {

constexpr std::string_view kClientSQLDirectory = "ClientSQLNative";
constexpr size_t kClientSQLReaderConnectionCount = 4;
constexpr size_t kClientSQLMaxQueryResultRows = 10000;
constexpr size_t kClientSQLMaxQueryResultBytes = 8 * 1024 * 1024;
// Include container/control-block slack in addition to payload so rows made up of
// NULL values or many short columns cannot evade the byte ceiling.
constexpr size_t kClientSQLQueryResultRowOverheadBytes = 64;
constexpr size_t kClientSQLQueryResultCellOverheadBytes = 64;
constexpr auto kClientSQLDefaultTransactionTimeout = std::chrono::seconds(30);
constexpr sqlite3_int64 kMaxSafeJavaScriptInteger = 9007199254740991LL;
constexpr int kMinimumClientSQLSQLiteVersion = 3016000;
std::atomic<size_t> gClientSQLLiveCoordinatorCount{0};

struct SQLiteStatementDeleter {
    void operator()(sqlite3_stmt* statement) const {
        sqlite3_finalize(statement);
    }
};

using SQLiteStatement = std::unique_ptr<sqlite3_stmt, SQLiteStatementDeleter>;

using SQLiteConnectionHandle = std::unique_ptr<sqlite3, decltype(&sqlite3_close)>;

const char* clientSQLSQLiteError(sqlite3* database) {
    return database == nullptr ? "SQLite error" : sqlite3_errmsg(database);
}

sqlite3_destructor_type sqliteTransient() {
    return reinterpret_cast<sqlite3_destructor_type>(-1);
}

Result<Void> checkSQLiteStatus(sqlite3* database, int status, std::string_view operation) {
    if (status == SQLITE_OK || status == SQLITE_DONE || status == SQLITE_ROW) {
        return Void();
    }

    return Error(STRING_FORMAT("ClientSQLNative {} failed: {}", operation, clientSQLSQLiteError(database)));
}

bool containsEmbeddedNull(std::string_view value) {
    return value.find('\0') != std::string_view::npos;
}

bool execRawSQL(sqlite3* database, std::string_view sql, Error* outError) {
    if (containsEmbeddedNull(sql)) {
        if (outError != nullptr) {
            *outError = Error("ClientSQLNative SQL contains an embedded NUL byte");
        }
        return false;
    }

    char* rawError = nullptr;
    const auto status = sqlite3_exec(database, std::string(sql).c_str(), nullptr, nullptr, &rawError);
    if (status == SQLITE_OK) {
        return true;
    }

    if (outError != nullptr) {
        std::string message = rawError != nullptr ? rawError : clientSQLSQLiteError(database);
        *outError = Error(STRING_FORMAT("ClientSQLNative SQL failed: {}", message));
    }
    sqlite3_free(rawError);
    return false;
}

Result<SQLiteStatement> prepareStatement(sqlite3* database, const StringBox& sql) {
    if (sql.length() > static_cast<size_t>(std::numeric_limits<int>::max())) {
        return Error("ClientSQLNative SQL text is too large");
    }

    sqlite3_stmt* statement = nullptr;
    const char* tail = nullptr;
    const auto status = sqlite3_prepare_v2(
        database, sql.getCStr(), static_cast<int>(sql.length()), &statement, &tail);
    if (status != SQLITE_OK) {
        return Error(STRING_FORMAT("ClientSQLNative prepare failed: {}", clientSQLSQLiteError(database)));
    }

    SQLiteStatement preparedStatement(statement);
    if (preparedStatement == nullptr) {
        return Error("ClientSQLNative SQL must contain exactly one statement");
    }

    const auto* sqlEnd = sql.getCStr() + sql.length();
    while (tail != nullptr && tail < sqlEnd) {
        sqlite3_stmt* trailingStatement = nullptr;
        const char* nextTail = nullptr;
        const auto trailingStatus = sqlite3_prepare_v2(
            database,
            tail,
            static_cast<int>(sqlEnd - tail),
            &trailingStatement,
            &nextTail);
        SQLiteStatement preparedTrailingStatement(trailingStatement);
        if (trailingStatus != SQLITE_OK || preparedTrailingStatement != nullptr) {
            return Error("ClientSQLNative SQL must contain exactly one statement");
        }
        if (nextTail == nullptr || nextTail <= tail) {
            return Error("ClientSQLNative SQL contains unsupported trailing content");
        }
        tail = nextTail;
    }

    return preparedStatement;
}

Result<int32_t> readUserVersion(sqlite3* database) {
    auto statementResult = prepareStatement(database, STRING_LITERAL("PRAGMA user_version"));
    if (!statementResult) {
        return statementResult.moveError();
    }

    auto statement = statementResult.moveValue();
    const auto status = sqlite3_step(statement.get());
    if (status != SQLITE_ROW) {
        return Error(STRING_FORMAT("ClientSQLNative failed to read schema version: {}", clientSQLSQLiteError(database)));
    }

    return sqlite3_column_int(statement.get(), 0);
}

Result<Void> enableAndVerifyWAL(sqlite3* database) {
    auto statementResult = prepareStatement(database, STRING_LITERAL("PRAGMA journal_mode = WAL"));
    if (!statementResult) {
        return statementResult.moveError();
    }

    auto statement = statementResult.moveValue();
    const auto status = sqlite3_step(statement.get());
    if (status != SQLITE_ROW) {
        return Error(STRING_FORMAT(
            "ClientSQLNative failed to enable WAL mode: {}", clientSQLSQLiteError(database)));
    }

    const auto* mode = sqlite3_column_text(statement.get(), 0);
    const auto modeLength = sqlite3_column_bytes(statement.get(), 0);
    if (mode == nullptr || modeLength != 3 || sqlite3_strnicmp(reinterpret_cast<const char*>(mode), "wal", 3) != 0) {
        const auto actualMode = mode == nullptr
            ? std::string("<null>")
            : std::string(reinterpret_cast<const char*>(mode), static_cast<size_t>(std::max(modeLength, 0)));
        return Error(STRING_FORMAT(
            "ClientSQLNative requires WAL journal mode but SQLite selected '{}'", actualMode));
    }

    return Void();
}

Error makeSQLExecutionError(sqlite3* database, std::string_view operation) {
    return Error(STRING_FORMAT("ClientSQLNative {} failed: {}", operation, clientSQLSQLiteError(database)));
}

Result<Void> configureSQLiteConnection(sqlite3* database, bool writer) {
    if (sqlite3_libversion_number() < kMinimumClientSQLSQLiteVersion) {
        return Error(STRING_FORMAT(
            "ClientSQLNative requires SQLite 3.16.0 or newer; found {}", sqlite3_libversion()));
    }
    sqlite3_busy_timeout(database, 5000);

    Error sqlError;
    if (writer) {
        auto walResult = enableAndVerifyWAL(database);
        if (!walResult) {
            return walResult.moveError();
        }
    }
    if (!execRawSQL(database, "PRAGMA foreign_keys = ON", &sqlError)) {
        return sqlError;
    }
    if (!execRawSQL(database, "PRAGMA synchronous = NORMAL", &sqlError)) {
        return sqlError;
    }
    if (!writer && !execRawSQL(database, "PRAGMA query_only = ON", &sqlError)) {
        return sqlError;
    }

    return Void();
}

Result<SQLiteConnectionHandle> openSQLiteDatabase(const std::string& databasePath,
                                                  const StringBox& name,
                                                  int flags,
                                                  bool writer) {
    sqlite3* rawDatabase = nullptr;
    const auto status = sqlite3_open_v2(databasePath.c_str(), &rawDatabase, flags | SQLITE_OPEN_FULLMUTEX, nullptr);
    SQLiteConnectionHandle database(rawDatabase, sqlite3_close);
    if (status != SQLITE_OK) {
        return Error(
            STRING_FORMAT("ClientSQLNative failed to open database '{}': {}", name, clientSQLSQLiteError(rawDatabase)));
    }

    auto configureResult = configureSQLiteConnection(database.get(), writer);
    if (!configureResult) {
        return configureResult.moveError();
    }

    return std::move(database);
}

Ref<IDispatchQueue> createClientSQLDedicatedQueue(const StringBox& name) {
    // Never substitute the resource-release queue here. The coordinator must
    // be able to transfer queue-owning refs away from whichever database queue
    // releases its final ref, so database queues must be structurally distinct.
    return DispatchQueue::createThreaded(name, ThreadQoSClassNormal);
}

Result<Void> bindParameters(sqlite3* database, sqlite3_stmt* statement, const Value& parametersValue) {
    const auto bindCount = sqlite3_bind_parameter_count(statement);
    Ref<ValueArray> parameters;
    if (parametersValue.isNullOrUndefined()) {
        parameters = ValueArray::make(0);
    } else if (parametersValue.isArray()) {
        parameters = parametersValue.getArrayRef();
    } else {
        return Error("ClientSQLNative parameters must be an array");
    }

    if (parameters->size() != static_cast<size_t>(bindCount)) {
        return Error(
            STRING_FORMAT("ClientSQLNative expected {} SQL parameters but received {}", bindCount, parameters->size()));
    }

    for (int parameterIndex = 0; parameterIndex < bindCount; ++parameterIndex) {
        const auto& parameter = (*parameters)[static_cast<size_t>(parameterIndex)];
        const auto sqliteIndex = parameterIndex + 1;
        int status = SQLITE_OK;

        switch (parameter.getType()) {
            case ValueType::Null:
                status = sqlite3_bind_null(statement, sqliteIndex);
                break;
            case ValueType::Bool:
                status = sqlite3_bind_int(statement, sqliteIndex, parameter.toBool() ? 1 : 0);
                break;
            case ValueType::Int:
                status = sqlite3_bind_int(statement, sqliteIndex, parameter.toInt());
                break;
            case ValueType::Long:
                status = sqlite3_bind_int64(statement, sqliteIndex, static_cast<sqlite3_int64>(parameter.toLong()));
                break;
            case ValueType::Double: {
                const auto value = parameter.toDouble();
                if (!std::isfinite(value)) {
                    return Error("ClientSQLNative double parameters must be finite");
                }
                status = sqlite3_bind_double(statement, sqliteIndex, value);
                break;
            }
            case ValueType::InternedString:
            case ValueType::StaticString: {
                auto string = parameter.toStringBox();
                if (string.length() > static_cast<size_t>(std::numeric_limits<int>::max())) {
                    return Error("ClientSQLNative string parameter is too large");
                }
                status = sqlite3_bind_text(
                    statement, sqliteIndex, string.getCStr(), static_cast<int>(string.length()), sqliteTransient());
                break;
            }
            case ValueType::TypedArray: {
                const auto& buffer = parameter.getTypedArray()->getBuffer();
                if (buffer.size() > static_cast<size_t>(std::numeric_limits<int>::max())) {
                    return Error("ClientSQLNative blob parameter is too large");
                }
                static constexpr Byte emptyBlobSentinel = 0;
                const auto* blobData = buffer.size() == 0 ? &emptyBlobSentinel : buffer.data();
                status = sqlite3_bind_blob(
                    statement,
                    sqliteIndex,
                    blobData,
                    static_cast<int>(buffer.size()),
                    sqliteTransient());
                break;
            }
            default:
                return Error(STRING_FORMAT(
                    "ClientSQLNative unsupported parameter type '{}'", valueTypeToString(parameter.getType())));
        }

        auto bindResult = checkSQLiteStatus(database, status, "bind");
        if (!bindResult) {
            return bindResult;
        }
    }

    return Void();
}

Result<Value> sqliteColumnToValue(sqlite3_stmt* statement, int columnIndex) {
    switch (sqlite3_column_type(statement, columnIndex)) {
        case SQLITE_NULL:
            return Value();
        case SQLITE_INTEGER: {
            const auto value = sqlite3_column_int64(statement, columnIndex);
            if (value >= std::numeric_limits<int32_t>::min() && value <= std::numeric_limits<int32_t>::max()) {
                return Value(static_cast<int32_t>(value));
            }
            if (value < -kMaxSafeJavaScriptInteger || value > kMaxSafeJavaScriptInteger) {
                return Error(STRING_FORMAT(
                    "ClientSQLNative integer value {} exceeds JavaScript's exact integer range; store 64-bit identifiers as TEXT",
                    value));
            }
            return Value(static_cast<double>(value));
        }
        case SQLITE_FLOAT:
            return Value(sqlite3_column_double(statement, columnIndex));
        case SQLITE_TEXT: {
            const auto* text = sqlite3_column_text(statement, columnIndex);
            const auto byteCount = sqlite3_column_bytes(statement, columnIndex);
            if (byteCount < 0) {
                return Error(STRING_FORMAT(
                    "ClientSQLNative SQLite TEXT column {} returned an invalid length", columnIndex));
            }
            if (byteCount == 0) {
                return Value(StringBox::emptyString());
            }
            if (text == nullptr) {
                return Error(STRING_FORMAT(
                    "ClientSQLNative SQLite TEXT column {} returned null data for {} bytes", columnIndex, byteCount));
            }
            return Value(StringCache::getGlobal().makeString(
                std::string_view(reinterpret_cast<const char*>(text), static_cast<size_t>(byteCount))));
        }
        case SQLITE_BLOB: {
            const auto* data = sqlite3_column_blob(statement, columnIndex);
            const auto byteCount = sqlite3_column_bytes(statement, columnIndex);
            if (byteCount < 0) {
                return Error(STRING_FORMAT(
                    "ClientSQLNative SQLite BLOB column {} returned an invalid length", columnIndex));
            }
            if (byteCount > 0 && data == nullptr) {
                return Error(STRING_FORMAT(
                    "ClientSQLNative SQLite BLOB column {} returned null data for {} bytes", columnIndex, byteCount));
            }
            auto bytes = makeShared<Bytes>();
            if (byteCount > 0) {
                bytes->assignData(reinterpret_cast<const Byte*>(data), static_cast<size_t>(byteCount));
            }
            return Value(makeShared<ValueTypedArray>(TypedArrayType::ArrayBuffer, bytes));
        }
        default:
            return Value::undefined();
    }
}

Result<Value> sqliteRowToValue(sqlite3_stmt* statement) {
    auto row = makeShared<ValueMap>();
    const auto columnCount = sqlite3_column_count(statement);
    for (int columnIndex = 0; columnIndex < columnCount; ++columnIndex) {
        auto columnName =
            StringCache::getGlobal().makeString(std::string_view(sqlite3_column_name(statement, columnIndex)));
        auto columnValue = sqliteColumnToValue(statement, columnIndex);
        if (!columnValue) {
            return columnValue.moveError();
        }
        (*row)[columnName] = columnValue.moveValue();
    }
    return Value(row);
}

Result<Void> addQueryMaterializationBytes(size_t byteCount, size_t* materializedByteSize) {
    if (byteCount > kClientSQLMaxQueryResultBytes - *materializedByteSize) {
        return Error(STRING_FORMAT(
            "ClientSQLNative query result exceeds the {} byte materialization limit; add a LIMIT clause or select less data",
            kClientSQLMaxQueryResultBytes));
    }
    *materializedByteSize += byteCount;
    return Void();
}

Result<size_t> sqliteRowMaterializedByteSize(sqlite3_stmt* statement) {
    size_t materializedByteSize = kClientSQLQueryResultRowOverheadBytes;
    const auto columnCount = sqlite3_column_count(statement);
    for (int columnIndex = 0; columnIndex < columnCount; ++columnIndex) {
        auto overheadResult = addQueryMaterializationBytes(
            kClientSQLQueryResultCellOverheadBytes, &materializedByteSize);
        if (!overheadResult) {
            return overheadResult.moveError();
        }

        const auto* columnName = sqlite3_column_name(statement, columnIndex);
        if (columnName == nullptr) {
            return Error(STRING_FORMAT(
                "ClientSQLNative SQLite column {} returned a null name", columnIndex));
        }
        auto columnNameResult = addQueryMaterializationBytes(
            std::string_view(columnName).size(), &materializedByteSize);
        if (!columnNameResult) {
            return columnNameResult.moveError();
        }

        size_t columnByteSize = 0;
        switch (sqlite3_column_type(statement, columnIndex)) {
            case SQLITE_INTEGER:
            case SQLITE_FLOAT:
                columnByteSize = sizeof(double);
                break;
            case SQLITE_TEXT:
            case SQLITE_BLOB: {
                const auto byteCount = sqlite3_column_bytes(statement, columnIndex);
                if (byteCount < 0) {
                    return Error(STRING_FORMAT(
                        "ClientSQLNative SQLite column {} returned an invalid length", columnIndex));
                }
                columnByteSize = static_cast<size_t>(byteCount);
                break;
            }
            default:
                break;
        }

        auto columnResult = addQueryMaterializationBytes(columnByteSize, &materializedByteSize);
        if (!columnResult) {
            return columnResult.moveError();
        }
    }
    return materializedByteSize;
}

struct ClientSQLRequest {
    StringBox sql;
    Value parameters;
};

struct ClientSQLDatabasePath {
    std::string databasePath;
    Path databaseRoot;
};

struct ClientSQLOpenRequest {
    StringBox name;
    ClientSQLDatabasePath databasePath;
    int32_t schemaVersion;
    Ref<ValueArray> createStatements;
    Ref<ValueArray> migrations;
};

void appendSchemaFingerprintString(std::string& fingerprint, const StringBox& value) {
    const auto view = value.toStringView();
    fingerprint.append(std::to_string(view.size()));
    fingerprint.push_back(':');
    fingerprint.append(view.data(), view.size());
}

std::string schemaFingerprint(const ClientSQLOpenRequest& request) {
    std::string fingerprint;
    fingerprint.append("schema-version:");
    fingerprint.append(std::to_string(request.schemaVersion));
    fingerprint.append(";create-count:");
    fingerprint.append(std::to_string(request.createStatements->size()));
    fingerprint.push_back(';');
    for (const auto& statement : *request.createStatements) {
        appendSchemaFingerprintString(fingerprint, statement.toStringBox());
    }
    fingerprint.append("migration-count:");
    fingerprint.append(std::to_string(request.migrations->size()));
    fingerprint.push_back(';');
    for (const auto& migration : *request.migrations) {
        fingerprint.append("migration-version:");
        fingerprint.append(std::to_string(migration.getMapValue("version").toInt()));
        fingerprint.push_back(';');
        auto statements = migration.getMapValue("statements").getArrayRef();
        const auto statementCount = statements == nullptr ? 0 : statements->size();
        fingerprint.append("statement-count:");
        fingerprint.append(std::to_string(statementCount));
        fingerprint.push_back(';');
        if (statements != nullptr) {
            for (const auto& statement : *statements) {
                appendSchemaFingerprintString(fingerprint, statement.toStringBox());
            }
        }
    }
    return fingerprint;
}

class ClientSQLQueueConnection : public SimpleRefCountable {
public:
    explicit ClientSQLQueueConnection(Ref<IDispatchQueue> queue)
        : database(nullptr, sqlite3_close), queue(std::move(queue)) {}
    ~ClientSQLQueueConnection() override = default;

    Result<sqlite3*> requireDatabase() {
        if (!openError.isEmpty()) {
            return openError;
        }
        if (database == nullptr) {
            return Error("ClientSQLNative database is not open");
        }
        return database.get();
    }

    SQLiteConnectionHandle database;
    Ref<IDispatchQueue> queue;
    Error openError;
};

Result<Void> applySchema(sqlite3* database,
                         int32_t currentVersion,
                         int32_t targetVersion,
                         const Ref<ValueArray>& createStatements,
                         const Ref<ValueArray>& migrations);
Result<Void> openWriterDatabase(const Ref<ClientSQLQueueConnection>& connection, const ClientSQLOpenRequest& request);
Result<Void> openReaderDatabases(const std::vector<Ref<ClientSQLQueueConnection>>& readerConnections,
                                 const ClientSQLOpenRequest& request);

Result<Void> executeStatement(sqlite3* database, const ClientSQLRequest& request) {
    auto statement = prepareStatement(database, request.sql);
    if (!statement) {
        return statement.moveError();
    }

    auto bindResult = bindParameters(database, statement.value().get(), request.parameters);
    if (!bindResult) {
        return bindResult.moveError();
    }

    while (true) {
        const auto status = sqlite3_step(statement.value().get());
        if (status == SQLITE_DONE) {
            return Void();
        }
        if (status != SQLITE_ROW) {
            return makeSQLExecutionError(database, "execute");
        }
    }
}

Result<Value> queryStatement(sqlite3* database, const ClientSQLRequest& request) {
    auto statement = prepareStatement(database, request.sql);
    if (!statement) {
        return statement.moveError();
    }

    auto bindResult = bindParameters(database, statement.value().get(), request.parameters);
    if (!bindResult) {
        return bindResult.moveError();
    }

    std::vector<Value> rows;
    size_t materializedByteSize = 0;
    while (true) {
        const auto status = sqlite3_step(statement.value().get());
        if (status == SQLITE_DONE) {
            return Value(ValueArray::make(std::move(rows)));
        }
        if (status != SQLITE_ROW) {
            return makeSQLExecutionError(database, "query");
        }
        if (rows.size() >= kClientSQLMaxQueryResultRows) {
            return Error(STRING_FORMAT(
                "ClientSQLNative query result exceeds the {} row materialization limit; add a LIMIT clause",
                kClientSQLMaxQueryResultRows));
        }
        auto rowMaterializedByteSize = sqliteRowMaterializedByteSize(statement.value().get());
        if (!rowMaterializedByteSize) {
            return rowMaterializedByteSize.moveError();
        }
        auto sizeResult = addQueryMaterializationBytes(
            rowMaterializedByteSize.value(), &materializedByteSize);
        if (!sizeResult) {
            return sizeResult.moveError();
        }
        auto row = sqliteRowToValue(statement.value().get());
        if (!row) {
            return row.moveError();
        }
        rows.emplace_back(row.moveValue());
    }
}

using ClientSQLValueCallback =
    snap::valdi_modules::client_sql::ClientSQLNativeConnectionProxy::ExecuteCallbackFn;
using ClientSQLArrayCallback =
    snap::valdi_modules::client_sql::ClientSQLNativeConnectionProxy::QueryCallbackFn;

struct ClientSQLTransactionCompletionState {
    std::atomic_bool completed{false};
    std::atomic<task_id_t> watchdogTaskId{DispatchQueue::TaskIDNull};
};

void notifyClientSQLCallbackSuccess(
    const ClientSQLValueCallback& callback,
    Value value) {
    callback(std::move(value), std::nullopt);
}

void notifyClientSQLCallbackError(const ClientSQLValueCallback& callback, Error error) {
    callback(Value::undefined(), error.getMessage());
}

void notifyClientSQLCallbackSuccess(
    const ClientSQLArrayCallback& callback,
    Value value) {
    auto array = value.getArrayRef();
    if (array == nullptr) {
        callback(std::nullopt, std::nullopt);
        return;
    }
    callback(std::vector<Value>(array->begin(), array->end()), std::nullopt);
}

void notifyClientSQLCallbackError(const ClientSQLArrayCallback& callback, Error error) {
    callback(std::nullopt, error.getMessage());
}

Value makeClientSQLParameters(std::optional<std::vector<Value>> parameters) {
    if (!parameters.has_value()) {
        return Value::undefined();
    }
    return Value(ValueArray::make(std::move(parameters.value())));
}

class ClientSQLConnection;
class ClientSQLTransaction;

class ClientSQLDatabaseCoordinator : public SharedPtrRefCountable {
public:
    ClientSQLDatabaseCoordinator(const Ref<IDispatchQueue>& resourceReleaseQueue,
                                 std::chrono::milliseconds transactionTimeout);
    ~ClientSQLDatabaseCoordinator() override;

    bool hasWriterQueue() const;
    Ref<ClientSQLConnection> openHandle(ClientSQLOpenRequest request);
    void execute(const Ref<ClientSQLConnection>& connection, ClientSQLValueCallback callback, ClientSQLRequest request);
    void query(const Ref<ClientSQLConnection>& connection, ClientSQLArrayCallback callback, ClientSQLRequest request);
    void queryOnWriter(
        const Ref<ClientSQLConnection>& connection,
        ClientSQLArrayCallback callback,
        ClientSQLRequest request);
    void transaction(
        const Ref<ClientSQLConnection>& connection,
        snap::valdi_modules::client_sql::ClientSQLNativeConnectionProxy::TransactionBodyFn body,
        ClientSQLValueCallback callback);
    void debugInfo(const Ref<ClientSQLConnection>& connection, ClientSQLValueCallback callback);
    void executeInTransaction(const Ref<ClientSQLConnection>& connection,
                              uint64_t transactionId,
                              ClientSQLValueCallback callback,
                              ClientSQLRequest request);
    void queryInTransaction(const Ref<ClientSQLConnection>& connection,
                            uint64_t transactionId,
                            ClientSQLArrayCallback callback,
                            ClientSQLRequest request);
    void finishTransaction(
        uint64_t transactionId,
        ClientSQLValueCallback callback,
        Value value,
        std::optional<StringBox> error);
    void closeHandle(const Ref<ClientSQLConnection>& connection, ClientSQLValueCallback callback);
    void abandonHandle();
    bool isTransactionOwner(const ClientSQLConnection* connection) const;

private:
    Result<Void> ensureOpen(const ClientSQLOpenRequest& request);
    Ref<ClientSQLQueueConnection> nextReaderConnection();
    void closeAllConnectionsOnWriterThread();
    void enqueueWriterWork(DispatchFunction work, bool allowDuringTransaction);
    void runWriterWorkOnWriterThread(DispatchFunction work, bool allowDuringTransaction);
    void drainDeferredWriterWorkOnWriterThread();
    void finishTransactionOnWriterThread(
        uint64_t transactionId,
        ClientSQLValueCallback callback,
        Value value,
        std::optional<StringBox> error);
    bool hasActiveTransaction(uint64_t transactionId) const;

    Ref<ClientSQLQueueConnection> _writerConnection;
    std::vector<Ref<ClientSQLQueueConnection>> _readerConnections;
    Ref<IDispatchQueue> _resourceReleaseQueue;
    std::chrono::milliseconds _transactionTimeout;
    std::atomic<size_t> _activeHandles{0};
    std::atomic<size_t> _nextReader{0};
    std::atomic_bool _readerConnectionsReady{false};
    uint64_t _activeTransactionId = 0;
    std::atomic<ClientSQLConnection*> _activeTransactionConnection{nullptr};
    uint64_t _nextTransactionId = 1;
    std::deque<DispatchFunction> _deferredWriterWork;
    std::optional<std::string> _schemaFingerprint;
};

class ClientSQLConnection final
    : public snap::valdi_modules::client_sql::ClientSQLNativeConnectionProxy {
public:
    explicit ClientSQLConnection(Ref<ClientSQLDatabaseCoordinator> coordinator)
        : _coordinator(std::move(coordinator)) {}
    ~ClientSQLConnection() override {
        if (_closed.exchange(true)) {
            return;
        }
        auto coordinator = releaseCoordinator();
        if (coordinator != nullptr) {
            coordinator->abandonHandle();
        }
    }

    bool isClosed() const {
        return _closed.load();
    }

    void markOpenSuccess() {
        std::lock_guard<std::mutex> lock(_openMutex);
        _openError = Error();
        _openComplete.store(true);
    }

    void markOpenError(Error error) {
        std::lock_guard<std::mutex> lock(_openMutex);
        _openError = std::move(error);
        _openComplete.store(true);
    }

    Result<Void> requireOpen() const {
        if (!_openComplete.load()) {
            return Error("ClientSQLNative database is not open");
        }

        std::lock_guard<std::mutex> lock(_openMutex);
        if (!_openError.isEmpty()) {
            return _openError;
        }
        return Void();
    }

    void execute(
        StringBox sql,
        std::optional<std::vector<Value>> parameters,
        ExecuteCallbackFn callback) final {
        if (isClosed()) {
            notifyClientSQLCallbackError(callback, Error("ClientSQLNative connection is closed"));
            return;
        }

        auto coordinator = coordinatorRef();
        if (coordinator == nullptr) {
            notifyClientSQLCallbackError(callback, Error("ClientSQLNative connection is closed"));
            return;
        }
        if (coordinator->isTransactionOwner(this)) {
            notifyClientSQLCallbackError(
                callback,
                Error("ClientSQLNative parent connection execute is not allowed inside its transaction body"));
            return;
        }
        coordinator->execute(
            strongSmallRef(this),
            std::move(callback),
            ClientSQLRequest{
                .sql = std::move(sql),
                .parameters = makeClientSQLParameters(std::move(parameters))});
    }

    void query(
        StringBox sql,
        std::optional<std::vector<Value>> parameters,
        QueryCallbackFn callback) final {
        if (isClosed()) {
            notifyClientSQLCallbackError(callback, Error("ClientSQLNative connection is closed"));
            return;
        }

        auto coordinator = coordinatorRef();
        if (coordinator == nullptr) {
            notifyClientSQLCallbackError(callback, Error("ClientSQLNative connection is closed"));
            return;
        }
        if (coordinator->isTransactionOwner(this)) {
            notifyClientSQLCallbackError(
                callback,
                Error("ClientSQLNative parent connection query is not allowed inside its transaction body"));
            return;
        }
        coordinator->query(
            strongSmallRef(this),
            std::move(callback),
            ClientSQLRequest{
                .sql = std::move(sql),
                .parameters = makeClientSQLParameters(std::move(parameters))});
    }

    void queryOnWriter(
        StringBox sql,
        std::optional<std::vector<Value>> parameters,
        QueryOnWriterCallbackFn callback) final {
        if (isClosed()) {
            notifyClientSQLCallbackError(callback, Error("ClientSQLNative connection is closed"));
            return;
        }

        auto coordinator = coordinatorRef();
        if (coordinator == nullptr) {
            notifyClientSQLCallbackError(callback, Error("ClientSQLNative connection is closed"));
            return;
        }
        if (coordinator->isTransactionOwner(this)) {
            notifyClientSQLCallbackError(
                callback,
                Error("ClientSQLNative parent connection query is not allowed inside its transaction body"));
            return;
        }
        coordinator->queryOnWriter(
            strongSmallRef(this),
            std::move(callback),
            ClientSQLRequest{
                .sql = std::move(sql),
                .parameters = makeClientSQLParameters(std::move(parameters))});
    }

    void transaction(TransactionBodyFn body, TransactionCallbackFn callback) final {
        if (isClosed()) {
            notifyClientSQLCallbackError(callback, Error("ClientSQLNative connection is closed"));
            return;
        }

        auto coordinator = coordinatorRef();
        if (coordinator == nullptr) {
            notifyClientSQLCallbackError(callback, Error("ClientSQLNative connection is closed"));
            return;
        }
        if (coordinator->isTransactionOwner(this)) {
            notifyClientSQLCallbackError(
                callback,
                Error("ClientSQLNative nested parent connection transaction is not allowed inside a transaction body"));
            return;
        }
        coordinator->transaction(strongSmallRef(this), std::move(body), std::move(callback));
    }

    void debugInfo(DebugInfoCallbackFn callback) final {
        auto coordinator = coordinatorRef();
        if (coordinator == nullptr) {
            notifyClientSQLCallbackError(callback, Error("ClientSQLNative connection is closed"));
            return;
        }
        coordinator->debugInfo(strongSmallRef(this), std::move(callback));
    }

    void close(CloseCallbackFn callback) final {
        if (_closed.load()) {
            notifyClientSQLCallbackSuccess(callback, Value::undefined());
            return;
        }

        auto coordinator = coordinatorRef();
        if (coordinator == nullptr) {
            notifyClientSQLCallbackSuccess(callback, Value::undefined());
            return;
        }
        if (coordinator->isTransactionOwner(this)) {
            notifyClientSQLCallbackError(
                callback,
                Error("ClientSQLNative parent connection close is not allowed inside its transaction body"));
            return;
        }
        if (_closed.exchange(true)) {
            notifyClientSQLCallbackSuccess(callback, Value::undefined());
            return;
        }
        coordinator = releaseCoordinator();
        if (coordinator == nullptr) {
            notifyClientSQLCallbackSuccess(callback, Value::undefined());
            return;
        }
        coordinator->closeHandle(strongSmallRef(this), std::move(callback));
    }

private:
    Ref<ClientSQLDatabaseCoordinator> coordinatorRef() const {
        std::lock_guard<std::mutex> lock(_coordinatorMutex);
        return _coordinator;
    }

    Ref<ClientSQLDatabaseCoordinator> releaseCoordinator() {
        std::lock_guard<std::mutex> lock(_coordinatorMutex);
        auto coordinator = std::move(_coordinator);
        _coordinator = nullptr;
        return coordinator;
    }

    mutable std::mutex _coordinatorMutex;
    Ref<ClientSQLDatabaseCoordinator> _coordinator;
    mutable std::mutex _openMutex;
    Error _openError;
    std::atomic_bool _openComplete{false};
    std::atomic_bool _closed{false};
};

class ClientSQLTransaction final
    : public snap::valdi_modules::client_sql::ClientSQLNativeTransactionProxy {
public:
    ClientSQLTransaction(Ref<ClientSQLDatabaseCoordinator> coordinator,
                         Ref<ClientSQLConnection> connection,
                         uint64_t transactionId)
        : _coordinator(std::move(coordinator)),
          _connection(std::move(connection)),
          _transactionId(transactionId) {}
    ~ClientSQLTransaction() override = default;

    void execute(
        StringBox sql,
        std::optional<std::vector<Value>> parameters,
        ExecuteCallbackFn callback) final {
        _coordinator->executeInTransaction(
            _connection,
            _transactionId,
            std::move(callback),
            ClientSQLRequest{
                .sql = std::move(sql),
                .parameters = makeClientSQLParameters(std::move(parameters))});
    }

    void query(
        StringBox sql,
        std::optional<std::vector<Value>> parameters,
        QueryCallbackFn callback) final {
        _coordinator->queryInTransaction(
            _connection,
            _transactionId,
            std::move(callback),
            ClientSQLRequest{
                .sql = std::move(sql),
                .parameters = makeClientSQLParameters(std::move(parameters))});
    }

private:
    Ref<ClientSQLDatabaseCoordinator> _coordinator;
    Ref<ClientSQLConnection> _connection;
    uint64_t _transactionId;
};

ClientSQLDatabaseCoordinator::ClientSQLDatabaseCoordinator(
    const Ref<IDispatchQueue>& resourceReleaseQueue,
    std::chrono::milliseconds transactionTimeout)
    : _resourceReleaseQueue(resourceReleaseQueue), _transactionTimeout(transactionTimeout) {
    gClientSQLLiveCoordinatorCount.fetch_add(1);
    auto writerQueue = createClientSQLDedicatedQueue(STRING_LITERAL("Valdi ClientSQL Writer"));
    if (writerQueue != nullptr) {
        _writerConnection = makeShared<ClientSQLQueueConnection>(std::move(writerQueue));
    }

    _readerConnections.reserve(kClientSQLReaderConnectionCount);
    for (size_t index = 0; index < kClientSQLReaderConnectionCount; ++index) {
        auto readerQueue = createClientSQLDedicatedQueue(STRING_LITERAL("Valdi ClientSQL Reader"));
        if (readerQueue != nullptr) {
            _readerConnections.emplace_back(makeShared<ClientSQLQueueConnection>(std::move(readerQueue)));
        }
    }
}

ClientSQLDatabaseCoordinator::~ClientSQLDatabaseCoordinator() {
    gClientSQLLiveCoordinatorCount.fetch_sub(1);

    auto writerConnection = std::move(_writerConnection);
    auto readerConnections = std::move(_readerConnections);
    const auto* currentQueue = DispatchQueue::getCurrent();
    const auto isDestroyingOnOwnedQueue = currentQueue != nullptr &&
        ((writerConnection != nullptr && currentQueue == writerConnection->queue.get()) ||
         std::any_of(
             readerConnections.begin(), readerConnections.end(), [currentQueue](const auto& readerConnection) {
                 return currentQueue == readerConnection->queue.get();
             }));
    // The final coordinator ref is commonly released by one of its own queue
    // tasks. Transfer the queue-owning refs before that task frame unwinds.
    if (isDestroyingOnOwnedQueue && _resourceReleaseQueue != nullptr &&
        _resourceReleaseQueue.get() != currentQueue) {
        _resourceReleaseQueue->async(
            [writerConnection = std::move(writerConnection),
             readerConnections = std::move(readerConnections)]() mutable {
                readerConnections.clear();
                writerConnection = nullptr;
            });
    }
}

bool ClientSQLDatabaseCoordinator::hasWriterQueue() const {
    return _writerConnection != nullptr && _writerConnection->queue != nullptr;
}

Ref<ClientSQLConnection> ClientSQLDatabaseCoordinator::openHandle(ClientSQLOpenRequest request) {
    _activeHandles.fetch_add(1);
    auto connection = makeShared<ClientSQLConnection>(strongSmallRef(this));
    enqueueWriterWork([self = strongSmallRef(this), connection, request = std::move(request)]() {
        if (connection->isClosed()) {
            return;
        }

        auto openResult = self->ensureOpen(request);
        if (!openResult) {
            connection->markOpenError(openResult.moveError());
            return;
        }
        connection->markOpenSuccess();
    }, false);
    return connection;
}

void ClientSQLDatabaseCoordinator::enqueueWriterWork(DispatchFunction work, bool allowDuringTransaction) {
    _writerConnection->queue->async(
        [self = strongSmallRef(this), work = std::move(work), allowDuringTransaction]() mutable {
            self->runWriterWorkOnWriterThread(std::move(work), allowDuringTransaction);
        });
}

void ClientSQLDatabaseCoordinator::runWriterWorkOnWriterThread(DispatchFunction work, bool allowDuringTransaction) {
    if (!allowDuringTransaction && _activeTransactionId != 0) {
        _deferredWriterWork.emplace_back(std::move(work));
        return;
    }

    work();
}

void ClientSQLDatabaseCoordinator::drainDeferredWriterWorkOnWriterThread() {
    while (_activeTransactionId == 0 && !_deferredWriterWork.empty()) {
        auto work = std::move(_deferredWriterWork.front());
        _deferredWriterWork.pop_front();
        work();
    }
}

Result<Void> ClientSQLDatabaseCoordinator::ensureOpen(const ClientSQLOpenRequest& request) {
    const auto requestFingerprint = schemaFingerprint(request);
    if (_schemaFingerprint.has_value() && _schemaFingerprint.value() != requestFingerprint) {
        return Error(STRING_FORMAT(
            "ClientSQLNative database '{}' is already open with a different schema fingerprint",
            request.name));
    }

    if (_writerConnection->database == nullptr) {
        auto writerOpenResult = openWriterDatabase(_writerConnection, request);
        if (!writerOpenResult) {
            return writerOpenResult.moveError();
        }
        _writerConnection->openError = Error();
        _readerConnectionsReady.store(false);
    } else {
        auto currentVersion = readUserVersion(_writerConnection->database.get());
        if (!currentVersion) {
            return currentVersion.moveError();
        }

        auto schemaResult =
            applySchema(_writerConnection->database.get(),
                        currentVersion.value(),
                        request.schemaVersion,
                        request.createStatements,
                        request.migrations);
        if (!schemaResult) {
            return schemaResult.moveError();
        }
    }

    _schemaFingerprint = requestFingerprint;

    if (!_readerConnections.empty() && !_readerConnectionsReady.load()) {
        auto readerOpenResult = openReaderDatabases(_readerConnections, request);
        if (readerOpenResult) {
            _readerConnectionsReady.store(true);
        }
    }

    return Void();
}

void ClientSQLDatabaseCoordinator::execute(const Ref<ClientSQLConnection>& connection,
                                           ClientSQLValueCallback callback,
                                           ClientSQLRequest request) {
    enqueueWriterWork(
        [self = strongSmallRef(this), connection, callback = std::move(callback), request = std::move(request)]() {
            if (connection->isClosed()) {
                notifyClientSQLCallbackError(callback, Error("ClientSQLNative connection is closed"));
                return;
            }

            auto openResult = connection->requireOpen();
            if (!openResult) {
                notifyClientSQLCallbackError(callback, openResult.moveError());
                return;
            }

            auto database = self->_writerConnection->requireDatabase();
            if (!database) {
                notifyClientSQLCallbackError(callback, database.moveError());
                return;
            }

            auto result = executeStatement(database.value(), request);
            if (!result) {
                notifyClientSQLCallbackError(callback, result.moveError());
                return;
            }
            notifyClientSQLCallbackSuccess(callback, Value::undefined());
        }, false);
}

void ClientSQLDatabaseCoordinator::query(const Ref<ClientSQLConnection>& connection,
                                         ClientSQLArrayCallback callback,
                                         ClientSQLRequest request) {
    _writerConnection->queue->async(
        [self = strongSmallRef(this), connection, callback = std::move(callback), request = std::move(request)]() {
            if (connection->isClosed()) {
                notifyClientSQLCallbackError(callback, Error("ClientSQLNative connection is closed"));
                return;
            }

            auto openResult = connection->requireOpen();
            if (!openResult) {
                notifyClientSQLCallbackError(callback, openResult.moveError());
                return;
            }

            auto reader = self->nextReaderConnection();
            if (reader.get() == self->_writerConnection.get()) {
                auto database = reader->requireDatabase();
                if (!database) {
                    notifyClientSQLCallbackError(callback, database.moveError());
                    return;
                }

                auto result = queryStatement(database.value(), request);
                if (!result) {
                    notifyClientSQLCallbackError(callback, result.moveError());
                    return;
                }
                notifyClientSQLCallbackSuccess(callback, result.moveValue());
                return;
            }

            reader->queue->async([connection, callback, request = std::move(request), reader]() {
                if (connection->isClosed()) {
                    notifyClientSQLCallbackError(callback, Error("ClientSQLNative connection is closed"));
                    return;
                }

                auto database = reader->requireDatabase();
                if (!database) {
                    notifyClientSQLCallbackError(callback, database.moveError());
                    return;
                }

                auto result = queryStatement(database.value(), request);
                if (!result) {
                    notifyClientSQLCallbackError(callback, result.moveError());
                    return;
                }
                notifyClientSQLCallbackSuccess(callback, result.moveValue());
            });
        });
}

void ClientSQLDatabaseCoordinator::queryOnWriter(const Ref<ClientSQLConnection>& connection,
                                                 ClientSQLArrayCallback callback,
                                                 ClientSQLRequest request) {
    enqueueWriterWork(
        [self = strongSmallRef(this), connection, callback = std::move(callback), request = std::move(request)]() {
            if (connection->isClosed()) {
                notifyClientSQLCallbackError(callback, Error("ClientSQLNative connection is closed"));
                return;
            }

            auto openResult = connection->requireOpen();
            if (!openResult) {
                notifyClientSQLCallbackError(callback, openResult.moveError());
                return;
            }

            auto database = self->_writerConnection->requireDatabase();
            if (!database) {
                notifyClientSQLCallbackError(callback, database.moveError());
                return;
            }

            auto result = queryStatement(database.value(), request);
            if (!result) {
                notifyClientSQLCallbackError(callback, result.moveError());
                return;
            }
            notifyClientSQLCallbackSuccess(callback, result.moveValue());
        }, false);
}

void ClientSQLDatabaseCoordinator::transaction(
    const Ref<ClientSQLConnection>& connection,
    snap::valdi_modules::client_sql::ClientSQLNativeConnectionProxy::TransactionBodyFn body,
    ClientSQLValueCallback callback) {
    enqueueWriterWork([self = strongSmallRef(this), connection, body = std::move(body), callback = std::move(callback)]() {
        if (connection->isClosed()) {
            notifyClientSQLCallbackError(callback, Error("ClientSQLNative connection is closed"));
            return;
        }

        auto openResult = connection->requireOpen();
        if (!openResult) {
            notifyClientSQLCallbackError(callback, openResult.moveError());
            return;
        }

        auto database = self->_writerConnection->requireDatabase();
        if (!database) {
            notifyClientSQLCallbackError(callback, database.moveError());
            return;
        }

        Error sqlError;
        if (!execRawSQL(database.value(), "BEGIN TRANSACTION", &sqlError)) {
            notifyClientSQLCallbackError(callback, sqlError);
            return;
        }

        const auto transactionId = self->_nextTransactionId++;
        self->_activeTransactionId = transactionId;
        self->_activeTransactionConnection.store(connection.get());
        auto transaction = makeShared<ClientSQLTransaction>(self, connection, transactionId);
        auto completionState = std::make_shared<ClientSQLTransactionCompletionState>();
        completionState->watchdogTaskId.store(self->_writerConnection->queue->asyncAfter(
            [self, transactionId, callback, completionState]() {
                if (completionState->completed.exchange(true)) {
                    return;
                }
                self->finishTransactionOnWriterThread(
                    transactionId,
                    callback,
                    Value::undefined(),
                    STRING_FORMAT(
                        "ClientSQLNative transaction timed out after {} ms because its completion callback was not invoked",
                        self->_transactionTimeout.count()));
            },
            self->_transactionTimeout));
        snap::valdi_modules::client_sql::ClientSQLNativeConnectionProxy::TransactionBodyCallbackFn doneCallback =
            [self, transactionId, callback, completionState](Value value, std::optional<StringBox> error) {
                if (completionState->completed.exchange(true)) {
                    return;
                }

                self->_writerConnection->queue->cancel(completionState->watchdogTaskId.load());

                self->finishTransaction(
                    transactionId,
                    callback,
                    std::move(value),
                    std::move(error));
            };

        try {
            body(std::move(transaction), std::move(doneCallback));
        } catch (const Exception& error) {
            if (completionState->completed.exchange(true)) {
                return;
            }
            self->_writerConnection->queue->cancel(completionState->watchdogTaskId.load());
            self->finishTransactionOnWriterThread(
                transactionId,
                callback,
                Value::undefined(),
                error.getMessage());
        } catch (const std::exception& error) {
            if (completionState->completed.exchange(true)) {
                return;
            }
            self->_writerConnection->queue->cancel(completionState->watchdogTaskId.load());
            self->finishTransactionOnWriterThread(
                transactionId,
                callback,
                Value::undefined(),
                STRING_FORMAT("ClientSQLNative transaction body threw a native exception: {}", error.what()));
        } catch (...) {
            if (completionState->completed.exchange(true)) {
                return;
            }
            self->_writerConnection->queue->cancel(completionState->watchdogTaskId.load());
            self->finishTransactionOnWriterThread(
                transactionId,
                callback,
                Value::undefined(),
                STRING_LITERAL("ClientSQLNative transaction body threw an unknown native exception"));
        }
    }, false);
}

void ClientSQLDatabaseCoordinator::debugInfo(
    const Ref<ClientSQLConnection>& connection,
    ClientSQLValueCallback callback) {
    enqueueWriterWork(
        [self = strongSmallRef(this), connection, callback = std::move(callback)]() {
            auto info = makeShared<ValueMap>();
            (*info)[STRING_LITERAL("activeHandles")] =
                Value(static_cast<int32_t>(self->_activeHandles.load()));
            (*info)[STRING_LITERAL("activeTransaction")] = Value(self->_activeTransactionId != 0);
            (*info)[STRING_LITERAL("deferredWriterWork")] =
                Value(static_cast<int32_t>(self->_deferredWriterWork.size()));
            (*info)[STRING_LITERAL("readerConnections")] =
                Value(static_cast<int32_t>(self->_readerConnections.size()));
            (*info)[STRING_LITERAL("readerConnectionsReady")] = Value(self->_readerConnectionsReady.load());
            (*info)[STRING_LITERAL("writerOpen")] = Value(self->_writerConnection->database != nullptr);
            (*info)[STRING_LITERAL("connectionClosed")] = Value(connection->isClosed());
            (*info)[STRING_LITERAL("liveCoordinators")] =
                Value(static_cast<int32_t>(gClientSQLLiveCoordinatorCount.load()));
            (*info)[STRING_LITERAL("sqliteVersionNumber")] = Value(sqlite3_libversion_number());
            notifyClientSQLCallbackSuccess(callback, Value(info));
        },
        true);
}

bool ClientSQLDatabaseCoordinator::hasActiveTransaction(uint64_t transactionId) const {
    return _activeTransactionId == transactionId;
}

bool ClientSQLDatabaseCoordinator::isTransactionOwner(const ClientSQLConnection* connection) const {
    return connection != nullptr && _activeTransactionConnection.load() == connection;
}

void ClientSQLDatabaseCoordinator::executeInTransaction(const Ref<ClientSQLConnection>& connection,
                                                        uint64_t transactionId,
                                                        ClientSQLValueCallback callback,
                                                        ClientSQLRequest request) {
    enqueueWriterWork(
        [self = strongSmallRef(this),
         connection,
         transactionId,
         callback = std::move(callback),
         request = std::move(request)]() {
            if (!self->hasActiveTransaction(transactionId)) {
                notifyClientSQLCallbackError(callback, Error("ClientSQLNative transaction is no longer active"));
                return;
            }

            if (connection->isClosed()) {
                notifyClientSQLCallbackError(callback, Error("ClientSQLNative connection is closed"));
                return;
            }

            auto openResult = connection->requireOpen();
            if (!openResult) {
                notifyClientSQLCallbackError(callback, openResult.moveError());
                return;
            }

            auto database = self->_writerConnection->requireDatabase();
            if (!database) {
                notifyClientSQLCallbackError(callback, database.moveError());
                return;
            }

            auto result = executeStatement(database.value(), request);
            if (!result) {
                notifyClientSQLCallbackError(callback, result.moveError());
                return;
            }
            notifyClientSQLCallbackSuccess(callback, Value::undefined());
        },
        true);
}

void ClientSQLDatabaseCoordinator::queryInTransaction(const Ref<ClientSQLConnection>& connection,
                                                      uint64_t transactionId,
                                                      ClientSQLArrayCallback callback,
                                                      ClientSQLRequest request) {
    enqueueWriterWork(
        [self = strongSmallRef(this),
         connection,
         transactionId,
         callback = std::move(callback),
         request = std::move(request)]() {
            if (!self->hasActiveTransaction(transactionId)) {
                notifyClientSQLCallbackError(callback, Error("ClientSQLNative transaction is no longer active"));
                return;
            }

            if (connection->isClosed()) {
                notifyClientSQLCallbackError(callback, Error("ClientSQLNative connection is closed"));
                return;
            }

            auto openResult = connection->requireOpen();
            if (!openResult) {
                notifyClientSQLCallbackError(callback, openResult.moveError());
                return;
            }

            auto database = self->_writerConnection->requireDatabase();
            if (!database) {
                notifyClientSQLCallbackError(callback, database.moveError());
                return;
            }

            auto result = queryStatement(database.value(), request);
            if (!result) {
                notifyClientSQLCallbackError(callback, result.moveError());
                return;
            }
            notifyClientSQLCallbackSuccess(callback, result.moveValue());
        },
        true);
}

void ClientSQLDatabaseCoordinator::finishTransaction(uint64_t transactionId,
                                                     ClientSQLValueCallback callback,
                                                     Value value,
                                                     std::optional<StringBox> error) {
    _writerConnection->queue->async(
        [self = strongSmallRef(this),
         transactionId,
         callback = std::move(callback),
         value = std::move(value),
         error = std::move(error)]() mutable {
            self->finishTransactionOnWriterThread(transactionId, callback, std::move(value), std::move(error));
        });
}

void ClientSQLDatabaseCoordinator::finishTransactionOnWriterThread(uint64_t transactionId,
                                                                   ClientSQLValueCallback callback,
                                                                   Value value,
                                                                   std::optional<StringBox> error) {
    if (!hasActiveTransaction(transactionId)) {
        return;
    }

    auto database = _writerConnection->requireDatabase();
    if (!database) {
        _activeTransactionId = 0;
        _activeTransactionConnection.store(nullptr);
        notifyClientSQLCallbackError(callback, database.moveError());
        drainDeferredWriterWorkOnWriterThread();
        return;
    }

    Error sqlError;
    if (!error.has_value()) {
        if (execRawSQL(database.value(), "COMMIT", &sqlError)) {
            _activeTransactionId = 0;
            _activeTransactionConnection.store(nullptr);
            notifyClientSQLCallbackSuccess(callback, std::move(value));
            drainDeferredWriterWorkOnWriterThread();
            return;
        }

        Error rollbackError;
        execRawSQL(database.value(), "ROLLBACK", &rollbackError);
        _activeTransactionId = 0;
        _activeTransactionConnection.store(nullptr);
        notifyClientSQLCallbackError(callback, sqlError);
        drainDeferredWriterWorkOnWriterThread();
        return;
    }

    if (!execRawSQL(database.value(), "ROLLBACK", &sqlError)) {
        _activeTransactionId = 0;
        _activeTransactionConnection.store(nullptr);
        notifyClientSQLCallbackError(callback, sqlError);
        drainDeferredWriterWorkOnWriterThread();
        return;
    }

    _activeTransactionId = 0;
    _activeTransactionConnection.store(nullptr);
    notifyClientSQLCallbackError(callback, Error(std::move(error.value())));
    drainDeferredWriterWorkOnWriterThread();
}

void ClientSQLDatabaseCoordinator::closeHandle(const Ref<ClientSQLConnection>& connection,
                                               ClientSQLValueCallback callback) {
    enqueueWriterWork([self = strongSmallRef(this), connection, callback = std::move(callback)]() {
        (void)connection.get();
        if (self->_activeHandles.fetch_sub(1) == 1) {
            self->closeAllConnectionsOnWriterThread();
        }
        notifyClientSQLCallbackSuccess(callback, Value::undefined());
    }, false);
}

void ClientSQLDatabaseCoordinator::abandonHandle() {
    enqueueWriterWork([self = strongSmallRef(this)]() {
        if (self->_activeHandles.fetch_sub(1) == 1) {
            self->closeAllConnectionsOnWriterThread();
        }
    }, false);
}

Ref<ClientSQLQueueConnection> ClientSQLDatabaseCoordinator::nextReaderConnection() {
    if (_readerConnections.empty() || !_readerConnectionsReady.load()) {
        return _writerConnection;
    }
    const auto index = _nextReader.fetch_add(1) % _readerConnections.size();
    return _readerConnections[index];
}

void ClientSQLDatabaseCoordinator::closeAllConnectionsOnWriterThread() {
    _writerConnection->database.reset();
    _writerConnection->openError = Error();
    _readerConnectionsReady.store(false);
    for (const auto& reader : _readerConnections) {
        auto resetReader = [reader]() {
            reader->database.reset();
            reader->openError = Error();
        };
        if (reader->queue.get() == _writerConnection->queue.get()) {
            resetReader();
        } else {
            reader->queue->sync(resetReader);
        }
    }
}

std::vector<std::pair<int32_t, Ref<ValueArray>>> sortedMigrations(const Ref<ValueArray>& migrations) {
    std::vector<std::pair<int32_t, Ref<ValueArray>>> out;
    for (const auto& migrationValue : *migrations) {
        auto version = migrationValue.getMapValue("version").toInt();
        auto statements = migrationValue.getMapValue("statements").getArrayRef();
        if (statements != nullptr) {
            out.emplace_back(version, statements);
        }
    }

    std::sort(out.begin(), out.end(), [](const auto& lhs, const auto& rhs) { return lhs.first < rhs.first; });
    return out;
}

Result<Void> applySchema(sqlite3* database,
                         int32_t currentVersion,
                         int32_t targetVersion,
                         const Ref<ValueArray>& createStatements,
                         const Ref<ValueArray>& migrations) {
    if (currentVersion > targetVersion) {
        return Error(STRING_FORMAT(
            "ClientSQLNative database schema version {} is newer than requested version {}",
            currentVersion,
            targetVersion));
    }
    if (currentVersion == targetVersion) {
        return Void();
    }

    Error sqlError;
    if (!execRawSQL(database, "BEGIN TRANSACTION", &sqlError)) {
        return sqlError;
    }

    auto rollback = [&database]() {
        Error ignoredError;
        execRawSQL(database, "ROLLBACK", &ignoredError);
    };

    if (currentVersion == 0) {
        for (const auto& statement : *createStatements) {
            if (!execRawSQL(database, statement.toStringBox().toStringView(), &sqlError)) {
                rollback();
                return sqlError;
            }
        }
    } else {
        auto expectedVersion = currentVersion + 1;
        for (const auto& migration : sortedMigrations(migrations)) {
            if (migration.first <= currentVersion || migration.first > targetVersion) {
                continue;
            }
            if (migration.first != expectedVersion) {
                rollback();
                return Error(STRING_FORMAT(
                    "ClientSQLNative missing migration for schema version {}", expectedVersion));
            }
            for (const auto& statement : *migration.second) {
                if (!execRawSQL(database, statement.toStringBox().toStringView(), &sqlError)) {
                    rollback();
                    return sqlError;
                }
            }
            expectedVersion++;
        }
        if (expectedVersion <= targetVersion) {
            rollback();
            return Error(STRING_FORMAT(
                "ClientSQLNative missing migration for schema version {}", expectedVersion));
        }
    }

    if (!execRawSQL(database, STRING_FORMAT("PRAGMA user_version = {}", targetVersion).toStringView(), &sqlError)) {
        rollback();
        return sqlError;
    }
    if (!execRawSQL(database, "COMMIT", &sqlError)) {
        rollback();
        return sqlError;
    }

    return Void();
}

Result<Void> openWriterDatabase(const Ref<ClientSQLQueueConnection>& connection, const ClientSQLOpenRequest& request) {
    if (!DiskUtils::isDirectory(request.databasePath.databaseRoot) &&
        !DiskUtils::makeDirectory(request.databasePath.databaseRoot, true)) {
        return Error(STRING_FORMAT(
            "Could not create ClientSQLNative database directory '{}'",
            request.databasePath.databaseRoot.toString()));
    }

    auto writerDatabase =
        openSQLiteDatabase(request.databasePath.databasePath, request.name, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, true);
    if (!writerDatabase) {
        return writerDatabase.moveError();
    }

    auto currentVersion = readUserVersion(writerDatabase.value().get());
    if (!currentVersion) {
        return currentVersion.moveError();
    }

    auto schemaResult =
        applySchema(writerDatabase.value().get(),
                    currentVersion.value(),
                    request.schemaVersion,
                    request.createStatements,
                    request.migrations);
    if (!schemaResult) {
        return schemaResult.moveError();
    }

    connection->database = writerDatabase.moveValue();
    return Void();
}

Result<Void> openReaderDatabases(const std::vector<Ref<ClientSQLQueueConnection>>& readerConnections,
                                 const ClientSQLOpenRequest& request) {
    for (const auto& readerConnection : readerConnections) {
        auto readerDatabase =
            openSQLiteDatabase(request.databasePath.databasePath, request.name, SQLITE_OPEN_READONLY, false);
        if (!readerDatabase) {
            auto error = readerDatabase.moveError();
            readerConnection->openError = error;
            return error;
        }
        readerConnection->openError = Error();
        readerConnection->database = readerDatabase.moveValue();
    }

    return Void();
}

Result<ClientSQLDatabasePath> resolveDatabasePath(const Ref<IDiskCache>& diskCache, const StringBox& name) {
    if (name.isEmpty()) {
        return Error("ClientSQLNative database name cannot be empty");
    }

    const auto nameView = name.toStringView();
    const auto isLowercaseASCIIAlphaNumeric = [](unsigned char byte) {
        return (byte >= 'a' && byte <= 'z') || (byte >= '0' && byte <= '9');
    };
    bool isCanonical = nameView.size() <= 240 &&
        isLowercaseASCIIAlphaNumeric(static_cast<unsigned char>(nameView.front())) &&
        isLowercaseASCIIAlphaNumeric(static_cast<unsigned char>(nameView.back()));
    for (const auto character : nameView) {
        const auto byte = static_cast<unsigned char>(character);
        if (!isLowercaseASCIIAlphaNumeric(byte) && byte != '-' && byte != '_' && byte != '.') {
            isCanonical = false;
            break;
        }
    }
    if (!isCanonical || nameView.find("..") != std::string_view::npos) {
        return Error(STRING_FORMAT("Invalid ClientSQLNative database name '{}'", name));
    }

    if (diskCache == nullptr) {
        return Error("ClientSQLNative requires an owned database storage root");
    }

    Path storageRoot = diskCache->getRootPath();
    storageRoot.normalize();
    if (storageRoot.empty() || !storageRoot.isAbsolute()) {
        return Error("ClientSQLNative database storage root must be nonempty and absolute");
    }

    Path databaseRoot = storageRoot.appending(kClientSQLDirectory);
    databaseRoot.normalize();

    Path databasePath = databaseRoot.appending(name.toStringView());
    databasePath.normalize();

    if (!databasePath.startsWith(databaseRoot)) {
        return Error(STRING_FORMAT("Invalid ClientSQLNative database name '{}'", name));
    }

    return ClientSQLDatabasePath{
        .databasePath = databasePath.toString(),
        .databaseRoot = databaseRoot,
    };
}

} // namespace

class ClientSQLDatabaseRegistry {
public:
    Ref<ClientSQLDatabaseCoordinator> coordinatorForDatabasePath(const std::string& databasePath,
                                                                 const Ref<IDispatchQueue>& resourceReleaseQueue,
                                                                 std::chrono::milliseconds transactionTimeout) {
        std::lock_guard<std::mutex> lock(_mutex);
        for (auto iterator = _coordinators.begin(); iterator != _coordinators.end();) {
            if (iterator->second.expired()) {
                iterator = _coordinators.erase(iterator);
            } else {
                ++iterator;
            }
        }
        auto existing = _coordinators.find(databasePath);
        if (existing != _coordinators.end()) {
            auto coordinator = strongRef(existing->second);
            if (coordinator != nullptr) {
                return coordinator;
            }
            _coordinators.erase(existing);
        }

        auto coordinator = makeShared<ClientSQLDatabaseCoordinator>(
            resourceReleaseQueue, transactionTimeout);
        _coordinators.emplace(databasePath, coordinator.toWeak());
        return coordinator;
    }

private:
    std::mutex _mutex;
    std::unordered_map<std::string, Weak<ClientSQLDatabaseCoordinator>> _coordinators;
};

namespace {

void requireSchemaSQLWithoutEmbeddedNull(const StringBox& sql, const StringBox& description) {
    if (containsEmbeddedNull(sql.toStringView())) {
        throw Exception(STRING_FORMAT(
            "ClientSQLNative {} contains an embedded NUL byte", description));
    }
}

Ref<snap::valdi_modules::client_sql::ClientSQLNativeConnectionProxy> openClientSQLDatabase(
    const Ref<IDiskCache>& diskCache,
    const Ref<IDispatchQueue>& resourceReleaseQueue,
    ClientSQLDatabaseRegistry& databaseRegistry,
    std::chrono::milliseconds transactionTimeout,
    StringBox name,
    double schemaVersion,
    std::vector<StringBox> createStatements,
    std::vector<snap::valdi_modules::client_sql::ClientSQLMigration> migrations) {
    if (!std::isfinite(schemaVersion) || std::trunc(schemaVersion) != schemaVersion ||
        schemaVersion < 1 ||
        schemaVersion > std::numeric_limits<int32_t>::max()) {
        throw Exception("ClientSQLNative schema version must be a positive 32-bit integer");
    }

    for (size_t statementIndex = 0; statementIndex < createStatements.size(); ++statementIndex) {
        requireSchemaSQLWithoutEmbeddedNull(
            createStatements[statementIndex],
            STRING_FORMAT("create statement {}", statementIndex));
    }

    std::unordered_set<int32_t> migrationVersions;
    for (const auto& migration : migrations) {
        const auto rawVersion = static_cast<double>(migration.getVersion());
        if (!std::isfinite(rawVersion) || std::trunc(rawVersion) != rawVersion || rawVersion < 2 ||
            rawVersion > std::numeric_limits<int32_t>::max()) {
            throw Exception("ClientSQLNative migration versions must be 32-bit integers starting at 2");
        }
        const auto migrationVersion = static_cast<int32_t>(rawVersion);
        if (migrationVersion > static_cast<int32_t>(schemaVersion)) {
            throw Exception(STRING_FORMAT(
                "ClientSQLNative migration version {} exceeds schema version {}",
                migrationVersion,
                static_cast<int32_t>(schemaVersion)));
        }
        if (!migrationVersions.emplace(migrationVersion).second) {
            throw Exception(STRING_FORMAT(
                "ClientSQLNative duplicate migration version {}", migrationVersion));
        }
        for (size_t statementIndex = 0; statementIndex < migration.getStatements().size(); ++statementIndex) {
            requireSchemaSQLWithoutEmbeddedNull(
                migration.getStatements()[statementIndex],
                STRING_FORMAT("migration {} statement {}", migrationVersion, statementIndex));
        }
    }

    auto databasePath = resolveDatabasePath(diskCache, name);
    if (!databasePath) {
        throw Exception(databasePath.moveError());
    }

    auto databasePathValue = databasePath.moveValue();
    if (resourceReleaseQueue == nullptr) {
        throw Exception("ClientSQLNative resource release queue is unavailable");
    }
    auto coordinator = databaseRegistry.coordinatorForDatabasePath(
        databasePathValue.databasePath,
        resourceReleaseQueue,
        transactionTimeout);
    if (coordinator == nullptr || !coordinator->hasWriterQueue()) {
        throw Exception("ClientSQLNative writer queue is unavailable");
    }

    std::vector<Value> createStatementValues;
    createStatementValues.reserve(createStatements.size());
    for (auto& statement : createStatements) {
        createStatementValues.emplace_back(std::move(statement));
    }

    std::vector<Value> migrationValues;
    migrationValues.reserve(migrations.size());
    for (auto& migration : migrations) {
        std::vector<Value> statementValues;
        statementValues.reserve(migration.getStatements().size());
        for (const auto& statement : migration.getStatements()) {
            statementValues.emplace_back(statement);
        }

        Value migrationValue;
        migrationValue.setMapValue("version", Value(migration.getVersion()));
        migrationValue.setMapValue("statements", Value(ValueArray::make(std::move(statementValues))));
        migrationValues.emplace_back(std::move(migrationValue));
    }

    ClientSQLOpenRequest openRequest{
        .name = name,
        .databasePath = std::move(databasePathValue),
        .schemaVersion = static_cast<int32_t>(schemaVersion),
        .createStatements = ValueArray::make(std::move(createStatementValues)),
        .migrations = ValueArray::make(std::move(migrationValues)),
    };
    auto connection = coordinator->openHandle(std::move(openRequest));

    return connection;
}

class ClientSQLNativeModule final : public snap::valdi_modules::client_sql::ClientSQLNativeModule {
public:
    ClientSQLNativeModule(Ref<IDiskCache> diskCache,
                          Ref<IDispatchQueue> workerQueue,
                          Ref<IDispatchQueue> resourceReleaseQueue,
                          std::chrono::milliseconds transactionTimeout)
        : _diskCache(std::move(diskCache)),
          _workerQueue(std::move(workerQueue)),
          _resourceReleaseQueue(std::move(resourceReleaseQueue)),
          _transactionTimeout(transactionTimeout) {}

    Ref<snap::valdi_modules::client_sql::ClientSQLNativeConnectionProxy> openDatabase(
        StringBox name,
        double schemaVersion,
        std::vector<StringBox> createStatements,
        std::vector<snap::valdi_modules::client_sql::ClientSQLMigration> migrations) final {
        auto diskCache = _diskCache;
        auto workerQueue = _workerQueue;
        auto resourceReleaseQueue = _resourceReleaseQueue;
        if (workerQueue == nullptr) {
            auto runtime = Runtime::currentRuntime();
            if (runtime == nullptr) {
                throw Exception("ClientSQLNative requires an active Valdi runtime");
            }
            diskCache = runtime->getDiskCache();
            workerQueue = runtime->getWorkerQueue();
        }
        if (resourceReleaseQueue == nullptr) {
            resourceReleaseQueue = workerQueue;
        }
        return openClientSQLDatabase(diskCache,
                                     resourceReleaseQueue,
                                     _databaseRegistry,
                                     _transactionTimeout,
                                     std::move(name),
                                     schemaVersion,
                                     std::move(createStatements),
                                     std::move(migrations));
    }

private:
    Ref<IDiskCache> _diskCache;
    Ref<IDispatchQueue> _workerQueue;
    Ref<IDispatchQueue> _resourceReleaseQueue;
    std::chrono::milliseconds _transactionTimeout;
    ClientSQLDatabaseRegistry _databaseRegistry;
};

} // namespace

ClientSQLNativeModuleFactory::ClientSQLNativeModuleFactory()
    : _transactionTimeout(kClientSQLDefaultTransactionTimeout) {}

ClientSQLNativeModuleFactory::ClientSQLNativeModuleFactory(const Ref<IDiskCache>& diskCache,
                                                           const Ref<IDispatchQueue>& workerQueue)
    : ClientSQLNativeModuleFactory(diskCache, workerQueue, kClientSQLDefaultTransactionTimeout) {}

ClientSQLNativeModuleFactory::ClientSQLNativeModuleFactory(
    const Ref<IDiskCache>& diskCache,
    const Ref<IDispatchQueue>& workerQueue,
    std::chrono::milliseconds transactionTimeout)
    : ClientSQLNativeModuleFactory(
          diskCache, workerQueue, transactionTimeout, workerQueue) {}

ClientSQLNativeModuleFactory::ClientSQLNativeModuleFactory(
    const Ref<IDiskCache>& diskCache,
    const Ref<IDispatchQueue>& workerQueue,
    std::chrono::milliseconds transactionTimeout,
    const Ref<IDispatchQueue>& resourceReleaseQueue)
    : _diskCache(diskCache),
      _workerQueue(workerQueue),
      _resourceReleaseQueue(resourceReleaseQueue),
      _transactionTimeout(transactionTimeout) {
    if (_transactionTimeout <= std::chrono::milliseconds::zero()) {
        _transactionTimeout = std::chrono::milliseconds(1);
    }
}

ClientSQLNativeModuleFactory::~ClientSQLNativeModuleFactory() = default;

Ref<snap::valdi_modules::client_sql::ClientSQLNativeModule> ClientSQLNativeModuleFactory::onLoadModule() {
    return makeShared<ClientSQLNativeModule>(
        _diskCache, _workerQueue, _resourceReleaseQueue, _transactionTimeout);
}

static auto kRegisterModule = Valdi::RegisterModuleFactory::registerTyped<ClientSQLNativeModuleFactory>();

} // namespace Valdi
