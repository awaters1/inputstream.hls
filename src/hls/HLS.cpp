/*
 * HLS.cpp Copyright (C) 2017 Anthony Waters <awaters1@gmail.com>
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>

#include "HLS.h"

std::string hls::MasterPlaylist::get_attribute_value(std::string line, std::string attribute_name) {
  size_t index = line.find(attribute_name);
  if (index == std::string::npos) {
      return "";
  }
  size_t comma_index = line.find_first_of(',', index);
  if (comma_index == std::string::npos) {
      comma_index = line.length();
  }
  size_t start = index + attribute_name.length() + 1;
  size_t end = comma_index;
  size_t length = end - start;
  std::string attribute_value = line.substr(start, length);
  return attribute_value;
}

uint32_t hls::MasterPlaylist::get_number_attribute_value(std::string line, std::string attribute_name) {
  std::string attribute_value = get_attribute_value(line, attribute_name);
  if (attribute_value.length() == 0) {
    return -1;
  }
  return atoi(attribute_value.c_str());
}

bool hls::MasterPlaylist::write_data(std::string line) {
  is_m3u8 = is_m3u8 || line == "#EXTM3U";
  if (!is_m3u8) {
      std::cerr << "First line isn't #EXTM3U" << std::endl;
      return false;
  }
  if (in_stream) {
      if (streams.empty()) {
          std::cerr << "In stream, but no streams found" << std::endl;
          return false;
      }
      MediaPlaylist *stream = streams.back();
      stream->url = base_url + line;
      in_stream = false;
      return true;
  }
  if (line.find("#EXT-X-STREAM-INF") == 0) {
      in_stream = true;
      MediaPlaylist *stream = new MediaPlaylist();
      stream->program_id = get_attribute_value(line, "PROGRAM-ID");
      stream->bandwidth = get_number_attribute_value(line, "BANDWIDTH");
      streams.push_back(stream);
  }
  return true;
}

void hls::MasterPlaylist::set_url(std::string url) {
  this->url = url;
  size_t last_slash = url.find_last_of('/');
  if (last_slash == std::string::npos) {
      base_url = "";
  }
  base_url = url.substr(0, last_slash + 1);
}

hls::MasterPlaylist::MasterPlaylist()
: is_m3u8(false), in_stream(false) {

}

hls::MasterPlaylist::~MasterPlaylist() {
  for(std::vector<MediaPlaylist*>::iterator it = streams.begin(); it != streams.end(); ++it) {
      delete *it;
  }
}

bool hls::FileMasterPlaylist::open(const char *file_path) {
  std::ifstream playlist_file(file_path);
  if (!playlist_file.is_open()) {
      std::cerr << "Unable to open " << file_path << std::endl;
      return false;
  }
  set_url(file_path);
  std::string line;
  while(std::getline(playlist_file, line)) {
      if (!write_data(line)) {
          return false;
      }
  }

  playlist_file.close();

  return true;
}
