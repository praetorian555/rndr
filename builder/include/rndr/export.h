/* rndr/export.h for builder's build of rndr, which is static: what CMake's generate_export_header writes
   with RNDR_STATIC_DEFINE set. The CMake build generates its own into its build tree. */
#ifndef RNDR_EXPORT_H
#define RNDR_EXPORT_H

#define RNDR_EXPORT
#define RNDR_NO_EXPORT

#if defined(_MSC_VER)
#define RNDR_DEPRECATED __declspec(deprecated)
#else
#define RNDR_DEPRECATED __attribute__((__deprecated__))
#endif
#define RNDR_DEPRECATED_EXPORT RNDR_EXPORT RNDR_DEPRECATED
#define RNDR_DEPRECATED_NO_EXPORT RNDR_NO_EXPORT RNDR_DEPRECATED

#endif /* RNDR_EXPORT_H */
