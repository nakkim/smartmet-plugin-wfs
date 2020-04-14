#pragma once

#include "RequiresGeoEngine.h"
#include "StoredQueryConfig.h"
#include "StoredQueryParamRegistry.h"
#include "SupportsExtraHandlerParams.h"
#include <boost/date_time/local_time/local_time.hpp>

namespace SmartMet
{
namespace Plugin
{
namespace WFS
{
/**
 *  @brief Base class for adding time zone support to stored query handler
 */
class SupportsTimeZone : protected virtual SupportsExtraHandlerParams,
                         protected virtual StoredQueryParamRegistry,
                         protected virtual RequiresGeoEngine
{
 public:
    SupportsTimeZone(SmartMet::Spine::Reactor* reactor, boost::shared_ptr<StoredQueryConfig> config);

  virtual ~SupportsTimeZone();

  std::string get_tz_name(const RequestParameterMap& param_values) const;

  boost::local_time::time_zone_ptr get_tz_for_site(double longitude,
                                                   double latitude,
                                                   const std::string& tz_name) const;

  boost::local_time::time_zone_ptr get_time_zone(const std::string& tz_name) const;

  static std::string format_local_time(const boost::posix_time::ptime& utc_time,
                                       boost::local_time::time_zone_ptr tz);
};

}  // namespace WFS
}  // namespace Plugin
}  // namespace SmartMet
