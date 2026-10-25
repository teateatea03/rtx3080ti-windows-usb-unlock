/* Modified 2026-10-25: PNY/B550 manual USB port; see docs/CHANGES-PNY.md. */ #pragma once
#include <stddef.h>
#include <stdarg.h>
/* snprintf length semantics; SIZE_MAX for unsupported/invalid formats. */
size_t GuFormatV(char *Destination,size_t Capacity,const char *Format,va_list Args);
