#include "stored_queries/StoredForecastQueryHandler.h"
#include "FeatureID.h"
#include "StoredQueryHandlerFactoryDef.h"
#include "WfsConvenience.h"
#include <boost/algorithm/string.hpp>
#include <boost/format.hpp>
#include <macgyver/StringConversion.h>
#include <macgyver/TimeFormatter.h>
#include <macgyver/TypeName.h>
#include <newbase/NFmiPoint.h>
#include <newbase/NFmiQueryData.h>
#include <smartmet/engines/querydata/MetaQueryOptions.h>
#include <smartmet/spine/Convenience.h>
#include <smartmet/spine/Exception.h>
#include <smartmet/spine/ParameterFactory.h>
#include <smartmet/spine/Table.h>
#include <smartmet/spine/TimeSeriesGenerator.h>
#include <smartmet/spine/TimeSeriesOutput.h>
#include <smartmet/spine/Value.h>
#include <limits>
#include <locale>
#include <map>

namespace bw = SmartMet::Plugin::WFS;
namespace ba = boost::algorithm;
namespace pt = boost::posix_time;
namespace lt = boost::local_time;
using namespace SmartMet::Spine;

using boost::format;
using boost::str;

const char* bw::StoredForecastQueryHandler::P_MODEL = "model";
const char* bw::StoredForecastQueryHandler::P_ORIGIN_TIME = "originTime";
const char* bw::StoredForecastQueryHandler::P_LEVEL_HEIGHTS = "levelHeights";
const char* bw::StoredForecastQueryHandler::P_LEVEL = "level";
const char* bw::StoredForecastQueryHandler::P_LEVEL_TYPE = "levelType";
const char* bw::StoredForecastQueryHandler::P_PARAM = "param";
const char* bw::StoredForecastQueryHandler::P_FIND_NEAREST_VALID = "findNearestValid";
const char* bw::StoredForecastQueryHandler::P_LOCALE = "locale";
const char* bw::StoredForecastQueryHandler::P_MISSING_TEXT = "missingText";
const char* bw::StoredForecastQueryHandler::P_CRS = "crs";

bw::StoredForecastQueryHandler::StoredForecastQueryHandler(
    SmartMet::Spine::Reactor* reactor,
    boost::shared_ptr<bw::StoredQueryConfig> config,
    PluginData& plugin_data,
    boost::optional<std::string> template_file_name)
    : SupportsExtraHandlerParams(config, false),
      bw::StoredQueryHandlerBase(reactor, config, plugin_data, template_file_name),
      bw::SupportsLocationParameters(config, SUPPORT_KEYWORDS | INCLUDE_GEOIDS),
      bw::SupportsTimeParameters(config),
      bw::SupportsTimeZone(config),
      geo_engine(nullptr),
      q_engine(nullptr),
      common_params(),
      ind_geoid(SmartMet::add_param(common_params, "geoid", Parameter::Type::DataIndependent)),
      ind_epoch(SmartMet::add_param(common_params, "time", Parameter::Type::DataIndependent)),
      ind_place(SmartMet::add_param(common_params, "name", Parameter::Type::DataIndependent)),
      ind_lat(SmartMet::add_param(common_params, "latitude", Parameter::Type::DataIndependent)),
      ind_lon(SmartMet::add_param(common_params, "longitude", Parameter::Type::DataIndependent)),
      ind_elev(SmartMet::add_param(common_params, "elevation", Parameter::Type::DataIndependent)),
      ind_level(SmartMet::add_param(common_params, "level", Parameter::Type::DataIndependent)),
      ind_region(SmartMet::add_param(common_params, "region", Parameter::Type::DataIndependent)),
      ind_country(SmartMet::add_param(common_params, "country", Parameter::Type::DataIndependent)),
      ind_country_iso(SmartMet::add_param(common_params, "iso2", Parameter::Type::DataIndependent)),
      ind_localtz(SmartMet::add_param(common_params, "localtz", Parameter::Type::DataIndependent))
{
  try
  {
    register_array_param<std::string>(P_MODEL, *config);
    register_scalar_param<pt::ptime>(P_ORIGIN_TIME, *config, false);
    register_array_param<double>(P_LEVEL_HEIGHTS, *config, 0, 99);
    register_array_param<int64_t>(P_LEVEL, *config, 0, 99);
    register_scalar_param<std::string>(P_LEVEL_TYPE, *config);
    register_array_param<std::string>(P_PARAM, *config, 1, 99);
    register_scalar_param<int64_t>(P_FIND_NEAREST_VALID, *config);
    register_scalar_param<std::string>(P_LOCALE, *config);
    register_scalar_param<std::string>(P_MISSING_TEXT, *config);
    register_scalar_param<std::string>(P_CRS, *config);

    max_np_distance = config->get_optional_config_param<double>("maxNpDistance", -1.0);
    separate_groups = config->get_optional_config_param<bool>("separateGroups", false);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

bw::StoredForecastQueryHandler::~StoredForecastQueryHandler() {}

void bw::StoredForecastQueryHandler::init_handler()
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

void bw::StoredForecastQueryHandler::query(const StoredQuery& stored_query,
                                           const std::string& language,
                                           std::ostream& output) const
{
  try
  {
    namespace pt = boost::posix_time;
    using namespace SmartMet;
    using namespace SmartMet::Plugin::WFS;

    static const long ref_jd = boost::gregorian::date(1970, 1, 1).julian_day();

    int debug_level = get_config()->get_debug_level();
    if (debug_level > 0)
      stored_query.dump_query_info(std::cout);

    bw::StoredForecastQueryHandler::Query query;

    query.language = language;
    const auto& params = stored_query.get_param_map();

    try
    {
      // Parsing request parameters
      try
      {
        SmartMet::Spine::ValueFormatterParam vf_param;

        query.missing_text = params.get_single<std::string>(P_MISSING_TEXT);
        vf_param.missingText = query.missing_text;
        query.set_locale(params.get_single<std::string>(P_LOCALE));
        query.max_distance = params.get_single<double>(P_MAX_DISTANCE);
        query.max_np_distance = max_np_distance > 0.0 ? max_np_distance : query.max_distance;
        query.keyword = params.get_optional<std::string>(P_KEYWORD, "");
        query.find_nearest_valid_point = params.get_single<int64_t>(P_FIND_NEAREST_VALID) != 0;
        query.tz_name = get_tz_name(params);

        parse_models(params, query);
        get_location_options(geo_engine, params, query.language, &query.locations);

        parse_level_heights(params, query);
        parse_levels(params, query);
        parse_params(params, query);
        query.value_formatter.reset(new SmartMet::Spine::ValueFormatter(vf_param));
        query.time_formatter.reset(Fmi::TimeFormatter::create("iso"));
        parse_times(params, query);

        const std::string crs = params.get_single<std::string>(P_CRS);
        auto transformation =
            plugin_data.get_crs_registry().create_transformation("urn:ogc:def:crs:EPSG::4326", crs);
        bool show_height = false;
        std::string proj_uri = "UNKNOWN";
        std::string proj_epoch_uri = "UNKNOWN";
        plugin_data.get_crs_registry().get_attribute(crs, "showHeight", &show_height);
        plugin_data.get_crs_registry().get_attribute(crs, "projUri", &proj_uri);
        plugin_data.get_crs_registry().get_attribute(crs, "projEpochUri", &proj_epoch_uri);

        query.result = extract_forecast(query);
        const std::size_t num_rows = query.result->rows().size();

        std::set<std::string> geo_id_set;
        std::multimap<int, std::string> group_map;
        std::multimap<std::string, std::size_t> result_map;
        for (std::size_t i = 0; i < num_rows; i++)
        {
          const std::string geo_id = query.result->get(ind_geoid, i);
          result_map.insert(std::make_pair(geo_id, i));
          geo_id_set.insert(geo_id);
        }

        std::size_t num_groups = 0;
        BOOST_FOREACH (const auto& geo_id, geo_id_set)
        {
          group_map.insert(std::make_pair(num_groups, geo_id));
          if (separate_groups)
            num_groups++;
        }

        if (not separate_groups and (geo_id_set.size() > 0))
          num_groups++;

        CTPP::CDT hash;

        hash["language"] = language;

        hash["responseTimestamp"] =
            Fmi::to_iso_extended_string(get_plugin_data().get_time_stamp()) + "Z";
        hash["numMatched"] = num_groups;
        hash["numReturned"] = num_groups;
        hash["numParam"] = query.last_data_ind - query.first_data_ind + 1;

        hash["fmi_apikey"] = bw::QueryBase::FMI_APIKEY_SUBST;
        hash["fmi_apikey_prefix"] = bw::QueryBase::FMI_APIKEY_PREFIX_SUBST;
        hash["hostname"] = QueryBase::HOSTNAME_SUBST;
        hash["protocol"] = QueryBase::PROTOCOL_SUBST;
        hash["srsName"] = proj_uri;
        hash["srsDim"] = show_height ? 3 : 2;
        hash["srsEpochName"] = proj_epoch_uri;
        hash["srsEpochDim"] = show_height ? 4 : 3;

        int sq_id = stored_query.get_query_id();
        const char* location_params[] = {P_PLACES, P_LATLONS, P_GEOIDS, P_KEYWORD};

        for (std::size_t group_id = 0; group_id < num_groups; group_id++)
        {
          std::size_t station_ind = 0;
          auto site_range = group_map.equal_range(group_id);
          CTPP::CDT& group = hash["groups"][group_id];

          FeatureID feature_id(get_config()->get_query_id(), params.get_map(), sq_id);
          for (std::size_t i = 0; i < sizeof(location_params) / sizeof(*location_params); i++)
          {
            feature_id.erase_param(location_params[i]);
          }
          auto geoid_range = group_map.equal_range(group_id);
          for (auto it = geoid_range.first; it != geoid_range.second; ++it)
          {
            feature_id.add_param(P_GEOIDS, it->second);
          }
          group["featureId"] = feature_id.get_id();

          group["groupId"] = str(format("%1%-%2%") % sq_id % (group_id + 1));
          if (query.origin_time)
          {
            const std::string origin_time_str =
                Fmi::to_iso_extended_string(*query.origin_time) + "Z";
            group["dataOriginTime"] = origin_time_str;
          }

          if (query.modification_time != boost::posix_time::not_a_date_time)
          {
            const std::string modification_time_str =
                Fmi::to_iso_extended_string(query.modification_time) + "Z";
            group["resultTime"] = modification_time_str;
          }

          for (std::size_t i = query.first_data_ind; i <= query.last_data_ind; i++)
          {
            const std::string& name = query.data_params.at(i).name();
            feature_id.erase_param(P_PARAM);
            feature_id.add_param(P_PARAM, name);
            group["paramList"][i - query.first_data_ind]["name"] = name;
            group["paramList"][i - query.first_data_ind]["featureId"] = feature_id.get_id();
          }

          group["metadata"]["boundingBox"]["lowerLeft"]["lat"] = query.bottom_left.Y();
          group["metadata"]["boundingBox"]["lowerLeft"]["lon"] = query.bottom_left.X();

          group["metadata"]["boundingBox"]["lowerRight"]["lat"] = query.bottom_right.Y();
          group["metadata"]["boundingBox"]["lowerRight"]["lon"] = query.bottom_right.X();

          group["metadata"]["boundingBox"]["upperLeft"]["lat"] = query.top_left.Y();
          group["metadata"]["boundingBox"]["upperLeft"]["lon"] = query.top_left.X();

          group["metadata"]["boundingBox"]["upperRight"]["lat"] = query.top_right.Y();
          group["metadata"]["boundingBox"]["upperRight"]["lon"] = query.top_right.X();

          group["producerName"] = query.producer_name;
#ifdef ENABLE_MODEL_PATH
          group["modelPath"] = query.model_path;
#endif

          std::size_t row_counter = 0;

          pt::ptime interval_begin = boost::date_time::pos_infin;
          pt::ptime interval_end = boost::date_time::neg_infin;

          for (auto site_iter = site_range.first; site_iter != site_range.second; ++site_iter)
          {
            const auto& geo_id = site_iter->second;
            auto row_range = result_map.equal_range(geo_id);
            lt::time_zone_ptr tzp;
            for (auto row_iter = row_range.first; row_iter != row_range.second; ++row_iter)
            {
              std::size_t i = row_iter->second;
              if (row_iter == row_range.first)
              {
                const std::string name = query.result->get(ind_place, i);
                const std::string country_code = query.result->get(ind_country_iso, i);
                const std::string country = query.result->get(ind_country, i);
                const std::string region = query.result->get(ind_region, i);
                const std::string localtz = query.result->get(ind_localtz, i);
                const double latitude = Fmi::stod(query.result->get(ind_lat, i));
                const double longitude = Fmi::stod(query.result->get(ind_lon, i));
                tzp = get_tz_for_site(longitude, latitude, query.tz_name);
                group["stationList"][station_ind]["geoid"] = geo_id;
                group["stationList"][station_ind]["name"] = name;
                set_2D_coord(
                    transformation, latitude, longitude, group["stationList"][station_ind]);
                if (show_height)
                {
                  group["stationList"][station_ind]["elev"] = query.result->get(ind_elev, i);
                  ;
                }

                group["stationList"][station_ind]["iso2"] = country_code;
                group["stationList"][station_ind]["country"] = country;
                group["stationList"][station_ind]["localtz"] = localtz;
                if ((country_code == "FI") or (name != region))
                {
                  group["stationList"][station_ind]["region"] = region;
                }

                station_ind++;
              }

              CTPP::CDT& row_data = group["returnArray"][row_counter++];

              set_2D_coord(transformation,
                           query.result->get(ind_lat, i),
                           query.result->get(ind_lon, i),
                           row_data);

              if (show_height)
              {
                row_data["elev"] = query.result->get(ind_level, i);
              }

              pt::ptime epoch = pt::from_iso_string(query.result->get(ind_epoch, i));
              long long jd = epoch.date().julian_day();
              long seconds = epoch.time_of_day().total_seconds();
              INT_64 s_epoch = 86400LL * (jd - ref_jd) + seconds;
              row_data["epochTime"] = s_epoch;
              row_data["epochTimeStr"] = format_local_time(epoch, tzp);

              for (std::size_t k = query.first_data_ind; k <= query.last_data_ind; k++)
              {
                row_data["data"][k - query.first_data_ind] =
                    remove_trailing_0(query.result->get(k, i));
              }

              interval_begin = i == 0 ? epoch : std::min(interval_begin, epoch);
              interval_end = i == 0 ? epoch : std::max(interval_end, epoch);
            }
          }

          group["phenomenonStartTime"] = Fmi::to_iso_extended_string(interval_begin) + "Z";
          group["phenomenonEndTime"] = Fmi::to_iso_extended_string(interval_end) + "Z";
        }

        format_output(hash, output, stored_query.get_use_debug_format());
      }
      catch (...)
      {
        throw SmartMet::Spine::Exception::Trace(BCP, "Operation parsing failed!")
            .addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PARSING_FAILED);
      }
    }
    catch (...)
    {
      SmartMet::Spine::Exception exception(BCP, "Operation parsing failed!", nullptr);
      if (exception.getExceptionByParameterName(WFS_EXCEPTION_CODE) == nullptr)
        exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PARSING_FAILED);
      exception.addParameter(WFS_LANGUAGE, query.language);
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
struct StationRec
{
  std::string geoid;
  std::string name;
  std::string lat;
  std::string lon;
  std::string elev;
};
}  // namespace

boost::shared_ptr<SmartMet::Spine::Table> bw::StoredForecastQueryHandler::extract_forecast(
    Query& query) const
{
  try
  {
    using namespace SmartMet;

    int debug_level = get_config()->get_debug_level();

    boost::shared_ptr<SmartMet::Spine::Table> ennusteet(new SmartMet::Spine::Table);

#ifdef ENABLE_MODEL_PATH
    boost::optional<std::string> model_path;
#endif

    std::string language = Fmi::ascii_tolower_copy(query.language);
    SupportsLocationParameters::engOrFinToEnOrFi(language);

    decltype(query.origin_time) origin_time;
    if (query.origin_time)
    {
      origin_time.reset(new pt::ptime(*query.origin_time));
    }

    int row = 0;
    BOOST_FOREACH (const auto& tloc, query.locations)
    {
      SmartMet::Spine::LocationPtr loc = tloc.second;
      const std::string place = tloc.second->name;
      const std::string country = geo_engine->countryName(loc->iso2, language);
      if (debug_level > 0)
      {
        std::cout << "Location: " << loc->name << " in " << country << std::endl;
      }

      SmartMet::Engine::Querydata::Producer producer = select_producer(*loc, query);

      if (debug_level > 0)
      {
        std::cout << "Selected producer: " << producer << std::endl;
      }

      if (producer.empty())
      {
        if (query.keyword.empty())
        {
          SmartMet::Spine::Exception exception(BCP, "No data available for '" + loc->name + "'!");
          exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PROCESSING_FAILED);
          throw exception;
        }
      }

      auto q = (origin_time ? q_engine->get(producer, *origin_time) : q_engine->get(producer));
      query.modification_time = q->modificationTime();

// FIXME: try to use the same model instead of searching model again
#ifdef ENABLE_MODEL_PATH
      if (model_path and q->path().string() != *model_path)
      {
        SmartMet::Spine::Exception exception(BCP, "Weather model mixing is not allowed!");
        exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PROCESSING_FAILED);
        exception.addParameter("Config name", configName);
        throw exception;
      }

      model_path = q->path().string();
#endif

      if (not query.origin_time)
      {
        // With multifile data q_engine->get() origintime must not be set/locked

        query.origin_time.reset(new pt::ptime(q->originTime()));

        if (not q_engine->getProducerConfig(producer).ismultifile)
        {
          origin_time.reset(new pt::ptime(*query.origin_time));
        }
      }

      query.have_model_area = false;

      if (q->isArea())
      {
        const NFmiArea& area = q->area();
        query.have_model_area = true;
        query.top_left = area.TopLeftLatLon();
        query.top_right = area.TopRightLatLon();
        query.bottom_left = area.BottomLeftLatLon();
        query.bottom_right = area.BottomRightLatLon();
      }

      if (debug_level > 0 and not query.have_model_area)
      {
        std::ostringstream msg;
        msg << METHOD_NAME << ": WARNING: model area not available";
#ifdef ENABLE_MODEL_PATH
        if (model_path)
          msg << " for " << *model_path;
#endif
        msg << std::endl;
        std::cout << msg.str() << std::flush;
      }

      // If we accept nearest valid points, find it now for this location
      // This is fast if the nearest point is already valid

      NFmiPoint nearestpoint(kFloatMissing, kFloatMissing);
      if (query.find_nearest_valid_point)
      {
        NFmiPoint latlon(loc->longitude, loc->latitude);
        // Querydata is using kilometers and WFS meters for a distance.
        nearestpoint = q->validPoint(latlon, query.max_np_distance / 1000.0);
      }

      if (debug_level > 0)
      {
        std::cout << "Producer : " << producer << std::endl;
#ifdef ENABLE_MODEL_PATH
        std::cout << "Selected model: " << q->path() << std::endl;
#endif
      }

      query.producer_name = producer;
#ifdef ENABLE_MODEL_PATH
      query.model_path = q->path().string();
#endif

      const std::string zone = "UTC";

      query.toptions->setDataTimes(q->validTimes(), q->isClimatology());
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
      const auto param_map = get_model_parameters(producer, q->originTime());
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

      if (not query.level_heights.empty() and q->levelType() != FmiLevelType::kFmiHybridLevel)
      {
        Spine::Exception exception(
            BCP, "Only hybrid data supports data fetching from an arbitrary height.");
        exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PROCESSING_FAILED);
        throw exception;
      }

      if (not query.levels.empty() and not query.level_heights.empty())
      {
        Spine::Exception exception(BCP,
                                   "Fetching data from a level and an arbitrary height is not "
                                   "supported in a same request.");
        exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PROCESSING_FAILED);
        throw exception;
      }

      /*
      if (query.levels.empty() and query.level_heights.empty())
      {
        Spine::Exception exception(BCP, "No level selected.");
        exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PROCESSING_FAILED);
        throw exception;
      }
      */

      // Fetch data from an arbitrary height.
      for (const auto& level_height : query.level_heights)
      {
        for (const lt::local_date_time& dt : tlist)
        {
          using SmartMet::Spine::Parameter;

          int column = 0;
          for (const Parameter& param : query.data_params)
          {
            int prec = param_precision_map.at(param.name());

            const std::string timestring = "";

            SmartMet::Engine::Querydata::ParameterOptions qengine_param(
                param,
                producer,
                *loc,
                country,
                place,
                *query.time_formatter,
                timestring,
                query.language,
                *query.output_locale,
                zone,
                query.find_nearest_valid_point,
                nearestpoint,
                query.lastpoint);
            SmartMet::Spine::TimeSeries::Value val =
                q->valueAtHeight(qengine_param, dt, level_height);

            std::stringstream ss;
            SmartMet::Spine::TimeSeries::OStreamVisitor osv(ss, *query.value_formatter, prec);
            boost::apply_visitor(osv, val);

            ennusteet->set(column, row, ss.str());

            ++column;
          }

          ++row;
        }
      }

      for (q->resetLevel(); q->nextLevel() and query.level_heights.empty();)
      {
        if (query.levels.empty() || query.levels.count(static_cast<int>(q->levelValue())) > 0)
        {
          BOOST_FOREACH (const lt::local_date_time& d, tlist)
          {
            using SmartMet::Spine::Parameter;

            int column = 0;
            BOOST_FOREACH (const Parameter& param, query.data_params)
            {
              int prec = param_precision_map.at(param.name());

              const std::string timestring = "";

              SmartMet::Engine::Querydata::ParameterOptions qengine_param(
                  param,
                  producer,
                  *loc,
                  country,
                  place,
                  *query.time_formatter,
                  timestring,
                  query.language,
                  *query.output_locale,
                  zone,
                  query.find_nearest_valid_point,
                  nearestpoint,
                  query.lastpoint);
              SmartMet::Spine::TimeSeries::Value val = q->value(qengine_param, d);

              std::stringstream ss;
              SmartMet::Spine::TimeSeries::OStreamVisitor osv(ss, *query.value_formatter, prec);
              boost::apply_visitor(osv, val);

              ennusteet->set(column, row, ss.str());

              ++column;
            }

            ++row;
          }
        }
      }
    }

    return ennusteet;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

SmartMet::Engine::Querydata::Producer bw::StoredForecastQueryHandler::select_producer(
    const SmartMet::Spine::Location& location, const Query& query) const
{
  try
  {
    SmartMet::Engine::Querydata::Producer producer;

    const bool use_data_specific_max_distance = false;
    if (query.models.empty())
    {
      // Querydata is using kilometers and WFS meters for a distance.
      producer = q_engine->find(location.longitude,
                                location.latitude,
                                query.max_distance / 1000.0,
                                use_data_specific_max_distance,
                                query.level_type);
    }
    else
    {
      producer = q_engine->find(query.models,
                                location.longitude,
                                location.latitude,
                                query.max_distance / 1000.0,
                                use_data_specific_max_distance,
                                query.level_type);
    }

    if (producer.empty() && query.keyword.empty())
    {
      throw SmartMet::Spine::Exception(BCP, "No data available for '" + location.name + "'!")
          .addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PROCESSING_FAILED)
          .disableStackTrace();
    }

    return producer;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void bw::StoredForecastQueryHandler::parse_models(const RequestParameterMap& params,
                                                  StoredForecastQueryHandler::Query& dest) const
{
  try
  {
    dest.models.clear();
    params.get<std::string>(P_MODEL, std::back_inserter(dest.models));
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void bw::StoredForecastQueryHandler::parse_level_heights(const RequestParameterMap& params,
                                                         Query& dest) const
{
  try
  {
    dest.level_heights.clear();
    std::vector<double> heights;
    params.get<double>(P_LEVEL_HEIGHTS, std::back_inserter(heights));
    for (const auto& item : heights)
    {
      float tmp = static_cast<float>(item);
      if (!dest.level_heights.insert(tmp).second)
      {
        SmartMet::Spine::Exception exception(
            BCP, "Duplicate geometric height '" + std::to_string(tmp) + "'!");
        exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PARSING_FAILED);
        throw exception;
      }
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Geometric height parsing failed!");
  }
}
void bw::StoredForecastQueryHandler::parse_levels(const RequestParameterMap& params,
                                                  Query& dest) const
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

void bw::StoredForecastQueryHandler::parse_times(const RequestParameterMap& param,
                                                 Query& dest) const
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

void bw::StoredForecastQueryHandler::parse_params(const RequestParameterMap& param,
                                                  Query& dest) const
{
  try
  {
    using SmartMet::Spine::Parameter;
    using SmartMet::Spine::ParameterFactory;

    dest.data_params = common_params;

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

std::map<std::string, SmartMet::Engine::Querydata::ModelParameter>
bw::StoredForecastQueryHandler::get_model_parameters(const std::string& producer,
                                                     const pt::ptime& origin_time) const
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

bw::StoredForecastQueryHandler::Query::Query()
    : max_distance(20000.0),
      missing_text("nan"),
      language("lan"),
      find_nearest_valid_point(false),
      value_formatter(new SmartMet::Spine::ValueFormatter(SmartMet::Spine::ValueFormatterParam())),
      have_model_area(false),
      top_left(NFmiPoint::gMissingLatlon),
      top_right(NFmiPoint::gMissingLatlon),
      bottom_left(NFmiPoint::gMissingLatlon),
      bottom_right(NFmiPoint::gMissingLatlon),
      first_data_ind(0),
      last_data_ind(0)
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

bw::StoredForecastQueryHandler::Query::~Query() {}

void bw::StoredForecastQueryHandler::Query::set_locale(const std::string& locale_name)
{
  try
  {
    try
    {
      output_locale.reset(new std::locale(locale_name.c_str()));
    }
    catch (...)
    {
      SmartMet::Spine::Exception exception(
          BCP, "Failed to set locale '" + locale_name + "'!", nullptr);
      if (exception.getExceptionByParameterName(WFS_EXCEPTION_CODE) == nullptr)
        exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PROCESSING_FAILED);
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

boost::shared_ptr<SmartMet::Plugin::WFS::StoredQueryHandlerBase> wfs_forecast_handler_create(
    SmartMet::Spine::Reactor* reactor,
    boost::shared_ptr<StoredQueryConfig> config,
    PluginData& plugin_data,
    boost::optional<std::string> template_file_name)
{
  try
  {
    StoredForecastQueryHandler* qh =
        new StoredForecastQueryHandler(reactor, config, plugin_data, template_file_name);
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

SmartMet::Plugin::WFS::StoredQueryHandlerFactoryDef wfs_forecast_handler_factory(
    &wfs_forecast_handler_create);

/**

@page WFS_SQ_FORECAST_QUERY_HANDLER Stored Query handler for querying weather forecast data

@section WFS_SQ_FORECAST_QUERY_HANDLER_INTRO Introduction

This stored query handler provides support of querying weather forecast data from
<a href="../../../engines/querydata/index.html">Querydata</a>

<table border="1">
  <tr>
    <td>Implementation</td>
    <td>SmartMet::Plugin::WFS::StoredForecastQueryHandler</td>
  </tr>
  <tr>
    <td>constructor name (for stored query configuration)</td>
    <td>@b wfs_forecast_handler_factory</td>
</table>

@section WFS_SQ_FORECAST_QUERY_HANDLER_PARAMS Query handler built-in parameters

The following stored query handler parameter groups are being used by this stored query handler:
- @ref WFS_SQ_LOCATION_PARAMS
- @ref WFS_SQ_TIME_PARAMS
- @ref WFS_SQ_TIME_ZONE

Additionally to parameters from these groups the following parameters are also in use

<table border="1">

<tr>
<th>Entry name</th>
<th>Type</th>
<th>Data type</th>
<th>Description</th>
</tr>

<tr>
  <td>model</td>
  <td>@ref WFS_CFG_ARRAY_PARAM_TMPL</td>
  <td>string</td>
  <td>Specifies weather models to use. Specifying an empty array allows to use all models
      available to Querydata</td>
</tr>

<tr>
  <td>originTime</td>
  <td>@ref WFS_CFG_SCALAR_PARAM_TMPL</td>
  <td>time</td>
  <td>Specifies origin time of weather models to use when specified. May be omitted in query.
      All available origin time values are acceptable when omitted</td>
</tr>

<tr>
  <td>level</td>
  <td>@ref WFS_CFG_ARRAY_PARAM_TMPL</td>
  <td>integer</td>
  <td>Model levels to query.</td>
</tr>

<tr>
  <td>levelHeights</td>
  <td>@ref WFS_CFG_ARRAY_PARAM_TMPL</td>
  <td>doulbeList</td>
  <td>Arbitrary level heighs to query.</td>
</tr>

<tr>
  <td>levelType</td>
  <td>@ref WFS_CFG_SCALAR_PARAM_TMPL</td>
  <td>string</td>
  <td>Specifies model level typo to use/td>
</tr>

<tr>
  <td>findNearestValid</td>
  <td>@ref WFS_CFG_SCALAR_PARAM_TMPL</td>
  <td>integer</td>
  <td>Specifying non zero value causes look-up of nearest point from model</td>
</tr>

<tr>
  <td>locale</td>
  <td>@ref WFS_CFG_SCALAR_PARAM_TMPL</td>
  <td>string</td>
  <td>Specifies locale to use (for example @b fi_FI.utf8 )</td>
</tr>

<tr>
  <td>missingText</td>
  <td>@ref WFS_CFG_SCALAR_PARAM_TMPL</td>
  <td>string</td>
  <td>Specifies text to use for missing values</td>
</tr>

<tr>
  <td>crs</td>
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
  <td>max_np_distance</td>
  <td>double</td>
  <td>optional</td>
  <td>Specifies maximal distance (meters) for looking up nearest valid point. Only matters if
      query handler parameter @b findNearestValid is non zero.</td>
</tr>

<tr>
  <td>separateGroups</td>
  <td>boolean</td>
  <td>optional (default false)</td>
  <td>Specifying @b true causes separate response group to be generated for each site/td>
</tr>

</table>


*/
