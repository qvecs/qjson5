# 📎 Quick JSON5 _(qjson5)_

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

### Complex Usage

```json5
{
  // comments
  unquoted: 'and you can quote me on that',
  singleQuotes: 'I can use "double quotes" here',
  lineBreaks: "Look, Mom! \
No \\n's!",
  hexadecimal: 0xdecaf,
  leadingDecimalPoint: .8675309, andTrailing: 8675309.,
  positiveSign: +1,
  trailingComma: 'in objects', andIn: ['arrays',],
  "backwardsCompatible": "with JSON",
}
```

```python
import qjson5

with open("in.json5") as f_in:
    with open("out.json", "w") as f_out:
        qjson5.dump(qjson5.load(f_in), f_out, indent=4)
```

```json
{
    "unquoted": "and you can quote me on that",
    "singleQuotes": "I can use \"double quotes\" here",
    "lineBreaks": "Look, Mom! No \\n's!",
    "hexadecimal": 912559,
    "leadingDecimalPoint": 0.8675309,
    "andTrailing": 8675309,
    "positiveSign": 1,
    "trailingComma": "in objects",
    "andIn": [
        "arrays"
    ],
    "backwardsCompatible": "with JSON"
}
```

## Benchmark

Comparing with the other JSON5 libraries:

* [pyjson5](https://pypi.org/project/pyjson5/)
* [json5](https://pypi.org/project/json5/)

Non-JSON5 library:

* [json](https://docs.python.org/3/library/json.html) (built-in)

```bash
==== JSON Libraries Dump+Load Benchmark ====

--- Data Set: SMALL (10 keys per level, 2 levels) ---
json5        => avg: 5274.9761 ms (std: 74.0018 ms) over 1000 iterations
builtin-json => avg: 14.1933 ms (std: 0.5063 ms) over 1000 iterations
pyjson5      => avg: 11.7564 ms (std: 0.1826 ms) over 1000 iterations
qjson5       => avg: 8.4363 ms (std: 0.0669 ms) over 1000 iterations

--- Data Set: MEDIUM (100 keys per level, 10 levels) ---
json5        => Skipped
builtin-json => avg: 150.4009 ms (std: 1.8448 ms) over 500 iterations
pyjson5      => avg: 147.5031 ms (std: 0.8454 ms) over 500 iterations
qjson5       => avg: 98.7803 ms (std: 0.3422 ms) over 500 iterations

--- Data Set: LARGE (1000 keys per level, 50 levels) ---
json5        => Skipped
builtin-json => avg: 150.6014 ms (std: 3.4461 ms) over 10 iterations
pyjson5      => Error: Maximum nesting level exceeded near 4642
qjson5       => avg: 99.5796 ms (std: 0.9641 ms) over 10 iterations
```

See `scripts/benchmark.py` for benchmarking details.
