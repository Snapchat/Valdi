from __future__ import annotations

import re
import sys
from pathlib import Path
from typing import Dict, Iterable, List, NamedTuple, Optional, Sequence, Tuple

from .model import Column, ParamOccurrence, Parameter, Query, SqlFile, Table, ClientSqlError


SQL_IDENTIFIER_PATTERN = (
    r"(?:[A-Za-z_][A-Za-z0-9_]*|\"(?:[^\"]|\"\")+\"|`(?:[^`]|``)+`|\[(?:[^\]]|\]\])+\])"
)
MAX_INT32 = 2_147_483_647
TYPESCRIPT_RESERVED_IDENTIFIERS = {
    "as", "async", "await", "break", "case", "catch", "class", "const", "constructor", "continue", "debugger",
    "default", "delete", "do", "else", "enum", "export", "extends", "false", "finally",
    "for", "function", "if", "implements", "import", "in", "instanceof", "interface",
    "let", "new", "null", "of", "package", "private", "protected", "public", "return", "static",
    "super", "switch", "this", "throw", "true", "try", "typeof", "undefined", "var", "void",
    "while", "with", "yield",
}

SQL_COLUMN_CONSTRAINT_KEYWORDS = {
    "AS", "CHECK", "COLLATE", "CONSTRAINT", "DEFAULT", "GENERATED", "NOT", "NULL", "PRIMARY",
    "REFERENCES", "UNIQUE",
}

SQL_TABLE_ALIAS_TERMINATORS = {
    "CROSS", "EXCEPT", "FULL", "GROUP", "HAVING", "INDEXED", "INNER", "INTERSECT", "JOIN", "LEFT",
    "LIMIT", "NATURAL", "NOT", "OFFSET", "ON", "ORDER", "OUTER", "RIGHT", "UNION", "USING", "WHERE",
}


class QueryTableSource(NamedTuple):
    table: Table
    aliases: List[str]
    nullable: bool


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
        schema = strip_sql_comments(sql_text)
        for match in re.finditer(
            rf"\bCREATE\s+(?:TEMP(?:ORARY)?\s+)?TABLE\s+(?:IF\s+NOT\s+EXISTS\s+)?"
            rf"({SQL_IDENTIFIER_PATTERN})\s*\(",
            schema,
            re.IGNORECASE,
        ):
            table_name = clean_identifier(match.group(1))
            opening_parenthesis = match.end() - 1
            closing_parenthesis = find_matching_parenthesis(schema, opening_parenthesis)
            if closing_parenthesis is None:
                continue
            body = schema[opening_parenthesis + 1:closing_parenthesis]
            columns = parse_columns(body, custom_types)
            if columns:
                tables[table_name.lower()] = Table(name=table_name, columns=columns)
    return tables


def collect_create_statements(sql_texts: Iterable[str]) -> List[str]:
    table_statements: List[str] = []
    other_statements: List[str] = []
    for sql_text in sql_texts:
        for statement in split_sql_statements(strip_sql_comments(schema_prefix(sql_text))):
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
    masked_lines = mask_sql_non_code(sql_text).splitlines()
    for raw_line, masked_line in zip(sql_text.splitlines(), masked_lines):
        if re.match(r"^\s*[A-Za-z_][A-Za-z0-9_]*\s*:\s*", masked_line):
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
        if version < 2 or version > MAX_INT32:
            raise ClientSqlError(
                f"Migration version must be an integer from 2 through {MAX_INT32}: {path}"
            )
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


def validate_generated_identifiers(sql_files: Sequence[SqlFile], tables: Dict[str, Table]) -> None:
    query_class_names: Dict[str, Path] = {}
    query_property_names: Dict[str, Path] = {}
    for sql_file in sql_files:
        class_name = f"{sanitize_type_name(sql_file.stem_path.name)}Queries"
        property_name = class_name.removesuffix("Queries")
        property_name = property_name[:1].lower() + property_name[1:] + "Queries"
        for generated_name, owner_map, kind in (
            (class_name, query_class_names, "query class"),
            (property_name, query_property_names, "database query property"),
        ):
            previous = owner_map.get(generated_name)
            if previous is not None:
                raise ClientSqlError(
                    f"Generated {kind} identifier '{generated_name}' collides between "
                    f"{previous} and {sql_file.rel_to_package}"
                )
            owner_map[generated_name] = sql_file.rel_to_package

        generated_methods: Dict[str, str] = {}
        emitted_symbols: Dict[str, str] = {}
        for table in tables.values():
            generated_name = sanitize_type_name(table.name)
            owner = f"table '{table.name}'"
            previous = emitted_symbols.get(generated_name)
            if previous is not None and previous != owner:
                raise ClientSqlError(
                    f"Generated type identifier '{generated_name}' collides between {previous} and {owner} "
                    f"in {sql_file.rel_to_package}"
                )
            emitted_symbols[generated_name] = owner
        for query in sql_file.queries:
            generated_parameters: Dict[str, str] = {}
            for parameter_name in query.param_order:
                generated_parameter = sanitize_identifier(parameter_name)
                previous_parameter = generated_parameters.get(generated_parameter)
                if previous_parameter is not None and previous_parameter != parameter_name:
                    raise ClientSqlError(
                        f"Generated query parameter identifier '{generated_parameter}' collides between "
                        f"'{previous_parameter}' and '{parameter_name}' in query '{query.name}' "
                        f"in {sql_file.rel_to_package}"
                    )
                generated_parameters[generated_parameter] = parameter_name

            method_name = sanitize_identifier(query.name)
            method_names = [method_name]
            if query.returns_rows:
                method_names.append(f"watch{sanitize_type_name(query.name)}")
            for generated_name in method_names:
                previous = generated_methods.get(generated_name)
                if previous is not None:
                    raise ClientSqlError(
                        f"Generated query method identifier '{generated_name}' collides between "
                        f"'{previous}' and '{query.name}' in {sql_file.rel_to_package}"
                    )
                generated_methods[generated_name] = query.name

            for generated_name in (
                f"{sanitize_type_name(query.name)}Params" if query.params else "",
                query.result_type
                if query.returns_rows and query.result_type == f"{sanitize_type_name(query.name)}Row"
                else "",
            ):
                if not generated_name:
                    continue
                owner = (
                    f"query parameters for '{query.name}'"
                    if generated_name.endswith("Params")
                    else f"query row for '{query.name}'"
                )
                previous = emitted_symbols.get(generated_name)
                if previous is not None and previous != owner:
                    raise ClientSqlError(
                        f"Generated type identifier '{generated_name}' collides between "
                        f"{previous} and {owner} in {sql_file.rel_to_package}"
                    )
                emitted_symbols[generated_name] = owner

            field_names: set[str] = set()
            for field in query.result_fields:
                if field.name in field_names:
                    raise ClientSqlError(
                        f"Duplicate result field '{field.name}' in query '{query.name}' "
                        f"in {sql_file.rel_to_package}"
                    )
                field_names.add(field.name)

def migration_sort_key(path: Path) -> Tuple[int, str]:
    match = re.match(r"(\d+)", path.stem)
    return (int(match.group(1)) if match else sys.maxsize, path.name)


def parse_columns(body: str, custom_types: Dict[str, str]) -> List[Column]:
    columns: List[Column] = []
    for part in split_top_level(body, ","):
        column_match = re.match(rf"^\s*({SQL_IDENTIFIER_PATTERN})(.*)$", part, re.DOTALL)
        if column_match is None:
            continue
        raw_name = column_match.group(1)
        name = clean_identifier(raw_name)
        if raw_name == name and name.upper() in {"PRIMARY", "FOREIGN", "UNIQUE", "CHECK", "CONSTRAINT"}:
            continue

        declaration = column_match.group(2).strip()
        constraint_start = find_top_level_keyword(declaration, SQL_COLUMN_CONSTRAINT_KEYWORDS)
        if constraint_start is None:
            raw_sql_type = declaration
            constraints = ""
        else:
            raw_sql_type = declaration[:constraint_start].strip()
            constraints = declaration[constraint_start:].upper()
        sql_type = normalize_sql_type(raw_sql_type)
        nullable = "NOT NULL" not in constraints and "PRIMARY KEY" not in constraints
        base_type = base_sql_type(sql_type)
        ts_type = custom_types.get(sql_type)
        if ts_type is None:
            ts_type = custom_types.get(base_type, default_ts_type(sql_type))
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

    masked_lines = mask_sql_non_code(sql_text).splitlines()
    for raw_line, masked_line in zip(sql_text.splitlines(), masked_lines):
        label = re.match(r"^\s*([A-Za-z_][A-Za-z0-9_]*)\s*:\s*(.*)$", masked_line)
        if label:
            raw_label = re.match(r"^\s*([A-Za-z_][A-Za-z0-9_]*)\s*:\s*(.*)$", raw_line)
            if raw_label is None:
                continue
            flush()
            current_name = raw_label.group(1)
            rest = raw_label.group(2).strip()
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
    occurrences = scan_param_occurrences(sql)
    out: List[str] = []
    cursor = 0
    for occurrence in occurrences:
        out.append(sql[cursor:occurrence.start])
        out.append("?")
        cursor = occurrence.end
    out.append(sql[cursor:])
    return "".join(out), [occurrence.name for occurrence in occurrences]


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
    sources = table_sources_for_query(sql, tables)
    if not sources:
        table = table_for_query(sql, tables)
        if table is not None:
            sources = [QueryTableSource(table=table, aliases=[table.name], nullable=False)]
    if not sources:
        return {}

    masked_sql = mask_sql_for_identifier_matching(sql)
    result: Dict[str, str] = {}
    for match in re.finditer(
        rf"(?<![A-Za-z0-9_])(?:({SQL_IDENTIFIER_PATTERN})\s*\.\s*)?"
        rf"({SQL_IDENTIFIER_PATTERN})\s*(?:<=|>=|!=|<>|=|<|>|LIKE|IN)\s*"
        rf"(\?|:[A-Za-z_][A-Za-z0-9_]*\??)",
        masked_sql,
        re.IGNORECASE,
    ):
        qualifier = sql[match.start(1):match.end(1)] if match.group(1) is not None else None
        column_name = sql[match.start(2):match.end(2)]
        if qualifier is not None:
            source = table_for_source_name(qualifier, sources)
            column = find_column(source.table, column_name) if source is not None else None
        else:
            matching_columns = [
                column
                for source in sources
                if (column := find_column(source.table, column_name)) is not None
            ]
            column = matching_columns[0] if len(matching_columns) == 1 else None
        if column is None:
            continue
        param_name = param_name_at(sql, match.start(3), param_order)
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
    code = mask_sql_non_code(sql)
    for occurrence in occurrences:
        if re.match(r"\s+IS\s+(?:NOT\s+)?NULL\b", code[occurrence.end:], re.IGNORECASE):
            nullable.add(occurrence.name)
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
    for start, end in sql_code_ranges(sql):
        i = start
        while i < end:
            ch = sql[i]
            if ch == "?":
                occurrences.append(
                    ParamOccurrence(start=i, end=i + 1, name=f"p{positional_index}", nullable=False)
                )
                positional_index += 1
                i += 1
                continue
            if ch == ":" and i + 1 < end and re.match(r"[A-Za-z_]", sql[i + 1]):
                match = re.match(r":([A-Za-z_][A-Za-z0-9_]*)(\?)?", sql[i:end])
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


def sql_code_ranges(sql: str) -> List[Tuple[int, int]]:
    """Return code spans, excluding literals, quoted identifiers, and comments."""
    ranges: List[Tuple[int, int]] = []
    code_start = 0
    i = 0
    while i < len(sql):
        quote = sql[i]
        is_line_comment = sql.startswith("--", i)
        is_block_comment = sql.startswith("/*", i)
        is_quoted = quote in {"'", '"', "`", "["}
        if not is_line_comment and not is_block_comment and not is_quoted:
            i += 1
            continue

        if code_start < i:
            ranges.append((code_start, i))
        if is_line_comment:
            newline = sql.find("\n", i + 2)
            i = len(sql) if newline == -1 else newline + 1
        elif is_block_comment:
            terminator = sql.find("*/", i + 2)
            i = len(sql) if terminator == -1 else terminator + 2
        else:
            closing_quote = "]" if quote == "[" else quote
            i += 1
            while i < len(sql):
                if sql[i] != closing_quote:
                    i += 1
                    continue
                if i + 1 < len(sql) and sql[i + 1] == closing_quote:
                    i += 2
                    continue
                i += 1
                break
        code_start = i
    if code_start < len(sql):
        ranges.append((code_start, len(sql)))
    return ranges


def mask_sql_non_code(sql: str) -> str:
    masked = [" " if not character.isspace() else character for character in sql]
    for start, end in sql_code_ranges(sql):
        masked[start:end] = sql[start:end]
    return "".join(masked)


def mask_sql_literals_and_comments(sql: str) -> str:
    """Mask string literals and comments while preserving quoted identifiers and offsets."""
    masked = list(sql)
    i = 0
    while i < len(sql):
        start = i
        if sql.startswith("--", i):
            newline = sql.find("\n", i + 2)
            i = len(sql) if newline == -1 else newline
        elif sql.startswith("/*", i):
            terminator = sql.find("*/", i + 2)
            i = len(sql) if terminator == -1 else terminator + 2
        elif sql[i] == "'":
            i += 1
            while i < len(sql):
                if sql[i] != "'":
                    i += 1
                    continue
                if i + 1 < len(sql) and sql[i + 1] == "'":
                    i += 2
                    continue
                i += 1
                break
        elif sql[i] in {'"', "`", "["}:
            closing_quote = "]" if sql[i] == "[" else sql[i]
            i += 1
            while i < len(sql):
                if sql[i] != closing_quote:
                    i += 1
                    continue
                if i + 1 < len(sql) and sql[i + 1] == closing_quote:
                    i += 2
                    continue
                i += 1
                break
            continue
        else:
            i += 1
            continue

        for index in range(start, i):
            if not masked[index].isspace():
                masked[index] = " "
    return "".join(masked)


def mask_sql_for_identifier_matching(sql: str) -> str:
    """Mask non-code while retaining identifier spans without exposing keyword-like contents."""
    masked = list(sql)
    i = 0
    while i < len(sql):
        start = i
        if sql.startswith("--", i):
            newline = sql.find("\n", i + 2)
            i = len(sql) if newline == -1 else newline
            preserve_delimiters = False
            closing_quote = None
        elif sql.startswith("/*", i):
            terminator = sql.find("*/", i + 2)
            i = len(sql) if terminator == -1 else terminator + 2
            preserve_delimiters = False
            closing_quote = None
        elif sql[i] == "'":
            closing_quote = "'"
            i += 1
            while i < len(sql):
                if sql[i] != closing_quote:
                    i += 1
                    continue
                if i + 1 < len(sql) and sql[i + 1] == closing_quote:
                    i += 2
                    continue
                i += 1
                break
            preserve_delimiters = False
        elif sql[i] in {'"', "`", "["}:
            closing_quote = "]" if sql[i] == "[" else sql[i]
            i += 1
            while i < len(sql):
                if sql[i] != closing_quote:
                    i += 1
                    continue
                if i + 1 < len(sql) and sql[i + 1] == closing_quote:
                    i += 2
                    continue
                i += 1
                break
            preserve_delimiters = True
        else:
            i += 1
            continue

        for index in range(start, i):
            if masked[index].isspace():
                continue
            if preserve_delimiters and (index == start or sql[index] == closing_quote):
                continue
            masked[index] = "_" if preserve_delimiters else " "
    return "".join(masked)


def sql_parenthesis_depths(sql: str) -> List[int]:
    """Return the nesting depth at each offset, ignoring parentheses in quoted identifiers."""
    depths = [0] * len(sql)
    depth = 0
    quote: Optional[str] = None
    closing_quote: Optional[str] = None
    i = 0
    while i < len(sql):
        depths[i] = depth
        ch = sql[i]
        if quote is not None:
            if ch == closing_quote:
                if i + 1 < len(sql) and sql[i + 1] == closing_quote:
                    depths[i + 1] = depth
                    i += 2
                    continue
                quote = None
                closing_quote = None
            i += 1
            continue
        if ch in {'"', "`", "["}:
            quote = ch
            closing_quote = "]" if ch == "[" else ch
        elif ch == "(":
            depth += 1
        elif ch == ")":
            depth = max(0, depth - 1)
        i += 1
    return depths


def top_level_select_projection_bounds(sql: str) -> Optional[Tuple[int, int]]:
    keyword_sql = mask_sql_non_code(sql)
    parenthesis_depths = sql_parenthesis_depths(keyword_sql)
    select_span = find_top_level_keyword_span(keyword_sql, parenthesis_depths, "SELECT", 0)
    if select_span is None:
        return None
    from_span = find_top_level_keyword_span(keyword_sql, parenthesis_depths, "FROM", select_span[1])
    if from_span is None:
        return None
    return (select_span[1], from_span[0])


def find_top_level_keyword_span(
    masked_sql: str,
    parenthesis_depths: Sequence[int],
    keyword: str,
    start: int,
) -> Optional[Tuple[int, int]]:
    for match in re.finditer(rf"\b{re.escape(keyword)}\b", masked_sql[start:], re.IGNORECASE):
        absolute_start = start + match.start()
        if parenthesis_depths[absolute_start] == 0:
            return (absolute_start, start + match.end())
    return None


def infer_result_fields(sql: str, tables: Dict[str, Table]) -> List[Column]:
    projection_bounds = top_level_select_projection_bounds(sql)
    if projection_bounds is None:
        return []

    sources = table_sources_for_query(sql, tables)
    if not sources:
        return []

    selected = sql[projection_bounds[0]:projection_bounds[1]].strip()
    fields: List[Column] = []
    for part in split_top_level(selected, ","):
        expression = part.strip()
        if expression == "*":
            for source in sources:
                for column in source.table.columns:
                    append_result_field(fields, result_column_for_source(source, column))
            continue

        qualified_star = re.fullmatch(
            rf"({SQL_IDENTIFIER_PATTERN})\s*\.\s*\*",
            expression,
            re.IGNORECASE,
        )
        if qualified_star:
            source = table_for_source_name(qualified_star.group(1), sources)
            if source is not None:
                for column in source.table.columns:
                    append_result_field(fields, result_column_for_source(source, column))
            continue

        alias_match = re.search(rf"\s+AS\s+({SQL_IDENTIFIER_PATTERN})\s*$", expression, re.IGNORECASE)
        if alias_match:
            name = clean_identifier(alias_match.group(1))
            source = expression[: alias_match.start()].strip()
        else:
            source = expression
            name = clean_identifier(source.split(".")[-1].strip())

        column = find_result_column(source, sources)
        if column:
            append_result_field(
                fields,
                Column(name=name, sql_type=column.sql_type, ts_type=column.ts_type, nullable=column.nullable),
            )
        else:
            append_result_field(
                fields,
                Column(name=name, sql_type="ANY", ts_type=infer_expression_type(source), nullable=True),
            )
    return fields


def select_star_table(sql: str, tables: Dict[str, Table]) -> Optional[Table]:
    projection_bounds = top_level_select_projection_bounds(sql)
    if projection_bounds is None or sql[projection_bounds[0]:projection_bounds[1]].strip() != "*":
        return None
    sources = table_sources_for_query(sql, tables)
    return sources[0].table if len(sources) == 1 else None


def table_sources_for_query(sql: str, tables: Dict[str, Table]) -> List[QueryTableSource]:
    sources: List[QueryTableSource] = []
    pattern = (
        rf"\b(FROM|(?:(?:NATURAL)\s+)?"
        rf"(?:(LEFT|RIGHT|FULL|INNER|CROSS)(?:\s+OUTER)?\s+)?JOIN)\s+"
        rf"({SQL_IDENTIFIER_PATTERN})"
    )
    masked_sql = mask_sql_for_identifier_matching(sql)
    parenthesis_depths = sql_parenthesis_depths(masked_sql)
    source_shape_end = len(masked_sql)
    keyword_sql = mask_sql_non_code(sql)
    for compound_match in re.finditer(r"\b(?:UNION|INTERSECT|EXCEPT)\b", keyword_sql, re.IGNORECASE):
        if parenthesis_depths[compound_match.start()] == 0:
            source_shape_end = compound_match.start()
            break
    for match in re.finditer(pattern, masked_sql[:source_shape_end], re.IGNORECASE):
        if parenthesis_depths[match.start()] != 0:
            continue
        table_identifier = sql[match.start(3):match.end(3)]
        table = tables.get(clean_identifier(table_identifier).lower())
        if table is None:
            continue

        aliases = [table.name]
        trailing = masked_sql[match.end():source_shape_end]
        alias_match = re.match(
            rf"\s+AS\s+({SQL_IDENTIFIER_PATTERN})",
            trailing,
            re.IGNORECASE,
        )
        if alias_match is None:
            bare_alias_match = re.match(rf"\s+({SQL_IDENTIFIER_PATTERN})", trailing)
            if bare_alias_match is not None:
                candidate = clean_identifier(bare_alias_match.group(1))
                if candidate.upper() not in SQL_TABLE_ALIAS_TERMINATORS:
                    alias_match = bare_alias_match
        if alias_match is not None:
            alias_start = match.end() + alias_match.start(1)
            alias_end = match.end() + alias_match.end(1)
            aliases.append(clean_identifier(sql[alias_start:alias_end]))
        join_kind = match.group(2).upper() if match.group(2) is not None else None
        if join_kind in {"RIGHT", "FULL"}:
            sources = [source._replace(nullable=True) for source in sources]
        sources.append(QueryTableSource(table=table, aliases=aliases, nullable=join_kind in {"LEFT", "FULL"}))
    return sources


def table_for_source_name(name: str, sources: Sequence[QueryTableSource]) -> Optional[QueryTableSource]:
    cleaned = clean_identifier(name).lower()
    matches = [source for source in sources if any(alias.lower() == cleaned for alias in source.aliases)]
    return matches[0] if len(matches) == 1 else None


def result_column_for_source(source: QueryTableSource, column: Column) -> Column:
    if not source.nullable or column.nullable:
        return column
    return Column(
        name=column.name,
        sql_type=column.sql_type,
        ts_type=nullable_type(column.ts_type),
        nullable=True,
    )


def find_result_column(source: str, sources: Sequence[QueryTableSource]) -> Optional[Column]:
    reference = re.fullmatch(
        rf"\s*(?:({SQL_IDENTIFIER_PATTERN})\s*\.\s*)?({SQL_IDENTIFIER_PATTERN})\s*",
        source,
    )
    if reference is None:
        return None

    qualifier = reference.group(1)
    column_name = reference.group(2)
    if qualifier is not None:
        table_source = table_for_source_name(qualifier, sources)
        if table_source is None:
            return None
        column = find_column(table_source.table, column_name)
        return result_column_for_source(table_source, column) if column is not None else None

    matches = [
        result_column_for_source(table_source, column)
        for table_source in sources
        if (column := find_column(table_source.table, column_name)) is not None
    ]
    return matches[0] if len(matches) == 1 else None


def append_result_field(fields: List[Column], column: Column) -> None:
    for index, existing in enumerate(fields):
        if existing.name == column.name:
            # The native row map keeps the final value when SQLite returns duplicate
            # column labels, so mirror that shape while retaining stable field order.
            fields[index] = column
            return
    fields.append(column)


def table_for_query(sql: str, tables: Dict[str, Table]) -> Optional[Table]:
    sources = table_sources_for_query(sql, tables)
    if sources:
        return sources[0].table
    masked_sql = mask_sql_literals_and_comments(sql)
    for pattern in [
        r"\bUPDATE\s+([A-Za-z_][A-Za-z0-9_]*)",
        r"\bINSERT\s+(?:OR\s+\w+\s+)?INTO\s+([A-Za-z_][A-Za-z0-9_]*)",
        r"\bDELETE\s+FROM\s+([A-Za-z_][A-Za-z0-9_]*)",
    ]:
        match = re.search(pattern, masked_sql, re.IGNORECASE)
        if match:
            return tables.get(match.group(1).lower())
    return None


def tables_read_by_query(sql: str, tables: Dict[str, Table]) -> List[str]:
    names: List[str] = []
    pattern = rf"\b(?:FROM|JOIN)\s+({SQL_IDENTIFIER_PATTERN})"
    masked_sql = mask_sql_for_identifier_matching(sql)
    for match in re.finditer(pattern, masked_sql, re.IGNORECASE):
        table_identifier = sql[match.start(1):match.end(1)]
        table = tables.get(clean_identifier(table_identifier).lower())
        if table is not None and table.name not in names:
            names.append(table.name)
    return names


def tables_changed_by_query(sql: str, tables: Dict[str, Table]) -> List[str]:
    names: List[str] = []
    masked_sql = mask_sql_for_identifier_matching(sql)
    patterns = [
        rf"\bUPDATE\s+({SQL_IDENTIFIER_PATTERN})",
        rf"\b(?:INSERT|REPLACE)\s+(?:OR\s+\w+\s+)?INTO\s+({SQL_IDENTIFIER_PATTERN})",
        rf"\bDELETE\s+FROM\s+({SQL_IDENTIFIER_PATTERN})",
    ]
    for pattern in patterns:
        for match in re.finditer(pattern, masked_sql, re.IGNORECASE):
            table_identifier = sql[match.start(1):match.end(1)]
            table = tables.get(clean_identifier(table_identifier).lower())
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
    normalized = base_sql_type(sql_type)
    if "INT" in normalized:
        return "number"
    if normalized in {"REAL", "DOUBLE", "DOUBLE PRECISION", "FLOAT", "NUMERIC", "DECIMAL"}:
        return "number"
    if any(marker in normalized for marker in ("CHAR", "CLOB", "TEXT")):
        return "string"
    if normalized == "BLOB":
        return "ArrayBuffer"
    if normalized in {"BOOL", "BOOLEAN"}:
        return "boolean"
    return "any"


def normalize_sql_type(sql_type: str) -> str:
    return re.sub(r"\s+", " ", sql_type.strip()).upper()


def base_sql_type(sql_type: str) -> str:
    normalized = normalize_sql_type(sql_type)
    return re.sub(r"\s*\([^()]*\)\s*$", "", normalized).strip()


def nullable_type(ts_type: str) -> str:
    if ts_type == "any" or re.search(r"(?:^|\|\s*)null(?:\s*\||$)", ts_type):
        return ts_type
    return f"{ts_type} | null"


def split_sql_statements(sql_text: str) -> List[str]:
    statements: List[str] = []
    current: List[str] = []
    code = mask_sql_non_code(sql_text)
    for index, ch in enumerate(sql_text):
        current.append(ch)
        if code[index] == ";":
            statement = "".join(current).strip()
            if statement:
                statements.append(statement)
            current = []

    tail = "".join(current).strip()
    if tail:
        statements.append(tail)
    return statements


def split_top_level(text: str, delimiter: str) -> List[str]:
    parts: List[str] = []
    current: List[str] = []
    depth = 0
    quote: Optional[str] = None
    closing_quote: Optional[str] = None
    i = 0
    while i < len(text):
        ch = text[i]
        if quote:
            current.append(ch)
            if ch == closing_quote:
                if i + 1 < len(text) and text[i + 1] == closing_quote:
                    current.append(text[i + 1])
                    i += 2
                    continue
                quote = None
                closing_quote = None
            i += 1
            continue
        if ch in {"'", '"', "`", "["}:
            quote = ch
            closing_quote = "]" if ch == "[" else ch
            current.append(ch)
            i += 1
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
        i += 1
    parts.append("".join(current))
    return parts


def find_matching_parenthesis(text: str, opening_parenthesis: int) -> Optional[int]:
    depth = 0
    quote: Optional[str] = None
    closing_quote: Optional[str] = None
    i = opening_parenthesis
    while i < len(text):
        ch = text[i]
        if quote is not None:
            if ch == closing_quote:
                if i + 1 < len(text) and text[i + 1] == closing_quote:
                    i += 2
                    continue
                quote = None
                closing_quote = None
            i += 1
            continue
        if ch in {"'", '"', "`", "["}:
            quote = ch
            closing_quote = "]" if ch == "[" else ch
        elif ch == "(":
            depth += 1
        elif ch == ")":
            depth -= 1
            if depth == 0:
                return i
        i += 1
    return None


def find_top_level_keyword(text: str, keywords: set[str]) -> Optional[int]:
    depth = 0
    quote: Optional[str] = None
    closing_quote: Optional[str] = None
    i = 0
    while i < len(text):
        ch = text[i]
        if quote is not None:
            if ch == closing_quote:
                if i + 1 < len(text) and text[i + 1] == closing_quote:
                    i += 2
                    continue
                quote = None
                closing_quote = None
            i += 1
            continue
        if ch in {"'", '"', "`", "["}:
            quote = ch
            closing_quote = "]" if ch == "[" else ch
            i += 1
            continue
        if ch == "(":
            depth += 1
            i += 1
            continue
        if ch == ")":
            depth = max(0, depth - 1)
            i += 1
            continue
        if depth == 0 and (ch.isalpha() or ch == "_"):
            end = i + 1
            while end < len(text) and (text[end].isalnum() or text[end] == "_"):
                end += 1
            if text[i:end].upper() in keywords:
                return i
            i = end
            continue
        i += 1
    return None


def strip_sql_comments(sql_text: str) -> str:
    stripped = list(sql_text)
    quote: Optional[str] = None
    closing_quote: Optional[str] = None
    i = 0
    while i < len(sql_text):
        ch = sql_text[i]
        if quote is not None:
            if ch == closing_quote:
                if i + 1 < len(sql_text) and sql_text[i + 1] == closing_quote:
                    i += 2
                    continue
                quote = None
                closing_quote = None
            i += 1
            continue
        if ch in {"'", '"', "`", "["}:
            quote = ch
            closing_quote = "]" if ch == "[" else ch
            i += 1
            continue
        if sql_text.startswith("--", i):
            end = sql_text.find("\n", i + 2)
            end = len(sql_text) if end == -1 else end
        elif sql_text.startswith("/*", i):
            terminator = sql_text.find("*/", i + 2)
            end = len(sql_text) if terminator == -1 else terminator + 2
        else:
            i += 1
            continue
        for index in range(i, end):
            if not stripped[index].isspace():
                stripped[index] = " "
        i = end
    return "".join(stripped)


def clean_identifier(identifier: str) -> str:
    cleaned = identifier.strip()
    if len(cleaned) < 2:
        return cleaned
    opening_quote = cleaned[0]
    closing_quote = "]" if opening_quote == "[" else opening_quote
    if opening_quote not in {'"', "`", "["} or cleaned[-1] != closing_quote:
        return cleaned
    inner = cleaned[1:-1]
    return inner.replace(closing_quote * 2, closing_quote)


def sanitize_identifier(name: str) -> str:
    cleaned = re.sub(r"\W+", "_", name)
    if not cleaned or re.match(r"\d", cleaned):
        cleaned = f"p_{cleaned}"
    if cleaned in TYPESCRIPT_RESERVED_IDENTIFIERS:
        cleaned = f"_{cleaned}"
    return cleaned


def sanitize_type_name(name: str) -> str:
    parts = re.split(r"[^A-Za-z0-9]+", name)
    result = "".join(part[:1].upper() + part[1:] for part in parts if part)
    if not result:
        return "Generated"
    if re.match(r"\d", result):
        return f"T{result}"
    return result
