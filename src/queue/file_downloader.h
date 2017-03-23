/*
 * file_downloader.h
 *
 *  Created on: Mar 22, 2017
 *      Author: anthony
 */

#pragma once

#include "downloader.h"

class FileDownloader : public Downloader {
  std::string download(std::string location);
};
