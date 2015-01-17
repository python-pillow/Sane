from distutils.core import setup, Extension
import distutils.sysconfig
import os

defs = []
extra_compile_args =  []
include_dirs = [os.path.join(distutils.sysconfig.get_python_inc(), "Imaging")]
try:
    import numarray
    defs.append(('WITH_NUMARRAY',None))
except ImportError:
    pass
try:
    import numpy
    defs.append(('WITH_NUMPY',None))
    extra_compile_args.append('-Wunused-function')
    include_dirs.append(numpy.get_include())
except ImportError:
    pass

sane = Extension('_sane',
                 include_dirs = include_dirs,
                 libraries = ['sane'],
                 define_macros = defs,
                 extra_compile_args = extra_compile_args,
                 sources = ['_sane.c'])

setup (name = 'pysane',
       version = '2.0',
       description = 'This is the pysane package',
       py_modules = ['sane'],
       ext_modules = [sane])
