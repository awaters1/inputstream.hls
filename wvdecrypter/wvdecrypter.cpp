/*
*      Copyright (C) 2016 liberty-developer
*      https://github.com/liberty-developer
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
*  <http://www.gnu.org/licenses/>.
*
*/

#include "cdm/media/cdm/cdm_adapter.h"
#include "../src/helpers.h"
#include "../src/SSD_dll.h"
#include "Ap4.h"
#include <stdarg.h>

#ifndef WIDEVINECDMFILENAME
#error  "WIDEVINECDMFILENAME must be set"
#endif


SSD_HOST *host = 0;

static void Log(SSD_HOST::LOGLEVEL loglevel, const char *format, ...)
{
  char buffer[16384];
  va_list args;
  va_start(args, format);
  vsprintf(buffer, format, args);
  va_end(args);
  return host->Log(loglevel, buffer);
}

/*******************************************************
CDM
********************************************************/

/*----------------------------------------------------------------------
|   CdmDecryptedBlock implementation
+---------------------------------------------------------------------*/

class CdmDecryptedBlock : public cdm::DecryptedBlock {
public:
  CdmDecryptedBlock() :buffer_(0), timestamp_(0) {};
  virtual ~CdmDecryptedBlock() {};

  virtual void SetDecryptedBuffer(cdm::Buffer* buffer) override { buffer_ = buffer; };
  virtual cdm::Buffer* DecryptedBuffer() override { return buffer_; };

  virtual void SetTimestamp(int64_t timestamp) override { timestamp_ = timestamp; };
  virtual int64_t Timestamp() const override { return timestamp_; };
private:
  cdm::Buffer *buffer_;
  int64_t timestamp_;
};

/*----------------------------------------------------------------------
|   CdmDecryptedBlock implementation
+---------------------------------------------------------------------*/
class CdmBuffer : public cdm::Buffer {
public:
  CdmBuffer(AP4_DataBuffer *buffer) :buffer_(buffer) {};
  virtual ~CdmBuffer() {};

  virtual void Destroy() override {};

  virtual uint32_t Capacity() const override
  {
    return buffer_->GetBufferSize();
  };
  virtual uint8_t* Data() override
  {
    return (uint8_t*)buffer_->GetData();
  };
  virtual void SetSize(uint32_t size) override
  {
    buffer_->SetDataSize(size);
  };
  virtual uint32_t Size() const override
  {
    return buffer_->GetDataSize();
  };
private:
  AP4_DataBuffer *buffer_;
};

/*----------------------------------------------------------------------
|   WV_CencSingleSampleDecrypter
+---------------------------------------------------------------------*/
class WV_CencSingleSampleDecrypter : public AP4_CencSingleSampleDecrypter
{
public:
  // methods
  WV_CencSingleSampleDecrypter(std::string licenseURL, const uint8_t *pssh, size_t pssh_size);

  bool initialized()const { return wv_adapter != 0; };

  virtual AP4_Result DecryptSampleData(AP4_DataBuffer& data_in,
    AP4_DataBuffer& data_out,

    // always 16 bytes
    const AP4_UI08* iv,

    // pass 0 for full decryption
    unsigned int    subsample_count,

    // array of <subsample_count> integers. NULL if subsample_count is 0
    const AP4_UI16* bytes_of_cleartext_data,

    // array of <subsample_count> integers. NULL if subsample_count is 0
    const AP4_UI32* bytes_of_encrypted_data);

private:
  media::CdmAdapter *wv_adapter;
  unsigned int max_subsample_count_;
  cdm::SubsampleEntry *subsample_buffer_;
};

/*----------------------------------------------------------------------
|   WV_CencSingleSampleDecrypter::WV_CencSingleSampleDecrypter
+---------------------------------------------------------------------*/

WV_CencSingleSampleDecrypter::WV_CencSingleSampleDecrypter(std::string licenseURL, const uint8_t *pssh, size_t pssh_size)
  : AP4_CencSingleSampleDecrypter(0)
  , wv_adapter(0)
  , max_subsample_count_(0)
  , subsample_buffer_(0)
{
  uint8_t buf[1024];

  if (pssh_size > 256)
  {
    Log(SSD_HOST::LL_ERROR, "Init_data with length: %u seems not to be cenc init data!", pssh_size);
    return;
  }

  std::string strPath = host->GetDecrypterPath();
  if (strPath.empty())
  {
    Log(SSD_HOST::LL_ERROR, "Absolute path to widevine in settings expected");
    return;
  }
  strPath += WIDEVINECDMFILENAME;

  wv_adapter = new media::CdmAdapter("com.widevine.alpha", strPath.c_str(), media::CdmConfig());
  unsigned int buf_size = 32 + pssh_size;

  if (!wv_adapter->valid())
  {
    Log(SSD_HOST::LL_ERROR, "Unable to load widevine shared library (%s)", strPath.c_str());
    goto FAILURE;
  }

  // This will request a new session and initializes session_id and message members in cdm_adapter.
  // message will be used to create a license request in the step after CreateSession call.
  // Initialization data is the widevine cdm pssh code in google proto style found in mpd schemeIdUri
  static uint8_t proto[] = { 0x00, 0x00, 0x00, 0x63, 0x70, 0x73, 0x73, 0x68, 0x00, 0x00, 0x00, 0x00, 0xed, 0xef, 0x8b, 0xa9,
    0x79, 0xd6, 0x4a, 0xce, 0xa3, 0xc8, 0x27, 0xdc, 0xd5, 0x1d, 0x21, 0xed, 0x00, 0x00, 0x00, 0x00 };

  proto[3] = static_cast<uint8_t>(buf_size);
  proto[31] = static_cast<uint8_t>(pssh_size);

  memcpy(buf, proto, sizeof(proto));
  memcpy(&buf[32], pssh, pssh_size);

  wv_adapter->CreateSessionAndGenerateRequest(0, cdm::SessionType::kTemporary, cdm::InitDataType::kCenc, buf, buf_size);

  if (wv_adapter->SessionValid())
  {
    std::string license, challenge(b64_encode(wv_adapter->GetMessage(), wv_adapter->GetMessageSize(), true));
    challenge = "widevine2Challenge=" + challenge;
    challenge += "&includeHdcpTestKeyInLicense=true";

    // open the file
    void* file = host->CURLCreate(licenseURL.c_str());
    host->CURLAddOption(file, SSD_HOST::OPTION_PROTOCOL, "acceptencoding", "gzip");
    host->CURLAddOption(file, SSD_HOST::OPTION_PROTOCOL, "seekable", "0");
    host->CURLAddOption(file, SSD_HOST::OPTION_HEADER, "Content-Type", "application/x-www-form-urlencoded");

    size_t nbRead;
    std::string::size_type licStartPos, licEndPos;

    if (!host->CURLOpen(file, SSD_HOST::FILE_POST))
    {
      Log(SSD_HOST::LL_ERROR, "Failed to open CURL file");
      goto FAILURE;
    }

    if (size_t sz = host->WriteFile(file, challenge.c_str(), challenge.size()) != 200)
    {
      Log(SSD_HOST::LL_ERROR, "License server returned failure (%d)", static_cast<int>(sz));
      host->CloseFile(file);
      goto FAILURE;
    }

    // read the file
    while ((nbRead = host->ReadFile(file, buf, 1024)) > 0)
      license += std::string((const char*)buf,nbRead);

    host->CloseFile(file);

    if (nbRead != 0)
    {
      Log(SSD_HOST::LL_ERROR, "Could not read full license response");
      goto FAILURE;
    }

    licStartPos = license.find("\"license\":\"");
    if (licStartPos == std::string::npos)
    {
      Log(SSD_HOST::LL_ERROR, "License start position not found");
      goto FAILURE;
    }
    licStartPos += 11;
    licEndPos = license.find("\",", licStartPos);
    if (licEndPos == std::string::npos)
    {
      Log(SSD_HOST::LL_ERROR, "License end position not found");
      goto FAILURE;
    }

    buf_size = 1024;
    b64_decode(license.c_str() + licStartPos, licEndPos - licStartPos, buf, buf_size);
    wv_adapter->UpdateSession(buf, buf_size);

    if (!wv_adapter->KeyIdValid())
    {
      Log(SSD_HOST::LL_ERROR, "License update not successful");
      goto FAILURE;
    }
    // forbit auto delete for this object
    SetParentIsOwner(false);
    return;
  }
FAILURE:
  delete wv_adapter;
  wv_adapter = 0;
}

/*----------------------------------------------------------------------
|   WV_CencSingleSampleDecrypter::DecryptSampleData
+---------------------------------------------------------------------*/
AP4_Result WV_CencSingleSampleDecrypter::DecryptSampleData(
  AP4_DataBuffer& data_in,
  AP4_DataBuffer& data_out,
  const AP4_UI08* iv,
  unsigned int    subsample_count,
  const AP4_UI16* bytes_of_cleartext_data,
  const AP4_UI32* bytes_of_encrypted_data)
{
  // the output has the same size as the input
  data_out.SetDataSize(data_in.GetDataSize());

  if (!wv_adapter)
  {
    data_out.SetData(data_in.GetData(), data_in.GetDataSize());
    return AP4_SUCCESS;
  }

  // check input parameters
  if (iv == NULL) return AP4_ERROR_INVALID_PARAMETERS;
  if (subsample_count) {
    if (bytes_of_cleartext_data == NULL || bytes_of_encrypted_data == NULL) {
      return AP4_ERROR_INVALID_PARAMETERS;
    }
  }

  // transform ap4 format into cmd format
  cdm::InputBuffer cdm_in;
  if (subsample_count > max_subsample_count_)
  {
    subsample_buffer_ = (cdm::SubsampleEntry*)realloc(subsample_buffer_, subsample_count*sizeof(cdm::SubsampleEntry));
    max_subsample_count_ = subsample_count;
  }
  for (cdm::SubsampleEntry *b(subsample_buffer_), *e(subsample_buffer_ + subsample_count); b != e; ++b, ++bytes_of_cleartext_data, ++bytes_of_encrypted_data)
  {
    b->clear_bytes = *bytes_of_cleartext_data;
    b->cipher_bytes = *bytes_of_encrypted_data;
  }
  cdm_in.data = data_in.GetData();
  cdm_in.data_size = data_in.GetDataSize();
  cdm_in.iv = iv;
  cdm_in.iv_size = 16; //Always 16, see AP4_CencSingleSampleDecrypter declaration.
  cdm_in.key_id = wv_adapter->GetKeyId();
  cdm_in.key_id_size = wv_adapter->GetKeyIdSize();
  cdm_in.num_subsamples = subsample_count;
  cdm_in.subsamples = subsample_buffer_;

  CdmBuffer buf(&data_out);
  CdmDecryptedBlock cdm_out;
  cdm_out.SetDecryptedBuffer(&buf);

  cdm::Status ret = wv_adapter->Decrypt(cdm_in, &cdm_out);

  return (ret == cdm::Status::kSuccess) ? AP4_SUCCESS : AP4_ERROR_INVALID_PARAMETERS;
}

class WVDecrypter: public SSD_DECRYPTER
{
public:
  // Return supported URN if type matches to capabikitues, otherwise null
  const char *Supported(const char* licenseType, const char *licenseKey) override
  {
    licenseKey_ = licenseKey;
    if (strcmp(licenseType, "com.widevine.alpha") == 0)
      return "urn:uuid:EDEF8BA9-79D6-4ACE-A3C8-27DCD51D21ED";
    return 0;
  };

  AP4_CencSingleSampleDecrypter *CreateSingleSampleDecrypter(AP4_DataBuffer &streamCodec) override
  {
    AP4_CencSingleSampleDecrypter *res = new WV_CencSingleSampleDecrypter(licenseKey_, streamCodec.GetData(), streamCodec.GetDataSize());
    if (!((WV_CencSingleSampleDecrypter*)res)->initialized())
    {
      delete res;
      res = 0;
    }
    return res;
  }
private:
  std::string licenseKey_;
} decrypter;

extern "C" {

#ifdef _WIN32
#define MODULE_API __declspec(dllexport)
#else
#define MODULE_API
#endif

  class SSD_DECRYPTER MODULE_API *CreateDecryptorInstance(class SSD_HOST *h)
  {
    host = h;
    return &decrypter;
  };

};
