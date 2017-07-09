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

Demux::Demux()
  : m_channel(1)
  , m_av_buf_size(AV_BUFFER_SIZE)
  , m_av_pos(0)
  , m_av_buf(NULL)
  , m_av_rbs(NULL)
  , m_av_rbe(NULL)
  , m_AVContext(NULL)
  , m_mainStreamPID(0xffff)
  , m_readTime(0)
  , include_discontinuity(false)
  , segment_reader(nullptr)
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
  }
  else
  {
    xbmc->Log(LOG_ERROR, LOGTAG "alloc AV buffer failed");
  }
}

Demux::~Demux()
{
  xbmc->Log(ADDON::LOG_DEBUG, LOGTAG "%s Deconstruct demux", __FUNCTION__);

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

void Demux::set_segment_reader(std::shared_ptr<SegmentReader> segment_reader) {
  m_AVContext->GoPosition(0);
  m_av_pos = 0;
  hls::Segment segment = segment_reader->get_segment();
  m_readTime = segment_reader->get_time_in_playlist() * DVD_TIME_BASE;
  if (segment.discontinuity) {
    include_discontinuity = true;
    xbmc->Log(LOG_DEBUG, LOGTAG "%s Segment discontinuity", __FUNCTION__);

    xbmc->Log(LOG_DEBUG, LOGTAG "%s: processing discontinuity", __FUNCTION__);
    xbmc->Log(LOG_DEBUG, LOGTAG "%s: resetting AV context", __FUNCTION__);
    m_AVContext->StreamDiscontinuity();
    m_AVContext->Reset();
    m_AVContext->ResetPackets();
  }
  xbmc->Log(LOG_DEBUG, LOGTAG "%s Pos: %d Current Segment: %d Time: %f", __FUNCTION__, m_av_pos,
                segment.media_sequence, segment_reader->get_time_in_playlist());
  this->segment_reader = std::move(segment_reader);
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
  // xbmc->Log(LOG_DEBUG, LOGTAG "%s Attempting to read pos %d size %d", __FUNCTION__, pos, n);
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

  reader_status = segment_reader->read(m_av_pos + dataread, len, m_av_rbe, n);
  // xbmc->Log(LOG_DEBUG, LOGTAG "%s Read at %d for %d bytes", __FUNCTION__, pos, len);


  m_av_rbe += len;
  dataread += len;

  if (dataread < n) {
    xbmc->Log(LOG_DEBUG, LOGTAG "%s Didn't read enough data, read %d", __FUNCTION__, dataread);
  }

  return dataread >= n ? m_av_rbs : NULL;
}

DemuxStatus Demux::Process(std::vector<DemuxContainer> &demux_packets)
{
  xbmc->Log(LOG_DEBUG, LOGTAG "%s: Processing demux", __FUNCTION__);
  if (!m_AVContext)
  {
    xbmc->Log(LOG_ERROR, LOGTAG "%s: no AVContext", __FUNCTION__);
    return DemuxStatus::ERROR;
  }

  bool return_stream_setup = false;
  bool added_stream_change = false;
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
          // TODO: Tweak stream change messages because at the beginning
          // of the stream
          // We cannot wait to push the stream change because our data packets will get in for one stream
          // and start playing while the other stream is attempting setup, so we updated the streams
          // and then when they are done updating we return from process to notify the caller
          if (m_nosetup.empty()) {
            demux_packets.push_back(get_stream_change());
            added_stream_change = true;
          }
          update_pvr_stream(pkt.pid);
        }
        DemuxPacket* dxp = stream_pvr_data(&pkt);
        DemuxContainer demux_container;
        demux_container.demux_packet = dxp;
        update_timing_data(demux_container);
        demux_packets.push_back(demux_container);
        if (include_discontinuity) {
          include_discontinuity = false;
        }
      }
    }
    if (m_AVContext->HasPIDPayload())
    {
      ret = m_AVContext->ProcessTSPayload();
      if (ret == TSDemux::AVCONTEXT_PROGRAM_CHANGE)
      {
        xbmc->Log(LOG_DEBUG, LOGTAG "%s: processing stream change", __FUNCTION__);
        populate_pvr_streams();
        demux_packets.push_back(get_stream_change());
        added_stream_change = true;
      }
    }

    if (ret < 0)
      xbmc->Log(LOG_NOTICE, LOGTAG "%s: error %d", __FUNCTION__, ret);

    if (ret == TSDemux::AVCONTEXT_TS_ERROR)
      m_AVContext->Shift();
    else
      m_AVContext->GoNext();

    if (demux_packets.size() >= DEMUX_BUFFER_SIZE) {
      break;
    }

  }
  // TODO: Need to be able to detect end of segmend and an actually error
  xbmc->Log(LOG_DEBUG, LOGTAG "%s: stopped with status %d", __FUNCTION__, ret);
  if (reader_status == SegmentReaderStatus::FLUSHED) {
    return DemuxStatus::FLUSH;
  } else if (demux_packets.size() >= DEMUX_BUFFER_SIZE) {
    return DemuxStatus::FILLED_BUFFER;
  } else if (reader_status == SegmentReaderStatus::ENDED) {
    return DemuxStatus::SEGMENT_DONE;
  }
  return ret >= 0 ? DemuxStatus::SEGMENT_DONE : DemuxStatus::ERROR;
}

void Demux::Abort()
{
  m_streamIds.m_streamCount = 0;
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
  if (demux_container.demux_packet->iStreamId == m_mainStreamPID) {
    m_readTime += demux_container.demux_packet->duration;
  }
  demux_container.discontinuity = include_discontinuity;
  demux_container.time_in_playlist = segment_reader->get_time_in_playlist();
}

DemuxContainer Demux::get_stream_change()
{
  DemuxPacket* dxp  = ipsh->AllocateDemuxPacket(0);
  dxp->iStreamId    = DMX_SPECIALID_STREAMCHANGE;

  DemuxContainer demux_container;
  demux_container.demux_packet = dxp;
  demux_container.stream_ids = m_streamIds;
  memcpy(demux_container.streams, m_streams,
         sizeof(INPUTSTREAM_INFO) * INPUTSTREAM_IDS::MAX_STREAM_COUNT);
  update_timing_data(demux_container);

  xbmc->Log(LOG_DEBUG, LOGTAG "%s: pushed stream change", __FUNCTION__);
  return demux_container;
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
