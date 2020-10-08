/*
 *  Copyright (C) 2020 Team Kodi (https://kodi.tv)
 *  Copyright (C) 2010-2012 Marcel Groothuis, Fred Hoogduin
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include <kodi/addon-instance/Inputstream.h>

class CArgusTVAddon;
class cPVRClientArgusTV;
class CKeepAliveThread;

namespace ArgusTV
{
class CTsReader;
}

class CStreamLive : public kodi::addon::CInstanceInputStream
{
public:
  CStreamLive(const CArgusTVAddon& base, KODI_HANDLE instance, const std::string& kodiVersion);

  bool Open(INPUTSTREAM& props) override;

  void Close() override;

  void GetCapabilities(INPUTSTREAM_CAPABILITIES& capabilities) override
  {
    capabilities.m_mask |= INPUTSTREAM_CAPABILITIES::SUPPORTS_SEEK;
    capabilities.m_mask |= INPUTSTREAM_CAPABILITIES::SUPPORTS_PAUSE;
  }

  INPUTSTREAM_IDS GetStreamIds() override
  {
    INPUTSTREAM_IDS ids = {0};
    return ids;
  }

  INPUTSTREAM_INFO GetStream(int streamid) override
  {
    INPUTSTREAM_INFO info = {INPUTSTREAM_INFO::STREAM_TYPE::TYPE_NONE};
    return info;
  }

  void EnableStream(int streamid, bool enable) override {}

  bool OpenStream(int streamid) override { return true; }

  int ReadStream(uint8_t* buffer, unsigned int bufferSize) override;
  int64_t SeekStream(int64_t position, int whence) override;
  int64_t LengthStream() override;
  bool IsRealTimeStream() override;

private:
  cPVRClientArgusTV* m_client = nullptr;

  ArgusTV::CTsReader* m_tsreader = nullptr;
  int m_iCurrentChannel = -1;
  bool m_bTimeShiftStarted = false;

  const CArgusTVAddon& m_base;
};
