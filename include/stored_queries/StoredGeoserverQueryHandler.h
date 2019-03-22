#pragma once

#include "ArrayParameterTemplate.h"
#include "GeoServerDataIndex.h"
#include "ScalarParameterTemplate.h"
#include "SupportsBoundingBox.h"
#include "stored_queries/StoredAtomQueryHandlerBase.h"

namespace SmartMet
{
namespace Plugin
{
namespace WFS
{
class StoredGeoserverQueryHandler : public StoredAtomQueryHandlerBase, protected SupportsBoundingBox
{
 public:
  StoredGeoserverQueryHandler(SmartMet::Spine::Reactor *reactor,
                              boost::shared_ptr<StoredQueryConfig> config,
                              PluginImpl &plugin_impl,
                              boost::optional<std::string> template_file_name);

  virtual ~StoredGeoserverQueryHandler();

 protected:
  virtual void update_parameters(
      const RequestParameterMap &request_params,
      int seq_id,
      std::vector<boost::shared_ptr<RequestParameterMap> > &result) const;

 private:
  /* FIXME: optimize to avoid need to read query paremeters again and again (if needed) */
  bool eval_db_select_params(const std::map<std::string, std::string> &select_params,
                             const GeoServerDataIndex::LayerRec &rec) const;

 private:
  /**
   *   @brief Maps layer name to database table name
   */
  boost::optional<std::map<std::string, std::string> > layer_map;

  std::map<std::string, std::string> layer_param_name_map;

  /**
   *   @brief Specifies the format (for boost::format)
   *          for generating PostGIS table name from laer name
   *
   *   The default value (if layer_nap is not provided either)
   *   is "mosaic.%1%".
   */
  boost::optional<std::string> layer_db_table_name_format;

  /**
   *   @brief Maps layer aliases to layer names
   */
  std::map<std::string, std::string> layer_alias_map;

  /**
   *   @brief Names of query handler named parameters used for filtering
   *          by database columns
   */
  std::set<std::string> db_filter_param_names;

  int debug_level;

 protected:
  static const char *P_BEGIN_TIME;
  static const char *P_END_TIME;
  static const char *P_LAYERS;
  static const char *P_WIDTH;
  static const char *P_HEIGHT;
  static const char *P_CRS;
};

}  // namespace WFS
}  // namespace Plugin
}  // namespace SmartMet
