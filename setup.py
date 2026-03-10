#!/usr/bin/env python3

from pathlib import Path

from setuptools import Extension, setup

ROOT = Path(__file__).resolve().parent


rpygrating_module = Extension(
    "_rpigratings",
    sources=["rpg/_rpigratings.c"],
    extra_compile_args=["-O3"],
    libraries=["wiringPi"],
)


setup(
    name="rpg",
    version="1.1",
    description="A drifting grating implementation",
    long_description=(ROOT / "README.md").read_text(encoding="utf-8"),
    long_description_content_type="text/markdown",
    packages=["rpg"],
    ext_modules=[rpygrating_module],
    python_requires=">=3.12",
)
