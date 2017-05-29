#pragma once
/*
 *      Copyright (C) 2013-2016 Team Kodi
 *      http://www.kodi.tv
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 51 Franklin Street, Fifth Floor, Boston,
 *  MA 02110-1301 USA
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#include "tsDemuxer.h"

#include <p8-platform/threads/threads.h>
#include <p8-platform/threads/mutex.h>
#include <p8-platform/util/buffer.h>

#include <future>
#include <thread>

#include <map>
#include <set>

#include "kodi_inputstream_types.h"
#include "../demux_container.h"
#include "../hls/segment_data.h"
#include "../ring_buffer.h"
#include "../segment_storage.h"

#define AV_BUFFER_SIZE          131072

const int MAX_DEMUX_PACKETS = 100;

class Demux : public TSDemux::TSDemuxer
{
public:
  Demux(SegmentStorage *segment_storage);
  ~Demux();

  INPUTSTREAM_IDS GetStreamIds();
  INPUTSTREAM_INFO* GetStreams();
  void Flush();
  void Abort();
  DemuxContainer Read();

  double get_percentage_packet_buffer_full() { return writePacketBuffer.size() / double(MAX_DEMUX_PACKETS); };
  int32_t get_current_media_sequence();
private:
  const unsigned char* ReadAV(uint64_t pos, size_t n);
  bool Process();
private:
  uint16_t m_channel;
  std::deque<DemuxContainer> writePacketBuffer; // Needs to be locked
  std::deque<DemuxContainer> readPacketBuffer; // Only read in Read()
  std::mutex demux_mutex;
  INPUTSTREAM_IDS m_streamIds;
  INPUTSTREAM_INFO m_streams[INPUTSTREAM_IDS::MAX_STREAM_COUNT];

  bool get_stream_data(TSDemux::STREAM_PKT* pkt);
  void reset_posmap();

  // PVR interfaces
  bool processed_discontinuity;
  std::mutex initial_setup_mutex;
  std::condition_variable initial_setup_cv;
  std::atomic_bool awaiting_initial_setup;
  void populate_pvr_streams();
  bool update_pvr_stream(uint16_t pid);
  void push_stream_change();
  DemuxPacket* stream_pvr_data(TSDemux::STREAM_PKT* pkt);
  void push_stream_data(DemuxContainer dxp);
  void process_demux_thread();
  bool should_process_demux();

  // AV raw buffer
  size_t m_av_buf_size;         ///< size of av buffer
  uint64_t m_av_pos;            ///< absolute position in av
  unsigned char* m_av_buf;      ///< buffer
  unsigned char* m_av_rbs;      ///< raw data start in buffer
  unsigned char* m_av_rbe;      ///< raw data end in buffer

  // Playback context
  TSDemux::AVContext* m_AVContext;
  uint16_t m_mainStreamPID;     ///< PID of main stream
  int64_t m_segmentReadTime;    ///< current relative position based on segments (DVD_TIME_BASE)
  int64_t m_readTime;           ///< current relative position based on packets read (DVD_TIME_BASE)
  typedef struct
  {
    uint64_t av_pts;
    uint64_t av_pos;
  } AV_POSMAP_ITEM;
  std::map<int64_t, AV_POSMAP_ITEM> m_posmap;
  std::set<uint16_t> m_nosetup;

  SegmentStorage *m_av_contents;
  hls::Segment current_segment;
  bool m_isStreamDone;
  bool m_segmentChanged;
  bool include_discontinuity;

  std::condition_variable read_demux_cv;
  std::condition_variable demux_cv;
  std::thread demux_thread;
  std::atomic_bool quit_processing;
};
