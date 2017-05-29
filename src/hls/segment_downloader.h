#pragma once
/*
 * segment_downloader.h Copyright (C) 2017 Anthony Waters <awaters1@gmail.com>
 */


class SegmentDownloader {
public:
  virtual void download_next_segment() = 0;
};
