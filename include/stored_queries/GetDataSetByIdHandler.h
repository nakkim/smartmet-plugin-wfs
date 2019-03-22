#pragma once

#include "PluginImpl.h"
#include "StoredQueryConfig.h"
#include "StoredQueryHandlerBase.h"
#include "StoredQueryHandlerFactoryDef.h"
#include <map>

namespace SmartMet
{
namespace Plugin
{
namespace WFS
{
/**
 *   @brief Handler for GetFeatureById stored query
 */
class GetDataSetByIdHandler : public StoredQueryHandlerBase
{
 public:
  GetDataSetByIdHandler(SmartMet::Spine::Reactor* reactor,
                        boost::shared_ptr<StoredQueryConfig> config,
                        PluginImpl& plugin_impl);

  virtual ~GetDataSetByIdHandler();

  virtual void query(const StoredQuery& query,
                     const std::string& language,
                     std::ostream& output) const;

  virtual bool redirect(const StoredQuery& query, std::string& new_stored_query_id) const;

  virtual std::vector<std::string> get_return_types() const;

  virtual void init_handler();

 private:
  /**
   *    @brief Maps data set ID to corresponding stored query ID which returns this
   */
  std::map<std::string, std::string> data_set_map;
};

}  // namespace WFS
}  // namespace Plugin
}  // namespace SmartMet

extern SmartMet::Plugin::WFS::StoredQueryHandlerFactoryDef wfs_get_feature_by_id_handler_factory;
