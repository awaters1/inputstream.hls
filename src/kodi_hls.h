/*
 * kodi_hls.h Copyright (C) 2017 Anthony Waters <awaters1@gmail.com>
 */

#pragma once

#include "hls/HLS.h"
#include "hls/session.h"

class KodiMasterPlaylist : public hls::MasterPlaylist {
  public:
    bool open(const char *url);
    void select_media_playlist();
};

class KodiSession : public hls::Session {
public:
  KodiSession(KodiMasterPlaylist master_playlist) : hls::Session(master_playlist) { };
protected:
    bool download_segment(hls::ActiveSegment *active_segment);
    hls::MediaPlaylist download_playlist(std::string url);
    std::string download_aes_key(std::string aes_uri);
};
