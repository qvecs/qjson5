"""
Testing the following libraries:

1) qjson5
2) pyjson5 (https://pypi.org/project/pyjson5/)
3) json5 (https://pypi.org/project/json5/)
4) json (builtin)

We test on three different data sets:
    1) Small: 10 keys per level, 2 levels deep
    2) Medium: 100 keys per level, 10 levels deep
    3) Large: 1000 keys per level, 50 levels deep

To run:
    virtualenv .venv
    source .venv/bin/activate
    pip install qjson5 pyjson5 json5
    python scripts/benchmark.py
"""

import json
import random
import statistics
import string
import time

import json5
import pyjson5

import qjson5

random.seed(0)


def random_string(length=8):
    return "".join(random.choices(string.ascii_letters, k=length))


def generate_data(count: int, max_depth: int = 1, current_depth: int = 0):
    if current_depth >= max_depth:
        return {random_string(): random_string() for _ in range(count)}
    else:
        d = {}
        for _ in range(count):
            key = random_string()
            if random.randint(0, 1):
                d[key] = random_string()
            else:
                d[key] = generate_data(random.randint(1, 3), max_depth, current_depth + 1)
        return d


def dump_load_builtin_json(data, num_iterations: int):
    for _ in range(num_iterations):
        encoded = json.dumps(data)
        _ = json.loads(encoded)


def dump_load_pyjson5(data, num_iterations: int):
    for _ in range(num_iterations):
        encoded = pyjson5.dumps(data)
        _ = pyjson5.loads(encoded)


def dump_load_json5(data, num_iterations: int):
    for _ in range(num_iterations):
        encoded = json5.dumps(data)
        _ = json5.loads(encoded)


def dump_load_qjson5(data, num_iterations: int):
    for _ in range(num_iterations):
        encoded = qjson5.dumps(data)
        _ = qjson5.loads(encoded)


def time_library(label: str, func, data, num_iterations: int) -> float:
    t0 = time.perf_counter()
    func(data, num_iterations)
    t1 = time.perf_counter()
    return t1 - t0


def run_benchmark(
    num_iterations_small=1000,
    num_data_count_small=10,
    num_data_nested_small=2,
    num_iterations_medium=500,
    num_data_count_medium=100,
    num_data_nested_medium=10,
    num_iterations_large=10,
    num_data_count_large=1000,
    num_data_nested_large=50,
    num_repeats=5,
):
    print("==== JSON Libraries Dump+Load Benchmark ====\n")

    data_sets = [
        (
            "SMALL",
            generate_data(num_data_count_small, num_data_nested_small),
            num_iterations_small,
            num_data_count_small,
            num_data_nested_small,
        ),
        (
            "MEDIUM",
            generate_data(num_data_count_medium, num_data_nested_medium),
            num_iterations_medium,
            num_data_count_medium,
            num_data_nested_medium,
        ),
        (
            "LARGE",
            generate_data(num_data_count_large, num_data_nested_large),
            num_iterations_large,
            num_data_count_large,
            num_data_nested_large,
        ),
    ]

    libs = [
        ("json5", dump_load_json5, ["SMALL", "MEDIUM", "LARGE"]),
        ("builtin-json", dump_load_builtin_json, []),
        ("pyjson5", dump_load_pyjson5, []),
        ("qjson5", dump_load_qjson5, []),
    ]

    for label, data_obj, iterations, count, nested in data_sets:
        print(f"--- Data Set: {label} ({count} keys per level, {nested} levels) ---")
        for lib_label, func, skip in libs:
            if label in skip:
                print(f"{lib_label:12s} => Skipped")
                continue

            timings = []
            try:
                for _ in range(num_repeats):
                    duration = time_library(lib_label, func, data_obj, iterations)
                    timings.append(duration)
            except Exception as e:
                if getattr(e, "message"):
                    e = e.message
                print(f"{lib_label:12s} => Error: {e}")
                continue
            avg_time = statistics.mean(timings)
            sd_time = statistics.pstdev(timings)

            # Convert seconds -> milliseconds
            avg_ms = avg_time * 1000
            sd_ms = sd_time * 1000
            print(f"{lib_label:12s} => avg: {avg_ms:.4f} ms (std: {sd_ms:.4f} ms) over {iterations} iterations")

        print()


if __name__ == "__main__":
    run_benchmark(
        num_iterations_small=1000,
        num_iterations_medium=500,
        num_iterations_large=10,
        num_repeats=5,
    )
