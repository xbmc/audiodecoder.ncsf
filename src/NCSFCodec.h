/*
 *      Copyright (C) 2019-2020 Team Kodi
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
 *  along with XBMC; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */

#pragma once

#include "CircularBuffer.h"

#include "SSEQPlayer/Player.h"
#include "SSEQPlayer/SDAT.h"

#include <kodi/addon-instance/AudioDecoder.h>
#include <math.h>
#include <string>
#include <vector>

struct NCSFLoaderState
{
  uint32_t sseq = 0;
  std::vector<uint8_t> sdatData;
  std::unique_ptr<SDAT> sdat;
};

struct NCSFContext
{
  int year = 0;
  int tagSongMs = 0;
  int tagFadeMs = 0;

  std::string artist;
  std::string title;
  std::string game;
  std::string copyright;
  std::string comment;
  std::string disc;
  std::string track;
};

class ATTRIBUTE_HIDDEN CNCSFCodec : public kodi::addon::CInstanceAudioDecoder
{
public:
  CNCSFCodec(KODI_HANDLE instance) :
    CInstanceAudioDecoder(instance) {}
  ~CNCSFCodec() override = default;

  bool Init(const std::string& filename, unsigned int filecache,
            int& channels, int& samplerate,
            int& bitspersample, int64_t& totaltime,
            int& bitrate, AEDataFormat& format,
            std::vector<AEChannel>& channellist) override;
  int ReadPCM(uint8_t* buffer, int size, int& actualsize) override;
  int64_t Seek(int64_t time) override;
  bool ReadTag(const std::string& file, std::string& title,
               std::string& artist, int& length) override;

private:
  static void NCFSPrintMessage(void* context, const char* message);
  static int NCSFLoader(void * context, const uint8_t * exe, size_t exe_size,
                        const uint8_t * reserved, size_t reserved_size);
  static int NCFSInfoMeta(void* context, const char* name, const char* value);

  bool Load();

  inline uint64_t time_to_samples(double p_time, uint32_t p_sample_rate)
  {
    return (uint64_t)floor((double)p_sample_rate * p_time + 0.5);
  }

  inline void calcfade()
  {
    m_songLength = mul_div(m_tagSongMs-m_posDelta, m_cfgDefaultSampleRate, 1000);
    m_fadeLength = mul_div(m_tagFadeMs, m_cfgDefaultSampleRate, 1000);
  }

  inline int mul_div(int number, int numerator, int denominator)
  {
    long long ret = number;
    ret *= numerator;
    ret /= denominator;
    return (int) ret;
  }

  int m_cfgDefaultSampleRate = 48000;
  bool m_cfgSuppressOpeningSilence = true;
  bool m_cfgSuppressEndSilence = true;
  int m_cfgEndSilenceSeconds = 5;

  std::string m_file;

  bool m_eof;
  int m_dataWritten;
  int m_remainder;
  int m_posDelta;
  int m_startSilence;
  int m_silence;

  double m_pos;

  int m_songLength;
  int m_fadeLength;
  int m_tagSongMs;
  int m_tagFadeMs;

  Player m_player;
  circular_buffer<int16_t> m_silenceTestBuffer = 0;
  std::vector<uint8_t> m_sampleBuffer;
  NCSFLoaderState m_sseq;
};
