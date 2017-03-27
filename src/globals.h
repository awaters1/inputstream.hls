/*
 * globals.h Copyright (C) 2017 Anthony Waters <awaters1@gmail.com>
 */

#pragma once

#include "libKODI_inputstream.h"
#include "libXBMC_addon.h"
#include "libXBMC_codec.h"

extern ADDON::CHelper_libXBMC_addon *xbmc;
extern std::uint16_t kodiDisplayWidth, kodiDisplayHeight;
extern bool g_bExtraDebug;
extern CHelper_libKODI_inputstream *ipsh;
extern CHelper_libXBMC_codec *CODEC;

#define SAFE_DELETE(p)       do { delete (p);     (p)=NULL; } while (0)


