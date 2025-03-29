import io
import textwrap

import qjson5


def test_boolean_and_null():
    data = """
    {
      // Single line comment`
      "a": true,
      "b": false,
      "c": null
    }
    """
    parsed = qjson5.loads(data)
    assert len(parsed) == 3
    assert parsed["a"] is True
    assert parsed["b"] is False
    assert parsed["c"] is None


def test_simple_numbers():
    data = """
    {
      "int": 42,
      "float": 3.14,
      "negative": -100,
      "scientific": 1.2e3
    }
    """
    parsed = qjson5.loads(data)
    assert len(parsed) == 4
    assert parsed["int"] == 42
    assert abs(parsed["float"] - 3.14) < 1e-9
    assert parsed["negative"] == -100
    assert abs(parsed["scientific"] - 1200) < 1e-9


def test_strings_and_unquoted_keys():
    data = """
    {
      /*
      Multi-line comment
      */
      unquotedKey: 'Hello World', // Single-line comment
      "quotedKey": "Another String"
    }
    """
    parsed = qjson5.loads(data)
    assert len(parsed) == 2
    assert parsed["unquotedKey"] == "Hello World"
    assert parsed["quotedKey"] == "Another String"


def test_trailing_commas():
    data = """
    {
      "one": 1,
      "two": 2,
    }
    """
    parsed = qjson5.loads(data)
    assert len(parsed) == 2
    assert parsed["one"] == 1
    assert parsed["two"] == 2

    data2 = """
    [
      1,
      2,
      3,
    ]
    """
    parsed2 = qjson5.loads(data2)
    assert len(parsed2) == 3
    assert parsed2 == [1, 2, 3]


def test_array_handling():
    data = "[1, 2, 3, /* comment */ 4, 5]"
    parsed = qjson5.loads(data)
    assert len(parsed) == 5
    assert parsed == [1, 2, 3, 4, 5]


def test_dump_basic():
    original = {"a": 1, "b": True, "c": None, "d": [1, 2, False]}
    dumped = qjson5.dumps(original)
    parsed = qjson5.loads(dumped)
    assert len(parsed) == 4
    assert parsed == original


def test_dump_with_indent():
    data = {"key": "value", "list": [1, 2]}
    dumped = qjson5.dumps(data, indent=2)
    parsed = qjson5.loads(dumped)
    assert len(parsed) == 2
    assert parsed == data


def test_unquoted_round_trip():
    data = """{ unquoted: 123 }"""
    parsed = qjson5.loads(data)
    assert len(parsed) == 1
    assert parsed["unquoted"] == 123

    dumped = qjson5.dumps(parsed)
    parsed_again = qjson5.loads(dumped)
    assert len(parsed_again) == 1
    assert parsed_again == parsed


def test_empty_object_and_array():
    data_obj = "{}"
    parsed_obj = qjson5.loads(data_obj)
    assert len(parsed_obj) == 0
    assert parsed_obj == {}

    data_arr = "[]"
    parsed_arr = qjson5.loads(data_arr)
    assert len(parsed_arr) == 0
    assert parsed_arr == []


def test_nested_structures():
    data = """
    {
      // nested object
      "obj": {
      "x": 10,
      "y": [1, 2, 3]
      },
      // nested array
      "arr": [
      {
        "foo": "bar"
      },
      42,
      true
      ]
    }
    """
    parsed = qjson5.loads(data)
    assert len(parsed) == 2
    assert len(parsed["obj"]) == 2
    assert len(parsed["obj"]["y"]) == 3
    assert len(parsed["arr"]) == 3
    assert parsed["obj"]["x"] == 10
    assert parsed["obj"]["y"] == [1, 2, 3]
    assert parsed["arr"][0] == {"foo": "bar"}
    assert parsed["arr"][1] == 42
    assert parsed["arr"][2] is True


def test_string_escapes():
    data = r"""
    {
      "simple": "Hello\nWorld",
      "tabs": "Hello\tWorld",
      "quotes": "He said \"Hi\"",
      "backslash": "C:\\Users\\Name"
    }
    """
    parsed = qjson5.loads(data)
    assert len(parsed) == 4
    assert parsed["simple"] == "Hello\nWorld"
    assert parsed["tabs"] == "Hello\tWorld"
    assert parsed["quotes"] == 'He said "Hi"'
    assert parsed["backslash"] == r"C:\Users\Name"


def test_large_numbers():
    data = """
    {
      "largePositive": 9007199254740991,
      "largeNegative": -9007199254740991,
      "floatEdge": 9007199254740991.0
    }
    """
    parsed = qjson5.loads(data)
    assert len(parsed) == 3
    assert parsed["largePositive"] == 9007199254740991
    assert parsed["largeNegative"] == -9007199254740991
    assert parsed["floatEdge"] == 9007199254740991


def test_plus_sign_number():
    data = """
    {
      "plusNumber": +42
    }
    """
    parsed = qjson5.loads(data)
    assert len(parsed) == 1
    assert parsed["plusNumber"] == 42


def test_unicode_characters():
    data = '{"unicode": "Hello \\u00F1 World", "emoji": "Smile \\uD83D\\uDE03"}'
    parsed = qjson5.loads(data)
    assert len(parsed) == 2
    assert "unicode" in parsed
    assert "emoji" in parsed


def test_mixed_types_round_trip():
    original = {
        "null": None,
        "bool": True,
        "int": 42,
        "float": 3.14,
        "str": 'Hello "World"!\nLine2',
        "list": [1, "two", {"nested": False}],
        "obj": {"x": -1.23, "y": [True, None]},
    }
    dumped = qjson5.dumps(original, indent=2)
    parsed = qjson5.loads(dumped)
    assert len(parsed) == 7
    assert len(parsed["list"]) == 3
    assert len(parsed["obj"]) == 2
    assert parsed == original


def test_file_load_and_dump():
    original = {"test": 123, "nested": {"foo": True, "bar": None}}
    buf = io.StringIO()
    qjson5.dump(original, buf, indent=2)
    contents = buf.getvalue()
    assert isinstance(contents, str)
    assert contents.strip() != ""

    buf2 = io.StringIO(contents)
    parsed = qjson5.load(buf2)
    assert len(parsed) == 2
    assert len(parsed["nested"]) == 2
    assert parsed == original


def test_json5_docs():
    data = textwrap.dedent(r"""{
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
    """)
    parsed = qjson5.loads(data)
    assert len(parsed) == 10
    assert parsed["unquoted"] == "and you can quote me on that"
    assert parsed["singleQuotes"] == 'I can use "double quotes" here'
    assert parsed["lineBreaks"] == "Look, Mom! No \\n's!"
    assert parsed["hexadecimal"] == 0xDECAF
    assert abs(parsed["leadingDecimalPoint"] - 0.8675309) < 1e-9
    assert abs(parsed["andTrailing"] - 8675309.0) < 1e-9
    assert parsed["positiveSign"] == 1
    assert parsed["trailingComma"] == "in objects"
    assert parsed["andIn"] == ["arrays"]
    assert parsed["backwardsCompatible"] == "with JSON"
