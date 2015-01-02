
Version 2.7.0
-------------

- Split from Pillow to it's own repo

- _sane.c : 
  - support for numpy (numarray still possible), define ARRAY_SUPPORT
  - arr_scan allows colors array using numpy
  _checkArray_Support : answer as tupple the array_support at compilation and
    if libraries are found at execution

- sane.py:
  - arr_scan allows colors array using numpy
  
- setup.py:
  - define WITH_NUMPY if available

- demo_numpy:
  - various call of arr_snap except 16bit colors not supported by Epkowa or libsane



from V1.0 to V2.0
-----------------

- _sane.c:
  - Values for option constraints are correctly translated to floats
    if value type is TYPE_FIXED for SANE_CONSTRAINT_RANGE and
    SANE_CONSTRAINT_WORD_LIST
  - added constants INFO_INEXACT, INFO_RELOAD_OPTIONS,
    INFO_RELOAD_PARAMS (possible return values of set_option())
    to module dictionnary.
  - removed additional return variable 'i' from SaneDev_get_option(),
    because it is only set when SANE_ACTION_SET_VALUE is used.
  - scanDev.get_parameters() now returns the scanner mode as 'format',
    no more the typical PIL codes. So 'L' became 'gray', 'RGB' is now
    'color', 'R' is 'red', 'G' is 'green', 'B' is 'red'. This matches
    the way scanDev.mode is set.
    This should be the only incompatibility vs. version 1.0.

- sane.py
  - ScanDev got new method __load_option_dict() called from __init__()
    and from __setattr__() if backend reported that the frontend should
    reload the options.
  - Nice human-readable __repr__() method added for class Option
  - if __setattr__ (i.e. set_option) reports that all other options
    have to be reloaded due to a change in the backend then they are reloaded.
  - due to the change in SaneDev_get_option() only the 'value' is
    returned from get_option().
  - in __setattr__ integer values are automatically converted to floats
    if SANE backend expects SANE_FIXED (i.e. fix-point float)
  - The scanner options can now directly be accessed via scanDev[optionName]
    instead scanDev.opt[optionName]. (The old way still works).

V1.0
----
-  A.M. Kuchling's original pysane package.
