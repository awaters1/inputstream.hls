/*
 * kodi_hls.h Copyright (C) 2017 Anthony Waters <awaters1@gmail.com>
 */

#pragma once

#include "hls/HLS.h"

class KodiMasterPlaylist : public hls::MasterPlaylist {
  public:
    bool open(const char *url);
    void select_media_playlist();
};
