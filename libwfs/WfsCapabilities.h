#pragma once

#include "WfsFeatureDef.h"
#include <map>
#include <set>
#include <string>

namespace SmartMet
{
namespace Plugin
{
namespace WFS
{
class WfsCapabilities
{
 public:
  WfsCapabilities();

  virtual ~WfsCapabilities();

  std::set<std::string> get_operations() const;

  bool register_operation(const std::string& operation);

  std::map<std::string, boost::shared_ptr<WfsFeatureDef> > get_features() const;

  const WfsFeatureDef* find_feature(const std::string& name) const;

  void register_feature(boost::shared_ptr<WfsFeatureDef>& feature_def);

  void register_feature_use(const std::string& name);

  std::set<std::string> get_used_features() const;

  void register_data_set(const std::string& code, const std::string& ns);

  std::map<std::string, std::string> get_data_set_map() const;

 private:
  /**
   *  @brief The set of supported WFS operations
   */
  std::set<std::string> operations;

  /**
   *  @brief The map of supported WFS features
   */
  std::map<std::string, boost::shared_ptr<WfsFeatureDef> > features;

  /**
   *  @brief The set of used features
   */
  std::set<std::string> used_features;

  /**
   *  @brief Registred data sets
   */
  std::map<std::string, std::string> data_set_map;

  /**
   *  @brief Mutex for preventing race conditions when accessing this object
   */
  mutable SmartMet::Spine::MutexType mutex;
};

}  // namespace WFS
}  // namespace Plugin
}  // namespace SmartMet
