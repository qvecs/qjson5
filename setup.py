from setuptools import Extension, setup

with open("README.md", encoding="utf-8") as f:
    long_desc = f.read()

setup(
    name="qjson5",
    version="1.0.3",
    author="Quick Vectors",
    author_email="felipe@qvecs.com",
    description="JSON5 parser for Python written in C.",
    long_description=long_desc,
    long_description_content_type="text/markdown",
    url="https://github.com/qvecs/qjson5",
    project_urls={
        "Bug Tracker": "https://github.com/qvecs/qjson5/issues",
        "Source Code": "https://github.com/qvecs/qjson5",
    },
    license="MIT",
    python_requires=">=3.10",
    include_package_data=True,
    packages=["qjson5"],
    classifiers=[
        "Programming Language :: Python :: 3",
        "Programming Language :: C",
        "Programming Language :: Python :: Implementation :: CPython",
        "License :: OSI Approved :: MIT License",
        "Operating System :: OS Independent",
        "Topic :: Software Development :: Libraries",
    ],
    ext_modules=[
        Extension(
            name="qjson5.py_json5",
            sources=[
                "qjson5/json5.c",
                "qjson5/py_json5.c",
            ],
            include_dirs=["qjson5"],
            extra_compile_args=[
                "-std=gnu17",
                "-Ofast",
                "-flto",
                "-fomit-frame-pointer",
                "-funroll-loops",
                "-ffast-math",
                "-fstrict-aliasing",
            ],
        )
    ],
    extras_require={"dev": ["pytest"]},
)
