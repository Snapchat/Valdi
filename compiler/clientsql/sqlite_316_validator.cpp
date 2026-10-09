#include <sqlite3.h>

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr char kProtocolHeader[] = "VALDI_CLIENTSQL_SQL_VALIDATOR_V1\n";
constexpr char kExpectedSQLiteVersion[] = "3.16.0";
constexpr int kExpectedSQLiteVersionNumber = 3016000;
constexpr char kExpectedSQLiteSourceId[] =
    "2017-01-02 11:57:58 04ac0b75b1716541b2b97704f4809cb7ef19cccf";
constexpr char kExpectedSQLite3CSha1[] = "e2920fb885569d14197c9b7958e6f1db573ee669";
constexpr std::uint32_t kMaximumStatements = 100000;
constexpr std::uint32_t kMaximumStatementBytes = 16 * 1024 * 1024;
constexpr std::size_t kMaximumRequestBytes = 64 * 1024 * 1024;

struct QueryRequest {
    std::uint32_t parameterCount;
    std::string sql;
};

struct ValidationRequest {
    std::vector<std::string> schemaStatements;
    std::vector<QueryRequest> queries;
    std::vector<std::string> migrationStatements;
};

class RequestReader {
public:
    explicit RequestReader(std::string input) : input_(std::move(input)) {}

    bool readHeader(std::string* error) {
        const std::size_t headerLength = std::strlen(kProtocolHeader);
        if (input_.size() < headerLength || input_.compare(0, headerLength, kProtocolHeader) != 0) {
            *error = "invalid request protocol header";
            return false;
        }
        offset_ = headerLength;
        return true;
    }

    bool readCount(std::uint32_t* value, std::string* error) {
        if (!readUint32(value, error)) {
            return false;
        }
        if (*value > kMaximumStatements) {
            *error = "request contains too many statements";
            return false;
        }
        return true;
    }

    bool readUint32(std::uint32_t* value, std::string* error) {
        if (input_.size() - offset_ < 4) {
            *error = "truncated uint32 in request";
            return false;
        }
        const auto* bytes = reinterpret_cast<const unsigned char*>(input_.data() + offset_);
        *value = (static_cast<std::uint32_t>(bytes[0]) << 24)
            | (static_cast<std::uint32_t>(bytes[1]) << 16)
            | (static_cast<std::uint32_t>(bytes[2]) << 8)
            | static_cast<std::uint32_t>(bytes[3]);
        offset_ += 4;
        return true;
    }

    bool readString(std::string* value, std::string* error) {
        std::uint32_t length = 0;
        if (!readUint32(&length, error)) {
            return false;
        }
        if (length > kMaximumStatementBytes) {
            *error = "SQL statement exceeds validator byte limit";
            return false;
        }
        if (input_.size() - offset_ < length) {
            *error = "truncated SQL statement in request";
            return false;
        }
        value->assign(input_.data() + offset_, length);
        offset_ += length;
        if (value->find('\0') != std::string::npos) {
            *error = "SQL statement contains an embedded NUL";
            return false;
        }
        return true;
    }

    bool consumedAll(std::string* error) const {
        if (offset_ != input_.size()) {
            *error = "request contains trailing bytes";
            return false;
        }
        return true;
    }

private:
    std::string input_;
    std::size_t offset_ = 0;
};

std::string validatorIdentity() {
    return std::string("valdi-clientsql-sqlite-validator protocol=1 sqlite=")
        + kExpectedSQLiteVersion + " sqlite_source_id=" + kExpectedSQLiteSourceId
        + " sqlite3_c_sha1=" + kExpectedSQLite3CSha1;
}

std::string sanitizeError(std::string value) {
    for (char& character : value) {
        if (character == '\n' || character == '\r' || character == '\t') {
            character = ' ';
        }
    }
    return value;
}

int reportError(const char* kind, std::size_t index, const std::string& message) {
    std::cerr << "clientsql-validator-error:" << kind << ':' << index << ':'
              << sanitizeError(message) << '\n';
    return 1;
}

bool verifyLinkedSQLite(std::string* error) {
    if (sqlite3_libversion_number() != kExpectedSQLiteVersionNumber
        || std::strcmp(sqlite3_libversion(), kExpectedSQLiteVersion) != 0
        || std::strcmp(sqlite3_sourceid(), kExpectedSQLiteSourceId) != 0) {
        std::ostringstream stream;
        stream << "validator linked unexpected SQLite " << sqlite3_libversion()
               << " source " << sqlite3_sourceid();
        *error = stream.str();
        return false;
    }
    return true;
}

bool readRequest(ValidationRequest* request, std::string* error) {
    std::ostringstream inputStream;
    inputStream << std::cin.rdbuf();
    std::string input = inputStream.str();
    if (input.size() > kMaximumRequestBytes) {
        *error = "validation request exceeds byte limit";
        return false;
    }

    RequestReader reader(std::move(input));
    if (!reader.readHeader(error)) {
        return false;
    }

    std::uint32_t schemaCount = 0;
    if (!reader.readCount(&schemaCount, error)) {
        return false;
    }
    request->schemaStatements.reserve(schemaCount);
    for (std::uint32_t index = 0; index < schemaCount; ++index) {
        std::string sql;
        if (!reader.readString(&sql, error)) {
            return false;
        }
        request->schemaStatements.push_back(std::move(sql));
    }

    std::uint32_t queryCount = 0;
    if (!reader.readCount(&queryCount, error)) {
        return false;
    }
    request->queries.reserve(queryCount);
    for (std::uint32_t index = 0; index < queryCount; ++index) {
        std::uint32_t parameterCount = 0;
        std::string sql;
        if (!reader.readUint32(&parameterCount, error) || !reader.readString(&sql, error)) {
            return false;
        }
        request->queries.push_back({parameterCount, std::move(sql)});
    }

    std::uint32_t migrationCount = 0;
    if (!reader.readCount(&migrationCount, error)) {
        return false;
    }
    request->migrationStatements.reserve(migrationCount);
    for (std::uint32_t index = 0; index < migrationCount; ++index) {
        std::string sql;
        if (!reader.readString(&sql, error)) {
            return false;
        }
        request->migrationStatements.push_back(std::move(sql));
    }
    return reader.consumedAll(error);
}

bool prepareExactlyOne(
    sqlite3* database,
    const std::string& sql,
    sqlite3_stmt** output,
    std::string* error
) {
    if (sql.empty()) {
        *error = "empty SQL statement";
        return false;
    }
    if (sql.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        *error = "SQL statement exceeds SQLite's input limit";
        return false;
    }

    const char* cursor = sql.data();
    const char* end = cursor + sql.size();
    sqlite3_stmt* prepared = nullptr;
    while (cursor < end) {
        sqlite3_stmt* candidate = nullptr;
        const char* tail = nullptr;
        const int remaining = static_cast<int>(end - cursor);
        const int result = sqlite3_prepare_v2(database, cursor, remaining, &candidate, &tail);
        if (result != SQLITE_OK) {
            if (prepared != nullptr) {
                sqlite3_finalize(prepared);
            }
            *error = sqlite3_errmsg(database);
            return false;
        }
        if (tail == nullptr || tail <= cursor) {
            if (candidate != nullptr) {
                sqlite3_finalize(candidate);
            }
            if (prepared != nullptr) {
                sqlite3_finalize(prepared);
            }
            *error = "SQLite validator made no progress while preparing SQL";
            return false;
        }
        if (candidate != nullptr) {
            if (prepared != nullptr) {
                sqlite3_finalize(candidate);
                sqlite3_finalize(prepared);
                *error = "validation record contains multiple SQL statements";
                return false;
            }
            prepared = candidate;
        }
        cursor = tail;
    }
    if (prepared == nullptr) {
        *error = "SQL record contains no statement";
        return false;
    }
    *output = prepared;
    return true;
}

bool executePrepared(sqlite3* database, sqlite3_stmt* statement, std::string* error) {
    for (;;) {
        const int result = sqlite3_step(statement);
        if (result == SQLITE_ROW) {
            continue;
        }
        if (result == SQLITE_DONE) {
            return true;
        }
        *error = sqlite3_errmsg(database);
        return false;
    }
}

bool startsWith(const std::string& value, const char* prefix) {
    const std::size_t prefixLength = std::strlen(prefix);
    return value.size() >= prefixLength && value.compare(0, prefixLength, prefix) == 0;
}

bool isMigrationResolutionError(const std::string& error) {
    // Migration sources describe older schemas, so resolving them against the
    // declared current schema can legitimately miss a retired table or column.
    // These messages mean SQLite 3.16 completed parsing and reached name/schema
    // resolution. Syntax and floor-semantic errors remain hard failures; this
    // is not a list of SQL features accepted or rejected by ClientSQL.
    return startsWith(error, "no such table:")
        || startsWith(error, "no such column:")
        || startsWith(error, "no such index:")
        || startsWith(error, "duplicate column name:")
        || (startsWith(error, "table ") && error.find(" already exists") != std::string::npos)
        || (startsWith(error, "index ") && error.find(" already exists") != std::string::npos);
}

int validateRequest(const ValidationRequest& request) {
    sqlite3* database = nullptr;
    const int openResult = sqlite3_open_v2(
        ":memory:",
        &database,
        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_MEMORY,
        nullptr
    );
    if (openResult != SQLITE_OK || database == nullptr) {
        const std::string message = database == nullptr ? "could not allocate SQLite database" : sqlite3_errmsg(database);
        if (database != nullptr) {
            sqlite3_close(database);
        }
        return reportError("protocol", 0, message);
    }

    char* pragmaError = nullptr;
    if (sqlite3_exec(database, "PRAGMA foreign_keys = ON", nullptr, nullptr, &pragmaError) != SQLITE_OK) {
        const std::string message = pragmaError == nullptr ? sqlite3_errmsg(database) : pragmaError;
        sqlite3_free(pragmaError);
        sqlite3_close(database);
        return reportError("protocol", 0, message);
    }

    for (std::size_t index = 0; index < request.schemaStatements.size(); ++index) {
        sqlite3_stmt* statement = nullptr;
        std::string error;
        if (!prepareExactlyOne(database, request.schemaStatements[index], &statement, &error)) {
            sqlite3_close(database);
            return reportError("schema", index, error);
        }
        const bool executed = executePrepared(database, statement, &error);
        sqlite3_finalize(statement);
        if (!executed) {
            sqlite3_close(database);
            return reportError("schema", index, error);
        }
    }

    for (std::size_t index = 0; index < request.queries.size(); ++index) {
        const QueryRequest& query = request.queries[index];
        sqlite3_stmt* statement = nullptr;
        std::string error;
        const std::string explainedSql = "EXPLAIN " + query.sql;
        if (!prepareExactlyOne(database, explainedSql, &statement, &error)) {
            sqlite3_close(database);
            return reportError("query", index, error);
        }
        const int parameterCount = sqlite3_bind_parameter_count(statement);
        if (parameterCount < 0 || static_cast<std::uint32_t>(parameterCount) != query.parameterCount) {
            std::ostringstream stream;
            stream << "expected " << query.parameterCount << " parameters but SQLite prepared " << parameterCount;
            sqlite3_finalize(statement);
            sqlite3_close(database);
            return reportError("query", index, stream.str());
        }
        for (int parameter = 1; parameter <= parameterCount; ++parameter) {
            if (sqlite3_bind_null(statement, parameter) != SQLITE_OK) {
                error = sqlite3_errmsg(database);
                sqlite3_finalize(statement);
                sqlite3_close(database);
                return reportError("query", index, error);
            }
        }
        const bool executed = executePrepared(database, statement, &error);
        sqlite3_finalize(statement);
        if (!executed) {
            sqlite3_close(database);
            return reportError("query", index, error);
        }
    }

    for (std::size_t index = 0; index < request.migrationStatements.size(); ++index) {
        sqlite3_stmt* statement = nullptr;
        std::string error;
        if (!prepareExactlyOne(database, request.migrationStatements[index], &statement, &error)) {
            if (!isMigrationResolutionError(error)) {
                sqlite3_close(database);
                return reportError("migration", index, error);
            }
            continue;
        }
        sqlite3_finalize(statement);
    }

    sqlite3_close(database);
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    std::string linkedSQLiteError;
    if (!verifyLinkedSQLite(&linkedSQLiteError)) {
        return reportError("identity", 0, linkedSQLiteError);
    }
    if (argc == 2 && std::strcmp(argv[1], "--version") == 0) {
        std::cout << validatorIdentity() << '\n';
        return 0;
    }
    if (argc != 2 || std::strcmp(argv[1], "--validate") != 0) {
        std::cerr << "usage: sqlite_316_validator --version|--validate\n";
        return 2;
    }

    ValidationRequest request;
    std::string requestError;
    if (!readRequest(&request, &requestError)) {
        return reportError("protocol", 0, requestError);
    }
    return validateRequest(request);
}
