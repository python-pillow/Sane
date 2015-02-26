*************************
python-sane documentation
*************************

The sane module is an Python interface to the SANE (Scanning is Now Easy)
library, which provides access to various raster scanning devices such as
flatbed scanners and digital cameras.  For more information about SANE, consult
the SANE website at `www.sane-project.org <http://www.sane-project.org>`_. 
Note that this documentation doesn't duplicate all the information in the SANE
documentation, which you must also consult to get a complete understanding.

This module has been originally developed by
`A.M. Kuchling <mailto:amk1@erols.com>`_, it is currently maintained by
`Sandro Mani <mailto:manisandro@gmail.com>`_.

.. contents::
   :local:
   :depth: 1

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
