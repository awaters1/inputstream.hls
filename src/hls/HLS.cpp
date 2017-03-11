/*
 * HLS.cpp Copyright (C) 2017 Anthony Waters <awaters1@gmail.com>
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>

#include "HLS.h"

bool hls::MasterPlaylist::write_data(std::string line) {
  is_m3u8 = is_m3u8 || line == "#EXTM3U";
  if (!is_m3u8) {
      std::cerr << "First line isn't #EXTM3U" << std::endl;
      return false;
  }
  return true;
}

bool hls::FileMasterPlaylist::open(const char *file_path) {
  std::ifstream playlist_file(file_path);
  if (!playlist_file.is_open()) {
      std::cerr << "Unable to open " << file_path << std::endl;
      return false;
  }
  std::string line;
  while(std::getline(playlist_file, line)) {
      if (!write_data(line)) {
          return false;
      }
  }

  playlist_file.close();

  return true;
}
