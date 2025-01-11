import qjson5


def test_readme():
    data = {"key": "value", "array": [1, 2, 3], "object": {"nested": "value"}}

    encoded = qjson5.dumps(data)
    print(f"Encoded: {encoded}")

    decoded = qjson5.loads(encoded)
    print(f"Decoded: {decoded}")

    assert data == decoded, f"Different data: {data} != {decoded}"
