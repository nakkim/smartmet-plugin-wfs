#pragma once

#include "PluginData.h"
#include "ScalarParameterTemplate.h"
#include "StoredQueryConfig.h"
#include "StoredQueryHandlerBase.h"
#include "StoredQueryHandlerFactoryDef.h"

namespace SmartMet
{
namespace Plugin
{
namespace WFS
{
/**
 *   @brief Handler for GetFeatureById stored query
 */
class GetFeatureByIdHandler : public StoredQueryHandlerBase
{
 public:
  GetFeatureByIdHandler(SmartMet::Spine::Reactor* reactor,
                        boost::shared_ptr<StoredQueryConfig> config,
                        PluginImpl& plugin_data);

  virtual ~GetFeatureByIdHandler();

  virtual void query(const StoredQuery& query,
                     const std::string& language,
                     std::ostream& output) const;

  virtual std::vector<std::string> get_return_types() const;
};

}  // namespace WFS
}  // namespace Plugin
}  // namespace SmartMet

extern SmartMet::Plugin::WFS::StoredQueryHandlerFactoryDef wfs_get_feature_by_id_handler_factory;
