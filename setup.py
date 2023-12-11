from setuptools import Extension, setup

ext_modules = [
    Extension(
        "_sane",
        include_dirs=[],
        libraries=["sane"],
        define_macros=[],
        extra_compile_args=[],
        sources=["_sane.c"],
    )
]

setup(ext_modules=ext_modules)
