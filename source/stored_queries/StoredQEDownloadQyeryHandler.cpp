#include "AreaUtils.h"
#include "FeatureID.h"
#include "StoredQueryHandlerFactoryDef.h"
#include "WfsConvenience.h"
#include "stored_queries/StoredQEDownloadQueryHandler.h"
#include <boost/algorithm/string.hpp>
#include <boost/bind.hpp>
#include <boost/date_time/posix_time/ptime.hpp>
#include <boost/format.hpp>
#include <boost/lambda/lambda.hpp>
#include <boost/shared_array.hpp>
#include <gdal/cpl_error.h>
#include <macgyver/StringConversion.h>
#include <macgyver/TypeName.h>
#include <newbase/NFmiQueryData.h>
#include <smartmet/engines/geonames/Engine.h>
#include <smartmet/engines/gis/GdalUtils.h>
#include <smartmet/engines/querydata/Engine.h>
#include <smartmet/engines/querydata/MetaQueryOptions.h>
#include <smartmet/spine/Convenience.h>
#include <smartmet/spine/Exception.h>
#include <algorithm>
#include <list>
#include <string>

namespace bw = SmartMet::Plugin::WFS;
namespace ba = boost::algorithm;
namespace bl = boost::lambda;
namespace pt = boost::posix_time;
namespace bg = boost::geometry;

using boost::format;
using boost::str;
using bw::StoredQEDownloadQueryHandler;
using SmartMet::Spine::CaseInsensitiveLess;
using SmartMet::Spine::str_iequal;

namespace qe = SmartMet::Engine::Querydata;

namespace
{
const char* P_PRODUCER = "producer";
const char* P_ORIGIN_TIME = "originTime";
const char* P_BEGIN = "beginTime";
const char* P_FULL_INTERVAL = "fullInterval";
const char* P_END = "endTime";
const char* P_PARAM = "meteoParam";
const char* P_LEVEL_TYPE = "levelType";
const char* P_LEVEL_VALUE = "levelValue";
const char* P_FORMAT = "format";
const char* P_PROJECTION = "projection";

const char* DATA_CRS_NAME = "urn:ogc:def:crs:EPSG::4326";
}  // namespace

StoredQEDownloadQueryHandler::StoredQEDownloadQueryHandler(
    SmartMet::Spine::Reactor* reactor,
    boost::shared_ptr<StoredQueryConfig> config,
    PluginData& plugin_data,
    boost::optional<std::string> template_file_name)
    : SupportsExtraHandlerParams(config, false),
      StoredAtomQueryHandlerBase(reactor, config, plugin_data, template_file_name),
      SupportsBoundingBox(config, plugin_data.get_crs_registry(), false),
      geo_engine(nullptr),
      q_engine(nullptr),
      producers(),
      default_format("grib2"),
      debug_level(get_config()->get_debug_level())
{
  try
  {
    register_array_param<std::string>(P_PRODUCER, *config, 0, 1);
    register_array_param<pt::ptime>(P_ORIGIN_TIME, *config, 0, 1);
    register_array_param<pt::ptime>(P_BEGIN, *config, 0, 1);
    register_array_param<pt::ptime>(P_END, *config, 0, 1);
    register_scalar_param<int64_t>(P_FULL_INTERVAL, *config, false);
    register_array_param<std::string>(P_PARAM, *config);
    register_array_param<std::string>(P_LEVEL_TYPE, *config, 0);
    register_array_param<double>(P_LEVEL_VALUE, *config, 0);
    register_array_param<std::string>(P_FORMAT, *config, 0, 1);
    register_array_param<std::string>(P_PROJECTION, *config, 0, 1);

    std::vector<std::string> tmp1;
    if (config->get_config_array<std::string>("producers", tmp1))
    {
      std::copy(tmp1.begin(), tmp1.end(), std::inserter(producers, producers.begin()));
      if (debug_level > 0)
      {
        std::ostringstream msg;
        msg << SmartMet::Spine::log_time_str() << ": [WFS] [DEBUG] ["
            << get_config()->get_query_id() << "] Supported producers:";
        std::for_each(producers.begin(), producers.end(), bl::var(msg) << " '" << bl::_1 << "'");
        msg << '\n';
        std::cout << msg.str() << std::flush;
      }
    }

    // Support at least default format
    formats.insert(default_format);

    // Support configured or default formats.
    tmp1.clear();
    if (config->get_config_array<std::string>("formats", tmp1))
    {
      std::copy(tmp1.begin(), tmp1.end(), std::inserter(formats, formats.begin()));
    }
    else
    {
      // Set default formats
      std::string formatList[] = {"grib2", "grib1", "netcdf"};
      formats.insert(formatList, formatList + 3);
    }

    BOOST_FOREACH (auto format, formats)
      Fmi::ascii_tolower(format);

    if (debug_level > 0)
    {
      std::ostringstream msg;
      msg << SmartMet::Spine::log_time_str() << ": [WFS] [DEBUG] [" << get_config()->get_query_id()
          << "] Supported formats:";
      std::for_each(formats.begin(), formats.end(), bl::var(msg) << " '" << bl::_1 << "'");
      msg << '\n';
      std::cout << msg.str() << std::flush;
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

StoredQEDownloadQueryHandler::~StoredQEDownloadQueryHandler() {}

void StoredQEDownloadQueryHandler::init_handler()
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

namespace
{
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
}  // namespace

void StoredQEDownloadQueryHandler::update_parameters(
    const bw::RequestParameterMap& params,
    int seq_id,
    std::vector<boost::shared_ptr<bw::RequestParameterMap> >& result) const
{
  try
  {
    qe::MetaQueryOptions opt;

    handle_opt_param(params, P_PRODUCER, opt, &qe::MetaQueryOptions::setProducer);
    handle_opt_param(params, P_ORIGIN_TIME, opt, &qe::MetaQueryOptions::setOriginTime);

    // FIXME: provide value of strict_interval_check through parameters
    bool strict_interval_check = false;

    pt::ptime start_time;
    pt::ptime end_time;
    if (params.count(P_FULL_INTERVAL))
      strict_interval_check = params.get_single<int64_t>(P_FULL_INTERVAL) != 0;
    if (params.count(P_BEGIN))
      start_time = params.get_single<pt::ptime>(P_BEGIN);
    if (params.count(P_END))
      end_time = params.get_single<pt::ptime>(P_END);

    std::set<std::string, CaseInsensitiveLess> req_params;
    params.get<std::string>(P_PARAM, std::inserter(req_params, req_params.begin()));
    std::for_each(req_params.begin(),
                  req_params.end(),
                  boost::bind(&qe::MetaQueryOptions::addParameter, opt, ::_1));

    std::set<std::string, CaseInsensitiveLess> req_level_types;
    params.get<std::string>(P_LEVEL_TYPE, std::inserter(req_level_types, req_level_types.begin()));
    std::for_each(req_level_types.begin(),
                  req_level_types.end(),
                  boost::bind(&qe::MetaQueryOptions::addLevelType, opt, ::_1));

    std::set<float> req_level_values;
    params.get<double>(P_LEVEL_VALUE, std::inserter(req_level_values, req_level_values.begin()));
    std::for_each(req_level_values.begin(),
                  req_level_values.end(),
                  boost::bind(&qe::MetaQueryOptions::addLevelValue, opt, ::_1));

    SmartMet::Spine::BoundingBox requested_bbox;
    SmartMet::Spine::BoundingBox query_bbox;

    bool have_bbox = get_bounding_box(params, DATA_CRS_NAME, &requested_bbox);
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

    // We do not use Querydata bounding box filter
    // if (bbox.size() == 4) /* The size is either 0 or 4: it is ensured earlier. */
    //  {
    //    NFmiPoint bl(bbox.at(0), bbox.at(1));
    //    NFmiPoint ur(bbox.at(2), bbox.at(3));
    //    opt.setBoundingBox(bl, ur);
    //  }

    result.clear();

    if (debug_level > 0)
    {
      dump_meta_query_options(opt);
    }

    bw::FeatureID feature_id(get_config()->get_query_id(), params.get_map(), seq_id);

    const auto md_list = q_engine->getEngineMetadata(opt);
    for (auto md_iter = md_list.begin(); md_iter != md_list.end(); ++md_iter)
    {
      const auto& meta_info = *md_iter;

      if (not producers.empty() and producers.count(meta_info.producer) == 0)
      {
        if (debug_level > 1)
        {
          std::ostringstream msg;
          msg << SmartMet::Spine::log_time_str() << ": [WFS] [DEBUG] ["
              << get_config()->get_query_id() << "] Skipping producer '" << meta_info.producer
              << '\n';
          std::cout << msg.str() << std::flush;
        }
        // Producer set specified and found producer does not belong to it
        continue;
      }

      if (strict_interval_check)
      {
        // Entire interval must be provided in model for it to be selected
        if (not start_time.is_not_a_date_time() and not end_time.is_not_a_date_time() and
            ((meta_info.firstTime > start_time) or (meta_info.lastTime < end_time)))
        {
          if (debug_level > 1)
          {
            std::ostringstream msg;
            msg << SmartMet::Spine::log_time_str() << ": [WFS] [DEBUG] ["
                << get_config()->get_query_id() << "] Skipping model producer='"
                << meta_info.producer << "'"
                << " origintime='" << meta_info.originTime << "'"
                << " firstTime='" << meta_info.firstTime << "'"
                << " lastTime='" << meta_info.lastTime << "'"
                << " strict_interval_check=true\n";
            std::cout << msg.str() << std::flush;
          }
          continue;
        }
        else if (start_time.is_not_a_date_time() and not end_time.is_not_a_date_time())
        {
        }
        else
        {
          SmartMet::Spine::Exception exception(
              BCP, "Both or none of the parameters 'starttime' and 'endtime' must be specified!");
          exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PROCESSING_FAILED);
          throw exception;
        }
      }
      else
      {
        // It is enough that intervals overlap
        if ((not end_time.is_not_a_date_time() and (meta_info.firstTime > end_time)) or
            (not start_time.is_not_a_date_time() and (meta_info.lastTime < start_time)))
        {
          if (debug_level > 1)
          {
            std::ostringstream msg;
            msg << SmartMet::Spine::log_time_str() << ": [WFS] [DEBUG] ["
                << get_config()->get_query_id() << "] Skipping model producer='"
                << meta_info.producer << "'"
                << " origintime='" << meta_info.originTime << "'"
                << " firstTime='" << meta_info.firstTime << "'"
                << " lastTime='" << meta_info.lastTime << "'"
                << " strict_interval_check=false\n";
            std::cout << msg.str() << std::flush;
          }
          continue;
        }
      }

      boost::shared_ptr<RequestParameterMap> pm(new RequestParameterMap);

      if (debug_level > 1)
      {
        std::ostringstream msg;
        msg << SmartMet::Spine::log_time_str() << ": [WFS] [DEBUG] ["
            << get_config()->get_query_id() << "] Found producer '" << meta_info.producer << '\n';
        std::cout << msg.str() << std::flush;
      }

      auto model_boundary = get_model_boundary(meta_info, DATA_CRS_NAME, 20);
      if (model_boundary)
      {
        add_bbox_info(pm.get(), "modelBBox", *model_boundary);
      }

      if (have_bbox)
      {
        auto geom = bbox_intersection(requested_bbox, meta_info);
        if (not geom)
        {
          if (debug_level > 1)
          {
            std::ostringstream msg;
            msg << SmartMet::Spine::log_time_str() << ": [WFS] [DEBUG] ["
                << get_config()->get_query_id() << "] Skipping producer '" << meta_info.producer
                << " bounding box to boox check\n";
            std::cout << msg.str() << std::flush;
          }

          continue;
        }

        bool swap_coord = false;
        crs_registry.get_attribute(requested_bbox.crs, "swapCoord", &swap_coord);
        pm->add("requestedBBox", swap_coord ? requested_bbox.yMin : requested_bbox.xMin);
        pm->add("requestedBBox", swap_coord ? requested_bbox.xMin : requested_bbox.yMin);
        pm->add("requestedBBox", swap_coord ? requested_bbox.yMax : requested_bbox.xMax);
        pm->add("requestedBBox", swap_coord ? requested_bbox.xMax : requested_bbox.yMax);

        add_bbox_info(pm.get(), "calcBBox", *geom);

        // FIXME: intersection may sometimes return OGRGeometryCollection
        //        with more than 1 polygon. Currently add bounding box instead
        //        of boundary for such cases.
        add_boundary(pm.get(), "intersection", *geom);
      }
      else if (model_boundary)
      {
        add_bbox_info(pm.get(), "calcBBox", *model_boundary);
        add_boundary(pm.get(), "intersection", *model_boundary);
      }
      else
      {
        std::ostringstream msg;
        msg << SmartMet::Spine::log_time_str() << " [WFS] [WARNING] " << METHOD_NAME
            << ": failed to get model boundary (producer='" << meta_info.producer
            << "' origin_time='" << meta_info.originTime << "'\n";
        std::cout << msg.str() << std::flush;
        continue;
      }

#if 1
      pm->add("_firstTime", meta_info.firstTime);
      pm->add("_lastTime", meta_info.lastTime);
      pm->add("_timeStep", meta_info.timeStep);
      pm->add("_nTimeSteps", meta_info.nTimeSteps);
      pm->add("_WKT", meta_info.WKT);
      pm->add("_ullon", meta_info.ullon);
      pm->add("_ullat", meta_info.ullat);
      pm->add("_urlon", meta_info.urlon);
      pm->add("_urlat", meta_info.urlat);
      pm->add("_bllon", meta_info.bllon);
      pm->add("_bllat", meta_info.bllat);
      pm->add("_brlon", meta_info.brlon);
      pm->add("_brlat", meta_info.brlat);
      pm->add("_xNumber", meta_info.xNumber);
      pm->add("_yNumber", meta_info.yNumber);
      pm->add("_xResolution", meta_info.xResolution);
      pm->add("_yResolution", meta_info.yResolution);
      pm->add("_areaWidth", meta_info.areaWidth);
      pm->add("_areaHeight", meta_info.areaHeight);
      pm->add("_aspectRatio", meta_info.aspectRatio);

      pm->extract_to(
          "_level", meta_info.levels.begin(), meta_info.levels.end(), &qe::ModelLevel::value);

      pm->extract_to("_parameters",
                     meta_info.parameters.begin(),
                     meta_info.parameters.end(),
                     &qe::ModelParameter::name);
#endif
      pm->add("producer", meta_info.producer);
      pm->add("originTime", Fmi::to_iso_extended_string(meta_info.originTime) + "Z");

      feature_id.erase_param(P_PRODUCER);
      feature_id.erase_param(P_ORIGIN_TIME);
      feature_id.add_param(P_PRODUCER, meta_info.producer);
      feature_id.add_param(P_ORIGIN_TIME, meta_info.originTime);
      pm->add("featureId", feature_id.get_id());

      const auto first_time = opt.hasFirstTime() ? std::max(opt.getFirstTime(), meta_info.firstTime)
                                                 : meta_info.firstTime;

      const auto last_time =
          opt.hasLastTime() ? std::min(opt.getLastTime(), meta_info.lastTime) : meta_info.lastTime;

      pm->add("firstTime", Fmi::to_iso_extended_string(first_time) + "Z");
      pm->add("lastTime", Fmi::to_iso_extended_string(last_time) + "Z");

      // Select exactly the time interval asked for.
      const auto phenomenonBeginTime =
          (params.count(P_BEGIN)) ? std::max(start_time, first_time) : first_time;
      const auto phenomenonEndTime =
          (params.count(P_END)) ? std::min(end_time, last_time) : last_time;
      pm->add("phenomenonBeginTime", Fmi::to_iso_extended_string(phenomenonBeginTime) + "Z");
      pm->add("phenomenonEndTime", Fmi::to_iso_extended_string(phenomenonEndTime) + "Z");

      // Parameters specified in request
      int num_empty_param_names = 0;
      for (auto it = meta_info.parameters.begin(); it != meta_info.parameters.end(); ++it)
      {
        const std::string& name = it->name;
        if (ba::trim_copy(name) == "")
        {
          num_empty_param_names++;
        }
        else if (req_params.empty() or req_params.count(name))
        {
          pm->add("param", name);
        }
      }

      if (num_empty_param_names > 0)
      {
        using SmartMet::Engine::Querydata::ModelParameter;
        std::ostringstream msg;
        msg << SmartMet::Spine::log_time_str() << ": [WFS] [ERROR] " << METHOD_NAME
            << ": empty parameter name found in Querydata metadata."
            << " producer='" << meta_info.producer << "',"
            << " origintime='" << (Fmi::to_iso_extended_string(meta_info.originTime) + "Z'")
            << " params=(";
        BOOST_FOREACH (const auto& param, meta_info.parameters)
        {
          msg << " '" << param.name << "'";
        }
        msg << ")\n";
        std::cout << msg.str() << std::flush;
      }

      for (auto it = meta_info.levels.begin(); it != meta_info.levels.end(); ++it)
      {
        if ((req_level_types.empty() or req_level_types.count(it->type)) and
            (req_level_values.empty() or (req_level_values.count(it->value))))
        {
          pm->add("level", it->value);
        }
      }

      // Test requested format support.
      std::string req_format = params.get_optional<std::string>(P_FORMAT, default_format);
      if (req_format.empty())
        req_format = default_format;
      Fmi::ascii_tolower(req_format);
      if ((formats.find(req_format) == formats.end()))
      {
        std::ostringstream msg;
        msg << " Supported formats:";
        std::for_each(formats.begin(), formats.end(), bl::var(msg) << " '" << bl::_1 << "'");

        SmartMet::Spine::Exception exception(BCP, "The requeted format is not supported!");
        exception.addDetail(msg.str());
        exception.addParameter(WFS_EXCEPTION_CODE, WFS_INVALID_PARAMETER_VALUE);
        exception.addParameter("Requested format", req_format);
        throw exception;
      }

      pm->add("format", req_format);

      // Units of different packaging formats might differ from timevaluepair format.
      // The correct units must be select for the metaplugin links.
      //
      // FIXME!! Make alias lists for the units values
      // For exammple, aliases of 'grib' are 'grib1','grib2'.
      if ((req_format == "grib1") or (req_format == "grib2"))
        pm->add("units", "grib");
      else
        pm->add("units", req_format);

      int epsg = -1;
      const std::string crs = params.get_single<std::string>(P_PROJECTION);
      if (not crs_registry.get_attribute(crs, "epsg", &epsg))
      {
        SmartMet::Spine::Exception exception(BCP, "Failed to get EPSG code for CRS '" + crs + "'!");
        exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PARSING_FAILED);
        throw exception;
      }
      pm->add("crs", str(format("EPSG:%04d") % epsg));

      // FIXME: maybe some combined data also is required

      result.push_back(pm);
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

boost::shared_ptr<OGRPolygon> StoredQEDownloadQueryHandler::get_model_boundary(
    const SmartMet::Engine::Querydata::MetaData& meta_info,
    const std::string& crs_name,
    int num_side_points) const
{
  try
  {
    // TODO TODO: THIS PLACE HAD STRANGE CODE, SIMPLIFIED IT A LOT
    auto q = q_engine->get(meta_info.producer, meta_info.originTime);

    if (!q->isArea())
      return boost::shared_ptr<OGRPolygon>();

    const NFmiArea& area = q->area();
    boost::shared_ptr<OGRPolygon> model_area(new OGRPolygon);
    SmartMet::Plugin::WFS::get_latlon_boundary(&area, model_area.get(), num_side_points);

    auto transformation = crs_registry.create_transformation(DATA_CRS_NAME, crs_name);
    transformation->transform(*model_area);

    return model_area;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

boost::shared_ptr<OGRGeometry> StoredQEDownloadQueryHandler::bbox_intersection(
    const SmartMet::Spine::BoundingBox& bbox,
    const SmartMet::Engine::Querydata::MetaData& meta_info) const
{
  try
  {
    // TODO TODO: STRANGE CODE WAS HERE
    const int BBOX_NPOINTS = 10;
    boost::shared_ptr<OGRGeometry> result;

    auto q = q_engine->get(meta_info.producer, meta_info.originTime);

    if (!q->isArea())
      return result;

    const NFmiArea& area = q->area();

    OGRPolygon query_bbox;
    auto transform_to_epsg4326 = crs_registry.create_transformation(bbox.crs, DATA_CRS_NAME);
    bbox2polygon(bbox, &query_bbox, BBOX_NPOINTS);
    transform_to_epsg4326->transform(query_bbox);

    const NFmiRect& rect = area.XYArea();

    auto model_area = SmartMet::Engine::Gis::bbox2polygon(rect);

    // std::cout << METHOD_NAME << ": " << meta_info.producer << "["
    //          << pt::to_simple_string(meta_info.originTime) << "]" << std::endl;
    // std::cout << METHOD_NAME << ": model_area='" << WKT(*model_area) << "'" << std::endl;

    SmartMet::Engine::Gis::GeometryConv conv1(boost::bind(&NFmiArea::ToXY, &area, ::_1));
    query_bbox.transform(&conv1);

    // std::cout << METHOD_NAME << ": query_bbox='" << WKT(*query_bbox) << "'" << std::endl;

    OGRGeometry* intersection = model_area->Intersection(&query_bbox);
    if (intersection and not intersection->IsEmpty())
    {
      result.reset(intersection);
      SmartMet::Engine::Gis::GeometryConv conv2(boost::bind(&NFmiArea::ToLatLon, &area, ::_1));
      result->transform(&conv2);

      // std::cout << METHOD_NAME <<": INTERSECTION='" << WKT(*result) << "'" << std::endl;
    }
    else if (intersection)
    {
    }
    else
    {
      int last_err = CPLGetLastErrorNo();
      if (last_err != CPLE_None)
      {
        throw SmartMet::Spine::Exception(BCP, str(format("GDAL error: %1%") % (int)last_err));
      }
    }

    return result;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void StoredQEDownloadQueryHandler::add_bbox_info(RequestParameterMap* param_map,
                                                 const std::string& name,
                                                 OGRGeometry& geom) const
{
  try
  {
    OGREnvelope envelope;
    geom.getEnvelope(&envelope);

    param_map->add(name, envelope.MinX);
    param_map->add(name, envelope.MinY);
    param_map->add(name, envelope.MaxX);
    param_map->add(name, envelope.MaxY);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void StoredQEDownloadQueryHandler::add_boundary(RequestParameterMap* param_map,
                                                const std::string& name,
                                                OGRGeometry& geom) const
{
  try
  {
    const auto geom_type = geom.getGeometryType();
    if (geom_type == wkbPolygon)
    {
      OGRPolygon& polygon = dynamic_cast<OGRPolygon&>(geom);
      OGRLinearRing* exterior = polygon.getExteriorRing();
      const int numPoints = exterior->getNumPoints();
      for (int i = 0; i < numPoints; i++)
      {
        param_map->add(name, str(format("%.4f %.4f") % exterior->getX(i) % exterior->getY(i)));
      }
    }
    else
    {
      // Currently just output bounding box (OGREnvelope). Could
      // be improved sometimes if needed
      add_bbox_info(param_map, name, geom);
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

boost::shared_ptr<SmartMet::Plugin::WFS::StoredQueryHandlerBase>
wfs_stored_qe_download_handler_create(SmartMet::Spine::Reactor* reactor,
                                      boost::shared_ptr<StoredQueryConfig> config,
                                      PluginData& plugin_data,
                                      boost::optional<std::string> template_file_name)
{
  try
  {
    StoredAtomQueryHandlerBase* qh =
        new StoredQEDownloadQueryHandler(reactor, config, plugin_data, template_file_name);
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

SmartMet::Plugin::WFS::StoredQueryHandlerFactoryDef wfs_stored_qe_download_handler_factory(
    &wfs_stored_qe_download_handler_create);

/**

@page WFS_SQ_QE_DOWNLOAD_HANDLER Stored Query handlers for Querydata data download

@section WFS_QE_DOWNLOAD_HANDLER_INTRO Introduction

This stored query handler provides supports for atom queries for grid data download from
<a href="../../dlsplugin/html/index.html">Download plugin</a>.

<table border="1">
  <tr>
     <td>Implementation</td>
     <td>SmartMet::Plugin::WFS::StoredQEDownloadQueryHandler</td>
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
  <td>Output format to use for data queries (default @b grib2 ).
      If other default format is defined it must also define into
      @b formats list.</td>
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
  <td>formats</td>
  <td>array of strings</td>
  <td>optional (default grib2,grib1,netcdf)</td>
  <td>Specifies output file formats (see format parameter). @b grib2 format is supported
      even when it is not defined at the array. If other than default file types are
      specified review of results_for_grid template is recommended.</td>
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
