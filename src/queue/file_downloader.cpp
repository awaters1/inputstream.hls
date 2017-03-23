/*
 * download_queue.cpp Copyright (C) 2017 Anthony Waters <awaters1@gmail.com>
 */

#include <fstream>
#include <sstream>

#include "file_downloader.h"

std::string FileDownloader::download(std::string location) {
  std::ifstream file(location);
  std::ostringstream ostrm;

  ostrm << file.rdbuf();
  return std::string(ostrm.str());
}
