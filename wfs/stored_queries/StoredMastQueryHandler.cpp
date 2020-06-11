#ifndef WITHOUT_OBSERVATION

#include "stored_queries/StoredMastQueryHandler.h"
#include "FeatureID.h"
#include "StoredQueryHandlerFactoryDef.h"
#include <engines/observation/DBRegistry.h>
#include <macgyver/StringConversion.h>
#include <smartmet/engines/observation/MastQuery.h>
#include <smartmet/spine/Convenience.h>
#include <smartmet/spine/Exception.h>
#include <tuple>

namespace bw = SmartMet::Plugin::WFS;

bw::StoredMastQueryHandler::StoredMastQueryHandler(SmartMet::Spine::Reactor* reactor,
                                                   boost::shared_ptr<StoredQueryConfig> config,
                                                   PluginImpl& plugin_data,
                                                   boost::optional<std::string> template_file_name)
    : bw::SupportsExtraHandlerParams(config),
      bw::RequiresGeoEngine(reactor),
      bw::RequiresObsEngine(reactor),
      bw::StoredQueryHandlerBase(reactor, config, plugin_data, template_file_name),
      bw::SupportsLocationParameters(
          reactor, config, INCLUDE_FMISIDS | INCLUDE_GEOIDS | INCLUDE_WMOS),
      bw::SupportsBoundingBox(config, plugin_data.get_crs_registry()),
      bw::SupportsQualityParameters(config)

{
  try
  {
    register_scalar_param<pt::ptime>(P_BEGIN_TIME, *config);
    register_scalar_param<pt::ptime>(P_END_TIME, *config);
    register_array_param<std::string>(P_METEO_PARAMETERS, *config, 1);
    register_scalar_param<std::string>(P_STATION_TYPE, *config);
    register_scalar_param<uint64_t>(P_TIME_STEP, *config);
    register_scalar_param<uint64_t>(P_NUM_OF_STATIONS, *config);
    register_scalar_param<uint64_t>(P_MAX_EPOCHS, *config);
    register_scalar_param<std::string>(P_MISSING_TEXT, *config);
    register_scalar_param<std::string>(P_CRS, *config);
    register_array_param<uint64_t>(P_PRODUCER_ID, *config);

    m_maxHours = config->get_optional_config_param<double>("maxHours", 7.0 * 24.0);
    m_sqRestrictions = plugin_data.get_config().getSQRestrictions();
    m_supportQCParameters = config->get_optional_config_param<bool>("supportQCParameters", false);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

bw::StoredMastQueryHandler::~StoredMastQueryHandler() {}

void bw::StoredMastQueryHandler::query(const StoredQuery& query,
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
      const std::string missingText = params.get_single<std::string>(P_MISSING_TEXT);
      const std::string requestedCrs = params.get_single<std::string>(P_CRS);

      const char* DATA_CRS_NAME = "urn:ogc:def:crs:EPSG::4326";
      auto& crs_registry = plugin_impl.get_crs_registry();

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

      // Get all the information needed in a OBsEngine station search.
      SmartMet::Engine::Observation::Settings stationSearchSettings;
      stationSearchSettings.allplaces = false;
      stationSearchSettings.numberofstations = numberOfStations;
      stationSearchSettings.stationtype = stationType;
      stationSearchSettings.maxdistance = maxDistance;

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
      SmartMet::Spine::Stations stations;
      for (SmartMet::Spine::Stations::const_iterator it = stationCandidates.begin();
           it != stationCandidates.end();
           ++it)
      {
        stations.push_back(*it);

        SmartMet::Spine::LocationPtr geoLoc = geo_engine->idSearch(it->geoid, langCode);
        if (geoLoc)
        {
          stations.back().country = geoLoc->country;
          stations.back().station_elevation = geoLoc->elevation;
        }
      }

      if (stations.empty())
        queryInitializationOK = false;

      // Producers
      std::vector<uint64_t> producerIdVector;
      params.get<uint64_t>(P_PRODUCER_ID, std::back_inserter(producerIdVector));
      if (producerIdVector.empty())
      {
        std::ostringstream msg;
        msg << "At least one producer has to be given.";
        SmartMet::Spine::Exception exception(BCP, msg.str());
        exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PROCESSING_FAILED);
        throw exception.disableStackTrace();
      }

      // Meteo parameters
      std::vector<std::string> meteoParametersVector;
      params.get<std::string>(P_METEO_PARAMETERS, std::back_inserter(meteoParametersVector));
      if (meteoParametersVector.empty())
      {
        SmartMet::Spine::Exception exception(BCP, "Operation processing failed!");
        exception.addDetail("At least one meteo parameter has to be given.");
        exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PROCESSING_FAILED);
        throw exception.disableStackTrace();
      }

      // Do not allow QC parameters if it is not enabled in a stored query.
      auto qc_param_name_ref = SupportsQualityParameters::firstQCParameter(meteoParametersVector);
      if (not m_supportQCParameters && qc_param_name_ref != meteoParametersVector.end())
      {
        SmartMet::Spine::Exception exception(BCP, "Invalid parameter value!");
        exception.addDetail("Quality code parameter '" + *qc_param_name_ref +
                            "' is not allowed in this query.");
        exception.addParameter(WFS_EXCEPTION_CODE, WFS_INVALID_PARAMETER_VALUE);
        throw exception.disableStackTrace();
      }

      // In case if someone is trying to use quality info support.
      if (SupportsQualityParameters::supportQualityInfo(params))
      {
        std::ostringstream msg;
        msg << "Quality info support is not implemented. "
            << "Stored query '" << StoredQueryHandlerBase::get_query_name()
            << "' is trying to use it.\n";
        std::cerr << msg.str();
      }

      // Gluing requested parameter name and parameter identities together.
      // Parameter names are needed in the result document.
      // map: id, (id, paramName, paramQCName, showValue, showQualityCode)
      typedef std::map<std::string, std::tuple<uint64_t, std::string, std::string, bool, bool> >
          MeteoParameterMap;
      MeteoParameterMap meteoParameterMap;
      BOOST_FOREACH (std::string name, meteoParametersVector)
      {
        const uint64_t paramId = obs_engine->getParameterId(name, stationType);
        if (paramId == 0)
        {
          SmartMet::Spine::Exception exception(BCP, "Unknown parameter in the query!");
          exception.addParameter(WFS_EXCEPTION_CODE, WFS_INVALID_PARAMETER_VALUE);
          exception.addParameter("Unknown parameter", name);
          throw exception.disableStackTrace();
        }

        bool isQCParameter = SupportsQualityParameters::isQCParameter(name);

        // Is the parameter a duplicate.
        const std::string paramIdStr = Fmi::to_string(paramId);
        MeteoParameterMap::iterator paramIdIt = meteoParameterMap.find(paramIdStr);
        if (paramIdIt != meteoParameterMap.end())
        {
          // Check if the parameter is already inserted.
          if ((std::get<3>(paramIdIt->second) and not isQCParameter) or
              (std::get<4>(paramIdIt->second) and isQCParameter))
          {
            std::string errParamName;
            if (isQCParameter)
              errParamName = std::get<3>(paramIdIt->second);
            else
              errParamName = std::get<2>(paramIdIt->second);

            std::ostringstream msg;
            msg << "Parameter '" << name << "' is already given by using '" << errParamName
                << "' name.";
            SmartMet::Spine::Exception exception(BCP, msg.str());
            exception.addParameter(WFS_EXCEPTION_CODE, WFS_INVALID_PARAMETER_VALUE);
            throw exception;
          }
          else
          {
            // Update values of a tuple item.
            if (isQCParameter)
            {
              std::get<2>(paramIdIt->second) = name;
              std::get<4>(paramIdIt->second) = true;
            }
            else
            {
              std::get<1>(paramIdIt->second) = name;
              std::get<3>(paramIdIt->second) = true;
            }
          }
        }
        else
        {
          // Initial map item
          meteoParameterMap.insert(std::make_pair(paramIdStr,
                                                  std::make_tuple(paramId,
                                                                  (isQCParameter ? "" : name),
                                                                  (isQCParameter ? name : ""),
                                                                  (not isQCParameter),
                                                                  (isQCParameter))));
        }
      }

      // Time range restriction to get data.
      pt::ptime startTime = params.get_single<pt::ptime>(P_BEGIN_TIME);
      pt::ptime endTime = params.get_single<pt::ptime>(P_END_TIME);
      const uint64_t timestep = params.get_single<uint64_t>(P_TIME_STEP);
      const uint64_t maxEpochs = params.get_single<uint64_t>(P_MAX_EPOCHS);
      if (m_sqRestrictions)
        check_time_interval(startTime, endTime, m_maxHours);

      // Only 24 hours allowed
      if (1440 < timestep)
      {
        SmartMet::Spine::Exception exception(BCP, "Invalid time step value!");
        exception.addDetail("Maximum timestep value is 1440 minutes.");
        exception.addParameter(WFS_EXCEPTION_CODE, WFS_INVALID_PARAMETER_VALUE);
        exception.addParameter("Timestep", std::to_string(timestep));
        throw exception.disableStackTrace();
      }

      // Assume timestep 1 minutes when value is 0.
      uint64_t ts1 = (timestep > 0) ? timestep : 1;
      if (m_sqRestrictions and startTime + pt::minutes(maxEpochs * ts1) < endTime)
      {
        SmartMet::Spine::Exception exception(BCP, "Too many time epochs in the time interval!");
        exception.addDetail("Use shorter time interval or larger time step.");
        exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PROCESSING_FAILED);
        exception.addParameter("Start time", pt::to_simple_string(startTime));
        exception.addParameter("End time", pt::to_simple_string(endTime));
        throw exception.disableStackTrace();
      }

      //
      // Observation ID search
      //

      // Using OBSERVATIONS_V2 as a base configuration
      // Creating query parameters object by using the base configuration.
      // All the other configurations are joined to this.
      bo::MastQueryParams stationQueryParams(dbRegistryConfig("OBSERVATIONS_V2"));
      stationQueryParams.addField("OBSERVATION_ID");

      // Join on CONFIGURATIONS_V2 view by using OBSERVATION_ID.
      stationQueryParams.addJoinOnConfig(dbRegistryConfig("CONFIGURATIONS_V2"), "OBSERVATION_ID");

      // Join on STATIONS_V1 view by using STATION_ID
      stationQueryParams.addJoinOnConfig(dbRegistryConfig("STATIONS_V1"), "STATION_ID");

      // Join on MEASURANDS_V1 view by using MEASURAND_ID.
      stationQueryParams.addJoinOnConfig(dbRegistryConfig("MEASURANDS_V1"), "MEASURAND_ID");

      // Producer identities
      for (std::vector<uint64_t>::const_iterator it = producerIdVector.begin();
           queryInitializationOK && it != producerIdVector.end();
           ++it)
        stationQueryParams.addOperation(
            "OR_GROUP_producer_id", "PRODUCER_ID", "PropertyIsEqualTo", *it);

      // Station identities
      for (SmartMet::Spine::Stations::const_iterator it = stations.begin();
           queryInitializationOK && it != stations.end();
           ++it)
        stationQueryParams.addOperation(
            "OR_GROUP_station_id", "STATION_ID", "PropertyIsEqualTo", (*it).fmisid);

      // Measurand identities
      for (MeteoParameterMap::const_iterator it = meteoParameterMap.begin();
           queryInitializationOK && it != meteoParameterMap.end();
           ++it)
        stationQueryParams.addOperation("OR_GROUP_measurand_code",
                                        "MEASURAND_ID",
                                        "PropertyIsEqualTo",
                                        std::get<0>(it->second));

      // Measurand period
      std::string measurandPeriod = "instant";
      stationQueryParams.addOperation(
          "OR_GROUP_measurand_period", "MEASURAND_PERIOD", "PropertyIsEqualTo", measurandPeriod);

      bo::MastQuery profileQuery;
      profileQuery.setQueryParams(&stationQueryParams);
      if (queryInitializationOK)
        obs_engine->makeQuery(&profileQuery);

      // Container with the observation identities of the requested stations.
      std::shared_ptr<bo::QueryResult> profileContainer = profileQuery.getQueryResultContainer();

      bo::MastQuery dataQuery;

      // At least one observation_id is required for data search.
      if (not profileContainer or (profileContainer->size("OBSERVATION_ID") == 0))
      {
        queryInitializationOK = false;
      }
      else
      {
        //
        // Get the data match with the observation_id values
        //

        // Using OBSERVATION_DATA_V1 as a base configuration
        bo::MastQueryParams dataQueryParams(dbRegistryConfig("OBSERVATION_DATA_V1"));

        dataQueryParams.addField("STATION_ID");
        dataQueryParams.addField("MEASURAND_ID");
        dataQueryParams.addField("DATA_TIME");
        dataQueryParams.addField("DATA_LEVEL");
        dataQueryParams.addField("DATA_VALUE");
        dataQueryParams.addField("DATA_QUALITY");

        dataQueryParams.addOrderBy("STATION_ID", "ASC");
        dataQueryParams.addOrderBy("DATA_TIME", "ASC");
        dataQueryParams.addOrderBy("MEASURAND_ID", "ASC");
        dataQueryParams.addOrderBy("DATA_LEVEL", "ASC");

        bo::QueryResult::ValueVectorType::const_iterator observationIdIt =
            profileContainer->begin("OBSERVATION_ID");
        bo::QueryResult::ValueVectorType::const_iterator observationIdItEnd =
            profileContainer->end("OBSERVATION_ID");

        for (; queryInitializationOK && observationIdIt != observationIdItEnd; ++observationIdIt)
          dataQueryParams.addOperation(
              "OR_GROUP_observation_id", "OBSERVATION_ID", "PropertyIsEqualTo", *observationIdIt);

        dataQueryParams.addOperation(
            "OR_GROUP_data_begin_time", "DATA_TIME", "PropertyIsGreaterThanOrEqualTo", startTime);
        dataQueryParams.addOperation(
            "OR_GROUP_data_end_time", "DATA_TIME", "PropertyIsLessThanOrEqualTo", endTime);
        dataQueryParams.addOperation(
            "OR_GROUP_data_timestep", "DATA_TIME", "PropertyMinuteValueModuloIsEqualToZero", ts1);
        dataQuery.setQueryParams(&dataQueryParams);

        if (queryInitializationOK)
          obs_engine->makeQuery(&dataQuery);
      }

      // Get the sequence number of query in the request
      int sq_id = query.get_query_id();
      FeatureID feature_id(get_config()->get_query_id(), params.get_map(), sq_id);
      const std::string fullFeatureId = feature_id.get_id();

      // Removing some feature id parameters
      feature_id.erase_param(P_BOUNDING_BOX);
      feature_id.erase_param(P_PLACES);
      feature_id.erase_param(P_GEOIDS);
      feature_id.erase_param(P_WMOS);

      //
      // Fill data for xml formatting.
      //

      CTPP::CDT hash;

      hash["featureId"] = feature_id.get_id();

      hash["responseTimestamp"] =
          Fmi::to_iso_extended_string(get_plugin_impl().get_time_stamp()) + "Z";
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

      std::shared_ptr<bo::QueryResult> dataContainer = dataQuery.getQueryResultContainer();

      int numberMatched = 0;
      if (queryInitializationOK and profileContainer and dataContainer)
      {
        bo::QueryResult::ValueVectorType::const_iterator dataFmisidIt =
            dataContainer->begin("STATION_ID");
        bo::QueryResult::ValueVectorType::const_iterator dataFmisidItEnd =
            dataContainer->end("STATION_ID");
        bo::QueryResult::ValueVectorType::const_iterator dataMeasurandIdIt =
            dataContainer->begin("MEASURAND_ID");
        bo::QueryResult::ValueVectorType::const_iterator dataTimeIt =
            dataContainer->begin("DATA_TIME");
        bo::QueryResult::ValueVectorType::const_iterator dataLevelIt =
            dataContainer->begin("DATA_LEVEL");
        bo::QueryResult::ValueVectorType::const_iterator dataValueIt =
            dataContainer->begin("DATA_VALUE");
        bo::QueryResult::ValueVectorType::const_iterator dataQualityIt =
            dataContainer->begin("DATA_QUALITY");

        std::string currentFmisid = "FooBarFmisid";
        std::string currentMeasurandId = "FooBarMeasurandId";
        std::string currentDataTime = "FooBarTime";
        int dataId = 0;
        int groupId = 0;
        double bottomHeight = 0.0;
        double deltaHeight = 0.0;
        bool showValue = false;
        bool showDataQuality = false;
        for (; dataFmisidIt != dataFmisidItEnd; ++dataFmisidIt,
                                                ++dataMeasurandIdIt,
                                                ++dataTimeIt,
                                                ++dataLevelIt,
                                                ++dataValueIt,
                                                ++dataQualityIt)
        {
          std::string fmisidStr = bo::QueryResult::toString(dataFmisidIt, 0);
          std::string measurandIdStr = bo::QueryResult::toString(dataMeasurandIdIt, 0);
          std::string dataTimeStr = bo::QueryResult::toString(dataTimeIt);
          std::string dataLevelStr = bo::QueryResult::toString(dataLevelIt, 1);
          std::string dataValueStr = bo::QueryResult::toString(dataValueIt, 1);
          std::string dataQualityStr = bo::QueryResult::toString(dataQualityIt, 0);
          deltaHeight = boost::any_cast<double>(*dataLevelIt);

          if (currentFmisid != fmisidStr or currentMeasurandId != measurandIdStr or
              currentDataTime != dataTimeStr)
          {
            currentFmisid = fmisidStr;
            currentMeasurandId = measurandIdStr;
            currentDataTime = dataTimeStr;

            numberMatched++;
            groupId = numberMatched - 1;

            std::string station_geoid;
            std::string station_wmo;
            std::string station_name;
            std::string station_region;
            std::string station_latitude;
            std::string station_longitude;
            std::string station_elevation;

            for (SmartMet::Spine::Stations::const_iterator sit = stations.begin();
                 sit != stations.end();
                 ++sit)
            {
              if (Fmi::to_string(sit->fmisid) == fmisidStr)
              {
                station_geoid = std::to_string(static_cast<long long int>(sit->geoid));
                station_wmo = std::to_string(static_cast<long long int>(sit->wmo));
                station_name = sit->station_formal_name;
                station_region = sit->region;
                station_latitude = std::to_string(static_cast<long double>(sit->latitude_out));
                station_longitude = std::to_string(static_cast<long double>(sit->longitude_out));
                station_elevation =
                    std::to_string(static_cast<long long int>(sit->station_elevation));
                bottomHeight = sit->station_elevation;
              }
            }

            // Setting the station id for the feature id.
            feature_id.erase_param(P_FMISIDS);
            feature_id.add_param(P_FMISIDS, fmisidStr);

            // Only one station per member group.
            feature_id.erase_param(P_NUM_OF_STATIONS);
            feature_id.add_param(P_NUM_OF_STATIONS, "1");

            hash["groups"][groupId]["station"]["fmisid"] = fmisidStr;
            if (not station_geoid.empty() && station_geoid != "0")
              hash["groups"][groupId]["station"]["geoid"] = station_geoid;
            if (not station_wmo.empty() && station_wmo != "0" && station_wmo != "NaN")
              hash["groups"][groupId]["station"]["wmo"] = station_wmo;
            if (not station_name.empty())
              hash["groups"][groupId]["station"]["name"] = station_name;
            if (not station_region.empty())
              hash["groups"][groupId]["station"]["region"] = station_region;

            set_2D_coord(transformation,
                         station_latitude,
                         station_longitude,
                         hash["groups"][groupId]["station"]);
            hash["groups"][groupId]["station"]["elevation"] = station_elevation;

            if (show_height)
              hash["groups"][groupId]["station"]["height"] = station_elevation;

            // Setting time the correct values for the feature id.
            // One member group contains only one time position.
            feature_id.erase_param(P_TIME_STEP);
            feature_id.add_param(P_TIME_STEP, "1");
            feature_id.erase_param(P_BEGIN_TIME);
            feature_id.add_param(P_BEGIN_TIME, dataTimeStr);
            feature_id.erase_param(P_END_TIME);
            feature_id.add_param(P_END_TIME, dataTimeStr);

            hash["groups"][groupId]["phenomenonTime"] = dataTimeStr;
            hash["groups"][groupId]["resultTime"] = dataTimeStr;

            // Deleting the requested meteoparameters from feature id
            // so we can add the parameters used in the member group.
            feature_id.erase_param(P_METEO_PARAMETERS);
            MeteoParameterMap::const_iterator it = meteoParameterMap.find(measurandIdStr);
            if (it != meteoParameterMap.end())
            {
              showValue = std::get<3>(it->second);
              showDataQuality = std::get<4>(it->second);
              int paramcount = 0;
              if (showValue)
              {
                const std::string paramName = std::get<1>(it->second);
                hash["groups"][groupId]["obsParamList"][paramcount]["name"] = paramName;
                feature_id.add_param(P_METEO_PARAMETERS, paramName);
                paramcount++;
              }
              if (showDataQuality)
              {
                const std::string paramName = std::get<2>(it->second);
                hash["groups"][groupId]["obsParamList"][paramcount]["name"] = paramName;
                hash["groups"][groupId]["obsParamList"][paramcount]["isQCParameter"] = "true";
                feature_id.add_param(P_METEO_PARAMETERS, paramName);
                paramcount++;
              }

              // This should newer happen (.. but in case off coding error).
              if (not showValue and not showDataQuality)
              {
                std::ostringstream msg;
                msg << "Cannot solve what to return by using parameter id '" << measurandIdStr
                    << "'";
                SmartMet::Spine::Exception exception(BCP, msg.str());
                exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PROCESSING_FAILED);
                throw exception;
              }
              hash["groups"][groupId]["featureId"] = feature_id.get_id();
            }
            else
            {
              std::ostringstream msg;
              msg << METHOD_NAME << ": Cannot find parameter name for id '" << measurandIdStr
                  << "'\n";
              ;
              std::cerr << msg.str();
            }

            dataId = 0;
          }

          hash["groups"][groupId]["data"][dataId]["time"] = dataTimeStr;
          hash["groups"][groupId]["data"][dataId]["level"] = dataLevelStr;
          if (showValue)
            hash["groups"][groupId]["data"][dataId]["value"] = dataValueStr;
          if (showDataQuality)
            hash["groups"][groupId]["data"][dataId]["quality"] = dataQualityStr;
          hash["groups"][groupId]["topHeight"] =
              std::to_string(static_cast<long long int>(bottomHeight + deltaHeight));

          dataId++;
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

void bw::StoredMastQueryHandler::update_parameters(
    const RequestParameterMap& params,
    int seq_id,
    std::vector<boost::shared_ptr<RequestParameterMap> >& result) const
{
  try
  {
    std::cerr << "bw::StoredMastQueryHandler::update_parameters(params, seq_id, result)\n";
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

const std::shared_ptr<SmartMet::Engine::Observation::DBRegistryConfig>
bw::StoredMastQueryHandler::dbRegistryConfig(const std::string& configName) const
{
  try
  {
    // Get database registry from Observation
    const std::shared_ptr<SmartMet::Engine::Observation::DBRegistry> dbRegistry =
        obs_engine->dbRegistry();
    if (not dbRegistry)
    {
      std::ostringstream msg;
      msg << "Database registry is not available!";
      SmartMet::Spine::Exception exception(BCP, msg.str());
      exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PROCESSING_FAILED);
      throw exception;
    }

    const std::shared_ptr<SmartMet::Engine::Observation::DBRegistryConfig> dbrConfig =
        dbRegistry->dbRegistryConfig(configName);
    if (not dbrConfig)
    {
      std::ostringstream msg;
      msg << "Database registry configuration '" << configName << "' is not available!";
      SmartMet::Spine::Exception exception(BCP, msg.str());
      exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PROCESSING_FAILED);
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

boost::shared_ptr<SmartMet::Plugin::WFS::StoredQueryHandlerBase> wfs_stored_mast_handler_create(
    SmartMet::Spine::Reactor* reactor,
    boost::shared_ptr<StoredQueryConfig> config,
    PluginImpl& plugin_data,
    boost::optional<std::string> template_file_name)
{
  try
  {
    bw::StoredMastQueryHandler* qh =
        new bw::StoredMastQueryHandler(reactor, config, plugin_data, template_file_name);
    boost::shared_ptr<SmartMet::Plugin::WFS::StoredQueryHandlerBase> instance(qh);
    return instance;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}
}  // namespace

SmartMet::Plugin::WFS::StoredQueryHandlerFactoryDef wfs_stored_mast_handler_factory(
    &wfs_stored_mast_handler_create);

/**

@page WFS_SQ_MAST_QUERY_HANDLER Stored Query handler for querying Mast data

@section WFS_SQ_MAST_QUERY_HANDLER_INTRO Introduction

The stored query handler provide access to the mast data.

<table border="1">
  <tr>
     <td>Implementation</td>
     <td>SmartMet::Plugin::WFS::StoredMastQueryHandler</td>
  </tr>
  <tr>
    <td>Constructor name (for stored query configuration)</td>
    <td>@b wfs_stored_mast_handler_factory</td>
  </tr>
</table>

@section WFS_SQ_MAST_QUERY_HANDLER_PARAMS Query handler built-in parameters

The following stored query handler parameters are being used by this stored query handler:

@ref WFS_SQ_PARAM_BBOX_PARAM "Bounding box"

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
  <td>maxEpochs</td>
  <td>@ref WFS_CFG_SCALAR_PARAM_TMPL</td>
  <td>integer</td>
  <td>Specifies maximum epochs allowed.</td>
</tr>

</table>

@section WFS_SQ_MAST_QUERY_HANDLER_EXTRA_CFG_PARAM Additional parameters

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
  <td>producerId</td>
  <td>@ref WFS_CFG_SCALAR_PARAM_TMPL</td>
  <td>integer</td>
  <td>Specifies producer id. Only the data that match with producer id will be returned.</td>
</tr>

<tr>
  <td>maxHours</td>
  <td>@ref WFS_CFG_SCALAR_PARAM_TMPL</td>
  <td>integer</td>
  <td>Specifies maximal permitted time interval in hours. The default value
      is 168 hours (7 days). This parameter is not valid, if the *optional*
      storedQueryRestrictions parameter is set to false in WFS Plugin
      configurations.</td>
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
