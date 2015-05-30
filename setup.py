from distutils.core import setup, Extension

sane = Extension('_sane',
                 include_dirs = [],
                 libraries = ['sane'],
                 define_macros = [],
                 extra_compile_args = [],
                 sources = ['_sane.c'])

setup (name = 'python-sane',
       version = '2.8.1',
       description = 'This is the python-sane package',
       py_modules = ['sane'],
       ext_modules = [sane])
