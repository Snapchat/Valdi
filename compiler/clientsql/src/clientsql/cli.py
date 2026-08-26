from __future__ import annotations

import argparse
import hashlib
import pkgutil
import sys
from pathlib import Path
from typing import Optional, Sequence

from .model import ClientSqlError
from .sql import (
    collect_create_statements,
    collect_migrations,
    collect_tables,
    load_type_mapping,
    parse_sql_file,
    sanitize_type_name,
    validate_generated_identifiers,
)
from .typescript import write_database_file, write_queries_file, write_types_file
from .validator import SQLite316Validator


SOURCE_FILES = (
    "__init__.py",
    "__main__.py",
    "cli.py",
    "model.py",
    "sql.py",
    "typescript.py",
    "validator.py",
)


def source_digest() -> str:
    digest = hashlib.sha256()
    for source_file in SOURCE_FILES:
        source = pkgutil.get_data("clientsql", source_file)
        if source is None:
            raise RuntimeError(f"Missing ClientSQL generator source '{source_file}'")
        encoded_name = source_file.encode("utf-8")
        digest.update(len(encoded_name).to_bytes(4, "big"))
        digest.update(encoded_name)
        digest.update(len(source).to_bytes(8, "big"))
        digest.update(source)
    return digest.hexdigest()


def generator_version(validator: SQLite316Validator) -> str:
    return (
        f"valdi-clientsql 0.2.0+source.sha256.{source_digest()}"
        f"+validator.{validator.cache_identity}"
    )


def main(argv: Sequence[str]) -> int:
    parser = argparse.ArgumentParser(prog="clientsql")
    parser.add_argument("-version", "--version", action="store_true", help="Print generator identity")
    parser.add_argument(
        "--sqlite-validator",
        help="Path to the hermetic SQLite 3.16.0 validation executable",
    )
    parser.add_argument("-s", "--source", help="SQL source directory")
    parser.add_argument("-p", "--package", help="Database package/name")
    parser.add_argument("-c", "--class", dest="class_name", help="Database class name")
    parser.add_argument("-m", "--module", help="Module name")
    parser.add_argument("-o", "--output", help="Output directory")
    parser.add_argument("-l", "--language", choices=["typescript"], help="Output language")
    parser.add_argument("-tm", "--type-mapping", dest="type_mapping", help="Optional sql_types.yaml")
    args = parser.parse_args(argv)

    try:
        validator = SQLite316Validator.resolve(args.sqlite_validator)
        if args.version:
            print(generator_version(validator))
            return 0
        missing_arguments = [
            option
            for option, value in (
                ("--source", args.source),
                ("--package", args.package),
                ("--class", args.class_name),
                ("--module", args.module),
                ("--output", args.output),
                ("--language", args.language),
            )
            if value is None
        ]
        if missing_arguments:
            parser.error(f"the following arguments are required: {', '.join(missing_arguments)}")
        generate(
            sql_dir=Path(args.source),
            package_name=args.package,
            class_name=args.class_name,
            module_name=args.module,
            output_dir=Path(args.output),
            type_mapping=args.type_mapping,
            validator=validator,
        )
    except ClientSqlError as exc:
        print(f"ClientSQL error: {exc}", file=sys.stderr)
        return 1

    return 0


def generate(
    sql_dir: Path,
    package_name: str,
    class_name: str,
    module_name: str,
    output_dir: Path,
    type_mapping: Optional[str],
    validator: SQLite316Validator,
) -> None:
    package_dir = sql_dir / package_name
    if not package_dir.is_dir():
        raise ClientSqlError(f"SQL package directory does not exist: {package_dir}")

    output_dir.mkdir(parents=True, exist_ok=True)
    custom_types = load_type_mapping(sql_dir, type_mapping)
    sql_paths = sorted(package_dir.rglob("*.sq"))
    if not sql_paths:
        raise ClientSqlError(f"No .sq files found under {package_dir}")

    sql_text_by_path = {path: path.read_text(encoding="utf-8") for path in sql_paths}
    tables = collect_tables(sql_text_by_path.values(), custom_types)
    sql_files = [
        parse_sql_file(path, package_dir, tables)
        for path in sql_paths
    ]

    create_statements = collect_create_statements(sql_text_by_path.values())
    migrations = collect_migrations(sql_dir)
    query_validations = [
        (
            f"query {sql_file.rel_to_package}:{query.name}",
            query.runtime_sql,
            len(query.param_order),
        )
        for sql_file in sql_files
        for query in sql_file.queries
    ]
    migration_validations = [
        (f"migration {version} statement {index + 1}", statement)
        for version, statements in migrations
        for index, statement in enumerate(statements)
    ]
    validator.validate(create_statements, query_validations, migration_validations)
    validate_generated_identifiers(sql_files, tables)

    for sql_file in sql_files:
        write_types_file(output_dir, sql_file, tables)
        write_queries_file(output_dir, sql_file)

    write_database_file(
        output_dir=output_dir,
        class_name=sanitize_type_name(class_name),
        db_name=package_name,
        module_name=module_name,
        sql_files=sql_files,
        create_statements=create_statements,
        migrations=migrations,
    )


def entrypoint() -> None:
    sys.exit(main(sys.argv[1:]))
