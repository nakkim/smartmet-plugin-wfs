#pragma once

#include "ParameterTemplateBase.h"
#include "ParameterTemplateItem.h"
#include <spine/Value.h>
#include <map>
#include <vector>

namespace SmartMet
{
namespace Plugin
{
namespace WFS
{
class RequestParameterMap;
class SupportsExtraHandlerParams;

class ArrayParameterTemplate : public ParameterTemplateBase
{
 public:
  ArrayParameterTemplate(StoredQueryConfig& config,
                         const std::string& config_path,
                         int min_size = 0,
                         int max_size = 999);

  ArrayParameterTemplate(StoredQueryConfig& config,
                         const std::string& base_path,
                         const std::string& config_path,
                         int min_size = 0,
                         int max_size = 999);

  virtual ~ArrayParameterTemplate();

  inline const std::vector<ParameterTemplateItem> get_items() const { return items; }
  inline std::size_t get_num_items() const { return items.size(); }
  inline const ParameterTemplateItem& get_item(std::size_t ind) const { return items.at(ind); }
  std::vector<SmartMet::Spine::Value> get_value(
      const RequestParameterMap& req_param_map,
      const SupportsExtraHandlerParams* extra_params = nullptr) const;

  virtual boost::tribool get_value(
      boost::variant<SmartMet::Spine::Value, std::vector<SmartMet::Spine::Value> >& result,
      const RequestParameterMap& req_param_map,
      const SupportsExtraHandlerParams* extra = nullptr,
      bool strict = true) const;

  std::vector<int64_t> get_int_array(const RequestParameterMap& req_param_map,
                                     const SupportsExtraHandlerParams* extra_params = nullptr) const;

  std::vector<uint64_t> get_uint_array(const RequestParameterMap& req_param_map,
                                       const SupportsExtraHandlerParams* extra_params = nullptr) const;

  std::vector<double> get_double_array(const RequestParameterMap& req_param_map,
                                       const SupportsExtraHandlerParams* extra_params = nullptr) const;

  std::vector<std::string> get_string_array(
      const RequestParameterMap& req_param_map,
      const SupportsExtraHandlerParams* extra_params = nullptr) const;

  std::vector<boost::posix_time::ptime> get_ptime_array(
      const RequestParameterMap& req_param_map,
      const SupportsExtraHandlerParams* extra_params = nullptr) const;

  std::vector<SmartMet::Spine::Point> get_point_array(
      const RequestParameterMap& req_param_map,
      const SupportsExtraHandlerParams* extra_params = nullptr) const;

  std::vector<SmartMet::Spine::BoundingBox> get_bbox_array(
      const RequestParameterMap& req_param_map,
      const SupportsExtraHandlerParams* extra_params = nullptr) const;

 private:
  void init();

 private:
  std::vector<ParameterTemplateItem> items;
  int min_size;
  int max_size;
};

}  // namespace WFS
}  // namespace Plugin
}  // namespace SmartMet
