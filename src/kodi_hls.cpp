/*
 * kodi_hls.cpp Copyright (C) 2017 Anthony Waters <awaters1@gmail.com>
 */

#include <iostream>

#define LOGTAG                  "[KODI_HLS] "

#include "globals.h"

#include "kodi_hls.h"

bool download_playlist_impl(const char *url, hls::Playlist &playlist) {
  // open the file
  void* file = xbmc->CURLCreate(url);
  if (!file)
    return false;
  playlist.set_url(url);
  xbmc->CURLAddOption(file, XFILE::CURL_OPTION_PROTOCOL, "seekable", "0");
  xbmc->CURLAddOption(file, XFILE::CURL_OPTION_PROTOCOL, "acceptencoding", "gzip");
  xbmc->CURLOpen(file, XFILE::READ_CHUNKED | XFILE::READ_NO_CACHE);

  // read the file
  static const unsigned int CHUNKSIZE = 16384;
  char buf[CHUNKSIZE];
  bool read;
  while ((read = xbmc->ReadFileString(file, buf, CHUNKSIZE)) > 0 && read) {
    // Take \n off of buffer
    if (buf[strlen(buf) - 1] == '\n') {
      buf[strlen(buf) - 1] = '\0';
    }
    bool ret = playlist.write_data(buf);
    if (!ret) {
      break;
    }
  }

  xbmc->CloseFile(file);
  return !read;
}

hls::MediaPlaylist KodiSession::download_playlist(std::string url) {
  hls::MediaPlaylist playlist;
  download_playlist_impl(url.c_str(), playlist);
  return playlist;
}

KodiSession::KodiSession(KodiMasterPlaylist master_playlist, double bandwidth, std::string profile_path,
      int min_bandwidth, int max_bandwidth, bool manual_streams, std::unordered_map<StateAction, double> q_map,
      std::unordered_map<State, double> explore_map) :
    hls::Session(master_playlist, new KodiDownloader(bandwidth), min_bandwidth, max_bandwidth, manual_streams, q_map, explore_map),
    profile_path(profile_path) {

}

KodiSession::~KodiSession() {
  std::string fn(profile_path + "bandwidth.bin");
  FILE* f = fopen(fn.c_str(), "wb");
  if (f) {
    double val(downloader->get_average_bandwidth());
    fwrite((const char*)&val, sizeof(double), 1, f);
    fclose(f);
  }
  fn = profile_path + "q_map.bin";
  f = fopen(fn.c_str(), "wb");
  if (f) {
    std::unordered_map<StateAction, double> q_map = segment_storage.get_q_map();
    for(auto it : q_map) {
      uint32_t buff_s = it.first.state.get_buffer_level_s();
      uint32_t bw_kbps = it.first.state.get_bandwidth_kbps();
      uint32_t prev_qual = it.first.state.get_previous_quality_kbps();
      uint32_t curr_qual = it.first.action.get_current_quality_kbps();
      fwrite((const char*)&buff_s, sizeof(uint32_t), 1, f);
      fwrite((const char*)&bw_kbps, sizeof(uint32_t), 1, f);
      fwrite((const char*)&prev_qual, sizeof(uint32_t), 1, f);
      fwrite((const char*)&curr_qual, sizeof(uint32_t), 1, f);
      fwrite((const char*)&it.second, sizeof(double), 1, f);
    }
    fclose(f);
  }
  fn = profile_path + "explore_map.bin";
  f = fopen(fn.c_str(), "wb");
  if (f) {
    std::unordered_map<State, double> explore_map = segment_storage.get_explore_map();
    for(auto it : explore_map) {
      uint32_t buff_s = it.first.get_buffer_level_s();
      uint32_t bw_kbps = it.first.get_bandwidth_kbps();
      uint32_t prev_qual = it.first.get_previous_quality_kbps();
      fwrite((const char*)&buff_s, sizeof(uint32_t), 1, f);
      fwrite((const char*)&bw_kbps, sizeof(uint32_t), 1, f);
      fwrite((const char*)&prev_qual, sizeof(uint32_t), 1, f);
      fwrite((const char*)&it.second, sizeof(double), 1, f);
    }
    fclose(f);
  }
}

void KodiMasterPlaylist::select_media_playlist() {
  if (media_playlist.empty()) {
    // Assume the URL we were given was a media playlist and not a master playlist
    hls::MediaPlaylist playlist;
    download_playlist_impl(get_url().c_str(), playlist);
    playlist.valid = true;
    media_playlist.push_back(playlist);
  }
  for(std::vector<hls::MediaPlaylist>::iterator it = media_playlist.begin(); it != media_playlist.end(); ++it) {
    xbmc->Log(ADDON::LOG_DEBUG, LOGTAG "Media Playlist: bandwidth: %d url: %s", it->bandwidth, it->get_url().c_str());
  }
}

bool KodiMasterPlaylist::open(const char *url) {
  return download_playlist_impl(url, *this);
}
