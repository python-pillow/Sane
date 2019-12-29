from distutils.core import setup, Extension

sane = Extension('_sane',
                 include_dirs=[],
                 libraries=['sane'],
                 define_macros=[],
                 extra_compile_args=[],
                 sources=['_sane.c'])

setup(name='python-sane',
      version='2.8.3',
      description='This is the python-sane package',
      url='https://github.com/python-pillow/Sane',
      maintainer='Sandro Mani',
      maintainer_email='manisandro@gmail.com',
      classifiers=[
          "Programming Language :: Python :: 3",
          "Programming Language :: Python :: 3.5",
          "Programming Language :: Python :: 3.6",
          "Programming Language :: Python :: 3.7",
          "Programming Language :: Python :: 3.8",
          "Programming Language :: Python :: 3 :: Only",
          "Programming Language :: Python :: Implementation :: CPython",
          "Programming Language :: Python :: Implementation :: PyPy",
          "Topic :: Multimedia :: Graphics",
          "Topic :: Multimedia :: Graphics :: Capture :: Scanners",
      ],
      python_requires=">=3.5",
      py_modules=['sane'],
      ext_modules=[sane])
