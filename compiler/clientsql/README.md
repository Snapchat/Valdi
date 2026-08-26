# ClientSQL generator

This directory contains the canonical, reviewable Python source for Valdi's
SQLDelight-style ClientSQL generator.

The source is divided by responsibility:

- `cli.py` owns command-line parsing and generation orchestration.
- `model.py` defines the schema and query model.
- `sql.py` parses and validates SQL, migrations, parameters, and result shapes.
- `typescript.py` emits generated TypeScript bindings and database classes.

The public Valdi toolchain continues to supply its ClientSQL executable through
the existing `sqldelight_compiler` target. This source package intentionally
does not replace that toolchain binary or check in a generated executable. Use
the Bazel `//compiler/clientsql:clientsql` target for source builds, or create a
deterministic standalone zipapp at an explicit local path:

```bash
python3 compiler/clientsql/package_clientsql.py --output /tmp/clientsql
```

An explicitly supplied executable can be checked against the canonical source:

```bash
python3 compiler/clientsql/package_clientsql.py --output /tmp/clientsql --check
```
