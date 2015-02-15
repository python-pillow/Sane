# sane.py
#
# Python wrapper on top of the _sane module, which is in turn a very
# thin wrapper on top of the SANE library.  For a complete understanding
# of SANE, consult the documentation at the SANE home page:
# http://www.sane-project.org/docs.html

__version__ = '2.8.0'
__author__ = ['Andrew Kuchling', 'Ralph Heinkel', 'Sandro Mani']

from PIL import Image
import _sane

TYPE_STR = {_sane.TYPE_BOOL:   "TYPE_BOOL",   _sane.TYPE_INT:    "TYPE_INT",
            _sane.TYPE_FIXED:  "TYPE_FIXED",  _sane.TYPE_STRING: "TYPE_STRING",
            _sane.TYPE_BUTTON: "TYPE_BUTTON", _sane.TYPE_GROUP:  "TYPE_GROUP"}

UNIT_STR = {_sane.UNIT_NONE:        "UNIT_NONE",
            _sane.UNIT_PIXEL:       "UNIT_PIXEL",
            _sane.UNIT_BIT:         "UNIT_BIT",
            _sane.UNIT_MM:          "UNIT_MM",
            _sane.UNIT_DPI:         "UNIT_DPI",
            _sane.UNIT_PERCENT:     "UNIT_PERCENT",
            _sane.UNIT_MICROSECOND: "UNIT_MICROSECOND"}


class Option:
    """
    Class representing a SANE option. These are returned by a __getitem__
    lookup of an option on the device, i.e.

        option = scanner["mode"]

    The Option class has the following attributes:
    index -- number from 0 to n, giving the option number
    name -- a string uniquely identifying the option
    title -- single-line string containing a title for the option
    desc -- a long string describing the option, useful as a help message
    type -- type of this option. Possible values: TYPE_BOOL, TYPE_INT,
            TYPE_STRING, etc.
    unit -- units of this option. Possible values: UNIT_NONE, UNIT_PIXEL, etc.
    size -- size of the value in bytes
    cap -- capabilities available; CAP_EMULATED, CAP_SOFT_SELECT, etc.
    constraint -- constraint on values. Possible values:
                  None : No constraint
                  (min,max,step) : Range
                  list of integers or strings: listed of permitted values
    """

    def __init__(self, args, scanDev):
        self.scanDev = scanDev  # needed to get current value of this option
        self.index, self.name = args[0], args[1]
        self.title, self.desc = args[2], args[3]
        self.type, self.unit = args[4], args[5]
        self.size, self.cap = args[6], args[7]
        self.constraint = args[8]

        if not isinstance(self.name, str):
            self.py_name = str(self.name)
        else:
            self.py_name = self.name.replace("-", "_")

    def is_active(self):
        """
        Return whether the option is active.
        """
        return _sane.OPTION_IS_ACTIVE(self.cap)

    def is_settable(self):
        """
        Return whether the option is settable.
        """
        return _sane.OPTION_IS_SETTABLE(self.cap)

    def __repr__(self):
        if self.is_settable():
            settable = 'yes'
        else:
            settable = 'no'
        if self.is_active():
            active = 'yes'
            curValue = repr(getattr(self.scanDev, self.py_name))
        else:
            active = 'no'
            curValue = '<not available, inactive option>'
        s = ("\n"
             "Name:      %s\n"
             "Cur value: %s\n"
             "Index:     %d\n"
             "Title:     %s\n"
             "Desc:      %s\n"
             "Type:      %s\n"
             "Unit:      %s\n"
             "Constr:    %s\n"
             "active:    %s\n"
             "settable:  %s\n" % (self.py_name, curValue, self.index,
                                  self.title, self.desc, TYPE_STR[self.type],
                                  UNIT_STR[self.unit], repr(self.constraint),
                                  active, settable))
        return s


class _SaneIterator:
    """
    Iterator for ADF scans.
    """

    def __init__(self, device):
        self.device = device

    def __iter__(self):
        return self

    def __del__(self):
        self.device.cancel()

    def next(self):
        try:
            self.device.start()
        except Exception as e:
            if e == 'Document feeder out of documents':
                raise StopIteration
            else:
                raise
        return self.device.snap(True)


class SaneDev:
    """
      Class representing a SANE device. Besides the functions documented below,
      the class has some special attributes which can be read:

      devname        -- the scanner device name (as passed to sane.open())
      sane_signature -- the tuple (devname, brand, name, type)
      scanner_model  -- the tuple (brand, name)
      opt            -- dictionary of options
      optlist        -- list of option names
      area           -- scan area

      Furthermore, the scanner options are also exposed as attributes, which can
      be read or set by using the option name as attribute name, i.e.:

          print scanner.mode
          scanner.mode = 'Color'

      An Option object for a scanner option can be retreived via __getitem__
      lookup, i.e.:

          option = scanner['mode']

    """
    def __init__(self, devname):
        d = self.__dict__
        d['devname'] = devname
        d['dev'] = _sane._open(devname)
        self.__load_option_dict()

    def __get_sane_signature(self):
        d = self.__dict__
        if 'sane_signature' not in d:
            devices = _sane.get_devices()
            if devices:
                for dev in devices:
                    if d['devname'] == dev[0]:
                        d['sane_signature'] = dev
                        break
        if 'sane_signature' not in d:
            raise RuntimeError("No such scan device '%s'" % d['devname'])
        return d['sane_signature']

    def __load_option_dict(self):
        d = self.__dict__
        d['opt'] = {}
        for t in d['dev'].get_options():
            o = Option(t, self)
            if o.type != _sane.TYPE_GROUP:
                d['opt'][o.py_name] = o

    def __setattr__(self, key, value):
        d = self.__dict__
        if key in ('dev', 'optlist', 'area', 'sane_signature', 'scanner_model'):
            raise AttributeError("Read-only attribute: " + key)

        if key not in self.opt:
            d[key] = value
            return

        opt = d['opt'][key]
        if opt.type == _sane.TYPE_BUTTON:
            raise AttributeError("Buttons don't have values: " + key)
        if opt.type == _sane.TYPE_GROUP:
            raise AttributeError("Groups don't have values: " + key)
        if not _sane.OPTION_IS_ACTIVE(opt.cap):
            raise AttributeError("Inactive option: " + key)
        if not _sane.OPTION_IS_SETTABLE(opt.cap):
            raise AttributeError("Option can't be set by software: " + key)
        if isinstance(value, int) and opt.type == _sane.TYPE_FIXED:
            # avoid annoying errors of backend if int is given instead float:
            value = float(value)
        result = d['dev'].set_option(opt.index, value)
        # do binary AND to find if we have to reload options:
        if result & _sane.INFO_RELOAD_OPTIONS:
            self.__load_option_dict()

    def __getattr__(self, key):
        d = self.__dict__
        if key == 'optlist':
            return list(self.opt.keys())
        if key == 'area':
            return (self.tl_x, self.tl_y), (self.br_x, self.br_y)
        if key == 'sane_signature':
            return self.__get_sane_signature()
        if key == 'scanner_model':
            return self.__get_sane_signature()[1:3]
        if key in d:
            return d[key]
        if key not in d['opt']:
            raise AttributeError("No such attribute: " + key)
        opt = d['opt'][key]
        if opt.type == _sane.TYPE_BUTTON:
            raise AttributeError("Buttons don't have values: " + key)
        if opt.type == _sane.TYPE_GROUP:
            raise AttributeError("Groups don't have values: " + key)
        if not _sane.OPTION_IS_ACTIVE(opt.cap):
            raise AttributeError("Inactive option: " + key)
        return d['dev'].get_option(opt.index)

    def __getitem__(self, key):
        return self.opt[key]

    def get_parameters(self):
        """
        Return a 5-tuple holding all the current device settings:
        (format, last_frame, (pixels_per_line, lines), depth, bytes_per_line)

        format -- one of "grey", "color", "red", "green", "blue" or "unknown format".
        last_frame -- whether this is the last frame of a multi frame image
        pixels_per_line -- width of the scanned image
        lines -- height of the scanned image
        depth -- gives number of bits per sample
        bytes_per_line -- the number of bytes per line
        """
        return self.__dict__['dev'].get_parameters()

    def get_options(self):
        """"
        Return a list of tuples describing all the available options.
        """
        return self.dev.get_options()

    def start(self):
        """"
        Initiate a scanning operation. This can throw a _sane.error if an
        invalid value is set for an option.
        """
        self.dev.start()

    def cancel(self):
        """"
        Cancel an in-progress scanning operation.
        """
        self.dev.cancel()

    def snap(self, no_cancel=False):
        """
        Read image data and return a PIL.Image object. An RGB image is returned
        for multi-band images, a L image for single-band images.
        No no_cancel is used for ADF scans by _SaneIterator.
        """
        (data, width, height, samples, sampleSize) = self.dev.snap(no_cancel)
        if not data:
            raise RuntimeError("Scanner returned no data")
        mode = 'RGB' if samples == 3 else 'L'
        return Image.frombuffer(mode, (width, height), buffer(data), "raw", mode, 0, 1)

    def scan(self):
        """
        Convenience method which calls start followed by snap.
        """
        self.start()
        return self.snap()

    def arr_snap(self):
        """
        Read image data and return a 2d numpy array. For single-band images,
        the array shape will be (width, heigth), for multi-band images, the
        array shape will be (nbands * width, height).
        """
        try:
            import numpy
        except:
            raise RuntimeError("Cannot import numpy")
        (data, width, height, samples, sampleSize) = self.dev.snap(False, True)
        if not data:
            raise RuntimeError("Scanner returned no data")
        if sampleSize == 1:
            np = numpy.frombuffer(data, numpy.uint8)
        elif sampleSize == 2:
            np = numpy.frombuffer(data, numpy.uint16)
        else:
            raise RuntimeError("Unexpected sample size: %d" % sampleSize)
        return numpy.reshape(np, (samples * width, height))

    def arr_scan(self):
        """
        Convenience method which calls start followed by arr_snap.
        """
        self.start()
        return self.arr_snap()

    def multi_scan(self):
        """
        Return a _SaneIterator for ADF scans.
        """
        return _SaneIterator(self)

    def fileno(self):
        """"
        Return the file descriptor for the scanning device.
        """
        return self.dev.fileno()

    def close(self):
        """
        Close the scanning device.
        """
        self.dev.close()


def init():
    """
    Initialize sane. Returns a tuple (sane_ver, ver_maj, ver_min, ver_patch).
    """
    return _sane.init()


def get_devices():
    """
    Return a list of 4-tuples containing the available scanning devices.
    Each tuple is of the format (device_name, vendor, model, type).

    device_name -- the device name, suitable for passing to open()
    vendor -- the device vendor
    mode -- the device model vendor
    type -- the device type, such as 'virtual device' or 'video camera'
    """
    return _sane.get_devices()


def open(devname):
    """"
    Open a device for scanning. Suitable values for devname are returned in the
    first item of the tuples returned by get_devices().
    Raises a _sane.error on error. Returns a SaneDev object on success.
    """
    return SaneDev(devname)


def exit():
    """
    Exit sane.
    """
    _sane.exit()
