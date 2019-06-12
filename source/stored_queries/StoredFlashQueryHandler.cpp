#include "stored_queries/StoredFlashQueryHandler.h"
#include "FeatureID.h"
#include "StoredQueryHandlerFactoryDef.h"
#include "WfsConst.h"
#include <macgyver/StringConversion.h>
#include <smartmet/spine/Convenience.h>
#include <smartmet/spine/Exception.h>
#include <smartmet/spine/Value.h>

namespace bw = SmartMet::Plugin::WFS;
namespace pt = boost::posix_time;
namespace bg = boost::gregorian;

const char* bw::StoredFlashQueryHandler::P_BEGIN_TIME = "beginTime";
const char* bw::StoredFlashQueryHandler::P_END_TIME = "endTime";
const char* bw::StoredFlashQueryHandler::P_PARAM = "meteoParameters";
const char* bw::StoredFlashQueryHandler::P_CRS = "crs";

bw::StoredFlashQueryHandler::StoredFlashQueryHandler(
    SmartMet::Spine::Reactor* reactor,
    boost::shared_ptr<StoredQueryConfig> config,
    PluginImpl& plugin_data,
    boost::optional<std::string> template_file_name)
    : SupportsExtraHandlerParams(config, false),
      StoredQueryHandlerBase(reactor, config, plugin_data, template_file_name),
      SupportsBoundingBox(config, plugin_data.get_crs_registry()),
      SupportsTimeZone(config),
      geo_engine(nullptr),
      obs_engine(nullptr),
      bs_param(),
      stroke_time_ind(
          add_param(bs_param, "utctime", SmartMet::Spine::Parameter::Type::DataIndependent)),
      lon_ind(add_param(bs_param, "longitude", SmartMet::Spine::Parameter::Type::DataDerived)),
      lat_ind(add_param(bs_param, "latitude", SmartMet::Spine::Parameter::Type::DataDerived))
{
  try
  {
    register_scalar_param<pt::ptime>(P_BEGIN_TIME, *config);
    register_scalar_param<pt::ptime>(P_END_TIME, *config);
    register_array_param<std::string>(P_PARAM, *config, 1, 999);
    register_scalar_param<std::string>(P_CRS, *config);

    station_type = config->get_optional_config_param<std::string>("stationType", "flash");
    max_hours = config->get_optional_config_param<double>("maxHours", 7.0 * 24.0);
    missing_text = config->get_optional_config_param<std::string>("missingText", "NaN");

    sq_restrictions = plugin_data.get_config().getSQRestrictions();
    time_block_size = 1;

    if (sq_restrictions)
      time_block_size = config->get_optional_config_param<unsigned>("timeBlockSize", 300);

    if ((time_block_size < 1) or ((86400 % time_block_size) != 0))
    {
      SmartMet::Spine::Exception exception(
          BCP, "Invalid time block size '" + std::to_string(time_block_size) + " seconds'!");
      exception.addDetail("Value must be a divisor of 24*60*60 (86400).");
      exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PROCESSING_FAILED);
      throw exception;
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

bw::StoredFlashQueryHandler::~StoredFlashQueryHandler() {}

void bw::StoredFlashQueryHandler::init_handler()
{
  try
  {
    auto* reactor = get_reactor();

    auto* engine = reactor->getSingleton("Observation", nullptr);
    if (engine == nullptr)
      throw SmartMet::Spine::Exception(BCP, "No Observation engine available");

    obs_engine = reinterpret_cast<SmartMet::Engine::Observation::Engine*>(engine);

    engine = reactor->getSingleton("Geonames", nullptr);
    if (engine == nullptr)
      throw SmartMet::Spine::Exception(BCP, "No Geonames engine available");

    geo_engine = reinterpret_cast<SmartMet::Engine::Geonames::Engine*>(engine);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

namespace
{
pt::ptime round_time(const pt::ptime& t0, unsigned step, int offset = 0)
{
  try
  {
    pt::ptime t = t0 + pt::seconds(offset);
    long sec = t.time_of_day().total_seconds();
    pt::ptime result(t.date(), pt::seconds(step * (sec / step)));
    return result;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}
}  // namespace

void bw::StoredFlashQueryHandler::query(const StoredQuery& query,
                                        const std::string& language,
                                        std::ostream& output) const
{
  try
  {
    using namespace SmartMet;
    using namespace SmartMet::Plugin::WFS;
    using SmartMet::Spine::BoundingBox;

    const int debug_level = get_config()->get_debug_level();
    if (debug_level > 0)
      query.dump_query_info(std::cout);

    const auto& params = query.get_param_map();

    try
    {
      SmartMet::Engine::Observation::Settings query_params;

      const char* DATA_CRS_NAME = "urn:ogc:def:crs:EPSG::4326";
      const std::string crs = params.get_single<std::string>(P_CRS);
      auto transformation = crs_registry.create_transformation(DATA_CRS_NAME, crs);
      // FIXME: shouldn't we transform at first from FMI sphere to WGS84?

      bool show_height = false;
      std::string proj_uri = "UNKNOWN";
      std::string proj_epoch_uri = "UNKNOWN";
      bool swap_coord = false;
      crs_registry.get_attribute(crs, "showHeight", &show_height);
      crs_registry.get_attribute(crs, "projUri", &proj_uri);
      crs_registry.get_attribute(crs, "projEpochUri", &proj_epoch_uri);
      crs_registry.get_attribute(crs, "swapCoord", &swap_coord);
      if (show_height)
      {
        SmartMet::Spine::Exception exception(
            BCP, "Projection '" + crs + "' not supported for lightning data!");
        exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PROCESSING_FAILED);
        throw exception;
      }

      pt::ptime begin = params.get_single<pt::ptime>(P_BEGIN_TIME);
      pt::ptime end = params.get_single<pt::ptime>(P_END_TIME);
      pt::ptime now = pt::second_clock().universal_time();
      pt::ptime last = now - pt::seconds(time_block_size);

      query_params.starttime = round_time(begin, time_block_size, 0);
      query_params.endtime = round_time(std::min(end, last), time_block_size, time_block_size - 1);

      if (sq_restrictions)
        check_time_interval(query_params.starttime, query_params.endtime, max_hours);

      query_params.parameters = bs_param;
      bool have_meteo_param = false;
      std::vector<std::string> param_names;
      params.get<std::string>(P_PARAM, std::back_inserter(param_names));
      int first_param = 0, last_param = 0;
      BOOST_FOREACH (std::string name, param_names)
      {
        SmartMet::Spine::Parameter param = obs_engine->makeParameter(name);
        query_params.parameters.push_back(param);
        int ind = query_params.parameters.size() - 1;
        if (first_param == 0)
          first_param = ind;
        last_param = ind;
        if (!special(param))
          have_meteo_param = true;
      }

      query_params.allplaces = false;
      query_params.stationtype = station_type;
      query_params.maxdistance = 20000.0;
      query_params.latest = false;
      query_params.timeformat = "iso";
      query_params.timezone = "UTC";
      query_params.numberofstations = 1;
      query_params.missingtext = missing_text;
      query_params.localename = get_config()->get_locale_name();
      query_params.starttimeGiven = true;

      const std::string tz_name = get_tz_name(params);
      boost::local_time::time_zone_ptr tzp = get_time_zone(tz_name);

      boost::shared_ptr<SmartMet::Engine::Gis::CRSRegistry::Transformation> to_bbox_transform;

      // boost::optional<> aiheuttaa täällä strict aliasing varoituksen jos gcc-4.4.X
      // on käytössä. Sen vuoksi std::unique_ptr on käytetty boost::optionla tilalle.

      SmartMet::Spine::BoundingBox requested_bbox;
      SmartMet::Spine::BoundingBox query_bbox;
      bool have_bbox = get_bounding_box(params, crs, &requested_bbox);
      bool bb_swap_coord = false;
      std::string bb_proj_uri;

      if (have_bbox)
      {
        query_bbox = transform_bounding_box(requested_bbox, DATA_CRS_NAME);
        to_bbox_transform = crs_registry.create_transformation(DATA_CRS_NAME, requested_bbox.crs);
        query_params.boundingBox["minx"] = query_bbox.xMin;
        query_params.boundingBox["miny"] = query_bbox.yMin;
        query_params.boundingBox["maxx"] = query_bbox.xMax;
        query_params.boundingBox["maxy"] = query_bbox.yMax;

        std::cerr << "query bbox = " << query_bbox.xMin << "..." << query_bbox.xMax << " "
                  << query_bbox.yMin << "..." << query_bbox.yMax << "\n";

        std::cerr << "query crs = " << requested_bbox.crs << "\n";

        crs_registry.get_attribute(requested_bbox.crs, "swapCoord", &bb_swap_coord);
        crs_registry.get_attribute(requested_bbox.crs, "projUri", &bb_proj_uri);

        if (debug_level > 0)
        {
          std::ostringstream msg;
          msg << SmartMet::Spine::log_time_str() << ": [WFS] [DEBUG] Original bounding box: ("
              << requested_bbox.xMin << " " << requested_bbox.yMin << " " << requested_bbox.xMax
              << " " << requested_bbox.yMax << " " << requested_bbox.crs << ")\n";
          msg << "                                   "
              << " Converted bounding box: (" << query_bbox.xMin << " " << query_bbox.yMin << " "
              << query_bbox.xMax << " " << query_bbox.yMax << " " << query_bbox.crs << ")\n";
          std::cout << msg.str() << std::flush;
        }
      }

      SmartMet::Spine::ValueFormatterParam vf_param;
      vf_param.missingText = query_params.missingtext;
      boost::shared_ptr<SmartMet::Spine::ValueFormatter> value_formatter(
          new SmartMet::Spine::ValueFormatter(vf_param));

      if (query_params.starttime > query_params.endtime)
      {
        SmartMet::Spine::Exception exception(BCP, "Invalid time interval!");
        exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PARSING_FAILED);
        exception.addParameter("Start time", pt::to_simple_string(query_params.starttime));
        exception.addParameter("End time", pt::to_simple_string(query_params.endtime));
        throw exception;
      }

      // FIXME: add parameter to restrict number of days per query

      if (not have_meteo_param)
      {
        SmartMet::Spine::Exception exception(BCP, "No meteo parameter found!");
        exception.addDetail("At least one meteo parameter must be specified!");
        exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PROCESSING_FAILED);
        throw exception;
      }

      // Fetch the values

      const auto result_ptr = obs_engine->values(query_params);

      CTPP::CDT hash;

      // Get the sequence number of query in the request
      int sq_id = query.get_query_id();

      bw::FeatureID feature_id(get_config()->get_query_id(), params.get_map(), sq_id);

      hash["language"] = language;

      hash["responseTimestamp"] =
          Fmi::to_iso_extended_string(get_plugin_impl().get_time_stamp()) + "Z";
      hash["queryNum"] = query.get_query_id();

      hash["featureId"] = feature_id.get_id();
      // FIXME: Do we need separate feature ID for each parameter?

      hash["fmi_apikey"] = bw::QueryBase::FMI_APIKEY_SUBST;
      hash["fmi_apikey_prefix"] = bw::QueryBase::FMI_APIKEY_PREFIX_SUBST;
      hash["hostname"] = QueryBase::HOSTNAME_SUBST;
      hash["protocol"] = QueryBase::PROTOCOL_SUBST;
      hash["srsName"] = proj_uri;
      hash["projSrsDim"] = (show_height ? 3 : 2);
      hash["srsEpochName"] = proj_epoch_uri;
      hash["projEpochSrsDim"] = (show_height ? 4 : 3);

      hash["memberId"] = "enn-m-1";
      hash["resultTimeId"] = "time-1";
      hash["samplingFeatureId"] = "flash-s-1";
      hash["sampledFeatureTargetId"] = "area-1";
      hash["sampledFeatureTargetPolygonId"] = "polygon-1";
      hash["resultCoverageId"] = "mpcv-1";
      hash["resultCoverageMultiPointId"] = "mp-1";

      for (int k = first_param; k <= last_param; k++)
      {
        const int k0 = k - first_param;
        const std::string& name = param_names.at(k0);
        feature_id.erase_param(P_PARAM);
        feature_id.add_param(P_PARAM, name);
        hash["paramList"][k0]["name"] = param_names.at(k0);
        hash["paramList"][k0]["featureID"] = feature_id.get_id();
      }

      if (have_bbox)
      {
        // FIXME: missä projektion pitäisi käyttää täälä. Todennäköisesti alkuperainen
        //        bbox projektio olisi parempi
        CTPP::CDT p_bb;
        double x_ll = bb_swap_coord ? requested_bbox.yMin : requested_bbox.xMin;
        double y_ll = bb_swap_coord ? requested_bbox.xMin : requested_bbox.yMin;
        double x_ur = bb_swap_coord ? requested_bbox.yMax : requested_bbox.xMax;
        double y_ur = bb_swap_coord ? requested_bbox.xMax : requested_bbox.yMax;

        p_bb["lowerLeft"]["x"] = x_ll;
        p_bb["lowerLeft"]["y"] = y_ll;

        p_bb["lowerRight"]["x"] = x_ur;
        p_bb["lowerRight"]["y"] = y_ll;

        p_bb["upperLeft"]["x"] = x_ll;
        p_bb["upperLeft"]["y"] = y_ur;

        p_bb["upperRight"]["x"] = x_ur;
        p_bb["upperRight"]["y"] = y_ur;

        p_bb["projUri"] = bb_proj_uri;
        p_bb["srsDim"] = 2;

        hash["metadata"]["boundingBox"] = p_bb;
      }

      std::size_t used_rows = 0;
      std::size_t num_skipped = 0;
      std::size_t num_rows = 0;

      if (result_ptr && !result_ptr->empty())
      {
        static const long ref_jd = boost::gregorian::date(1970, 1, 1).julian_day();

        const auto& result = *result_ptr;
        num_rows = result[1].size();

        for (std::size_t i = 0; i < num_rows; i++)
        {
          // I have no clue why the ordering is this, this changed code
          // does the same as the original (sx = lat, sy = lon) - Mika

          double lon = boost::get<double>(result[lat_ind][i].value);
          double lat = boost::get<double>(result[lon_ind][i].value);

          if (to_bbox_transform)
          {
            try
            {
              NFmiPoint p1(lon, lat);
              NFmiPoint p2 = to_bbox_transform->transform(p1);

              double x = bb_swap_coord ? p2.Y() : p2.X();
              double y = bb_swap_coord ? p2.X() : p2.Y();

              if ((x < requested_bbox.xMin) or (x > requested_bbox.xMax) or
                  (y < requested_bbox.yMin) or (y > requested_bbox.yMax))
              {
                if (debug_level > 3)
                {
                  std::ostringstream msg;
                  msg << SmartMet::Spine::log_time_str() << ": [WFS] [DEBUG] Event (" << p1.X()
                      << " " << p1.Y() << ") ==> (" << p2.X() << " " << p2.Y()
                      << ") skipped due to bounding box check\n";
                  std::cout << msg.str() << std::flush;
                }
                num_skipped++;
                continue;
              }
            }
            catch (...)
            {
              // Ignore errors while extra bounding box checks
              //(void)err;
            }
          }

          CTPP::CDT& result_row = hash["returnArray"][used_rows++];
          set_2D_coord(transformation, Fmi::to_string(lon), Fmi::to_string(lat), result_row);
          const pt::ptime epoch = result[lon_ind][i].time.utc_time();
          long long jd = epoch.date().julian_day();
          long seconds = epoch.time_of_day().total_seconds();
          INT_64 s_epoch = 86400LL * (jd - ref_jd) + seconds;
          result_row["stroke_time"] = s_epoch;
          result_row["stroke_time_str"] = format_local_time(epoch, tzp);

          for (int k = first_param; k <= last_param; k++)
          {
            std::string value;
            if (boost::get<int>(&result[k][i].value))
              value = Fmi::to_string(boost::get<int>(result[k][i].value));
            else if (boost::get<double>(&result[k][i].value))
            {
              auto tmp = boost::get<double>(result[k][i].value);
              if (static_cast<int>(tmp) != tmp)
                value = value_formatter->format(tmp, 1);
              else
                value = Fmi::to_string(tmp);
            }
            else
              value = query_params.missingtext;
            result_row["data"][k - first_param] = remove_trailing_0(value);
          }
        }
      }

      if (debug_level > 1 and num_skipped)
      {
        std::ostringstream msg;
        msg << SmartMet::Spine::log_time_str() << " [WFS] [DEBUG] " << num_skipped << " of "
            << num_rows << " lightning observations skipped due to bounding box check\n";
        std::cout << msg.str() << std::flush;
      }

      hash["numMatched"] = used_rows == 0 ? 0 : 1;
      hash["numReturned"] = used_rows == 0 ? 0 : 1;

      hash["phenomenonTimeId"] = "time-interval-1";
      hash["phenomenonStartTime"] = Fmi::to_iso_extended_string(query_params.starttime) + "Z";
      hash["phenomenonEndTime"] = Fmi::to_iso_extended_string(query_params.endtime) + "Z";

      format_output(hash, output, query.get_use_debug_format());
    }
    catch (...)
    {
      SmartMet::Spine::Exception exception(BCP, "Operation failed!", nullptr);
      // Set language for exception and re-throw it
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

boost::shared_ptr<SmartMet::Plugin::WFS::StoredQueryHandlerBase> wfs_flash_handler_create(
    SmartMet::Spine::Reactor* reactor,
    boost::shared_ptr<StoredQueryConfig> config,
    PluginImpl& plugin_data,
    boost::optional<std::string> template_file_name)
{
  try
  {
    StoredFlashQueryHandler* qh =
        new StoredFlashQueryHandler(reactor, config, plugin_data, template_file_name);
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

SmartMet::Plugin::WFS::StoredQueryHandlerFactoryDef wfs_flash_handler_factory(
    &wfs_flash_handler_create);

/**

@page WFS_SQ_FLASH_OBS_HANDLER Stored Query handler for querying lightning observations

@section WFS_FLASH_OBS_HANDLER_INTRO Introduction

This stored query handler provides access to FMI lightning observations

<table border="1">
  <tr>
     <td>Implementation</td>
     <td>SmartMet::Plugin::WFS::StoredFlashQueryHandler</td>
  </tr>
  <tr>
    <td>Constructor name (for stored query configuration)</td>
    <td>@b wfs_flash_handler_factory</td>
  </tr>
</table>


@section WFS_SQ_FLASH_OBS_HANDLER_PARAMS Query handler built-in parameters

The following stored query handler parameter groups are being used by this stored query handler:
- @ref WFS_SQ_PARAM_BBOX
- @ref WFS_SQ_TIME_ZONE

Additionally to the parameters from these stored query handler parameter groups the
following built-in handler parameters are being used:

<table border="1">

<tr>
<th>Entry name</th>
<th>Type</th>
<th>Data type</th>
<th>Description</th>
</tr>

<tr>
  <td>beginTime</td>
  <td>@ref WFS_CFG_SCALAR_PARAM_TMPL</td>
  <td>time</td>
  <td>Specifies the time of the begin of time interval</td>
</tr>

<tr>
  <td>endTime</td>
  <td>@ref WFS_CFG_SCALAR_PARAM_TMPL</td>
  <td>time</td>
  <td>Specifies the time of the end of time interval</td>
</tr>

<tr>
  <td>meteoParameters</td>
  <td>@ref WFS_CFG_ARRAY_PARAM_TMPL</td>
  <td>string</td>
  <td>Specifies the meteo parameters to query</td>
</tr>

<tr>
  <td>CRS</td>
  <td>@ref WFS_CFG_SCALAR_PARAM_TMPL</td>
  <td>string</td>
  <td>The coordinate projection to use</td>
</tr>

</table>

@section WFS_SQ_FLASH_OBS_HANLDER_EXTRA_CFG_PARAM Additional parameters

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
  <td>stationType</td>
  <td>string</td>
  <td>optional (default @b flash)</td>
  <td>Specifies station type. Intended to be used if distinction between commercial and open data
services
      will be implemented as different station types</td>
</tr>

<tr>
  <td>maxHours</td>
  <td>double</td>
  <td>optional</td>
  <td>Specifies maximal permitted time interval in hours. The default value
      is 168 hours (7 days). This restriction is not valid, if the optional
      storedQueryRestrictions parameter is set to false in WFS Plugin
      configurations.</td>
</tr>

<tr>
  <td>missingText</td>
  <td>string</td>
  <td>optional</td>
  <td>Specifies text to use in response for missing values.</td>
</tr>

<tr>
  <td>timeBlockSize</td>
  <td>integer</td>
  <td>optional</td>
  <td>Specifies the time step for lightning observations in seconds. The observations
      are returned in blocks of this length. Only full blocks are supported.
      This parameter is not valid, if the *optional* storedQueryRestrictions
      parameter is set to false in WFS Plugin configurations.</td>
</tr>

</table>


*/
