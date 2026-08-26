/*
 * Mock libsane backend for the SaneDev_snap params-cache regression test.
 *
 * Emulates the epsonscan2 backend behavior (see epsonscan2_bug.md):
 * - sane_get_parameters() returns SANE_STATUS_INVAL on a second call after
 *   sane_start (finding 18: the wrapper's get_parameters succeeds, but
 *   snap()'s redundant call fails without the params-cache fix).
 * - sane_read() accepts any request size and serves a deterministic byte
 *   stream (finding 13: the real backend accepts per-scanline reads).
 *
 * Loaded via LD_PRELOAD in a dedicated subprocess (see
 * tests/test_snap_chunked_read.py) so it only shadows libsane inside the
 * test's controlled process.
 *
 * Scenario knobs (environment variables):
 *   SNAP_MOCK_DEPTH      1 or 8 (default 8)
 *   SNAP_MOCK_PARTIAL    1 = truncate the final scanline by 7 bytes
 */
#include <sane/sane.h>

#include <stdlib.h>
#include <string.h>

#define MOCK_DEVICE "mock:0"
#define MOCK_LINES   2280
#define MOCK_PPL_8   2280
#define MOCK_BPL_8   2280
#define MOCK_PPL_1   1700
#define MOCK_BPL_1   213

static SANE_Int g_depth = 8;
static SANE_Int g_partial = 0;
static long g_offset = 0;
static int g_started = 0;
static int g_get_params_count = 0;

static void
mock_configure(void)
{
  const char *v;
  if((v = getenv("SNAP_MOCK_DEPTH")))
    g_depth = atoi(v);
  if((v = getenv("SNAP_MOCK_PARTIAL")))
    g_partial = atoi(v);
}

/* Deterministic stream: byte k = (k*37 + 11) % 256 */
static SANE_Byte
mock_byte(long k)
{
  return (SANE_Byte)(((k * 37 + 11) % 256));
}

static long
mock_stream_len(void)
{
  long full = MOCK_LINES * (g_depth == 1 ? MOCK_BPL_1 : MOCK_BPL_8);
  if(g_partial)
    full -= 7;
  return full;
}

SANE_Status
sane_init(SANE_Int *version_code, SANE_Auth_Callback authorize)
{
  (void)authorize;
  mock_configure();
  g_offset = 0;
  g_started = 0;
  g_get_params_count = 0;
  if(version_code)
    *version_code = SANE_VERSION_CODE(SANE_CURRENT_MAJOR,
                                      SANE_CURRENT_MINOR, 0);
  return SANE_STATUS_GOOD;
}

void
sane_exit(void)
{
}

SANE_Status
sane_get_devices(const SANE_Device ***device_list, SANE_Bool local_only)
{
  (void)local_only;
  static const SANE_Device device = {
    MOCK_DEVICE, "Mock Vendor", "Mock Scanner", "virtual device"
  };
  static const SANE_Device *devices[] = { &device, NULL };
  *device_list = devices;
  return SANE_STATUS_GOOD;
}

SANE_Status
sane_open(SANE_String_Const devicename, SANE_Handle *handle)
{
  (void)devicename;
  *handle = (SANE_Handle)&g_started;
  g_started = 0;
  g_get_params_count = 0;
  g_offset = 0;
  return SANE_STATUS_GOOD;
}

void
sane_close(SANE_Handle handle)
{
  (void)handle;
}

void
sane_cancel(SANE_Handle handle)
{
  (void)handle;
}

const SANE_Option_Descriptor *
sane_get_option_descriptor(SANE_Handle handle, SANE_Int option)
{
  (void)handle;
  (void)option;
  return NULL;
}

SANE_Status
sane_control_option(SANE_Handle handle, SANE_Int option, SANE_Action action,
                    void *value, SANE_Int *info)
{
  (void)handle;
  (void)option;
  (void)action;
  (void)value;
  (void)info;
  return SANE_STATUS_INVAL;
}

SANE_Status
sane_get_select_fd(SANE_Handle handle, SANE_Int *fd)
{
  (void)handle;
  (void)fd;
  return SANE_STATUS_INVAL;
}

SANE_Status
sane_start(SANE_Handle handle)
{
  (void)handle;
  g_started = 1;
  g_get_params_count = 0;
  return SANE_STATUS_GOOD;
}

SANE_Status
sane_get_parameters(SANE_Handle handle, SANE_Parameters *params)
{
  (void)handle;

  /* Replicate epsonscan2 behavior: the first sane_get_parameters call after
     sane_start succeeds; subsequent calls return INVAL. This is the bug that
     finding 18 identified - the python-sane wrapper's get_parameters() call
     succeeds, but snap()'s redundant second call fails. */
  if(g_started)
    {
      ++g_get_params_count;
      if(g_get_params_count > 1)
        return SANE_STATUS_INVAL;
    }

  memset(params, 0, sizeof(*params));
  params->format = SANE_FRAME_GRAY;
  params->last_frame = SANE_TRUE;
  if(g_depth == 1)
    {
      params->pixels_per_line = MOCK_PPL_1;
      params->lines = MOCK_LINES;
      params->depth = 1;
      params->bytes_per_line = MOCK_BPL_1;
    }
  else
    {
      params->pixels_per_line = MOCK_PPL_8;
      params->lines = MOCK_LINES;
      params->depth = 8;
      params->bytes_per_line = MOCK_BPL_8;
    }
  return SANE_STATUS_GOOD;
}

SANE_Status
sane_read(SANE_Handle handle, SANE_Byte *data, SANE_Int max_length,
          SANE_Int *length)
{
  (void)handle;
  *length = 0;

  if(!g_started)
    return SANE_STATUS_INVAL;

  long stream_len = mock_stream_len();
  if(g_offset >= stream_len)
    return SANE_STATUS_EOF;

  long avail = stream_len - g_offset;
  SANE_Int n = max_length < avail ? max_length : (SANE_Int)avail;
  long k = g_offset;
  SANE_Int i;
  for(i = 0; i < n; ++i, ++k)
    data[i] = mock_byte(k);
  g_offset += n;
  *length = n;
  return SANE_STATUS_GOOD;
}

SANE_String_Const
sane_strstatus(SANE_Status status)
{
  switch(status)
    {
      case SANE_STATUS_GOOD:           return "Success";
      case SANE_STATUS_UNSUPPORTED:    return "Operation is not supported";
      case SANE_STATUS_CANCELLED:      return "Operation was cancelled";
      case SANE_STATUS_DEVICE_BUSY:    return "Device busy";
      case SANE_STATUS_INVAL:          return "Invalid argument";
      case SANE_STATUS_EOF:            return "End of file reached";
      case SANE_STATUS_JAMMED:         return "Document feeder jammed";
      case SANE_STATUS_NO_DOCS:        return "Document feeder out of documents";
      case SANE_STATUS_COVER_OPEN:     return "Scanner cover is open";
      case SANE_STATUS_IO_ERROR:       return "I/O error";
      case SANE_STATUS_NO_MEM:         return "Out of memory";
      case SANE_STATUS_ACCESS_DENIED:  return "Access denied";
      default:                         return "Unknown status";
    }
}
