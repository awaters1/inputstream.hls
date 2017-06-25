/*
 * SegmentReader.cpp Copyright (C) 2017 Anthony Waters <awaters1@gmail.com>
 */

#include "SegmentReader.h"

SegmentReader::SegmentReader(hls::Segment segment, double time_in_playlist) :
segment(segment),
time_in_playlist(time_in_playlist),
can_overwrite(false),
finished(false) {

}

SegmentReader::~SegmentReader() {

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
  contents += data;
  can_overwrite = false;
  // TODO: Notify read_data method
  data_cv.notify_all();
}

void SegmentReader::end_data() {
  finished = true;
  // TODO: Notify read_data method
  data_cv.notify_all();
}

hls::Segment SegmentStorage::read(uint64_t pos, size_t &size, uint8_t * const destination, size_t min_read) {
  std::chrono::steady_clock::time_point t1 = std::chrono::steady_clock::now();
  size_t desired_size = size;
  size_t data_read = 0;
  hls::Segment segment = read_impl(pos, size, destination);
  if (size >= min_read) {
    return segment;
  }
  {
    std::lock_guard<std::mutex> lock(data_lock);
    if (size < min_read && no_more_data) {
      return segment;
    }
  }
  data_read += size;
  // Size == 0
  while(data_read < min_read) {
    // xbmc->Log(ADDON::LOG_DEBUG, LOGTAG "%s Waiting for data to be downloaded, pos %d size %d bytes_read %d", __FUNCTION__,pos, size, bytes_read);
    download_cv.notify_all();
    std::unique_lock<std::mutex> lock(data_lock);
    data_cv.wait_for(lock, std::chrono::milliseconds(100));
    if (quit_processing || no_more_data) {
      return segment;
    }
    lock.unlock();
    size = desired_size - data_read;
    segment = read_impl(pos + data_read, size, destination + data_read);
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
  return segment;
}

hls::Segment SegmentStorage::read_impl(uint64_t pos, size_t &size, uint8_t * const destination) {
  std::lock_guard<std::mutex> lock(data_lock);
  uint64_t destination_offset = 0;
  uint32_t current_read_segment_index = read_segment_data_index;
  uint64_t next_offset = offset;
  hls::Segment first_segment;
  uint64_t data_read = 0;
  uint64_t wanted_data = size;
  while(size > 0) {
    uint64_t relative_offset;
    if (pos >= next_offset) {
      relative_offset = pos - next_offset;
    } else {
      relative_offset = 0; // start at beginning of segment
    }
    std::lock_guard<std::mutex> segment_lock(segment_locks.at(current_read_segment_index));
    SegmentData &current_segment = segment_data.at(current_read_segment_index);
    if (!current_segment.segment.valid) {
      break;
    }
    bool go_to_next_segment = false;
    if (relative_offset < current_segment.contents.length()) {
      if (!first_segment.valid) {
        first_segment = current_segment.segment;
      }
      size_t data_left_in_segment = current_segment.contents.length() - relative_offset;
      size_t data_to_read_from_segment;
      if (data_left_in_segment < size) {
        data_to_read_from_segment = data_left_in_segment;
        next_offset += current_segment.contents.length();
      } else {
        data_to_read_from_segment = size;
      }
      std::memcpy(destination + destination_offset,
          current_segment.contents.c_str() + relative_offset, data_to_read_from_segment);
      destination_offset += data_to_read_from_segment;
      size -= data_to_read_from_segment;
      data_read += data_to_read_from_segment;
    } else if (current_segment.finished) {
      // We read all of the data in this segment so it is safe to overwrite
      if (!current_segment.can_overwrite) {
        // data_lock is locked up top
        xbmc->Log(ADDON::LOG_DEBUG, LOGTAG "%s Triggering download", __FUNCTION__);
        current_segment.can_overwrite = true;
        download_cv.notify_all();
      }
      next_offset += current_segment.contents.length();
    } else {
      // The segment we are reading from isn't finished so we cannot read anymore
      break;
    }
    current_read_segment_index = (current_read_segment_index + 1)% MAX_SEGMENTS;
    if (current_read_segment_index == read_segment_data_index) {
      // We looped around
      break;
    }
  }
  if (data_read < wanted_data) {
//    xbmc->Log(ADDON::LOG_DEBUG, LOGTAG "%s Not enough data read pos: %d, data read %d data wanted %d", __FUNCTION__, pos, data_read, wanted_data);
  } else {
//    xbmc->Log(ADDON::LOG_DEBUG, LOGTAG "%s Read %d bytes at %d", __FUNCTION__, data_read, pos);
  }
  size = data_read;
  if (!first_segment.valid) {
      xbmc->Log(ADDON::LOG_DEBUG, LOGTAG "%s First segment is invalid", __FUNCTION__);
  }
  return first_segment;
}

