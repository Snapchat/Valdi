from __future__ import annotations

import hashlib
import re
import struct
import subprocess
from pathlib import Path
from typing import Sequence, Tuple

from .model import ClientSqlError


SQLITE_VALIDATOR_VERSION = "3.16.0"
SQLITE_VALIDATOR_SOURCE_ID = "2017-01-02 11:57:58 04ac0b75b1716541b2b97704f4809cb7ef19cccf"
SQLITE_VALIDATOR_SQLITE3_C_SHA1 = "e2920fb885569d14197c9b7958e6f1db573ee669"
SQLITE_VALIDATOR_PROTOCOL = 1
SQLITE_VALIDATOR_DECLARED_IDENTITY = (
    "valdi-clientsql-sqlite-validator protocol=1 sqlite=3.16.0 "
    f"sqlite_source_id={SQLITE_VALIDATOR_SOURCE_ID} "
    f"sqlite3_c_sha1={SQLITE_VALIDATOR_SQLITE3_C_SHA1}"
)
PROTOCOL_HEADER = b"VALDI_CLIENTSQL_SQL_VALIDATOR_V1\n"
MAX_STATEMENT_BYTES = 16 * 1024 * 1024
MAX_REQUEST_BYTES = 64 * 1024 * 1024
VALIDATOR_ERROR_PATTERN = re.compile(
    r"^clientsql-validator-error:([a-z]+):(\d+):(.*)$",
    re.MULTILINE,
)

QueryValidation = Tuple[str, str, int]
MigrationValidation = Tuple[str, str]


class SQLite316Validator:
    def __init__(self, path: Path, binary_sha256: str):
        self.path = path
        self.binary_sha256 = binary_sha256

    @property
    def cache_identity(self) -> str:
        source_revision = SQLITE_VALIDATOR_SOURCE_ID.rsplit(" ", 1)[-1]
        return (
            f"sqlite-{SQLITE_VALIDATOR_VERSION}"
            f".source-id-{source_revision}"
            f".sqlite3-c-sha1-{SQLITE_VALIDATOR_SQLITE3_C_SHA1}"
            f".protocol-{SQLITE_VALIDATOR_PROTOCOL}"
            f".binary-sha256-{self.binary_sha256}"
        )

    @classmethod
    def resolve(cls, path_value: str | None) -> "SQLite316Validator":
        if not path_value:
            raise ClientSqlError(
                "ClientSQL requires --sqlite-validator pointing to the pinned SQLite 3.16.0 validator"
            )
        path = Path(path_value).expanduser()
        if not path.is_file():
            raise ClientSqlError(f"ClientSQL SQLite 3.16.0 validator does not exist: {path}")
        try:
            version_result = subprocess.run(
                [str(path), "--version"],
                check=False,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                timeout=10,
            )
        except (OSError, subprocess.SubprocessError) as exc:
            raise ClientSqlError(
                f"ClientSQL could not execute SQLite 3.16.0 validator '{path}': {exc}"
            ) from exc
        declared_identity = version_result.stdout.strip()
        if version_result.returncode != 0 or declared_identity != SQLITE_VALIDATOR_DECLARED_IDENTITY:
            details = version_result.stderr.strip() or declared_identity or "no identity returned"
            raise ClientSqlError(
                "ClientSQL SQLite validator identity mismatch; expected exact SQLite 3.16.0 "
                f"source {SQLITE_VALIDATOR_SOURCE_ID}, got: {details}"
            )
        try:
            binary_sha256 = cls._binary_sha256(path)
        except OSError as exc:
            raise ClientSqlError(f"ClientSQL could not hash SQLite validator '{path}': {exc}") from exc
        return cls(path=path, binary_sha256=binary_sha256)

    def validate(
        self,
        schema_statements: Sequence[str],
        queries: Sequence[QueryValidation],
        migrations: Sequence[MigrationValidation],
    ) -> None:
        request = bytearray(PROTOCOL_HEADER)
        self._append_uint32(request, len(schema_statements), "schema statement count")
        for statement in schema_statements:
            self._append_sql(request, statement)
        self._append_uint32(request, len(queries), "query count")
        for _context, sql, parameter_count in queries:
            self._append_uint32(request, parameter_count, "query parameter count")
            self._append_sql(request, sql)
        self._append_uint32(request, len(migrations), "migration statement count")
        for _context, statement in migrations:
            self._append_sql(request, statement)
        if len(request) > MAX_REQUEST_BYTES:
            raise ClientSqlError(
                f"ClientSQL SQL validation request exceeds the {MAX_REQUEST_BYTES}-byte limit"
            )

        try:
            current_sha256 = self._binary_sha256(self.path)
        except OSError as exc:
            raise ClientSqlError(f"ClientSQL could not re-read SQLite validator '{self.path}': {exc}") from exc
        if current_sha256 != self.binary_sha256:
            raise ClientSqlError("ClientSQL SQLite 3.16.0 validator changed after identity resolution")

        try:
            result = subprocess.run(
                [str(self.path), "--validate"],
                input=bytes(request),
                check=False,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                timeout=30,
            )
        except (OSError, subprocess.SubprocessError) as exc:
            raise ClientSqlError(
                f"ClientSQL could not run pinned SQLite 3.16.0 validation: {exc}"
            ) from exc
        if result.returncode == 0:
            if result.stdout:
                raise ClientSqlError("ClientSQL SQLite 3.16.0 validator returned unexpected output")
            return

        stderr = result.stderr.decode("utf-8", errors="replace")
        match = VALIDATOR_ERROR_PATTERN.search(stderr)
        if match is None:
            raise ClientSqlError(
                "ClientSQL SQLite 3.16.0 validator failed without a structured error: "
                f"{stderr.strip() or f'exit {result.returncode}'}"
            )
        kind = match.group(1)
        index = int(match.group(2))
        message = match.group(3).strip()
        if kind == "schema" and index < len(schema_statements):
            context = f"schema statement {index + 1}"
        elif kind == "query" and index < len(queries):
            context = queries[index][0]
        elif kind == "migration" and index < len(migrations):
            context = migrations[index][0]
        else:
            context = f"validator {kind} record {index}"
        raise ClientSqlError(
            f"SQLite {SQLITE_VALIDATOR_VERSION} rejected {context}: {message}"
        )

    @staticmethod
    def _append_uint32(output: bytearray, value: int, label: str) -> None:
        if value < 0 or value > 0xFFFFFFFF:
            raise ClientSqlError(f"ClientSQL {label} exceeds validator protocol range")
        output.extend(struct.pack(">I", value))

    @classmethod
    def _append_sql(cls, output: bytearray, sql: str) -> None:
        encoded = sql.encode("utf-8")
        if b"\0" in encoded:
            raise ClientSqlError("ClientSQL SQL cannot contain embedded NUL characters")
        if len(encoded) > MAX_STATEMENT_BYTES:
            raise ClientSqlError(
                f"ClientSQL SQL exceeds the {MAX_STATEMENT_BYTES}-byte validator statement limit"
            )
        cls._append_uint32(output, len(encoded), "SQL byte length")
        output.extend(encoded)

    @staticmethod
    def _binary_sha256(path: Path) -> str:
        digest = hashlib.sha256()
        with path.open("rb") as executable:
            for chunk in iter(lambda: executable.read(1024 * 1024), b""):
                digest.update(chunk)
        return digest.hexdigest()
