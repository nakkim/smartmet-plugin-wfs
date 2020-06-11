#pragma once

#include <macgyver/StringConversion.h>
#include <memory>
#include <string>

namespace SmartMet
{
namespace Plugin
{
namespace WFS
{
class MediaMonitored
{
 public:
  typedef std::string MediaValueType;
  typedef std::string KeywordType;
  typedef std::set<MediaValueType> MediaValueSetType;
  typedef std::map<KeywordType, MediaValueType> KeywordToMediaValueMapType;

  MediaMonitored() {}
  virtual ~MediaMonitored() {}
  void add(const KeywordType& keyword)
  {
    // Validate the keyword and insert the MediaValue.
    KeywordToMediaValueMapType::const_iterator ktmvmIt =
        m_keywordToMediaValueMap.find(Fmi::ascii_tolower_copy(keyword));
    if (ktmvmIt != m_keywordToMediaValueMap.end())
      m_mediaValueSet.insert(ktmvmIt->second);
  }

  MediaValueSetType::const_iterator begin() const { return m_mediaValueSet.begin(); }
  MediaValueSetType::const_iterator end() const { return m_mediaValueSet.end(); }

 private:
  MediaMonitored& operator=(const MediaMonitored& other);
  MediaMonitored(const MediaMonitored& other);

  MediaValueSetType m_mediaValueSet;

  // first: keyword, second: meadiValue
  const KeywordToMediaValueMapType m_keywordToMediaValueMap{{"atmosphere", "air"},
                                                            {"surface", "air"},
                                                            {"seasurf", "water"},
                                                            {"sealayer", "water"},
                                                            {"magnetosphere", "air"},
                                                            {"air", "air"},
                                                            {"biota", "biota"},
                                                            {"landscape", "landscape"},
                                                            {"sediment", "sediment"},
                                                            {"soil", "soil"},
                                                            {"ground", "ground"},
                                                            {"waste", "waste"},
                                                            {"water", "water"}};
};

}  // namespace WFS
}  // namespace Plugin
}  // namespace SmartMet
