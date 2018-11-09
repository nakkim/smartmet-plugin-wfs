#include "stored_queries/StoredObsQueryHandler.h"
#include "FeatureID.h"
#include "StoredQueryHandlerFactoryDef.h"
#include "WfsConst.h"
#include "WfsConvenience.h"
#include <boost/algorithm/string.hpp>
#include <boost/format.hpp>
#include <boost/lambda/lambda.hpp>
#include <macgyver/StringConversion.h>
#include <smartmet/spine/Convenience.h>
#include <smartmet/spine/Exception.h>
#include <smartmet/spine/TimeSeries.h>
#include <smartmet/spine/TimeSeriesOutput.h>
#include <smartmet/spine/Value.h>
#include <algorithm>
#include <functional>

#define P_BEGIN_TIME "beginTime"
#define P_END_TIME "endTime"
#define P_METEO_PARAMETERS "meteoParameters"
#define P_STATION_TYPE "stationType"
#define P_TIME_STEP "timeStep"
#define P_LPNNS "lpnns"
#define P_NUM_OF_STATIONS "numOfStations"
#define P_HOURS "hours"
#define P_WEEK_DAYS "weekDays"
#define P_LOCALE "locale"
#define P_MISSING_TEXT "missingText"
#define P_MAX_EPOCHS "maxEpochs"
#define P_CRS "crs"
#define P_LATEST "latest"

namespace pt = boost::posix_time;
namespace lt = boost::local_time;
namespace ba = boost::algorithm;
namespace bl = boost::lambda;
using namespace SmartMet::Spine;

namespace SmartMet
{
namespace Plugin
{
namespace WFS
{
using boost::format;
using boost::str;

StoredObsQueryHandler::StoredObsQueryHandler(SmartMet::Spine::Reactor* reactor,
                                             boost::shared_ptr<StoredQueryConfig> config,
                                             PluginData& plugin_data,
                                             boost::optional<std::string> template_file_name)
    : SupportsExtraHandlerParams(config, false),
      StoredQueryHandlerBase(reactor, config, plugin_data, template_file_name),
      SupportsLocationParameters(config, SUPPORT_KEYWORDS | INCLUDE_GEOIDS),
      SupportsBoundingBox(config, plugin_data.get_crs_registry()),
      SupportsTimeZone(config),
      SupportsQualityParameters(config),
      SupportsMeteoParameterOptions(config),
      geo_engine(nullptr),
      obs_engine(nullptr),
      initial_bs_param(),
      fmisid_ind(SmartMet::add_param(initial_bs_param, "fmisid", Parameter::Type::DataIndependent)),
      geoid_ind(SmartMet::add_param(initial_bs_param, "geoid", Parameter::Type::DataIndependent)),
      lon_ind(SmartMet::add_param(initial_bs_param, "stationlon", Parameter::Type::DataDerived)),
      lat_ind(SmartMet::add_param(initial_bs_param, "stationlat", Parameter::Type::DataDerived)),
      height_ind(
          SmartMet::add_param(initial_bs_param, "elevation", Parameter::Type::DataIndependent)),
      name_ind(
          SmartMet::add_param(initial_bs_param, "stationname", Parameter::Type::DataIndependent)),
      dist_ind(SmartMet::add_param(initial_bs_param, "distance", Parameter::Type::DataIndependent)),
      direction_ind(
          SmartMet::add_param(initial_bs_param, "direction", Parameter::Type::DataIndependent)),
      wmo_ind(SmartMet::add_param(initial_bs_param, "wmo", Parameter::Type::DataIndependent))
{
  try
  {
    register_scalar_param<pt::ptime>(P_BEGIN_TIME, *config);
    register_scalar_param<pt::ptime>(P_END_TIME, *config);
    register_scalar_param<bool>(P_LATEST, *config);
    register_array_param<std::string>(P_METEO_PARAMETERS, *config, 1);
    register_scalar_param<std::string>(P_STATION_TYPE, *config);
    register_scalar_param<uint64_t>(P_TIME_STEP, *config);
    register_array_param<int64_t>(P_LPNNS, *config);
    register_scalar_param<uint64_t>(P_NUM_OF_STATIONS, *config);
    register_array_param<int64_t>(P_HOURS, *config);
    register_array_param<int64_t>(P_WEEK_DAYS, *config);
    register_scalar_param<std::string>(P_LOCALE, *config);
    register_scalar_param<std::string>(P_MISSING_TEXT, *config);
    register_scalar_param<uint64_t>(P_MAX_EPOCHS, *config);
    register_scalar_param<std::string>(P_CRS, *config);
    register_array_param<int64_t>(P_FMISIDS, *config);
    register_array_param<int64_t>(P_WMOS, *config);

    max_hours = config->get_optional_config_param<double>("maxHours", 7.0 * 24.0);
    max_station_count = config->get_optional_config_param<unsigned>("maxStationCount", 0);
    separate_groups = config->get_optional_config_param<bool>("separateGroups", false);
    sq_restrictions = plugin_data.get_config().getSQRestrictions();
    m_support_qc_parameters = config->get_optional_config_param<bool>("supportQCParameters", false);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

StoredObsQueryHandler::~StoredObsQueryHandler() {}

void StoredObsQueryHandler::init_handler()
{
  try
  {
    auto* reactor = get_reactor();

    void* engine;

    engine = reactor->getSingleton("Geonames", nullptr);
    if (engine == nullptr)
    {
      throw SmartMet::Spine::Exception(BCP, "No Geonames engine available");
    }

    geo_engine = reinterpret_cast<SmartMet::Engine::Geonames::Engine*>(engine);

    engine = reactor->getSingleton("Observation", nullptr);
    if (engine == nullptr)
    {
      throw SmartMet::Spine::Exception(BCP, "No ObservationEngine available");
    }

    obs_engine = reinterpret_cast<SmartMet::Engine::Observation::Engine*>(engine);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void StoredObsQueryHandler::query(const StoredQuery& query,
                                  const std::string& language,
                                  std::ostream& output) const
{
  try
  {
    namespace pt = boost::posix_time;
    using namespace SmartMet;
    using namespace SmartMet::Plugin::WFS;

    const int debug_level = get_config()->get_debug_level();
    if (debug_level > 0)
      query.dump_query_info(std::cout);

    // If 'data_source'-field exists
    // extend it so that for every meteo.parameter
    // a corresponding '*data_source'-field is added,
    // for example, Temperature -> Temperature_data_source
    auto params = query.get_param_map();

    std::set<std::string> keys = params.get_keys();
    for (auto k : keys)
    {
      if (k == "meteoParameters")
      {
        std::vector<SmartMet::Spine::Value> values = params.get_values(k);
        std::vector<std::string> mparams;
        bool dataSourceFound = false;
        for (auto v : values)
        {
          std::string val = v.to_string();
          if (val == "data_source")
          {
            dataSourceFound = true;
          }
          else
          {
            mparams.push_back(val);
          }
        }
        if (dataSourceFound && mparams.size() > 0)
        {
          params.remove("meteoParameters");
          for (auto p : mparams)
          {
            std::string fieldname = p + "_data_source";
            params.add("meteoParameters", p);
            params.add("meteoParameters", fieldname);
          }
        }
        break;
      }
    }

    try
    {
      bool have_meteo_param = false;
      std::vector<int64_t> tmp_vect;
      SmartMet::Engine::Observation::Settings query_params;
      query_params.useDataCache = true;

      const char* DATA_CRS_NAME = "urn:ogc:def:crs:EPSG::4326";

      query_params.latest = params.get_optional<bool>(P_LATEST, false);
      query_params.starttime = params.get_single<pt::ptime>(P_BEGIN_TIME);

      query_params.endtime = params.get_single<pt::ptime>(P_END_TIME);
      if (sq_restrictions)
        check_time_interval(query_params.starttime, query_params.endtime, max_hours);

      std::list<std::pair<std::string, SmartMet::Spine::LocationPtr> > locations_list;
      get_location_options(geo_engine, params, language, &locations_list);
      std::transform(
          locations_list.begin(),
          locations_list.end(),
          std::back_inserter(query_params.locations),
          boost::bind(&std::pair<std::string, SmartMet::Spine::LocationPtr>::second, ::_1));

      std::vector<std::string> param_names;
      params.get<std::string>(P_METEO_PARAMETERS, std::back_inserter(param_names), 1);

      query_params.stationtype =
          Fmi::ascii_tolower_copy(params.get_single<std::string>(P_STATION_TYPE));

      // Only one name for each one is allowed (case insensitively).
      // Avoiding oracle errors from Observation
      std::vector<std::string> lowerCaseParamNames;
      BOOST_FOREACH (const std::string& name, param_names)
        lowerCaseParamNames.push_back(Fmi::ascii_tolower_copy(name));
      BOOST_FOREACH (const std::string& name, param_names)
      {
        if (std::count(lowerCaseParamNames.begin(),
                       lowerCaseParamNames.end(),
                       Fmi::ascii_tolower_copy(name)) > 1)
        {
          SmartMet::Spine::Exception exception(BCP, "Extra parameter name '" + name + "'!");
          exception.addParameter(WFS_EXCEPTION_CODE, WFS_INVALID_PARAMETER_VALUE);
          throw exception;
        }
      }

      std::vector<std::string> qc_info_param_names;
      const bool support_quality_info = SupportsQualityParameters::supportQualityInfo(params);

      auto qc_param_name_ref = SupportsQualityParameters::firstQCParameter(param_names);
      if (not m_support_qc_parameters or not support_quality_info)
      {
        // Do not allow QC parameters if it is not enabled in stored query.
        if (qc_param_name_ref != param_names.end())
        {
          SmartMet::Spine::Exception exception(BCP, "Invalid parameter!");
          exception.addDetail("Quality code parameter '" + *qc_param_name_ref +
                              "' is not allowed in this query.");
          exception.addParameter(WFS_EXCEPTION_CODE, WFS_INVALID_PARAMETER_VALUE);
          throw exception;
        }
      }

      // Quality info parameter construction if there is not any QC parameters given.
      if (support_quality_info and qc_param_name_ref == param_names.end())
      {
        BOOST_FOREACH (const std::string& name, param_names)
          qc_info_param_names.push_back("qc_" + name);
      }

      int first_param = 0, last_param = 0;
      query_params.parameters = initial_bs_param;

      BOOST_FOREACH (std::string name, param_names)
      {
        // Is the parameter configured in Observation
        if (not obs_engine->isParameter(name, query_params.stationtype))
        {
          SmartMet::Spine::Exception exception(BCP, "Unknown parameter in the query!");
          exception.addParameter(WFS_EXCEPTION_CODE, WFS_INVALID_PARAMETER_VALUE);
          exception.addParameter("Unknown parameter", name);
          throw exception;
        }

        SmartMet::Spine::Parameter param = obs_engine->makeParameter(name);
        query_params.parameters.push_back(param);
        int ind = query_params.parameters.size() - 1;
        if (first_param == 0)
          first_param = ind;
        last_param = ind;
        if (!special(param))
          have_meteo_param = true;
      }

      // QC parameter inclusion.
      int first_qc_param = 0;
      // int last_qc_param = 0;
      BOOST_FOREACH (std::string name, qc_info_param_names)
      {
        if (not obs_engine->isParameter(name, query_params.stationtype))
        {
          SmartMet::Spine::Exception exception(BCP, "Unknown parameter in the query!");
          exception.addParameter(WFS_EXCEPTION_CODE, WFS_INVALID_PARAMETER_VALUE);
          exception.addParameter("Unknown parameter", name);
          throw exception;
        }

        SmartMet::Spine::Parameter param = obs_engine->makeParameter(name);
        query_params.parameters.push_back(param);
        int ind = query_params.parameters.size() - 1;
        if (first_qc_param == 0)
          first_qc_param = ind;
        // last_qc_param = ind;
        if (!special(param))
          have_meteo_param = true;
      }

      if (not have_meteo_param)
      {
        SmartMet::Spine::Exception exception(BCP, "Operation processin failed!");
        exception.addDetail("At least one meteo parameter must be specified");
        exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PROCESSING_FAILED);
        throw exception;
      }

      const std::string crs = params.get_single<std::string>(P_CRS);
      auto transformation = crs_registry.create_transformation(DATA_CRS_NAME, crs);
      bool show_height = false;
      std::string proj_uri = "UNKNOWN";
      std::string proj_epoch_uri = "UNKNOWN";
      crs_registry.get_attribute(crs, "showHeight", &show_height);
      crs_registry.get_attribute(crs, "projUri", &proj_uri);
      crs_registry.get_attribute(crs, "projEpochUri", &proj_epoch_uri);

      uint64_t timestep = params.get_single<uint64_t>(P_TIME_STEP);
      query_params.timestep = (timestep > 0 ? timestep : 1);

      query_params.allplaces = false;

      query_params.maxdistance = params.get_single<double>(P_MAX_DISTANCE);

      query_params.timeformat = "iso";

      query_params.timezone = "UTC";

      query_params.numberofstations = params.get_single<uint64_t>(P_NUM_OF_STATIONS);

      query_params.missingtext = params.get_single<std::string>(P_MISSING_TEXT);

      query_params.localename = params.get_single<std::string>(P_LOCALE);

      query_params.starttimeGiven = true;

      params.get<int64_t>(P_WMOS, std::back_inserter(query_params.wmos));

      params.get<int64_t>(P_LPNNS, std::back_inserter(query_params.lpnns));

      params.get<int64_t>(P_FMISIDS, std::back_inserter(query_params.fmisids));

      params.get<int64_t>(P_GEOIDS, std::back_inserter(query_params.geoids));

      params.get<int64_t>(P_HOURS, std::back_inserter(query_params.hours));

      params.get<int64_t>(P_WEEK_DAYS, std::back_inserter(query_params.weekdays));

      const std::string tz_name = get_tz_name(params);

      using SmartMet::Spine::BoundingBox;
      BoundingBox requested_bbox;
      bool have_bbox = get_bounding_box(params, crs, &requested_bbox);
      std::unique_ptr<BoundingBox> query_bbox;
      if (have_bbox)
      {
        query_bbox.reset(new BoundingBox);
        *query_bbox = transform_bounding_box(requested_bbox, DATA_CRS_NAME);
        query_params.boundingBox["minx"] = query_bbox->xMin;
        query_params.boundingBox["miny"] = query_bbox->yMin;
        query_params.boundingBox["maxx"] = query_bbox->xMax;
        query_params.boundingBox["maxy"] = query_bbox->yMax;

        if (debug_level > 0)
        {
          std::ostringstream msg;
          msg << SmartMet::Spine::log_time_str() << ": [WFS] [DEBUG] Original bounding box: ("
              << requested_bbox.xMin << " " << requested_bbox.yMin << " " << requested_bbox.xMax
              << " " << requested_bbox.yMax << " " << requested_bbox.crs << ")\n";
          msg << "                                   "
              << " Converted bounding box: (" << query_bbox->xMin << " " << query_bbox->yMin << " "
              << query_bbox->xMax << " " << query_bbox->yMax << " " << query_bbox->crs << ")\n";
          std::cout << msg.str() << std::flush;
        }
      }

      if (query_params.starttime > query_params.endtime)
      {
        SmartMet::Spine::Exception exception(BCP, "Invalid time interval!");
        exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PARSING_FAILED);
        exception.addParameter("Start time", pt::to_simple_string(query_params.starttime));
        exception.addParameter("End time", pt::to_simple_string(query_params.endtime));
        throw exception;
      }

      // Avoid an attempt to dump too much data.
      unsigned maxEpochs = params.get_single<uint64_t>(P_MAX_EPOCHS);
      unsigned ts1 = (timestep ? timestep : 60);
      if (sq_restrictions and
          query_params.starttime + pt::minutes(maxEpochs * ts1) < query_params.endtime)
      {
        SmartMet::Spine::Exception exception(BCP, "Too many time epochs in the time interval!");
        exception.addDetail("Use shorter time interval or larger time step.");
        exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PROCESSING_FAILED);
        exception.addParameter("Start time", pt::to_simple_string(query_params.starttime));
        exception.addParameter("End time", pt::to_simple_string(query_params.endtime));
        throw exception;
      }

      if (sq_restrictions and max_station_count > 0 and have_bbox)
      {
        Stations stations;
        obs_engine->getStationsByBoundingBox(stations, query_params);
        if (stations.size() > max_station_count)
        {
          SmartMet::Spine::Exception exception(
              BCP,
              "Too many stations (" + std::to_string(stations.size()) + ") in the bounding box!");
          exception.addDetail("No more than " + std::to_string(max_station_count) + " is allowed.");
          exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PROCESSING_FAILED);
          throw exception;
        }
      }

      /*******************************************
       *  Finally perform the query              *
       *******************************************/

      SmartMet::Spine::TimeSeries::TimeSeriesVectorPtr obsengine_result(
          obs_engine->values(query_params));
      const bool emptyResult = (!obsengine_result || obsengine_result->size() == 0);

      CTPP::CDT hash;

      // Create index of all result rows (by observation site)
      std::map<std::string, SiteRec> site_map;
      std::map<int, GroupRec> group_map;

      // Formatting the SmartMet::Spine::TimeSeries::Value values.
      SmartMet::Spine::ValueFormatter fmt{SmartMet::Spine::ValueFormatterParam()};
      fmt.setMissingText(query_params.missingtext);
      SmartMet::Spine::TimeSeries::StringVisitor sv(fmt, 3);

      if (not emptyResult)
      {
        const SmartMet::Spine::TimeSeries::TimeSeries& ts_fmisid = obsengine_result->at(fmisid_ind);
        for (std::size_t i = 0; i < ts_fmisid.size(); i++)
        {
          // Fmisids are doubles for some reason, fix this!!!!
          sv.setPrecision(0);
          const std::string fmisid = boost::apply_visitor(sv, ts_fmisid[i].value);
          site_map[fmisid].row_index_vect.push_back(i);
        }
      }

      // Get the sequence number of query in the request
      int sq_id = query.get_query_id();

      const int num_groups = separate_groups ? site_map.size() : ((site_map.size() != 0) ? 1 : 0);

      FeatureID feature_id(get_config()->get_query_id(), params.get_map(), sq_id);

      if (separate_groups)
      {
        // Assign group IDs to stations for separate groups requested
        int group_cnt = 0;
        BOOST_FOREACH (auto& item, site_map)
        {
          const std::string& fmisid = item.first;
          const int group_id = group_cnt++;
          item.second.group_id = group_id;
          item.second.ind_in_group = 0;

          const char* place_params[] = {
              P_WMOS, P_LPNNS, P_FMISIDS, P_PLACES, P_LATLONS, P_GEOIDS, P_KEYWORD, P_BOUNDING_BOX};
          for (unsigned i = 0; i < sizeof(place_params) / sizeof(*place_params); i++)
          {
            feature_id.erase_param(place_params[i]);
          }
          feature_id.add_param(P_FMISIDS, fmisid);
          group_map[group_id].feature_id = feature_id.get_id();

          BOOST_FOREACH (const std::string& param_name, param_names)
          {
            feature_id.erase_param(P_METEO_PARAMETERS);
            feature_id.add_param(P_METEO_PARAMETERS, param_name);
            group_map[group_id].param_ids[param_name] = feature_id.get_id();
          }
        }
      }
      else
      {
        const int group_id = 0;
        int ind_in_group = 0;
        BOOST_FOREACH (auto& item, site_map)
        {
          item.second.group_id = group_id;
          item.second.ind_in_group = ind_in_group++;
        }

        group_map[group_id].feature_id = feature_id.get_id();

        BOOST_FOREACH (const std::string& param_name, param_names)
        {
          feature_id.erase_param(P_METEO_PARAMETERS);
          feature_id.add_param(P_METEO_PARAMETERS, param_name);
          group_map[group_id].param_ids[param_name] = feature_id.get_id();
        }
      }

      hash["responseTimestamp"] =
          Fmi::to_iso_extended_string(get_plugin_data().get_time_stamp()) + "Z";
      hash["numMatched"] = num_groups;
      hash["numReturned"] = num_groups;
      hash["numParam"] = param_names.size();
      hash["language"] = language;
      hash["projSrsDim"] = (show_height ? 3 : 2);
      hash["projSrsName"] = proj_uri;
      hash["projEpochSrsDim"] = (show_height ? 4 : 3);
      hash["projEpochSrsName"] = proj_epoch_uri;
      hash["queryNum"] = query.get_query_id();
      hash["queryName"] = get_query_name();

      params.dump_params(hash["query_parameters"]);
      dump_named_params(params, hash["named_parameters"]);

      hash["fmi_apikey"] = QueryBase::FMI_APIKEY_SUBST;
      hash["fmi_apikey_prefix"] = QueryBase::FMI_APIKEY_PREFIX_SUBST;
      hash["hostname"] = QueryBase::HOSTNAME_SUBST;
      hash["protocol"] = QueryBase::PROTOCOL_SUBST;

      if (not emptyResult)
      {
        const SmartMet::Spine::TimeSeries::TimeSeries& ts_lat = obsengine_result->at(lat_ind);
        const SmartMet::Spine::TimeSeries::TimeSeries& ts_lon = obsengine_result->at(lon_ind);
        const SmartMet::Spine::TimeSeries::TimeSeries& ts_height = obsengine_result->at(height_ind);
        const SmartMet::Spine::TimeSeries::TimeSeries& ts_name = obsengine_result->at(name_ind);
        const SmartMet::Spine::TimeSeries::TimeSeries& ts_dist = obsengine_result->at(dist_ind);
        const SmartMet::Spine::TimeSeries::TimeSeries& ts_direction =
            obsengine_result->at(direction_ind);
        const SmartMet::Spine::TimeSeries::TimeSeries& ts_geoid = obsengine_result->at(geoid_ind);
        const SmartMet::Spine::TimeSeries::TimeSeries& ts_wmo = obsengine_result->at(wmo_ind);

        BOOST_FOREACH (const auto& it1, site_map)
        {
          const std::string& fmisid = it1.first;
          const int group_id = it1.second.group_id;
          const int ind = it1.second.ind_in_group;
          const int row_1 = it1.second.row_index_vect.at(0);

          CTPP::CDT& group = hash["groups"][group_id];

          group["obsStationList"][ind]["fmisid"] = fmisid;

          sv.setPrecision(5);
          const std::string lat = boost::apply_visitor(sv, ts_lat[row_1].value);
          const std::string lon = boost::apply_visitor(sv, ts_lon[row_1].value);

          sv.setPrecision(1);
          const std::string height = boost::apply_visitor(sv, ts_height[row_1].value);
          const std::string name = boost::apply_visitor(sv, ts_name[row_1].value);
          const std::string distance = boost::apply_visitor(sv, ts_dist[row_1].value);
          const std::string bearing = boost::apply_visitor(sv, ts_direction[row_1].value);
          const std::string geoid = boost::apply_visitor(sv, ts_geoid[row_1].value);
          const std::string wmo = boost::apply_visitor(sv, ts_wmo[row_1].value);

          if (not lat.empty() and not lon.empty())
            set_2D_coord(transformation, lat, lon, group["obsStationList"][ind]);
          else
            throw SmartMet::Spine::Exception(BCP, "wfs: Internal LatLon query error.");

          // Region solving
          std::string region;
          if (not geoid.empty())
          {
            try
            {
              const long geoid_long = Fmi::stol(geoid);
              std::string langCode = language;
              SupportsLocationParameters::engOrFinToEnOrFi(langCode);
              SmartMet::Spine::LocationPtr geoLoc = geo_engine->idSearch(geoid_long, langCode);
              region = (geoLoc ? geoLoc->area : "");
            }
            catch (...)
            {
            }
          }

          if (show_height)
            group["obsStationList"][ind]["height"] =
                (height.empty() ? query_params.missingtext : height);
          group["obsStationList"][ind]["distance"] =
              (distance.empty() ? query_params.missingtext : distance);
          group["obsStationList"][ind]["bearing"] =
              ((distance.empty() or bearing.empty() or (distance == query_params.missingtext))
                   ? query_params.missingtext
                   : bearing);
          group["obsStationList"][ind]["name"] = name;

          if (not region.empty())
            group["obsStationList"][ind]["region"] = region;

          if (not wmo.empty())
            group["obsStationList"][ind]["wmo"] = wmo;

          (geoid.empty() or geoid == "-1")
              ? group["obsStationList"][ind]["geoid"] = query_params.missingtext
              : group["obsStationList"][ind]["geoid"] = geoid;
        }

        for (int group_id = 0; group_id < num_groups; group_id++)
        {
          int ind = 0;
          CTPP::CDT& group = hash["groups"][group_id];

          // Time values of a group.
          group["obsPhenomenonStartTime"] =
              Fmi::to_iso_extended_string(query_params.starttime) + "Z";
          group["obsPhenomenonEndTime"] = Fmi::to_iso_extended_string(query_params.endtime) + "Z";
          group["obsResultTime"] = Fmi::to_iso_extended_string(query_params.endtime) + "Z";

          group["featureId"] = group_map.at(group_id).feature_id;
          group["crs"] = crs;
          if (tz_name != "UTC")
            group["timezone"] = tz_name;
          group["timestep"] = query_params.timestep;
          if (support_quality_info)
            group["qualityInfo"] = "on";

          for (int k = first_param; k <= last_param; k++)
          {
            const int k0 = k - first_param;
            group["obsParamList"][k0]["name"] = param_names.at(k0);
            group["obsParamList"][k0]["featureId"] =
                group_map.at(group_id).param_ids.at(param_names.at(k0));

            // Mark QC parameters
            if (m_support_qc_parameters and
                SupportsQualityParameters::isQCParameter(param_names.at(k0)))
              group["obsParamList"][k0]["isQCParameter"] = "true";
          }

          // Reference id of om:phenomenonTime and om:resultTime in
          // a group (a station). The first parameter in a list
          // get the time values (above) and other ones get only
          // references to the values.
          const std::string group_id_str = str(format("%1%-%2%") % sq_id % (group_id + 1));
          group["groupId"] = group_id_str;
          group["groupNum"] = group_id + 1;

          BOOST_FOREACH (const auto& it1, site_map)
          {
            if (it1.second.group_id == group_id)
            {
              bool first = true;
              lt::time_zone_ptr tzp;

              const SmartMet::Spine::TimeSeries::TimeSeries& ts_epoch =
                  obsengine_result->at(first_param);
              BOOST_FOREACH (int row_num, it1.second.row_index_vect)
              {
                static const long ref_jd = boost::gregorian::date(1970, 1, 1).julian_day();

                sv.setPrecision(5);
                const std::string latitude = boost::apply_visitor(sv, ts_lat[row_num].value);
                const std::string longitude = boost::apply_visitor(sv, ts_lon[row_num].value);

                if (first)
                {
                  tzp = get_tz_for_site(std::stod(longitude), std::stod(latitude), tz_name);
                }

                CTPP::CDT obs_rec;
                set_2D_coord(transformation, latitude, longitude, obs_rec);

                if (show_height)
                {
                  sv.setPrecision(1);
                  const std::string height = boost::apply_visitor(sv, ts_height[row_num].value);
                  obs_rec["height"] = (height.empty() ? query_params.missingtext : height);
                }

                pt::ptime epoch = ts_epoch.at(row_num).time.utc_time();
                // Cannot use pt::time_difference::total_seconds() directly as it returns long and
                // could overflow.
                long long jd = epoch.date().julian_day();
                long seconds = epoch.time_of_day().total_seconds();
                INT_64 s_epoch = 86400LL * (jd - ref_jd) + seconds;
                obs_rec["epochTime"] = s_epoch;
                obs_rec["epochTimeStr"] = format_local_time(epoch, tzp);

                for (int k = first_param; k <= last_param; k++)
                {
                  const SmartMet::Spine::TimeSeries::TimeSeries& ts_k = obsengine_result->at(k);
                  const uint precision =
                      get_meteo_parameter_options(param_names.at(k - first_param))->precision;

                  sv.setPrecision(precision);
                  const std::string value = boost::apply_visitor(sv, ts_k[row_num].value);
                  obs_rec["data"][k - first_param]["value"] = value;

                  // QC data if requested.
                  if (qc_info_param_names.size() > 0)
                  {
                    std::stringstream qc_value;
                    const int qc_k = first_qc_param + k - first_param;
                    const SmartMet::Spine::TimeSeries::TimeSeries& ts_qc_k =
                        obsengine_result->at(qc_k);
                    sv.setPrecision(0);
                    const std::string value_qc = boost::apply_visitor(sv, ts_qc_k[row_num].value);
                    obs_rec["data"][k - first_param]["qcValue"] = value_qc;
                  }
                }

                group["obsReturnArray"][ind++] = obs_rec;
              }
            }
          }
        }
      }

      format_output(hash, output, query.get_use_debug_format());
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

}  // namespace WFS
}  // namespace Plugin
}  // namespace SmartMet

namespace
{
using namespace SmartMet::Plugin::WFS;

boost::shared_ptr<SmartMet::Plugin::WFS::StoredQueryHandlerBase> wfs_obs_handler_create(
    SmartMet::Spine::Reactor* reactor,
    boost::shared_ptr<StoredQueryConfig> config,
    PluginData& plugin_data,
    boost::optional<std::string> template_file_name)
{
  try
  {
    StoredObsQueryHandler* qh =
        new StoredObsQueryHandler(reactor, config, plugin_data, template_file_name);
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

SmartMet::Plugin::WFS::StoredQueryHandlerFactoryDef wfs_obs_handler_factory(
    &wfs_obs_handler_create);

/**

   @page WFS_SQ_GENERIC_OBS Stored query handler for querying observation data

   @section WFS_SQ_GENERIC_OBS_INTRO Introduction

   Stored query handler for accessing observation data provides access to weather
   observations (except lightning observations)

   <table border="1">
   <tr>
   <td>Implementation</td>
   <td>SmartMet::Plugin::WFS::StoredObsQueryHandler</td>
   </tr>
   <tr>
   <td>Constructor name (for stored query configuration)</td>
   <td>@b wfs_obs_handler_factory</td>
   </tr>
   </table>

   @section WFS_SQ_GENERIC_OBS_PARAM Stored query handler built-in parameters

   The following stored query handler parameter groups are being used by this stored query handler:
   - @ref WFS_SQ_LOCATION_PARAMS
   - @ref WFS_SQ_PARAM_BBOX
   - @ref WFS_SQ_TIME_ZONE
   - @ref WFS_SQ_QUALITY_PARAMS
   - @ref WFS_SQ_METEO_PARAM_OPTIONS

   Additionally to parameters from these groups the following parameters are also in use

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
   <td>Specifies the meteo parameters to query. At least one parameter must be specified</td>
   </tr>

   <tr>
   <td>stationType</td>
   <td>@ref WFS_CFG_SCALAR_PARAM_TMPL</td>
   <td>string</td>
   <td>Specifies station type.</td>
   </tr>

   <tr>
   <td>timeStep</td>
   <td>@ref WFS_CFG_SCALAR_PARAM_TMPL</td>
   <td>unsigned integer</td>
   <td>Specifies time stamp for querying observations</td>
   </tr>

   <tr>
   <td>wmos</td>
   <td>@ref WFS_CFG_ARRAY_PARAM_TMPL</td>
   <td>integer</td>
   <td>Specifies stations WMO codes</td>
   </tr>

   <tr>
   <td>lpnns</td>
   <td>@ref WFS_CFG_ARRAY_PARAM_TMPL</td>
   <td>integer</td>
   <td>Specifies stations LPPN codes</td>
   </tr>

   <tr>
   <td>fmisids</td>
   <td>@ref WFS_CFG_ARRAY_PARAM_TMPL</td>
   <td>integer</td>
   <td>Specifies stations FMISID codes</td>
   </tr>

   <tr>
   <td>numOfStations</td>
   <td>@ref WFS_CFG_SCALAR_PARAM_TMPL</td>
   <td>unsigned integer</td>
   <td>Specifies how many nearest stations to select with distance less than @b maxDistance
   (see @ref WFS_SQ_LOCATION_PARAMS "location parameters" for details)</td>
   </tr>

   <tr>
   <td>hours</td>
   <td>@ref WFS_CFG_ARRAY_PARAM_TMPL</td>
   <td>integer</td>
   <td>Specifies hours for which to return data if non empty array provided</td>
   </tr>

   <tr>
   <td>weekDays</td>
   <td>@ref WFS_CFG_ARRAY_PARAM_TMPL</td>
   <td>integer</td>
   <td>Week days for which to return data if non empty array is provided</td>
   </tr>

   <tr>
   <td>locale</td>
   <td>@ref WFS_CFG_SCALAR_PARAM_TMPL</td>
   <td>string</td>
   <td>The locale code (like 'fi_FI.utf8')</td>
   </tr>

   <tr>
   <td>missingText</td>
   <td>@ref WFS_CFG_SCALAR_PARAM_TMPL</td>
   <td>string</td>
   <td>Text to use to indicate missing value</td>
   </tr>

   <tr>
   <td>maxEpochs</td>
   <td>@ref WFS_CFG_SCALAR_PARAM_TMPL</td>
   <td>unsigned integer</td>
   <td>Maximal number of epochs that can be returned. If the estimated number
   before query is larger than the specified one, query is aborted with
   and processing error. This parameter is not valid, if the *optional*
   storedQueryRestrictions parameter is set to false in WFS Plugin
   configurations.</td>
   </tr>

   <tr>
   <td>CRS</td>
   <td>@ref WFS_CFG_SCALAR_PARAM_TMPL</td>
   <td>string</td>
   <td>Specifies coordinate projection to use</td>
   </tr>

   </table>

   @section WFS_SQ_GENERIC_OBS_EXTRA_CFG_PARAM Additional parameters

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
   <td>Specifies maximal permitted time interval in hours. The default value
   is 168 hours (7 days). This parameter is not valid, if the *optional*
   storedQueryRestrictions parameter is set to false in WFS Plugin
   configurations.</td>
   </tr>

   <tr>
   <td>maxStationCount</td>
   <td>unsigned integer</td>
   <td>optional</td>
   <td>Specifies maximal permitted number of stations. This count is currently checked for bounding
   boxes only.
      The ProcessingError exception report is returned if the actual count exceeds the specified
one.
   The default value 0 means unlimited. The default value is also used,
   if the *optional* storedQueryRestrictions parameter is set to false in
   WFS Plugin configurations.</td>
   </tr>

   <tr>
   <td>separateGroups</td>
   <td>boolean</td>
   <td>optional (default @b false)</td>
   <td>Providing value @b true tells handler to split returned observations into separate groups
   by station (separate group for each station)</td>
   </tr>

   <tr>
   <td>supportQCParameters</td>
   <td>boolean</td>
   <td>optional (default @b false)</td>
   <td>Providing value @b true tells handler to allow meteoParameters with "qc_" prefix to
   query.</td>
   </tr>

   </table>

*/
