/***********************************************************
(C) Copyright 2003 A.M. Kuchling.  All Rights Reserved
(C) Copyright 2004 A.M. Kuchling, Ralph Heinkel  All Rights Reserved

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose and without fee is hereby granted,
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in
supporting documentation, and that the name of A.M. Kuchling and
Ralph Heinkel not be used in advertising or publicity pertaining to
distribution of the software without specific, written prior permission.

A.M. KUCHLING, R.H. HEINKEL DISCLAIM ALL WARRANTIES WITH REGARD TO THIS
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

/* SaneDev objects */

#include "Python.h"
#include <sane/sane.h>

#include <sys/time.h>

#if PY_MAJOR_VERSION >= 3
  #define PyInt_AsLong PyLong_AsLong
  #define PyInt_FromLong PyLong_FromLong
  #define PyInt_Check PyLong_Check
#endif

static PyObject *ErrorObject;

typedef struct {
	PyObject_HEAD
	SANE_Handle h;
} SaneDevObject;

#define	UINT8 unsigned char
#define INT16 short

typedef struct {
    int xsize;		/* Image dimension. */
    int ysize;
    UINT8 **image8;	/* Set for 8-bit images (pixelsize=1). */
    UINT8 **image32;	/* Set for 32-bit images (pixelsize=4). */
} Imaging;

#ifdef WITH_THREAD
PyThreadState *_save;
#endif

/* Raise a SANE exception */
PyObject *
PySane_Error(SANE_Status st)
{
  const char *string;

  if (st==SANE_STATUS_GOOD) {Py_INCREF(Py_None); return (Py_None);}
  string=sane_strstatus(st);
  PyErr_SetString(ErrorObject, string);
  return NULL;
}

static PyTypeObject SaneDev_Type;

#define SaneDevObject_Check(v)	(Py_TYPE(v) == &SaneDev_Type)

static SaneDevObject *
newSaneDevObject(void)
{
	SaneDevObject *self;

    if (PyType_Ready(&SaneDev_Type) < 0)
        return NULL;

	self = PyObject_NEW(SaneDevObject, &SaneDev_Type);
	if (self == NULL)
		return NULL;
	self->h=NULL;
	return self;
}

/* SaneDev methods */

static void
SaneDev_dealloc(SaneDevObject *self)
{
  if (self->h) sane_close(self->h);
  self->h=NULL;
  PyObject_DEL(self);
}

static PyObject *
SaneDev_close(SaneDevObject *self, PyObject *args)
{
  if (!PyArg_ParseTuple(args, ""))
    return NULL;
  if (self->h) sane_close(self->h);
  self->h=NULL;
  Py_INCREF(Py_None);
  return (Py_None);
}

static PyObject *
SaneDev_get_parameters(SaneDevObject *self, PyObject *args)
{
  SANE_Status st;
  SANE_Parameters p;
  char *format="unknown format";

  if (!PyArg_ParseTuple(args, ""))
    return NULL;
  if (self->h==NULL)
    {
      PyErr_SetString(ErrorObject, "SaneDev object is closed");
      return NULL;
    }
  Py_BEGIN_ALLOW_THREADS
  st=sane_get_parameters(self->h, &p);
  Py_END_ALLOW_THREADS

  if (st) return PySane_Error(st);
  switch (p.format)
    {
    case(SANE_FRAME_GRAY):  format="gray"; break;
    case(SANE_FRAME_RGB):   format="color"; break;
    case(SANE_FRAME_RED):   format="red"; break;
    case(SANE_FRAME_GREEN): format="green"; break;
    case(SANE_FRAME_BLUE):  format="blue"; break;
    }

  return Py_BuildValue("si(ii)ii", format, p.last_frame, p.pixels_per_line,
		       p.lines, p.depth, p.bytes_per_line);
}


static PyObject *
SaneDev_fileno(SaneDevObject *self, PyObject *args)
{
  SANE_Status st;
  SANE_Int fd;

  if (!PyArg_ParseTuple(args, ""))
    return NULL;
  if (self->h==NULL)
    {
      PyErr_SetString(ErrorObject, "SaneDev object is closed");
      return NULL;
    }
  st=sane_get_select_fd(self->h, &fd);
  if (st) return PySane_Error(st);
  return PyInt_FromLong(fd);
}

static PyObject *
SaneDev_start(SaneDevObject *self, PyObject *args)
{
  SANE_Status st;

  if (!PyArg_ParseTuple(args, ""))
    return NULL;
  if (self->h==NULL)
    {
      PyErr_SetString(ErrorObject, "SaneDev object is closed");
      return NULL;
    }
  /* sane_start can take several seconds, if the user initiates
     a new scan, while the scan head of a flatbed scanner moves
     back to the start position after finishing a previous scan.
     Hence it is worth to allow threads here.
  */
  Py_BEGIN_ALLOW_THREADS
  st=sane_start(self->h);
  Py_END_ALLOW_THREADS
  if (st) return PySane_Error(st);
  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject *
SaneDev_cancel(SaneDevObject *self, PyObject *args)
{
  if (!PyArg_ParseTuple(args, ""))
    return NULL;
  if (self->h==NULL)
    {
      PyErr_SetString(ErrorObject, "SaneDev object is closed");
      return NULL;
    }
  sane_cancel(self->h);
  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject *
SaneDev_get_options(SaneDevObject *self, PyObject *args)
{
  const SANE_Option_Descriptor *d;
  PyObject *list, *value;
  int i=1;

  if (!PyArg_ParseTuple(args, ""))
    return NULL;
  if (self->h==NULL)
    {
      PyErr_SetString(ErrorObject, "SaneDev object is closed");
      return NULL;
    }
  if (!(list = PyList_New(0)))
	    return NULL;

  do
    {
      d=sane_get_option_descriptor(self->h, i);
      if (d!=NULL)
	{
	  PyObject *constraint=NULL;
	  int j;

	  switch (d->constraint_type)
	    {
	    case(SANE_CONSTRAINT_NONE):
	      Py_INCREF(Py_None); constraint=Py_None; break;
	    case(SANE_CONSTRAINT_RANGE):
	      if (d->type == SANE_TYPE_INT)
		constraint=Py_BuildValue("iii", d->constraint.range->min,
					 d->constraint.range->max,
					 d->constraint.range->quant);
	      else
		constraint=Py_BuildValue("ddd",
					 SANE_UNFIX(d->constraint.range->min),
					 SANE_UNFIX(d->constraint.range->max),
					 SANE_UNFIX(d->constraint.range->quant));
	      break;
	    case(SANE_CONSTRAINT_WORD_LIST):
	      constraint=PyList_New(d->constraint.word_list[0]);
	      if (d->type == SANE_TYPE_INT)
		for (j=1; j<=d->constraint.word_list[0]; j++)
		  PyList_SetItem(constraint, j-1,
				 PyInt_FromLong(d->constraint.word_list[j]));
	      else
		for (j=1; j<=d->constraint.word_list[0]; j++)
		  PyList_SetItem(constraint, j-1,
				 PyFloat_FromDouble(SANE_UNFIX(d->constraint.word_list[j])));
	      break;
	    case(SANE_CONSTRAINT_STRING_LIST):
	      constraint=PyList_New(0);
	      for(j=0; d->constraint.string_list[j]!=NULL; j++)
		PyList_Append(constraint,
#if PY_MAJOR_VERSION >= 3
			      PyUnicode_DecodeLatin1(d->constraint.string_list[j], strlen(d->constraint.string_list[j]), NULL));
#else
			      PyString_FromString(d->constraint.string_list[j]));
#endif
	      break;
	    }
	  value=Py_BuildValue("isssiiiiO", i, d->name, d->title, d->desc,
			      d->type, d->unit, d->size, d->cap, constraint);
	  PyList_Append(list, value);
	}
      i++;
    } while (d!=NULL);
  return list;
}

static PyObject *
SaneDev_get_option(SaneDevObject *self, PyObject *args)
{
  SANE_Status st;
  const SANE_Option_Descriptor *d;
  PyObject *value=NULL;
  int n;
  void *v;

  if (!PyArg_ParseTuple(args, "i", &n))
    {
      return NULL;
    }
  if (self->h==NULL)
    {
      PyErr_SetString(ErrorObject, "SaneDev object is closed");
      return NULL;
    }
  d=sane_get_option_descriptor(self->h, n);
  v=malloc(d->size+1);
  st=sane_control_option(self->h, n, SANE_ACTION_GET_VALUE,
			 v, NULL);

  if (st)
    {
      free(v);
      return PySane_Error(st);
    }

  switch(d->type)
    {
    case(SANE_TYPE_BOOL):
    case(SANE_TYPE_INT):
      value=Py_BuildValue("i", *( (SANE_Int*)v) );
      break;
    case(SANE_TYPE_FIXED):
      value=Py_BuildValue("d", SANE_UNFIX((*((SANE_Fixed*)v))) );
      break;
    case(SANE_TYPE_STRING):
#if PY_MAJOR_VERSION >= 3
      value=PyUnicode_DecodeLatin1((const char *) v, strlen((const char *) v), NULL);
#else
      value=Py_BuildValue("s", v);
#endif
      break;
    case(SANE_TYPE_BUTTON):
    case(SANE_TYPE_GROUP):
      value=Py_BuildValue("O", Py_None);
      break;
    }

  free(v);
  return value;
}

static PyObject *
SaneDev_set_option(SaneDevObject *self, PyObject *args)
{
  SANE_Status st;
  const SANE_Option_Descriptor *d;
  SANE_Int i;
  PyObject *value;
  int n;
  void *v;

  if (!PyArg_ParseTuple(args, "iO", &n, &value))
    return NULL;
  if (self->h==NULL)
    {
      PyErr_SetString(ErrorObject, "SaneDev object is closed");
      return NULL;
    }
  d=sane_get_option_descriptor(self->h, n);
  v=malloc(d->size+1);

  switch(d->type)
    {
    case(SANE_TYPE_BOOL):
      if (!PyInt_Check(value))
	{
	  PyErr_SetString(PyExc_TypeError, "SANE_BOOL requires an integer");
	  free(v);
	  return NULL;
	}
	/* fall through */
    case(SANE_TYPE_INT):
      if (!PyInt_Check(value))
	{
	  PyErr_SetString(PyExc_TypeError, "SANE_INT requires an integer");
	  free(v);
	  return NULL;
	}
      *( (SANE_Int*)v) = PyInt_AsLong(value);
      break;
    case(SANE_TYPE_FIXED):
      if (!PyFloat_Check(value))
	{
	  PyErr_SetString(PyExc_TypeError, "SANE_FIXED requires a floating point number");
	  free(v);
	  return NULL;
	}
      *( (SANE_Fixed*)v) = SANE_FIX(PyFloat_AsDouble(value));
      break;
    case(SANE_TYPE_STRING):
#if PY_MAJOR_VERSION >= 3
      if (!PyUnicode_Check(value))
	{
	  PyErr_SetString(PyExc_TypeError, "SANE_STRING requires a string");
	  free(v);
	  return NULL;
	}
      {
	PyObject *encoded = PyUnicode_AsLatin1String(value);

	if (!encoded)
	  return NULL;

	strncpy(v, PyBytes_AsString(encoded), d->size-1);
	((char*)v)[d->size-1] = 0;
	Py_DECREF(encoded);
      }
#else
      if (!PyString_Check(value))
	{
	  PyErr_SetString(PyExc_TypeError, "SANE_STRING requires a string");
	  free(v);
	  return NULL;
	}
      strncpy(v, PyString_AsString(value), d->size-1);
      ((char*)v)[d->size-1] = 0;
#endif
      break;
    case(SANE_TYPE_BUTTON):
    case(SANE_TYPE_GROUP):
      break;
    }

  st=sane_control_option(self->h, n, SANE_ACTION_SET_VALUE,
			 v, &i);
  if (st) {free(v); return PySane_Error(st);}

  free(v);
  return Py_BuildValue("i", i);
}

static PyObject *
SaneDev_set_auto_option(SaneDevObject *self, PyObject *args)
{
  SANE_Status st;
  SANE_Int i;
  int n;

  if (!PyArg_ParseTuple(args, "i", &n))
    return NULL;
  if (self->h==NULL)
    {
      PyErr_SetString(ErrorObject, "SaneDev object is closed");
      return NULL;
    }
  st=sane_control_option(self->h, n, SANE_ACTION_SET_AUTO,
			 NULL, &i);
  if (st) {return PySane_Error(st);}

  return Py_BuildValue("i", i);
 }

#define READSIZE 32768

static PyObject *
SaneDev_snap(SaneDevObject *self, PyObject *args)
{
  SANE_Status st;
   /* The buffer should be a multiple of 3 in size, so each sane_read
      operation will return an integral number of RGB triples. */
  SANE_Byte buffer[READSIZE];  /* XXX how big should the buffer be? */
  SANE_Int len, lastlen;
  Imaging im;
  SANE_Parameters p;
  int px, py, remain, cplen, bufpos, padbytes;
  char errmsg[80];
  union
    { char c[2];
      INT16 i16;
    }
  endian;
  PyObject *pyNoCancel = NULL;
  PyObject *ptrDict = NULL;
  PyObject *dictItem = NULL;
  int noCancel = 0;

  endian.i16 = 1;

  if (!PyArg_ParseTuple(args, "O|O", &ptrDict, &pyNoCancel))
      return NULL;

  if (self->h==NULL)
    {
      PyErr_SetString(ErrorObject, "SaneDev object is closed");
      return NULL;
    }

  if (pyNoCancel)
    noCancel = PyObject_IsTrue(pyNoCancel);

  if (!PyDict_Check(ptrDict)){
      PyErr_SetString(ErrorObject, "Dictionary required");
      return NULL;
  }

  dictItem = PyDict_GetItemString(ptrDict, "ysize");
  if (dictItem) {
      im.ysize = (int)PyInt_AsLong(dictItem);
  } else { 
      PyErr_SetString(ErrorObject, "ysize missing");
      return NULL; 
  }
  dictItem = PyDict_GetItemString(ptrDict, "xsize");
  if (dictItem) {
      im.xsize = (int)PyInt_AsLong(dictItem);
  } else { 
      PyErr_SetString(ErrorObject, "xsize missing");
      return NULL; 
  }
  dictItem = PyDict_GetItemString(ptrDict, "image8");
  if (dictItem) {
      im.image8 = (UINT8 **)PyLong_AsSsize_t(dictItem);
  } else { 
      PyErr_SetString(ErrorObject, "image8 missing");
      return NULL; 
  }
  dictItem = PyDict_GetItemString(ptrDict, "image32");
  if (dictItem) {
      im.image32 = (UINT8 **)PyLong_AsSsize_t(dictItem);
  } else { 
      PyErr_SetString(ErrorObject, "image32 missing");
      return NULL; 
  }
  

  st=SANE_STATUS_GOOD; px=py=0;
  /* xxx not yet implemented
     - handscanner support (i.e., unknown image length during start)
     - generally: move the functionality from method snap in sane.py
       down here -- I don't like this cross-dependency.
       we need to call sane_get_parameters here, and we can create
       the result Image object here.
  */

  Py_UNBLOCK_THREADS
  sane_get_parameters(self->h, &p);
  if (p.format == SANE_FRAME_GRAY)
    {
      switch (p.depth)
        {
          case 1:
            remain = p.bytes_per_line * im.ysize;
            padbytes = p.bytes_per_line - (im.xsize+7)/8;
            bufpos = 0;
            lastlen = len = 0;
            while (st!=SANE_STATUS_EOF && py < im.ysize)
              {
                while (len > 0 && py < im.ysize)
                  {
                    int i, j, k;
                    j = buffer[bufpos++];
                    k = 0x80;
                    for (i = 0; i < 8 && px < im.xsize; i++)
                      {
                        im.image8[py][px++] = (k&j) ? 0 : 0xFF;
                        k = k >> 1;
                      }
                    len--;
                    if (px >= im.xsize)
                      {
                        bufpos += padbytes;
                        len -= padbytes;
                        py++;
                        px = 0;
                      }
                  }
                st=sane_read(self->h, buffer,
                             remain<READSIZE ? remain : READSIZE, &len);
                if (st && (st!=SANE_STATUS_EOF))
                  {
                    sane_cancel(self->h);
                    Py_BLOCK_THREADS
                    return PySane_Error(st);
                  }
                bufpos -= lastlen;
                lastlen = len;
                remain -= len;
                /* skip possible pad bytes at the start of the buffer */
                len -= bufpos;
              }
            break;
          case 8:
            remain = p.bytes_per_line * im.ysize;
            padbytes = p.bytes_per_line - im.xsize;
            bufpos = 0;
            len = 0;
            while (st!=SANE_STATUS_EOF && py < im.ysize)
              {
                while (len > 0 && py < im.ysize)
                  {
                    cplen = len;
                    if (px + cplen >= im.xsize)
                        cplen = im.xsize - px;
                    memcpy(&im.image8[py][px], &buffer[bufpos], cplen);
                    len -= cplen;
                    bufpos += cplen;
                    px += cplen;
                    if (px >= im.xsize)
                      {
                        px = 0;
                        py++;
                        bufpos += padbytes;
                        len -= padbytes;
                      }
                  }
                bufpos = -len;

                st=sane_read(self->h, buffer,
                             remain<READSIZE ? remain : READSIZE, &len);
                if (st && (st!=SANE_STATUS_EOF))
                  {
                    sane_cancel(self->h);
                    Py_BLOCK_THREADS
                    return PySane_Error(st);
                  }
                remain -= len;
                len -= bufpos;
              }
              break;
          case 16:
            remain = p.bytes_per_line * im.ysize;
            padbytes = p.bytes_per_line - 2 * im.xsize;
            bufpos = endian.c[0];
            lastlen = len = 0;
            while (st!=SANE_STATUS_EOF && py < im.ysize)
              {
                while (len > 0 && py < im.ysize)
                  {
                    im.image8[py][px++] = buffer[bufpos];
                    bufpos += 2;
                    len -= 2;
                    if (px >= im.xsize)
                      {
                        bufpos += padbytes;
                        len -= padbytes;
                        py++;
                        px = 0;
                      }
                  }
                st=sane_read(self->h, buffer,
                             remain<READSIZE ? remain : READSIZE, &len);
                if (st && (st!=SANE_STATUS_EOF))
                  {
                    sane_cancel(self->h);
                    Py_BLOCK_THREADS
                    return PySane_Error(st);
                  }
                remain -= len;
                bufpos -= lastlen;
                lastlen = len;
                len -= bufpos;
              }
            break;
          default:
            /* other depths are not formally "illegal" according to the
               Sane API, but it's agreed by Sane developers that other
               depths than 1, 8, 16 should not be used
            */
            sane_cancel(self->h);
            Py_BLOCK_THREADS
            snprintf(errmsg, 80, "unsupported pixel depth: %i", p.depth);
            PyErr_SetString(ErrorObject, errmsg);
            return NULL;
        }
    }
  else if (p.format == SANE_FRAME_RGB)
    {
      int incr, color, pxs, pxmax, bit, val, mask;
      switch (p.depth)
        {
          case 1:
            remain = p.bytes_per_line * im.ysize;
            padbytes = p.bytes_per_line - ((im.xsize+7)/8) * 3;
            bufpos = 0;
            len = 0;
            lastlen = 0;
            pxmax = 4 * im.xsize;
            while (st!=SANE_STATUS_EOF && py < im.ysize)
              {
                pxs = px;
                for (color = 0; color < 3; color++)
                  {
                    while (len <= 0 && st == SANE_STATUS_GOOD)
                      {
                        st=sane_read(self->h, buffer,
                                     remain<READSIZE ? remain : READSIZE, &len);
                        if (st && (st!=SANE_STATUS_EOF))
                          {
                            sane_cancel(self->h);
                            Py_BLOCK_THREADS
                            return PySane_Error(st);
                          }
                        bufpos -= lastlen;
                        remain -= len;
                        lastlen = len;
                        /* skip possible pad bytes at the start of the buffer */
                        len -= bufpos;
                      }
                    if (st == SANE_STATUS_EOF) break;
                    pxs = px;
                    val = buffer[bufpos++];
                    len--;
                    mask = 0x80;
                    for (bit = 0; (bit < 8) && (px < pxmax); bit++)
                      {
                         ((UINT8**)(im.image32))[py][px] = (val&mask) ? 0xFF : 0;
                        mask = mask >> 1;
                        px += 4;
                      }
                    pxs++;
                    px = pxs;
                  }
                if (st == SANE_STATUS_EOF)
                  break;
                for (bit = 0; bit < 8 && px < pxmax; bit++)
                  {
                     ((UINT8**)(im.image32))[py][px] = 0;
                     px += 4;
                  }
                px -= 3;
                if (px >= pxmax)
                  {
                    bufpos += padbytes;
                    len -= padbytes;
                    py++;
                    px = 0;
                  }
              }
            break;
          case 8:
          case 16:
            if (p.depth == 8)
              {
                padbytes = p.bytes_per_line - 3 * im.xsize;
                bufpos = 0;
                incr = 1;
              }
            else
              {
                padbytes = p.bytes_per_line - 6 * im.xsize;
                bufpos = endian.c[0];
                incr = 2;
              }
            remain = p.bytes_per_line * im.ysize;
            len = 0;
            lastlen = 0;
            pxmax = 4 * im.xsize;
            /* probably not very efficient. But we have to deal with these
               possible conditions:
               - we may have padding bytes at the end of a scan line
               - the number of bytes read with sane_read may be smaller
                 than the number of pad bytes
               - the buffer may become empty after setting any of the
                 red/green/blue pixel values

            */
            while (st != SANE_STATUS_EOF && py < im.ysize)
              {
                for (color = 0; color < 3; color++)
                  {
                    while (len <= 0 && st == SANE_STATUS_GOOD)
                      {
                        bufpos -= lastlen;
                        if (remain == 0)
                          {
                            sane_cancel(self->h);
                            Py_BLOCK_THREADS
                            PyErr_SetString(ErrorObject, "internal _sane error: premature end of scan");
                            return NULL;
                          }
                        st = sane_read(self->h, buffer,
                              remain<(READSIZE) ? remain : (READSIZE), &len);
                        if (st && (st!=SANE_STATUS_EOF))
                          {
                            sane_cancel(self->h);
                            Py_BLOCK_THREADS
                            return PySane_Error(st);
                          }
                        lastlen = len;
                        remain -= len;
                        len -= bufpos;
                      }
                    if (st == SANE_STATUS_EOF) break;
                    ((UINT8**)(im.image32))[py][px++] = buffer[bufpos];
                    bufpos += incr;
                    len -= incr;
                  }
                if (st == SANE_STATUS_EOF) break;

                ((UINT8**)(im.image32))[py][px++] = 0;

                if (px >= pxmax)
                  {
                    px = 0;
                    py++;
                    bufpos += padbytes;
                    len -= padbytes;
                  }
              }
            break;
          default:
            Py_BLOCK_THREADS
            sane_cancel(self->h);
            snprintf(errmsg, 80, "unsupported pixel depth: %i", p.depth);
            PyErr_SetString(ErrorObject, errmsg);
            return NULL;
        }

    }
  else /* should be SANE_FRAME_RED, GREEN or BLUE */
    {
      int lastlen, pxa, pxmax, offset, incr, frame_count = 0;
      /* at least the Sane test backend behaves a bit weird, if
         it returns "premature EOF" for sane_read, i.e., if the
         option "return value of sane_read" is set to SANE_STATUS_EOF.
         In this case, the test backend does not advance to the next frame,
         so p.last_frame will never be set...
         So let's count the number of frames we try to acquire
      */
      while (!p.last_frame && frame_count < 4)
        {
          frame_count++;
          st = sane_get_parameters(self->h, &p);
          if (st)
            {
              sane_cancel(self->h);
              Py_BLOCK_THREADS
              return PySane_Error(st);
            }
          remain = p.bytes_per_line * im.ysize;
          bufpos = 0;
          len = 0;
          lastlen = 0;
          py = 0;
          switch (p.format)
            {
              case SANE_FRAME_RED:
                offset = 0;
                break;
              case SANE_FRAME_GREEN:
                offset = 1;
                break;
              case SANE_FRAME_BLUE:
                offset = 2;
                break;
              default:
                sane_cancel(self->h);
                Py_BLOCK_THREADS
                snprintf(errmsg, 80, "unknown/invalid frame format: %i", p.format);
                PyErr_SetString(ErrorObject, errmsg);
                return NULL;
            }
          px = offset;
          pxa = 3;
          pxmax = im.xsize * 4;
          switch (p.depth)
            {
              case 1:
                padbytes = p.bytes_per_line - (im.xsize+7)/8;
                st = SANE_STATUS_GOOD;
                while (st != SANE_STATUS_EOF && py < im.ysize)
                  {
                    while (len > 0)
                      {
                        int bit, mask, val;
                        val = buffer[bufpos++]; len--;
                        mask = 0x80;
                        for (bit = 0; bit < 8 && px < pxmax; bit++)
                          {
                            ((UINT8**)(im.image32))[py][px]
                                = val&mask ? 0xFF : 0;
                            ((UINT8**)(im.image32))[py][pxa] = 0;
                            px += 4;
                            pxa += 4;
                            mask = mask >> 1;
                          }

                        if (px >= pxmax)
                          {
                            px = offset;
                            pxa = 3;
                            py++;
                            bufpos += padbytes;
                            len -= padbytes;
                          }
                      }
                    while (len <= 0 && st == SANE_STATUS_GOOD && remain > 0)
                      {
                        bufpos -= lastlen;
                        st = sane_read(self->h, buffer,
                              remain<(READSIZE) ? remain : (READSIZE), &len);
                        if (st && (st!=SANE_STATUS_EOF))
                          {
                            sane_cancel(self->h);
                            Py_BLOCK_THREADS
                            return PySane_Error(st);
                          }
                        remain -= len;
                        lastlen = len;
                        len -= bufpos;
                      }
                  }
                break;
              case 8:
              case 16:
                if (p.depth == 8)
                  {
                    padbytes = p.bytes_per_line - im.xsize;
                    incr = 1;
                  }
                else
                  {
                    padbytes = p.bytes_per_line - 2 * im.xsize;
                    incr = 2;
                    bufpos = endian.c[0];
                  }
                st = SANE_STATUS_GOOD;
                while (st != SANE_STATUS_EOF && py < im.ysize)
                  {
                    while (len <= 0)
                      {
                        bufpos -= lastlen;
                        if (remain == 0)
                          {
                            sane_cancel(self->h);
                            Py_BLOCK_THREADS
                            PyErr_SetString(ErrorObject, "internal _sane error: premature end of scan");
                            return NULL;
                          }
                        st = sane_read(self->h, buffer,
                              remain<(READSIZE) ? remain : (READSIZE), &len);
                        if (st && (st!=SANE_STATUS_EOF))
                          {
                            sane_cancel(self->h);
                            Py_BLOCK_THREADS
                            return PySane_Error(st);
                          }
                        if (st == SANE_STATUS_EOF)
                          break;
                        lastlen = len;
                        remain -= len;
                        if (bufpos >= len)
                          len = 0;
                        else
                          len -= bufpos;
                      }
                    if (st == SANE_STATUS_EOF)
                      break;
                    ((UINT8**)(im.image32))[py][px] = buffer[bufpos];
                    ((UINT8**)(im.image32))[py][pxa] = 0;
                    bufpos += incr;
                    len -= incr;
                    px += 4;
                    pxa += 4;

                    if (px >= pxmax)
                      {
                        px = offset;
                        pxa = 3;
                        py++;
                        bufpos += padbytes;
                        len -= padbytes;
                      }
                  }
                break;
              default:
                sane_cancel(self->h);
                Py_BLOCK_THREADS
                snprintf(errmsg, 80, "unsupported pixel depth: %i", p.depth);
                PyErr_SetString(ErrorObject, errmsg);
                return NULL;
            }
          if (!p.last_frame)
            {
              /* all sane_read calls in the above loop may return
                 SANE_STATUS_GOOD, but the backend may need another sane_read
                 call which returns SANE_STATUS_EOF in order to start
                 a new frame.
              */
              if (st != SANE_STATUS_EOF)
                {
                  do {
                    st = sane_read(self->h, buffer, READSIZE, &len);
                  }
                  while (st == SANE_STATUS_GOOD);
                }
              if (st != SANE_STATUS_EOF)
                {
                   Py_BLOCK_THREADS
                   sane_cancel(self->h);
                   return PySane_Error(st);
                }

              st = sane_start(self->h);
              if (st)
                {
                   Py_BLOCK_THREADS
                   return PySane_Error(st);
                }
            }
        }
    }
  /* enforce SANE_STATUS_EOF. Can be necessary for ADF scans for some backends */
  if (st != SANE_STATUS_EOF)
    {
      do {
        st = sane_read(self->h, buffer, READSIZE, &len);
      }
      while (st == SANE_STATUS_GOOD);
    }
  if (st != SANE_STATUS_EOF)
    {
      sane_cancel(self->h);
      Py_BLOCK_THREADS
      return PySane_Error(st);
    }

  if (!noCancel)
    sane_cancel(self->h);
  Py_BLOCK_THREADS
  Py_INCREF(Py_None);
  return Py_None;
}


#ifdef WITH_NUMPY
#define ARRAY_SUPPORT
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION  /* compilation warning in include */
#include "numpy/ndarraytypes.h"
#include "numpy/ndarrayobject.h" 
/* #define DUMMY void * */
#define ARRAY_TYPE    int
#define NAT_uint8     NPY_UBYTE 
#define NAT_uint16    NPY_USHORT 
#define ARRAY_SUPPORT_PACKAGE_ERROR "numpy package not available"
#define ARRAY_SUPPORT_MSG   "Numpy"
#elif defined  WITH_NUMARRAY
#define ARRAY_SUPPORT
#include "numarray/libnumarray.h"

#define ARRAY_TYPE    NumarrayType 
#define NAT_uint8     tUInt8
#define NAT_uint16    tUInt16
#define ARRAY_SUPPORT_PACKAGE_ERROR "numarray package not available"
#define ARRAY_SUPPORT_MSG   "NumArray"
#endif /* WITH_NUMARRAY or WITH_NUMPY both defining ARRAY_SUPPORT*/


#ifdef ARRAY_SUPPORT 
/* this global variable is set to 1 in 'init_sane()' after successfully
   importing the numarray module. */
int ARRAY_IMPORTED = 0;

static PyObject *
SaneDev_arr_snap(SaneDevObject *self, PyObject *args)
{
  SANE_Status st;
  SANE_Byte buffer[READSIZE];
  SANE_Int len;
  SANE_Parameters p;

  PyArrayObject *pyArr = NULL;
  ARRAY_TYPE arrType;
  int line, line_index, buffer_index, remain_bytes_line, num_pad_bytes;
  int cp_num_bytes, total_remain, bpp, arr_bytes_per_line,bfc;
  int pixels_per_line = -1;
  char errmsg[80];

  if (!ARRAY_IMPORTED)
    {
        PyErr_SetString(ErrorObject, ARRAY_SUPPORT_PACKAGE_ERROR);
        return NULL;
    }

  if (!PyArg_ParseTuple(args, "|i", &pixels_per_line))
    return NULL;
  if (self->h==NULL)
    {
      PyErr_SetString(ErrorObject, "SaneDev object is closed");
      return NULL;
    }

  sane_get_parameters(self->h, &p);
#ifdef WITH_NUMARRAY
  if (p.format != SANE_FRAME_GRAY)
    {
      sane_cancel(self->h);
      snprintf(errmsg, 80, "numarray only supports gray-scale images");
      PyErr_SetString(ErrorObject, errmsg);
      return NULL;
    }
#endif
  if (p.format == SANE_FRAME_GRAY) bfc = 1;   /* bytes for color */
  else if  (p.format == SANE_FRAME_RGB) bfc = 3;
  else
    {
      sane_cancel(self->h);
      snprintf(errmsg, 80, "Array only supports gray-scale or rgb-color images");
      PyErr_SetString(ErrorObject, errmsg);
      return NULL;
    }
          
  if (p.depth == 8)
    {
      bpp=1;   /* bytes-per-pixel */
      arrType = NAT_uint8;
    }
  else if (p.depth == 16)
    {
      bpp=2;   /* bytes-per-pixel */
      arrType = NAT_uint16;
    }
  else
    {
      sane_cancel(self->h);
      snprintf(errmsg, 80, "arrsnap: unsupported pixel depth: %i", p.depth);
      PyErr_SetString(ErrorObject, errmsg);
      return NULL;
    }
  if (pixels_per_line < 1)
    /* The user can choose a smaller result array than the actual scan */
    pixels_per_line = p.pixels_per_line;
  else
    if (pixels_per_line > p.pixels_per_line)
      {
	PyErr_SetString(ErrorObject,"given pixels_per_line too big");
	return NULL;
      }
#ifdef WITH_NUMPY
  npy_intp numpy_shape[] = { p.lines, pixels_per_line, bfc};
  /* printf ("numpy shape %d x %d x %d  shape_dim %d\n", numpy_shape[0], numpy_shape[1], numpy_shape[2], bfc==1?2:3); */
  if (!( pyArr = (PyArrayObject *)PyArray_SimpleNew(bfc==1?2:3, numpy_shape, arrType) )) 
#elif defined WITH_NUMARRAY
      /* important: NumArray have indices like (y, x) !! */
  if (!(pyArr = NA_NewArray(NULL, arrType, 2, p.lines, pixels_per_line)))
#endif
    {
      PyErr_SetString(ErrorObject, "failed to create ARRAY_SUPPORT object");
      return NULL;
    }

#ifdef WRITE_PGM
  FILE *fp;
  fp = fopen("sane_p5.pgm", "w");
  fprintf(fp, "P5\n%d %d\n%d\n", p.pixels_per_line,
	  p.lines, (int) pow(2.0, (double) p.depth)-1);
#endif
  line_index = line = 0;
  arr_bytes_per_line = pixels_per_line * bpp * bfc;
  remain_bytes_line = arr_bytes_per_line;
  total_remain = p.bytes_per_line  * p.lines;
  num_pad_bytes = p.bytes_per_line - arr_bytes_per_line;
  st=SANE_STATUS_GOOD;
  while (st!=SANE_STATUS_EOF)
    {
#ifdef DEBUG
      printf(" Total remain %d READSIZE %d  ", total_remain, READSIZE);
#endif
      Py_BEGIN_ALLOW_THREADS
      st = sane_read(self->h, buffer,
             READSIZE < total_remain ? READSIZE : total_remain, &len); 
#ifdef DEBUG
      printf("st=%d\n",st);
#endif
      Py_END_ALLOW_THREADS
      if (st && (st!=SANE_STATUS_EOF))
      {   sane_cancel(self->h);
          return PySane_Error(st);
        }
#ifdef WRITE_PGM
      printf("p5_write: read %d of %d st=%d\n", len, READSIZE, st);
      fwrite(buffer, 1, len, fp);
#endif

      buffer_index = 0;
      total_remain -= len;

      while (len > 0)
	{
	  /* copy at most the number of bytes that fit into (the rest of)
	     one line: */
	  cp_num_bytes = (len > remain_bytes_line ? remain_bytes_line : len);
	  remain_bytes_line -= cp_num_bytes;
	  len -= cp_num_bytes;
#ifdef DEBUG
	  printf("copying %d bytes from b_idx %d to d_idx %d\n",
		 cp_num_bytes, buffer_index,
		 line * arr_bytes_per_line + line_index);
	  printf("len is now %d\n", len);
#endif
#ifdef WITH_NUMPY
      memcpy(PyArray_DATA(pyArr)+ line * arr_bytes_per_line + line_index, 
             buffer + buffer_index, cp_num_bytes);
#elif defined WITH_NUMARRAY
      memcpy(pyArr->data + line * arr_bytes_per_line + line_index,
		     buffer + buffer_index, cp_num_bytes);
#endif
	  buffer_index += cp_num_bytes;
	  if (remain_bytes_line ==0)
	    {
	      /* The line has been completed, so reinitialize remain_bytes_line
		 increase the line counter, and reset line_index */
#ifdef DEBUG
	      printf("line %d full, skipping %d bytes\n",line,num_pad_bytes);
#endif
	      remain_bytes_line = arr_bytes_per_line;
	      line++;
	      line_index = 0;
	      /* Skip the number of bytes in the input stream which
		 are not used: */
	      len -= num_pad_bytes;
	      buffer_index += num_pad_bytes;
	    }
	  else
	    line_index += cp_num_bytes;
	}
    }
#ifdef WRITE_PGM
  fclose(fp);
  printf("p5_write finished\n");
#endif
  sane_cancel(self->h);
  return (PyObject*) pyArr;
}

static PyObject *
PySane_checkSaneArray(PyObject *self, PyObject *args)
{
  return Py_BuildValue("ssi", "checkSaneArray", ARRAY_SUPPORT_MSG, ARRAY_IMPORTED);
}

#else  
static PyObject *
SaneDev_arr_snap(SaneDevObject *self, PyObject *args)
{
  PyErr_SetString(ErrorObject, "Not compiled with array support (numpy or numarray)");
  return NULL;
}

static PyObject *
PySane_checkSaneArray(PyObject *self, PyObject *args)
{
  PyErr_SetString(ErrorObject, "Not compiled with array support (numpy or numarray)");
  return NULL;
}

#endif /* WITH_NUMARRAY or WITH_NUMPY both defining ARRAY_SUPPORT */

static PyMethodDef SaneDev_methods[] = {
	{"get_parameters",	(PyCFunction)SaneDev_get_parameters,	1},

	{"get_options",	(PyCFunction)SaneDev_get_options,	1},
	{"get_option",	(PyCFunction)SaneDev_get_option,	1},
	{"set_option",	(PyCFunction)SaneDev_set_option,	1},
	{"set_auto_option",	(PyCFunction)SaneDev_set_auto_option,	1},

	{"start",	(PyCFunction)SaneDev_start,	1},
	{"cancel",	(PyCFunction)SaneDev_cancel,	1},
	{"snap",	(PyCFunction)SaneDev_snap,	1},
/* #ifdef ARRAY_SUPPORT */
	{"arr_snap",	(PyCFunction)SaneDev_arr_snap,	1},
/*#endif  */
	{"fileno",	(PyCFunction)SaneDev_fileno,	1},
 	{"close",	(PyCFunction)SaneDev_close,	1},
	{NULL,		NULL}		/* sentinel */
};

static PyTypeObject SaneDev_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"SaneDev",			/*tp_name*/
	sizeof(SaneDevObject),	/*tp_basicsize*/
	0,			/*tp_itemsize*/
	/* methods */
	(destructor)SaneDev_dealloc, /*tp_dealloc*/
	0,			/*tp_print*/
    0,                          /*tp_getattr*/
    0,                          /*tp_setattr*/
    0,                          /*tp_compare*/
    0,                          /*tp_repr*/
    0,                          /*tp_as_number */
    0,                          /*tp_as_sequence */
    0,                          /*tp_as_mapping */
    0,                          /*tp_hash*/
    0,                          /*tp_call*/
    0,                          /*tp_str*/
    0,                          /*tp_getattro*/
    0,                          /*tp_setattro*/
    0,                          /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT,         /*tp_flags*/
    0,                          /*tp_doc*/
    0,                          /*tp_traverse*/
    0,                          /*tp_clear*/
    0,                          /*tp_richcompare*/
    0,                          /*tp_weaklistoffset*/
    0,                          /*tp_iter*/
    0,                          /*tp_iternext*/
    SaneDev_methods,            /*tp_methods*/
    0,                          /*tp_members*/
    0,                          /*tp_getset*/
};

/* --------------------------------------------------------------------- */

static PyObject *
PySane_init(PyObject *self, PyObject *args)
{
  SANE_Status st;
  SANE_Int version;

  if (!PyArg_ParseTuple(args, ""))
    return NULL;

  /* XXX Authorization is not yet supported */
  st=sane_init(&version, NULL);
  if (st) return PySane_Error(st);
  return Py_BuildValue("iiii", version, SANE_VERSION_MAJOR(version),
		       SANE_VERSION_MINOR(version), SANE_VERSION_BUILD(version));
}

static PyObject *
PySane_exit(PyObject *self, PyObject *args)
{
  if (!PyArg_ParseTuple(args, ""))
    return NULL;

  sane_exit();
  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject *
PySane_get_devices(PyObject *self, PyObject *args)
{
  const SANE_Device **devlist;
  const SANE_Device *dev;
  SANE_Status st;
  PyObject *list;
  int local_only = 0, i;

  if (!PyArg_ParseTuple(args, "|i", &local_only))
    {
      return NULL;
    }

  Py_BEGIN_ALLOW_THREADS
  st=sane_get_devices(&devlist, local_only);
  Py_END_ALLOW_THREADS
  if (st) return PySane_Error(st);
  if (!(list = PyList_New(0)))
	    return NULL;
  for(i=0; devlist[i]!=NULL; i++)
    {
      dev=devlist[i];
      PyList_Append(list, Py_BuildValue("ssss", dev->name, dev->vendor,
					dev->model, dev->type));
    }

  return list;
}

/* Function returning new SaneDev object */

static PyObject *
PySane_open(PyObject *self, PyObject *args)
{
	SaneDevObject *rv;
	SANE_Status st;
	char *name;

	if (!PyArg_ParseTuple(args, "s", &name))
		return NULL;
	rv = newSaneDevObject();
	if ( rv == NULL )
	    return NULL;
	Py_BEGIN_ALLOW_THREADS
	st = sane_open(name, &(rv->h));
	Py_END_ALLOW_THREADS
	if (st)
	  {
	    Py_DECREF(rv);
	    return PySane_Error(st);
	  }
	return (PyObject *)rv;
}

static PyObject *
PySane_OPTION_IS_ACTIVE(PyObject *self, PyObject *args)
{
  SANE_Int cap;
  long lg;

  if (!PyArg_ParseTuple(args, "l", &lg))
    return NULL;
  cap=lg;
  return PyInt_FromLong( SANE_OPTION_IS_ACTIVE(cap));
}

static PyObject *
PySane_OPTION_IS_SETTABLE(PyObject *self, PyObject *args)
{
  SANE_Int cap;
  long lg;

  if (!PyArg_ParseTuple(args, "l", &lg))
    return NULL;
  cap=lg;
  return PyInt_FromLong( SANE_OPTION_IS_SETTABLE(cap));
}

/* List of functions defined in the module */

static PyMethodDef PySane_methods[] = {
    {"init",    PySane_init,        1},
	{"exit",	PySane_exit,		1},
	{"get_devices",	PySane_get_devices,	1},
	{"_open",	PySane_open,	1},
	{"OPTION_IS_ACTIVE",	PySane_OPTION_IS_ACTIVE,	1},
	{"OPTION_IS_SETTABLE",	PySane_OPTION_IS_SETTABLE,	1},
    {"checkSaneArray",  PySane_checkSaneArray,      1},
	{NULL,		NULL}		/* sentinel */
};


static void
insint(PyObject *d, char *name, int value)
{
	PyObject *v = PyInt_FromLong((long) value);
	if (!v || PyDict_SetItemString(d, name, v))
		Py_FatalError("can't initialize sane module");

	Py_DECREF(v);
}

#if PY_MAJOR_VERSION >= 3
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
#else /* if PY_MAJOR_VERSION < 3 */

PyMODINIT_FUNC
init_sane(void)
{
    /* Create the module and add the functions */
    PyObject *m = Py_InitModule("_sane", PySane_methods);
    if(!m)
        return;
#endif

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
	if (PyErr_Occurred())
		Py_FatalError("can't initialize module _sane");

#ifdef ARRAY_SUPPORT
#ifdef WITH_NUMPY
    import_array();
#elif defined WITH_NUMARRAY
	import_libnumarray();
#endif
    if (PyErr_Occurred())
      Py_FatalError("can't import library");
	/*  PyErr_Clear();*/
	else
	  /* this global variable is declared just in front of the
	     arr_snap() function and should be set to 1 after
	     successfully importing the numarray module. */
	  ARRAY_IMPORTED = 1;

#endif /* WITH_ARRAY */
#if PY_MAJOR_VERSION >= 3
    return m;
#endif
}
