#ifndef __math_compat_h
#define __math_compat_h

/* Define isnan, isinf, infinity and nan on Windows/MSVC */

#ifndef HAVE_DECL_ISNAN
# ifdef HAVE_DECL__ISNAN
#include <float.h>
#define isnan(x) _isnan(x)
# endif
#endif

#ifndef HAVE_DECL_ISINF
# ifdef HAVE_DECL__FINITE
#include <float.h>
#define isinf(x) (!_finite(x))
# endif
#endif

#ifndef HAVE_DECL_INFINITY
#include <float.h>
#ifndef INFINITY
#define INFINITY (DBL_MAX + DBL_MAX)
#endif
#define HAVE_DECL_INFINITY
#endif

#ifndef HAVE_DECL_NAN
#ifndef NAN
#define NAN (INFINITY - INFINITY)
#endif
#define HAVE_DECL_NAN
#endif

#endif
