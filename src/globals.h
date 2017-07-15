#pragma once
/*
 *      Copyright (C) 2017 Anthony Waters <awaters1@gmail.com>
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  <http://www.gnu.org/licenses/>.
 *
 */

#include "libKODI_inputstream.h"
#include "libXBMC_addon.h"
#include "libXBMC_codec.h"

//! @todo original definition is in DVDClock.h
#define DVD_NOPTS_VALUE 0xFFF0000000000000
#define DVD_TIME_BASE 1000000

const int SEGMENT_TIMEOUT_DELAY = 60000;
const int MAX_BUFFER_MS = 8000;

extern ADDON::CHelper_libXBMC_addon *xbmc;
extern std::uint16_t kodiDisplayWidth, kodiDisplayHeight;
extern bool g_bExtraDebug;
extern CHelper_libKODI_inputstream *ipsh;
extern CHelper_libXBMC_codec *CODEC;

#define SAFE_DELETE(p)       do { delete (p);     (p)=NULL; } while (0)


