#pragma once

#include "StoredQueryConfig.h"
#include "WfsConvenience.h"
#include <boost/logic/tribool.hpp>
#include <boost/optional.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/variant.hpp>
#include <spine/Value.h>

namespace SmartMet
{
namespace Plugin
{
namespace WFS
{
class RequestParameterMap;
class SupportsExtraHandlerParams;

/**
 *   @brief Base class for WFS stored query parameter template
 *
 *   Object of class derived from this base class is used
 *   to obtain values of parameters to be used for
 *   handling the query (for example for providing them to
 *   ObservationEngine) from values provided in WFS request
 */
class ParameterTemplateBase
{
  StoredQueryConfig& config;
  const std::string base_path;
  const std::string config_path;

 protected:
  ParameterTemplateBase(StoredQueryConfig& config,
                        const std::string& base_path,
                        const std::string& config_path);

 public:
  static const char* HANDLER_PARAM_NAME;

 public:
  virtual ~ParameterTemplateBase();

  /**
   *   @brief Resolves value (or values) from parameter template
   *
   *   @retval true result found
   *   @retval false result not found
   *   @retval boost::indeterminate partial result found (eg. part of template
   *           members for @ref ArrayParameterTemplate)
   */
  virtual boost::tribool get_value(
      boost::variant<SmartMet::Spine::Value, std::vector<SmartMet::Spine::Value> >& result,
      const RequestParameterMap& req_param_map,
      const SupportsExtraHandlerParams* extra = nullptr,
      bool strict = true) const = 0;

 protected:
  inline const StoredQueryConfig& get_config() const { return config; }
  inline const std::string& get_base_path() const { return base_path; }
  inline const std::string& get_config_path() const { return config_path; }
  libconfig::Setting* get_setting_root();

  /**
   *   @brief Get the description of WFS request parameter by its name
   *
   *   Throws std::runtime_error if not found.
   */
  const StoredQueryConfig::ParamDesc& get_param_desc(const std::string& link_name) const;

  void handle_exceptions(const std::string& location) const __attribute__((noreturn));
};

}  // namespace WFS
}  // namespace Plugin
}  // namespace SmartMet
