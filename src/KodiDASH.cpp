/*
 * decoding.cpp Copyright (C) 2017 Anthony Waters <awaters1@gmail.com>
 */

#include "globals.h"
#include "Ap4.h"
#include "KodiDASH.h"

/*******************************************************
Kodi Streams implementation
********************************************************/

bool KodiDASHTree::download(const char* url)
{
  // open the file
  void* file = xbmc->CURLCreate(url);
  if (!file)
    return false;
  xbmc->CURLAddOption(file, XFILE::CURL_OPTION_PROTOCOL, "seekable", "0");
  xbmc->CURLAddOption(file, XFILE::CURL_OPTION_PROTOCOL, "acceptencoding", "gzip");
  xbmc->CURLOpen(file, XFILE::READ_CHUNKED | XFILE::READ_NO_CACHE);

  // read the file
  static const unsigned int CHUNKSIZE = 16384;
  char buf[CHUNKSIZE];
  bool read;
  while ((read = xbmc->ReadFileString(file, buf, CHUNKSIZE)) > 0 && read && write_data(buf)) {
          xbmc->Log(ADDON::LOG_DEBUG, "Read Line %s", buf);
  }

  //download_speed_ = xbmc->GetFileDownloadSpeed(file);

  xbmc->CloseFile(file);

  xbmc->Log(ADDON::LOG_DEBUG, "Download %s finished", url);

  return !read;
}

bool KodiDASHStream::download(const char* url, const char* rangeHeader)
{
  // open the file
  void* file = xbmc->CURLCreate(url);
  if (!file)
    return false;
  xbmc->CURLAddOption(file, XFILE::CURL_OPTION_PROTOCOL, "seekable" , "0");
  if (rangeHeader)
    xbmc->CURLAddOption(file, XFILE::CURL_OPTION_HEADER, "Range", rangeHeader);
  xbmc->CURLAddOption(file, XFILE::CURL_OPTION_HEADER, "Connection", "keep-alive");
  xbmc->CURLAddOption(file, XFILE::CURL_OPTION_PROTOCOL, "acceptencoding", "gzip, deflate");

  xbmc->CURLOpen(file, XFILE::READ_CHUNKED | XFILE::READ_NO_CACHE | XFILE::READ_AUDIO_VIDEO);

  // read the file
  char *buf = (char*)malloc(1024*1024);
  size_t nbRead, nbReadOverall = 0;
  while ((nbRead = xbmc->ReadFile(file, buf, 1024 * 1024)) > 0 && ~nbRead && write_data(buf, nbRead)) nbReadOverall+= nbRead;
  free(buf);

  if (!nbReadOverall)
  {
    xbmc->Log(ADDON::LOG_ERROR, "Download %s doesn't provide any data: invalid", url);
    return false;
  }

  double current_download_speed_ = xbmc->GetFileDownloadSpeed(file);
  //Calculate the new downloadspeed to 1MB
  static const size_t ref_packet = 1024 * 1024;
  if (nbReadOverall >= ref_packet)
    set_download_speed(current_download_speed_);
  else
  {
    double ratio = (double)nbReadOverall / ref_packet;
    set_download_speed((get_download_speed() * (1.0 - ratio)) + current_download_speed_*ratio);
  }

  xbmc->CloseFile(file);

  xbmc->Log(ADDON::LOG_DEBUG, "Download %s finished, average download speed: %0.4lf", url, get_download_speed());

  return nbRead == 0;
}

bool KodiDASHStream::parseIndexRange()
{
  // open the file
  xbmc->Log(ADDON::LOG_DEBUG, "Downloading %s for SIDX generation", getRepresentation()->url_.c_str());

  void* file = xbmc->CURLCreate(getRepresentation()->url_.c_str());
  if (!file)
    return false;
  xbmc->CURLAddOption(file, XFILE::CURL_OPTION_PROTOCOL, "seekable", "0");
  char rangebuf[64];
  sprintf(rangebuf, "bytes=%u-%u", getRepresentation()->indexRangeMin_, getRepresentation()->indexRangeMax_);
  xbmc->CURLAddOption(file, XFILE::CURL_OPTION_HEADER, "Range", rangebuf);
  if (!xbmc->CURLOpen(file, XFILE::READ_CHUNKED | XFILE::READ_NO_CACHE | XFILE::READ_AUDIO_VIDEO))
  {
    xbmc->Log(ADDON::LOG_ERROR, "Download SIDX retrieval failed");
    return false;
  }

  // read the file into AP4_MemoryByteStream
  AP4_MemoryByteStream byteStream;

  char buf[16384];
  size_t nbRead, nbReadOverall = 0;
  while ((nbRead = xbmc->ReadFile(file, buf, 16384)) > 0 && ~nbRead && AP4_SUCCEEDED(byteStream.Write(buf, nbRead))) nbReadOverall += nbRead;
  xbmc->CloseFile(file);

  if (nbReadOverall != getRepresentation()->indexRangeMax_ - getRepresentation()->indexRangeMin_ +1)
  {
    xbmc->Log(ADDON::LOG_ERROR, "Size of downloaded SIDX section differs from expected");
    return false;
  }
  byteStream.Seek(0);

  dash::DASHTree::Representation *rep(const_cast<dash::DASHTree::Representation*>(getRepresentation()));
  dash::DASHTree::AdaptationSet *adp(const_cast<dash::DASHTree::AdaptationSet*>(getAdaptationSet()));

  if (!getRepresentation()->indexRangeMin_)
  {
    AP4_File f(byteStream, AP4_DefaultAtomFactory::Instance, true);
    AP4_Movie* movie = f.GetMovie();
    if (movie == NULL)
    {
      xbmc->Log(ADDON::LOG_ERROR, "No MOOV in stream!");
      return false;
    }
    rep->flags_ |= dash::DASHTree::Representation::INITIALIZATION;
    rep->initialization_.range_begin_ = 0;
    AP4_Position pos;
    byteStream.Tell(pos);
    rep->initialization_.range_end_ = pos - 1;
  }

  dash::DASHTree::Segment seg;
  seg.startPTS_ = 0;
  unsigned int numSIDX(1);

  do
  {
    AP4_Atom *atom(NULL);
    if (AP4_FAILED(AP4_DefaultAtomFactory::Instance.CreateAtomFromStream(byteStream, atom)))
    {
      xbmc->Log(ADDON::LOG_ERROR, "Unable to create SIDX from IndexRange bytes");
      return false;
    }

    if (atom->GetType() == AP4_ATOM_TYPE_MOOF)
    {
      delete atom;
      break;
    }
    else if (atom->GetType() != AP4_ATOM_TYPE_SIDX)
    {
      delete atom;
      continue;
    }

    AP4_SidxAtom *sidx(AP4_DYNAMIC_CAST(AP4_SidxAtom, atom));
    const AP4_Array<AP4_SidxAtom::Reference> &refs(sidx->GetReferences());
    if (refs[0].m_ReferenceType == 1)
    {
      numSIDX = refs.ItemCount();
      delete atom;
      continue;
    }
    AP4_Position pos;
    byteStream.Tell(pos);
    seg.range_end_ = pos + getRepresentation()->indexRangeMin_ + sidx->GetFirstOffset() - 1;
    rep->timescale_ = sidx->GetTimeScale();

    for (unsigned int i(0); i < refs.ItemCount(); ++i)
    {
      seg.range_begin_ = seg.range_end_ + 1;
      seg.range_end_ = seg.range_begin_ + refs[i].m_ReferencedSize - 1;
      rep->segments_.data.push_back(seg);
      if (adp->segment_durations_.data.size() < rep->segments_.data.size() - 1)
        adp->segment_durations_.data.push_back(refs[i].m_SubsegmentDuration);
      seg.startPTS_ += refs[i].m_SubsegmentDuration;
    }
    delete atom;
    --numSIDX;
  } while (numSIDX);
  return true;
}
