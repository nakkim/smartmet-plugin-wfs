#include "stored_queries/StoredWWProbabilityQueryHandler.h"
#include <gis/Box.h>
#include <gis/OGR.h>
#include <macgyver/TimeFormatter.h>
#include <newbase/NFmiEnumConverter.h>
#include <smartmet/spine/Exception.h>

#include <boost/algorithm/string/replace.hpp>
#include <boost/range/adaptor/map.hpp>
#include <boost/range/algorithm/copy.hpp>
#include <iomanip>
#include <locale>

namespace
{
const char* P_FIND_NEAREST_VALID = "findNearestValid";
const char* P_LOCALE = "locale";
const char* P_MISSING_TEXT = "missingText";
const char* P_PRODUCER = "producer";
const char* P_ORIGIN_TIME = "originTime";
const char* P_CRS = "crs";
const char* P_ICAO_CODE = "icaoCode";

std::string double2string(double d, unsigned int precision)
{
  try
  {
    std::stringstream ss;

    ss << std::fixed << std::setprecision(precision) << d;

    return ss.str();
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

#if 0
std::string bbox2string(const SmartMet::Spine::BoundingBox& bbox, OGRSpatialReference& targetSRS)
{
  try
  {
    std::stringstream ss;

    unsigned int precision(targetSRS.IsGeographic() ? 6 : 1);

    if (targetSRS.EPSGTreatsAsLatLong())
    {
      ss << std::fixed << std::setprecision(precision) << bbox.yMin << "," << bbox.xMin << " "
         << bbox.yMax << "," << bbox.xMin << " " << bbox.yMax << "," << bbox.xMax << " "
         << bbox.yMin << "," << bbox.xMax << " " << bbox.yMin << "," << bbox.xMin;
    }
    else
    {
      ss << std::fixed << std::setprecision(precision) << bbox.xMin << "," << bbox.yMin << " "
         << bbox.xMax << "," << bbox.yMin << " " << bbox.xMax << "," << bbox.yMax << " "
         << bbox.xMin << "," << bbox.yMax << " " << bbox.xMin << "," << bbox.yMin;
    }

    return ss.str();
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}
#endif

FmiParameterName get_parameter(boost::shared_ptr<SmartMet::Plugin::WFS::StoredQueryConfig> config,
                               const std::string& param_name)
{
  try
  {
    uint64_t cpid = config->get_mandatory_config_param<uint64_t>(param_name);
    return static_cast<FmiParameterName>(cpid);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

SmartMet::Engine::Querydata::ParameterOptions get_qengine_parameter(
    const SmartMet::Plugin::WFS::ProbabilityQueryParam& queryParam,
    const SmartMet::Spine::Parameter& smartmetParam)
{
  try
  {
    std::unique_ptr<Fmi::TimeFormatter> timeFormatter(Fmi::TimeFormatter::create("iso"));

    NFmiPoint nearestpoint(kFloatMissing, kFloatMissing);
    bool nearestFlag(false);
    std::string emptystring("");

    SmartMet::Engine::Querydata::ParameterOptions qengine_param(smartmetParam,
                                                                queryParam.producer,
                                                                *(queryParam.loc),
                                                                queryParam.loc->country,
                                                                queryParam.loc->name,
                                                                *timeFormatter,
                                                                emptystring,
                                                                queryParam.language,
                                                                queryParam.outputLocale,
                                                                queryParam.tz_name,
                                                                nearestFlag,
                                                                nearestpoint,
                                                                nearestpoint);

    return qengine_param;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void populate_result_vector(
    SmartMet::Spine::TimeSeries::TimeSeriesPtr qengine_result_light,
    SmartMet::Spine::TimeSeries::TimeSeriesPtr qengine_result_moderate,
    SmartMet::Spine::TimeSeries::TimeSeriesPtr qengine_result_heavy,
    SmartMet::Plugin::WFS::WinterWeatherIntensityProbabilities& result_vector)
{
  try
  {
    unsigned int result_set_size(qengine_result_light->size());

    for (unsigned int i = 0; i < result_set_size; i++)
    {
      const SmartMet::Spine::TimeSeries::TimedValue& tv_light = qengine_result_light->at(i);
      const SmartMet::Spine::TimeSeries::TimedValue& tv_moderate = qengine_result_moderate->at(i);
      const SmartMet::Spine::TimeSeries::TimedValue& tv_heavy = qengine_result_heavy->at(i);

      if (!(boost::get<double>(&tv_light.value)))
      {
        // nan-value is string
        break;
      }

      double light_prob = *(boost::get<double>(&tv_light.value));
      double moderate_prob = *(boost::get<double>(&tv_moderate.value));
      double heavy_prob = *(boost::get<double>(&tv_heavy.value));
      result_vector.push_back(SmartMet::Plugin::WFS::WinterWeatherProbability(
          tv_light.time.utc_time(), light_prob, moderate_prob, heavy_prob));
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // anonymous namespace

namespace SmartMet
{
namespace Plugin
{
namespace WFS
{
StoredWWProbabilityQueryHandler::StoredWWProbabilityQueryHandler(
    SmartMet::Spine::Reactor* reactor,
    boost::shared_ptr<StoredQueryConfig> config,
    PluginData& pluginData,
    boost::optional<std::string> templateFileName)
    : SupportsExtraHandlerParams(config, false),
      StoredQueryHandlerBase(reactor, config, pluginData, templateFileName),
      SupportsLocationParameters(config, SUPPORT_KEYWORDS | INCLUDE_GEOIDS),
      SupportsBoundingBox(config, pluginData.get_crs_registry(), false),
      SupportsTimeParameters(config),
      SupportsTimeZone(config),
      itsQEngine(nullptr),
      itsGeonames(nullptr)
{
  try
  {
    register_scalar_param<int64_t>(P_FIND_NEAREST_VALID, *config);
    register_scalar_param<std::string>(P_LOCALE, *config);
    register_scalar_param<std::string>(P_MISSING_TEXT, *config);
    register_scalar_param<std::string>(P_PRODUCER, *config);
    register_scalar_param<boost::posix_time::ptime>(P_ORIGIN_TIME, *config, false);
    register_scalar_param<std::string>(P_CRS, *config);
    register_array_param<std::string>(P_ICAO_CODE, *config);

    itsProbabilityConfigParams.probabilityUnit =
        config->get_mandatory_config_param<std::string>("probability_params.probabilityUnit");
    itsProbabilityConfigParams.intensityLight = config->get_mandatory_config_param<std::string>(
        "probability_params.precipitationIntensityLight");
    itsProbabilityConfigParams.intensityModerate = config->get_mandatory_config_param<std::string>(
        "probability_params.precipitationIntensityModerate");
    itsProbabilityConfigParams.intensityHeavy = config->get_mandatory_config_param<std::string>(
        "probability_params.precipitationIntensityHeavy");

    std::vector<std::string> configParamIds =
        config->get_mandatory_config_array<std::string>("probability_params.param_id", 0);

    for (const std::string& paramId : configParamIds)
    {
      ProbabilityConfigParam p;

      p.idLight = get_parameter(config, "probability_params." + paramId + ".idLight");
      p.idModerate = get_parameter(config, "probability_params." + paramId + ".idModerate");
      p.idHeavy = get_parameter(config, "probability_params." + paramId + ".idHeavy");
      p.precipitationType = config->get_mandatory_config_param<std::string>(
          "probability_params." + paramId + ".precipitationType");

      itsProbabilityConfigParams.params.push_back(p);
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

StoredWWProbabilityQueryHandler::~StoredWWProbabilityQueryHandler() {}

void StoredWWProbabilityQueryHandler::init_handler()
{
  try
  {
    auto* reactor = get_reactor();
    void* engine = reactor->getSingleton("Querydata", nullptr);
    if (engine == nullptr)
      throw SmartMet::Spine::Exception(BCP, "No Querydata engine available");

    itsQEngine = reinterpret_cast<SmartMet::Engine::Querydata::Engine*>(engine);

    engine = reactor->getSingleton("Geonames", nullptr);
    if (engine == nullptr)
      throw SmartMet::Spine::Exception(BCP, "No Geonames engine available");

    itsGeonames = reinterpret_cast<SmartMet::Engine::Geonames::Engine*>(engine);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void StoredWWProbabilityQueryHandler::parseQueryResults(
    const ProbabilityQueryResultSet& query_results,
    const SmartMet::Spine::BoundingBox& bbox,
    const AirportLocationList& airp_llist,
    const std::string& language,
    SmartMet::Engine::Gis::CRSRegistry& crsRegistry,
    const std::string& requestedCRS,
    const boost::posix_time::ptime& origintime,
    const boost::posix_time::ptime& modificationtime,
    const std::string& tz_name,
    CTPP::CDT& hash) const
{
  try
  {
    // create targer spatial reference to get precision and coordinate order
    OGRSpatialReference targetSRS;
    std::string targetURN("urn:ogc:def:crs:" + requestedCRS);
    targetSRS.importFromURN(targetURN.c_str());

    // if geographic projection, precision is 6 digits, otherwise 1 digit
    unsigned int precision = ((targetSRS.IsGeographic()) ? 6 : 1);
    // coordinate order
    bool latLonOrder(targetSRS.EPSGTreatsAsLatLong());

    SmartMet::Spine::BoundingBox query_bbox = bbox;

    // handle lowerCorner and upperCorner
    if (latLonOrder)
    {
      hash["bbox_lower_corner"] = (double2string(query_bbox.yMin, precision) + " " +
                                   double2string(query_bbox.xMin, precision));
      hash["bbox_upper_corner"] = (double2string(query_bbox.yMax, precision) + " " +
                                   double2string(query_bbox.xMax, precision));
      hash["axis_labels"] = "Lat Long";
    }
    else
    {
      hash["bbox_lower_corner"] = (double2string(query_bbox.xMin, precision) + " " +
                                   double2string(query_bbox.yMin, precision));
      hash["bbox_upper_corner"] = (double2string(query_bbox.xMax, precision) + " " +
                                   double2string(query_bbox.yMax, precision));
      hash["axis_labels"] = "Long Lat";
    }

    std::string proj_uri = "UNKNOWN";
    plugin_data.get_crs_registry().get_attribute(requestedCRS, "projUri", &proj_uri);

    boost::local_time::time_zone_ptr tzp = get_time_zone(tz_name);
    std::string runtime_timestamp = format_local_time(plugin_data.get_time_stamp(), tzp);

    hash["proj_uri"] = proj_uri;
    hash["fmi_apikey"] = QueryBase::FMI_APIKEY_SUBST;
    hash["fmi_apikey_prefix"] = QueryBase::FMI_APIKEY_PREFIX_SUBST;
    hash["hostname"] = QueryBase::HOSTNAME_SUBST;
    hash["protocol"] = QueryBase::PROTOCOL_SUBST;
    hash["language"] = language;
    hash["response_timestamp"] = runtime_timestamp;

    // wfs members
    hash["wfs_members"] = CTPP::CDT(CTPP::CDT::ARRAY_VAL);

    unsigned int wfs_member_index(0);
    // iterate locations

    for (const AirportLocation& airp_loc : airp_llist)
    {
      SmartMet::Spine::LocationPtr loc = airp_loc.loc;

      WinterWeatherTypeProbabilities ww_tprobs = query_results.at(loc);

      std::vector<std::string> wwtypes;
      // Retrieve all wwtypes
      boost::copy(ww_tprobs | boost::adaptors::map_keys, std::back_inserter(wwtypes));

      for (const std::string& wwtype : wwtypes)
      {
        // own wfs_member for each precipitation type
        CTPP::CDT& wfs_member = hash["wfs_members"][wfs_member_index];
        wfs_member_index++;

        wfs_member["precipitation_form_id"] = wwtype;
        wfs_member["phenomenon_time"] = runtime_timestamp;
        wfs_member["analysis_time"] = format_local_time(origintime, tzp);
        wfs_member["resultTime"] = format_local_time(modificationtime, tzp);
        wfs_member["designator"] = loc->iso2;
        wfs_member["name"] = loc->name;
        wfs_member["location_indicator_icao"] = airp_loc.icao_code;
        wfs_member["field_elevation"] = double2string(loc->elevation, 1);

        auto transformation = crs_registry.create_transformation("EPSG::4326", requestedCRS);

        NFmiPoint p1(loc->longitude, loc->latitude);
        NFmiPoint p2 = transformation->transform(p1);

        double longitude(p2.X());
        double latitude(p2.Y());

        std::stringstream ss_pos;
        if (latLonOrder)
          ss_pos << double2string(latitude, precision) << " "
                 << double2string(longitude, precision);
        else
          ss_pos << double2string(longitude, precision) << " "
                 << double2string(latitude, precision);

        wfs_member["position"] = ss_pos.str();

        CTPP::CDT& result = wfs_member["result"];
        result["timesteps"] = CTPP::CDT(CTPP::CDT::ARRAY_VAL);

        WinterWeatherIntensityProbabilities ww_iprobs = ww_tprobs[wwtype];

        unsigned int timestep_index_index(0);

        for (const WinterWeatherProbability ww_iprob : ww_iprobs)
        {
          std::string timestamp(format_local_time(ww_iprob.timestamp, tzp));

          CTPP::CDT& timestep_data = result["timesteps"][timestep_index_index];
          timestep_index_index++;

          timestep_data["precipitation_form_id"] = wwtype;
          timestep_data["date_time"] = timestamp;
          timestep_data["icao_code"] = airp_loc.icao_code;
          timestep_data["latitude"] = double2string(latitude, precision);
          timestep_data["longitude"] = double2string(longitude, precision);
          timestep_data["type_light"] = itsProbabilityConfigParams.intensityLight;
          timestep_data["type_moderate"] = itsProbabilityConfigParams.intensityModerate;
          timestep_data["type_heavy"] = itsProbabilityConfigParams.intensityHeavy;
          timestep_data["value_light"] = double2string(ww_iprob.light_probability, 1);
          timestep_data["value_moderate"] = double2string(ww_iprob.moderate_probability, 1);
          timestep_data["value_heavy"] = double2string(ww_iprob.heavy_probability, 1);
          timestep_data["unit"] = itsProbabilityConfigParams.probabilityUnit;
        }
      }
    }

    hash["number_matched"] = std::to_string(wfs_member_index);
    hash["number_returned"] = std::to_string(wfs_member_index);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

WinterWeatherIntensityProbabilities StoredWWProbabilityQueryHandler::getProbabilities(
    const ProbabilityQueryParam& queryParam) const
{
  try
  {
    WinterWeatherIntensityProbabilities ret;

    SmartMet::Engine::Querydata::ParameterOptions qengine_param_light =
        get_qengine_parameter(queryParam, queryParam.paramLight);
    SmartMet::Engine::Querydata::ParameterOptions qengine_param_moderate =
        get_qengine_parameter(queryParam, queryParam.paramModerate);
    SmartMet::Engine::Querydata::ParameterOptions qengine_param_heavy =
        get_qengine_parameter(queryParam, queryParam.paramHeavy);

    SmartMet::Spine::TimeSeries::TimeSeriesPtr qengine_result_light =
        queryParam.q->values(qengine_param_light, queryParam.tlist);
    SmartMet::Spine::TimeSeries::TimeSeriesPtr qengine_result_moderate =
        queryParam.q->values(qengine_param_moderate, queryParam.tlist);
    SmartMet::Spine::TimeSeries::TimeSeriesPtr qengine_result_heavy =
        queryParam.q->values(qengine_param_heavy, queryParam.tlist);

    unsigned int result_set_size(qengine_result_light->size());
    // all intensities should have same timesteps
    if (qengine_result_moderate->size() != result_set_size ||
        qengine_result_heavy->size() != result_set_size)
    {
      std::ostringstream msg;
      msg << "Inconsistent timesteps in LIGHT, MODERATE, HEAVY Winter Weather Condition "
             "parameters: '"
          << queryParam.paramLight.name() << "', '" << queryParam.paramModerate.name() << "', '"
          << queryParam.paramHeavy.name() << "'";
      SmartMet::Spine::Exception exception(BCP, msg.str());
      exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PROCESSING_FAILED);
      throw exception;
    }

    populate_result_vector(
        qengine_result_light, qengine_result_moderate, qengine_result_heavy, ret);

    return ret;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void StoredWWProbabilityQueryHandler::query(const StoredQuery& query,
                                            const std::string& language,
                                            std::ostream& output) const
{
  try
  {
    // 1) query precipitation probabilities from Querydata
    // 2) generate output xml

    const auto& sq_params = query.get_param_map();

    std::string requestedCRS = sq_params.get_single<std::string>(P_CRS);

    std::string targetURN("urn:ogc:def:crs:" + requestedCRS);
    OGRSpatialReference sr;
    if (sr.importFromURN(targetURN.c_str()) != OGRERR_NONE)
    {
      SmartMet::Spine::Exception exception(BCP, "Invalid crs '" + requestedCRS + "'!");
      exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PROCESSING_FAILED);
      throw exception;
    }

    std::string localeName(sq_params.get_single<std::string>(P_LOCALE));
    std::locale outputLocale(localeName.c_str());
    std::string tz_name(get_tz_name(sq_params));

    typedef std::pair<std::string, SmartMet::Spine::LocationPtr> LocationListItem;
    std::list<LocationListItem> llist;

    get_location_options(itsGeonames, sq_params, language, &llist);

    if (llist.empty())
    {
      std::cout << "No locations!!!!!!!!!!!!" << std::endl;
      return;
    }

    // bounding box where all locations must reside
    SmartMet::Spine::BoundingBox bbox;
    get_bounding_box(sq_params, requestedCRS, &bbox);

    // if requested CRS and bounding box CRS are different, do transformation
    if (requestedCRS.compare(bbox.crs) != 0)
      bbox = transform_bounding_box(bbox, requestedCRS);

    std::unique_ptr<OGRGeometry> geom;
    Fmi::OGR::CoordinatePoints geoCoordinates;
    geoCoordinates.push_back(std::pair<double, double>(bbox.xMin, bbox.yMin));
    geoCoordinates.push_back(std::pair<double, double>(bbox.xMin, bbox.yMax));
    geoCoordinates.push_back(std::pair<double, double>(bbox.xMax, bbox.yMax));
    geoCoordinates.push_back(std::pair<double, double>(bbox.xMax, bbox.yMin));
    geoCoordinates.push_back(std::pair<double, double>(bbox.xMin, bbox.yMin));
    geom.reset(Fmi::OGR::constructGeometry(geoCoordinates, wkbPolygon, 4326));

    // fetch all icao codes
    std::list<LocationListItem> llist_icao;
    get_location_options(itsGeonames, sq_params, "icao", &llist_icao);

    // requested icao codes
    std::vector<std::string> icaoCodeVector;
    sq_params.get<std::string>(P_ICAO_CODE, std::back_inserter(icaoCodeVector));
    std::set<std::string> icaoCodeSet(icaoCodeVector.begin(), icaoCodeVector.end());

    AirportLocationList airp_llist;
    for (LocationListItem item : llist)
    {
      // if location not inside bounding box, ignore it
      OGRPoint point(item.second->longitude, item.second->latitude);
      if (!point.Within(geom.get()))
        continue;

      for (LocationListItem icao_item : llist_icao)
      {
        if (item.second->geoid == icao_item.second->geoid)
        {
          // if icao code not amongst requested, ignore location
          if (icaoCodeSet.size() > 0 &&
              icaoCodeSet.find(icao_item.second->name) == icaoCodeSet.end())
            break;

          airp_llist.push_back(AirportLocation(icao_item.first, item.second));
          break;
        }
      }
    }

    SmartMet::Engine::Querydata::Producer producer = sq_params.get_single<std::string>(P_PRODUCER);
    std::string missingText = sq_params.get_single<std::string>(P_MISSING_TEXT);
    boost::optional<boost::posix_time::ptime> requested_origintime =
        sq_params.get_optional<boost::posix_time::ptime>(P_ORIGIN_TIME);

    SmartMet::Engine::Querydata::Q q;
    if (requested_origintime)
      q = itsQEngine->get(producer, *requested_origintime);
    else
      q = itsQEngine->get(producer);

    boost::posix_time::ptime origintime = q->originTime();
    boost::posix_time::ptime modificationtime = q->modificationTime();

    boost::shared_ptr<SmartMet::Spine::TimeSeriesGeneratorOptions> pTimeOptions =
        get_time_generator_options(sq_params);
    // get data in UTC
    const std::string zone = "UTC";
    auto tz = itsGeonames->getTimeZones().time_zone_from_string(zone);
    SmartMet::Spine::TimeSeriesGenerator::LocalTimeList tlist =
        SmartMet::Spine::TimeSeriesGenerator::generate(*pTimeOptions, tz);

    ProbabilityQueryResultSet query_results;
    // iterate locations
    for (AirportLocation airp_loc : airp_llist)
    {
      WinterWeatherTypeProbabilities wwprobs;

      // iterate winter weather condition parameters
      for (const ProbabilityConfigParam& configParam : itsProbabilityConfigParams.params)
      {
        // parameter read from configuration file
        SmartMet::Spine::Parameter paramLight(
            "", SmartMet::Spine::Parameter::Type::Data, configParam.idLight);
        SmartMet::Spine::Parameter paramModerate(
            "", SmartMet::Spine::Parameter::Type::Data, configParam.idModerate);
        SmartMet::Spine::Parameter paramHeavy(
            "", SmartMet::Spine::Parameter::Type::Data, configParam.idHeavy);

        ProbabilityQueryParam query_param(paramLight, paramModerate, paramHeavy, q, sr);
        query_param.tlist = tlist;
        query_param.loc = airp_loc.loc;  // item.second;
        query_param.producer = producer;
        query_param.bbox = bbox;
        query_param.missingText = missingText;
        query_param.language = language;
        query_param.outputLocale = outputLocale;
        query_param.tz_name = tz_name;
        query_param.precipitationType = configParam.precipitationType;

        // fetch probabilities for one precipitation type for all intensities
        wwprobs.insert(make_pair(configParam.precipitationType, getProbabilities(query_param)));
      }
      // probabilities for one location for all precipitation types
      query_results.insert(make_pair(airp_loc.loc, wwprobs));
    }

    SmartMet::Engine::Gis::CRSRegistry& crsRegistry = plugin_data.get_crs_registry();
    CTPP::CDT hash;

    parseQueryResults(query_results,
                      bbox,
                      airp_llist,
                      language,
                      crsRegistry,
                      requestedCRS,
                      origintime,
                      modificationtime,
                      tz_name,
                      hash);

    // Format output
    format_output(hash, output, query.get_use_debug_format());
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

boost::shared_ptr<SmartMet::Plugin::WFS::StoredQueryHandlerBase>
wfs_winterweather_probabilities_query_handler_create(SmartMet::Spine::Reactor* reactor,
                                                     boost::shared_ptr<StoredQueryConfig> config,
                                                     PluginData& pluginData,
                                                     boost::optional<std::string> templateFileName)
{
  try
  {
    StoredWWProbabilityQueryHandler* qh =
        new StoredWWProbabilityQueryHandler(reactor, config, pluginData, templateFileName);
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

SmartMet::Plugin::WFS::StoredQueryHandlerFactoryDef
    wfs_winterweather_probabilities_query_handler_factory(
        &wfs_winterweather_probabilities_query_handler_create);
