/*
 * SegmentReader.cpp Copyright (C) 2017 Anthony Waters <awaters1@gmail.com>
 */

#include <cstring>

#include "../globals.h"
#include "SegmentReader.h"

#define LOGTAG                  "[SegmentReader] "

SegmentReader::SegmentReader(hls::Segment segment, double time_in_playlist) :
segment(segment),
time_in_playlist(time_in_playlist),
can_overwrite(false),
finished(false) {

}

SegmentReader::~SegmentReader() {

}

hls::Segment SegmentReader::get_segment() {
  return segment;
}

bool SegmentReader::get_can_overwrite() {
  std::lock_guard<std::mutex> lock(data_mutex);
  return can_overwrite;
}

bool SegmentReader::is_finished() {
  std::lock_guard<std::mutex> lock(data_mutex);
  return finished;
}

void SegmentReader::write_data(std::string data) {
  std::lock_guard<std::mutex> lock(data_mutex);
  contents += data;
  can_overwrite = false;
  data_cv.notify_all();
}

void SegmentReader::end_data() {
  std::lock_guard<std::mutex> lock(data_mutex);
  finished = true;
  data_cv.notify_all();
}

void SegmentReader::read(uint64_t pos, size_t &size, uint8_t * const destination, size_t min_read) {
  std::chrono::steady_clock::time_point t1 = std::chrono::steady_clock::now();
  size_t desired_size = size;
  size_t data_read = 0;
  read_impl(pos, size, destination);
  if (size >= min_read) {
    return;
  }
  {
    std::lock_guard<std::mutex> lock(data_mutex);
    if (size < min_read && finished) {
      return;
    }
  }
  data_read += size;
  // Size == 0
  while(data_read < min_read && !can_overwrite) {
    std::unique_lock<std::mutex> lock(data_mutex);
    data_cv.wait_for(lock, std::chrono::milliseconds(100));
    lock.unlock();
    size = desired_size - data_read;
    read_impl(pos + data_read, size, destination + data_read);
    data_read += size;
    std::chrono::steady_clock::time_point t2 = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>( t2 - t1 ).count();
    if (duration >= READ_TIMEOUT_MS) {
        xbmc->Log(ADDON::LOG_ERROR, LOGTAG "%s Read timeout", __FUNCTION__);
        break;
    }
  }
  size = data_read;
  if (!segment.valid) {
      xbmc->Log(ADDON::LOG_DEBUG, LOGTAG "%s segment is invalid", __FUNCTION__);
  }
}

void SegmentReader::read_impl(uint64_t pos, size_t &size, uint8_t * const destination) {
  std::lock_guard<std::mutex> lock(data_mutex);
  if (pos > contents.length()) {
    size = 0;
    return;
  }
  if (pos + size > contents.length()) {
    size = contents.length() - pos;
  }
  std::memcpy(destination, contents.c_str() + pos, size);
  can_overwrite = finished && (pos + size) >= contents.length();
  if (can_overwrite) {
    xbmc->Log(ADDON::LOG_DEBUG, LOGTAG "%s Read all data pos %d size %d length %d", __FUNCTION__,
        pos, size, contents.length());
  }
}

