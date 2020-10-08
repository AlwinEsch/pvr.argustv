/*
 *  Copyright (C) 2020 Team Kodi (https://kodi.tv)
 *  Copyright (C) 2010-2012 Marcel Groothuis, Fred Hoogduin
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include "EventsThread.h"
#include "KeepAliveThread.h"
#include "addon.h"
#include "argustvrpc.h"
#include "channel.h"
#include "guideprogram.h"
#include "recording.h"

#include <kodi/addon-instance/PVR.h>
#include <map>
#include <vector>

namespace ArgusTV
{
class CTsReader;
}

#undef ATV_DUMPTS

class cPVRClientArgusTV : public kodi::addon::CInstancePVRClient
{
public:
  /* Class interface */
  cPVRClientArgusTV(const CArgusTVAddon& base,
                    KODI_HANDLE instance,
                    const std::string& kodiVersion);
  ~cPVRClientArgusTV();

  /* Server handling */
  bool Connect();
  void Disconnect();
  bool IsUp();
  bool ShareErrorsFound(void);

  /* General handling */
  PVR_ERROR GetCapabilities(kodi::addon::PVRCapabilities& capabilities) override;
  PVR_ERROR GetBackendName(std::string& name) override;
  PVR_ERROR GetBackendVersion(std::string& version) override;
  PVR_ERROR GetConnectionString(std::string& connection) override;
  PVR_ERROR GetDriveSpace(uint64_t& total, uint64_t& used) override;

  /* EPG handling */
  PVR_ERROR GetEPGForChannel(int channelUid,
                             time_t start,
                             time_t end,
                             kodi::addon::PVREPGTagsResultSet& results) override;

  /* Channel handling */
  PVR_ERROR GetChannelsAmount(int& amount) override;
  PVR_ERROR GetChannels(bool radio, kodi::addon::PVRChannelsResultSet& results) override;
  PVR_ERROR GetChannelStreamProperties(
      const kodi::addon::PVRChannel& channel,
      std::vector<kodi::addon::PVRStreamProperty>& properties) override;

  /* Channel group handling */
  PVR_ERROR GetChannelGroupsAmount(int& amount) override;
  PVR_ERROR GetChannelGroups(bool radio, kodi::addon::PVRChannelGroupsResultSet& results) override;
  PVR_ERROR GetChannelGroupMembers(const kodi::addon::PVRChannelGroup& group,
                                   kodi::addon::PVRChannelGroupMembersResultSet& results) override;

  /* Record handling **/
  PVR_ERROR GetRecordingsAmount(bool deleted, int& amount) override;
  PVR_ERROR GetRecordings(bool deleted, kodi::addon::PVRRecordingsResultSet& results) override;
  PVR_ERROR DeleteRecording(const kodi::addon::PVRRecording& recinfo) override;
  PVR_ERROR RenameRecording(const kodi::addon::PVRRecording& recinfo) override;
  PVR_ERROR SetRecordingLastPlayedPosition(const kodi::addon::PVRRecording& recinfo,
                                           int lastplayedposition) override;
  PVR_ERROR GetRecordingLastPlayedPosition(const kodi::addon::PVRRecording& recording,
                                           int& position) override;
  PVR_ERROR SetRecordingPlayCount(const kodi::addon::PVRRecording& recinfo, int playcount) override;
  PVR_ERROR GetRecordingStreamProperties(
      const kodi::addon::PVRRecording& recinfo,
      std::vector<kodi::addon::PVRStreamProperty>& properties) override;

  /* Timer handling */
  PVR_ERROR GetTimersAmount(int& amount) override;
  PVR_ERROR GetTimers(kodi::addon::PVRTimersResultSet& results) override;
  PVR_ERROR AddTimer(const kodi::addon::PVRTimer& timer) override;
  PVR_ERROR DeleteTimer(const kodi::addon::PVRTimer& timer, bool bForceDelete = false) override;
  PVR_ERROR UpdateTimer(const kodi::addon::PVRTimer& timer) override;

  /* Live stream handling */
  PVR_ERROR GetSignalStatus(int channelUid, kodi::addon::PVRSignalStatus& signalStatus) override;

  CArgusTV& GetRPC() { return m_rpc; }
  cChannel* FetchChannel(int channelid, bool LogError = true);
  bool FindRecEntry(const std::string& recId, std::string& recEntryURL);
  CKeepAliveThread* KeepAlive() { return m_keepalive; }
  void ResetSignalQuality() { m_signalqualityInterval = 0; }

private:
  cChannel* FetchChannel(std::vector<cChannel*> m_Channels, int channelid, bool LogError = true);
  void FreeChannels(std::vector<cChannel*> m_Channels);
  bool FindRecEntryUNC(const std::string& recId, std::string& recEntryURL);

  int m_iCurrentChannel = -1;
  bool m_bConnected = false;
  bool m_bTimeShiftStarted = false;
  int m_iBackendVersion = 0;
  std::string m_sBackendVersion;
  time_t m_BackendUTCoffset = 0;
  time_t m_BackendTime = 0;

  std::mutex m_ChannelCacheMutex;
  std::vector<cChannel*>
      m_TVChannels; // Local TV channel cache list needed for id to guid conversion
  std::vector<cChannel*>
      m_RadioChannels; // Local Radio channel cache list needed for id to guid conversion
  std::map<std::string, std::string>
      m_RecordingsMap; // <PVR_RECORDING.strRecordingId, URL of recording>
  int m_epg_id_offset = 0;
  int m_signalqualityInterval = 0;
  ArgusTV::CTsReader* m_tsreader = nullptr;
  CKeepAliveThread* m_keepalive = {new CKeepAliveThread(*this)};
  CEventsThread* m_eventmonitor = {new CEventsThread(*this)};
  bool m_bRecordingPlayback = false;
#if defined(ATV_DUMPTS)
  char ofn[25];
  int ofd;
#endif

  std::string m_baseURL;
  CArgusTV m_rpc;
  const CArgusTVAddon& m_base;
};
