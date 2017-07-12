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

#include "hls/HLS.h"
#include "hls/session.h"
#include "downloader/kodi_downloader.h"

class KodiMasterPlaylist : public hls::MasterPlaylist {
  public:
    bool open(const char *url);
    void select_media_playlist();
};

class KodiSession : public hls::Session {
public:
  KodiSession(KodiMasterPlaylist master_playlist, double bandwidth, std::string profile_path,
      int min_bandwidth, int max_bandwidth, bool manual_streams, std::unordered_map<Stage, double> q_map);
  ~KodiSession();
protected:
  hls::MediaPlaylist download_playlist(std::string url);
  std::string download_aes_key(std::string aes_uri);
private:
  std::string profile_path;
};
