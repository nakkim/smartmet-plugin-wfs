#pragma once

#ifndef WITHOUT_OBSERVATION

#include "StoredQueryHandlerBase.h"
#include "SupportsExtraHandlerParams.h"
#include "RequiresGeoEngine.h"
#include "RequiresObsEngine.h"
#include <engines/geonames/Engine.h>
#include <engines/observation/Engine.h>
#include <engines/observation/MastQuery.h>

namespace SmartMet
{
namespace Plugin
{
namespace WFS
{
class StoredEnvMonitoringNetworkQueryHandler : protected virtual SupportsExtraHandlerParams,
                                               protected virtual RequiresGeoEngine,
                                               protected virtual RequiresObsEngine,
                                               public StoredQueryHandlerBase

{
 public:
  StoredEnvMonitoringNetworkQueryHandler(SmartMet::Spine::Reactor* reactor,
                                         StoredQueryConfig::Ptr config,
                                         PluginImpl& plugin_impl,
                                         boost::optional<std::string> template_file_name);
  virtual ~StoredEnvMonitoringNetworkQueryHandler();

  virtual void query(const StoredQuery& query,
                     const std::string& language,
		     const boost::optional<std::string> &hostname,
                     std::ostream& output) const;

 private:
  const std::shared_ptr<SmartMet::Engine::Observation::DBRegistryConfig> dbRegistryConfig(
      const std::string& configName) const;

  std::string m_missingText;
  int m_debugLevel;
};

}  // namespace WFS
}  // namespace Plugin
}  // namespace SmartMet

#endif  // WITHOUT_OBSERVATION
