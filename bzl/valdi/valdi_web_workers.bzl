"""Shared validation helpers for Valdi browser worker declarations."""

def validate_web_workers(web_workers, owner):
    """Validates browser worker entry module paths."""
    for entry_path in sorted(web_workers):
        _validate_web_worker_path(entry_path, owner)

def _validate_web_worker_path(path, owner):
    if not path:
        fail("Invalid web_workers declaration on {}: entry path must not be empty".format(owner))
    if "?" in path or "#" in path:
        fail("Invalid web_workers declaration on {}: entry path '{}' must not contain a query or fragment".format(owner, path))
    if path.endswith(".js"):
        fail("Invalid web_workers declaration on {}: entry path '{}' must not end in .js".format(owner, path))
