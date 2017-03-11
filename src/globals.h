/*
 * globals.h Copyright (C) 2017 Anthony Waters <awaters1@gmail.com>
 */

#pragma once

#include "libXBMC_addon.h"

extern ADDON::CHelper_libXBMC_addon *xbmc;
extern std::uint16_t kodiDisplayWidth, kodiDisplayHeight;

#define SAFE_DELETE(p)       do { delete (p);     (p)=NULL; } while (0)


