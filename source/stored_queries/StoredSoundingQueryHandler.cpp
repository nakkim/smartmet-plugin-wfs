#ifndef WITHOUT_OBSERVATION

#include "stored_queries/StoredSoundingQueryHandler.h"
#include "FeatureID.h"
#include "StoredQueryHandlerFactoryDef.h"
#include <boost/date_time/posix_time/ptime.hpp>
#include <engines/gis/CRSRegistry.h>
#include <engines/gis/Engine.h>
#include <engines/observation/MastQuery.h>
#include <macgyver/TimeParser.h>
#include <spine/Convenience.h>
#include <spine/Exception.h>

#include <tuple>

namespace pt = boost::posix_time;
namespace bo = SmartMet::Engine::Observation;

namespace SmartMet
{
namespace Plugin
{
namespace WFS
{
StoredSoundingQueryHandler::StoredSoundingQueryHandler(
    SmartMet::Spine::Reactor* reactor,
    boost::shared_ptr<StoredQueryConfig> config,
    PluginImpl& pluginData,
    boost::optional<std::string> templateFileName)
    : SupportsExtraHandlerParams(config),
      StoredQueryHandlerBase(reactor, config, pluginData, templateFileName),
      SupportsLocationParameters(
          config, SUPPORT_KEYWORDS | INCLUDE_FMISIDS | INCLUDE_GEOIDS | INCLUDE_WMOS),
      SupportsBoundingBox(config, pluginData.get_crs_registry()),
      SupportsQualityParameters(config)
{
  register_scalar_param<pt::ptime>(P_BEGIN_TIME, *config);
  register_scalar_param<pt::ptime>(P_END_TIME, *config);
  register_array_param<std::string>(P_METEO_PARAMETERS, *config, 1);
  register_scalar_param<std::string>(P_STATION_TYPE, *config);
  register_scalar_param<uint64_t>(P_NUM_OF_STATIONS, *config);
  register_scalar_param<std::string>(P_MISSING_TEXT, *config);
  register_scalar_param<std::string>(P_CRS, *config);
  register_scalar_param<bool>(P_LATEST, *config);
  register_scalar_param<uint64_t>(P_SOUNDING_TYPE, *config);
  register_array_param<uint64_t>(P_PUBLICITY, *config, 1);
  register_array_param<double>(P_ALTITUDE_RANGES, *config, 0, 2, 2);
  register_array_param<double>(P_PRESSURE_RANGES, *config, 0, 2, 2);

  mMaxHours = config->get_optional_config_param<double>("maxHours", 7.0 * 24.0);
  mSqRestrictions = pluginData.get_config().getSQRestrictions();
  mSupportQCParameters = config->get_optional_config_param<bool>("supportQCParameters", false);
  mMaxSoundings = config->get_optional_config_param<uint64_t>("maxSoundings", 15);
}

StoredSoundingQueryHandler::~StoredSoundingQueryHandler() {}

void StoredSoundingQueryHandler::init_handler()
{
  auto* reactor = get_reactor();

  void* engine;
  engine = reactor->getSingleton("Geonames", nullptr);
  if (engine == nullptr)
  {
    throw std::runtime_error("No Geonames available");
  }

  mGeonames = reinterpret_cast<SmartMet::Engine::Geonames::Engine*>(engine);

  engine = reactor->getSingleton("Observation", nullptr);
  if (engine == nullptr)
  {
    throw std::runtime_error("No Observation available");
  }

  mObservation = reinterpret_cast<SmartMet::Engine::Observation::Engine*>(engine);
}

void StoredSoundingQueryHandler::query(const StoredQuery& query,
                                       const std::string& language,
                                       const boost::optional<std::string>& hostname,
                                       std::ostream& output) const
{
  const auto& params = query.get_param_map();

  // Monitoring query parameter initialization process.
  // If the value is false empty result will be returned.
  bool queryInitializationOK = true;

  try
  {
    auto missingValue = params.get_single<std::string>(P_MISSING_TEXT);
    const int debugLevel = get_config()->get_debug_level();
    const std::string crs = solveCrs(params);
    SmartMet::Engine::Gis::CRSRegistry& crsRegistry = get_plugin_impl().get_crs_registry();
    auto transformation = crsRegistry.create_transformation(DATA_CRS_NAME, crs);

    bool showHeight = false;
    crsRegistry.get_attribute(crs, "showHeight", &showHeight);
    if (!showHeight)
    {
      SmartMet::Spine::Exception exception(BCP, "Invalid parameter value");
      exception.addParameter(WFS_EXCEPTION_CODE, WFS_INVALID_PARAMETER_VALUE);
      exception.addDetail("A crs without z dimension is not allowed in this query.");
      throw exception;
    }

    std::string projUri = "UNKNOWN";
    std::string projEpochUri = "UNKNOWN";
    std::string axisLabels = "";
    crsRegistry.get_attribute(crs, "projUri", &projUri);
    crsRegistry.get_attribute(crs, "projEpochUri", &projEpochUri);
    crsRegistry.get_attribute(crs, "axisLabels", &axisLabels);

    SmartMet::Engine::Observation::Settings stationSearchSettings;
    getStationSearchSettings(stationSearchSettings, params, language);

    // The result does not contain duplicates.
    SmartMet::Spine::Stations stations;
    mObservation->getStations(stations, stationSearchSettings);
    if (stations.empty())
      queryInitializationOK = false;

    // Gluing requested parameter name and parameter identities together.
    // Parameter names are needed in the result document.
    // map: id, (id, paramName, paramQCName, showValue, showQualityCode)
    MeteoParameterMap meteoParameterMap;
    std::string pressureParameterName = "";
    validateAndPopulateMeteoParametersToMap(params, meteoParameterMap, pressureParameterName);

    // Time range restriction to get data.
    pt::ptime startTime = params.get_single<pt::ptime>(P_BEGIN_TIME);
    pt::ptime endTime = params.get_single<pt::ptime>(P_END_TIME);
    if (mSqRestrictions)
      check_time_interval(startTime, endTime, mMaxHours);

    RadioSoundingMap radioSoundingMap;
    QueryResultShared soundingQueryResult;
    if (queryInitializationOK)
    {
      makeSoundingQuery(params, stations, soundingQueryResult);
      parseSoundingQuery(params, soundingQueryResult, radioSoundingMap);
    }

    if (radioSoundingMap.empty())
      queryInitializationOK = false;

    if (mSqRestrictions)
      checkMaxSoundings(startTime, endTime, radioSoundingMap);

    CTPP::CDT hash;
    int numberMatched = 0;

    boost::posix_time::ptime queryStartTime = get_plugin_impl().get_time_stamp();

    // Execute query
    QueryResultShared dataContainer;
    makeSoundingDataQuery(params, radioSoundingMap, meteoParameterMap, dataContainer);
    if (not dataContainer or dataContainer->size("SOUNDING_ID") == 0)
      queryInitializationOK = false;

    if (queryInitializationOK)
    {
      boost::posix_time::ptime queryEndTime = get_plugin_impl().get_time_stamp();
      if (debugLevel > 2)
        std::cerr << "Data query time delta: " << (queryEndTime - queryStartTime).total_seconds()
                  << " seconds\n";

      int sq_id = query.get_query_id();
      FeatureID featureId(get_config()->get_query_id(), params.get_map(), sq_id);
      const char* placeParams[] = {
          P_WMOS, P_FMISIDS, P_PLACES, P_LATLONS, P_GEOIDS, P_KEYWORD, P_BOUNDING_BOX};
      for (unsigned i = 0; i < sizeof(placeParams) / sizeof(*placeParams); i++)
      {
        featureId.erase_param(placeParams[i]);
      }

      hash["projSrsDim"] = 3;
      hash["projSrsName"] = projUri;
      hash["projEpochSrsDim"] = 4;
      hash["projEpochSrsName"] = projEpochUri;
      hash["axisLabels"] = axisLabels;
      hash["queryNum"] = query.get_query_id();
      hash["queryName"] = StoredQueryHandlerBase::get_query_name();
      hash["phenomenonBeginTime"] = pt::to_iso_extended_string(startTime) + "Z";
      hash["phenomenonEndTime"] = pt::to_iso_extended_string(endTime) + "Z";

      if (debugLevel > 2)
        params.dump_params(hash["query_parameters"]);
      dump_named_params(params, hash["named_parameters"]);

      if (queryInitializationOK and dataContainer)
      {
        ValueVectorConstIt dataSoundingIdItBegin = dataContainer->begin("SOUNDING_ID");
        ValueVectorConstIt dataSoundingIdIt = dataContainer->begin("SOUNDING_ID");
        ValueVectorConstIt dataSoundingIdItEnd = dataContainer->end("SOUNDING_ID");
        ValueVectorConstIt dataMeasurandIdIt = dataContainer->begin("MEASURAND_ID");
        ValueVectorConstIt dataLevelTimeIt = dataContainer->begin("LEVEL_TIME");
        ValueVectorConstIt dataAltitudeIt = dataContainer->begin("ALTITUDE");
        ValueVectorConstIt dataPressureIt = dataContainer->begin("PRESSURE");
        ValueVectorConstIt dataLongitudeIt = dataContainer->begin("LONGITUDE");
        ValueVectorConstIt dataLatitudeIt = dataContainer->begin("LATITUDE");
        ValueVectorConstIt dataQualityIt = dataContainer->begin("DATA_QUALITY");
        ValueVectorConstIt dataValueIt = dataContainer->begin("DATA_VALUE");
        ValueVectorConstIt dataSignificanceIt = dataContainer->begin("SIGNIFICANCE");

        std::string currentStationId, currentMeasId;
        int dataId = 0, groupId = 0;
        int paramcount = 0;
        int paramListCount = 0;
        bool showValue = false, showDataQuality = false;
        INT_64 sEpoch = 0;
        int64_t currentSoundingId = 0;
        long double stationLatitude = 0.0;
        long double stationLongitude = 0.0;

        for (; dataSoundingIdIt != dataSoundingIdItEnd; ++dataSoundingIdIt,
                                                        ++dataMeasurandIdIt,
                                                        ++dataLevelTimeIt,
                                                        ++dataAltitudeIt,
                                                        ++dataPressureIt,
                                                        ++dataLongitudeIt,
                                                        ++dataLatitudeIt,
                                                        ++dataValueIt,
                                                        ++dataQualityIt,
                                                        ++dataSignificanceIt)

        {
          std::string measurandIdStr =
              SmartMet::Engine::Observation::QueryResult::toString(dataMeasurandIdIt, 0);
          int64_t soundingId = dataContainer->castTo<int64_t>(dataSoundingIdIt);

          if ((dataSoundingIdIt == dataSoundingIdItBegin) or (currentSoundingId != soundingId))
          {
            currentSoundingId = soundingId;
            groupId = numberMatched;
            numberMatched++;
            dataId = 0;

            auto rsIt = radioSoundingMap.find(currentSoundingId);
            if (rsIt == radioSoundingMap.end())
            {
              std::ostringstream msg;
              msg << "Sounding metadata does not found for sounding id '" << currentSoundingId
                  << "'.";

              SmartMet::Spine::Exception exception(BCP, "Operation processsing failed");
              exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PROCESSING_FAILED);
              exception.addDetail(msg.str());
              throw exception;
            }

            currentStationId =
                SmartMet::Engine::Observation::QueryResult::toString(rsIt->second.mStationId, 0);

            std::string dataMessageTimeStr =
                SmartMet::Engine::Observation::QueryResult::toString(rsIt->second.mMessageTime);
            static const long ref_jd = boost::gregorian::date(1970, 1, 1).julian_day();
            boost::posix_time::ptime epoch = Fmi::TimeParser::parse_iso(dataMessageTimeStr);
            long long jd = epoch.date().julian_day();
            long seconds = epoch.time_of_day().total_seconds();
            sEpoch = 86400LL * (jd - ref_jd) + seconds;

            CTPP::CDT& group = hash["groups"][groupId];
            const std::string launcTimeStr =
                SmartMet::Engine::Observation::QueryResult::toString(rsIt->second.mLaunchTime);
            group["phenomenonBeginTime"] = launcTimeStr.empty() ? dataMessageTimeStr : launcTimeStr;
            const std::string soundingEndStr =
                SmartMet::Engine::Observation::QueryResult::toString(rsIt->second.mSoundingEnd);
            group["phenomenonEndTime"] =
                soundingEndStr.empty() ? dataMessageTimeStr : soundingEndStr;
            group["phenomenonTime"] = dataMessageTimeStr;
            group["resultTime"] = dataMessageTimeStr;
            group["soundingId"] = soundingId;

            featureId.erase_param(P_BEGIN_TIME);
            featureId.erase_param(P_END_TIME);
            featureId.erase_param(P_FMISIDS);
            featureId.add_param(P_BEGIN_TIME, dataMessageTimeStr);
            featureId.add_param(P_END_TIME, dataMessageTimeStr);
            featureId.add_param(P_FMISIDS, currentStationId);

            group["featureId"] = featureId.get_id();

            for (SmartMet::Spine::Stations::const_iterator sit = stations.begin();
                 sit != stations.end();
                 ++sit)
            {
              if (Fmi::to_string(sit->fmisid) == currentStationId)
              {
                CTPP::CDT& station = group["station"];
                station["fmisid"] = currentStationId;
                std::string geoid = std::to_string(static_cast<long long int>(sit->geoid));
                if (not geoid.empty())
                  station["geoid"] = geoid;
                std::string wmo = std::to_string(static_cast<long long int>(sit->wmo));
                if (not wmo.empty())
                  station["wmo"] = wmo;
                if (not sit->station_formal_name.empty())
                  station["name"] = sit->station_formal_name;
                if (not sit->region.empty())
                  station["region"] = sit->region;

                stationLatitude = static_cast<long double>(sit->latitude_out);
                stationLongitude = static_cast<long double>(sit->longitude_out);
                set_2D_coord(transformation,
                             std::to_string(stationLatitude),
                             std::to_string(stationLongitude),
                             station);
                station["elevation"] =
                    std::to_string(static_cast<long long int>(sit->station_elevation));
              }
              else if (std::next(sit, 0) == stations.end())
              {
                std::ostringstream msg;
                msg << METHOD_NAME << ": Cannot find station metadata for fmisid '"
                    << currentStationId << "'\n";
                SmartMet::Spine::Exception exception(BCP, "Operation processsing failed");
                exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PROCESSING_FAILED);
                exception.addDetail(msg.str());
                throw exception;
              }
            }

            paramcount = 0;
            paramListCount = 0;
          }

          if (paramListCount == 0 or (currentMeasId != measurandIdStr))
          {
            currentMeasId = measurandIdStr;

            MeteoParameterMap::const_iterator it = meteoParameterMap.find(measurandIdStr);
            if (it == meteoParameterMap.end())
            {
              std::ostringstream msg;
              msg << METHOD_NAME << ": Cannot find parameter name for id '" << measurandIdStr
                  << "'\n";
              SmartMet::Spine::Exception exception(BCP, "Operation processsing failed");
              exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PROCESSING_FAILED);
              exception.addDetail(msg.str());
              throw exception;
            }

            showValue = std::get<3>(it->second);
            showDataQuality = std::get<4>(it->second);

            // This should newer happen (.. but in case off coding error).
            if (not showValue and not showDataQuality)
            {
              std::ostringstream msg;
              msg << METHOD_NAME << ": Cannot solve what to return by using parameter id '"
                  << measurandIdStr << "'\n";
              SmartMet::Spine::Exception exception(BCP, "Operation processsing failed");
              exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PROCESSING_FAILED);
              exception.addDetail(msg.str());
              throw exception;
            }

            if (showValue)
            {
              if (paramListCount == 0 && !pressureParameterName.empty())
              {
                CTPP::CDT& param = hash["groups"][groupId]["obsParamList"][paramListCount++];
                param["name"] = pressureParameterName;
              }
              CTPP::CDT& param = hash["groups"][groupId]["obsParamList"][paramListCount++];
              param["name"] = std::get<1>(it->second);
            }
            if (showDataQuality)
            {
              CTPP::CDT& param = hash["groups"][groupId]["obsParamList"][paramListCount++];
              param["name"] = std::get<2>(it->second);
              param["isQCParameter"] = "true";
            }

            dataId = 0;
            paramcount++;
          }

          if (paramcount < 1)
          {
            std::ostringstream msg;
            msg << "Invalid parameter count '" << paramcount << "'.\n";
            SmartMet::Spine::Exception exception(BCP, "Operation processsing failed");
            exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PROCESSING_FAILED);
            exception.addDetail(msg.str());
            throw exception;
          }

          CTPP::CDT& row = hash["groups"][groupId]["obsReturnArray"][dataId];

          if (paramcount == 1)
          {
            row["epochTime"] = sEpoch + dataContainer->castTo<int64_t>(dataLevelTimeIt);
            row["altitude"] =
                SmartMet::Engine::Observation::QueryResult::toString(dataAltitudeIt, 1);
            row["pressure"] = dataContainer->castTo<double>(dataPressureIt);
            const double levelLat = stationLatitude + dataContainer->castTo<double>(dataLatitudeIt);
            const double levelLon =
                stationLongitude + dataContainer->castTo<double>(dataLongitudeIt);
            set_2D_coord(transformation, levelLat, levelLon, row);
            uint32_t significance = dataContainer->castTo<uint32_t>(dataSignificanceIt);
            row["significance"] = significance;
            std::bitset<18> sBitset(significance);
            if (sBitset.any())
            {
              /*
                1 Surface
                2 Standard level
                3 Tropopause level
                4 Maximum wind level
                5 Significant temperature level
                6 Significant humidity level
                7 Significant wind level
                8 Beginning of missing temperature data
                9 End of missing temperature data
                10 Beginning of missing humidity data
                11 End of missing humidity data
                12 Beginning of missing wind data
                13 End of missing wind data
                14 the top ow wind sounding
                15 a level determined by regional decision
                18 all bits set to 1 indicate a missing value
                all bits set to 0 indicate a level determined by national decisionor a level of
                no significance that has been included when high resolution data are reported
              */
              row["bitset"] = sBitset.to_string();
              sBitset >>= 11;
              if (sBitset.any())
                row["selectedLevel"] = 1;
            }

            if (!pressureParameterName.empty())
            {
              CTPP::CDT& rowData = row["data"][paramcount - 1];
              rowData["value"] = dataContainer->castTo<double>(dataPressureIt);
            }
          }

          CTPP::CDT& rowData = row["data"][paramcount + (pressureParameterName.empty() ? -1 : 0)];
          if (showValue)
          {
            auto value = dataContainer->toString(dataValueIt, 1);
            rowData["value"] = (value.empty() ? missingValue : value);
          }
          if (showDataQuality)
          {
            auto quality = dataContainer->toString(dataQualityIt, 0);
            rowData["quality"] = (quality.empty() ? missingValue : quality);
          }
          dataId++;
        }
      }

      if (debugLevel > 2)
      {
        boost::posix_time::ptime xmlEndTime = get_plugin_impl().get_time_stamp();
        std::cerr << "Hash population time delta: " << (xmlEndTime - queryEndTime).total_seconds()
                  << " seconds\n";
      }
    }

    hash["responseTimestamp"] =
        pt::to_iso_extended_string(get_plugin_impl().get_time_stamp()) + "Z";
    hash["fmi_apikey"] = QueryBase::FMI_APIKEY_SUBST;
    hash["fmi_apikey_prefix"] = QueryBase::FMI_APIKEY_PREFIX_SUBST;
    hash["hostname"] = QueryBase::HOSTNAME_SUBST;
    hash["protocol"] = QueryBase::PROTOCOL_SUBST;
    hash["language"] = language;
    hash["numberMatched"] = numberMatched;
    hash["numberReturned"] = numberMatched;

    boost::posix_time::ptime outputFormatTime = get_plugin_impl().get_time_stamp();
    format_output(hash, output, query.get_use_debug_format());

    if (debugLevel > 2)
    {
      boost::posix_time::ptime outputFormatTimeEnd = get_plugin_impl().get_time_stamp();
      std::cerr << "Output XML format time delta: "
                << (outputFormatTimeEnd - outputFormatTime).total_seconds() << " seconds\n"
                << "Total time delta: " << (outputFormatTimeEnd - queryStartTime).total_seconds()
                << " seconds\n";
    }
  }
  catch (const SmartMet::Spine::Exception& e)
  {
    SmartMet::Spine::Exception exception(e);
    exception.addParameter(WFS_LANGUAGE, language);
    throw exception;
  }
}

void StoredSoundingQueryHandler::update_parameters(
    const RequestParameterMap& params,
    int seqId,
    std::vector<boost::shared_ptr<RequestParameterMap> >& result) const
{
}

const std::shared_ptr<SmartMet::Engine::Observation::DBRegistryConfig>
StoredSoundingQueryHandler::dbRegistryConfig(const std::string& configName) const
{
  const std::shared_ptr<SmartMet::Engine::Observation::DBRegistry> dbRegistry =
      mObservation->dbRegistry();
  if (not dbRegistry)
  {
    std::ostringstream msg;
    msg << "Database registry is not available!";
    SmartMet::Spine::Exception exception(BCP, "Operation processing failed");
    exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PROCESSING_FAILED);
    exception.addDetail(msg.str());
    throw exception;
  }

  const std::shared_ptr<SmartMet::Engine::Observation::DBRegistryConfig> dbrConfig =
      dbRegistry->dbRegistryConfig(configName);
  if (not dbrConfig)
  {
    std::ostringstream msg;
    msg << "Database registry configuration '" << configName << "' is not available!";
    SmartMet::Spine::Exception exception(BCP, "Operation processing failed");
    exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PROCESSING_FAILED);
    exception.addDetail(msg.str());
    throw exception;
  }

  return dbrConfig;
}

std::string StoredSoundingQueryHandler::solveCrs(const RequestParameterMap& params) const
{
  // Output CRS priority: user defined -> default in stored query -> feature type default crs
  const std::string requestedCrs = params.get_single<std::string>(P_CRS);
  return (requestedCrs.empty() ? DATA_CRS_NAME : requestedCrs);
}

void StoredSoundingQueryHandler::validateAndPopulateMeteoParametersToMap(
    const RequestParameterMap& params,
    MeteoParameterMap& meteoParameterMap,
    std::string& pressureParameterName) const
{
  // Meteo parameters
  std::vector<ParamName> meteoParametersVector;
  params.get<ParamName>(P_METEO_PARAMETERS, std::back_inserter(meteoParametersVector));
  if (meteoParametersVector.empty())
  {
    std::ostringstream msg;
    msg << "At least one meteo parameter has to be given.";
    SmartMet::Spine::Exception exception(BCP, "Operation processing failed");
    exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PROCESSING_FAILED);
    exception.addDetail(msg.str());
    throw exception;
  }

  // Do not allow QC parameters if it is not enabled in a stored query.
  auto qcParamNameRef = SupportsQualityParameters::firstQCParameter(meteoParametersVector);
  if (not mSupportQCParameters && qcParamNameRef != meteoParametersVector.end())
  {
    std::ostringstream msg;
    msg << "Quality code parameter '" << *qcParamNameRef << "' is not allowed in this query.";
    SmartMet::Spine::Exception exception(BCP, "Invalid parameter value");
    exception.addParameter(WFS_EXCEPTION_CODE, WFS_INVALID_PARAMETER_VALUE);
    exception.addDetail(msg.str());
    throw exception;
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

  auto stationtype = params.get_single<std::string>(P_STATION_TYPE);
  for (auto& name : meteoParametersVector)
  {
    const uint64_t paramId = mObservation->getParameterId(name, stationtype);
    if (paramId == 0)
    {
      std::string paramIdStr = mObservation->getParameterIdAsString(name, stationtype);
      if (paramIdStr == "pressure")
      {
        pressureParameterName = name;
        continue;
      }
      std::ostringstream msg;
      msg << "Unknown parameter '" << name << "' in this query.";
      SmartMet::Spine::Exception exception(BCP, "Invalid parameter value");
      exception.addParameter(WFS_EXCEPTION_CODE, WFS_INVALID_PARAMETER_VALUE);
      exception.addDetail(msg.str());
      throw exception;
    }

    bool isQCParameter = SupportsQualityParameters::isQCParameter(name);

    // Is the parameter a duplicate.
    const ParamIdStr paramIdStr = Fmi::to_string(paramId);
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
        SmartMet::Spine::Exception exception(BCP, "Invalid parameter value");
        exception.addParameter(WFS_EXCEPTION_CODE, WFS_INVALID_PARAMETER_VALUE);
        exception.addDetail(msg.str());
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
}

void StoredSoundingQueryHandler::parseSoundingQuery(const RequestParameterMap& params,
                                                    const QueryResultShared& soundingQueryResult,
                                                    RadioSoundingMap& radioSoundingMap) const
{
  // At least one id row is required for data search.
  if (not soundingQueryResult or (soundingQueryResult->size("STATION_ID") == 0))
    return;

  using LatestSet = std::set<int>;
  LatestSet latestSet;

  ValueVectorConstIt soundingIdItBegin = soundingQueryResult->begin("SOUNDING_ID");
  ValueVectorConstIt soundingIdItEnd = soundingQueryResult->end("SOUNDING_ID");
  ValueVectorConstIt soundingIdIt = soundingIdItBegin;
  ValueVectorConstIt stationIdIt = soundingQueryResult->begin("STATION_ID");
  ValueVectorConstIt messageTimeIt = soundingQueryResult->begin("MESSAGE_TIME");
  ValueVectorConstIt launchTimeIt = soundingQueryResult->begin("LAUNCH_TIME");
  ValueVectorConstIt soundingEndIt = soundingQueryResult->begin("SOUNDING_END");
  std::string stationId, prevStationId;

  const bool latest = params.get_single<bool>(P_LATEST);
  for (; soundingIdIt != soundingIdItEnd;
       ++soundingIdIt, ++stationIdIt, ++messageTimeIt, ++launchTimeIt, ++soundingEndIt)
  {
    const int32_t tmpstationId = soundingQueryResult->castTo<int32_t>(stationIdIt);

    // The messages are arranged to descending order using MESSAGE_TIME.
    // If the value of latest is true, we pick only the first radiosounding.
    if ((not latest) or (latest and latestSet.find(tmpstationId) == latestSet.end()))
    {
      const int32_t soundingId = soundingQueryResult->castTo<int32_t>(soundingIdIt);
      RadioSounding rSounding;
      rSounding.mStationId = stationIdIt;
      rSounding.mMessageTime = messageTimeIt;
      rSounding.mLaunchTime = launchTimeIt;
      rSounding.mSoundingEnd = soundingEndIt;
      latestSet.insert(tmpstationId);
      radioSoundingMap.emplace(soundingId, rSounding);
    }
  }
}

void StoredSoundingQueryHandler::makeSoundingQuery(const RequestParameterMap& params,
                                                   const SmartMet::Spine::Stations& stations,
                                                   QueryResultShared& soundingQueryResult) const
{
  SmartMet::Engine::Observation::MastQueryParams profileQueryParams(
      dbRegistryConfig("RADIOSOUNDINGS_V1"));
  profileQueryParams.addField("STATION_ID");
  profileQueryParams.addField("SOUNDING_ID");
  profileQueryParams.addField("MESSAGE_TIME");
  profileQueryParams.addField("LAUNCH_TIME");
  profileQueryParams.addField("SOUNDING_END");

  // Station identities
  for (auto& station : stations)
  {
    profileQueryParams.addOperation(
        "OR_GROUP_station_id", "STATION_ID", "PropertyIsEqualTo", station.fmisid);
  }

  const uint64_t soundingType = params.get_single<uint64_t>(P_SOUNDING_TYPE);
  if (soundingType == 1)
  {
    std::ostringstream msg;
    msg << "Sounding type '" << soundingType << "' is not supported.";
    SmartMet::Spine::Exception exception(BCP, "Invalid parameter value");
    exception.addParameter(WFS_EXCEPTION_CODE, WFS_INVALID_PARAMETER_VALUE);
    exception.addDetail(msg.str());
    throw exception;
  }

  profileQueryParams.addOperation(
      "OR_GROUP_sounding_type", "SOUNDING_TYPE", "PropertyIsEqualTo", (long)soundingType);

  std::vector<uint64_t> publicityFlags;
  params.get<uint64_t>(P_PUBLICITY, std::back_inserter(publicityFlags));
  for (auto& publicity : publicityFlags)
  {
    profileQueryParams.addOperation(
        "OR_GROUP_publicity", "PUBLICITY", "PropertyIsEqualTo", publicity);
  }

  pt::ptime startTime = params.get_single<pt::ptime>(P_BEGIN_TIME);
  profileQueryParams.addOperation(
      "OR_GROUP_data_begin_time", "MESSAGE_TIME", "PropertyIsGreaterThanOrEqualTo", startTime);

  pt::ptime endTime = params.get_single<pt::ptime>(P_END_TIME);
  profileQueryParams.addOperation(
      "OR_GROUP_data_end_time", "MESSAGE_TIME", "PropertyIsLessThanOrEqualTo", endTime);

  profileQueryParams.addOrderBy("STATION_ID", "ASC");
  profileQueryParams.addOrderBy("MESSAGE_TIME", "DESC");

  // Execute query
  SmartMet::Engine::Observation::MastQuery profileQuery;
  profileQuery.setQueryParams(&profileQueryParams);
  mObservation->makeQuery(&profileQuery);

  soundingQueryResult = profileQuery.getQueryResultContainer();
}

void StoredSoundingQueryHandler::makeSoundingDataQuery(const RequestParameterMap& params,
                                                       const RadioSoundingMap& radioSoundingMap,
                                                       const MeteoParameterMap& meteoParameterMap,
                                                       QueryResultShared& dataContainer) const
{
  if (radioSoundingMap.empty() or meteoParameterMap.empty())
    return;

  SmartMet::Engine::Observation::MastQueryParams dataQueryParams(
      dbRegistryConfig("RADIOSOUNDING_LEVELS_V1"));
  std::list<SmartMet::Engine::Observation::MastQueryParams::NameType> levelJoinFields;
  levelJoinFields.push_back("SOUNDING_ID");
  levelJoinFields.push_back("LEVEL_NO");
  dataQueryParams.addJoinOnConfig(dbRegistryConfig("RADIOSOUNDING_DATA_V1"), levelJoinFields);
  dataQueryParams.addField("SOUNDING_ID");
  dataQueryParams.addField("MEASURAND_ID");
  dataQueryParams.addField("LEVEL_TIME");
  dataQueryParams.addField("ALTITUDE");
  dataQueryParams.addField("PRESSURE");
  dataQueryParams.addField("LATITUDE");
  dataQueryParams.addField("LONGITUDE");
  dataQueryParams.addField("DATA_VALUE");
  dataQueryParams.addField("DATA_QUALITY");
  dataQueryParams.addField("SIGNIFICANCE");
  dataQueryParams.addOrderBy("SOUNDING_ID", "ASC");
  dataQueryParams.addOrderBy("MEASURAND_ID", "ASC");
  dataQueryParams.addOrderBy("LEVEL_TIME", "ASC");

  for (auto& radioSounding : radioSoundingMap)
  {
    dataQueryParams.addOperation(
        "OR_GROUP_sounding_id", "SOUNDING_ID", "PropertyIsEqualTo", radioSounding.first);
  }

  // Measurand identities
  for (auto& item : meteoParameterMap)
  {
    dataQueryParams.addOperation("OR_GROUP_measurand_id",
                                 "MEASURAND_ID",
                                 "PropertyIsEqualTo",
                                 (long)std::get<0>(item.second));
  }

  // Handle altitude min-max pairs
  std::vector<double> altitudeRanges;
  params.get<double>(P_ALTITUDE_RANGES, std::back_inserter(altitudeRanges), 0, 2, 2);
  if (altitudeRanges.size() & 1)
  {
    SmartMet::Spine::Exception exception(BCP, "Invalid altitudeRange list!");
    exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PARSING_FAILED);
    throw exception;
  }
  for (std::size_t i = 0; i < altitudeRanges.size(); i += 2)
  {
    const double& low = altitudeRanges[i];
    const double& high = altitudeRanges[i + 1];
    dataQueryParams.addOperation(
        "OR_GROUP_altitude_low", "ALTITUDE", "PropertyIsGreaterThanOrEqualTo", low);
    dataQueryParams.addOperation(
        "OR_GROUP_altitude_high", "ALTITUDE", "PropertyIsLessThanOrEqualTo", high);
  }

  // Handle pressure max-min
  std::vector<double> pressureRanges;
  params.get<double>(P_PRESSURE_RANGES, std::back_inserter(pressureRanges), 0, 2, 2);
  if (pressureRanges.size() & 1)
  {
    SmartMet::Spine::Exception exception(BCP, "Invalid pressureRange list!");
    exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PARSING_FAILED);
    throw exception;
  }
  for (std::size_t i = 0; i < pressureRanges.size(); i += 2)
  {
    const double& low = pressureRanges[i];
    const double& high = pressureRanges[i + 1];
    dataQueryParams.addOperation(
        "OR_GROUP_pressure_low", "PRESSURE", "PropertyIsGreaterThanOrEqualTo", low);
    dataQueryParams.addOperation(
        "OR_GROUP_pressure_high", "PRESSURE", "PropertyIsLessThanOrEqualTo", high);
  }

  SmartMet::Engine::Observation::MastQuery dataQuery;
  dataQuery.setQueryParams(&dataQueryParams);
  mObservation->makeQuery(&dataQuery);
  dataContainer = dataQuery.getQueryResultContainer();
}

void StoredSoundingQueryHandler::getStationSearchSettings(
    SmartMet::Engine::Observation::Settings& settings,
    const RequestParameterMap& params,
    const std::string& language) const
{
  // Search and validate the locations.
  using LocationListItem = std::pair<std::string, SmartMet::Spine::LocationPtr>;
  using LocationList = std::list<LocationListItem>;
  LocationList locationsList;
  get_location_options(mGeonames, params, language, &locationsList);

  settings.allplaces = false;
  settings.numberofstations = params.get_single<uint64_t>(P_NUM_OF_STATIONS);
  settings.stationtype = params.get_single<std::string>(P_STATION_TYPE);
  settings.maxdistance = params.get_single<double>(P_MAX_DISTANCE);

  SmartMet::Spine::BoundingBox requestedBBox;
  bool haveBBox = get_bounding_box(params, solveCrs(params), &requestedBBox);
  if (haveBBox)
  {
    std::unique_ptr<SmartMet::Spine::BoundingBox> queryBBox;
    queryBBox.reset(new SmartMet::Spine::BoundingBox);
    *queryBBox = SupportsBoundingBox::transform_bounding_box(requestedBBox, DATA_CRS_NAME);
    settings.boundingBox["minx"] = queryBBox->xMin;
    settings.boundingBox["miny"] = queryBBox->yMin;
    settings.boundingBox["maxx"] = queryBBox->xMax;
    settings.boundingBox["maxy"] = queryBBox->yMax;
  }

  // Include valid locations.
  std::transform(locationsList.begin(),
                 locationsList.end(),
                 std::back_inserter(settings.locations),
                 boost::bind(&std::pair<std::string, SmartMet::Spine::LocationPtr>::second, ::_1));
}

void StoredSoundingQueryHandler::checkMaxSoundings(const pt::ptime startTime,
                                                   const pt::ptime& endTime,
                                                   const RadioSoundingMap& radioSoundingMap) const
{
  if (radioSoundingMap.size() > mMaxSoundings)
  {
    SmartMet::Spine::Exception exception(BCP,
                                         "Too many sounding observations in the time interval!");
    std::ostringstream msg;
    msg << "The time interval '" << Fmi::to_iso_extended_string(startTime).append("Z") << " - "
        << Fmi::to_iso_extended_string(endTime).append("Z") << "' contains '"
        << radioSoundingMap.size() << "' sounding observations (max " << mMaxSoundings << ").";
    exception.addDetail(msg.str());
    exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PROCESSING_FAILED);
    exception.addDetail("Use shorter time interval or reduce the number of locations.");
    throw exception;
  }
}
}  // namespace WFS
}  // namespace Plugin
}  // namespace SmartMet

namespace
{
using namespace SmartMet::Plugin::WFS;

boost::shared_ptr<SmartMet::Plugin::WFS::StoredQueryHandlerBase> wfsStoredSoundingHandlerCreate(
    SmartMet::Spine::Reactor* reactor,
    boost::shared_ptr<StoredQueryConfig> config,
    PluginImpl& pluginData,
    boost::optional<std::string> templateFileName)
{
  try
  {
    StoredSoundingQueryHandler* qh =
        new StoredSoundingQueryHandler(reactor, config, pluginData, templateFileName);
    boost::shared_ptr<SmartMet::Plugin::WFS::StoredQueryHandlerBase> instance(qh);
    instance->init_handler();
    return instance;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}
}  // namespace

SmartMet::Plugin::WFS::StoredQueryHandlerFactoryDef wfs_stored_sounding_handler_factory(
    &wfsStoredSoundingHandlerCreate);

#endif  // WITHOUT_OBSERVATION
