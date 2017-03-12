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
  is_m3u8 = is_m3u8 || line.find("#EXTM3U") == 0;
  if (!is_m3u8) {
      std::cerr << "First line isn't #EXTM3U" << std::endl;
      return false;
  }
  if (in_stream) {
      if (media_playlist.empty()) {
          std::cerr << "In stream, but no streams found" << std::endl;
          return false;
      }
      MediaPlaylist stream = media_playlist.back();
      media_playlist.pop_back();
      stream.set_url(base_url + line);
      media_playlist.push_back(stream);
      in_stream = false;
      return true;
  }
  if (line.find("#EXT-X-STREAM-INF") == 0) {
      in_stream = true;
      MediaPlaylist stream;
      stream.program_id = get_attribute_value(line, "PROGRAM-ID");
      stream.bandwidth = get_number_attribute_value(line, "BANDWIDTH");
      media_playlist.push_back(stream);
  }
  return true;
}

hls::MasterPlaylist::MasterPlaylist()
: in_stream(false) {

}

hls::MasterPlaylist::~MasterPlaylist() {

}

std::vector<std::string> hls::MediaPlaylist::get_attributes(std::string line) {
  std::vector<std::string> attributes;
  size_t colon_index = line.find_first_of(':');
  if (colon_index == std::string::npos) {
      return attributes;
  }
  size_t starting_index = colon_index + 1;
  while(starting_index < line.length()) {
      size_t comma_index = line.find_first_of(',', starting_index);
      if (comma_index == std::string::npos) {
          comma_index = line.length();
      }
      size_t length = comma_index - starting_index;
      std::string attribute_value = line.substr(starting_index, length);
      attributes.push_back(attribute_value);
      starting_index = comma_index + 1;
  }
  return attributes;
}

bool hls::MediaPlaylist::write_data(std::string line) {
  is_m3u8 = is_m3u8 || line.find("#EXTM3U") == 0;
  if (!is_m3u8) {
      std::cerr << "First line isn't #EXTM3U" << std::endl;
      return false;
  }
  if (in_segment) {
      if (segments.empty()) {
          std::cerr << "In segment, but no segments found" << std::endl;
          return false;
      }
      Segment segment = segments.back();
      segments.pop_back();
      segment.set_url(base_url + line);
      segments.push_back(segment);
      in_segment = false;
      return true;
  }
  if (line.find("#EXT-X-TARGETDURATION") != std::string::npos) {
      segment_target_duration = std::stod(get_attributes(line)[0]);
  } else if (line.find("#EXT-X-MEDIA-SEQUENCE") != std::string::npos) {
      starting_media_sequence = std::stod(get_attributes(line)[0]);
      current_media_sequence = starting_media_sequence;
  } else if (line.find("#EXTINF") != std::string::npos) {
      in_segment = true;
      std::vector<std::string> attributes = get_attributes(line);
      Segment segment;
      segment.media_sequence = current_media_sequence++;
      segment.duration = std::stod(attributes[0]);
      segment.description = attributes[1];
      segments.push_back(segment);
  } else if (line == "#EXT-X-ENDLIST") {
      // TODO: Handle end list
  }
  return true;
}

hls::MediaPlaylist::MediaPlaylist()
: segment_target_duration(0), starting_media_sequence(0), current_media_sequence(0), in_segment(false) {

}

hls::MediaPlaylist::~MediaPlaylist() {

}

void hls::Resource::set_url(std::string url) {
  this->url = url;
  size_t last_slash = url.find_last_of('/');
  if (last_slash == std::string::npos) {
      base_url = "";
  }
  base_url = url.substr(0, last_slash + 1);
}

bool open_playlist_file(const char *file_path, hls::Playlist &playlist) {
  std::ifstream playlist_file(file_path);
  if (!playlist_file.is_open()) {
      std::cerr << "Unable to open " << file_path << std::endl;
      return false;
  }
  playlist.set_url(file_path);
  std::string line;
  while(std::getline(playlist_file, line)) {
      if (!playlist.write_data(line)) {
          return false;
      }
  }

  playlist_file.close();
  return true;
}

bool hls::FileMasterPlaylist::open(const char *file_path) {
  if (!open_playlist_file(file_path, *this)) {
      return false;
  }

  for(std::vector<MediaPlaylist>::iterator it = media_playlist.begin(); it != media_playlist.end(); ++it) {
      open_playlist_file(it->get_url().c_str(), *it);
  }

  return true;
}

bool hls::FileMediaPlaylist::open(const char *file_path) {
  if (!open_playlist_file(file_path, *this)){
      return false;
  }

  return true;
}
