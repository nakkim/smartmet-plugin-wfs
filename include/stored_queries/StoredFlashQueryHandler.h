#pragma once

#ifndef WITHOUT_OBSERVATION

#include "ArrayParameterTemplate.h"
#include "ScalarParameterTemplate.h"
#include "StoredQueryHandlerBase.h"
#include "SupportsBoundingBox.h"
#include "SupportsExtraHandlerParams.h"
#include "SupportsTimeZone.h"
#include <engines/geonames/Engine.h>
#include <engines/observation/Engine.h>

namespace SmartMet
{
namespace Plugin
{
namespace WFS
{
class StoredFlashQueryHandler : public StoredQueryHandlerBase,
                                protected SupportsBoundingBox,
                                protected SupportsTimeZone,
                                protected virtual SupportsExtraHandlerParams
{
 public:
  StoredFlashQueryHandler(SmartMet::Spine::Reactor *reactor,
                          boost::shared_ptr<StoredQueryConfig> config,
                          PluginImpl &plugin_impl,
                          boost::optional<std::string> template_file_name);

  virtual ~StoredFlashQueryHandler();

  virtual void init_handler();

  virtual void query(const StoredQuery &query,
                     const std::string &language,
		     const boost::optional<std::string> &hostname,
                     std::ostream &output) const;

 private:
  SmartMet::Engine::Geonames::Engine *geo_engine;

  SmartMet::Engine::Observation::Engine *obs_engine;

  std::vector<SmartMet::Spine::Parameter> bs_param;
  int stroke_time_ind;
  int lon_ind;
  int lat_ind;
  std::string station_type;

  double max_hours;
  std::string missing_text;
  bool sq_restrictions;
  int time_block_size;
};

}  // namespace WFS
}  // namespace Plugin
}  // namespace SmartMet

#endif  // WITHOUT_OBSERVATION
