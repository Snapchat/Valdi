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
