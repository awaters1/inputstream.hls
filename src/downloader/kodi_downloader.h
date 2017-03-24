/*
 * file_downloader.h
 *
 *  Created on: Mar 22, 2017
 *      Author: anthony
 */

#pragma once

#include "downloader.h"

class KodiDownloader : public Downloader {
  std::string download(std::string location, uint32_t byte_offset, uint32_t byte_length);
};
