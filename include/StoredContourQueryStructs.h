#pragma once

#include "PluginData.h"
#include "StoredQueryConfig.h"
#include "StoredQueryHandlerBase.h"
#include "StoredQueryHandlerFactoryDef.h"
#include "SupportsBoundingBox.h"
#include "SupportsExtraHandlerParams.h"
#include "SupportsLocationParameters.h"
#include "SupportsTimeParameters.h"
#include "SupportsTimeZone.h"

#include <engines/contour/Engine.h>
#include <engines/querydata/Engine.h>

#include <gis/OGR.h>

namespace SmartMet
{
namespace Plugin
{
namespace WFS
{
struct ContourQueryParameter
{
  const SmartMet::Spine::Parameter& parameter;
  const SmartMet::Engine::Querydata::Q& q;
  OGRSpatialReference& sr;
  SmartMet::Spine::BoundingBox bbox;
  std::string tz_name;
  bool smoothing;
  unsigned short smoothing_degree;
  unsigned short smoothing_size;
  SmartMet::Spine::TimeSeriesGenerator::LocalTimeList tlist;

  ContourQueryParameter(const SmartMet::Spine::Parameter& p,
                        const SmartMet::Engine::Querydata::Q& qe,
                        OGRSpatialReference& spref)
      : parameter(p),
        q(qe),
        sr(spref),
        bbox(-180.0, -90.0, 180.0, 90.0),
        tz_name("UTC"),
        smoothing(false),
        smoothing_degree(2),
        smoothing_size(2)
  {
  }
  virtual ~ContourQueryParameter() {}
};

struct CoverageQueryParameter : ContourQueryParameter
{
  CoverageQueryParameter(const SmartMet::Spine::Parameter& p,
                         const SmartMet::Engine::Querydata::Q& q,
                         OGRSpatialReference& sr)
      : ContourQueryParameter(p, q, sr)
  {
  }

  std::vector<SmartMet::Engine::Contour::Range> limits;
};

struct IsolineQueryParameter : ContourQueryParameter
{
  IsolineQueryParameter(const SmartMet::Spine::Parameter& p,
                        const SmartMet::Engine::Querydata::Q& q,
                        OGRSpatialReference& sr)
      : ContourQueryParameter(p, q, sr)
  {
  }

  std::string unit;
  std::vector<double> isovalues;
};

// contains timestamp and corresponding geometry
// for example heavy snow areas at 12:00
struct WeatherAreaGeometry
{
  boost::posix_time::ptime timestamp;
  OGRGeometryPtr geometry;

  WeatherAreaGeometry(const boost::posix_time::ptime& t, OGRGeometryPtr g)
      : timestamp(t), geometry(g)
  {
  }
};

struct ContourQueryResult
{
  std::string name;
  std::string unit;
  std::vector<WeatherAreaGeometry> area_geoms;
};

// contains weather name and result set for a query
// for example heavy snow areas at 12:00, 13:00, 14:00
struct CoverageQueryResult : ContourQueryResult
{
  double lolimit;
  double hilimit;
};

struct IsolineQueryResult : ContourQueryResult
{
  double isovalue;
};

typedef boost::shared_ptr<ContourQueryResult> ContourQueryResultPtr;
typedef boost::shared_ptr<CoverageQueryResult> CoverageQueryResultPtr;
typedef boost::shared_ptr<IsolineQueryResult> IsolineQueryResultPtr;

typedef std::vector<ContourQueryResultPtr> ContourQueryResultSet;

}  // namespace WFS
}  // namespace Plugin
}  // namespace SmartMet
