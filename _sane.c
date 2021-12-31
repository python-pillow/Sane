/***********************************************************
(C) Copyright 2003 A.M. Kuchling.  All Rights Reserved
(C) Copyright 2004 A.M. Kuchling, Ralph Heinkel  All Rights Reserved
(C) Copyright 2013-2022 Sandro Mani  All Rights Reserved

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose and without fee is hereby granted,
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in
supporting documentation, and that the name of A.M. Kuchling and
Ralph Heinkel not be used in advertising or publicity pertaining to
distribution of the software without specific, written prior permission.

A.M. KUCHLING, R.H. HEINKEL S. MANI DISCLAIM ALL WARRANTIES WITH REGARD TO THIS
SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS,
IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF
USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
PERFORMANCE OF THIS SOFTWARE.

******************************************************************/
/*
 *   added support for arrays using numpy :
 *                          dec. 13th, 2014, J.F. Berar 
 */


#include <Python.h>
#include <sane/sane.h>

#include <sys/time.h>

#define RAISE_IF(test, message) \
  if(test) \
    { \
      PyErr_SetString(ErrorObject, message); \
      return NULL; \
    }

static PyObject *ErrorObject;

typedef struct {
  PyObject_HEAD
  SANE_Handle h;
} SaneDevObject;

static PyTypeObject SaneDev_Type;

static int g_sane_initialized = 0;

/* Raise a SANE exception */
static PyObject *
PySane_Error(SANE_Status st)
{
  PyErr_SetString(ErrorObject, sane_strstatus(st));
  return NULL;
}

/* SaneDev methods */

static void
SaneDev_dealloc(SaneDevObject *self)
{
  if(self->h && g_sane_initialized)
    sane_close(self->h);
  self->h = NULL;
  PyObject_DEL(self);
}

static PyObject *
SaneDev_close(SaneDevObject *self, PyObject *args)
{
  if(!PyArg_ParseTuple(args, ""))
    return NULL;

  if(self->h)
    sane_close(self->h);
  self->h = NULL;
  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject *
SaneDev_get_parameters(SaneDevObject *self, PyObject *args)
{
  SANE_Status st;
  SANE_Parameters p;
  char *format;

  if(!PyArg_ParseTuple(args, ""))
    return NULL;

  RAISE_IF(self->h == NULL, "SaneDev object is closed");

  Py_BEGIN_ALLOW_THREADS
  st = sane_get_parameters(self->h, &p);
  Py_END_ALLOW_THREADS

  if(st != SANE_STATUS_GOOD)
    return PySane_Error(st);
  switch(p.format)
    {
      case(SANE_FRAME_GRAY):  format="gray"; break;
      case(SANE_FRAME_RGB):   format="color"; break;
      case(SANE_FRAME_RED):   format="red"; break;
      case(SANE_FRAME_GREEN): format="green"; break;
      case(SANE_FRAME_BLUE):  format="blue"; break;
      default:                format="unknown format"; break;
    }

  return Py_BuildValue("si(ii)ii", format, p.last_frame, p.pixels_per_line,
                       p.lines, p.depth, p.bytes_per_line);
}

static PyObject *
SaneDev_fileno(SaneDevObject *self, PyObject *args)
{
  if(!PyArg_ParseTuple(args, ""))
    return NULL;

  RAISE_IF(self->h == NULL, "SaneDev object is closed");

  SANE_Int fd;
  SANE_Status st = sane_get_select_fd(self->h, &fd);
  if(st != SANE_STATUS_GOOD)
    return PySane_Error(st);
  return PyLong_FromLong(fd);
}

static PyObject *
SaneDev_start(SaneDevObject *self, PyObject *args)
{
  SANE_Status st;

  if(!PyArg_ParseTuple(args, ""))
    return NULL;

  RAISE_IF(self->h == NULL, "SaneDev object is closed");

  Py_BEGIN_ALLOW_THREADS
  st = sane_start(self->h);
  Py_END_ALLOW_THREADS
  if(st != SANE_STATUS_GOOD)
    return PySane_Error(st);
  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject *
SaneDev_cancel(SaneDevObject *self, PyObject *args)
{
  if(!PyArg_ParseTuple(args, ""))
    return NULL;

  RAISE_IF(self->h == NULL, "SaneDev object is closed");

  Py_BEGIN_ALLOW_THREADS
  sane_cancel(self->h);
  Py_END_ALLOW_THREADS
  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject *
SaneDev_get_options(SaneDevObject *self, PyObject *args)
{
  int i = 0, j = 0;

  if(!PyArg_ParseTuple(args, ""))
    return NULL;

  RAISE_IF(self->h == NULL, "SaneDev object is closed");

  PyObject *list = PyList_New(0);
  if(!list)
    return NULL;

  while(1)
    {
      const SANE_Option_Descriptor *d = sane_get_option_descriptor(self->h, i);
      if(!d)
        break;

      PyObject *constraint = NULL;
      switch(d->constraint_type)
        {
          case SANE_CONSTRAINT_NONE:
            Py_INCREF(Py_None);
            constraint = Py_None;
            break;
          case SANE_CONSTRAINT_RANGE:
            if(d->type == SANE_TYPE_INT)
              constraint = Py_BuildValue("iii",
                                         d->constraint.range->min,
                                         d->constraint.range->max,
                                         d->constraint.range->quant);
            else if(d->type == SANE_TYPE_FIXED)
              constraint = Py_BuildValue("ddd",
                                         SANE_UNFIX(d->constraint.range->min),
                                         SANE_UNFIX(d->constraint.range->max),
                                         SANE_UNFIX(d->constraint.range->quant)
                                        );
            break;
          case SANE_CONSTRAINT_WORD_LIST:
            constraint = PyList_New(d->constraint.word_list[0]);
            if(!constraint)
              break;
            if(d->type == SANE_TYPE_INT)
              for(j = 1; j <= d->constraint.word_list[0]; ++j)
                PyList_SetItem(constraint, j - 1,
                                PyLong_FromLong(d->constraint.word_list[j]));
            else if(d->type == SANE_TYPE_FIXED)
              for(j = 1; j <= d->constraint.word_list[0]; ++j)
                PyList_SetItem(constraint, j - 1,
                  PyFloat_FromDouble(SANE_UNFIX(d->constraint.word_list[j])));
            break;
          case SANE_CONSTRAINT_STRING_LIST:
            constraint = PyList_New(0);
            if(!constraint)
              break;
            PyObject *item = NULL;
            for(j = 0; d->constraint.string_list[j] != NULL; j++)
              {
                item = PyUnicode_DecodeLatin1(d->constraint.string_list[j],
                                          strlen(d->constraint.string_list[j]),
                                              NULL);
                PyList_Append(constraint, item);
                Py_XDECREF(item);
              }
            break;
        }
      if(constraint)
        {
          PyObject *value = Py_BuildValue("isssiiiiO", i, d->name, d->title,
                                          d->desc, d->type, d->unit, d->size,
                                          d->cap, constraint);
          PyList_Append(list, value);
          Py_XDECREF(value);
          Py_DECREF(constraint);
        }
      i++;
    }
  return list;
}

static PyObject *
SaneDev_get_option(SaneDevObject *self, PyObject *args)
{
  int n = 0;
  if(!PyArg_ParseTuple(args, "i", &n))
    return NULL;

  RAISE_IF(self->h == NULL, "SaneDev object is closed");

  const SANE_Option_Descriptor *d = sane_get_option_descriptor(self->h, n);
  RAISE_IF(d == NULL, "Invalid option specified");

  void *v = malloc(d->size + 1);
  SANE_Status st = sane_control_option(self->h, n, SANE_ACTION_GET_VALUE, v,
                                       NULL);

  if(st != SANE_STATUS_GOOD)
    {
      free(v);
      return PySane_Error(st);
    }

  PyObject *value = NULL;
  switch(d->type)
    {
    case SANE_TYPE_BOOL:
    case SANE_TYPE_INT:
      value = Py_BuildValue("i", *( (SANE_Int*)v) );
      break;
    case SANE_TYPE_FIXED:
      value = Py_BuildValue("d", SANE_UNFIX((*((SANE_Fixed*)v))) );
      break;
    case SANE_TYPE_STRING:
      value = PyUnicode_DecodeLatin1((const char *) v,
                                   strlen((const char *) v),
                                   NULL);
      break;
    case SANE_TYPE_BUTTON:
    case SANE_TYPE_GROUP:
      value = Py_BuildValue("O", Py_None);
      break;
    default:
      PyErr_SetString(ErrorObject, "Unknown option type");
    }

  free(v);
  return value;
}

static PyObject *
SaneDev_set_option(SaneDevObject *self, PyObject *args)
{
  PyObject *value = NULL;
  int n = 0;
  if(!PyArg_ParseTuple(args, "iO", &n, &value))
    return NULL;

  RAISE_IF(self->h == NULL, "SaneDev object is closed");

  const SANE_Option_Descriptor *d = sane_get_option_descriptor(self->h, n);
  RAISE_IF(d == NULL, "Invalid option specified");

  void *v = malloc(d->size + 1);
  SANE_Word wordval;

  switch(d->type)
    {
    case SANE_TYPE_BOOL:
    case SANE_TYPE_INT:
      if(!PyLong_Check(value))
        {
          PyErr_SetString(PyExc_TypeError, "SANE_INT and SANE_BOOL require an integer");
          free(v);
          return NULL;
        }
      wordval = PyLong_AsLong(value);
      memcpy(v, &wordval, sizeof(SANE_Word));
      break;
    case SANE_TYPE_FIXED:
      if(!PyFloat_Check(value))
        {   
          PyErr_SetString(PyExc_TypeError, "SANE_FIXED requires a floating point number");
          free(v);
          return NULL;
        }
      wordval = SANE_FIX(PyFloat_AsDouble(value));
      memcpy(v, &wordval, sizeof(SANE_Word));
      break;
    case SANE_TYPE_STRING:
      if(!PyUnicode_Check(value))
        {
          PyErr_SetString(PyExc_TypeError, "SANE_STRING requires a string");
          free(v);
          return NULL;
        }
      PyObject *strobj = PyUnicode_AsLatin1String(value);
      if(!strobj)
        {
          PyErr_SetString(PyExc_TypeError, "SANE_STRING requires a latin1 string");
          free(v);
          return NULL;
        }
      strncpy(v, PyBytes_AsString(strobj), d->size - 1);
      ((char*)v)[d->size - 1] = 0;
      Py_DECREF(strobj);
      break;
    case SANE_TYPE_BUTTON:
    case SANE_TYPE_GROUP:
      PyErr_SetString(ErrorObject, "SANE_TYPE_BUTTON and SANE_TYPE_GROUP can't be set");
      free(v);
      return NULL;
    }

  SANE_Int info = 0;
  SANE_Status st = sane_control_option(self->h, n, SANE_ACTION_SET_VALUE, v, &info);
  free(v);
  if(st != SANE_STATUS_GOOD)
    return PySane_Error(st);
  return Py_BuildValue("i", info);
}

static PyObject *
SaneDev_set_auto_option(SaneDevObject *self, PyObject *args)
{
  SANE_Int i = 0;
  int n = 0;
  if(!PyArg_ParseTuple(args, "i", &n))
    return NULL;
  
  RAISE_IF(self->h == NULL, "SaneDev object is closed");

  SANE_Status st = sane_control_option(self->h, n, SANE_ACTION_SET_AUTO, NULL, &i);
  if(st != SANE_STATUS_GOOD)
    return PySane_Error(st);
  return Py_BuildValue("i", i);
}

static PyObject *
SaneDev_snap(SaneDevObject *self, PyObject *args)
{
  SANE_Status st;
  
  int noCancel = 0;
  int allow16bitsamples = 0;
  PyObject *progress = NULL;
  if(!PyArg_ParseTuple(args, "|iiO", &noCancel, &allow16bitsamples, &progress))
    return NULL;
  
  if(progress && progress != Py_None && !PyCallable_Check(progress))
    {
      PyErr_SetString(PyExc_ValueError, "progress is not callable");
      return NULL;
    }
  RAISE_IF(self->h == NULL, "SaneDev object is closed");
  
  /* Get parameters, prepare buffers */
  SANE_Parameters p = {};
  st = sane_get_parameters(self->h, &p);
  if(st != SANE_STATUS_GOOD)
    return PySane_Error(st);
  
  RAISE_IF(p.depth != 1 && p.depth != 8 && p.depth != 16, "Bad pixel depth");
  
  int imgSamplesPerPixel = (p.format == SANE_FRAME_GRAY ? 1 : 3);
  int imgPixelsPerLine = p.pixels_per_line;
  int imgSampleSize = (p.depth == 16 && allow16bitsamples ? 2 : 1);
  int imgBytesPerLine = imgPixelsPerLine * imgSamplesPerPixel * imgSampleSize;
  int imgBytesPerScanLine = imgBytesPerLine;
  if(p.depth == 1)
    {
      /* See Sane spec chapter 4.3.8 */
      imgBytesPerScanLine = imgSamplesPerPixel * ((imgPixelsPerLine + 7) / 8);
    }
  int imgBufCurLine = 0;
  int imgBufLines = p.lines < 1 ? 1 : p.lines;
  int imgPrioriLines = p.lines;
  const unsigned char bitMasks[8] = {128, 64, 32, 16, 8, 4, 2, 1};
  SANE_Byte* imgBuf = (SANE_Byte*)malloc(imgBufLines * imgBytesPerLine);
  
  SANE_Int lineBufUsed = 0;
  SANE_Byte* lineBuf = (SANE_Byte*)malloc(imgBytesPerScanLine);
  int i, j;
  
  /* Read data */
  st = SANE_STATUS_GOOD;
  while(st == SANE_STATUS_GOOD)
    {
      Py_BEGIN_ALLOW_THREADS
      /* Read one line */
      lineBufUsed = 0;
      while(lineBufUsed < imgBytesPerScanLine)
        {
          SANE_Int nRead = 0;
          st = sane_read(self->h, lineBuf + lineBufUsed,
                         imgBytesPerScanLine - lineBufUsed,
                         &nRead);
          if(st != SANE_STATUS_GOOD)
            break;
          lineBufUsed += nRead;
        }
      Py_END_ALLOW_THREADS

      /* Check status, in particular if need to restart for the next frame */
      if(st != SANE_STATUS_GOOD)
        {
          if(st == SANE_STATUS_EOF && p.last_frame != SANE_TRUE)
            {
              /* If this was not the last frame, setup to read the next one */
              st = sane_start(self->h);
              if(st != SANE_STATUS_GOOD)
                break;
              st = sane_get_parameters(self->h, &p);
              if(st != SANE_STATUS_GOOD)
                break;
              /* Continue reading */
              continue;
            }
          break;
        }
      
      /* Resize image buffer if necessary */
      if(imgBufCurLine >= imgBufLines)
        {
          imgBufLines *= 2;
          imgBuf = (SANE_Byte*)realloc(imgBuf, imgBufLines * imgBytesPerLine);
        }
         
      int imgBufOffset = imgBufCurLine * imgBytesPerLine;
      /* Copy data to image buffer */
      if(p.format == SANE_FRAME_GRAY || p.format == SANE_FRAME_RGB)
        {
          if(p.depth == 1)
            {
              /* See Sane spec chapter 3.2.1 */
              for(j = 0; j < imgSamplesPerPixel; ++j)
                {
                  for(i = 0; i < imgPixelsPerLine; ++i)
                    {
                      int iImgBuf = imgBufOffset + imgSamplesPerPixel * i + j;
                      int lineByte = imgSamplesPerPixel * (i / 8) + j;
                      imgBuf[iImgBuf] = (lineBuf[lineByte] & bitMasks[i % 8]) ? 0 : 255;
                    }
                }
            }
          else if(p.depth == 8)
            {
              memcpy(imgBuf + imgBufOffset, lineBuf, imgBytesPerLine);
            }
          else if(p.depth == 16)
            {
              if(imgSampleSize == 2)
                memcpy(imgBuf + imgBufOffset, lineBuf, imgBytesPerLine);
              else
                for(i = 0; i < imgBytesPerLine; ++i)
                  {
                    int16_t value = *((int16_t*)(&lineBuf[2 * i]));
                    /* x >> 8 == x / 256 => rescale from uint16 to uint8 */
                    imgBuf[imgBufOffset + i] = value >> 8;
                  }
            }
        }
      else if(p.format == SANE_FRAME_RED ||
              p.format == SANE_FRAME_GREEN ||
              p.format == SANE_FRAME_BLUE)
        {
          int channel = p.format - SANE_FRAME_RED;
          if(p.depth == 1)
            {
              /* See Sane spec chapter 3.2.1 */
              for(i = 0; i < imgPixelsPerLine; ++i)
                {
                  int iImgBuf = imgBufOffset + 3 * i + channel;
                  imgBuf[iImgBuf] = (lineBuf[i / 8] & bitMasks[i % 8]) ? 0 : 255;
                }
            }
          else if(p.depth == 8)
            {
              for(i = 0; i < p.pixels_per_line; ++i)
                imgBuf[imgBufOffset + 3 * i + channel] = lineBuf[i];
            }
          else if(p.depth == 16)
            {
              
              for(i = 0; i < p.pixels_per_line; ++i)
                {
                  int16_t value = *(int16_t*)(lineBuf + 2 * i);
                  if(imgSampleSize == 2)
                    {
                      *(int16_t*)(imgBuf+imgBufOffset + 2 * (3 * i + channel)) = value;
                    }
                  else
                    {
                      /* x >> 8 == x / 256 => rescale from uint16 to uint8 */
                      imgBuf[imgBufOffset + 3 * i + channel] = value >> 8;
                    }
                }
            }
        }
      else
        {
          free(lineBuf);
          free(imgBuf);
          PyErr_SetString(ErrorObject, "Invalid frame format");
          return NULL;
        }
      ++imgBufCurLine;
  
      if(progress && progress != Py_None)
        {
          PyObject *progArgs = Py_BuildValue("ii", imgBufCurLine, imgPrioriLines);
          PyObject *result = PyObject_Call(progress, progArgs, NULL);
          Py_DECREF(result);
          Py_DECREF(progArgs);
          if(PyErr_Occurred())
            {
              free(lineBuf);
              free(imgBuf);
              sane_cancel(self->h);
              return NULL;
            }
        }
    }
  /* noCancel is true for ADF scans, see _SaneIterator class in sane.py */
  if(noCancel != 1)
    {
      sane_cancel(self->h);
    }
  free(lineBuf);
  
  if(st != SANE_STATUS_EOF)
    {
      free(imgBuf);
      return PySane_Error(st);
    }

  /* Create byte array with image data (PyByteArray_FromStringAndSize makes a copy) */
  imgBufLines = imgBufCurLine;
  imgBuf = (SANE_Byte*)realloc(imgBuf, imgBufLines * imgBytesPerLine);
  PyObject* pyByteArray = PyByteArray_FromStringAndSize((const char*)imgBuf,
                                                imgBufLines * imgBytesPerLine);
  free(imgBuf);
  if(!pyByteArray)
    return NULL;
    
  PyObject* ret = Py_BuildValue("Oiiii", pyByteArray, imgPixelsPerLine,
                                         imgBufLines, imgSamplesPerPixel,
                                         imgSampleSize);
  Py_DECREF(pyByteArray);
  
  return ret;
}

static PyMethodDef SaneDev_methods[] = {
  {"get_parameters",    (PyCFunction)SaneDev_get_parameters,    1},

  {"get_options",       (PyCFunction)SaneDev_get_options,       1},
  {"get_option",        (PyCFunction)SaneDev_get_option,        1},
  {"set_option",        (PyCFunction)SaneDev_set_option,        1},
  {"set_auto_option",   (PyCFunction)SaneDev_set_auto_option,   1},

  {"start",     (PyCFunction)SaneDev_start,     1},
  {"cancel",    (PyCFunction)SaneDev_cancel,    1},
  {"snap",      (PyCFunction)SaneDev_snap,      1},
  {"fileno",    (PyCFunction)SaneDev_fileno,    1},
  {"close",     (PyCFunction)SaneDev_close,     1},
  {NULL,        NULL} /* sentinel */
};

static PyTypeObject SaneDev_Type = {
  PyVarObject_HEAD_INIT(NULL, 0)
  "SaneDev",                    /*tp_name*/
  sizeof(SaneDevObject),        /*tp_basicsize*/
  0,                            /*tp_itemsize*/
  /* methods */
  (destructor)SaneDev_dealloc,  /*tp_dealloc*/
  0,                            /*tp_print*/
  0,                            /*tp_getattr*/
  0,                            /*tp_setattr*/
  0,                            /*tp_compare*/
  0,                            /*tp_repr*/
  0,                            /*tp_as_number */
  0,                            /*tp_as_sequence */
  0,                            /*tp_as_mapping */
  0,                            /*tp_hash*/
  0,                            /*tp_call*/
  0,                            /*tp_str*/
  0,                            /*tp_getattro*/
  0,                            /*tp_setattro*/
  0,                            /*tp_as_buffer*/
  Py_TPFLAGS_DEFAULT,           /*tp_flags*/
  0,                            /*tp_doc*/
  0,                            /*tp_traverse*/
  0,                            /*tp_clear*/
  0,                            /*tp_richcompare*/
  0,                            /*tp_weaklistoffset*/
  0,                            /*tp_iter*/
  0,                            /*tp_iternext*/
  SaneDev_methods,              /*tp_methods*/
  0,                            /*tp_members*/
  0,                            /*tp_getset*/
};

/* ------------------------------------------------------------------------- */

static PyObject *
PySane_init(PyObject *self, PyObject *args)
{
  if(!PyArg_ParseTuple(args, ""))
    return NULL;

  /* XXX Authorization is not yet supported */
  SANE_Int version;
  SANE_Status st = sane_init(&version, NULL);
  if(st != SANE_STATUS_GOOD)
    return PySane_Error(st);
  g_sane_initialized = 1;
  return Py_BuildValue("iiii", version,
                       SANE_VERSION_MAJOR(version),
                       SANE_VERSION_MINOR(version),
                       SANE_VERSION_BUILD(version));
}

static PyObject *
PySane_exit(PyObject *self, PyObject *args)
{
  if(!PyArg_ParseTuple(args, ""))
    return NULL;

  sane_exit();
  g_sane_initialized = 0;
  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject *
PySane_get_devices(PyObject *self, PyObject *args)
{
  int local_only = 0;

  if(!PyArg_ParseTuple(args, "|i", &local_only))
    return NULL;

  const SANE_Device **devices;
  SANE_Status st;
  Py_BEGIN_ALLOW_THREADS
  st = sane_get_devices(&devices, local_only);
  Py_END_ALLOW_THREADS
  
  if(st != SANE_STATUS_GOOD)
    return PySane_Error(st);

  PyObject *list = NULL;
  if(!(list = PyList_New(0)))
    return NULL;

  int i;
  for(i = 0; devices[i] != NULL; ++i)
    {
      const SANE_Device *dev = devices[i];
      PyObject *tuple = Py_BuildValue("ssss", dev->name, dev->vendor,
                                      dev->model, dev->type);
      PyList_Append(list, tuple);
      Py_XDECREF(tuple);
    }

  return list;
}

static PyObject *
PySane_open(PyObject *self, PyObject *args)
{
  char *name;
  if(!PyArg_ParseTuple(args, "s", &name))
    return NULL;
  
  if(PyType_Ready(&SaneDev_Type) < 0)
    return NULL;

  SaneDevObject *dev = PyObject_NEW(SaneDevObject, &SaneDev_Type);
  RAISE_IF(dev == NULL, "Failed to create SaneDev object");
  dev->h = NULL;

  SANE_Status st;
  Py_BEGIN_ALLOW_THREADS
  st = sane_open(name, &(dev->h));
  Py_END_ALLOW_THREADS

  if(st != SANE_STATUS_GOOD)
    {
      Py_DECREF(dev);
      return PySane_Error(st);
    }
  return(PyObject *)dev;
}

static PyObject *
PySane_OPTION_IS_ACTIVE(PyObject *self, PyObject *args)
{
  long lg;
  if(!PyArg_ParseTuple(args, "l", &lg))
    return NULL;

  SANE_Int cap = lg;
  return PyLong_FromLong( SANE_OPTION_IS_ACTIVE(cap));
}

static PyObject *
PySane_OPTION_IS_SETTABLE(PyObject *self, PyObject *args)
{
  long lg;
  if(!PyArg_ParseTuple(args, "l", &lg))
    return NULL;

  SANE_Int cap = lg;
  return PyLong_FromLong( SANE_OPTION_IS_SETTABLE(cap));
}

/* List of functions defined in the module */

static PyMethodDef PySane_methods[] = {
  {"init",              PySane_init,                    1},
  {"exit",              PySane_exit,                    1},
  {"get_devices",       PySane_get_devices,             1},
  {"_open",             PySane_open,                    1},
  {"OPTION_IS_ACTIVE",  PySane_OPTION_IS_ACTIVE,        1},
  {"OPTION_IS_SETTABLE",PySane_OPTION_IS_SETTABLE,      1},
  {NULL,                NULL} /* sentinel */
};

static void
insint(PyObject *d, char *name, int value)
{
  PyObject *v = PyLong_FromLong((long) value);
  if(!v || PyDict_SetItemString(d, name, v) == -1)
    PyErr_SetString(ErrorObject, "Can't initialize sane module");

  Py_XDECREF(v);
}

static struct PyModuleDef PySane_moduledef = {
  PyModuleDef_HEAD_INIT,
  "_sane",
  NULL,
  0,
  PySane_methods,
  NULL,
  NULL,
  NULL,
  NULL
};

PyMODINIT_FUNC
PyInit__sane(void)
{
  /* Create the module and add the functions */
  PyObject *m = PyModule_Create(&PySane_moduledef);
  if(!m)
    return NULL;

  /* Add some symbolic constants to the module */
  PyObject *d = PyModule_GetDict(m);
  ErrorObject = PyErr_NewException("_sane.error", NULL, NULL);
  PyDict_SetItemString(d, "error", ErrorObject);

  insint(d, "INFO_INEXACT", SANE_INFO_INEXACT);
  insint(d, "INFO_RELOAD_OPTIONS", SANE_INFO_RELOAD_OPTIONS);
  insint(d, "RELOAD_PARAMS", SANE_INFO_RELOAD_PARAMS);

  insint(d, "FRAME_GRAY", SANE_FRAME_GRAY);
  insint(d, "FRAME_RGB", SANE_FRAME_RGB);
  insint(d, "FRAME_RED", SANE_FRAME_RED);
  insint(d, "FRAME_GREEN", SANE_FRAME_GREEN);
  insint(d, "FRAME_BLUE", SANE_FRAME_BLUE);

  insint(d, "CONSTRAINT_NONE", SANE_CONSTRAINT_NONE);
  insint(d, "CONSTRAINT_RANGE", SANE_CONSTRAINT_RANGE);
  insint(d, "CONSTRAINT_WORD_LIST", SANE_CONSTRAINT_WORD_LIST);
  insint(d, "CONSTRAINT_STRING_LIST", SANE_CONSTRAINT_STRING_LIST);

  insint(d, "TYPE_BOOL", SANE_TYPE_BOOL);
  insint(d, "TYPE_INT", SANE_TYPE_INT);
  insint(d, "TYPE_FIXED", SANE_TYPE_FIXED);
  insint(d, "TYPE_STRING", SANE_TYPE_STRING);
  insint(d, "TYPE_BUTTON", SANE_TYPE_BUTTON);
  insint(d, "TYPE_GROUP", SANE_TYPE_GROUP);

  insint(d, "UNIT_NONE", SANE_UNIT_NONE);
  insint(d, "UNIT_PIXEL", SANE_UNIT_PIXEL);
  insint(d, "UNIT_BIT", SANE_UNIT_BIT);
  insint(d, "UNIT_MM", SANE_UNIT_MM);
  insint(d, "UNIT_DPI", SANE_UNIT_DPI);
  insint(d, "UNIT_PERCENT", SANE_UNIT_PERCENT);
  insint(d, "UNIT_MICROSECOND", SANE_UNIT_MICROSECOND);

  insint(d, "CAP_SOFT_SELECT", SANE_CAP_SOFT_SELECT);
  insint(d, "CAP_HARD_SELECT", SANE_CAP_HARD_SELECT);
  insint(d, "CAP_SOFT_DETECT", SANE_CAP_SOFT_DETECT);
  insint(d, "CAP_EMULATED", SANE_CAP_EMULATED);
  insint(d, "CAP_AUTOMATIC", SANE_CAP_AUTOMATIC);
  insint(d, "CAP_INACTIVE", SANE_CAP_INACTIVE);
  insint(d, "CAP_ADVANCED", SANE_CAP_ADVANCED);

  /* handy for checking array lengths: */
  insint(d, "SANE_WORD_SIZE", sizeof(SANE_Word));

  /* possible return values of set_option() */
  insint(d, "INFO_INEXACT", SANE_INFO_INEXACT);
  insint(d, "INFO_RELOAD_OPTIONS", SANE_INFO_RELOAD_OPTIONS);
  insint(d, "INFO_RELOAD_PARAMS", SANE_INFO_RELOAD_PARAMS);

  /* Check for errors */
  if(PyErr_Occurred())
    {
      Py_DECREF(m);
      m = NULL;
    }

  return m;
}
