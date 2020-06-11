#ifndef WITHOUT_OBSERVATION

#include "stored_queries/StoredAirNuclideQueryHandler.h"
#include "FeatureID.h"
#include "StoredQueryHandlerFactoryDef.h"
#include <boost/format.hpp>
#include <engines/observation/DBRegistry.h>
#include <engines/observation/MastQuery.h>
#include <smartmet/spine/Exception.h>
#include <spine/Convenience.h>

namespace bw = SmartMet::Plugin::WFS;

bw::StoredAirNuclideQueryHandler::StoredAirNuclideQueryHandler(
    SmartMet::Spine::Reactor* reactor,
    boost::shared_ptr<StoredQueryConfig> config,
    PluginImpl& plugin_data,
    boost::optional<std::string> template_file_name)

    : bw::SupportsExtraHandlerParams(config),
      bw::RequiresGeoEngine(reactor),
      bw::RequiresObsEngine(reactor),
      bw::StoredQueryHandlerBase(reactor, config, plugin_data, template_file_name),
      bw::SupportsLocationParameters(
          reactor, config, INCLUDE_FMISIDS | INCLUDE_GEOIDS | INCLUDE_WMOS | SUPPORT_KEYWORDS),
      bw::SupportsBoundingBox(config, plugin_data.get_crs_registry()),
      bw::SupportsQualityParameters(config)

{
  try
  {
    register_scalar_param<pt::ptime>(P_BEGIN_TIME, *config);
    register_scalar_param<pt::ptime>(P_END_TIME, *config);
    register_scalar_param<std::string>(P_STATION_TYPE, *config);
    register_scalar_param<uint64_t>(P_TIME_STEP, *config);
    register_scalar_param<uint64_t>(P_NUM_OF_STATIONS, *config);
    register_scalar_param<std::string>(P_CRS, *config);
    register_scalar_param<bool>(P_LATEST, *config);
    register_array_param<std::string>(P_NUCLIDE_CODES, *config);

    m_maxHours = config->get_optional_config_param<double>("maxHours", 365 * 24.0);
    m_sqRestrictions = plugin_data.get_config().getSQRestrictions();
    m_supportQCParameters = false;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

bw::StoredAirNuclideQueryHandler::~StoredAirNuclideQueryHandler() {}

void bw::StoredAirNuclideQueryHandler::query(const StoredQuery& query,
                                             const std::string& language,
                                             const boost::optional<std::string>& hostname,
                                             std::ostream& output) const
{
  try
  {
    namespace bo = SmartMet::Engine::Observation;
    const auto& params = query.get_param_map();

    // Monitoring query parameter initialization process.
    // If the value is false empty result will be returned.
    bool queryInitializationOK = true;

    try
    {
      const std::string stationType = params.get_single<std::string>(P_STATION_TYPE);
      const double maxDistance = params.get_single<double>(P_MAX_DISTANCE);
      const uint64_t numberOfStations = params.get_single<uint64_t>(P_NUM_OF_STATIONS);
      const std::string requestedCrs = params.get_single<std::string>(P_CRS);
      const bool latest = params.get_single<bool>(P_LATEST);
      const bool showDataQuality = SupportsQualityParameters::supportQualityInfo(params);

      std::vector<std::string> nuclideCodes;
      params.get<std::string>(P_NUCLIDE_CODES, std::back_inserter(nuclideCodes));
      BOOST_FOREACH (auto& n, nuclideCodes)
        n = prepare_nuclide(n);

      const char* DATA_CRS_NAME = "urn:ogc:def:crs:EPSG::4326";
      SmartMet::Spine::CRSRegistry& crs_registry = plugin_impl.get_crs_registry();

      // Output CRS priority: user defined -> default in stored query -> feature type default crs
      const std::string crs = requestedCrs.empty() ? DATA_CRS_NAME : requestedCrs;
      auto transformation = crs_registry.create_transformation(DATA_CRS_NAME, crs);

      bool show_height = false;
      std::string proj_uri = "UNKNOWN";
      std::string proj_epoch_uri = "UNKNOWN";
      std::string axis_labels = "";
      crs_registry.get_attribute(crs, "showHeight", &show_height);
      crs_registry.get_attribute(crs, "projUri", &proj_uri);
      crs_registry.get_attribute(crs, "projEpochUri", &proj_epoch_uri);
      crs_registry.get_attribute(crs, "axisLabels", &axis_labels);

      // Search and validate the locations.
      typedef std::pair<std::string, SmartMet::Spine::LocationPtr> LocationListItem;
      typedef std::list<LocationListItem> LocationList;
      LocationList locations_list;
      get_location_options(params, language, &locations_list);

      const int debug_level = get_config()->get_debug_level();
      if (debug_level > 2)
      {
        BOOST_FOREACH (auto id, locations_list)
          std::cerr << "Found location: name: " << id.first << " geoid: " << id.second->geoid
                    << "\n";
      }

      pt::ptime startTime = params.get_single<pt::ptime>(P_BEGIN_TIME);
      pt::ptime endTime = params.get_single<pt::ptime>(P_END_TIME);
      const uint64_t timestep = params.get_single<uint64_t>(P_TIME_STEP);

      if (m_sqRestrictions)
        check_time_interval(startTime, endTime, m_maxHours);

      // Only 24 hours allowed
      if (1440 < timestep)
      {
        SmartMet::Spine::Exception exception(BCP, "Operation processing failed!");
        exception.addDetail("Invalid time step value. Maximum is 1440 minutes.");
        exception.addParameter(WFS_EXCEPTION_CODE, WFS_INVALID_PARAMETER_VALUE);
        exception.addParameter("Timestep", std::to_string(timestep));
        throw exception.disableStackTrace();
      }

      // Get all the information needed in a OBsEngine station search.
      SmartMet::Engine::Observation::Settings stationSearchSettings;
      stationSearchSettings.allplaces = false;
      stationSearchSettings.numberofstations = numberOfStations;
      stationSearchSettings.stationtype = stationType;
      stationSearchSettings.maxdistance = maxDistance;
      stationSearchSettings.useDataCache = false;
      stationSearchSettings.starttime = startTime;
      stationSearchSettings.endtime = endTime;

      SmartMet::Engine::Observation::StationSettings stationSettings;

      // Include valid locations.
      for (const auto& item : locations_list)
      {
        stationSettings.nearest_station_settings.emplace_back(
            item.second->longitude, item.second->latitude, maxDistance, 1, item.first);
      }

      // Bounding box search options.
      SmartMet::Spine::BoundingBox requested_bbox;
      bool have_bbox = get_bounding_box(params, crs, &requested_bbox);
      if (have_bbox)
      {
        std::unique_ptr<SmartMet::Spine::BoundingBox> query_bbox;
        query_bbox.reset(new SmartMet::Spine::BoundingBox);
        *query_bbox =
            bw::SupportsBoundingBox::transform_bounding_box(requested_bbox, DATA_CRS_NAME);
        SmartMet::Engine::Observation::Settings bboxSettings;
        stationSettings.bounding_box_settings["minx"] = query_bbox->xMin;
        stationSettings.bounding_box_settings["miny"] = query_bbox->yMin;
        stationSettings.bounding_box_settings["maxx"] = query_bbox->xMax;
        stationSettings.bounding_box_settings["maxy"] = query_bbox->yMax;
      }

      stationSearchSettings.taggedFMISIDs =
          obs_engine->translateToFMISID(stationSearchSettings.starttime,
                                        stationSearchSettings.endtime,
                                        stationSearchSettings.stationtype,
                                        stationSettings);

      // Search stations based on location settings.
      // The result does not contain duplicates.
      SmartMet::Spine::Stations stationCandidates;
      obs_engine->getStations(stationCandidates, stationSearchSettings);

      std::string langCode = language;
      SupportsLocationParameters::engOrFinToEnOrFi(langCode);

      // Get information from GeoEngien. Elevation is required.
      typedef std::map<std::string, SmartMet::Spine::Station> StationMap;
      StationMap stations;
      // SmartMet::Spine::Stations stations;
      for (SmartMet::Spine::Stations::const_iterator it = stationCandidates.begin();
           it != stationCandidates.end();
           ++it)
      {
        std::string fmisidStr = std::to_string((*it).fmisid);
        stations.emplace(fmisidStr, *it);

        SmartMet::Spine::LocationPtr geoLoc = geo_engine->idSearch(it->geoid, langCode);
        if (geoLoc)
        {
          stations[fmisidStr].country = geoLoc->country;
          stations[fmisidStr].station_elevation = geoLoc->elevation;
        }
      }

      if (stations.empty())
        queryInitializationOK = false;

      //
      // Observation ID search
      //

      // Using OBSERVATIONS_V2 as a base configuration
      // Creating query parameters object by using the base configuration.
      // All the other configurations are joined to this.
      bo::MastQueryParams stationQueryParams(dbRegistryConfig("OBSERVATIONS_V2"));

      // Join on STATIONS_V1 view by using STATION_ID
      stationQueryParams.addJoinOnConfig(dbRegistryConfig("STATIONS_V1"), "STATION_ID");

      stationQueryParams.addJoinOnConfig(dbRegistryConfig("NETWORK_MEMBERS_V1"), "STATION_ID");

      stationQueryParams.addJoinOnConfig(dbRegistryConfig("STUK_RADIONUCLIDE_ANALYSES_V1"),
                                         "STATION_ID");

      // Station identities
      for (StationMap::const_iterator it = stations.begin();
           queryInitializationOK && it != stations.end();
           ++it)
      {
        stationQueryParams.addOperation(
            "OR_GROUP_station_id", "STATION_ID", "PropertyIsEqualTo", it->second.fmisid);
      }

      stationQueryParams.addOperation(
          "OR_GROUP_network_id", "NETWORK_ID", "PropertyIsEqualTo", 59);  // STUK-ILMA-verkko (59)

      std::string paramName = "AC_P7D_avg";
      // Is the parameter configured in Observation
      const uint64_t paramId = obs_engine->getParameterId(paramName, stationType);
      if (not paramId)
      {
        SmartMet::Spine::Exception exception(BCP, "Unknown parameter in the query!");
        exception.addParameter(WFS_EXCEPTION_CODE, WFS_INVALID_PARAMETER_VALUE);
        exception.addParameter("Unknown parameter", paramName);
        throw exception.disableStackTrace();
      }

      stationQueryParams.addOperation(
          "OR_GROUP_measurand_id", "MEASURAND_ID", "PropertyIsEqualTo", paramId);

      stationQueryParams.addOperation(
          "OR_GROUP_end_time", "PERIOD_START", "PropertyIsLessThanOrEqualTo", endTime);
      stationQueryParams.addOperation(
          "OR_GROUP_start_time", "PERIOD_END", "PropertyIsGreaterThanOrEqualTo", startTime);
      stationQueryParams.addOperation(
          "OR_GROUP_atime", "ANALYSIS_TIME", "PropertyIsGreaterThanOrEqualTo", startTime);

      stationQueryParams.addField("ANALYSIS_TIME");
      stationQueryParams.addField("STATION_ID");
      stationQueryParams.addField("ANALYSIS_ID");
      stationQueryParams.addOrderBy("ANALYSIS_TIME", "DESC");
      stationQueryParams.addOrderBy("STATION_ID", "ASC");

      bo::MastQuery profileQuery;
      profileQuery.setQueryParams(&stationQueryParams);
      if (queryInitializationOK)
      {
        obs_engine->makeQuery(&profileQuery);
      }

      // Container with the observation identities of the requested stations.
      std::shared_ptr<bo::QueryResult> profileContainer = profileQuery.getQueryResultContainer();

      bo::MastQuery dataQuery;

      bo::MastQuery radioNuclidesQuery;
      typedef std::map<std::string, std::string> NuclideNameMap;
      NuclideNameMap nuclideNameMap;

      // At least one observation_id is required for data search.
      if (not profileContainer or (profileContainer->size("ANALYSIS_ID") == 0))
      {
        queryInitializationOK = false;
      }
      else
      {
        //
        // Get the data match with the analysis_id values
        //

        // Using STUK_RADIONUCLIDE_ANALYSES_V1 as a base configuration
        bo::MastQueryParams dataQueryParams(dbRegistryConfig("STUK_RADIONUCLIDE_ANALYSES_V1"));
        dataQueryParams.addJoinOnConfig(dbRegistryConfig("STUK_RADIONUCLIDE_DATA_V1"),
                                        "ANALYSIS_ID");

        bo::QueryResult::ValueVectorType::const_iterator analysisIdIt =
            profileContainer->begin("ANALYSIS_ID");
        bo::QueryResult::ValueVectorType::const_iterator analysisIdItEnd =
            profileContainer->end("ANALYSIS_ID");
        bo::QueryResult::ValueVectorType::const_iterator stationIdIt =
            profileContainer->begin("STATION_ID");

        // Getting nuclide names in finnish or english
        bo::MastQueryParams radioNuclidesQueryParams(dbRegistryConfig("STUK_RADIONUCLIDES_VL1"));
        radioNuclidesQueryParams.addJoinOnConfig(dbRegistryConfig("STUK_RADIONUCLIDES_V1"),
                                                 "NUCLIDE_ID");
        radioNuclidesQueryParams.addOperation(
            "OR_GROUP_language_code", "LANGUAGE_CODE", "PropertyIsEqualTo", langCode);

        std::vector<std::string>::const_iterator nucCodeIt = nuclideCodes.begin();
        for (; queryInitializationOK && nucCodeIt != nuclideCodes.end(); ++nucCodeIt)
          radioNuclidesQueryParams.addOperation("OR_GROUP_nuclide_code",
                                                "NUCLIDE_CODE",
                                                "PropertyIsEqualTo",
                                                std::string(*nucCodeIt));

        radioNuclidesQueryParams.addField("NUCLIDE_CODE");
        radioNuclidesQueryParams.addField("NUCLIDE_NAME");
        radioNuclidesQueryParams.useDistinct();

        radioNuclidesQuery.setQueryParams(&radioNuclidesQueryParams);
        obs_engine->makeQuery(&radioNuclidesQuery);

        std::shared_ptr<bo::QueryResult> radioNuclidesContainer =
            radioNuclidesQuery.getQueryResultContainer();

        if (radioNuclidesContainer->size())
        {
          bo::QueryResult::ValueVectorType::const_iterator nCodeIt =
              radioNuclidesContainer->begin("NUCLIDE_CODE");
          bo::QueryResult::ValueVectorType::const_iterator nCodeItEnd =
              radioNuclidesContainer->end("NUCLIDE_CODE");
          bo::QueryResult::ValueVectorType::const_iterator nNameIt =
              radioNuclidesContainer->begin("NUCLIDE_NAME");

          for (; nCodeIt != nCodeItEnd; ++nCodeIt, ++nNameIt)
          {
            nuclideNameMap.emplace(bo::QueryResult::toString(nCodeIt),
                                   bo::QueryResult::toString(nNameIt));
          }
        }

        typedef std::set<int> LatestSet;
        LatestSet latestSet;
        for (; queryInitializationOK && analysisIdIt != analysisIdItEnd;
             ++analysisIdIt, ++stationIdIt)
        {
          // Select only latest if wanted.
          if (latest)
          {
            const int tmpstationId = std::stoi(bo::QueryResult::toString(stationIdIt, 0));
            LatestSet::const_iterator latestSetIt = latestSet.find(tmpstationId);
            if (latestSetIt == latestSet.end())
            {
              latestSet.insert(tmpstationId);
              int tmpAnalysisId = std::stoi(bo::QueryResult::toString(analysisIdIt, 0));
              dataQueryParams.addOperation(
                  "OR_GROUP_analysis_id", "ANALYSIS_ID", "PropertyIsEqualTo", tmpAnalysisId);
            }
          }
          else
          {
            int tmpAnalysisId = std::stoi(bo::QueryResult::toString(analysisIdIt, 0));
            dataQueryParams.addOperation(
                "OR_GROUP_analysis_id", "ANALYSIS_ID", "PropertyIsEqualTo", tmpAnalysisId);
          }
        }

        std::vector<std::string>::const_iterator nuclideCodeIt = nuclideCodes.begin();
        for (; queryInitializationOK && nuclideCodeIt != nuclideCodes.end(); ++nuclideCodeIt)
          dataQueryParams.addOperation("OR_GROUP_nuclide_code",
                                       "NUCLIDE_CODE",
                                       "PropertyIsEqualTo",
                                       std::string(*nuclideCodeIt));

        dataQueryParams.addField("ANALYSIS_ID");
        dataQueryParams.addField("STATION_ID");
        dataQueryParams.addField("OBSERVATION_ID");
        dataQueryParams.addField("PERIOD_START");
        dataQueryParams.addField("PERIOD_END");
        dataQueryParams.addField("ANALYSIS_TIME");
        dataQueryParams.addField("ANALYSIS_VERSION");
        dataQueryParams.addField("AIR_VOLUME");
        dataQueryParams.addField("NUCLIDE_CODE");
        dataQueryParams.addField("DATA_QUALITY");
        dataQueryParams.addField("CONCENTRATION");
        dataQueryParams.addField("UNCERTAINTY");

        dataQueryParams.addOrderBy("STATION_ID", "ASC");
        dataQueryParams.addOrderBy("OBSERVATION_ID", "ASC");
        dataQueryParams.addOrderBy("ANALYSIS_ID", "ASC");
        dataQueryParams.addOrderBy("CONCENTRATION", "DESC");

        dataQuery.setQueryParams(&dataQueryParams);

        if (queryInitializationOK)
          obs_engine->makeQuery(&dataQuery);
      }

      // Get the sequence number of query in the request
      int sq_id = query.get_query_id();
      FeatureID feature_id(get_config()->get_query_id(), params.get_map(), sq_id);
      const std::string fullFeatureId = feature_id.get_id();

      // Removing some feature id parameters
      const char* place_params[] = {
          P_WMOS, P_FMISIDS, P_PLACES, P_LATLONS, P_GEOIDS, P_KEYWORD, P_BOUNDING_BOX};
      for (unsigned i = 0; i < sizeof(place_params) / sizeof(*place_params); i++)
      {
        feature_id.erase_param(place_params[i]);
      }

      //
      // Fill data for xml formatting.
      //
      CTPP::CDT hash;

      params.dump_params(hash["query_parameters"]);
      dump_named_params(params, hash["named_parameters"]);

      hash["featureId"] = feature_id.get_id();

      hash["responseTimestamp"] =
          pt::to_iso_extended_string(get_plugin_impl().get_time_stamp()) + "Z";
      hash["fmi_apikey"] = QueryBase::FMI_APIKEY_SUBST;
      hash["fmi_apikey_prefix"] = bw::QueryBase::FMI_APIKEY_PREFIX_SUBST;
      hash["hostname"] = QueryBase::HOSTNAME_SUBST;
      hash["protocol"] = QueryBase::PROTOCOL_SUBST;
      hash["language"] = language;
      hash["projSrsDim"] = (show_height ? 3 : 2);
      hash["projSrsName"] = proj_uri;
      hash["projEpochSrsDim"] = (show_height ? 4 : 3);
      hash["projEpochSrsName"] = proj_epoch_uri;
      hash["axisLabels"] = axis_labels;
      hash["queryNum"] = query.get_query_id();
      hash["queryName"] = get_query_name();
      hash["stationType"] = stationType;

      std::shared_ptr<bo::QueryResult> dataContainer = dataQuery.getQueryResultContainer();

      int numberMatched = 0;
      if (queryInitializationOK and profileContainer and dataContainer)
      {
        bo::QueryResult::ValueVectorType::const_iterator analysisIdIt =
            dataContainer->begin("ANALYSIS_ID");
        bo::QueryResult::ValueVectorType::const_iterator stationIdIt =
            dataContainer->begin("STATION_ID");
        bo::QueryResult::ValueVectorType::const_iterator stationIdItEnd =
            dataContainer->end("STATION_ID");
        bo::QueryResult::ValueVectorType::const_iterator observationIdIt =
            dataContainer->begin("OBSERVATION_ID");
        bo::QueryResult::ValueVectorType::const_iterator periodStartIt =
            dataContainer->begin("PERIOD_START");
        bo::QueryResult::ValueVectorType::const_iterator periodEndIt =
            dataContainer->begin("PERIOD_END");
        bo::QueryResult::ValueVectorType::const_iterator analysisTimeIt =
            dataContainer->begin("ANALYSIS_TIME");
        bo::QueryResult::ValueVectorType::const_iterator analysisVersionIt =
            dataContainer->begin("ANALYSIS_VERSION");
        bo::QueryResult::ValueVectorType::const_iterator airVolumeIt =
            dataContainer->begin("AIR_VOLUME");
        bo::QueryResult::ValueVectorType::const_iterator nuclideCodeIt =
            dataContainer->begin("NUCLIDE_CODE");
        bo::QueryResult::ValueVectorType::const_iterator concentrationIt =
            dataContainer->begin("CONCENTRATION");
        bo::QueryResult::ValueVectorType::const_iterator dataQualityIt =
            dataContainer->begin("DATA_QUALITY");
        bo::QueryResult::ValueVectorType::const_iterator uncertaintyIt =
            dataContainer->begin("UNCERTAINTY");

        std::string currentAnalysisIdStr = "dummyAnalysisId";
        int groupId = 0;

        for (; stationIdIt != stationIdItEnd;)
        {
          std::string stationIdStr = bo::QueryResult::toString(stationIdIt, 0);
          std::string observationIdStr = bo::QueryResult::toString(observationIdIt, 0);
          std::string analysisIdStr = bo::QueryResult::toString(analysisIdIt, 0);
          std::string periodStartStr = bo::QueryResult::toString(periodStartIt);
          std::string periodEndStr = bo::QueryResult::toString(periodEndIt);

          StationMap::const_iterator sit = stations.find(stationIdStr);
          if (sit == stations.end())
          {
            std::ostringstream msg;
            msg << "SmartMet::Plugin::WFS::StoredAirNuclideQueryHandler : missing station data ("
                << stationIdStr << "\n";
            break;
          }

          // Group is an analysis of an observation in a station.
          CTPP::CDT& group = hash["groups"][groupId];
          const std::string groupId_str = str(boost::format("%1%-%2%") % sq_id % (groupId + 1));
          group["groupId"] = groupId_str;
          group["groupNum"] = groupId + 1;
          group["obsParamList"][0]["name"] = paramName;

          if (not sit->second.station_formal_name.empty())
            group["stationFormalName"] = sit->second.station_formal_name;
          if (not sit->second.region.empty())
            group["region"] = sit->second.region;
          if (not sit->second.country.empty())
            group["country"] = sit->second.country;

          set_2D_coord(transformation, sit->second.latitude_out, sit->second.longitude_out, group);

          group["latitude"] = std::to_string(sit->second.latitude_out);
          group["longitude"] = std::to_string(sit->second.longitude_out);
          if (show_height)
            group["elevation"] = std::to_string(sit->second.station_elevation);

          group["fmisid"] = stationIdStr;
          group["observationId"] = observationIdStr;
          group["analysisId"] = analysisIdStr;
          group["obsPhenomenonStartTime"] = periodStartStr;
          group["obsPhenomenonEndTime"] = periodEndStr;

          group["analysisTime"] = bo::QueryResult::toString(analysisTimeIt);
          group["obsResultTime"] = bo::QueryResult::toString(analysisTimeIt);
          group["analysisVersion"] = bo::QueryResult::toString(analysisVersionIt);
          group["airVolume"] = bo::QueryResult::toString(airVolumeIt);

          // Using the exact time period of measurement in featureId search.
          feature_id.erase_param(P_BEGIN_TIME);
          feature_id.erase_param(P_END_TIME);
          feature_id.add_param(P_BEGIN_TIME, periodStartStr);
          feature_id.add_param(P_END_TIME, periodEndStr);

          // Using fmisid for selecting a location in featureId search.
          feature_id.erase_param(P_FMISIDS);
          feature_id.add_param(P_FMISIDS, sit->second.fmisid);

          const std::string analysisfeatureId = feature_id.get_id();

          group["featureId"] = analysisfeatureId;

          int ind = 0;

          currentAnalysisIdStr = analysisIdStr;
          for (; (stationIdIt != stationIdItEnd) and
                 (currentAnalysisIdStr == bo::QueryResult::toString(analysisIdIt, 0));
               ++analysisIdIt,
               ++stationIdIt,
               ++observationIdIt,
               ++periodStartIt,
               ++periodEndIt,
               ++analysisTimeIt,
               ++analysisVersionIt,
               ++airVolumeIt,
               ++nuclideCodeIt,
               ++concentrationIt,
               ++dataQualityIt,
               ++uncertaintyIt)
          {
            const std::string nCode = bo::QueryResult::toString(nuclideCodeIt);
            group["data"][ind]["nuclideCode"] = nCode;
            NuclideNameMap::const_iterator nNameIt = nuclideNameMap.find(nCode);
            if (nNameIt != nuclideNameMap.end())
              group["data"][ind]["nuclideName"] = nNameIt->second;

            group["data"][ind]["concentration"] = bo::QueryResult::toString(concentrationIt, 2);
            if (showDataQuality)
              group["data"][ind]["dataQuality"] = bo::QueryResult::toString(dataQualityIt, 0);
            group["data"][ind]["uncertainty"] = bo::QueryResult::toString(uncertaintyIt, 0);

            ind++;
          }

          numberMatched++;
          groupId++;
        }
      }

      hash["numberMatched"] = numberMatched;
      hash["numberReturned"] = numberMatched;
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

std::string bw::StoredAirNuclideQueryHandler::prepare_nuclide(const std::string& nuclide) const
{
  try
  {
    std::string out = nuclide;
    boost::algorithm::trim(out);

    const size_t size = out.size();
    if (size < 3)
    {
      SmartMet::Spine::Exception exception(BCP, "Invalid parameter value!");
      exception.addDetail("Too short nuclide code. Minimum length is 3.");
      exception.addParameter(WFS_EXCEPTION_CODE, "WFS_INVALID_PARAMETER_VALUE");
      exception.addParameter("Nuclide code", out);
      throw exception;
    }

    if (size > 8)
    {
      SmartMet::Spine::Exception exception(BCP, "Invalid parameter value!");
      exception.addDetail("Too long nuclide code. Maximum length is 8.");
      exception.addParameter(WFS_EXCEPTION_CODE, WFS_INVALID_PARAMETER_VALUE);
      exception.addParameter("Nuclide code", out);
      throw exception;
    }

    // Nuclide code must begin with upper_case char.
    // It has always '-' char and after that some number.
    // It may end with char.
    // E.g "Cs-137" or "Pm-148M".
    std::string nuclide_upper = Fmi::ascii_toupper_copy(out);
    Fmi::ascii_tolower(out);
    out.at(0) = nuclide_upper.at(0);
    out.at(size - 1) = nuclide_upper.at(size - 1);

    using boost::spirit::ascii::space;
    using boost::spirit::qi::char_;
    using boost::spirit::qi::eps;
    using boost::spirit::qi::lower;
    using boost::spirit::qi::phrase_parse;
    using boost::spirit::qi::upper;
    using boost::spirit::qi::ushort_;

    std::string::const_iterator first = out.begin();
    std::string::const_iterator last = out.end();

    phrase_parse(first, last, upper >> *lower >> char_('-') >> ushort_ >> (upper | eps), space);
    if (first != last)
    {
      SmartMet::Spine::Exception exception(BCP, "Invalid parameter value!");
      exception.addDetail("Unrecognized nuclide code.");
      exception.addParameter(WFS_EXCEPTION_CODE, WFS_INVALID_PARAMETER_VALUE);
      exception.addParameter("Nuclide code", out);
      throw exception;
    }

    return out;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

const std::shared_ptr<SmartMet::Engine::Observation::DBRegistryConfig>
bw::StoredAirNuclideQueryHandler::dbRegistryConfig(const std::string& configName) const
{
  try
  {
    // Get database registry from Engine::Observation.
    const std::shared_ptr<SmartMet::Engine::Observation::DBRegistry> dbRegistry =
        obs_engine->dbRegistry();
    if (not dbRegistry)
    {
      SmartMet::Spine::Exception exception(BCP, "Database registry is not available!");
      exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PROCESSING_FAILED);
      throw exception;
    }

    const std::shared_ptr<SmartMet::Engine::Observation::DBRegistryConfig> dbrConfig =
        dbRegistry->dbRegistryConfig(configName);
    if (not dbrConfig)
    {
      SmartMet::Spine::Exception exception(BCP,
                                           "Database registry configuration is not available!");
      exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PROCESSING_FAILED);
      exception.addParameter("Configration name", configName);
      throw exception;
    }

    return dbrConfig;
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
wfs_stored_air_nuclide_handler_create(SmartMet::Spine::Reactor* reactor,
                                      boost::shared_ptr<StoredQueryConfig> config,
                                      PluginImpl& plugin_data,
                                      boost::optional<std::string> template_file_name)
{
  try
  {
    bw::StoredAirNuclideQueryHandler* qh =
        new bw::StoredAirNuclideQueryHandler(reactor, config, plugin_data, template_file_name);
    boost::shared_ptr<SmartMet::Plugin::WFS::StoredQueryHandlerBase> instance(qh);
    return instance;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}
}  // namespace

SmartMet::Plugin::WFS::StoredQueryHandlerFactoryDef wfs_stored_air_nuclide_handler_factory(
    &wfs_stored_air_nuclide_handler_create);

/**

@page WFS_SQ_AIR_NUCLIDE_QUERY_HANDLER Stored Query handler for querying Air Radionuclide activity
concentration data

@section WFS_SQ_AIR_NUCLIDE_QUERY_HANDLER_INTRO Introduction

The stored query handler provide access to the Air Radionuclide activity concentration data.

<table border="1">
  <tr>
     <td>Implementation</td>
     <td>SmartMet::Plugin::WFS::StoredAirNuclideQueryHandler</td>
  </tr>
  <tr>
    <td>Constructor name (for stored query configuration)</td>
    <td>@b wfs_stored_air_nuclide_handler_factory</td>
  </tr>
</table>



@section WFS_SQ_AIR_NUCLIDE_QUERY_HANDLER_PARAMS Stored query handler built-in parameters

The following stored query handler parameters are being used by this stored query handler:

- @ref WFS_SQ_PARAM_BBOX
- @ref WFS_SQ_LOCATION_PARAMS
- @ref WFS_SQ_QUALITY_PARAMS

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
  <td>nuclideCodes</td>
  <td>@ref WFS_CFG_ARRAY_PARAM_TMPL</td>
  <td>string</td>
  <td>Specifies a list of nuclides. If at least one listed nuclide
      match with a nuclide in a analysis, the analysis will be included into the result.
      In case of empty list match of nuclide will be ignored.</td>
</tr>

<tr>
  <td>geoids</td>
  <td>@ref WFS_CFG_ARRAY_PARAM_TMPL</td>
  <td>int</td>
  <td>Maps to stored query array parameter containing GEOID values for stations. Note that
      this parameter may be left out by constructor parameter.</td>
</tr>

<tr>
  <td>places</td>
  <td>@ref WFS_CFG_ARRAY_PARAM_TMPL</td>
  <td>string</td>
  <td>Maps to the place names like 'Helsinki'</td>
</tr>

<tr>
  <td>stationType</td>
  <td>@ref WFS_CFG_SCALAR_PARAM_TMPL</td>
  <td>string</td>
  <td>Specifies station type.</td>
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
  <td>latest</td>
  <td>@ref WFS_CFG_SCALAR_PARAM_TMPL</td>
  <td>bool</td>
  <td>To return latest (true) values from stations or all (false).</td>
</tr>

</table>

@section WFS_SQ_AIR_NUCLIDE_QUERY_HANDLER_EXTRA_CFG_PARAM Additional parameters

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
  <td>supportQCParameters</td>
  <td>@ref WFS_CFG_SCALAR_PARAM_TMPL</td>
  <td>boolean</td>
  <td>Specifies support of quality code parameters. Quality code parameters are as the normal meteo
parameters but prefixed with "qc_" string. Default value is \b false.</td>
</tr>

</table>

*/

#endif  // WITHOUT_OBSERVATION
