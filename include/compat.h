/*------------------------------------------------------------------------------
 * Copyright (c) 2017
 *     Michael Theall (mtheall)
 *
 * This file is part of 3dstex.
 *
 * 3dstex is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * 3dstex is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with 3dstex.  If not, see <http://www.gnu.org/licenses/>.
 *----------------------------------------------------------------------------*/
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
