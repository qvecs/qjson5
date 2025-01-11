from typing import Any, Optional

def loads(data: str) -> Any:
    """
    Parse a JSON5 string into a Python object.
    Raises ValueError on invalid JSON5.
    """
    ...

def dumps(obj: Any, indent: Optional[int] = None) -> str:
    """
    Convert a Python object into a JSON5 string.
    Indent >= 1 => pretty-print.
    """
    ...
