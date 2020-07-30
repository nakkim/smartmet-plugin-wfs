#include "SupportsTimeZone.h"
#include <boost/algorithm/string.hpp>
#include <macgyver/StringConversion.h>
#include <macgyver/TimeZones.h>
#include <spine/Convenience.h>
#include <spine/Exception.h>

namespace bw = SmartMet::Plugin::WFS;

namespace ba = boost::algorithm;
namespace lt = boost::local_time;
namespace pt = boost::posix_time;
namespace dt = boost::date_time;

namespace
{
const char* P_TZ = "timeZone";
}

bw::SupportsTimeZone::SupportsTimeZone(SmartMet::Spine::Reactor* reactor, StoredQueryConfig::Ptr config)
    : bw::StoredQueryParamRegistry(config)
    , bw::SupportsExtraHandlerParams(config, false)
    , bw::RequiresGeoEngine(reactor)
{
  try
  {
    register_scalar_param<std::string>(P_TZ);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

bw::SupportsTimeZone::~SupportsTimeZone() {}

std::string bw::SupportsTimeZone::get_tz_name(const RequestParameterMap& param_values) const
{
  try
  {
    std::string result = "UTC";
    if (param_values.count(P_TZ))
      result = param_values.get_single<std::string>(P_TZ);
    return result;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

lt::time_zone_ptr bw::SupportsTimeZone::get_tz_for_site(double longitude,
                                                        double latitude,
                                                        const std::string& tz_name) const
{
  try
  {
    if (ba::iequals(tz_name, "local"))
    {
      return geo_engine
          ->getTimeZones()
          .time_zone_from_coordinate(longitude, latitude);
    }
    else
    {
      return get_time_zone(tz_name);
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

boost::local_time::time_zone_ptr bw::SupportsTimeZone::get_time_zone(const std::string& tz_name) const
{
  try
  {
    std::string name = ba::trim_copy(tz_name);
    if (ba::iequals(name, "utc") or (ba::iequals(name, "gmt")))
      name = "Etc/GMT";
    return geo_engine
        ->getTimeZones()
        .time_zone_from_region(name);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

std::string bw::SupportsTimeZone::format_local_time(const pt::ptime& utc_time,
                                                    boost::local_time::time_zone_ptr tz)
{
  try
  {
    lt::local_date_time t1(utc_time, tz);

    std::string result = Fmi::to_iso_extended_string(t1.local_time()) + t1.zone_abbrev(true);

    // Replace '+0000' at end with 'Z' if needed and otherwise insert ':'
    if (ba::ends_with(result, "+0000"))
    {
      result = result.substr(0, result.length() - 5) + "Z";
    } else {
      result.insert(result.length() - 2, 1, ':');
    }

    return result;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

/**

@page WFS_SQ_TIME_ZONE Time zone parameter

This parameter group adds a single stored query handler parameter @b timeZone, which should be used
to specify time zone.

Following time zone specification formats are supported:
- @b utc or @b gmt (case insensitive) is translated at first to <b>Etc/GMT</b>
- format <i>region/place</i> is supported
- POSIX time zone specification should also work

 */
