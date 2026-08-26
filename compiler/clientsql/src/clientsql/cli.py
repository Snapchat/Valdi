from __future__ import annotations

import argparse
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
    validate_schema_and_queries,
)
from .typescript import write_database_file, write_queries_file, write_types_file


VERSION = "valdi-clientsql 0.2.0"


def main(argv: Sequence[str]) -> int:
    if "-version" in argv or "--version" in argv:
        print(VERSION)
        return 0

    parser = argparse.ArgumentParser(prog="clientsql")
    parser.add_argument("-s", "--source", required=True, help="SQL source directory")
    parser.add_argument("-p", "--package", required=True, help="Database package/name")
    parser.add_argument("-c", "--class", dest="class_name", required=True, help="Database class name")
    parser.add_argument("-m", "--module", required=True, help="Module name")
    parser.add_argument("-o", "--output", required=True, help="Output directory")
    parser.add_argument("-l", "--language", required=True, choices=["typescript"], help="Output language")
    parser.add_argument("-tm", "--type-mapping", dest="type_mapping", help="Optional sql_types.yaml")
    args = parser.parse_args(argv)

    try:
        generate(
            sql_dir=Path(args.source),
            package_name=args.package,
            class_name=args.class_name,
            output_dir=Path(args.output),
            type_mapping=args.type_mapping,
        )
    except ClientSqlError as exc:
        print(f"ClientSQL error: {exc}", file=sys.stderr)
        return 1

    return 0


def generate(
    sql_dir: Path,
    package_name: str,
    class_name: str,
    output_dir: Path,
    type_mapping: Optional[str],
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
    validate_schema_and_queries(create_statements, sql_files)

    for sql_file in sql_files:
        write_types_file(output_dir, sql_file, tables)
        write_queries_file(output_dir, sql_file)

    write_database_file(
        output_dir=output_dir,
        class_name=sanitize_type_name(class_name),
        db_name=package_name,
        sql_files=sql_files,
        create_statements=create_statements,
        migrations=migrations,
    )


def entrypoint() -> None:
    sys.exit(main(sys.argv[1:]))
