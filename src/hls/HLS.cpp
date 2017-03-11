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
  std::cout << "Index: " << index << " Comma: " << comma_index << "\n";
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
  if (line.find_first_of("#EXT-X-STREAM-INF") == 0) {
      in_stream = true;
      MasterStream stream = MasterStream();
      stream.program_id = get_attribute_value(line, "PROGRAM-ID");
      stream.bandwidth = get_number_attribute_value(line, "BANDWIDTH");
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
