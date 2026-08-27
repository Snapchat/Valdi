from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
from typing import List


@dataclass
class Column:
    name: str
    sql_type: str
    ts_type: str
    nullable: bool


@dataclass
class Table:
    name: str
    columns: List[Column]


@dataclass
class Parameter:
    name: str
    ts_type: str


@dataclass
class ParamOccurrence:
    start: int
    end: int
    name: str
    nullable: bool


@dataclass
class Query:
    name: str
    sql: str
    runtime_sql: str
    param_order: List[str]
    params: List[Parameter]
    result_type: str
    result_fields: List[Column]
    returns_rows: bool
    read_tables: List[str]
    changed_tables: List[str]


@dataclass
class SqlFile:
    path: Path
    rel_to_package: Path
    stem_path: Path
    queries: List[Query]


class ClientSqlError(Exception):
    pass
