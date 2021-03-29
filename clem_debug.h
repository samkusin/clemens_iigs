#ifndef CLEM_DEBUG_H
#define CLEM_DEBUG_H

/* TODO: something a little less reliant on clib */
#include <stdio.h>
#include <assert.h>

#define CLEM_UNIMPLEMENTED(_fmt_, ...) do { \
  fprintf(stderr, _fmt_, __VA_ARGS__); \
  fputc('\n', stderr);  assert(0); \
} while (0)

#define CLEM_ASSERT(_cond_) do { \
  assert(_cond_); \
} while (0)

#define CLEM_WARN(_fmt_, ...) do { \
  fprintf(stderr, _fmt_, __VA_ARGS__); \
  fputc('\n', stderr); \
} while (0)

#define CLEM_LOG(_fmt_, ...) do { \
  fprintf(stdout, _fmt_, __VA_ARGS__); \
  fputc('\n', stdout); \
} while (0)


#endif
