/*
 * KodiDASH.h Copyright (C) 2017 Anthony Waters <awaters1@gmail.com>
 */

#pragma once

#include "dash/DASHTree.h"
#include "dash/DASHStream.h"

/*******************************************************
Kodi Streams implementation
********************************************************/

class KodiDASHTree : public dash::DASHTree
{
protected:
  virtual bool download(const char* url);
};

class KodiDASHStream : public dash::DASHStream
{
public:
  KodiDASHStream(dash::DASHTree &tree, dash::DASHTree::StreamType type)
    :dash::DASHStream(tree, type){};
protected:
  virtual bool download(const char* url, const char* rangeHeader) override;
  virtual bool parseIndexRange() override;
};
