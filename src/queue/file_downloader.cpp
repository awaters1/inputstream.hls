/*
 * download_queue.cpp Copyright (C) 2017 Anthony Waters <awaters1@gmail.com>
 */

#include <fstream>
#include <sstream>

#include "file_downloader.h"

std::string FileDownloader::download(std::string location, uint32_t byte_offset, uint32_t byte_length) {
  std::ifstream file(location);
  std::ostringstream ostrm;

  ostrm << file.rdbuf();
  return std::string(ostrm.str());
}
