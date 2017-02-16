#pragma once

#ifdef __cplusplus
#if __cplusplus >= 201103L
// C++11
#include <cstdint>
#include <cstdlib>
#else
// C++98
#include <cstdlib>
#include <stdint.h>
#define nullptr NULL
#endif
#else
// C
#include <stdint.h>
#include <stdlib.h>
#endif
