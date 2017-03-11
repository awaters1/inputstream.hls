/*
 * kodi.h Copyright (C) 2017 Anthony Waters <awaters1@gmail.com>
 */

#pragma once

#include "globals.h"

#include "SSD_dll.h"

struct addonstring
{
    addonstring(char *d){data_= d;};
    ~addonstring() {xbmc->FreeString(data_);};
    const char* c_str() {return data_? data_:"";};
    char *data_;
};


/*******************************************************
kodi host - interface for decrypter libraries
********************************************************/
class KodiHost : public SSD_HOST
{
public:
  virtual const char *GetLibraryPath() const override
  {
    return m_strLibraryPath.c_str();
  };

  virtual const char *GetProfilePath() const override
  {
    return m_strProfilePath.c_str();
  };

  virtual void* CURLCreate(const char* strURL) override
  {
    return xbmc->CURLCreate(strURL);
  };

  virtual bool CURLAddOption(void* file, CURLOPTIONS opt, const char* name, const char * value)override
  {
    const XFILE::CURLOPTIONTYPE xbmcmap[] = {XFILE::CURL_OPTION_PROTOCOL, XFILE::CURL_OPTION_HEADER};
    return xbmc->CURLAddOption(file, xbmcmap[opt], name, value);
  }

  virtual bool CURLOpen(void* file)override
  {
    return xbmc->CURLOpen(file, XFILE::READ_NO_CACHE);
  };

  virtual size_t ReadFile(void* file, void* lpBuf, size_t uiBufSize)override
  {
    return xbmc->ReadFile(file, lpBuf, uiBufSize);
  };

  virtual void CloseFile(void* file)override
  {
    return xbmc->CloseFile(file);
  };

  virtual bool CreateDirectory(const char *dir)override
  {
    return xbmc->CreateDirectory(dir);
  };

  virtual void Log(LOGLEVEL level, const char *msg)override
  {
    const ADDON::addon_log_t xbmcmap[] = { ADDON::LOG_DEBUG, ADDON::LOG_INFO, ADDON::LOG_ERROR };
    return xbmc->Log(xbmcmap[level], msg);
  };

  void SetLibraryPath(const char *libraryPath)
  {
    m_strLibraryPath = libraryPath;

    const char *pathSep(libraryPath[0] && libraryPath[1] == ':' && isalpha(libraryPath[0]) ? "\\" : "/");

    if (m_strLibraryPath.size() && m_strLibraryPath.back() != pathSep[0])
      m_strLibraryPath += pathSep;
  }

  void SetProfilePath(const char *profilePath)
  {
    m_strProfilePath = profilePath;

    const char *pathSep(profilePath[0] && profilePath[1] == ':' && isalpha(profilePath[0]) ? "\\" : "/");

    if (m_strProfilePath.size() && m_strProfilePath.back() != pathSep[0])
      m_strProfilePath += pathSep;

    //let us make cdm userdata out of the addonpath and share them between addons
    m_strProfilePath.resize(m_strProfilePath.find_last_of(pathSep[0], m_strProfilePath.length() - 2));
    m_strProfilePath.resize(m_strProfilePath.find_last_of(pathSep[0], m_strProfilePath.length() - 1));
    m_strProfilePath.resize(m_strProfilePath.find_last_of(pathSep[0], m_strProfilePath.length() - 1) + 1);

    xbmc->CreateDirectory(m_strProfilePath.c_str());
    m_strProfilePath += "cdm";
    m_strProfilePath += pathSep;
    xbmc->CreateDirectory(m_strProfilePath.c_str());
  }

private:
  std::string m_strProfilePath, m_strLibraryPath;

};

extern KodiHost kodihost;

