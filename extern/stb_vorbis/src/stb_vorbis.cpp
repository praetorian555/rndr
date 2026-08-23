// stb_vorbis is not clean under /W4 and this build treats warnings as errors, so the implementation is
// compiled with warnings off. Every other file includes the header with STB_VORBIS_HEADER_ONLY defined.
#if defined(_MSC_VER)
// Reported by the code generator after the whole file is parsed, so a push/pop around the include does not
// reach them; they stay disabled for the rest of this translation unit, which is only this include.
#pragma warning(disable : 4701)  // potentially uninitialized local variable used
#pragma warning(disable : 4703)  // potentially uninitialized local pointer variable used
#pragma warning(push, 0)
#endif

#include "stb_vorbis/stb_vorbis.h"

#if defined(_MSC_VER)
#pragma warning(pop)
#endif
