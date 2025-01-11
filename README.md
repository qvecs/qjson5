# Quick JSON5 _(qjson5)_

<p align="center">

  <a href="https://github.com/qvecs/qjson5/actions?query=workflow%3ABuild">
    <img src="https://github.com/qvecs/qjson5/workflows/Build/badge.svg">
  </a>

  <a href="https://opensource.org/licenses/MIT">
    <img src="https://img.shields.io/badge/License-MIT-blue.svg">
  </a>
</p>

A quick [JSON5](https://json5.org/) implementation written in C, with Python bindings.

## Install

```
pip install qjson5
```

## Usage

```python
import qjson5

data = {
    "key": "value",
    "array": [1, 2, 3],
    "object": {
        "nested": "value"
    }
}

encoded = qjson5.dumps(data)
print(f"Encoded: {encoded}")

decoded = qjson5.loads(encoded)
print(f"Decoded: {decoded}")
```

## Benchmark

Comparing with the other JSON5 libraries:

* [pyjson5](https://pypi.org/project/pyjson5/)
* [json5](https://pypi.org/project/json5/)

Non-JSON5 library:

* [json](https://docs.python.org/3/library/json.html) (built-in)

```bash
--- Data Set: SMALL (10 keys per level, 2 levels) ---
json5        => Skipped
builtin-json => avg: 15.2858 ms (std: 0.3651 ms) over 1000 iterations
pyjson5      => avg: 15.7470 ms (std: 0.3296 ms) over 1000 iterations
qjson5       => avg: 7.6033 ms (std: 0.1131 ms) over 1000 iterations

--- Data Set: MEDIUM (100 keys per level, 10 levels) ---
json5        => Skipped
builtin-json => avg: 169.5224 ms (std: 1.3099 ms) over 500 iterations
pyjson5      => avg: 207.0516 ms (std: 0.6961 ms) over 500 iterations
qjson5       => avg: 105.7047 ms (std: 1.4205 ms) over 500 iterations

--- Data Set: LARGE (1000 keys per level, 50 levels) ---
json5        => Skipped
builtin-json => avg: 179.8407 ms (std: 2.6937 ms) over 10 iterations
pyjson5      => Error: Maximum nesting level exceeded
qjson5       => avg: 119.5468 ms (std: 0.4641 ms) over 10 iterations
```

See `scripts/benchmark.py` for benchmarking details.
