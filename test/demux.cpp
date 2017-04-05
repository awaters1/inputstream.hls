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

#include "../src/demuxer/demux.h"

Demux::Demux(Downloader *downloader, hls::MediaPlaylist &media_playlist)
  : m_channel(1)
  , m_av_buf_size(AV_BUFFER_SIZE)
  , m_av_pos(0)
  , m_av_buf(NULL)
  , m_av_rbs(NULL)
  , m_av_rbe(NULL)
  , m_AVContext(NULL)
  , m_mainStreamPID(0xffff)
  , m_DTS(PTS_UNSET)
  , m_PTS(PTS_UNSET)
  , m_dts(PTS_UNSET)
  , m_pts(PTS_UNSET)
  , m_startpts(0)
  , m_pinTime(0)
  , m_curTime(0)
  , m_endTime(0)
  , m_isChangePlaced(false)
  , m_playlist(media_playlist)
  , m_active_segment_controller(downloader, media_playlist)
{

}

Demux::~Demux()
{

}

/*
 * Implement our AV reader
 */
const unsigned char* Demux::ReadAV(uint64_t pos, size_t n)
{
  return nullptr;
}

void Demux::Process()
{
}

INPUTSTREAM_IDS Demux::GetStreamIds()
{
}

INPUTSTREAM_INFO* Demux::GetStreams()
{
}

void Demux::Flush(void)
{
}

void Demux::Abort()
{
  Flush();
  m_streamIds.m_streamCount = 0;
}

DemuxContainer Demux::Read()
{
  return DemuxContainer();
}

bool Demux::SeekTime(double time, bool backwards, double* startpts)
{
  return true;
}

int Demux::GetPlayingTime()
{
  return 0;
}

bool Demux::get_stream_data(TSDemux::STREAM_PKT* pkt)
{

  return true;
}

void Demux::reset_posmap()
{

}

static inline int stream_identifier(int composition_id, int ancillary_id)
{
  return (composition_id & 0xffff) | ((ancillary_id & 0xffff) << 16);
}

static void recode_language(const char* muxLanguage, char* strLanguage)
{

}

void Demux::populate_pvr_streams()
{

}

bool Demux::update_pvr_stream(uint16_t pid)
{

  return false;
}

void Demux::push_stream_change()
{

}

DemuxPacket* Demux::stream_pvr_data(TSDemux::STREAM_PKT* pkt)
{
  return nullptr;
}

void Demux::push_stream_data(DemuxContainer dxp)
{

}

void Demux::PushData(SegmentData content) {

}

void Demux::skip_to_pts(double pts) {

}
