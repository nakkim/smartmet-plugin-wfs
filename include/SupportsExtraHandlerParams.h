#pragma once

#include "ParameterTemplateBase.h"
#include "RequestParameterMap.h"
#include "StoredQueryConfig.h"
#include "StoredQueryParamRegistry.h"
#include <boost/shared_ptr.hpp>
#include <ctpp2/CDT.hpp>
#include <map>

namespace SmartMet
{
namespace Plugin
{
namespace WFS
{
class ParameterTemplateBase;

/**
 *  @brief Adds supports of additional stored query handler parameters which
 *         can be added from configuration.
 *
 *  Stored query handler itself must have support for this feature
 *  to be usefull for anything but redirecting parameter query to another
 *  handler parameter when primary parameter is not specified
 */
class SupportsExtraHandlerParams : virtual protected StoredQueryParamRegistry
{
 public:
  SupportsExtraHandlerParams(boost::shared_ptr<StoredQueryConfig> config,
                             bool mandatory = false,
                             const char* path = "named_params");

  virtual ~SupportsExtraHandlerParams();

  const ParameterTemplateBase& get_param(const std::string& name) const;

  inline const std::map<std::string, boost::shared_ptr<ParameterTemplateBase> >& get_param_map()
      const
  {
    return handler_params;
  }

  /**
   *  @brief Add named parameters to template formatter hash
   */
  void dump_named_params(const RequestParameterMap& req_param_map, CTPP::CDT& hash) const;

  const StoredQueryConfig& get_config() const { return *config; }

 private:
  const std::string path;
  std::map<std::string, boost::shared_ptr<ParameterTemplateBase> > handler_params;
  boost::shared_ptr<StoredQueryConfig> config;
};

}  // namespace WFS
}  // namespace Plugin
}  // namespace SmartMet
