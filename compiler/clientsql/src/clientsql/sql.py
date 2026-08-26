from __future__ import annotations

import re
import sqlite3
import sys
from pathlib import Path
from typing import Dict, Iterable, List, Optional, Sequence, Tuple

from .model import Column, ParamOccurrence, Parameter, Query, SqlFile, Table, ClientSqlError


SQL_IDENTIFIER_PATTERN = r"[`\"\[]?[A-Za-z_][A-Za-z0-9_]*[`\"\]]?"


def load_type_mapping(sql_dir: Path, type_mapping: Optional[str]) -> Dict[str, str]:
    if not type_mapping:
        return {}

    mapping_path = Path(type_mapping)
    if not mapping_path.is_absolute():
        mapping_path = sql_dir / mapping_path

    if not mapping_path.is_file():
        raise ClientSqlError(f"Type mapping file does not exist: {mapping_path}")

    result: Dict[str, str] = {}
    current_key: Optional[str] = None
    for raw_line in mapping_path.read_text(encoding="utf-8").splitlines():
        line = raw_line.rstrip()
        if not line.strip() or line.lstrip().startswith("#"):
            continue
        top_level = re.match(r"^([A-Za-z_][A-Za-z0-9_]*)\s*:\s*$", line)
        if top_level:
            current_key = top_level.group(1).upper()
            continue
        if current_key is None:
            continue
        type_match = re.search(r"\b(?:typescript|ts|type)\s*:\s*['\"]?([^'\"]+)['\"]?", line, re.IGNORECASE)
        if type_match:
            result[current_key] = type_match.group(1).strip()
    return result


def collect_tables(sql_texts: Iterable[str], custom_types: Dict[str, str]) -> Dict[str, Table]:
    tables: Dict[str, Table] = {}
    for sql_text in sql_texts:
        for match in re.finditer(
            r"\bCREATE\s+TABLE\s+(?:IF\s+NOT\s+EXISTS\s+)?([`\"A-Za-z_][`\"A-Za-z0-9_]*)\s*\((.*?)\)\s*;",
            strip_sql_comments(sql_text),
            re.IGNORECASE | re.DOTALL,
        ):
            table_name = clean_identifier(match.group(1))
            body = match.group(2)
            columns = parse_columns(body, custom_types)
            if columns:
                tables[table_name.lower()] = Table(name=table_name, columns=columns)
    return tables


def collect_create_statements(sql_texts: Iterable[str]) -> List[str]:
    table_statements: List[str] = []
    other_statements: List[str] = []
    for sql_text in sql_texts:
        for statement in split_sql_statements(schema_prefix(sql_text)):
            if re.match(
                r"^\s*CREATE\s+(?:UNIQUE\s+)?(?:TABLE|INDEX|VIEW|VIRTUAL\s+TABLE)\b",
                statement,
                re.IGNORECASE,
            ):
                target = table_statements if re.match(
                    r"^\s*CREATE\s+(?:VIRTUAL\s+)?TABLE\b", statement, re.IGNORECASE
                ) else other_statements
                target.append(statement.strip())
    return table_statements + other_statements


def schema_prefix(sql_text: str) -> str:
    lines: List[str] = []
    for raw_line in sql_text.splitlines():
        if re.match(r"^\s*[A-Za-z_][A-Za-z0-9_]*\s*:\s*", raw_line) and not raw_line.lstrip().startswith("--"):
            break
        lines.append(raw_line)
    return "\n".join(lines)


def collect_migrations(sql_dir: Path) -> List[Tuple[int, List[str]]]:
    migration_dir = sql_dir / "migration"
    if not migration_dir.is_dir():
        return []

    migrations: List[Tuple[int, List[str]]] = []
    seen_versions: set[int] = set()
    for path in sorted(migration_dir.glob("*.sqm"), key=migration_sort_key):
        version_match = re.match(r"(\d+)", path.stem)
        if version_match is None:
            raise ClientSqlError(f"Migration filename must start with a version number: {path}")
        version = int(version_match.group(1))
        if version in seen_versions:
            raise ClientSqlError(f"Duplicate migration version {version}: {path}")
        seen_versions.add(version)
        statements = [statement.strip() for statement in split_sql_statements(path.read_text(encoding="utf-8"))]
        migrations.append((version, statements))

    if migrations:
        versions = [version for version, _ in migrations]
        expected_versions = list(range(2, versions[-1] + 1))
        if versions != expected_versions:
            raise ClientSqlError(
                f"Migration versions must be contiguous starting at 2; found {versions}"
            )
    return migrations


def validate_schema_and_queries(create_statements: Sequence[str], sql_files: Sequence[SqlFile]) -> None:
    database = sqlite3.connect(":memory:")
    try:
        database.execute("PRAGMA foreign_keys = ON")
        for statement in create_statements:
            try:
                database.execute(statement)
            except sqlite3.Error as exc:
                raise ClientSqlError(f"Invalid schema statement: {exc}\n{statement}") from exc

        for sql_file in sql_files:
            for query in sql_file.queries:
                try:
                    database.execute(f"EXPLAIN {query.runtime_sql}", [None] * len(query.param_order))
                except sqlite3.Error as exc:
                    raise ClientSqlError(
                        f"Invalid query {sql_file.rel_to_package}:{query.name}: {exc}"
                    ) from exc
    finally:
        database.close()


def migration_sort_key(path: Path) -> Tuple[int, str]:
    match = re.match(r"(\d+)", path.stem)
    return (int(match.group(1)) if match else sys.maxsize, path.name)


def parse_columns(body: str, custom_types: Dict[str, str]) -> List[Column]:
    columns: List[Column] = []
    for part in split_top_level(body, ","):
        tokens = part.strip().split()
        if len(tokens) < 2:
            continue
        if tokens[0].upper() in {"PRIMARY", "FOREIGN", "UNIQUE", "CHECK", "CONSTRAINT"}:
            continue

        name = clean_identifier(tokens[0])
        sql_type = clean_identifier(tokens[1]).upper()
        constraints = " ".join(tokens[2:]).upper()
        nullable = "NOT NULL" not in constraints and "PRIMARY KEY" not in constraints
        ts_type = custom_types.get(sql_type, default_ts_type(sql_type))
        if nullable and ts_type != "any":
            ts_type = f"{ts_type} | null"
        columns.append(Column(name=name, sql_type=sql_type, ts_type=ts_type, nullable=nullable))
    return columns


def parse_sql_file(path: Path, package_dir: Path, tables: Dict[str, Table]) -> SqlFile:
    rel_to_package = path.relative_to(package_dir)
    stem_path = rel_to_package.with_suffix("")
    query_blocks = parse_query_blocks(path.read_text(encoding="utf-8"))
    query_names: set[str] = set()
    for name, _ in query_blocks:
        if name in query_names:
            raise ClientSqlError(f"Duplicate query name '{name}' in {rel_to_package}")
        query_names.add(name)
    queries = [
        analyze_query(name, sql, tables)
        for name, sql in query_blocks
    ]
    return SqlFile(path=path, rel_to_package=rel_to_package, stem_path=stem_path, queries=queries)


def parse_query_blocks(sql_text: str) -> List[Tuple[str, str]]:
    blocks: List[Tuple[str, str]] = []
    current_name: Optional[str] = None
    current_lines: List[str] = []

    def flush() -> None:
        nonlocal current_name, current_lines
        if current_name is not None:
            sql = "\n".join(current_lines).strip()
            if sql:
                blocks.append((current_name, sql))
        current_name = None
        current_lines = []

    for raw_line in sql_text.splitlines():
        label = re.match(r"^\s*([A-Za-z_][A-Za-z0-9_]*)\s*:\s*(.*)$", raw_line)
        if label and not raw_line.lstrip().startswith("--"):
            flush()
            current_name = label.group(1)
            rest = label.group(2).strip()
            if rest:
                current_lines.append(rest)
            continue
        if current_name is not None:
            current_lines.append(raw_line)

    flush()
    return blocks


def analyze_query(name: str, sql: str, tables: Dict[str, Table]) -> Query:
    runtime_sql, param_order = normalize_params(sql)
    params = infer_params(sql, param_order, tables)
    read_tables = tables_read_by_query(sql, tables)
    changed_tables = tables_changed_by_query(sql, tables)
    # Result-shape inference is deliberately conservative. SQLite validates more
    # statement forms below, but generated row types are only promised for the
    # SELECT grammar that infer_result_fields() understands.
    returns_rows = bool(re.match(r"^\s*SELECT\b", sql, re.IGNORECASE))
    result_fields = infer_result_fields(sql, tables) if returns_rows else []
    result_type = f"{sanitize_type_name(name)}Row" if returns_rows else "void"
    query_table = table_for_query(sql, tables)
    if returns_rows:
        star_table = select_star_table(sql, tables)
        if star_table is not None:
            result_type = sanitize_type_name(star_table.name)
    if not returns_rows:
        read_tables = []

    return Query(
        name=name,
        sql=sql.strip(),
        runtime_sql=runtime_sql.strip(),
        param_order=param_order,
        params=params,
        result_type=result_type,
        result_fields=result_fields,
        returns_rows=returns_rows,
        read_tables=read_tables,
        changed_tables=changed_tables,
    )


def normalize_params(sql: str) -> Tuple[str, List[str]]:
    out: List[str] = []
    params: List[str] = []
    index = 0
    i = 0
    quote: Optional[str] = None
    while i < len(sql):
        ch = sql[i]
        if quote:
            out.append(ch)
            if ch == quote:
                if i + 1 < len(sql) and sql[i + 1] == quote:
                    out.append(sql[i + 1])
                    i += 2
                    continue
                quote = None
            i += 1
            continue
        if ch in {"'", '"'}:
            quote = ch
            out.append(ch)
            i += 1
            continue
        if ch == "?":
            param_name = f"p{index}"
            params.append(param_name)
            out.append("?")
            index += 1
            i += 1
            continue
        if ch == ":" and i + 1 < len(sql) and re.match(r"[A-Za-z_]", sql[i + 1]):
            match = re.match(r":([A-Za-z_][A-Za-z0-9_]*)(\?)?", sql[i:])
            if match:
                params.append(match.group(1))
                out.append("?")
                i += len(match.group(0))
                continue
        out.append(ch)
        i += 1
    return "".join(out), params


def infer_params(sql: str, param_order: List[str], tables: Dict[str, Table]) -> List[Parameter]:
    unique_order: List[str] = []
    for name in param_order:
        if name not in unique_order:
            unique_order.append(name)

    insert_types = infer_insert_param_types(sql, param_order, tables)
    update_types = infer_update_param_types(sql, param_order, tables)
    predicate_types = infer_predicate_param_types(sql, param_order, tables)
    limit_types = infer_limit_param_types(sql, param_order)
    nullable_params = infer_nullable_params(sql, param_order)
    params: List[Parameter] = []
    for name in unique_order:
        ts_type = (
            insert_types.get(name)
            or update_types.get(name)
            or predicate_types.get(name)
            or limit_types.get(name)
            or "ClientSQLValue"
        )
        if name in nullable_params:
            ts_type = nullable_type(ts_type)
        params.append(Parameter(name=sanitize_identifier(name), ts_type=ts_type))
    return params


def infer_insert_param_types(sql: str, param_order: List[str], tables: Dict[str, Table]) -> Dict[str, str]:
    match = re.search(
        r"\bINSERT\s+(?:OR\s+\w+\s+)?INTO\s+([A-Za-z_][A-Za-z0-9_]*)\s*\((.*?)\)\s*VALUES\s*\((.*?)\)",
        sql,
        re.IGNORECASE | re.DOTALL,
    )
    if not match:
        return {}

    table = tables.get(match.group(1).lower())
    if table is None:
        return {}

    columns = [clean_identifier(part.strip()) for part in split_top_level(match.group(2), ",")]
    values = [part.strip() for part in split_top_level(match.group(3), ",")]
    result: Dict[str, str] = {}
    for idx, value in enumerate(values):
        if idx >= len(columns):
            continue
        param_name: Optional[str] = None
        named = re.fullmatch(r":([A-Za-z_][A-Za-z0-9_]*)(?:\?)?", value)
        if named:
            param_name = named.group(1)
        elif value == "?" and idx < len(param_order):
            param_name = param_order[idx]
        if param_name is None:
            continue
        column = find_column(table, columns[idx])
        if column:
            result[param_name] = column.ts_type
    return result


def infer_update_param_types(sql: str, param_order: List[str], tables: Dict[str, Table]) -> Dict[str, str]:
    match = re.search(
        r"\bUPDATE\s+([A-Za-z_][A-Za-z0-9_]*)\s+SET\s+(.*?)(?:\s+WHERE\b|\s+ORDER\s+BY\b|\s+LIMIT\b|;|$)",
        sql,
        re.IGNORECASE | re.DOTALL,
    )
    if not match:
        return {}

    table = tables.get(match.group(1).lower())
    if table is None:
        return {}

    set_body = match.group(2)
    search_start = match.start(2)
    result: Dict[str, str] = {}
    for assignment in split_top_level(set_body, ","):
        assignment_start = sql.find(assignment, search_start)
        if assignment_start == -1:
            assignment_start = search_start
        search_start = assignment_start + len(assignment)
        assignment_match = re.match(r"\s*([A-Za-z_][A-Za-z0-9_]*)\s*=\s*(.*?)\s*$", assignment, re.DOTALL)
        if not assignment_match:
            continue
        column = find_column(table, assignment_match.group(1))
        if column is None:
            continue
        expr_start = assignment_start + assignment_match.start(2)
        expr_end = assignment_start + assignment_match.end(2)
        for param_name in param_names_between(sql, expr_start, expr_end, param_order):
            result[param_name] = column.ts_type
    return result


def infer_predicate_param_types(sql: str, param_order: List[str], tables: Dict[str, Table]) -> Dict[str, str]:
    table = table_for_query(sql, tables)
    if table is None:
        return {}

    result: Dict[str, str] = {}
    for match in re.finditer(
        r"\b([A-Za-z_][A-Za-z0-9_]*)\b\s*(?:<=|>=|!=|<>|=|<|>|LIKE|IN)\s*(\?|:[A-Za-z_][A-Za-z0-9_]*\??)",
        sql,
        re.IGNORECASE,
    ):
        column = find_column(table, match.group(1))
        if column is None:
            continue
        param_name = param_name_at(sql, match.start(2), param_order)
        if param_name is not None:
            result[param_name] = column.ts_type
    return result


def infer_limit_param_types(sql: str, param_order: List[str]) -> Dict[str, str]:
    result: Dict[str, str] = {}
    value_pattern = r"(\?|:[A-Za-z_][A-Za-z0-9_]*\??)"
    for match in re.finditer(rf"\bLIMIT\s+{value_pattern}(?:\s*,\s*{value_pattern})?", sql, re.IGNORECASE):
        param_name = param_name_at(sql, match.start(1), param_order)
        if param_name is not None:
            result[param_name] = "number"
        if match.lastindex and match.lastindex >= 2 and match.group(2):
            param_name = param_name_at(sql, match.start(2), param_order)
            if param_name is not None:
                result[param_name] = "number"
    for match in re.finditer(rf"\bOFFSET\s+{value_pattern}", sql, re.IGNORECASE):
        param_name = param_name_at(sql, match.start(1), param_order)
        if param_name is not None:
            result[param_name] = "number"
    return result


def infer_nullable_params(sql: str, param_order: List[str]) -> set[str]:
    occurrences = scan_param_occurrences(sql)
    nullable = {occurrence.name for occurrence in occurrences if occurrence.nullable}
    nullable.update(
        match.group(1)
        for match in re.finditer(r":([A-Za-z_][A-Za-z0-9_]*)\s+IS\s+(?:NOT\s+)?NULL\b", sql, re.IGNORECASE)
    )

    for match in re.finditer(r"\?\s+IS\s+(?:NOT\s+)?NULL\b", sql, re.IGNORECASE):
        param_name = param_name_at(sql, match.start(), param_order)
        if param_name is not None:
            nullable.add(param_name)
    return nullable


def param_names_between(sql: str, start: int, end: int, param_order: List[str]) -> List[str]:
    del param_order
    return [
        occurrence.name
        for occurrence in scan_param_occurrences(sql)
        if start <= occurrence.start and occurrence.end <= end
    ]


def param_name_at(sql: str, start: int, param_order: List[str]) -> Optional[str]:
    del param_order
    for occurrence in scan_param_occurrences(sql):
        if occurrence.start == start:
            return occurrence.name
    return None


def scan_param_occurrences(sql: str) -> List[ParamOccurrence]:
    occurrences: List[ParamOccurrence] = []
    positional_index = 0
    i = 0
    quote: Optional[str] = None
    while i < len(sql):
        ch = sql[i]
        if quote:
            if ch == quote:
                if i + 1 < len(sql) and sql[i + 1] == quote:
                    i += 2
                    continue
                quote = None
            i += 1
            continue
        if ch in {"'", '"'}:
            quote = ch
            i += 1
            continue
        if ch == "?":
            occurrences.append(ParamOccurrence(start=i, end=i + 1, name=f"p{positional_index}", nullable=False))
            positional_index += 1
            i += 1
            continue
        if ch == ":" and i + 1 < len(sql) and re.match(r"[A-Za-z_]", sql[i + 1]):
            match = re.match(r":([A-Za-z_][A-Za-z0-9_]*)(\?)?", sql[i:])
            if match:
                occurrences.append(
                    ParamOccurrence(
                        start=i,
                        end=i + len(match.group(0)),
                        name=match.group(1),
                        nullable=bool(match.group(2)),
                    )
                )
                i += len(match.group(0))
                continue
        i += 1
    return occurrences


def infer_result_fields(sql: str, tables: Dict[str, Table]) -> List[Column]:
    table = table_for_query(sql, tables)
    if table is None:
        return []

    select_match = re.search(r"\bSELECT\s+(.*?)\s+FROM\b", sql, re.IGNORECASE | re.DOTALL)
    if not select_match:
        return []

    selected = select_match.group(1).strip()
    if selected == "*":
        return table.columns

    fields: List[Column] = []
    for part in split_top_level(selected, ","):
        expression = part.strip()
        alias_match = re.search(r"\s+AS\s+([A-Za-z_][A-Za-z0-9_]*)$", expression, re.IGNORECASE)
        if alias_match:
            name = alias_match.group(1)
            source = expression[: alias_match.start()].strip()
        else:
            source = expression
            name = clean_identifier(source.split(".")[-1].strip())

        column = find_column(table, source.split(".")[-1].strip())
        if column:
            fields.append(Column(name=name, sql_type=column.sql_type, ts_type=column.ts_type, nullable=column.nullable))
        else:
            fields.append(Column(name=name, sql_type="ANY", ts_type=infer_expression_type(source), nullable=True))
    return fields


def select_star_table(sql: str, tables: Dict[str, Table]) -> Optional[Table]:
    if not re.search(r"\bSELECT\s+\*\s+FROM\b", sql, re.IGNORECASE | re.DOTALL):
        return None
    return table_for_query(sql, tables)


def table_for_query(sql: str, tables: Dict[str, Table]) -> Optional[Table]:
    for pattern in [
        r"\bFROM\s+([A-Za-z_][A-Za-z0-9_]*)",
        r"\bUPDATE\s+([A-Za-z_][A-Za-z0-9_]*)",
        r"\bINSERT\s+(?:OR\s+\w+\s+)?INTO\s+([A-Za-z_][A-Za-z0-9_]*)",
        r"\bDELETE\s+FROM\s+([A-Za-z_][A-Za-z0-9_]*)",
    ]:
        match = re.search(pattern, sql, re.IGNORECASE)
        if match:
            return tables.get(match.group(1).lower())
    return None


def tables_read_by_query(sql: str, tables: Dict[str, Table]) -> List[str]:
    names: List[str] = []
    pattern = rf"\b(?:FROM|JOIN)\s+({SQL_IDENTIFIER_PATTERN})"
    for match in re.finditer(pattern, sql, re.IGNORECASE):
        table = tables.get(clean_identifier(match.group(1)).lower())
        if table is not None and table.name not in names:
            names.append(table.name)
    return names


def tables_changed_by_query(sql: str, tables: Dict[str, Table]) -> List[str]:
    names: List[str] = []
    patterns = [
        rf"\bUPDATE\s+({SQL_IDENTIFIER_PATTERN})",
        rf"\b(?:INSERT|REPLACE)\s+(?:OR\s+\w+\s+)?INTO\s+({SQL_IDENTIFIER_PATTERN})",
        rf"\bDELETE\s+FROM\s+({SQL_IDENTIFIER_PATTERN})",
    ]
    for pattern in patterns:
        for match in re.finditer(pattern, sql, re.IGNORECASE):
            table = tables.get(clean_identifier(match.group(1)).lower())
            if table is not None and table.name not in names:
                names.append(table.name)
    return names


def find_column(table: Table, name: str) -> Optional[Column]:
    cleaned = clean_identifier(name).lower()
    for column in table.columns:
        if column.name.lower() == cleaned:
            return column
    return None


def infer_expression_type(expression: str) -> str:
    if re.search(r"\bCOUNT\s*\(", expression, re.IGNORECASE):
        return "number"
    if re.search(r"\b(?:SUM|AVG|MIN|MAX)\s*\(", expression, re.IGNORECASE):
        return "number | null"
    return "any"


def default_ts_type(sql_type: str) -> str:
    normalized = sql_type.upper()
    if normalized in {"INT", "INTEGER", "BIGINT", "SMALLINT", "TINYINT"}:
        return "number"
    if normalized in {"REAL", "DOUBLE", "FLOAT", "NUMERIC", "DECIMAL"}:
        return "number"
    if normalized in {"TEXT", "CHAR", "CLOB", "VARCHAR", "NCHAR", "NVARCHAR"}:
        return "string"
    if normalized == "BLOB":
        return "ArrayBuffer"
    if normalized in {"BOOL", "BOOLEAN"}:
        return "boolean"
    return "any"


def nullable_type(ts_type: str) -> str:
    if ts_type == "any" or re.search(r"(?:^|\|\s*)null(?:\s*\||$)", ts_type):
        return ts_type
    return f"{ts_type} | null"


def split_sql_statements(sql_text: str) -> List[str]:
    statements: List[str] = []
    current: List[str] = []
    quote: Optional[str] = None
    i = 0
    while i < len(sql_text):
        ch = sql_text[i]
        current.append(ch)
        if quote:
            if ch == quote:
                if i + 1 < len(sql_text) and sql_text[i + 1] == quote:
                    current.append(sql_text[i + 1])
                    i += 2
                    continue
                quote = None
        elif ch in {"'", '"'}:
            quote = ch
        elif ch == ";":
            statement = "".join(current).strip()
            if statement:
                statements.append(statement)
            current = []
        i += 1

    tail = "".join(current).strip()
    if tail:
        statements.append(tail)
    return statements


def split_top_level(text: str, delimiter: str) -> List[str]:
    parts: List[str] = []
    current: List[str] = []
    depth = 0
    quote: Optional[str] = None
    for ch in text:
        if quote:
            current.append(ch)
            if ch == quote:
                quote = None
            continue
        if ch in {"'", '"'}:
            quote = ch
            current.append(ch)
            continue
        if ch == "(":
            depth += 1
        elif ch == ")":
            depth = max(0, depth - 1)
        if ch == delimiter and depth == 0:
            parts.append("".join(current))
            current = []
        else:
            current.append(ch)
    parts.append("".join(current))
    return parts


def strip_sql_comments(sql_text: str) -> str:
    without_block = re.sub(r"/\*.*?\*/", "", sql_text, flags=re.DOTALL)
    return "\n".join(line.split("--", 1)[0] for line in without_block.splitlines())


def clean_identifier(identifier: str) -> str:
    return identifier.strip().strip("`\"[]")


def sanitize_identifier(name: str) -> str:
    cleaned = re.sub(r"\W+", "_", name)
    if not cleaned or re.match(r"\d", cleaned):
        cleaned = f"p_{cleaned}"
    return cleaned


def sanitize_type_name(name: str) -> str:
    parts = re.split(r"[^A-Za-z0-9]+", name)
    result = "".join(part[:1].upper() + part[1:] for part in parts if part)
    if not result:
        return "Generated"
    if re.match(r"\d", result):
        return f"T{result}"
    return result
