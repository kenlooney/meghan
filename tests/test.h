#ifndef MEG_TEST_H
#define MEG_TEST_H
#include <stdio.h>
static int failures;
#define EXPECT(x) do { if (!(x)) { fprintf(stderr, "%s:%d: failed: %s\n", __FILE__, __LINE__, #x); ++failures; } } while (0)
#define RESULT() (failures ? 1 : 0)
#endif
