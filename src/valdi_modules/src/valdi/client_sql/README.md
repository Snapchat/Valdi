# ClientSQL runtime integration boundaries

ClientSQL's debugger adapter intentionally imports the generic
`valdi_core/src/debugging/DebuggerProvider` contract. That source is supplied by
the reviewed debugger-provider stack and is not duplicated in this optional
lane. Restack this branch on the provider branch before Valdi typecheck or Bazel
validation.

The adapter returns the generic contract's `{ json: string }` result from one
helper. That helper's only input is ClientSQL's bounded, pre-serialized JSON
document (40 KiB maximum, below the generic provider's 48 KiB action-document
limit), and the generic provider validates the document without traversing a
provider-owned object graph. The generic provider owns the exact 128 KiB final
`{ handled, data }` response limit, transport, and request routing; ClientSQL
defines no fallback protocol. Every serialized array and object is capped at
100 items/properties to match the generic provider parser. Truncation is
deterministic and reports its omitted-value count and `collectionValues` or
`objectProperties` reason in the document metadata.

Table actions return each row as a positional value array aligned with the
separately bounded `columns` metadata. Arbitrarily long valid SQL identifiers
therefore remain string values and never become JSON property names, avoiding
the generic provider's 1024-character property-name boundary without reducing
the core ClientSQL identifier contract.

The adapter creates its provider owner with the actual ClientSQL module object
and the stable `client_sql/src/ClientSQLDebug` owner key. The generic provider
binds that owner to Valdi's module-loader hot-reload callback. Reload disposal is
therefore automatic even when `module.path` is absent, and same-key replacement
permanently retires the old owner before the new module registers the one active
`client-sql` provider. ClientSQL does not add a second manual disposal protocol;
closing the final live database only disposes the current registration so the
same loaded module can register again later.

## Native query bounds

Every native `query()`, `queryOnWriter()`, and `transaction.query()` call is
materialized in memory before its callback runs. A result is therefore limited
to 10,000 rows and a conservative 8 MiB allocation estimate that includes row
maps, cells, column labels, and SQLite scalar, text, and blob payload. Queries
that would exceed either ceiling fail with an error asking the caller to add a
`LIMIT` clause or select less data; no partial result is returned.

Transaction bodies must invoke their completion callback exactly once. The
native coordinator rolls back a transaction whose callback has not been
invoked within 30 seconds, releases queued writer work, and reports a timeout;
late or duplicate callback invocations are ignored.

## Query snapshot semantics

`connection.query()` runs on a pool of read-only WAL connections. Each SQL
statement observes the SQLite snapshot established when that reader begins the
statement. If a write transaction is open at that time, the reader does not see
its uncommitted changes; depending on queue scheduling, it may observe any
committed snapshot available before the statement starts. Non-transactional
queries therefore provide neither read-your-writes ordering with an active
transaction nor callback ordering relative to writer work.

Use `transaction.query()` when a transaction must read its own writes. Use
`connection.queryOnWriter()` when a read must be serialized behind pending
writer work; while a transaction is active, that read waits for the transaction
to commit or roll back and then observes the resulting committed state.
