/*
* DASHTree.cpp
*****************************************************************************
* Copyright (C) 2015, liberty_developer
*
* Email: liberty.developer@xmail.net
*
* This source code and its use and distribution, is subject to the terms
* and conditions of the applicable license agreement.
*****************************************************************************/

#include <string>
#include <cstring>

#include "DASHTree.h"
#include "../oscompat.h"
#include "../helpers.h"

using namespace dash;

const char* TRANSLANG[370] = {
  "aa", "aar",
  "ab", "abk",
  "ae", "ave",
  "af", "afr",
  "ak", "aka",
  "am", "amh",
  "an", "arg",
  "ar", "ara",
  "as", "asm",
  "av", "ava",
  "ay", "aym",
  "az", "aze",
  "ba", "bak",
  "be", "bel",
  "bg", "bul",
  "bi", "bis",
  "bm", "bam",
  "bn", "ben",
  "bo", "bod",
  "br", "bre",
  "bs", "bos",
  "ca", "cat",
  "ce", "che",
  "ch", "cha",
  "co", "cos",
  "cr", "cre",
  "cs", "ces",
  "cu", "chu",
  "cv", "chv",
  "cy", "cym",
  "da", "dan",
  "de", "deu",
  "dv", "div",
  "dz", "dzo",
  "ee", "ewe",
  "el", "ell",
  "en", "eng",
  "eo", "epo",
  "es", "spa",
  "et", "est",
  "eu", "eus",
  "fa", "fas",
  "ff", "ful",
  "fi", "fin",
  "fj", "fij",
  "fo", "fao",
  "fr", "fra",
  "fy", "fry",
  "ga", "gle",
  "gd", "gla",
  "gl", "glg",
  "gn", "grn",
  "gu", "guj",
  "gv", "glv",
  "ha", "hau",
  "he", "heb",
  "hi", "hin",
  "ho", "hmo",
  "hr", "hrv",
  "ht", "hat",
  "hu", "hun",
  "hy", "hye",
  "hz", "her",
  "ia", "ina",
  "id", "ind",
  "ie", "ile",
  "ig", "ibo",
  "ii", "iii",
  "ik", "ipk",
  "io", "ido",
  "is", "isl",
  "it", "ita",
  "iu", "iku",
  "ja", "jpn",
  "jv", "jav",
  "ka", "kat",
  "kg", "kon",
  "ki", "kik",
  "kj", "kua",
  "kk", "kaz",
  "kl", "kal",
  "km", "khm",
  "kn", "kan",
  "ko", "kor",
  "kr", "kau",
  "ks", "kas",
  "ku", "kur",
  "kv", "kom",
  "kw", "cor",
  "ky", "kir",
  "la", "lat",
  "lb", "ltz",
  "lg", "lug",
  "li", "lim",
  "ln", "lin",
  "lo", "lao",
  "lt", "lit",
  "lu", "lub",
  "lv", "lav",
  "mg", "mlg",
  "mh", "mah",
  "mi", "mri",
  "mk", "mkd",
  "ml", "mal",
  "mn", "mon",
  "mr", "mar",
  "ms", "msa",
  "mt", "mlt",
  "my", "mya",
  "na", "nau",
  "nb", "nob",
  "nd", "nde",
  "ne", "nep",
  "ng", "ndo",
  "nl", "nld",
  "nn", "nno",
  "no", "nor",
  "nr", "nbl",
  "nv", "nav",
  "ny", "nya",
  "oc", "oci",
  "oj", "oji",
  "om", "orm",
  "or", "ori",
  "os", "oss",
  "pa", "pan",
  "pi", "pli",
  "pl", "pol",
  "ps", "pus",
  "pt", "por",
  "qu", "que",
  "rm", "roh",
  "rn", "run",
  "ro", "ron",
  "ru", "rus",
  "rw", "kin",
  "sa", "san",
  "sc", "srd",
  "sd", "snd",
  "se", "sme",
  "sg", "sag",
  "sh", "hbs",
  "si", "sin",
  "sk", "slk",
  "sl", "slv",
  "sm", "smo",
  "sn", "sna",
  "so", "som",
  "sq", "sqi",
  "sr", "srp",
  "ss", "ssw",
  "st", "sot",
  "su", "sun",
  "sv", "swe",
  "sw", "swa",
  "ta", "tam",
  "te", "tel",
  "tg", "tgk",
  "th", "tha",
  "ti", "tir",
  "tk", "tuk",
  "tl", "tgl",
  "tn", "tsn",
  "to", "ton",
  "tr", "tur",
  "ts", "tso",
  "tt", "tat",
  "tw", "twi",
  "ty", "tah",
  "ug", "uig",
  "uk", "ukr",
  "ur", "urd",
  "uz", "uzb",
  "ve", "ven",
  "vi", "vie",
  "vo", "vol",
  "wa", "wln",
  "wo", "wol",
  "xh", "xho",
  "yi", "yid",
  "yo", "yor",
  "za", "zha",
  "zh", "zho",
  "zu", "zul"
};

static const char* ltranslate(const char * in)
{
  //TODO: qfind or stop if >
  if (strlen(in) == 2)
  {
    for (unsigned int i(0); i < 185; ++i)
      if (strcmp(in, TRANSLANG[i * 2]) == 0)
        return TRANSLANG[i * 2 + 1];
  }
  else if (strlen(in) == 3)
    return in;
  return "";
}

DASHTree::DASHTree()
  :download_speed_(0.0)
  , parser_(0)
  , encryptionState_(ENCRYTIONSTATE_UNENCRYPTED)
  , current_period_(0)
{
}


DASHTree::~DASHTree()
{
}

static uint8_t GetChannels(const char **attr)
{
  const char *schemeIdUri(0), *value(0);

  for (; *attr;)
  {
    if (strcmp((const char*)*attr, "schemeIdUri") == 0)
      schemeIdUri = (const char*)*(attr + 1);
    else if (strcmp((const char*)*attr, "value") == 0)
      value = (const char*)*(attr + 1);
    attr += 2;
  }
  if (schemeIdUri && value)
  {
    if (strcmp(schemeIdUri, "urn:mpeg:dash:23003:3:audio_channel_configuration:2011") == 0)
      return atoi(value);
    else if (strcmp(schemeIdUri, "urn:dolby:dash:audio_channel_configuration:2011") == 0)
    {
      if (strcmp(value, "F801") == 0)
        return 6;
      else if (strcmp(value, "FE01") == 0)
        return 8;
    }
  }
  return 0;
}

/*----------------------------------------------------------------------
|   expat start
+---------------------------------------------------------------------*/
static void XMLCALL
start(void *data, const char *el, const char **attr)
{
  DASHTree *dash(reinterpret_cast<DASHTree*>(data));

  if (dash->currentNode_ & DASHTree::MPDNODE_MPD)
  {
    if (dash->currentNode_ & DASHTree::MPDNODE_PERIOD)
    {
      if (dash->currentNode_ & DASHTree::MPDNODE_ADAPTIONSET)
      {
        if (dash->currentNode_ & DASHTree::MPDNODE_REPRESENTATION)
        {
          if (dash->currentNode_ & DASHTree::MPDNODE_BASEURL)
          {
          }
          else if (dash->currentNode_ & DASHTree::MPDNODE_SEGMENTLIST)
          {
            DASHTree::Segment seg;
            if (strcmp(el, "SegmentURL") == 0)
            {
              for (; *attr;)
              {
                if (strcmp((const char*)*attr, "mediaRange") == 0)
                {
                  seg.SetRange((const char*)*(attr + 1));
                  break;
                }
                attr += 2;
              }
            }
            else if (strcmp(el, "Initialization") == 0)
            {
              for (; *attr;)
              {
                if (strcmp((const char*)*attr, "range") == 0)
                {
                  seg.SetRange((const char*)*(attr + 1));
                  break;
                }
                attr += 2;
              }
              dash->current_representation_->hasInitialization_ = true;
            }
            else
              return;
            dash->current_representation_->segments_.push_back(seg);
          }
          else if (strcmp(el, "AudioChannelConfiguration") == 0)
          {
            dash->current_representation_->channelCount_ = GetChannels(attr);
          }
          else if (strcmp(el, "BaseURL") == 0)
          {
            dash->strXMLText_.clear();
            dash->currentNode_ |= DASHTree::MPDNODE_BASEURL;
          }
          else if (strcmp(el, "SegmentList") == 0)
          {

            for (; *attr;)
            {
              if (strcmp((const char*)*attr, "duration") == 0)
                dash->current_representation_->duration_ = atoi((const char*)*(attr + 1));
              else if (strcmp((const char*)*attr, "timescale") == 0)
                dash->current_representation_->timescale_ = atoi((const char*)*(attr + 1));
              attr += 2;
            }
            if (dash->current_representation_->timescale_)
            {
              dash->current_representation_->segments_.reserve(dash->estimate_segcount(
                dash->current_representation_->duration_,
                dash->current_representation_->timescale_));
              dash->currentNode_ |= DASHTree::MPDNODE_SEGMENTLIST;
            }
          }

        }
        else if (dash->currentNode_ & DASHTree::MPDNODE_SEGMENTDURATIONS)
        {
          if (strcmp(el, "S") == 0 && *(const char*)*attr == 'd')
            dash->current_adaptationset_->segment_durations_.push_back(atoi((const char*)*(attr + 1)));
        }
        else if (dash->currentNode_ & DASHTree::MPDNODE_CONTENTPROTECTION)
        {
          if (strcmp(el, "cenc:pssh") == 0)
            dash->currentNode_ |= DASHTree::MPDNODE_PSSH;
        }
        else if (strcmp(el, "ContentComponent") == 0)
        {
          for (; *attr;)
          {
            if (strcmp((const char*)*attr, "contentType") == 0)
            {
              dash->current_adaptationset_->type_ =
                stricmp((const char*)*(attr + 1), "video") == 0 ? DASHTree::VIDEO
                : stricmp((const char*)*(attr + 1), "audio") == 0 ? DASHTree::AUDIO
                : DASHTree::NOTYPE;
              break;
            }
            attr += 2;
          }
        }
        else if (dash->currentNode_ & DASHTree::MPDNODE_BASEURL)
        {
        }
        else if (strcmp(el, "SegmentTemplate") == 0)
        {
          for (; *attr;)
          {
            if (strcmp((const char*)*attr, "timescale") == 0)
              dash->current_adaptationset_->segtpl_.timescale = atoi((const char*)*(attr + 1));
            else if (strcmp((const char*)*attr, "duration") == 0)
              dash->current_adaptationset_->segtpl_.duration = atoi((const char*)*(attr + 1));
            else if (strcmp((const char*)*attr, "media") == 0)
              dash->current_adaptationset_->segtpl_.media = (const char*)*(attr + 1);
            else if (strcmp((const char*)*attr, "startNumber") == 0)
              dash->current_adaptationset_->segtpl_.startNumber = atoi((const char*)*(attr + 1));
            else if (strcmp((const char*)*attr, "initialization") == 0)
              dash->current_adaptationset_->segtpl_.initialization = (const char*)*(attr + 1);
            attr += 2;
          }
          //We only support templates with id and number so far.......
          std::string &media(dash->current_adaptationset_->segtpl_.media);
          if (media.find("$RepresentationID$") == std::string::npos
            || media.find("$Number") == std::string::npos)
            media.clear();
          else
            media = dash->current_adaptationset_->base_url_ + media;
        }
        else if (strcmp(el, "Representation") == 0)
        {
          dash->current_representation_ = new DASHTree::Representation();
          dash->current_representation_->channelCount_ = dash->adpChannelCount_;
          dash->current_representation_->codecs_ = dash->current_adaptationset_->codecs_;
          dash->current_adaptationset_->repesentations_.push_back(dash->current_representation_);
          for (; *attr;)
          {
            if (strcmp((const char*)*attr, "bandwidth") == 0)
              dash->current_representation_->bandwidth_ = atoi((const char*)*(attr + 1));
            else if (strcmp((const char*)*attr, "codecs") == 0)
              dash->current_representation_->codecs_ = (const char*)*(attr + 1);
            else if (strcmp((const char*)*attr, "width") == 0)
              dash->current_representation_->width_ = static_cast<uint16_t>(atoi((const char*)*(attr + 1)));
            else if (strcmp((const char*)*attr, "height") == 0)
              dash->current_representation_->height_ = static_cast<uint16_t>(atoi((const char*)*(attr + 1)));
            else if (strcmp((const char*)*attr, "audioSamplingRate") == 0)
              dash->current_representation_->samplingRate_ = static_cast<uint32_t>(atoi((const char*)*(attr + 1)));
            else if (strcmp((const char*)*attr, "frameRate") == 0)
              sscanf((const char*)*(attr + 1), "%" SCNu32 "/%" SCNu32, &dash->current_representation_->fpsRate_, &dash->current_representation_->fpsScale_);
            else if (strcmp((const char*)*attr, "id") == 0)
              dash->current_representation_->id = (const char*)*(attr + 1);
            else if (strcmp((const char*)*attr, "codecPrivateData") == 0)
              dash->current_representation_->codec_private_data_ = annexb_to_avc((const char*)*(attr + 1));
            attr += 2;
          }
          dash->currentNode_ |= DASHTree::MPDNODE_REPRESENTATION;
        }
        else if (strcmp(el, "SegmentDurations") == 0)
        {
          dash->current_adaptationset_->segment_durations_.reserve(dash->segcount_);
          for (; *attr;)
          {
            if (strcmp((const char*)*attr, "timescale") == 0)
            {
              dash->current_adaptationset_->timescale_ = atoi((const char*)*(attr + 1));
              break;
            }
            attr += 2;
          }
          dash->currentNode_ |= DASHTree::MPDNODE_SEGMENTDURATIONS;
        }
        else if (strcmp(el, "ContentProtection") == 0)
        {
          dash->strXMLText_.clear();
          dash->encryptionState_ |= DASHTree::ENCRYTIONSTATE_ENCRYPTED;
          bool urnFound(false);
          for (; *attr;)
          {
            if (strcmp((const char*)*attr, "schemeIdUri") == 0)
            {
              urnFound = dash->adp_pssh_.first == (const char*)*(attr + 1);
              break;
            }
            attr += 2;
          }
          if (urnFound)
          {
            dash->currentNode_ |= DASHTree::MPDNODE_CONTENTPROTECTION;
            dash->encryptionState_ |= DASHTree::ENCRYTIONSTATE_SUPPORTED;
          }
        }
        else if (strcmp(el, "AudioChannelConfiguration") == 0)
        {
          dash->adpChannelCount_ = GetChannels(attr);
        }
        else if (strcmp(el, "BaseURL") == 0)
        {
          dash->strXMLText_.clear();
          dash->currentNode_ |= DASHTree::MPDNODE_BASEURL;
        }
      }
      else if (strcmp(el, "AdaptationSet") == 0)
      {
        //<AdaptationSet contentType="video" group="2" lang="en" mimeType="video/mp4" par="16:9" segmentAlignment="true" startWithSAP="1" subsegmentAlignment="true" subsegmentStartsWithSAP="1">
        dash->current_adaptationset_ = new DASHTree::AdaptationSet();
        dash->current_period_->adaptationSets_.push_back(dash->current_adaptationset_);
        dash->current_adaptationset_->base_url_ = dash->current_period_->base_url_;
        dash->adp_pssh_.second.clear();
        dash->adpChannelCount_ = 0;
        
        for (; *attr;)
        {
          if (strcmp((const char*)*attr, "contentType") == 0)
            dash->current_adaptationset_->type_ =
            stricmp((const char*)*(attr + 1), "video") == 0 ? DASHTree::VIDEO
            : stricmp((const char*)*(attr + 1), "audio") == 0 ? DASHTree::AUDIO
            : DASHTree::NOTYPE;
          else if (strcmp((const char*)*attr, "lang") == 0)
            dash->current_adaptationset_->language_ = ltranslate((const char*)*(attr + 1));
          else if (strcmp((const char*)*attr, "mimeType") == 0)
            dash->current_adaptationset_->mimeType_ = (const char*)*(attr + 1);
          else if (strcmp((const char*)*attr, "codecs") == 0)
            dash->current_adaptationset_->codecs_ = (const char*)*(attr + 1);
          attr += 2;
        }
        if (dash->current_adaptationset_->type_ == DASHTree::NOTYPE)
        {
          if (strncmp(dash->current_adaptationset_->mimeType_.c_str(), "video", 5) == 0)
            dash->current_adaptationset_->type_ = DASHTree::VIDEO;
          else if (strncmp(dash->current_adaptationset_->mimeType_.c_str(), "audio", 5) == 0)
            dash->current_adaptationset_->type_ = DASHTree::AUDIO;
        }
        dash->segcount_ = 0;
        dash->currentNode_ |= DASHTree::MPDNODE_ADAPTIONSET;
      }
    }
    else if (strcmp(el, "Period") == 0)
    {
      dash->current_period_ = new DASHTree::Period();
      dash->current_period_->base_url_ = dash->base_url_;
      dash->periods_.push_back(dash->current_period_);
      dash->currentNode_ |= DASHTree::MPDNODE_PERIOD;
    }
  }
  else if (strcmp(el, "MPD") == 0)
  {
    const char *mpt(0);

    dash->overallSeconds_ = 0;
    for (; *attr;)
    {
      if (strcmp((const char*)*attr, "mediaPresentationDuration") == 0)
      {
        mpt = (const char*)*(attr + 1);
        break;
      }
      attr += 2;
    }
    if (mpt && *mpt++ == 'P' && *mpt++ == 'T')
    {
      const char *next = strchr(mpt, 'H');
      if (next){
        dash->overallSeconds_ += atof(mpt)*3600;
        mpt = next + 1;
      }
      next = strchr(mpt, 'M');
      if (next){
        dash->overallSeconds_ += atof(mpt)*60;
        mpt = next + 1;
      }
      next = strchr(mpt, 'S');
      if (next)
        dash->overallSeconds_ += atof(mpt);
    }
    dash->currentNode_ |= DASHTree::MPDNODE_MPD;
  }
}

/*----------------------------------------------------------------------
|   expat text
+---------------------------------------------------------------------*/
static void XMLCALL
text(void *data, const char *s, int len)
{
  DASHTree *dash(reinterpret_cast<DASHTree*>(data));
  if (dash->currentNode_ & (DASHTree::MPDNODE_BASEURL | DASHTree::MPDNODE_PSSH))
    dash->strXMLText_ += std::string(s, len);
}

/*----------------------------------------------------------------------
|   expat end
+---------------------------------------------------------------------*/
static void XMLCALL
end(void *data, const char *el)
{
  DASHTree *dash(reinterpret_cast<DASHTree*>(data));

  if (dash->currentNode_ & DASHTree::MPDNODE_MPD)
  {
    if (dash->currentNode_ & DASHTree::MPDNODE_MPD)
    {
      if (dash->currentNode_ & DASHTree::MPDNODE_ADAPTIONSET)
      {
        if (dash->currentNode_ & DASHTree::MPDNODE_REPRESENTATION)
        {
          if (dash->currentNode_ & DASHTree::MPDNODE_BASEURL)
          {
            if (strcmp(el, "BaseURL") == 0)
            {
              dash->current_representation_->url_ = dash->current_adaptationset_->base_url_ + dash->strXMLText_;
              dash->currentNode_ &= ~DASHTree::MPDNODE_BASEURL;
            }
          }
          else if (dash->currentNode_ & DASHTree::MPDNODE_SEGMENTLIST)
          {
            if (strcmp(el, "SegmentList") == 0)
            {
              dash->currentNode_ &= ~DASHTree::MPDNODE_SEGMENTLIST;
              if (!dash->segcount_)
                dash->segcount_ = dash->current_representation_->segments_.size();
            }
          }
          else if (strcmp(el, "Representation") == 0)
          {
            dash->currentNode_ &= ~DASHTree::MPDNODE_REPRESENTATION;
            if (dash->current_representation_->segments_.empty())
            {
              if (!dash->current_adaptationset_->segtpl_.media.empty() && dash->overallSeconds_ > 0
                && dash->current_adaptationset_->segtpl_.timescale > 0 && dash->current_adaptationset_->segtpl_.duration > 0)
              {
                unsigned int countSegs = (unsigned int)(dash->overallSeconds_ / (((double)dash->current_adaptationset_->segtpl_.duration) / dash->current_adaptationset_->segtpl_.timescale)) + 1;
                if (countSegs < 65536)
                {
                  DASHTree::Segment seg;
                  seg.range_begin_ = ~0;

                  dash->current_representation_->segments_.reserve(countSegs + 1);
                  if (!dash->current_adaptationset_->segtpl_.initialization.empty())
                  {
                    seg.range_end_ = ~0;
                    dash->current_representation_->url_ = dash->current_adaptationset_->base_url_ + dash->current_adaptationset_->segtpl_.initialization;
                    
                    std::string::size_type repPos = dash->current_representation_->url_.find("$RepresentationID$");
                    if (repPos != std::string::npos)
                      dash->current_representation_->url_.replace(repPos, 18, dash->current_representation_->id);
                    
                    dash->current_representation_->segments_.push_back(seg);
                    dash->current_representation_->hasInitialization_ = true;
                  }
                  unsigned int numberEnd(dash->current_adaptationset_->segtpl_.startNumber + countSegs);
                  for (seg.range_end_ = dash->current_adaptationset_->segtpl_.startNumber; seg.range_end_ < numberEnd; ++seg.range_end_)
                    dash->current_representation_->segments_.push_back(seg);
                  return;
                }
              }
              delete dash->current_representation_;
              dash->current_adaptationset_->repesentations_.pop_back();
            }
          }
        }
        else if (dash->currentNode_ & DASHTree::MPDNODE_SEGMENTDURATIONS)
        {
          if (strcmp(el, "SegmentDurations") == 0)
            dash->currentNode_ &= ~DASHTree::MPDNODE_SEGMENTDURATIONS;
        }
        else if (dash->currentNode_ & DASHTree::MPDNODE_CONTENTPROTECTION)
        {
          if (dash->currentNode_ & DASHTree::MPDNODE_PSSH)
          {
            if (strcmp(el, "cenc:pssh") == 0)
            {
              dash->adp_pssh_.second = dash->strXMLText_;
              dash->currentNode_ &= ~DASHTree::MPDNODE_PSSH;
            }
          }
          else if (strcmp(el, "ContentProtection") == 0)
          {
            if (dash->adp_pssh_.second.empty())
              dash->adp_pssh_.second = "FILE";
            dash->currentNode_ &= ~DASHTree::MPDNODE_CONTENTPROTECTION;
          }
        }
        else if (dash->currentNode_ & DASHTree::MPDNODE_BASEURL)
        {
          if (strcmp(el, "BaseURL") == 0)
          {
            dash->current_adaptationset_->base_url_ = dash->base_url_ + dash->strXMLText_;
            dash->currentNode_ &= ~DASHTree::MPDNODE_BASEURL;
          }
        }
        else if (strcmp(el, "AdaptationSet") == 0)
        {
          dash->currentNode_ &= ~DASHTree::MPDNODE_ADAPTIONSET;
          if (dash->current_adaptationset_->type_ == DASHTree::NOTYPE
          || ((dash->encryptionState_ & DASHTree::ENCRYTIONSTATE_ENCRYPTED) && dash->adp_pssh_.second.empty())
          || (!dash->current_adaptationset_->language_.empty() && dash->current_adaptationset_->language_.size()!=3)
          || dash->current_adaptationset_->repesentations_.empty())
          {
            delete dash->current_adaptationset_;
            dash->current_period_->adaptationSets_.pop_back();
          }
          else
          {
            dash->pssh_ = dash->adp_pssh_;
            
            if (dash->current_adaptationset_->segment_durations_.empty()
              && !dash->current_adaptationset_->segtpl_.media.empty())
            {
              for (std::vector<DASHTree::Representation*>::iterator 
                b(dash->current_adaptationset_->repesentations_.begin()), 
                e(dash->current_adaptationset_->repesentations_.end()); b != e; ++b)
              {
                if (!(*b)->duration_ || !(*b)->timescale_)
                {
                  (*b)->duration_ = dash->current_adaptationset_->segtpl_.duration;
                  (*b)->timescale_ = dash->current_adaptationset_->segtpl_.timescale;
                }
              }
            }
          }
        }
      }
      else if (strcmp(el, "Period") == 0)
      {
        dash->currentNode_ &= ~DASHTree::MPDNODE_PERIOD;
      }
    }
    else if (strcmp(el, "MPD") == 0)
    {
      dash->currentNode_ &= ~DASHTree::MPDNODE_MPD;
    }
  }
}

/*----------------------------------------------------------------------
|   DASHTree
+---------------------------------------------------------------------*/

void DASHTree::Segment::SetRange(const char *range)
{
  const char *delim(strchr(range, '-'));
  if (delim)
  {
    range_begin_ = strtoull(range, 0, 10);
    range_end_ = strtoull(delim + 1, 0, 10);
  }
  else
    range_begin_ = range_end_ = 0;
}

bool DASHTree::open(const char *url)
{
  parser_ = XML_ParserCreate(NULL);
  if (!parser_)
    return false;
  XML_SetUserData(parser_, (void*)this);
  XML_SetElementHandler(parser_, start, end);
  XML_SetCharacterDataHandler(parser_, text);
  currentNode_ = 0;
  strXMLText_.clear();

  bool ret = download(url);
  
  XML_ParserFree(parser_);
  parser_ = 0;

  return ret;
}

bool DASHTree::write_data(void *buffer, size_t buffer_size)
{
  bool done(false);
  XML_Status retval = XML_Parse(parser_, (const char*)buffer, buffer_size, done);

  if (retval == XML_STATUS_ERROR)
  {
    unsigned int byteNumber = XML_GetErrorByteIndex(parser_);
    return false;
  }
  return true;
}

bool DASHTree::has_type(StreamType t)
{
  if (periods_.empty())
    return false;

  for (std::vector<AdaptationSet*>::const_iterator b(periods_[0]->adaptationSets_.begin()), e(periods_[0]->adaptationSets_.end()); b != e; ++b)
    if ((*b)->type_ == t)
      return true;
  return false;
}

uint32_t DASHTree::estimate_segcount(uint32_t duration, uint32_t timescale)
{
  double tmp(duration);
  duration /= timescale;
  return static_cast<uint32_t>((overallSeconds_ / duration)*1.01);
}
