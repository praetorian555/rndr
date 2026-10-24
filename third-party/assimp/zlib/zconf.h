/* zconf.h as zlib's CMake build configures it for assimp 6.0.2: no Z_PREFIX, and Z_HAVE_UNISTD_H where unistd.h exists. */
#if !defined(_WIN32) && !defined(Z_HAVE_UNISTD_H)
#    define Z_HAVE_UNISTD_H
#endif
#include "zconf.h.in"
