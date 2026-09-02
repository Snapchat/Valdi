# SQLite Dependency

Non-Apple ClientSQL builds use SQLite 3.53.4 from Bazel's external `sqlite`
repository. The repository is declared in `bzl/dependencies.bzl` for WORKSPACE
consumers and `MODULE.bazel` for Bzlmod consumers. Apple builds use the system
SQLite framework instead.

Source:

- Official archive: `https://www.sqlite.org/2026/sqlite-autoconf-3530400.tar.gz`
- SHA-256: `0e9483900e92cd5de8fd48d16bf9200145a61f7fd5be542a5ac81d8a9516eb9c`

`sqlite.BUILD` defines the portable C library over the downloaded amalgamation.
Android and default/Linux configurations compile and link that target so the
header and library come from the same hermetic dependency. SQLite remains
module-scoped and is linked only when a target depends on the `client_sql` Valdi
module.

ClientSQL supports SQLite 3.16.0 and newer. That floor covers the table-valued
PRAGMA support used by the generic debugger inspector; WAL itself predates the
floor. Every native connection checks `sqlite3_libversion_number()` before use,
and writer opens execute `PRAGMA journal_mode = WAL` and verify that SQLite
actually returned `wal` rather than assuming the requested mode was accepted.

## Generator validation floor

SQL source compatibility is checked by the separately named
`valdi_clientsql_sqlite_316`
repository and `//compiler/clientsql:sqlite_316_validator`. It is not linked
into the ClientSQL runtime. The validator is built against this primary SQLite
artifact:

- Official archive: `https://www.sqlite.org/2017/sqlite-amalgamation-3160000.zip`
- Official release identity: `https://sqlite.org/releaselog/3_16_0.html`
- SHA-256: `3b5dfb65807e2b17e6463357df848e322badba01dc9a4a1de8fdbb72d448e3b0`
- SQLite version: `3.16.0`
- `SQLITE_SOURCE_ID`: `2017-01-02 11:57:58 04ac0b75b1716541b2b97704f4809cb7ef19cccf`
- SHA-1 of `sqlite3.c`: `e2920fb885569d14197c9b7958e6f1db573ee669`
- License: SQLite public-domain dedication (`LICENSE.md` in this directory)

The version, source ID, and `sqlite3.c` SHA-1 match SQLite's official 3.16.0
release log. Both Bzlmod and WORKSPACE declarations pin the downloaded archive
by SHA-256. The validator checks its linked SQLite identity on every invocation;
the generator then checks the validator's declared identity and incorporates
the executable's SHA-256 into `clientsql -version` and compiler cache metadata.
