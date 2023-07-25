*************************
python-sane documentation
*************************

The sane module is a Python interface to the SANE (Scanner Access Now Easy)
library, which provides access to various raster scanning devices such as
flatbed scanners and digital cameras.  For more information about SANE, consult
the SANE website at `www.sane-project.org <http://www.sane-project.org>`_. 
Note that this documentation doesn't duplicate all the information in the SANE
documentation, which you must also consult to get a complete understanding.

This module has been originally developed by
`A.M. Kuchling <http://amk.ca/>`_, it is currently maintained by
`Sandro Mani <mailto:manisandro@gmail.com>`_.

.. contents::
   :local:
   :depth: 1

Installation
============

Basic Installation using pip
----------------------------
Before you begin, ensure that the libsane-dev package is installed on your system. This is a required dependency.

On Debian-based system (like Ubuntu), you can install libsane-dev using the apt package manager.

    apt install libsane-dev

Then install the packages via pip

    pip install python-sane


Building from sources
---------------------

You can find the instructions on how to build and install from sources in the main README.rst


Indices
=======

* :ref:`genindex`
* :ref:`search`

Reference
=========

.. automodule:: sane
   :members: init, get_devices, open, exit

.. autoclass:: sane.SaneDev
   :members:

.. autoclass:: sane.Option
   :members:

.. autoclass:: sane._SaneIterator
   :members:
   

Example
=======

.. literalinclude:: ../example.py
   :language: python
   :linenos:
