/*
 * HLS.cpp Copyright (C) 2017 Anthony Waters <awaters1@gmail.com>
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <algorithm>
#include <climits>

#include "HLS.h"
#include "../globals.h"

#define LOGTAG                  "[HLS] "

hls::Segment::Segment() :
Resource(),
duration(0),
description(""),
media_sequence(0),
aes_uri(""),
aes_iv(""),
encrypted(false),
byte_length(0),
byte_offset(0),
valid(false),
discontinuity(false),
time_in_playlist(0)
{

}

bool hls::MasterPlaylist::write_data(std::string line) {
  is_m3u8 = is_m3u8 || line.find("#EXTM3U") == 0;
  if (!is_m3u8) {
    xbmc->Log(ADDON::LOG_ERROR, LOGTAG "First line isn't #EXTM3U");
    return false;
  }
  if (in_stream) {
      if (media_playlist.empty()) {
        xbmc->Log(ADDON::LOG_ERROR, LOGTAG "In stream, but no streams found");
        return false;
      }
      MediaPlaylist &stream = media_playlist.back();
      if (line.find("http") == std::string::npos) {
        stream.set_url(base_url + line);
      } else {
        stream.set_url(line);
      }
      in_stream = false;
      return true;
  }
  if (line.find("#EXT-X-STREAM-INF") == 0) {
      in_stream = true;
      MediaPlaylist stream;
      stream.valid = true;
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

bool hls::MediaPlaylist::write_data(std::string line) {
  is_m3u8 = is_m3u8 || line.find("#EXTM3U") == 0;
  if (!is_m3u8) {
    xbmc->Log(ADDON::LOG_ERROR, LOGTAG "First line isn't #EXTM3U");
    return false;
  }
  if (in_segment) {
      if (segments.empty()) {
        xbmc->Log(ADDON::LOG_ERROR, LOGTAG "In segment, but no segments found");
        return false;
      }
      Segment segment = segments.back();
      segments.pop_back();
      if (line.find("#EXT-X-BYTERANGE") == 0) {
        std::string byte_range = get_attributes(line)[0];
        size_t at_symbol = byte_range.find_first_of('@');
        if (at_symbol == std::string::npos) {
          segment.byte_length = atoi(byte_range.c_str());
          if (segments.empty()) {
            segment.byte_offset = 0;
          } else {
            uint32_t byte_offset = segments.back().byte_offset + segments.back().byte_length;
            segment.byte_offset = byte_offset + 1;
          }
        } else {
          std::string byte_length = byte_range.substr(0, at_symbol);
          std::string byte_offset = byte_range.substr(at_symbol + 1);
          segment.byte_length = atoi(byte_length.c_str());
          segment.byte_offset = atoi(byte_offset.c_str());
        }
      } else if (line.find("#") == 0) {
        // Skip unknown tags
      } else {
        if (line.find("http") == std::string::npos) {
          segment.set_url(base_url + line);
        } else {
          segment.set_url(line);
        }
        in_segment = false;
      }
      segments.push_back(segment);
      return true;
  }
  if (line.find("#EXT-X-TARGETDURATION") != std::string::npos) {
      segment_target_duration = std::stod(get_attributes(line)[0]);
  } else if (line.find("#EXT-X-MEDIA-SEQUENCE") != std::string::npos) {
      starting_media_sequence = std::stoul(get_attributes(line)[0]);
      current_media_sequence = starting_media_sequence;
  } else if (line.find("#EXT-X-KEY") != std::string::npos) {
      encrypted = true;
      std::string method = get_attribute_value(line, "METHOD");
      if (method == "AES-128") {
          aes_uri = get_string_attribute_value(line, "URI");
          if (aes_uri.find("http") == std::string::npos) {
              aes_uri = get_base_url() + aes_uri;
          }
          aes_iv = get_attribute_value(line, "IV");
      } else {
        xbmc->Log(ADDON::LOG_DEBUG, LOGTAG "Encryption method %s not supported", method.c_str());
      }
  } else if (line.find("#EXTINF") != std::string::npos) {
      in_segment = true;
      std::vector<std::string> attributes = get_attributes(line);
      Segment segment;
      segment.valid = true;
      segment.media_sequence = current_media_sequence++;
      segment.duration = std::stod(attributes[0]);
      if (!segments.empty()) {
        segment.time_in_playlist = (segments.back().time_in_playlist + segments.back().duration);
      }
      if (aes_iv.empty()) {
        segment.aes_iv = std::to_string(segment.media_sequence);
      } else {
        segment.aes_iv = aes_iv;
      }
      segment.aes_uri = aes_uri;
      segment.encrypted = encrypted;
      if (attributes.size() > 1) {
          segment.description = attributes[1];
      }
      segment.discontinuity = discontinuity;
      discontinuity = false;
      segments.push_back(segment);
  } else if (line.find("#EXT-X-ENDLIST") != std::string::npos) {
      live = false;
  } else if (line.find("#EXT-X-DISCONTINUITY") != std::string::npos) {
    discontinuity = true;
  }
  return true;
}

bool hls::MediaPlaylist::load_contents(std::string playlist_contents) {
  std::istringstream content_stream(playlist_contents);
  std::string line;
  while(std::getline(content_stream, line)) {
      if (!write_data(line)) {
          return false;
      }
  }
  return true;
}

hls::MediaPlaylist::MediaPlaylist()
: Playlist(),
  segment_target_duration(0),
  starting_media_sequence(0),
  current_media_sequence(0),
  in_segment(false),
  encrypted(false),
  live(true),
  bandwidth(0),
  valid(false),
  discontinuity(false)
{

}

hls::MediaPlaylist::~MediaPlaylist() {

}

std::string hls::Playlist::get_attribute_value(std::string line, std::string attribute_name) {
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

std::string hls::Playlist::get_string_attribute_value(std::string line, std::string attribute_name) {
  std::string attribute_value = get_attribute_value(line, attribute_name);
  attribute_value.erase(std::remove(attribute_value.begin(), attribute_value.end(), '\"' ), attribute_value.end());
  return attribute_value;
}

uint32_t hls::Playlist::get_number_attribute_value(std::string line, std::string attribute_name) {
  std::string attribute_value = get_attribute_value(line, attribute_name);
  if (attribute_value.length() == 0) {
    return 0;
  }
  return atoi(attribute_value.c_str());
}


std::vector<std::string> hls::Playlist::get_attributes(std::string line) {
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
    xbmc->Log(ADDON::LOG_ERROR, LOGTAG "Unable to open %s", file_path);
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
  valid = true;
  return true;
}
