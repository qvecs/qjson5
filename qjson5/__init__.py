from typing import IO, Any, Optional

from .py_json5 import dumps, loads


def load(fp: IO[str]) -> Any:
    """
    Read JSON5 text from a file-like object and parse into Python data.
    """
    return loads(fp.read())


def dump(obj: Any, fp: IO[str], indent: Optional[int] = None) -> None:
    """
    Serialize 'obj' as JSON5 into a file-like object.
    """
    text = dumps(obj, indent=indent)
    fp.write(text)


__all__ = ["loads", "dumps", "load", "dump"]
