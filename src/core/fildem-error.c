#include "fildem-error.h"

GQuark
fildem_error_quark(void)
{
  return g_quark_from_static_string("fildem-error-quark");
}
