#include "StoredContourHandlerBase.h"
#include <gis/Box.h>
#include <grid-content/queryServer/definition/QueryConfigurator.h>
#include <grid-files/common/GraphFunctions.h>
#include <grid-files/common/ImageFunctions.h>
#include <grid-files/common/ImagePaint.h>
#include <macgyver/StringConversion.h>
#include <macgyver/TimeParser.h>
#include <newbase/NFmiEnumConverter.h>

#include <boost/algorithm/string/replace.hpp>
#include <boost/format.hpp>
#include <iomanip>

namespace bw = SmartMet::Plugin::WFS;

namespace
{
const char* P_GRID = "grid";
const char* P_LIMITS = "limits";
const char* P_PRODUCER = "producer";
const char* P_ORIGIN_TIME = "originTime";
const char* P_CRS = "crs";
const char* P_SMOOTHING = "smoothing";
const char* P_SMOOTHING_DEGREE = "smoothing_degree";
const char* P_SMOOTHING_SIZE = "smoothing_size";
const char* P_IMAGE_DIR = "imageDir";
const char* P_IMAGE_FILE = "imageFile";
}  // anonymous namespace

bw::StoredContourQueryHandler::StoredContourQueryHandler(
    SmartMet::Spine::Reactor* reactor,
    bw::StoredQueryConfig::Ptr config,
    PluginImpl& plugin_data,
    boost::optional<std::string> template_file_name)

    : StoredQueryParamRegistry(config),
      SupportsExtraHandlerParams(config, false),
      RequiresGridEngine(reactor),
      RequiresContourEngine(reactor),
      RequiresQEngine(reactor),
      RequiresGeoEngine(reactor),
      bw::StoredQueryHandlerBase(reactor, config, plugin_data, template_file_name),
      bw::SupportsBoundingBox(config, plugin_data.get_crs_registry(), false),
      bw::SupportsTimeParameters(config),
      bw::SupportsTimeZone(reactor, config)
{
  try
  {
    register_scalar_param<std::string>(P_PRODUCER);
    register_scalar_param<boost::posix_time::ptime>(P_ORIGIN_TIME, false);
    register_scalar_param<std::string>(P_CRS);

    if (config->find_setting(config->get_root(), "handler_params.imageDir", false))
      register_scalar_param<std::string>(P_IMAGE_DIR);
    if (config->find_setting(config->get_root(), "handler_params.imageFile", false))
      register_scalar_param<std::string>(P_IMAGE_FILE);

    if (config->find_setting(config->get_root(), "handler_params.limits", false))
      register_array_param<double>(P_LIMITS, 0, 999, 2);
    if (config->find_setting(config->get_root(), "handler_params.smoothing", false))
      register_scalar_param<bool>(P_SMOOTHING);
    if (config->find_setting(config->get_root(), "handler_params.smoothing_degree", false))
      register_scalar_param<uint64_t>(P_SMOOTHING_DEGREE);
    if (config->find_setting(config->get_root(), "handler_params.smoothing_size", false))
      register_scalar_param<uint64_t>(P_SMOOTHING_SIZE);

    // read contour parameters from config and check validity
    name = config->get_mandatory_config_param<std::string>("contour_param.name");
    uint64_t cpid = config->get_mandatory_config_param<uint64_t>("contour_param.id");

    // check parameter id
    NFmiEnumConverter ec;
    std::string pname = ec.ToString(cpid);
    if (pname.empty())
    {
      Fmi::Exception exception(
          BCP, "Invalid contour parameter id '" + std::to_string(cpid) + "'!");
      exception.addParameter(WFS_EXCEPTION_CODE, WFS_INVALID_PARAMETER_VALUE);
      throw exception;
    }
    id = static_cast<FmiParameterName>(cpid);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

bw::StoredContourQueryHandler::~StoredContourQueryHandler() {}

bw::ContourQueryResultSet bw::StoredContourQueryHandler::getContours(
    const ContourQueryParameter& queryParameter) const
{
  try
  {
    const std::string& data_source = get_data_source();
    bool gridengine_disabled = is_gridengine_disabled();

    if (!gridengine_disabled && data_source == P_GRID &&
        queryParameter.gridQuery.mQueryParameterList.size() > 0)
    {
      return getContours_gridEngine(queryParameter);
    }
    else
    {
      return getContours_qEngine(queryParameter);
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

bw::ContourQueryResultSet bw::StoredContourQueryHandler::getContours_qEngine(
    const ContourQueryParameter& queryParameter) const
{
  try
  {
    ContourQueryResultSet ret;

    for (auto& timestep : queryParameter.tlist)
    {
      boost::posix_time::ptime utctime(timestep.utc_time());

      SmartMet::Engine::Contour::Options options(getContourEngineOptions(utctime, queryParameter));

      if (queryParameter.smoothing)
      {
        options.filter_degree = queryParameter.smoothing_degree;
        options.filter_size = queryParameter.smoothing_size;
      }

      std::vector<OGRGeometryPtr> geoms;

      try
      {
        std::size_t qhash = SmartMet::Engine::Querydata::hash_value(queryParameter.q);
        auto valueshash = qhash;
        boost::hash_combine(valueshash, options.data_hash_value());
        std::string wkt = queryParameter.q->area().WKT();

        // Select the data

        if (!queryParameter.q->param(options.parameter.number()))
        {
          throw Fmi::Exception(
              BCP, "Parameter '" + options.parameter.name() + "' unavailable.");
        }

        if (!queryParameter.q->firstLevel())
        {
          throw Fmi::Exception(BCP, "Unable to set first level in querydata.");
        }

        // Select the level.
        if (options.level)
        {
          if (!queryParameter.q->selectLevel(*options.level))
          {
            throw Fmi::Exception(
                BCP, "Level value " + Fmi::to_string(*options.level) + " is not available.");
          }
        }

        auto matrix = q_engine->getValues(queryParameter.q, valueshash, options.time);
        CoordinatesPtr coords =
            q_engine->getWorldCoordinates(queryParameter.q, &queryParameter.sr);

        geoms = contour_engine->contour(qhash,
                                        wkt,
                                        *matrix,
                                        coords,
                                        options,
                                        queryParameter.q->needsWraparound(),
                                        &queryParameter.sr);
      }
      catch (const std::exception& e)
      {
        continue;
      }
#ifdef MYDEBUG
      if (pGeom->getGeometryType() == wkbMultiPolygon)
      {
        std::cout << "MULTIPOLYGON" << std::endl;
      }
      else if (pGeom->getGeometryType() == wkbPolygon)
      {
        std::cout << "POLYGON" << std::endl;
      }
      else if (pGeom->getGeometryType() == wkbLinearRing)
      {
        std::cout << "LINEAR RING" << std::endl;
      }
      else if (pGeom->getGeometryType() == wkbLineString)
      {
        std::cout << "LINE STRING" << std::endl;
      }
      else if (pGeom->getGeometryType() == wkbMultiLineString)
      {
        std::cout << "MULTILINE STRING" << std::endl;
      }
      else if (pGeom->getGeometryType() == wkbGeometryCollection)
      {
        std::cout << "GEOMETRY COLLECTION" << std::endl;
      }
      else
      {
        std::cout << "Unknown geometry: " << pGeom->getGeometryType() << std::endl;
      }
#endif

      // if no geometry just continue
      if (geoms.empty())
        continue;

      // clip the geometry into bounding box
      Fmi::Box bbox(queryParameter.bbox.xMin,
                    queryParameter.bbox.yMin,
                    queryParameter.bbox.xMax,
                    queryParameter.bbox.yMax,
                    0,
                    0);

      ContourQueryResultPtr cgr(new ContourQueryResult());
      for (auto geom : geoms)
      {
        clipGeometry(geom, bbox);
        cgr->area_geoms.push_back(WeatherAreaGeometry(utctime, geom));
      }
      ret.push_back(cgr);
    }
    return ret;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

bw::ContourQueryResultSet bw::StoredContourQueryHandler::getContours_gridEngine(
    const ContourQueryParameter& queryParameter) const
{
  try
  {
    std::shared_ptr<ContentServer::ServiceInterface> contentServer =
        grid_engine->getContentServer_sptr();
    Spine::TimeSeries::Value missing_value = Spine::TimeSeries::None();
    ContourQueryResultSet ret;

    const char* urnStr = queryParameter.gridQuery.mAttributeList.getAttributeValue("grid.urn");

    std::string targetURN("urn:ogc:def:crs:EPSG:4326");
    if (urnStr != nullptr)
      targetURN = urnStr;

    OGRSpatialReference sr;
    if (sr.importFromURN(targetURN.c_str()) != OGRERR_NONE)
    {
      Fmi::Exception exception(BCP, "Invalid crs '" + targetURN + "'!");
      exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PARSING_FAILED);
      throw exception;
    }

    // QueryServer::Query query(queryParameter.gridQuery);

    boost::posix_time::ptime utctime;
    SmartMet::Engine::Contour::Options options(getContourEngineOptions(utctime, queryParameter));

    for (auto param = queryParameter.gridQuery.mQueryParameterList.begin();
         param != queryParameter.gridQuery.mQueryParameterList.end();
         ++param)
    {
      if (options.isovalues.size() > 0)
      {
        param->mType = QueryServer::QueryParameter::Type::Isoline;
        param->mLocationType = QueryServer::QueryParameter::LocationType::Geometry;

        for (auto v = options.isovalues.begin(); v != options.isovalues.end(); ++v)
          param->mContourLowValues.push_back(*v);
      }
      else
      {
        param->mType = QueryServer::QueryParameter::Type::Isoband;
        param->mLocationType = QueryServer::QueryParameter::LocationType::Geometry;

        for (auto v = options.limits.begin(); v != options.limits.end(); ++v)
        {
          if (v->lolimit && v->hilimit)
          {
            param->mContourLowValues.push_back(*(v->lolimit));
            param->mContourHighValues.push_back(*(v->hilimit));
          }
        }
      }
    }

    queryParameter.gridQuery.mAttributeList.setAttribute(
        "contour.coordinateType", std::to_string(T::CoordinateTypeValue::LATLON_COORDINATES));

    int result = grid_engine->executeQuery(queryParameter.gridQuery);
    if (result != 0)
    {
      Fmi::Exception exception(BCP, "The query server returns an error message!");
      exception.addParameter("Result", std::to_string(result));
      exception.addParameter("Message", QueryServer::getResultString(result));

      switch (result)
      {
        case QueryServer::Result::NO_PRODUCERS_FOUND:
          exception.addDetail(
              "The reason for this situation is usually that the given producer is unknown");
          exception.addDetail(
              "or there are no producer list available in the grid engine's configuration file.");
          break;
      }
      throw exception;
    }

    T::Attribute* imageDir = queryParameter.gridQuery.mAttributeList.getAttribute("contour.imageDir");
    T::Attribute* imageFile = queryParameter.gridQuery.mAttributeList.getAttribute("contour.imageFile");

    for (auto param = queryParameter.gridQuery.mQueryParameterList.begin();
         param != queryParameter.gridQuery.mQueryParameterList.end();
         ++param)
    {
      for (auto val = param->mValueList.begin(); val != param->mValueList.end(); ++val)
      {
        std::vector<OGRGeometryPtr> geoms;
        boost::posix_time::ptime utcTime = Fmi::TimeParser::parse_iso((*val)->mForecastTime);

        if ((*val)->mValueData.size() > 0)
        {
          for (auto wkb = (*val)->mValueData.begin(); wkb != (*val)->mValueData.end(); ++wkb)
          {
            unsigned char* cwkb = reinterpret_cast<unsigned char*>(wkb->data());

            OGRGeometry* geom = nullptr;
            /*OGRErr err =*/OGRGeometryFactory::createFromWkb(cwkb, nullptr, &geom, wkb->size());
            if (geom != nullptr)
            {
              auto geomPtr = OGRGeometryPtr(geom);
              geoms.push_back(geomPtr);
            }
          }

          if (imageDir != nullptr && imageDir->mValue > " " && imageFile != nullptr && imageFile->mValue > " ")
          {
            // Contours can be painted and saved as images. This functionality
            // is usually used for debugging purposes.

            int width = 3600;   // atoi(query.mAttributeList.getAttributeValue("grid.width"));
            int height = 1800;  // atoi(query.mAttributeList.getAttributeValue("grid.height"));

            if (width > 0 && height > 0)
            {
              double mp = 10;
              ImagePaint imagePaint(width, height, 0xFFFFFF, false, true);

              // ### Painting contours into the image:

              if (param->mType == QueryServer::QueryParameter::Type::Isoband)
              {
                if ((*val)->mValueData.size() > 0)
                {
                  uint c = 250;
                  uint step = 250 / (*val)->mValueData.size();

                  for (auto it = (*val)->mValueData.begin(); it != (*val)->mValueData.end(); ++it)
                  {
                    uint col = (c << 16) + (c << 8) + c;
                    imagePaint.paintWkb(mp, mp, 180, 90, *it, col);
                    c = c - step;
                  }
                }
              }
              else
              {
                imagePaint.paintWkb(mp, mp, 180, 90, (*val)->mValueData, 0x00);
              }

              // ### Saving the image and releasing the image data:


              std::string filename = imageDir->mValue + "/" + imageFile->mValue;

              imagePaint.saveJpgImage(filename.c_str());
            }
          }
        }

        if (geoms.size() > 0)
        {
          // clip the geometry into bounding box
          Fmi::Box bbox(queryParameter.bbox.xMin,
                        queryParameter.bbox.yMin,
                        queryParameter.bbox.xMax,
                        queryParameter.bbox.yMax,
                        0,
                        0);

          ContourQueryResultPtr cgr(new ContourQueryResult());
          for (auto geom : geoms)
          {
            clipGeometry(geom, bbox);
            cgr->area_geoms.push_back(WeatherAreaGeometry(utcTime, geom));
          }
          ret.push_back(cgr);
        }
      }
    }

    return ret;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void bw::StoredContourQueryHandler::parsePolygon(OGRPolygon* polygon,
                                                 bool latLonOrder,
                                                 unsigned int precision,
                                                 CTPP::CDT& polygon_patch) const
{
  try
  {
    OGRGeometry* pExteriorRing(polygon->getExteriorRing());

    pExteriorRing->assignSpatialReference(polygon->getSpatialReference());

    polygon_patch["exterior_ring"] = formatCoordinates(pExteriorRing, latLonOrder, precision);
    polygon_patch["interior_rings"] = CTPP::CDT(CTPP::CDT::ARRAY_VAL);

    int num_interior_rings = polygon->getNumInteriorRings();

    // iterate interior rings of polygon
    for (int k = 0; k < num_interior_rings; k++)
    {
      OGRGeometry* pInteriorRing(polygon->getInteriorRing(k));

      pInteriorRing->assignSpatialReference(polygon->getSpatialReference());
      polygon_patch["interior_rings"][k] = formatCoordinates(pInteriorRing, latLonOrder, precision);
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void bw::StoredContourQueryHandler::query(const StoredQuery& stored_query,
                                          const std::string& language,
					  const boost::optional<std::string>& hostname,
                                          std::ostream& output) const
{
  try
  {
    const auto& sq_params = stored_query.get_param_map();
    std::string producerName = sq_params.get_single<std::string>(P_PRODUCER);
    std::string dataSource = get_plugin_impl().get_data_source();
    bool gridengine_disabled = get_plugin_impl().is_gridengine_disabled();

    if (!gridengine_disabled && dataSource == P_GRID && grid_engine->isGridProducer(producerName))
    {
      query_gridEngine(stored_query, language, output);
    }
    else
    {
      query_qEngine(stored_query, language, output);
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void bw::StoredContourQueryHandler::query_qEngine(const StoredQuery& stored_query,
                                                  const std::string& language,
                                                  std::ostream& output) const
{
  try
  {
    // 1) query geometries from countour engine; iterate timesteps
    // 2) if bounding box given, clip the polygons
    // 3) generate output xml

    const auto& sq_params = stored_query.get_param_map();

    std::string requestedCRS = sq_params.get_optional<std::string>(P_CRS, "EPSG::4326");

    std::string targetURN("urn:ogc:def:crs:" + requestedCRS);
    OGRSpatialReference sr;
    if (sr.importFromURN(targetURN.c_str()) != OGRERR_NONE)
    {
      Fmi::Exception exception(BCP, "Invalid crs '" + requestedCRS + "'!");
      exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PARSING_FAILED);
      throw exception;
    }

    SmartMet::Engine::Querydata::Producer producer = sq_params.get_single<std::string>(P_PRODUCER);
    boost::optional<boost::posix_time::ptime> requested_origintime =
        sq_params.get_optional<boost::posix_time::ptime>(P_ORIGIN_TIME);

    SmartMet::Engine::Querydata::Q q;
    if (requested_origintime)
      q = q_engine->get(producer, *requested_origintime);
    else
      q = q_engine->get(producer);

    boost::posix_time::ptime origintime = q->originTime();
    boost::posix_time::ptime modificationtime = q->modificationTime();

    SmartMet::Spine::Parameter parameter(name, SmartMet::Spine::Parameter::Type::Data, id);

    boost::shared_ptr<ContourQueryParameter> query_param = getQueryParameter(parameter, q, sr);
    CoverageQueryParameter* qParam = dynamic_cast<CoverageQueryParameter*>(query_param.get());
    if (qParam)
    {
      std::vector<double> limits;
      sq_params.get<double>(P_LIMITS, std::back_inserter(limits), 0, 998, 2);
      if (limits.size() & 1)
      {
        Fmi::Exception exception(BCP, "Invalid list of doubles in parameter 'limits'!");
        exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PARSING_FAILED);
        throw exception;
      }
      for (std::size_t i = 0; i < limits.size(); i += 2)
      {
        const double& lower = limits[i];
        const double& upper = limits[i + 1];
        qParam->limits.push_back(SmartMet::Engine::Contour::Range(lower, upper));
      }
    }

    query_param->smoothing = sq_params.get_optional<bool>(P_SMOOTHING, false);
    query_param->smoothing_degree = sq_params.get_optional<uint64_t>(P_SMOOTHING_DEGREE, 2);
    query_param->smoothing_size = sq_params.get_optional<uint64_t>(P_SMOOTHING_SIZE, 2);

    boost::shared_ptr<SmartMet::Spine::TimeSeriesGeneratorOptions> pTimeOptions =
        get_time_generator_options(sq_params);

    // get data in UTC
    const std::string zone = "UTC";
    auto tz = geo_engine->getTimeZones().time_zone_from_string(zone);
    query_param->tlist = SmartMet::Spine::TimeSeriesGenerator::generate(*pTimeOptions, tz);
    query_param->tz_name = get_tz_name(sq_params);

    CTPP::CDT hash;

    get_bounding_box(sq_params, requestedCRS, &(query_param->bbox));

    // if requested CRS and bounding box CRS are different, do transformation
    if (requestedCRS.compare(query_param->bbox.crs) != 0)
      query_param->bbox = transform_bounding_box(query_param->bbox, requestedCRS);

    std::vector<ContourQueryResultPtr> query_results(processQuery(*query_param));

    SmartMet::Spine::CRSRegistry& crsRegistry = plugin_impl.get_crs_registry();

    parseQueryResults(query_results,
                      query_param->bbox,
                      language,
                      crsRegistry,
                      requestedCRS,
                      origintime,
                      modificationtime,
                      query_param->tz_name,
                      hash);

    // Format output
    format_output(hash, output, stored_query.get_use_debug_format());
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void bw::StoredContourQueryHandler::query_gridEngine(const StoredQuery& stored_query,
                                                     const std::string& language,
                                                     std::ostream& output) const
{
  try
  {
    QueryServer::QueryConfigurator queryConfigurator;
    T::AttributeList attributeList;

    const auto& sq_params = stored_query.get_param_map();

    std::string requestedCRS = sq_params.get_optional<std::string>(P_CRS, "EPSG::4326");

    std::string targetURN("urn:ogc:def:crs:" + requestedCRS);
    OGRSpatialReference sr;
    if (sr.importFromURN(targetURN.c_str()) != OGRERR_NONE)
    {
      Fmi::Exception exception(BCP, "Invalid crs '" + requestedCRS + "'!");
      exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PARSING_FAILED);
      throw exception;
    }
    sr.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

    std::string producer = sq_params.get_single<std::string>(P_PRODUCER);

    boost::optional<boost::posix_time::ptime> requested_origintime =
        sq_params.get_optional<boost::posix_time::ptime>(P_ORIGIN_TIME);
    if (requested_origintime)
    {
      std::string originTime = to_iso_string(*requested_origintime);
      attributeList.addAttribute("originTime", originTime);
    }

    Engine::Grid::ParameterDetails_vec parameters;

    std::string pName = name;
    std::string interpolationMethod = "";
    auto pos = pName.find(".raw");
    if (pos != std::string::npos)
    {
      attributeList.addAttribute("areaInterpolationMethod",std::to_string(T::AreaInterpolationMethod::Linear));
      pName.erase(pos,4);
    }

    std::string key = producer + ";" + pName;

    grid_engine->getParameterDetails(producer, pName, parameters);

    std::string prod;
    std::string geomId;
    std::string level;
    std::string levelId;
    std::string forecastType;
    std::string forecastNumber;

    size_t len = parameters.size();
    if (len > 0 && strcasecmp(parameters[0].mProducerName.c_str(), key.c_str()) != 0)
    {
      for (size_t t = 0; t < len; t++)
      {
        if (parameters[t].mLevelId > "")
          levelId = parameters[t].mLevelId;

        if (parameters[t].mLevel > "")
          level = parameters[t].mLevel;

        if (parameters[t].mForecastType > "")
          forecastType = parameters[t].mForecastType;

        if (parameters[t].mForecastNumber > "")
          forecastNumber = parameters[t].mForecastNumber;

        if (parameters[t].mGeometryId > "")
        {
          prod = parameters[t].mProducerName;
          geomId = parameters[t].mGeometryId;
        }
      }
      std::string param = name + ":" + prod + ":" + geomId + ":" + levelId + ":" + level + ":" +
                          forecastType + ":" + forecastNumber;
      attributeList.addAttribute("param", param);
    }
    else
    {
      attributeList.addAttribute("producer", producer);
      attributeList.addAttribute("param", name);
    }

    boost::shared_ptr<SmartMet::Spine::TimeSeriesGeneratorOptions> tOptions =
        get_time_generator_options(sq_params);

    if (tOptions->startTimeData)
      attributeList.addAttribute("startTime", "data");
    else
      attributeList.addAttribute("startTime", Fmi::to_iso_string(tOptions->startTime));

    if (tOptions->endTimeData)
      attributeList.addAttribute("endTime", "data");
    else
      attributeList.addAttribute("endTime", Fmi::to_iso_string(tOptions->endTime));

    if (tOptions->timeSteps)
      attributeList.addAttribute("timesteps", std::to_string(*tOptions->timeSteps));

    if (tOptions->timeStep)
      attributeList.addAttribute("timestep", std::to_string(*tOptions->timeStep));

    attributeList.addAttribute("timezone", "UTC");

    SmartMet::Engine::Querydata::Q q;
    SmartMet::Spine::Parameter parameter(name, SmartMet::Spine::Parameter::Type::Data, id);
    boost::shared_ptr<ContourQueryParameter> query_param = getQueryParameter(parameter, q, sr);

    CoverageQueryParameter* qParam = dynamic_cast<CoverageQueryParameter*>(query_param.get());
    if (qParam)
    {
      std::vector<double> limits;
      sq_params.get<double>(P_LIMITS, std::back_inserter(limits), 0, 998, 2);
      if (limits.size() & 1)
      {
        Fmi::Exception exception(BCP, "Invalid list of doubles in parameter 'limits'!");
        exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PARSING_FAILED);
        throw exception;
      }
      for (std::size_t i = 0; i < limits.size(); i += 2)
      {
        const double& lower = limits[i];
        const double& upper = limits[i + 1];
        qParam->limits.push_back(SmartMet::Engine::Contour::Range(lower, upper));
      }
    }

    query_param->smoothing = sq_params.get_optional<bool>(P_SMOOTHING, false);
    query_param->smoothing_degree = sq_params.get_optional<uint64_t>(P_SMOOTHING_DEGREE, 2);
    query_param->smoothing_size = sq_params.get_optional<uint64_t>(P_SMOOTHING_SIZE, 2);

    boost::shared_ptr<SmartMet::Spine::TimeSeriesGeneratorOptions> pTimeOptions =
        get_time_generator_options(sq_params);
    const std::string zone = "UTC";
    auto tz = geo_engine->getTimeZones().time_zone_from_string(zone);
    query_param->tlist = SmartMet::Spine::TimeSeriesGenerator::generate(*pTimeOptions, tz);
    query_param->tz_name = get_tz_name(sq_params);
    get_bounding_box(sq_params, requestedCRS, &(query_param->bbox));

    // if requested CRS and bounding box CRS are different, do transformation
    if (requestedCRS.compare(query_param->bbox.crs) != 0)
      query_param->bbox = transform_bounding_box(query_param->bbox, requestedCRS);

    // attributeList.print(std::cout,0,0);
    queryConfigurator.configure(query_param->gridQuery, attributeList);

    auto smoothing = sq_params.get_optional<bool>(P_SMOOTHING, false);
    if (smoothing)
    {
      auto smoothing_degree = sq_params.get_optional<uint64_t>(P_SMOOTHING_DEGREE, 2);
      auto smoothing_size = sq_params.get_optional<uint64_t>(P_SMOOTHING_SIZE, 2);

      if (smoothing_degree)
        query_param->gridQuery.mAttributeList.addAttribute("contour.smoothingDegree",
                                                           std::to_string(smoothing_degree));

      if (smoothing_size)
        query_param->gridQuery.mAttributeList.addAttribute("contour.smoothingSize",
                                                           std::to_string(smoothing_size));
    }

    /*
        T::Coordinate_vec coordinates;
        coordinates.push_back(T::Coordinate(query_param->bbox.xMin,query_param->bbox.yMin));
        coordinates.push_back(T::Coordinate(query_param->bbox.xMax,query_param->bbox.yMax));
        query_param->gridQuery.mAreaCoordinates.push_back(coordinates);
    */

    char tmp[100];
    query_param->gridQuery.mAttributeList.addAttribute("grid.urn", targetURN);
    sprintf(tmp,
            "%f,%f,%f,%f",
            query_param->bbox.xMin,
            query_param->bbox.yMin,
            query_param->bbox.xMax,
            query_param->bbox.yMax);
    query_param->gridQuery.mAttributeList.addAttribute("grid.bbox", tmp);
    query_param->gridQuery.mAttributeList.addAttribute("grid.width", "1000");
    query_param->gridQuery.mAttributeList.addAttribute("grid.height", "1000");

    std::string imageFile = sq_params.get_optional<std::string>(P_IMAGE_FILE, "");
    if (imageFile > " ")
      query_param->gridQuery.mAttributeList.addAttribute("contour.imageFile", imageFile);

    std::string imageDir = sq_params.get_optional<std::string>(P_IMAGE_DIR, "");
    if (imageDir > " ")
      query_param->gridQuery.mAttributeList.addAttribute("contour.imageDir", imageDir);

    // query_param->gridQuery.print(std::cout,0,0);

    std::vector<ContourQueryResultPtr> query_results(processQuery(*query_param));

    SmartMet::Engine::Gis::CRSRegistry& crsRegistry = plugin_impl.get_crs_registry();

    std::string analysisTimeStr;
    std::string modificationTimeStr;

    for (auto param = query_param->gridQuery.mQueryParameterList.begin();
         param != query_param->gridQuery.mQueryParameterList.end();
         ++param)
    {
      for (auto val = param->mValueList.begin(); val != param->mValueList.end(); ++val)
      {
        if ((*val)->mAnalysisTime > " ")
        {
          analysisTimeStr = (*val)->mAnalysisTime;
          modificationTimeStr = (*val)->mModificationTime;
        }
      }
    }

    CTPP::CDT hash;
    boost::posix_time::ptime origintime;
    boost::posix_time::ptime modificationtime;

    if (analysisTimeStr > " ")
      origintime =
          boost::posix_time::ptime(Fmi::TimeParser::parse_iso(analysisTimeStr));

    if (modificationTimeStr > " ")
      modificationtime =
          boost::posix_time::ptime(Fmi::TimeParser::parse_iso(modificationTimeStr));

    parseQueryResults(query_results,
                      query_param->bbox,
                      language,
                      crsRegistry,
                      requestedCRS,
                      origintime,
                      modificationtime,
                      query_param->tz_name,
                      hash);

    // Format output
    format_output(hash, output, stored_query.get_use_debug_format());
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

std::string bw::StoredContourQueryHandler::double2string(double d, unsigned int precision) const
{
  switch (precision)
  {
    case 0:
      return Fmi::to_string("%.0f", d);
    case 1:
      return Fmi::to_string("%.1f", d);
    case 2:
      return Fmi::to_string("%.2f", d);
    case 3:
      return Fmi::to_string("%.3f", d);
    case 4:
      return Fmi::to_string("%.4f", d);
    case 5:
      return Fmi::to_string("%.5f", d);
    case 6:
      return Fmi::to_string("%.6f", d);
    case 7:
      return Fmi::to_string("%.7f", d);
    case 8:
      return Fmi::to_string("%.8f", d);
    case 9:
      return Fmi::to_string("%.9f", d);
    case 10:
      return Fmi::to_string("%.10f", d);
    case 11:
      return Fmi::to_string("%.11f", d);
    case 12:
      return Fmi::to_string("%.12f", d);
    case 13:
      return Fmi::to_string("%.13f", d);
    case 14:
      return Fmi::to_string("%.14f", d);
    case 15:
      return Fmi::to_string("%.15f", d);
    default:
      return Fmi::to_string("%.16f", d);
  }
}

std::string bw::StoredContourQueryHandler::bbox2string(const SmartMet::Spine::BoundingBox& bbox,
                                                       OGRSpatialReference& targetSRS) const
{
  unsigned int precision(targetSRS.IsGeographic() ? 6 : 1);

  if (targetSRS.EPSGTreatsAsLatLong())
  {
    return (double2string(bbox.yMin, precision) + ',' + double2string(bbox.xMin, precision) + ' ' +
            double2string(bbox.yMax, precision) + ',' + double2string(bbox.xMin, precision) + ' ' +
            double2string(bbox.yMax, precision) + ',' + double2string(bbox.xMax, precision) + ' ' +
            double2string(bbox.yMin, precision) + ',' + double2string(bbox.xMax, precision) + ' ' +
            double2string(bbox.yMin, precision) + ',' + double2string(bbox.xMin, precision));
  }

  return (double2string(bbox.xMin, precision) + ',' + double2string(bbox.yMin, precision) + ' ' +
          double2string(bbox.xMax, precision) + ',' + double2string(bbox.yMin, precision) + ' ' +
          double2string(bbox.xMax, precision) + ',' + double2string(bbox.yMax, precision) + ' ' +
          double2string(bbox.xMin, precision) + ',' + double2string(bbox.yMax, precision) + ' ' +
          double2string(bbox.xMin, precision) + ',' + double2string(bbox.yMin, precision));
}

std::string bw::StoredContourQueryHandler::formatCoordinates(const OGRGeometry* geom,
                                                             bool latLonOrder,
                                                             unsigned int precision) const
{
  std::string ret;
  const OGRLineString* lineString = reinterpret_cast<const OGRLineString*>(geom);

  int num_points = lineString->getNumPoints();
  OGRPoint point;
  for (int i = 0; i < num_points; i++)
  {
    lineString->getPoint(i, &point);
    if (!ret.empty())
      ret += ',';
    if (latLonOrder)
      ret += double2string(point.getY(), precision) + ' ' + double2string(point.getX(), precision);
    else
      ret += double2string(point.getX(), precision) + ' ' + double2string(point.getY(), precision);
  }

  return ret;
}

void bw::StoredContourQueryHandler::parseGeometry(OGRGeometry* geom,
                                                  bool latLonOrder,
                                                  unsigned int precision,
                                                  CTPP::CDT& result) const
{
  try
  {
    if (geom->getGeometryType() == wkbMultiPolygon)
    {
      result["surface_members"] = CTPP::CDT(CTPP::CDT::ARRAY_VAL);
      OGRMultiPolygon* pMultiPolygon = reinterpret_cast<OGRMultiPolygon*>(geom);
      int num_geometries = pMultiPolygon->getNumGeometries();

      // iterate polygons
      for (int i = 0; i < num_geometries; i++)
      {
        CTPP::CDT& polygon_patch = result["surface_members"][i];
        OGRPolygon* pPolygon = reinterpret_cast<OGRPolygon*>(pMultiPolygon->getGeometryRef(i));
        pPolygon->assignSpatialReference(pMultiPolygon->getSpatialReference());
        parsePolygon(pPolygon, latLonOrder, precision, polygon_patch);
      }
    }
    else if (geom->getGeometryType() == wkbPolygon)
    {
      result["surface_members"] = CTPP::CDT(CTPP::CDT::ARRAY_VAL);
      OGRPolygon* pPolygon = reinterpret_cast<OGRPolygon*>(geom);
      CTPP::CDT& polygon_patch = result["surface_members"][0];
      parsePolygon(pPolygon, latLonOrder, precision, polygon_patch);
    }
    else if (geom->getGeometryType() == wkbLinearRing)
    {
      result["surface_members"] = CTPP::CDT(CTPP::CDT::ARRAY_VAL);
      OGRLinearRing* pLinearRing = reinterpret_cast<OGRLinearRing*>(geom);
      OGRPolygon polygon;
      polygon.addRing(pLinearRing);
      CTPP::CDT& polygon_patch = result["surface_members"][0];
      parsePolygon(&polygon, latLonOrder, precision, polygon_patch);
    }
    else if (geom->getGeometryType() == wkbLineString)
    {
      result["isolines"] = CTPP::CDT(CTPP::CDT::ARRAY_VAL);
      OGRLineString* pLineString = reinterpret_cast<OGRLineString*>(geom);
      CTPP::CDT& linestring_patch = result["isolines"][0];
      linestring_patch["coordinates"] = formatCoordinates(pLineString, latLonOrder, precision);
    }
    else if (geom->getGeometryType() == wkbMultiLineString)
    {
      result["isolines"] = CTPP::CDT(CTPP::CDT::ARRAY_VAL);
      OGRMultiLineString* pMultiLineString = reinterpret_cast<OGRMultiLineString*>(geom);
      for (int i = 0; i < pMultiLineString->getNumGeometries(); i++)
      {
        CTPP::CDT& linestring_patch = result["isolines"][i];
        OGRLineString* pLineString =
            reinterpret_cast<OGRLineString*>(pMultiLineString->getGeometryRef(i));
        linestring_patch["coordinates"] = formatCoordinates(pLineString, latLonOrder, precision);
      }
    }
    else
    {
      std::cout << "Unknown geometry: " << geom->getGeometryType() << std::endl;
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void bw::StoredContourQueryHandler::handleGeometryCollection(
    OGRGeometryCollection* pGeometryCollection,
    bool latLonOrder,
    unsigned int precision,
    std::vector<CTPP::CDT>& results) const
{
  try
  {
    // dig out the geometries from OGRGeometryCollection
    for (int i = 0; i < pGeometryCollection->getNumGeometries(); i++)
    {
      OGRGeometry* geom = pGeometryCollection->getGeometryRef(i);
      // dont parse empty geometry
      if (geom->IsEmpty())
        continue;

      if (geom->getGeometryType() == wkbGeometryCollection)
        handleGeometryCollection(
            reinterpret_cast<OGRGeometryCollection*>(geom), latLonOrder, precision, results);
      else
      {
        CTPP::CDT result;
        parseGeometry(geom, latLonOrder, precision, result);
        results.push_back(result);
      }
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void bw::StoredContourQueryHandler::parseQueryResults(
    const std::vector<ContourQueryResultPtr>& query_results,
    const SmartMet::Spine::BoundingBox& bbox,
    const std::string& language,
    SmartMet::Spine::CRSRegistry& crsRegistry,
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
    targetSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

    // for geographic projection precision is 6 digits, for other projections 1 digit
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
    plugin_impl.get_crs_registry().get_attribute(requestedCRS, "projUri", &proj_uri);

    boost::local_time::time_zone_ptr tzp = get_time_zone(tz_name);
    std::string runtime_timestamp = format_local_time(plugin_impl.get_time_stamp(), tzp);
    std::string origintime_timestamp = format_local_time(origintime, tzp);
    std::string modificationtime_timestamp = format_local_time(modificationtime, tzp);

    hash["proj_uri"] = proj_uri;
    hash["fmi_apikey"] = bw::QueryBase::FMI_APIKEY_SUBST;
    hash["fmi_apikey_prefix"] = bw::QueryBase::FMI_APIKEY_PREFIX_SUBST;
    hash["hostname"] = QueryBase::HOSTNAME_SUBST;
    hash["protocol"] = QueryBase::PROTOCOL_SUBST;
    hash["language"] = language;
    hash["response_timestamp"] = runtime_timestamp;

    // wfs members
    hash["wfs_members"] = CTPP::CDT(CTPP::CDT::ARRAY_VAL);

    // iterate  results
    unsigned int wfs_member_index = 0;
    for (ContourQueryResultPtr result_item : query_results)
    {
      // iterate timesteps
      for (auto& area_geom : result_item->area_geoms)
      {
        std::string geom_timestamp = format_local_time(area_geom.timestamp, tzp);

        OGRGeometryPtr geom = area_geom.geometry;

        // dont parse empty geometry
        if (geom->IsEmpty())
          continue;

        if (geom->getGeometryType() == wkbMultiPolygon || geom->getGeometryType() == wkbPolygon ||
            geom->getGeometryType() == wkbLinearRing || geom->getGeometryType() == wkbLinearRing ||
            geom->getGeometryType() == wkbLineString ||
            geom->getGeometryType() == wkbMultiLineString)
        {
          CTPP::CDT& wfs_member = hash["wfs_members"][wfs_member_index];
          wfs_member["phenomenon_time"] = geom_timestamp;
          wfs_member["analysis_time"] = origintime_timestamp;
          wfs_member["modification_time"] = modificationtime_timestamp;
          wfs_member["feature_of_interest_shape"] = bbox2string(query_bbox, targetSRS);

          CTPP::CDT& result = wfs_member["result"];
          result["timestamp"] = geom_timestamp;
          setResultHashValue(result, *result_item);

          parseGeometry(geom.get(), latLonOrder, precision, result);

          wfs_member_index++;

          parseGeometry(geom.get(), latLonOrder, precision, result);
        }
        else if (geom->getGeometryType() == wkbGeometryCollection)
        {
          std::vector<CTPP::CDT> results;
          OGRGeometryCollection* pGeometryCollection =
              reinterpret_cast<OGRGeometryCollection*>(geom.get());
          handleGeometryCollection(pGeometryCollection, latLonOrder, precision, results);

          for (auto res : results)
          {
            CTPP::CDT& wfs_member = hash["wfs_members"][wfs_member_index];
            wfs_member["phenomenon_time"] = geom_timestamp;
            wfs_member["analysis_time"] = origintime_timestamp;
            wfs_member["modification_time"] = modificationtime_timestamp;
            wfs_member["feature_of_interest_shape"] = bbox2string(query_bbox, targetSRS);

            CTPP::CDT& result = wfs_member["result"];
            result = res;
            result["timestamp"] = geom_timestamp;
            setResultHashValue(result, *result_item);
            wfs_member_index++;
          }
        }
      }
    }

    hash["number_matched"] = std::to_string(wfs_member_index);
    hash["number_returned"] = std::to_string(wfs_member_index);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}
