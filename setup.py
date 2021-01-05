import setuptools

with open("README.rst", "r", encoding="utf-8") as fh:
    long_description = fh.read()

sane = setuptools.Extension('_sane',
                 include_dirs=[],
                 libraries=['sane'],
                 define_macros=[],
                 extra_compile_args=[],
                 sources=['_sane.c'])

setuptools.setup(name='python-sane',
      version='2.9.1',
      description='This is the python-sane package',
      long_description=long_description,
      long_description_content_type="text/x-rst",
      url='https://github.com/python-pillow/Sane',
      maintainer='Sandro Mani',
      maintainer_email='manisandro@gmail.com',
      packages=setuptools.find_packages(),
      classifiers=[
          "Programming Language :: Python :: 3",
          "Programming Language :: Python :: 3.6",
          "Programming Language :: Python :: 3.7",
          "Programming Language :: Python :: 3.8",
          "Programming Language :: Python :: 3.9",
          "Programming Language :: Python :: 3 :: Only",
          "Programming Language :: Python :: Implementation :: CPython",
          "Programming Language :: Python :: Implementation :: PyPy",
          "Topic :: Multimedia :: Graphics",
          "Topic :: Multimedia :: Graphics :: Capture :: Scanners",
      ],
      python_requires=">=3.6",
      py_modules=['sane'],
      ext_modules=[sane])
