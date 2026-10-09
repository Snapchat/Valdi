from __future__ import annotations

import os
import sys
from pathlib import Path
from typing import NamedTuple

from python.runfiles import Runfiles


class OptionValue(NamedTuple):
    argument_index: int
    value: str
    inline: bool


def find_option(arguments: list[str], short_name: str, long_name: str) -> OptionValue:
    matches: list[OptionValue] = []
    for index, argument in enumerate(arguments):
        if argument == short_name or argument == long_name:
            if index + 1 >= len(arguments):
                raise ValueError(f"{argument} requires a value")
            matches.append(OptionValue(index, arguments[index + 1], False))
        elif argument.startswith(f"{short_name}=") or argument.startswith(f"{long_name}="):
            matches.append(OptionValue(index, argument.split("=", 1)[1], True))

    if len(matches) != 1:
        raise ValueError(f"expected exactly one {short_name}/{long_name} option")
    return matches[0]


def public_generator_arguments(arguments: list[str]) -> list[str]:
    if "-version" in arguments or "--version" in arguments:
        return arguments

    package = find_option(arguments, "-p", "--package")
    class_name = find_option(arguments, "-c", "--class")
    module = find_option(arguments, "-m", "--module")
    output = find_option(arguments, "-o", "--output")
    if package.value != class_name.value or package.value != module.value:
        raise ValueError(
            "expected the legacy ClientSQL protocol to pass the database class "
            "unchanged through -p, -c, and -m"
        )

    output_directory = Path(output.value)
    if output_directory.name != "sqlgen" or output_directory.parent.name != "src":
        raise ValueError("ClientSQL output must end in <bundle>/src/sqlgen")
    bundle_directory = output_directory.parent.parent
    generated_directory = bundle_directory.parent
    if (
        not bundle_directory.name
        or bundle_directory.name in (".", "..")
        or generated_directory == bundle_directory
        or not generated_directory.name
    ):
        raise ValueError("ClientSQL output must be <generated>/<bundle>/src/sqlgen")

    translated = list(arguments)
    if module.inline:
        translated[module.argument_index] = (
            f"{translated[module.argument_index].split('=', 1)[0]}={bundle_directory.name}"
        )
    else:
        translated[module.argument_index + 1] = bundle_directory.name
    return translated


def runfile_path(runfiles: Runfiles, repository: str, path: str) -> str:
    repositories = [repository] if repository else ["_main", "valdi"]
    for candidate_repository in repositories:
        resolved = runfiles.Rlocation(f"{candidate_repository}/{path}")
        if resolved is not None and os.path.isfile(resolved):
            return resolved
    raise RuntimeError(f"Missing ClientSQL toolchain runfile: {path}")


def main() -> None:
    runfiles = Runfiles.Create()
    if runfiles is None:
        raise RuntimeError("ClientSQL toolchain wrapper requires Bazel runfiles")

    repository = runfiles.CurrentRepository()
    generator = runfile_path(runfiles, repository, "compiler/clientsql/clientsql")
    validator = runfile_path(runfiles, repository, "compiler/clientsql/sqlite_316_validator")

    try:
        generator_arguments = public_generator_arguments(sys.argv[1:])
    except ValueError as error:
        print(f"clientsql toolchain: {error}", file=sys.stderr)
        raise SystemExit(2) from error

    environment = dict(os.environ)
    environment.update(runfiles.EnvVars())
    os.execve(
        generator,
        [generator, "--sqlite-validator", validator, *generator_arguments],
        environment,
    )


if __name__ == "__main__":
    main()
