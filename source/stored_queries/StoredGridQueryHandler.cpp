#include <algorithm>
#include <iomanip>
#include <list>
#include <string>

#include <boost/algorithm/string.hpp>
#include <boost/bind.hpp>
#include <boost/foreach.hpp>
#include <boost/format.hpp>
#include <boost/lambda/lambda.hpp>
#include <boost/shared_array.hpp>

#include <gdal/cpl_error.h>

#include <newbase/NFmiIndexMask.h>
#include <newbase/NFmiIndexMaskTools.h>
#include <newbase/NFmiPoint.h>
#include <newbase/NFmiQueryData.h>
#include <newbase/NFmiSvgPath.h>
#include <newbase/NFmiSvgTools.h>

#include <macgyver/StringConversion.h>
#include <macgyver/TypeName.h>

#include <smartmet/engines/gis/GdalUtils.h>
#include <smartmet/engines/querydata/MetaQueryOptions.h>
#include <smartmet/spine/Convenience.h>
#include <smartmet/spine/Exception.h>
#include <smartmet/spine/ParameterFactory.h>
#include <smartmet/spine/TimeSeriesOutput.h>

#include "AreaUtils.h"
#include "FeatureID.h"
#include "StoredQueryHandlerFactoryDef.h"
#include "WfsConvenience.h"
#include "stored_queries/StoredGridQueryHandler.h"

namespace ts = SmartMet::Spine::TimeSeries;
namespace bw = SmartMet::Plugin::WFS;
namespace ba = boost::algorithm;
namespace bl = boost::lambda;
namespace pt = boost::posix_time;
namespace bg = boost::geometry;
using namespace SmartMet::Spine;

using boost::format;
using boost::str;
using bw::StoredGridQueryHandler;
using SmartMet::Spine::CaseInsensitiveLess;
using SmartMet::Spine::str_iequal;

namespace qe = SmartMet::Engine::Querydata;

namespace
{
const char* P_PRODUCER = "producer";
const char* P_ORIGIN_TIME = "originTime";
const char* P_PARAM = "param";
const char* P_LEVEL = "level";
const char* P_LEVEL_TYPE = "levelType";
const char* P_LEVEL_VALUE = "levelValue";
const char* P_MISSING_TEXT = "missingText";
const char* P_SCALE_FACTOR = "scaleFactor";
const char* P_PRECISION = "precision";
const char* P_DATASTEP = "datastep";
const char* P_DATA_CRS = "dataCRS";

const char* QENGINE_CRS = "EPSG::4326";
}  // namespace

StoredGridQueryHandler::StoredGridQueryHandler(SmartMet::Spine::Reactor* reactor,
                                               boost::shared_ptr<StoredQueryConfig> config,
                                               PluginData& plugin_data,
                                               boost::optional<std::string> template_file_name)
    : SupportsExtraHandlerParams(config, false),
      StoredQueryHandlerBase(reactor, config, plugin_data, template_file_name),
      SupportsBoundingBox(config, plugin_data.get_crs_registry(), false),
      SupportsTimeParameters(config),
      geo_engine(nullptr),
      q_engine(nullptr),
      debug_level(get_config()->get_debug_level())
{
  try
  {
    register_scalar_param<std::string>(P_PRODUCER, *config);
    register_scalar_param<pt::ptime>(P_ORIGIN_TIME, *config, false);
    register_array_param<std::string>(P_PARAM, *config);
    register_scalar_param<std::string>(P_LEVEL_TYPE, *config, false);
    register_array_param<double>(P_LEVEL_VALUE, *config, 0);
    register_scalar_param<std::string>(P_MISSING_TEXT, *config);
    register_scalar_param<std::string>(P_DATA_CRS, *config, true);
    register_scalar_param<unsigned long>(P_SCALE_FACTOR, *config, false);
    register_scalar_param<unsigned long>(P_PRECISION, *config, false);
    register_scalar_param<unsigned long>(P_DATASTEP, *config, false);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

StoredGridQueryHandler::~StoredGridQueryHandler() {}

void StoredGridQueryHandler::init_handler()
{
  try
  {
    auto* reactor = get_reactor();

    void* engine;

    engine = reactor->getSingleton("Geonames", nullptr);
    if (engine == nullptr)
      throw SmartMet::Spine::Exception(BCP, "No Geonames engine available");

    geo_engine = reinterpret_cast<SmartMet::Engine::Geonames::Engine*>(engine);

    engine = reactor->getSingleton("Querydata", nullptr);
    if (engine == nullptr)
      throw SmartMet::Spine::Exception(BCP, "No Querydata engine available");

    q_engine = reinterpret_cast<SmartMet::Engine::Querydata::Engine*>(engine);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

StoredGridQueryHandler::Query::Query()
    : missing_text("nan"),
      language("lan"),
      value_formatter(new SmartMet::Spine::ValueFormatter(SmartMet::Spine::ValueFormatterParam())),
      top_left(NFmiPoint::gMissingLatlon),
      top_right(NFmiPoint::gMissingLatlon),
      bottom_left(NFmiPoint::gMissingLatlon),
      bottom_right(NFmiPoint::gMissingLatlon),
      find_nearest_valid_point(false)

{
  try
  {
    output_locale.reset(new std::locale("fi_FI.utf8"));
    time_formatter.reset(Fmi::TimeFormatter::create("iso"));
    value_formatter->setMissingText(missing_text);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

StoredGridQueryHandler::Query::~Query() {}

namespace
{
double comparisonDistance(const NFmiPoint& first, const NFmiPoint& second)
{
  try
  {
    double result = ((first.X() - second.X()) * (first.X() - second.X())) +
                    ((first.Y() - second.Y()) * (first.Y() - second.Y()));

    return result;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

std::string formatToScaledInteger(double input, unsigned long digits, unsigned long precision)
{
  try
  {
    std::ostringstream os;

    os << std::setprecision(precision) << std::setiosflags(std::ios::fixed) << input * digits;

    return os.str();
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

ts::TimeSeriesGroupPtr makeSparseGrid(const ts::TimeSeriesGroupPtr& inputGrid,
                                      unsigned long step,
                                      const std::pair<unsigned, unsigned>& extents)
{
  try
  {
    ts::TimeSeriesGroupPtr result(new ts::TimeSeriesGroup);

    unsigned long index = 0;
    unsigned long xindex;
    unsigned long yindex;
    for (auto locationIt = inputGrid->begin(); locationIt != inputGrid->end();
         ++locationIt, ++index)
    {
      xindex = index / extents.first;
      yindex = index % extents.first;

      if (xindex % step == 0 && yindex % step == 0)
      {
        result->push_back(ts::LonLatTimeSeries(locationIt->lonlat, locationIt->timeseries));
      }
    }

    return result;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

#ifdef OLD_IMPL
boost::any makeSparseGrid(const boost::any& inputGrid,
                          unsigned long step,
                          const std::pair<unsigned, unsigned>& extents)
{
  typedef std::vector<boost::any> AnyVector;

  AnyVector result;

  auto valVector = boost::any_cast<AnyVector>(inputGrid);

  for (auto timeIt = valVector.begin(); timeIt != valVector.end(); ++timeIt)
  {
    AnyVector area;
    AnyVector dataVec;

    auto placeVec = boost::any_cast<std::vector<boost::any> >(*timeIt);

    auto locationVec = boost::any_cast<std::vector<boost::any> >(
        *(placeVec.begin()));  // Only one area, this is sufficient

    unsigned long index = 0;
    unsigned long xindex;
    unsigned long yindex;
    for (auto innerIt = locationVec.begin(); innerIt != locationVec.end(); ++innerIt, ++index)
    {
      xindex = index / extents.first;
      yindex = index % extents.first;

      if (xindex % step == 0 && yindex % step == 0)
      {
        dataVec.push_back(*innerIt);
      }
    }

    area.push_back(dataVec);

    result.push_back(area);
  }

  return result;
}
#endif

// make rectangle nfmisvgpath
void make_rectangle_path(NFmiSvgPath& thePath,
                         const std::pair<double, double>& theFirstCorner,
                         const std::pair<double, double>& theSecondCorner)
{
  try
  {
    std::pair<double, double> point1(theFirstCorner);
    std::pair<double, double> point2(theFirstCorner.first, theSecondCorner.second);
    std::pair<double, double> point3(theSecondCorner);
    std::pair<double, double> point4(theSecondCorner.first, theFirstCorner.second);

    NFmiSvgPath::Element element1(NFmiSvgPath::kElementMoveto, point1.first, point1.second);
    NFmiSvgPath::Element element2(NFmiSvgPath::kElementLineto, point2.first, point2.second);
    NFmiSvgPath::Element element3(NFmiSvgPath::kElementLineto, point3.first, point3.second);
    NFmiSvgPath::Element element4(NFmiSvgPath::kElementLineto, point4.first, point4.second);
    NFmiSvgPath::Element element5(NFmiSvgPath::kElementClosePath, point1.first, point1.second);
    thePath.push_back(element1);
    thePath.push_back(element2);
    thePath.push_back(element3);
    thePath.push_back(element4);
    thePath.push_back(element5);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void make_rectangle_path(NFmiSvgPath& thePath,
                         const NFmiPoint& theFirstCorner,
                         const NFmiPoint& theSecondCorner)
{
  try
  {
    std::pair<double, double> first = std::make_pair(theFirstCorner.X(), theFirstCorner.Y());
    std::pair<double, double> second = std::make_pair(theSecondCorner.X(), theSecondCorner.Y());

    make_rectangle_path(thePath, first, second);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

template <typename ValueType, typename DestType>
void handle_opt_param(const bw::RequestParameterMap& params,
                      const std::string& name,
                      DestType& dest,
                      void (DestType::*setter)(const ValueType&))
{
  try
  {
    if (params.count(name))
    {
      ValueType value = params.get_single<ValueType>(name);
      (dest.*setter)(value);
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

template <typename ValueType, typename DestType, typename Setter>
void handle_array_param(const bw::RequestParameterMap& params,
                        const std::string& name,
                        DestType& dest,
                        Setter setter)
{
  try
  {
    std::list<ValueType> tmp;
    params.get<ValueType>(name, std::back_inserter(tmp));
    for (auto it = tmp.begin(); it != tmp.end(); ++it)
    {
      (dest.*setter)(*it);
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

#if 0
void dump_meta_query_options(const qe::MetaQueryOptions& opt)
{
  try
  {
    std::cout << "MetaQueryOptions:";
    if (opt.hasProducer())
      std::cout << " producer='" << opt.getProducer() << "'";

    if (opt.hasOriginTime())
      std::cout << " originTime='" << pt::to_simple_string(opt.getOriginTime()) << "'";

    if (opt.hasFirstTime())
      std::cout << " firstTime='" << pt::to_simple_string(opt.getFirstTime()) << "'";

    if (opt.hasLastTime())
      std::cout << " lastTime='" << pt::to_simple_string(opt.getLastTime()) << "'";

    if (opt.hasParameters())
    {
      std::cout << " parameters=[";
      BOOST_FOREACH (const auto& param, opt.getParameters())
      {
        std::cout << "'" << param << "' ";
      }
      std::cout << "]";
    }

    if (opt.hasBoundingBox())
    {
      const auto bbox = opt.getBoundingBox();
      std::cout << " boundingBox=((" << bbox.ul << ") (" << bbox.ur << ") (" << bbox.br << ") ("
                << bbox.bl << "))";
    }

    if (opt.hasLevelTypes())
    {
      std::string sep = "";
      const auto level_types = opt.getLevelTypes();
      std::cout << " levelTypes=[";
      BOOST_FOREACH (const auto& item, level_types)
      {
        std::cout << sep << "'" << item << "'";
        sep = " ";
      }
      std::cout << "]";
    }

    if (opt.hasLevelValues())
    {
      std::string sep = "";
      const auto level_values = opt.getLevelValues();
      std::cout << " levelValues=[";
      BOOST_FOREACH (const auto& item, level_values)
      {
        std::cout << sep << item;
        sep = " ";
      }
      std::cout << "]";
    }
    std::cout << std::endl;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}
#endif

}  // namespace

std::pair<unsigned int, unsigned int> StoredGridQueryHandler::getDataIndexExtents(
    const SmartMet::Spine::TimeSeries::TimeSeriesGroupPtr& longitudes,
    const SmartMet::Spine::TimeSeries::TimeSeriesGroupPtr& latitudes,
    const Query& query,
    const std::string& dataCrs) const
{
  try
  {
    std::pair<unsigned int, unsigned int> result;

    if (longitudes->empty())
    {
      std::ostringstream msg;
      msg << " No data available for producer '" << query.producer_name << "'";
      SmartMet::Spine::Exception exception(BCP, msg.str());
      exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PROCESSING_FAILED);
      throw exception;
    }

    if (latitudes->empty())
    {
      std::ostringstream msg;
      msg << " No data available for producer '" << query.producer_name << "'";
      SmartMet::Spine::Exception exception(BCP, msg.str());
      exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PROCESSING_FAILED);
      throw exception;
    }

    std::vector<double> locationVecLon;
    std::vector<double> locationVecLat;

    BOOST_FOREACH (const auto& item, *longitudes)
      locationVecLon.push_back(item.lonlat.lon);
    BOOST_FOREACH (const auto& item, *latitudes)
      locationVecLat.push_back(item.lonlat.lat);

    double firstLon = locationVecLon[0];
    auto lonIter = locationVecLon.begin();
    lonIter++;

    auto firstLat = locationVecLat[0];
    auto latIter = locationVecLat.begin();
    latIter++;

    if (lonIter == locationVecLon.end() || latIter == locationVecLat.end())
    {
      std::ostringstream msg;
      msg << " Empty result grid, check data extents";
      SmartMet::Spine::Exception exception(BCP, msg.str());
      exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PROCESSING_FAILED);
      throw exception;
    }

    NFmiPoint firstPoint(firstLon, firstLat);
    NFmiPoint secondPoint(*lonIter, *latIter);

    double compDistance = ::comparisonDistance(firstPoint, secondPoint) *
                          1.1;  // A little bit of slack, just to be safe

    ++lonIter;
    ++latIter;

    auto previousPoint = secondPoint;

    while (lonIter != locationVecLon.end() && latIter != locationVecLat.end())
    {
      NFmiPoint thisPoint(*lonIter, *latIter);

      double dist = ::comparisonDistance(previousPoint, thisPoint);

      if (dist > compDistance)  // We have jumped to the next row in data
      {
        break;
      }
      else
      {
        lonIter++;
        latIter++;
        previousPoint = thisPoint;
      }
    }

    result.first = lonIter - locationVecLon.begin();
    result.second = locationVecLon.size() / result.first;

    return result;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

#ifdef OLD_IMPL
std::pair<unsigned int, unsigned int> StoredGridQueryHandler::getDataIndexExtents(
    const boost::any& longitudes,
    const boost::any& latitudes,
    const Query& query,
    const std::string& dataCrs) const
{
  std::pair<unsigned int, unsigned int> result;

  auto valVectorLon = boost::any_cast<std::vector<boost::any> >(longitudes);

  if (valVectorLon.empty())
  {
    std::ostringstream msg;
    msg << " No data available for producer '" << query.producer_name << "'";
    SmartMet::Spine::Exception exception(BCP, msg.str());
    exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PROCESSING_FAILED);
    throw exception;
  }

  auto valVectorLat = boost::any_cast<std::vector<boost::any> >(latitudes);

  if (valVectorLat.empty())
  {
    std::ostringstream msg;
    msg << " No data available for producer '" << query.producer_name << "'";
    SmartMet::Spine::Exception exception(BCP, msg.str());
    exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PROCESSING_FAILED);
    throw exception;
  }

  auto timeItLon = valVectorLon.begin();
  auto placeVecLon = boost::any_cast<std::vector<boost::any> >(*timeItLon);
  auto locationVecLon = boost::any_cast<std::vector<boost::any> >(
      *(placeVecLon.begin()));  // Only one area, this is sufficient
  auto firstLon = boost::any_cast<double>(*(locationVecLon.begin()));
  auto lonIter = locationVecLon.begin();
  lonIter++;

  auto timeItLat = valVectorLat.begin();
  auto placeVecLat = boost::any_cast<std::vector<boost::any> >(*timeItLat);
  auto locationVecLat = boost::any_cast<std::vector<boost::any> >(
      *(placeVecLat.begin()));  // Only one area, this is sufficient
  auto firstLat = boost::any_cast<double>(*(locationVecLat.begin()));
  auto latIter = locationVecLat.begin();
  latIter++;

  if (lonIter == locationVecLon.end() || latIter == locationVecLat.end())
  {
    std::ostringstream msg;
    msg << " Empty result grid, check data extents";
    SmartMet::Spine::Exception exception(BCP, msg.str());
    exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PROCESSING_FAILED);
    throw exception;
  }

  NFmiPoint firstPoint(firstLon, firstLat);
  NFmiPoint secondPoint(boost::any_cast<double>(*lonIter), boost::any_cast<double>(*latIter));

  double compDistance = ::comparisonDistance(firstPoint, secondPoint) *
                        1.1;  // A little bit of slack, just to be safe

  ++lonIter;
  ++latIter;

  auto previousPoint = secondPoint;

  while (lonIter != locationVecLon.end() && latIter != locationVecLat.end())
  {
    NFmiPoint thisPoint(boost::any_cast<double>(*lonIter), boost::any_cast<double>(*latIter));

    double dist = ::comparisonDistance(previousPoint, thisPoint);

    if (dist > compDistance)  // We have jumped to the next row in data
    {
      break;
    }
    else
    {
      lonIter++;
      latIter++;
      previousPoint = thisPoint;
    }
  }

  result.first = lonIter - locationVecLon.begin();
  result.second = locationVecLon.size() / result.first;

  return result;
}
#endif

StoredGridQueryHandler::Result::Grid StoredGridQueryHandler::rearrangeGrid(
    const Result::Grid& inputGrid, int arrayWidth) const
{
  try
  {
    // In-place rearrange would be nicer

    Result::Grid orderedGrid(inputGrid.size());

    auto outputIt = orderedGrid.end();

    std::advance(outputIt, -arrayWidth);

    unsigned stepsSoFar = 0;

    if (inputGrid.size() % arrayWidth != 0)
    {
      std::ostringstream os;
      os << "Array width '" << arrayWidth
         << "' is not multiple of data size, data rearrange is impossible";
      throw SmartMet::Spine::Exception(BCP, os.str());
    }

    while (true)
    {
      // If array dimensions change (i.e. queried bbox is in different projection than data, the
      // following segfaults

      outputIt = std::copy(inputGrid.begin() + arrayWidth * stepsSoFar,
                           inputGrid.begin() + arrayWidth * (stepsSoFar + 1),
                           outputIt);

      if (outputIt == orderedGrid.begin() + arrayWidth)
      {
        break;
      }

      std::advance(outputIt, -2 * arrayWidth);
      stepsSoFar++;
    }

    return orderedGrid;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

std::map<std::string, SmartMet::Engine::Querydata::ModelParameter>
SmartMet::Plugin::WFS::StoredGridQueryHandler::get_model_parameters(
    const std::string& producer, const pt::ptime& origin_time) const
{
  try
  {
    using SmartMet::Engine::Querydata::MetaData;
    using SmartMet::Engine::Querydata::MetaQueryOptions;

    MetaQueryOptions qopt;
    qopt.setProducer(producer);
    qopt.setOriginTime(origin_time);
    std::list<MetaData> meta_info = q_engine->getEngineMetadata(qopt);
    std::map<std::string, SmartMet::Engine::Querydata::ModelParameter> result;
    BOOST_FOREACH (const auto& item, meta_info)
    {
      // std::cout << "### " << item.producer << std::endl;
      BOOST_FOREACH (const auto& param, item.parameters)
      {
        // std::cout << "=== " << param.name << " : " << param.precision << " : "
        //          << param.description << std::endl;
        result.insert(std::make_pair(Fmi::ascii_tolower_copy(param.name), param));
      }
    }

    return result;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void StoredGridQueryHandler::parse_levels(
    const bw::RequestParameterMap& params,
    SmartMet::Plugin::WFS::StoredGridQueryHandler::Query& dest) const
{
  try
  {
    dest.levels.clear();

    dest.level_type = params.get_single<std::string>(P_LEVEL_TYPE);

    std::vector<int64_t> levels;
    params.get<int64_t>(P_LEVEL, std::back_inserter(levels));
    BOOST_FOREACH (int64_t level, levels)
    {
      int tmp = cast_int_type<int>(level);
      if (!dest.levels.insert(tmp).second)
      {
        SmartMet::Spine::Exception exception(BCP, "Duplicate level '" + std::to_string(tmp) + "'!");
        exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PARSING_FAILED);
        throw exception;
      }
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void StoredGridQueryHandler::parse_times(const RequestParameterMap& param, Query& dest) const
{
  try
  {
    dest.toptions = get_time_generator_options(param);

    // boost::optional + gcc-4.4.X käytäytyy huonosti
    if (param.count(P_ORIGIN_TIME) > 0)
      dest.origin_time.reset(new pt::ptime(param.get_single<pt::ptime>(P_ORIGIN_TIME)));
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void StoredGridQueryHandler::parse_params(const RequestParameterMap& param, Query& dest) const
{
  try
  {
    using SmartMet::Spine::Parameter;
    using SmartMet::Spine::ParameterFactory;

    std::vector<std::string> names;
    param.get<std::string>(P_PARAM, std::back_inserter(names));
    BOOST_FOREACH (const std::string& name, names)
    {
      std::size_t ind = dest.data_params.size();
      dest.data_params.push_back(ParameterFactory::instance().parse(name));
      if (dest.first_data_ind == 0)
        dest.first_data_ind = ind;
      dest.last_data_ind = ind;
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

StoredGridQueryHandler::Result StoredGridQueryHandler::extract_forecast(
    const RequestParameterMap& params, Query& query, const std::string& dataCrs) const
{
  try
  {
    using namespace SmartMet;

    int debug_level = get_config()->get_debug_level();

    double lon1, lat1;

    // Get the bbox for corner (should be middle) coordinates

    lon1 = query.requested_bbox.xMax;
    lat1 = query.requested_bbox.yMax;

    SmartMet::Spine::LocationPtr loc = geo_engine->lonlatSearch(lon1, lat1, query.language);

    const std::string place = loc->name;
    const std::string country = geo_engine->countryName(loc->iso2, query.language);
    if (debug_level > 0)
    {
      std::cout << "Location: " << loc->name << " in " << country << std::endl;
    }

    SmartMet::Engine::Querydata::Producer producer = params.get_single<std::string>(P_PRODUCER);

    if (debug_level > 0)
    {
      std::cout << "Selected producer: " << producer << std::endl;
    }

    auto model =
        (query.origin_time ? q_engine->get(producer, *query.origin_time) : q_engine->get(producer));

    if (model->isGrid())
    {
      const auto grid_info = model->grid();
      auto* model_area = grid_info.Area();
      if (!model_area)
      {
        SmartMet::Spine::Exception exception(
            BCP, "Attempted to use producer with no Area specified: " + producer);
        exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PROCESSING_FAILED);
        throw exception;
      }
    }
    else
    {
      SmartMet::Spine::Exception exception(
          BCP, "Attempted to use producer which has no grid specified: " + producer);
      exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PROCESSING_FAILED);
      throw exception;
    }

    NFmiPoint nearestpoint(kFloatMissing, kFloatMissing);

    if (debug_level > 0)
    {
      std::cout << "Producer : " << producer << std::endl;
    }

    query.producer_name = producer;

    const std::string zone = "UTC";

    query.toptions->setDataTimes(model->validTimes(), model->isClimatology());
    if (debug_level > 2)
    {
      std::cout << *query.toptions << "\n";
    }

    auto tz = geo_engine->getTimeZones().time_zone_from_string(zone);
    auto tlist = SmartMet::Spine::TimeSeriesGenerator::generate(*query.toptions, tz);

    if (debug_level > 2)
    {
      std::cout << __FILE__ << "#" << __LINE__ << ": generated times:";
      BOOST_FOREACH (const auto& t, tlist)
      {
        std::cout << " '" << t << "'";
      }
      std::cout << std::endl;
    }

    const int default_prec = 6;
    const auto param_map = get_model_parameters(producer, model->originTime());
    std::map<std::string, int> param_precision_map;
    BOOST_FOREACH (const Parameter& param, query.data_params)
    {
      const std::string& name = param.name();
      auto pos = param_map.find(Fmi::ascii_tolower_copy(name));
      if (pos == param_map.end())
      {
        param_precision_map[name] = default_prec;
      }
      else
      {
        param_precision_map[name] = pos->second.precision;
      }
    }

    // Build NFmiSvgPath for Querydata
    NFmiIndexMask mask;
    NFmiSvgPath bbox_path;

    NFmiPoint firstPoint(query.requested_bbox.xMin, query.requested_bbox.yMin);
    NFmiPoint secondPoint(query.requested_bbox.xMax, query.requested_bbox.yMax);

    make_rectangle_path(bbox_path, firstPoint, secondPoint);
    mask = NFmiIndexMaskTools::MaskExpand(model->grid(), bbox_path, loc->radius);

    // Result is set here
    Result theResult;

    // Times
    BOOST_FOREACH (auto& time, tlist)
    {
      theResult.timesteps.push_back(time.local_time());
    }

    // Get the given grid step

    unsigned long data_step = params.get_single<unsigned long>(P_DATASTEP);

    if (data_step < 1)
    {
      std::ostringstream msg;
      msg << "Datastep parameter must be > 0";
      SmartMet::Spine::Exception exception(BCP, msg.str());
      exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PROCESSING_FAILED);
      throw exception;
    }

    // Get result array dimensions, CURRENTLY ONLY WORK ON LATLON DATA

    auto secondIt = tlist.begin();
    std::advance(secondIt, 1);

    TimeSeriesGenerator::LocalTimeList oneTimeStep(tlist.begin(), secondIt);

    auto lon = ParameterFactory::instance().parse("longitude");

    auto lat = ParameterFactory::instance().parse("latitude");

    // Add these so they will be returned to caller
    if (query.includeDebugData)
    {
      query.data_params.push_back(lon);
      query.data_params.push_back(lat);
    }

    SmartMet::Engine::Querydata::ParameterOptions qengine_lonparam(lon,
                                                                   producer,
                                                                   *loc,
                                                                   country,
                                                                   place,
                                                                   *query.time_formatter,
                                                                   "",
                                                                   query.language,
                                                                   *query.output_locale,
                                                                   loc->timezone,
                                                                   query.find_nearest_valid_point,
                                                                   nearestpoint,
                                                                   query.lastpoint);

    auto longitudeData = model->values(qengine_lonparam, mask, oneTimeStep);

    SmartMet::Engine::Querydata::ParameterOptions qengine_latparam(lat,
                                                                   producer,
                                                                   *loc,
                                                                   country,
                                                                   place,
                                                                   *query.time_formatter,
                                                                   "",
                                                                   query.language,
                                                                   *query.output_locale,
                                                                   loc->timezone,
                                                                   query.find_nearest_valid_point,
                                                                   nearestpoint,
                                                                   query.lastpoint);

    auto latitudeData = model->values(qengine_latparam, mask, oneTimeStep);

    auto extents = getDataIndexExtents(longitudeData, latitudeData, query, dataCrs);

    longitudeData = makeSparseGrid(longitudeData, data_step, extents);
    latitudeData = makeSparseGrid(latitudeData, data_step, extents);

    // MakeSparseGrid may have modified the extents, check them here again
    auto resulting_extents = getDataIndexExtents(longitudeData, latitudeData, query, dataCrs);

    theResult.xdim = resulting_extents.first;
    theResult.ydim = resulting_extents.second;

    std::vector<double> locationVecLon;
    std::vector<double> locationVecLat;
    BOOST_FOREACH (const auto& item, *longitudeData)
      locationVecLon.push_back(item.lonlat.lon);
    BOOST_FOREACH (const auto& item, *latitudeData)
      locationVecLat.push_back(item.lonlat.lat);

    // Set the corresponding geographical coordinates
    if (locationVecLon.size() > 1 && locationVecLat.size() > 1)
    {
      theResult.ll_lon = locationVecLon[0];
      theResult.ur_lon = locationVecLon[locationVecLon.size() - 1];

      theResult.ll_lat = locationVecLat[0];
      theResult.ur_lat = locationVecLat[locationVecLat.size() - 1];
    }
    else
    {
      std::ostringstream msg;
      msg << "Grid sizes of less than 2 are not supported";
      SmartMet::Spine::Exception exception(BCP, msg.str());
      exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PROCESSING_FAILED);
      throw exception;
    }

    for (model->resetLevel(); model->nextLevel();)
    {
      if (query.levels.empty() ||
          query.levels.count(static_cast<int>(model->level().LevelValue())) > 0)
      {
        theResult.dataLevels.push_back(Result::LevelData());
        auto& thisLevel = theResult.dataLevels.back();

        BOOST_FOREACH (const Parameter& param, query.data_params)
        {
          // Get the metadata infos if not already in the vector
          auto paramInfo = std::make_pair(param.name(), param.number());
          if (std::find(theResult.paramInfos.begin(), theResult.paramInfos.end(), paramInfo) ==
              theResult.paramInfos.end())
          {
            theResult.paramInfos.push_back(paramInfo);
          }

          using SmartMet::Spine::Parameter;

          // Build index mask for the bbox area

          SmartMet::Engine::Querydata::ParameterOptions qengine_param(
              param,
              producer,
              *loc,
              country,
              place,
              *query.time_formatter,
              "",
              query.language,
              *query.output_locale,
              loc->timezone,
              query.find_nearest_valid_point,
              nearestpoint,
              query.lastpoint);

          auto val = model->values(qengine_param, mask, tlist);

          val = makeSparseGrid(val, data_step, extents);

          Result::ParamTimeSeries result;

          unsigned int timesteps(val->size() == 0 ? 0 : val->begin()->timeseries.size());

          for (unsigned int i = 0; i < timesteps; i++)
          {
            Result::Grid thisXYGrid;
            thisXYGrid.reserve(val->size());

            for (auto locationIt = val->begin(); locationIt != val->end(); ++locationIt)
            {
              auto tsValue = locationIt->timeseries[i].value;

              if (boost::get<SmartMet::Spine::TimeSeries::None>(&(tsValue)))
              {
                thisXYGrid.emplace_back(query.missing_text);
              }
              else if (!(boost::get<double>(&(tsValue))))
              {
                std::stringstream ss;
                ss << tsValue;
                thisXYGrid.emplace_back(ss.str());
              }
              else
              {
                double result = boost::get<double>(tsValue);

                thisXYGrid.emplace_back(
                    formatToScaledInteger(result, query.scaleFactor, query.precision));
              }
            }

            auto rearranged = rearrangeGrid(thisXYGrid, resulting_extents.first);
            result.emplace_back(rearranged);
          }
          thisLevel.push_back(std::move(result));
        }
      }
    }

    return theResult;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

SmartMet::Engine::Querydata::Producer StoredGridQueryHandler::select_producer(
    const SmartMet::Spine::Location& location, const Query& query) const
{
  try
  {
    SmartMet::Engine::Querydata::Producer producer;

    if (query.models.empty())
    {
      producer = q_engine->find(location.longitude, location.latitude);
    }
    else
    {
      producer = q_engine->find(query.models, location.longitude, location.latitude);
    }

    return producer;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void StoredGridQueryHandler::query(const StoredQuery& stored_query,
                                   const std::string& language,
                                   std::ostream& output) const
{
  try
  {
    namespace pt = boost::posix_time;
    using namespace SmartMet;
    using namespace SmartMet::Plugin::WFS;

    Query query;
    query.language = language;

    int debug_level = get_config()->get_debug_level();
    if (debug_level > 0)
      stored_query.dump_query_info(std::cout);

    const RequestParameterMap& params = stored_query.get_param_map();

    try
    {
      query.missing_text = params.get_single<std::string>(P_MISSING_TEXT);
      parse_levels(params, query);
      parse_params(params, query);
      query.time_formatter.reset(Fmi::TimeFormatter::create("iso"));
      parse_times(params, query);

      unsigned long scaleFactor = 1;
      auto hasScale = params.get_optional<unsigned long>(P_SCALE_FACTOR);
      if (hasScale)
      {
        scaleFactor = *hasScale;
      }
      query.scaleFactor = scaleFactor;

      unsigned long precision = 1;
      auto hasPrecision = params.get_optional<unsigned long>(P_PRECISION);
      if (hasPrecision)
      {
        precision = *hasPrecision;
      }
      query.precision = precision;

      SmartMet::Spine::ValueFormatterParam vf_param;
      vf_param.missingText = query.missing_text;
      query.value_formatter.reset(new SmartMet::Spine::ValueFormatter(vf_param));

      const std::string dataCrs = params.get_single<std::string>(P_DATA_CRS);

      // Get bounding box information, should be given in data CRS for rectangular array
      SmartMet::Spine::BoundingBox requested_bbox;
      bool have_bbox = get_bounding_box(params, dataCrs, &requested_bbox);
      if (have_bbox)
      {
        if (debug_level > 1)
        {
          SmartMet::Spine::Value v_bbox0(requested_bbox);
          std::ostringstream msg;
          msg << SmartMet::Spine::log_time_str() << ": [WFS] [DEBUG] ["
              << get_config()->get_query_id() << "] requested_bbox=" << v_bbox0 << '\n';
          std::cout << msg.str() << std::flush;
        }
      }
      else
      {
        // Bounding box is mandatory for Grid Stored queries
        std::ostringstream msg;
        msg << "No bounding box specified.";
        SmartMet::Spine::Exception exception(BCP, msg.str());
        exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PROCESSING_FAILED);
        throw exception;
      }

      // Make sure that given CRS is same as the data CRS, this is needed to produce rectangular
      // grids

      if (dataCrs != requested_bbox.crs)
      {
        std::ostringstream msg;
        msg << "Bounding box must be given in data CRS: " << dataCrs;
        SmartMet::Spine::Exception exception(BCP, msg.str());
        exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PROCESSING_FAILED);
        throw exception;
      }

      // Transform the bounding box to latlon for Qengine

      auto transformedBox = transform_bounding_box(requested_bbox, QENGINE_CRS);
      query.requested_bbox =
          requested_bbox;  // store the original queried box, not the latlon-converted one

      query.includeDebugData = stored_query.get_use_debug_format();

      query.result = extract_forecast(params, query, dataCrs);
      if (query.result.timesteps.empty())
      {
        SmartMet::Spine::Exception exception(BCP, "Empty result!");
        exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PROCESSING_FAILED);
        throw exception;
      }

      CTPP::CDT hash;

      hash["language"] = language;
      hash["responseTimestamp"] =
          Fmi::to_iso_extended_string(get_plugin_data().get_time_stamp()) + "Z";
      hash["fmi_apikey"] = bw::QueryBase::FMI_APIKEY_SUBST;
      hash["fmi_apikey_prefix"] = bw::QueryBase::FMI_APIKEY_PREFIX_SUBST;
      hash["hostname"] = QueryBase::HOSTNAME_SUBST;
      hash["protocol"] = QueryBase::PROTOCOL_SUBST;
      hash["timeSteps"] = query.result.timesteps.size();
      hash["firstTime"] = Fmi::to_iso_extended_string(*query.result.timesteps.begin()) + "Z";

      hash["missingText"] = query.missing_text;

      auto lastIter = query.result.timesteps.end();
      std::advance(lastIter, -1);

      hash["lastTime"] = Fmi::to_iso_extended_string(*lastIter) + "Z";
      hash["producerName"] = query.producer_name;

      hash["scaleFactor"] = scaleFactor;

      hash["metadata"]["boundingBox"]["lowerLeft"]["lat"] = query.result.ll_lat;
      hash["metadata"]["boundingBox"]["lowerLeft"]["lon"] = query.result.ll_lon;

      hash["metadata"]["boundingBox"]["upperRight"]["lat"] = query.result.ur_lat;
      hash["metadata"]["boundingBox"]["upperRight"]["lon"] = query.result.ur_lon;

      hash["metadata"]["xDim"] = query.result.xdim;
      hash["metadata"]["yDim"] = query.result.ydim;

      std::size_t levelindex = 0;

      BOOST_FOREACH (auto& leveldata, query.result.dataLevels)
      {
        CTPP::CDT& returnArray = hash["returnArray"][levelindex];

        std::size_t paramIndex = 0;

        BOOST_FOREACH (auto& param, leveldata)
        {
          auto paramName = query.result.paramInfos[paramIndex].first;
          auto paramNumber = query.result.paramInfos[paramIndex].second;

          CTPP::CDT& thisParam = returnArray[paramName];

          thisParam["name"] = paramName;
          thisParam["newbasenum"] = paramNumber;

          std::size_t timeindex = 0;

          BOOST_FOREACH (auto& timestep, param)
          {
            CTPP::CDT& thisTime = thisParam["timeSteps"][timeindex];

            thisTime["time"] = Fmi::to_iso_extended_string(query.result.timesteps[timeindex]) + "Z";

            thisTime["param"] = paramName;

            thisTime["newbasenum"] = paramNumber;

            for (std::size_t ind = 0; ind < timestep.size(); ++ind)
            {
              thisTime["data"][ind] = timestep[ind];
            }

            ++timeindex;
          }

          ++paramIndex;
        }

        ++levelindex;
      }

      format_output(hash, output, stored_query.get_use_debug_format());
    }
    catch (...)
    {
      SmartMet::Spine::Exception exception(BCP, "Operation processing failed!", nullptr);
      if (exception.getExceptionByParameterName(WFS_EXCEPTION_CODE) == nullptr)
        exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PROCESSING_FAILED);
      exception.addParameter(WFS_LANGUAGE, language);
      throw exception;
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

namespace
{
using namespace SmartMet::Plugin::WFS;

boost::shared_ptr<SmartMet::Plugin::WFS::StoredQueryHandlerBase> wfs_stored_grid_handler_create(
    SmartMet::Spine::Reactor* reactor,
    boost::shared_ptr<StoredQueryConfig> config,
    PluginData& plugin_data,
    boost::optional<std::string> template_file_name)
{
  try
  {
    StoredQueryHandlerBase* qh =
        new StoredGridQueryHandler(reactor, config, plugin_data, template_file_name);
    boost::shared_ptr<SmartMet::Plugin::WFS::StoredQueryHandlerBase> result(qh);
    result->init_handler();
    return result;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}
}  // namespace

SmartMet::Plugin::WFS::StoredQueryHandlerFactoryDef wfs_stored_grid_handler_factory(
    &wfs_stored_grid_handler_create);

/**

@page WFS_SQ_QE_DOWNLOAD_HANDLER Stored Query handlers for Querydata data download

@section WFS_QE_DOWNLOAD_HANDLER_INTRO Introduction

This stored query handler provides supports for atom queries for grid data download from
<a href="../../dlsplugin/html/index.html">Download plugin</a>.

<table border="1">
  <tr>
     <td>Implementation</td>
     <td>SmartMet::Plugin::WFS::StoredGridQueryHandler</td>
  </tr>
  <tr>
    <td>Constructor name (for stored query configuration)</td>
    <td>@b wfs_stored_qe_download_handler_factory</td>
  </tr>
</table>

@section WFS_SQ_QE_DOWNLOAD_HANDLER_PARAMS Query handler built-in parameters

The following stored query handler parameter groups are being used by this stored query handler:
- @ref WFS_SQ_PARAM_BBOX

Additionally to parameters from these groups the following parameters are also in use

<table border="1">

<tr>
<th>Entry name</th>
<th>Type</th>
<th>Data type</th>
<th>Description</th>
</tr>

<tr>
  <td>producer</td>
  <td>@ref WFS_CFG_ARRAY_PARAM_TMPL (size 0 or 1)</td>
  <td>string</td>
  <td>Specifies which models to use</td>
</tr>

<tr>
  <td>originTime</td>
  <td>@ref WFS_CFG_ARRAY_PARAM_TMPL (size 0 or 1)</td>
  <td>time</td>
  <td>Model origin time/td>
</tr>

<tr>
  <td>beginTime</td>
  <td>@ref WFS_CFG_ARRAY_PARAM_TMPL (size 0 or 1)</td>
  <td>time</td>
  <td>Specifies the time of the begin of time interval when present</td>
</tr>

<tr>
  <td>endTime</td>
  <td>@ref WFS_CFG_ARRAY_PARAM_TMPL (size 0 or 1)</td>
  <td>time</td>
  <td>Specifies the end of time interval when present</td>
</tr>

<tr>
  <td>fullInterval</td>
  <td>@ref WFS_CFG_SCALAR_PARAM_TMPL</td>
  <td>integer</td>
  <td>If non zero then full specified time interval must be present
      in model data for model to be used. Otherwise overlapping is
       sufficient</td>
</tr>

<tr>
  <td>meteoParam</td>
  <td>@ref WFS_CFG_ARRAY_PARAM_TMPL</td>
  <td>string</td>
  <td>Provides meteo parameters to query</td>
</tr>

<tr>
  <td>levelType</td>
  <td>@ref WFS_CFG_ARRAY_PARAM_TMPL</td>
  <td>string</td>
  <td>Provides model level types to query. Empty array causes all
      available level types to be used</td>
</tr>

<tr>
  <td>levelValue</td>
  <td>@ref WFS_CFG_ARRAY_PARAM_TMPL</td>
  <td>double</td>
  <td>Provides level values to query. Empty array causes all
      available level values to be accepted</td>
</tr>

<tr>
  <td>format</td>
  <td>@ref WFS_CFG_ARRAY_PARAM_TMPL (size 0 or 1)</td>
  <td>string</td>
  <td>Output format to use for data queries (default @b grib2 )</td>
</tr>

<tr>
  <td>projection</td>
  <td>@ref WFS_CFG_ARRAY_PARAM_TMPL (size 0 or 1)</td>
  <td>string</td>
  <td>Coordinate projection to use</td>
</tr>

</table>

@section WFS_SQ_QE_DOWNLOAD_HANDLER_CONFIG_ENTRIES Additional configuration entries

This section describes this stored query handler configuration parameters which does not map to
stored query parameters
and must be specified on top level of stored query configuration file when present

<table border="1">

<tr>
<th>Entry name</th>
<th>Type</th>
<th>Use</th>
<th>Description</th>
</tr>

<tr>
  <td>maxHours</td>
  <td>double</td>
  <td>optional</td>
  <td>Specifies maximal permitted time interval in hours. The default value is 168 hours (7
days)</td>
</tr>

<tr>
  <td>producers</td>
  <td>array of string</td>
  <td>optional (default empty array)</td>
  <td>Specifies supported producers. All producers available to Querydata are supported if
      an empty array is provided</td>
</tr>

<tr>
  <td>url_template</td>
  <td>@ref WFS_CFG_URL_TEMPLATE "cfgUrlTemplate"</td>
  <td>mandatory</td>
  <td>Specifies a template for generating data download URL</td>
</tr>

*/
