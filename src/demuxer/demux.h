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
#include "../hls/active_segment_controller.h"

#define AV_BUFFER_SIZE          131072

const int MAX_SEGMENT_DATA = 1;
const int MAX_DEMUX_PACKETS = 1000;

class Demux : public TSDemux::TSDemuxer
{
public:
  Demux(Downloader *downloader, hls::MediaPlaylist &media_playlist);
  ~Demux();

  const unsigned char* ReadAV(uint64_t pos, size_t n);

  void Process();

  INPUTSTREAM_IDS GetStreamIds();
  INPUTSTREAM_INFO* GetStreams();
  void Flush();
  void Abort();
  DemuxContainer Read();
  bool SeekTime(double time, bool backwards, double* startpts);

  int GetPlayingTime();

  void PushData(SegmentData content);

  double get_percentage_buffer_full() { return m_segment_data.size() / double(MAX_SEGMENT_DATA); };
  double get_percentage_packet_buffer_full() { return m_demuxPacketBuffer.size() / double(MAX_DEMUX_PACKETS); };
  hls::Segment get_current_segment() {
    if (!m_segment_data.empty()) {
      return m_segment_data.front().segment;
    }
    return hls::Segment();
  }
  void skip_to_pts(double pts);


private:
  uint16_t m_channel;
  std::vector<DemuxContainer> m_demuxPacketBuffer; // Needs to be locked
  P8PLATFORM::CMutex m_mutex;
  P8PLATFORM::CCondition<bool> m_cv;
  INPUTSTREAM_IDS m_streamIds;
  INPUTSTREAM_INFO m_streams[INPUTSTREAM_IDS::MAX_STREAM_COUNT];

  bool get_stream_data(TSDemux::STREAM_PKT* pkt);
  void reset_posmap();

  // PVR interfaces
  void populate_pvr_streams();
  bool update_pvr_stream(uint16_t pid);
  void push_stream_change();
  DemuxPacket* stream_pvr_data(TSDemux::STREAM_PKT* pkt);
  void push_stream_data(DemuxContainer dxp);

  // AV raw buffer
  size_t m_av_buf_size;         ///< size of av buffer
  uint64_t m_av_pos;            ///< absolute position in av
  unsigned char* m_av_buf;      ///< buffer
  unsigned char* m_av_rbs;      ///< raw data start in buffer
  unsigned char* m_av_rbe;      ///< raw data end in buffer

  // Playback context
  TSDemux::AVContext* m_AVContext;
  uint16_t m_mainStreamPID;     ///< PID of main stream
  uint64_t m_DTS;               ///< absolute decode time of main stream
  uint64_t m_PTS;               ///< absolute presentation time of main stream
  uint64_t m_dts;               ///< rebased DTS for the program chain
  uint64_t m_pts;               ///< rebased PTS for the program chain
  uint64_t m_startpts;          ///< start PTS for the program chain
  int64_t m_pinTime;            ///< pinned relative position (90Khz)
  int64_t m_curTime;            ///< current relative position (90Khz)
  int64_t m_endTime;            ///< last relative marked position (90Khz))
  typedef struct
  {
    uint64_t av_pts;
    uint64_t av_pos;
  } AV_POSMAP_ITEM;
  std::map<int64_t, AV_POSMAP_ITEM> m_posmap;

  bool m_isChangePlaced;
  std::set<uint16_t> m_nosetup;

  std::vector<SegmentData> m_segment_data; // Needs to be locked
  uint64_t m_segment_buffer_pos;
  bool m_segment_changed;

  hls::MediaPlaylist &m_playlist;
  ActiveSegmentController m_active_segment_controller;
};
