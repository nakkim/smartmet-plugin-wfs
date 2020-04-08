#pragma once

#include <boost/date_time/posix_time/ptime.hpp>
#include <boost/geometry/geometry.hpp>
#include <boost/shared_ptr.hpp>
#include <gdal/ogr_geometry.h>

#include <engines/geonames/Engine.h>
#include <engines/querydata/Engine.h>
#include <spine/TimeSeries.h>

#include "RequestParameterMap.h"
#include "RequiresGeoEngine.h"
#include "RequiresQEngine.h"
#include "SupportsBoundingBox.h"
#include "SupportsTimeParameters.h"
#include "stored_queries/StoredAtomQueryHandlerBase.h"

namespace SmartMet
{
#ifndef DOXYGEN_SHOULD_SKIP_THIS
namespace Engine
{
namespace Geonames
{
class Engine;
}
namespace Querydata
{
class Engine;
}
}  // namespace Engine
#endif

namespace Plugin
{
namespace WFS
{
class StoredGridQueryHandler : public StoredQueryHandlerBase,
                               protected SupportsBoundingBox,
                               protected SupportsTimeParameters,
                               protected virtual RequiresGeoEngine,
                               protected virtual RequiresQEngine
{
  struct Result
  {
    typedef std::vector<std::string> Grid;
    typedef std::vector<Grid> ParamTimeSeries;
    typedef std::vector<ParamTimeSeries> LevelData;
    std::list<LevelData> dataLevels;  // The order is data[level][parameter][time][grid]

    std::vector<boost::posix_time::ptime> timesteps;

    std::vector<std::pair<std::string, FmiParameterName> > paramInfos;

    unsigned int xdim;

    unsigned int ydim;

    float ll_lon;

    float ll_lat;

    float ur_lon;

    float ur_lat;
  };

  typedef boost::geometry::model::point<float, 2, boost::geometry::cs::cartesian> point_t;
  typedef boost::geometry::model::box<point_t> box_t;
  typedef boost::geometry::model::ring<point_t> ring_t;

 public:
  struct Query
  {
    std::list<std::string> models;
    std::set<int> levels;
    std::string level_type;
    std::list<std::pair<std::string, SmartMet::Spine::LocationPtr> > locations;
    std::vector<SmartMet::Spine::Parameter> data_params;
    std::string missing_text;
    std::string language;
    std::shared_ptr<const std::locale> output_locale;

    Result result;

    std::unique_ptr<boost::posix_time::ptime> origin_time;

    std::unique_ptr<SmartMet::Spine::ValueFormatter> value_formatter;
    std::unique_ptr<Fmi::TimeFormatter> time_formatter;
    boost::shared_ptr<SmartMet::Spine::TimeSeriesGeneratorOptions> toptions;

    SmartMet::Spine::BoundingBox requested_bbox;

    NFmiPoint top_left;
    NFmiPoint top_right;
    NFmiPoint bottom_left;
    NFmiPoint bottom_right;

    mutable NFmiPoint lastpoint;
    bool find_nearest_valid_point;

    std::string producer_name;
    std::string model_path;

    std::size_t first_data_ind;
    std::size_t last_data_ind;

    unsigned long scaleFactor;
    unsigned long precision;

    bool includeDebugData;

    Query(boost::shared_ptr<const StoredQueryConfig> config);
    ~Query();
  };

  StoredGridQueryHandler(SmartMet::Spine::Reactor* reactor,
                         boost::shared_ptr<StoredQueryConfig> config,
                         PluginImpl& plugin_impl,
                         boost::optional<std::string> template_file_name);
  virtual ~StoredGridQueryHandler();

  virtual void query(const StoredQuery& query,
                     const std::string& language,
		     const boost::optional<std::string>& hostname,
                     std::ostream& output) const;

 private:
  void parse_times(const RequestParameterMap& param, Query& dest) const;

  void parse_params(const RequestParameterMap& param, Query& dest) const;

  void parse_models(const RequestParameterMap& param, Query& dest) const;

  void parse_levels(const RequestParameterMap& params, Query& dest) const;

  std::map<std::string, SmartMet::Engine::Querydata::ModelParameter> get_model_parameters(
      const std::string& producer, const boost::posix_time::ptime& origin_time) const;

  SmartMet::Engine::Querydata::Producer select_producer(const SmartMet::Spine::Location& location,
                                                        const Query& query) const;

  Result extract_forecast(const RequestParameterMap& params,
                          Query& query,
                          const std::string& dataCrs) const;

  Result::Grid rearrangeGrid(const Result::Grid& inputGrid, int arrayWidth) const;

  std::pair<unsigned int, unsigned int> getDataIndexExtents(
      const SmartMet::Spine::TimeSeries::TimeSeriesGroupPtr& longitudes,
      const SmartMet::Spine::TimeSeries::TimeSeriesGroupPtr& latitudes,
      const Query& query,
      const std::string& dataCrs) const;

 private:
  SmartMet::Engine::Geonames::Engine* geo_engine;
  SmartMet::Engine::Querydata::Engine* q_engine;

  const int debug_level;
};

}  // namespace WFS
}  // namespace Plugin
}  // namespace SmartMet
