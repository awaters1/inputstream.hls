/*
 * download_queue.h Copyright (C) 2017 Anthony Waters <awaters1@gmail.com>
 */

#pragma once

#include <string>

class Downloader {
public:
  virtual std::string download(std::string location) = 0;
};
