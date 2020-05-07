#pragma once

#include <boost/shared_ptr.hpp>
#include <spine/Value.h>
#include <map>
#include <string>

namespace SmartMet
{
namespace Plugin
{
namespace WFS
{
/**
 *   @brief Feature ID mapping to query parameters
 *
 *   @note One must increment unsigned integer constant
 *         FEATURE_ID_REVISION in FeatureID.cpp
 *         if FeatureID generation is changed to prevent
 *         accepting outdated FeatureID values.
 */
class FeatureID
{
  std::string stored_query_id;
  std::multimap<std::string, SmartMet::Spine::Value> params;
  unsigned seq_id;

 public:
  FeatureID(std::string stored_query_id,
            const std::multimap<std::string, SmartMet::Spine::Value> params,
            unsigned seq_id = 0);

  virtual ~FeatureID();

  /**
   *   @brief Restore FeatureID from string earlier returned by method get_id().
   */
  static boost::shared_ptr<FeatureID> create_from_id(const std::string& id);

  /**
   *   @brief Generate feature ID which can be used to reconstruct FeatureID data later.
   */
  std::string get_id() const;

  inline const std::string& get_stored_query_id() const { return stored_query_id; }
  inline const std::multimap<std::string, SmartMet::Spine::Value> get_params() const
  {
    return params;
  }
  void erase_param(const std::string& name);

  template <typename ValueType>
  void add_param(const std::string& name, const ValueType& value)
  {
    params.insert(std::make_pair(name, SmartMet::Spine::Value(value)));
  }

  template <typename IteratorType>
  void add_param(const std::string& name, IteratorType begin, IteratorType end)
  {
    while (begin != end)
    {
      const typename std::iterator_traits<IteratorType>::value_type& tmp = *begin++;
      params.insert(std::make_pair(name, SmartMet::Spine::Value(tmp)));
    }
  }

 public:
  static std::string prefix;
};

}  // namespace WFS
}  // namespace Plugin
}  // namespace SmartMet
