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
#include <iostream>
#include <algorithm>
#include <chrono>

#include <p8-platform/os.h>
#include <xbmc_codec_types.h>

#include "demux.h"
#include "debug.h"

#include <climits>

#include "../globals.h"

#define LOGTAG                  "[DEMUX] "
#define POSMAP_PTS_INTERVAL     (PTS_TIME_BASE * 2)       // 2 secs
#define READAV_TIMEOUT          10000                     // 10 secs
const double BUFFER_LOWER_BOUND = 0.50;

using namespace ADDON;
using namespace P8PLATFORM;

void DemuxLog(int level, char *msg)
{
  if (msg && level != DEMUX_DBG_NONE)
  {
    bool doLog = g_bExtraDebug;
    addon_log_t loglevel = LOG_DEBUG;
    switch (level)
    {
    case DEMUX_DBG_ERROR:
      loglevel = LOG_ERROR;
      doLog = true;
      break;
    case DEMUX_DBG_WARN:
    case DEMUX_DBG_INFO:
      loglevel = LOG_INFO;
      break;
    case DEMUX_DBG_DEBUG:
    case DEMUX_DBG_PARSE:
    case DEMUX_DBG_ALL:
      loglevel = LOG_DEBUG;
      break;
    }
    if (xbmc && doLog)
      xbmc->Log(loglevel, LOGTAG "%s", msg);
  }
}

Demux::Demux(SegmentStorage *segment_storage)
  : m_channel(1)
  , m_av_buf_size(AV_BUFFER_SIZE)
  , m_av_pos(0)
  , m_av_buf(NULL)
  , m_av_rbs(NULL)
  , m_av_rbe(NULL)
  , m_AVContext(NULL)
  , m_mainStreamPID(0xffff)
  , m_isStreamDone(false)
  , m_segmentChanged(false)
  , m_readTime(-1)
  , m_segmentReadTime(-1)
  , quit_processing(false)
  , processed_discontinuity(true)
  , awaiting_initial_setup(false)
  , include_discontinuity(false)
  , m_av_contents(segment_storage)
{
  xbmc->Log(ADDON::LOG_DEBUG, LOGTAG "%s Starting demux", __FUNCTION__);
  memset(&m_streams, 0, sizeof(INPUTSTREAM_IDS));
  m_av_buf = (unsigned char*)malloc(sizeof(*m_av_buf) * (m_av_buf_size + 1));
  if (m_av_buf)
  {
    m_av_rbs = m_av_buf;
    m_av_rbe = m_av_buf;

    if (g_bExtraDebug)
      TSDemux::DBGLevel(DEMUX_DBG_DEBUG);
    else
      TSDemux::DBGLevel(DEMUX_DBG_ERROR);
    TSDemux::SetDBGMsgCallback(DemuxLog);

    m_AVContext = new TSDemux::AVContext(this, m_av_pos, m_channel);

    demux_thread = std::thread(&Demux::process_demux_thread, this);
    demux_cv.notify_one();
  }
  else
  {
    xbmc->Log(LOG_ERROR, LOGTAG "alloc AV buffer failed");
  }
}

Demux::~Demux()
{
  xbmc->Log(ADDON::LOG_DEBUG, LOGTAG "%s Deconstruct demux", __FUNCTION__);
  {
    std::lock_guard<std::mutex> lock1(demux_mutex);
    std::lock_guard<std::mutex> lock2(initial_setup_mutex);
    quit_processing = true;
  }
  initial_setup_cv.notify_all();
  demux_cv.notify_all();
  demux_thread.join();

  Abort();

  // Free AV context
  if (m_AVContext)
    SAFE_DELETE(m_AVContext);
  // Free AV buffer
  if (m_av_buf)
  {
    if (g_bExtraDebug)
      xbmc->Log(LOG_DEBUG, LOGTAG "free AV buffer: allocated size was %zu", m_av_buf_size);
    free(m_av_buf);
    m_av_buf = NULL;
  }
}

/*
 * Implement our AV reader
 */
const unsigned char* Demux::ReadAV(uint64_t pos, size_t n)
{
  // out of range
  if (n > m_av_buf_size)
    return NULL;

  // Already read ?
  size_t sz = m_av_rbe - m_av_buf;
  if (pos < m_av_pos || pos > (m_av_pos + sz))
  {
    // seek and reset buffer
    m_av_rbs = m_av_rbe = m_av_buf;
  }
  else
  {
    // move to the desired pos in buffer
    m_av_rbs = m_av_buf + (size_t)(pos - m_av_pos);
  }

  size_t dataread = m_av_rbe - m_av_rbs;
  if (dataread >= n)
    return m_av_rbs;
  dataread = 0;
//  xbmc->Log(LOG_DEBUG, LOGTAG "%s: Starting read at %d", __FUNCTION__, pos);
  // flush old data to free up space at the end
  memmove(m_av_buf, m_av_rbs, dataread);
  m_av_rbs = m_av_buf;
  m_av_rbe = m_av_rbs + dataread;
  m_av_pos = pos - dataread;
  // fill new data
  size_t len = (size_t)(m_av_buf_size - dataread);
  // xbmc->Log(LOG_DEBUG, LOGTAG "%s Going to read at %d for %d bytes, dataread: %d len %d", __FUNCTION__, pos, n, dataread, len);

  hls::Segment segment_read = m_av_contents->read(m_av_pos + dataread, len, m_av_rbe, n);
  // xbmc->Log(LOG_DEBUG, LOGTAG "%s Read at %d for %d bytes", __FUNCTION__, pos, len);
  if (!(segment_read == current_segment) && len > 0) {
    m_segmentChanged = true;
    if (m_segmentReadTime == -1) {
        m_segmentReadTime = segment_read.time_in_playlist * DVD_TIME_BASE;
        xbmc->Log(LOG_DEBUG, LOGTAG "%s Setting segment read time: %d", __FUNCTION__, m_segmentReadTime);
    }
    m_readTime = m_segmentReadTime;
    current_segment = segment_read;
    if (current_segment.valid) {
      m_segmentReadTime += (current_segment.duration * DVD_TIME_BASE);
    }
    if (current_segment.discontinuity) {
      processed_discontinuity = false;
      include_discontinuity = true;
      xbmc->Log(LOG_DEBUG, LOGTAG "%s Segment discontinuity", __FUNCTION__);

      if (!processed_discontinuity) {
        xbmc->Log(LOG_DEBUG, LOGTAG "%s: processing discontinuity", __FUNCTION__);
        awaiting_initial_setup = true;
        xbmc->Log(LOG_DEBUG, LOGTAG "%s: resetting AV context", __FUNCTION__);
        m_AVContext->StreamDiscontinuity();
        m_AVContext->Reset();
        m_AVContext->ResetPackets();
        processed_discontinuity = true;
      }
    }
    xbmc->Log(LOG_DEBUG, LOGTAG "%s Pos: %d Current Segment: %d", __FUNCTION__, m_av_pos,
                  current_segment.media_sequence);
  }
  if (len == 0) {
    m_isStreamDone = true;
  }

  m_av_rbe += len;
  dataread += len;

  if (dataread < n) {
    xbmc->Log(LOG_DEBUG, LOGTAG "%s Didn't read enough data, read %d", __FUNCTION__, dataread);
  }

  return dataread >= n ? m_av_rbs : NULL;
}

bool Demux::Process()
{
  xbmc->Log(LOG_DEBUG, LOGTAG "%s: Processing demux", __FUNCTION__);
  if (!m_AVContext)
  {
    xbmc->Log(LOG_ERROR, LOGTAG "%s: no AVContext", __FUNCTION__);
    return false;
  }

  int ret = 0;

  while (true)
  {
    ret = m_AVContext->TSResync();
    if (ret != TSDemux::AVCONTEXT_CONTINUE)
      break;

    ret = m_AVContext->ProcessTSPacket();

    if (m_AVContext->HasPIDStreamData())
    {
      TSDemux::STREAM_PKT pkt;
      while (get_stream_data(&pkt))
      {
        if (pkt.streamChange)
        {
          // We cannot wait to push the stream change because our data packets will get in for one stream
          // and start playing while the other stream is attempting setup
          update_pvr_stream(pkt.pid);
          if (awaiting_initial_setup) {
            if (m_nosetup.empty()) {
              std::lock_guard<std::mutex> lock(initial_setup_mutex);
              awaiting_initial_setup = false;
            }
            initial_setup_cv.notify_all();
          } else {
            push_stream_change();
          }
        }
        DemuxPacket* dxp = stream_pvr_data(&pkt);
        DemuxContainer demux_container;
        demux_container.demux_packet = dxp;
        demux_container.pcr = pkt.pcr;
        update_timing_data(demux_container);
        if (m_segmentChanged) {
          m_segmentChanged = false;
          include_discontinuity = false;
        }
        push_stream_data(demux_container);
      }
    }
    if (m_AVContext->HasPIDPayload())
    {
      ret = m_AVContext->ProcessTSPayload();
      if (ret == TSDemux::AVCONTEXT_PROGRAM_CHANGE)
      {
        xbmc->Log(LOG_DEBUG, LOGTAG "%s: processing stream change", __FUNCTION__);
        awaiting_initial_setup = true;
        populate_pvr_streams();
        push_stream_change();
      }
    }

    if (ret < 0)
      xbmc->Log(LOG_NOTICE, LOGTAG "%s: error %d", __FUNCTION__, ret);

    if (ret == TSDemux::AVCONTEXT_TS_ERROR)
      m_AVContext->Shift();
    else
      m_AVContext->GoNext();

    {
      std::lock_guard<std::mutex> lock(demux_mutex);
      if (writePacketBuffer.size() >= MAX_DEMUX_PACKETS) {
        ret = 0;
        break;
      }
      if (quit_processing) {
        break;
      }
    }
  }
  xbmc->Log(LOG_DEBUG, LOGTAG "%s: stopped with status %d", __FUNCTION__, ret);
  if (ret < 0) {
    {
      std::lock_guard<std::mutex> lock(demux_mutex);
      quit_processing = true;
    }
    read_demux_cv.notify_all();
  }
  return ret >= 0 ? true : false;
}

INPUTSTREAM_IDS Demux::GetStreamIds()
{
  while(!quit_processing && awaiting_initial_setup) {
    xbmc->Log(LOG_NOTICE, LOGTAG "%s: incomplete setup for streamids", __FUNCTION__);
    std::unique_lock<std::mutex> lock(initial_setup_mutex);
    initial_setup_cv.wait(lock, [this] {
      return !awaiting_initial_setup || quit_processing;
    });
  }

  std::lock_guard<std::mutex> lock(demux_mutex);
  return m_streamIds;
}

INPUTSTREAM_INFO* Demux::GetStreams()
{
  std::lock_guard<std::mutex> lock(demux_mutex);
  return m_streams;
}

void Demux::Flush(void)
{
  std::lock_guard<std::mutex> lock(demux_mutex);
  for(auto it = writePacketBuffer.begin(); it != writePacketBuffer.end(); ++it) {
    ipsh->FreeDemuxPacket(it->demux_packet);
  }
  writePacketBuffer.clear();
}

void Demux::Abort()
{
  Flush();
  std::lock_guard<std::mutex> lock(demux_mutex);
  m_streamIds.m_streamCount = 0;
}

DemuxContainer Demux::Read(bool remove_packet)
{
  // TODO: Need to do something better to prevent stutters
  std::chrono::high_resolution_clock::time_point t1 = std::chrono::high_resolution_clock::now();
  if (readPacketBuffer.empty()) {
    std::unique_lock<std::mutex> lock(demux_mutex);
    read_demux_cv.wait_for(lock, std::chrono::milliseconds(10), [&] {
      return quit_processing || writePacketBuffer.size() == MAX_DEMUX_PACKETS;
    });
    readPacketBuffer.swap(writePacketBuffer);
//      xbmc->Log(LOG_NOTICE, LOGTAG "%s: Loaded %d packets", __FUNCTION__, readPacketBuffer.size());
  }
  if (readPacketBuffer.size() / (double) MAX_DEMUX_PACKETS < 0.5) {
    demux_cv.notify_all();
  }
  if (readPacketBuffer.empty()) {
    if (quit_processing) {
      xbmc->Log(LOG_NOTICE, LOGTAG "%s: Quit read", __FUNCTION__);
      return DemuxContainer();
    } else {
      xbmc->Log(LOG_NOTICE, LOGTAG "%s: Returning empty packet", __FUNCTION__);
      DemuxContainer container;
      container.demux_packet = ipsh->AllocateDemuxPacket(0);
      return container;
    }
  }
  DemuxContainer packet = readPacketBuffer.front();
  if (remove_packet) {
      readPacketBuffer.pop_front();
  }
  std::chrono::high_resolution_clock::time_point t2 = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::microseconds>( t2 - t1 ).count();

  if (duration > 10000) {
    // xbmc->Log(LOG_NOTICE, LOGTAG "%s: Read Duration %d, packets %d", __FUNCTION__, duration, readPacketBuffer.size());
  }
  return packet;
}

uint32_t Demux::get_current_media_sequence() {
  return Read(false).segment.media_sequence;
}

bool Demux::get_stream_data(TSDemux::STREAM_PKT* pkt)
{
  TSDemux::ElementaryStream* es = m_AVContext->GetPIDStream();
  if (!es)
    return false;

  if (!es->GetStreamPacket(pkt))
    return false;

  pkt->pcr = es->c_pcr;



  if (pkt->duration > PTS_TIME_BASE * 2)
  {
    pkt->duration = 0;
  }
  return true;
}

static void recode_language(const char* muxLanguage, char* strLanguage)
{
  /*
   * While XBMC does'nt support them.
   * Fix unsupported language codes (EN 300 468 Annex F & J)
   * 'qaa'        : Original audio
   * 'qad','NAR'  : Audio Description
   */
  if (strncmp(muxLanguage, "qaa", 3) == 0 ||
      strncmp(muxLanguage, "qad", 3) == 0 ||
      strncmp(muxLanguage, "NAR", 3) == 0)
  {
    strLanguage[0] = 0;
    strLanguage[1] = 0;
    strLanguage[2] = 0;
    strLanguage[3] = 0;
  }
  else
  {
    strLanguage[0] = muxLanguage[0];
    strLanguage[1] = muxLanguage[1];
    strLanguage[2] = muxLanguage[2];
    strLanguage[3] = 0;
  }
}

void Demux::populate_pvr_streams()
{
  std::lock_guard<std::mutex> lock(demux_mutex);

  uint16_t mainPid = 0xffff;
  int mainType = XBMC_CODEC_TYPE_UNKNOWN;
  const std::vector<TSDemux::ElementaryStream*> es_streams = m_AVContext->GetStreams();
  int count = 0;
  for (std::vector<TSDemux::ElementaryStream*>::const_iterator it = es_streams.begin(); it != es_streams.end(); ++it)
  {
    const char* codec_name = (*it)->GetStreamCodecName();
    xbmc_codec_t codec = CODEC->GetCodecByName(codec_name);
    if (codec.codec_type != XBMC_CODEC_TYPE_UNKNOWN)
    {
      // Find the main stream:
      // The best candidate would be the first video. Else the first audio
      switch (mainType)
      {
      case XBMC_CODEC_TYPE_VIDEO:
        break;
      case XBMC_CODEC_TYPE_AUDIO:
        if (codec.codec_type != XBMC_CODEC_TYPE_VIDEO)
          break;
      default:
        mainPid = (*it)->pid;
        mainType = codec.codec_type;
      }

      m_streams[count].m_pID           = (*it)->pid;
      memcpy(m_streams[count].m_codecName, codec_name, 32);
      switch(codec.codec_type) {
      case XBMC_CODEC_TYPE_VIDEO:
              m_streams[count].m_streamType = INPUTSTREAM_INFO::STREAM_TYPE::TYPE_VIDEO; break;
      case XBMC_CODEC_TYPE_AUDIO:
              m_streams[count].m_streamType = INPUTSTREAM_INFO::STREAM_TYPE::TYPE_AUDIO; break;
      case XBMC_CODEC_TYPE_SUBTITLE:
              m_streams[count].m_streamType = INPUTSTREAM_INFO::STREAM_TYPE::TYPE_SUBTITLE; break;
      case XBMC_CODEC_TYPE_DATA:
              m_streams[count].m_streamType = INPUTSTREAM_INFO::STREAM_TYPE::TYPE_TELETEXT; break;
      default:
              m_streams[count].m_streamType = INPUTSTREAM_INFO::STREAM_TYPE::TYPE_NONE; break;
      }
      recode_language((*it)->stream_info.language, m_streams[count].m_language);
      // m_streams[count].iSubtitleInfo  = stream_identifier((*it)->stream_info.composition_id, (*it)->stream_info.ancillary_id);
      m_streams[count].m_FpsScale      = (*it)->stream_info.fps_scale;
      m_streams[count].m_FpsRate       = (*it)->stream_info.fps_rate;
      m_streams[count].m_Height        = (*it)->stream_info.height;
      m_streams[count].m_Width         = (*it)->stream_info.width;
      m_streams[count].m_Aspect        = (*it)->stream_info.aspect;
      m_streams[count].m_Channels      = (*it)->stream_info.channels;
      m_streams[count].m_SampleRate    = (*it)->stream_info.sample_rate;
      m_streams[count].m_BlockAlign    = (*it)->stream_info.block_align;
      m_streams[count].m_BitRate       = (*it)->stream_info.bit_rate;
      m_streams[count].m_BitsPerSample = (*it)->stream_info.bits_per_sample;
      m_streams[count].m_Bandwidth = 0;
      m_streams[count].m_ExtraSize = 0;
      m_streams[count].m_ExtraData = nullptr;

      m_streamIds.m_streamIds[count] = (*it)->pid;

      count++;
      m_AVContext->StartStreaming((*it)->pid);

      // Add stream to no setup set
      if (!(*it)->has_stream_info)
        m_nosetup.insert((*it)->pid);

      if (g_bExtraDebug)
        xbmc->Log(LOG_DEBUG, LOGTAG "%s: register PES %.4x %s", __FUNCTION__, (*it)->pid, codec_name);
    }
  }
  m_streamIds.m_streamCount = count;
  // Renew main stream
  m_mainStreamPID = mainPid;
}

bool Demux::update_pvr_stream(uint16_t pid)
{
  TSDemux::ElementaryStream* es = m_AVContext->GetStream(pid);
  if (!es)
    return false;

  const char* codec_name = es->GetStreamCodecName();
  xbmc_codec_t codec = CODEC->GetCodecByName(codec_name);
  if (g_bExtraDebug)
    xbmc->Log(LOG_DEBUG, LOGTAG "%s: update info PES %.4x %s", __FUNCTION__, es->pid, codec_name);

  std::lock_guard<std::mutex> lock(demux_mutex);

  // find stream index for pid
  for (unsigned i = 0; i < m_streamIds.m_streamCount; i++)
  {
    if (m_streams[i].m_pID == es->pid)
    {
      memcpy(m_streams[i].m_codecName, codec_name, 32);
      switch(codec.codec_type) {
      case XBMC_CODEC_TYPE_VIDEO:
              m_streams[i].m_streamType = INPUTSTREAM_INFO::STREAM_TYPE::TYPE_VIDEO; break;
      case XBMC_CODEC_TYPE_AUDIO:
              m_streams[i].m_streamType = INPUTSTREAM_INFO::STREAM_TYPE::TYPE_AUDIO; break;
      case XBMC_CODEC_TYPE_SUBTITLE:
              m_streams[i].m_streamType = INPUTSTREAM_INFO::STREAM_TYPE::TYPE_SUBTITLE; break;
      case XBMC_CODEC_TYPE_DATA:
              m_streams[i].m_streamType = INPUTSTREAM_INFO::STREAM_TYPE::TYPE_TELETEXT; break;
      default:
              m_streams[i].m_streamType = INPUTSTREAM_INFO::STREAM_TYPE::TYPE_NONE; break;
      }
      recode_language(es->stream_info.language, m_streams[i].m_language);
      // m_streams[i].m_Subtitle  = stream_identifier(es->stream_info.composition_id, es->stream_info.ancillary_id);
      m_streams[i].m_FpsScale      = es->stream_info.fps_scale;
      m_streams[i].m_FpsRate       = es->stream_info.fps_rate;
      m_streams[i].m_Height        = es->stream_info.height;
      m_streams[i].m_Width         = es->stream_info.width;
      m_streams[i].m_Aspect        = es->stream_info.aspect;
      m_streams[i].m_Channels      = es->stream_info.channels;
      m_streams[i].m_SampleRate    = es->stream_info.sample_rate;
      m_streams[i].m_BlockAlign    = es->stream_info.block_align;
      m_streams[i].m_BitRate       = es->stream_info.bit_rate;
      m_streams[i].m_BitsPerSample = es->stream_info.bits_per_sample;

      if (es->has_stream_info)
      {
        // Now stream is setup. Remove it from no setup set
        std::set<uint16_t>::iterator it = m_nosetup.find(es->pid);
        if (it != m_nosetup.end())
        {
          m_nosetup.erase(it);
          if (m_nosetup.empty())
            xbmc->Log(LOG_DEBUG, LOGTAG "%s: setup is completed", __FUNCTION__);
        }
      }
      return true;
    }
  }
  return false;
}

void Demux::update_timing_data(DemuxContainer &demux_container) {
  double current_time_ms = (double)m_readTime / 1000.0;
  if (current_time_ms > INT_MAX)
    current_time_ms = INT_MAX;
  demux_container.current_time = (int) current_time_ms;
  demux_container.segment = current_segment;
  if (demux_container.demux_packet->iStreamId == m_mainStreamPID) {
    m_readTime += demux_container.demux_packet->duration;
  }
  demux_container.segment_changed = m_segmentChanged;
  demux_container.segment = current_segment;
  demux_container.discontinuity = include_discontinuity;
}

void Demux::push_stream_change()
{
  DemuxPacket* dxp  = ipsh->AllocateDemuxPacket(0);
  dxp->iStreamId    = DMX_SPECIALID_STREAMCHANGE;

  DemuxContainer demux_container;
  demux_container.demux_packet = dxp;
  update_timing_data(demux_container);

  push_stream_data(demux_container);
  xbmc->Log(LOG_DEBUG, LOGTAG "%s: pushed stream change", __FUNCTION__);
}

DemuxPacket* Demux::stream_pvr_data(TSDemux::STREAM_PKT* pkt)
{
  if (!pkt)
    return NULL;

  DemuxPacket* dxp = ipsh->AllocateDemuxPacket(pkt->size);
  if (dxp)
  {
    if (pkt->size > 0 && pkt->data)
      memcpy(dxp->pData, pkt->data, pkt->size);

    dxp->iStreamId = (int)(pkt->pid);
    dxp->iSize = pkt->size;
    dxp->duration = (double)pkt->duration * DVD_TIME_BASE / PTS_TIME_BASE;
    if (pkt->dts != PTS_UNSET)
      dxp->dts = (double)pkt->dts * DVD_TIME_BASE / PTS_TIME_BASE;
    else
      dxp->dts = DVD_NOPTS_VALUE;
    if (pkt->pts != PTS_UNSET)
      dxp->pts = (double)pkt->pts * DVD_TIME_BASE / PTS_TIME_BASE;
    else
      dxp->pts = DVD_NOPTS_VALUE;
  }
  return dxp;
}

void Demux::push_stream_data(DemuxContainer dxp) {
  std::lock_guard<std::mutex> lock(demux_mutex);
  writePacketBuffer.push_back(dxp);
  read_demux_cv.notify_all();
}

void Demux::process_demux_thread() {
  xbmc->Log(ADDON::LOG_DEBUG, LOGTAG "%s Starting demuxer thread", __FUNCTION__);
  while(true) {
   std::unique_lock<std::mutex> lock(demux_mutex);
   demux_cv.wait(lock, [this] {
     return quit_processing || (writePacketBuffer.size() / (double) MAX_DEMUX_PACKETS) < 0.5;
   });

   if (quit_processing) {
     break;
   }
   lock.unlock();

   Process();
  }
  xbmc->Log(ADDON::LOG_DEBUG, LOGTAG "Exiting demux thread");
}

