/*------------------------------------------------------------------------------
 * Copyright (c) 2017
 *     Michael Theall (mtheall)
 *
 * This file is part of tex3ds.
 *
 * tex3ds is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * tex3ds is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with tex3ds.  If not, see <http://www.gnu.org/licenses/>.
 *----------------------------------------------------------------------------*/
/** @file compat.h
 *  @brief C/C++ platform compatibility
 */
#pragma once

#ifdef __cplusplus
#if __cplusplus >= 201103L
// C++11
#include <cstdint>
#include <cstdlib>

/** @brief Deleted constructor decoration */
#define DELETE_CONSTRUCTOR = delete
#else
// C++98
#include <cstdlib>
#include <stdint.h>
#define nullptr NULL

/** @brief Deleted constructor decoration */
#define DELETE_CONSTRUCTOR
#endif
#else
// C
#include <stdint.h>
#include <stdlib.h>
#endif
