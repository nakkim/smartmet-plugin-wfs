#include "FeatureID.h"
#include "GeoServerDataIndex.h"
#include "StoredQueryHandlerFactoryDef.h"
#include "WfsConvenience.h"
#include "WfsException.h"
#include "stored_queries/StoredGeoserverQueryHandler.h"
#include <boost/algorithm/string.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <macgyver/StringConversion.h>
#include <macgyver/TypeName.h>
#include <smartmet/spine/Convenience.h>
#include <smartmet/spine/Exception.h>
#include <sstream>

namespace ba = boost::algorithm;
namespace bw = SmartMet::Plugin::WFS;
namespace pt = boost::posix_time;

const char* bw::StoredGeoserverQueryHandler::P_BEGIN_TIME = "begin";
const char* bw::StoredGeoserverQueryHandler::P_END_TIME = "end";
const char* bw::StoredGeoserverQueryHandler::P_LAYERS = "layers";
const char* bw::StoredGeoserverQueryHandler::P_WIDTH = "width";
const char* bw::StoredGeoserverQueryHandler::P_HEIGHT = "height";
const char* bw::StoredGeoserverQueryHandler::P_CRS = "crs";

bw::StoredGeoserverQueryHandler::StoredGeoserverQueryHandler(
    SmartMet::Spine::Reactor* reactor,
    boost::shared_ptr<StoredQueryConfig> config,
    PluginImpl& plugin_data,
    boost::optional<std::string> template_file_name)
    : bw::SupportsExtraHandlerParams(config),
      bw::StoredAtomQueryHandlerBase(reactor, config, plugin_data, template_file_name),
      bw::SupportsBoundingBox(config, plugin_data.get_crs_registry()),
      debug_level(get_config()->get_debug_level())
{
  try
  {
    register_scalar_param<pt::ptime>(P_BEGIN_TIME, *config);
    register_scalar_param<pt::ptime>(P_END_TIME, *config);
    register_array_param<std::string>(P_LAYERS, *config, 1, 999);
    register_scalar_param<uint64_t>(P_WIDTH, *config);
    register_scalar_param<uint64_t>(P_HEIGHT, *config);
    register_scalar_param<std::string>(P_CRS, *config);

    std::string table_name_format;
    if (config->get_config().lookupValue("layerDbTableNameFormat", table_name_format))
    {
      layer_db_table_name_format = table_name_format;
    }
    else if (config->get_config().exists("layerMap"))
    {
      auto& cfg_layer_map = config->get_mandatory_config_param<libconfig::Setting&>("layerMap");
      config->assert_is_list(cfg_layer_map, 1);
      const int len = cfg_layer_map.getLength();
      layer_map = std::map<std::string, std::string>();
      for (int i = 0; i < len; i++)
      {
        auto& cfg_item = cfg_layer_map[i];
        config->assert_is_group(cfg_item);
        std::vector<std::string> aliases;
        const std::string name = config->get_mandatory_config_param<std::string>(cfg_item, "name");
        const std::string db_table =
            config->get_mandatory_config_param<std::string>(cfg_item, "db_table");
        config->get_config_array(cfg_item, "alias", aliases, 0);
        if (not layer_map->insert(std::make_pair(name, db_table)).second)
        {
          throw SmartMet::Spine::Exception(BCP, "Duplicate layer name '" + name + "'!");
        }

        BOOST_FOREACH (const auto& alias, aliases)
        {
          const std::string alias_name = Fmi::ascii_tolower_copy(alias);
          if (not layer_alias_map.insert(std::make_pair(alias_name, name)).second)
          {
            throw SmartMet::Spine::Exception(BCP, "Duplicate layer alias '" + alias + "'!");
          }
        }
      }
    }
    else
    {
      layer_db_table_name_format = "mosaic.%1%";
    }

    const char* DB_SELECT_PARAMS = "dbSelectParams";

    std::vector<std::string> tmp;
    if (config->get_config_array(DB_SELECT_PARAMS, tmp))
    {
      for (auto it = tmp.begin(); it != tmp.end(); ++it)
      {
        const auto& name = *it;
        if (not db_filter_param_names.insert(name).second)
        {
          std::ostringstream msg;
          msg << "{" << config->get_file_name() << "}: " << DB_SELECT_PARAMS << ": duplicate name '"
              << name << "' specified";
          throw SmartMet::Spine::Exception(BCP, msg.str());
        }
      }
    }

    auto& layer_and_param_name_map =
        config->get_mandatory_config_param<libconfig::Setting&>("layerParamNameMap");
    config->assert_is_list(layer_and_param_name_map, 1);
    const int lapnm_len = layer_and_param_name_map.getLength();
    layer_param_name_map = std::map<std::string, std::string>();
    for (int i = 0; i < lapnm_len; i++)
    {
      auto& item = layer_and_param_name_map[i];
      config->assert_is_group(item);
      const std::string layer_name = config->get_mandatory_config_param<std::string>(item, "layer");
      const std::string parameter_name =
          config->get_mandatory_config_param<std::string>(item, "param");
      if (layer_param_name_map.find(layer_name) != layer_param_name_map.end())
      {
        throw SmartMet::Spine::Exception(BCP, "Duplicate layer name '" + layer_name + "'!");
      }
      layer_param_name_map.insert(std::make_pair(Fmi::ascii_tolower_copy(layer_name),
                                                 Fmi::ascii_tolower_copy(parameter_name)));
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

bw::StoredGeoserverQueryHandler::~StoredGeoserverQueryHandler() {}

void bw::StoredGeoserverQueryHandler::update_parameters(
    const RequestParameterMap& params,
    int seq_id,
    std::vector<boost::shared_ptr<RequestParameterMap> >& result) const
{
  try
  {
    namespace ba = boost::algorithm;

    const unsigned MIN_SIZE = 10;
    const char* P_SELECTED_NAME = "selectedName";

    std::vector<std::string> layers;  //  = t_layers.get_string_array(params, this);
    const std::string crs_name = params.get_single<std::string>(P_CRS);
    std::string s_crs = Fmi::ascii_tolower_copy(crs_name);
    std::size_t pos = s_crs.rfind("epsg::");
    if (pos != std::string::npos)
      s_crs = s_crs.substr(6);
    int crs = Fmi::stoi(s_crs);
    pt::ptime start_time = params.get_single<pt::ptime>(P_BEGIN_TIME);
    pt::ptime end_time = params.get_single<pt::ptime>(P_END_TIME);
    unsigned width = params.get_single<uint64_t>(P_WIDTH);
    unsigned height = params.get_single<uint64_t>(P_HEIGHT);
    const std::string selected_name = params.get_optional<std::string>(P_SELECTED_NAME, "");
    params.get<std::string>(P_LAYERS, std::back_inserter(layers));
    BOOST_FOREACH (std::string& layer, layers)
    {
      auto pos = layer_alias_map.find(Fmi::ascii_tolower_copy(layer));
      if (pos != layer_alias_map.end())
      {
        layer = pos->second;
      }

      // Handling the case when not found
      std::map<std::string, std::string>::const_iterator layerIt = layer_param_name_map.find(layer);
      if (layerIt == layer_param_name_map.end() and pos == layer_alias_map.end())
      {
        SmartMet::Spine::Exception exception(
            BCP, "Parameter configuration for layer '" + layer + "' not found!");
        exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PARSING_FAILED);
        throw exception;
      }
    }

    SmartMet::Spine::BoundingBox requested_bbox;
    if (not get_bounding_box(params, crs_name, &requested_bbox))
    {
      SmartMet::Spine::Exception exception(BCP, "Invalid or missing bounding box!");
      exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PARSING_FAILED);
      throw exception;
    }

    std::map<std::string, std::string> db_select_map;
    for (auto it = db_filter_param_names.begin(); it != db_filter_param_names.end(); ++it)
    {
      std::vector<SmartMet::Spine::Value> sel_param;
      params.get<SmartMet::Spine::Value>(*it, std::back_inserter(sel_param), 1, 2);
      if (sel_param.size() == 2)
      {
        const std::string& name = sel_param.at(0).get<std::string>();
        const std::string& val = sel_param.at(1).get<std::string>();
        db_select_map[name] = val;
      }
    }

    int bb_epsg = 3067;
    crs_registry.get_attribute(requested_bbox.crs, "epsg", &bb_epsg);

    if (debug_level > 1)
    {
      std::ostringstream msg;
      SmartMet::Spine::Value bbox1(requested_bbox), bbox2(bb_epsg);
      msg << SmartMet::Spine::log_time_str() << ": [WFS] [DEBUG] [" << get_config()->get_query_id()
          << "]:"
          << " requested BBOX:" << bbox1 << ", converted BBOX:" << bbox2 << "\n";
      std::cout << msg.str() << std::flush;
    }

    if (width < MIN_SIZE or height < MIN_SIZE)
    {
      SmartMet::Spine::Exception exception(
          BCP, "Invalid image size : " + std::to_string(width) + "x" + std::to_string(height));
      exception.addDetail("The values must be " + std::to_string(MIN_SIZE) + " or larger.");
      exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PARSING_FAILED);
      throw exception;
    }

    double bb[4] = {
        requested_bbox.xMin, requested_bbox.yMin, requested_bbox.xMax, requested_bbox.yMax};

    auto& db = *get_plugin_data().get_geo_server_database();
    std::unique_ptr<bw::GeoServerDataIndex> gs_index;
    if (layer_db_table_name_format)
    {
      gs_index.reset(new bw::GeoServerDataIndex(db, *layer_db_table_name_format));
    }
    else
    {
      assert(layer_map);
      gs_index.reset(new bw::GeoServerDataIndex(db, *layer_map));
    }

    gs_index->set_debug_level(debug_level);
    gs_index->query(start_time, end_time, layers, bb, bb_epsg, crs);

    if ((debug_level > 1) and gs_index->size() == 0)
    {
      SmartMet::Spine::Exception exception(BCP, "No data found!");
      exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PROCESSING_FAILED);
      throw exception;
    }

    result.clear();

    bw::FeatureID feature_id(get_config()->get_query_id(), params.get_map(), seq_id);

    for (auto it1 = gs_index->begin(); it1 != gs_index->end(); ++it1)
    {
      for (auto it2 = it1->second.layers.begin(); it2 != it1->second.layers.end(); ++it2)
      {
        if ((selected_name == "") or (selected_name == it2->name))
        {
          if (not eval_db_select_params(db_select_map, *it2))
          {
            continue;
          }

          feature_id.erase_param(P_SELECTED_NAME);
          feature_id.add_param(P_SELECTED_NAME, it2->name);

          boost::shared_ptr<RequestParameterMap> pm1(new RequestParameterMap);
          pm1->add("epoch", Fmi::to_iso_extended_string(it1->second.epoch) + "Z");
          pm1->add("name", it2->name);
          pm1->add("layer", it2->layer);
          pm1->add("origSrs", it2->orig_srs);
          pm1->add("destSrs", it2->dest_srs);
          pm1->add("featureId", feature_id.get_id());

          // Axis labels
          std::string orig_axis_labels = "Easting Northing";
          std::string dest_axis_labels = "Easting Northing";
          crs_registry.get_attribute(
              "epsg::" + Fmi::to_string(it2->orig_srs), "axisLabels", &orig_axis_labels);
          crs_registry.get_attribute(
              "epsg::" + Fmi::to_string(it2->dest_srs), "axisLabels", &dest_axis_labels);

          std::vector<std::string> orig_axisLabels_vect;
          std::vector<std::string> dest_axisLabels_vect;
          ba::split(orig_axisLabels_vect, orig_axis_labels, ba::is_any_of(" "));
          ba::split(dest_axisLabels_vect, dest_axis_labels, ba::is_any_of(" "));

          BOOST_FOREACH (const std::string& label, orig_axisLabels_vect)
            pm1->add("origSrsAxisLabels", label);
          BOOST_FOREACH (const std::string& label, dest_axisLabels_vect)
            pm1->add("destSrsAxisLabels", label);

          const auto orig_coords = it2->get_orig_coords();
          BOOST_FOREACH (double value, orig_coords)
          {
            pm1->add("origBoundary", value);
          }

          OGREnvelope envelope;

          it2->orig_geom->getEnvelope(&envelope);
          double c_env[4] = {envelope.MinX, envelope.MinY, envelope.MaxX, envelope.MaxY};
          pm1->add("origEnvelope", c_env, c_env + 4);

          if (it2->orig_srs_swapped)
            pm1->add("origSrsSwapped", it2->orig_srs_swapped);
          if (it2->dest_srs_swapped)
            pm1->add("destSrsSwapped", it2->dest_srs_swapped);

          pm1->add("corner", envelope.MinX);
          pm1->add("corner", envelope.MinY);

          const double xStep = (envelope.MaxX - envelope.MinX) / width;
          const double yStep = (envelope.MaxY - envelope.MinY) / height;

          pm1->add("xStep", xStep);
          pm1->add("yStep", yStep);
          pm1->add("width", width);
          pm1->add("height", height);

          const std::string param_name;
          std::map<std::string, std::string>::const_iterator layerParamNameIt =
              layer_param_name_map.find(it2->layer);
          if (layerParamNameIt != layer_param_name_map.end())
            pm1->add("layerParam", layerParamNameIt->second);

          const auto dest_coords = it2->get_dest_coords();
          BOOST_FOREACH (double value, dest_coords)
          {
            char tmp[80];
	    snprintf(tmp, sizeof(tmp), "%.6f", value);
            pm1->add("destBoundary", tmp);
          }

          pm1->add("origSrsDim", it2->orig_geom->getCoordinateDimension());
          pm1->add("destSrsDim", it2->dest_geom->getCoordinateDimension());

          BOOST_FOREACH (const auto& map_item, it2->data_map)
          {
            pm1->add(std::string("db_") + map_item.first, map_item.second.to_string());
          }

          result.push_back(pm1);
        }
      }
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

bool bw::StoredGeoserverQueryHandler::eval_db_select_params(
    const std::map<std::string, std::string>& select_params,
    const GeoServerDataIndex::LayerRec& rec) const
{
  try
  {
    bool ok = true;
    for (auto it = select_params.begin(); ok and it != select_params.end(); ++it)
    {
      // FIXME: handle type of param[1] instead of asuming std::string
      const std::string& column_name = it->first;
      const std::string& column_value = it->second;
      auto map_iter = rec.data_map.find(column_name);
      if ((map_iter == rec.data_map.end()) or (map_iter->second.get<std::string>() != column_value))
      {
        ok = false;
      }
    }

    return ok;
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
wfs_stored_geoserver_handler_create(SmartMet::Spine::Reactor* reactor,
                                    boost::shared_ptr<StoredQueryConfig> config,
                                    PluginImpl& plugin_data,
                                    boost::optional<std::string> template_file_name)
{
  try
  {
    StoredAtomQueryHandlerBase* qh =
        new StoredGeoserverQueryHandler(reactor, config, plugin_data, template_file_name);
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

SmartMet::Plugin::WFS::StoredQueryHandlerFactoryDef wfs_stored_geoserver_handler_factory(
    &wfs_stored_geoserver_handler_create);

/**

@page WFS_SQ_GEOSERVER_DOWNLOAD_HANDLER Stored Query handler for GeoServer data download

@section WFS_GEOSERVER_DOWNLOAD_HANDLER_INTRO Introduction

This stored query handler provides support for data download from GeoServer. It checks
index of data from PostGIS database and provide download links for related data.

<table border="1">
  <tr>
     <td>Implementation</td>
     <td>SmartMet::Plugin::WFS::StoredGeoserverQueryHandler</td>
  </tr>
  <tr>
    <td>Constructor name (for stored query configuration)</td>
    <td>@b wfs_stored_geoserver_handler_factory</td>
  </tr>
</table>

@section WFS_SQ_GEOSERVER_DOWNLOAD_HANDLER_PARAMS Query handler built-in parameters

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
  <td>beginTime</td>
  <td>@ref WFS_CFG_SCALAR_PARAM_TMPL</td>
  <td>time</td>
  <td>Specifies the begin of time interval for which to query data</td>
</tr>

<tr>
  <td>endTime</td>
  <td>@ref WFS_CFG_SCALAR_PARAM_TMPL</td>
  <td>time</td>
  <td>Specifies the end of time interval for which to query data</td>
</tr>

<tr>
  <td>layers</td>
  <td>@ref WFS_CFG_ARRAY_PARAM_TMPL</td>
  <td>time</td>
  <td>Specifies the layer names which to use. Specifying an empty array causes all available
      layers being used</td>
</tr>

<tr>
  <td>width</td>
  <td>@ref WFS_CFG_SCALAR_PARAM_TMPL</td>
  <td>unsigned integer</td>
  <td>Specifies the width of image to return</td>
</tr>

<tr>
  <td>height</td>
  <td>@ref WFS_CFG_SCALAR_PARAM_TMPL</td>
  <td>unsigned integer</td>
  <td>Specifies the height of image to return</td>
</tr>

<tr>
  <td>crs</td>
  <td>@ref WFS_CFG_SCALAR_PARAM_TMPL</td>
  <td>string</td>
  <td>Specifies coordinate projection</td>
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
  <td>url_template</td>
  <td>@ref WFS_CFG_URL_TEMPLATE "cfgUrlTemplate"</td>
  <td>mandatory</td>
  <td>Specifies a template for generating data download URL</td>
</tr>

<tr>
  <td>layerDbTableNameFormat</td>
  <td>string</td>
  <td>optional</td>
  <td>Specifies how to generate PostGIS database tablename from layer name in boost::format
      format string. boost::format() is is invoked with a single parameter of
      layer name</td>
</tr>

<tr>
  <td>layerMap</td>
  <td>list of @ref WFS_PG_CFG_LAYER_ITEM "cfgPGLayerMapItem"</td>
  <td>optional</td>
  <td>Specifies the mapping of layers (and their aliases) to actual database tables.
      Ignored if configuration parameter @b layerDbTableNameFormat is specified</td>
</tr>

<tr>
  <td>layerParamNameMap</td>
  <td>list of @ref WFS_CFG_LAYER_PARAMETER_NAME_ITEM "cnfLayerParameterNameItem"</td>
  <td>mandatory</td>
  <td>Specifies the mapping between layer and the parameter name of the data used in the layer.</td>
</tr>

<tr>
  <td>dbSelectParams</td>
  <td>array of strings</td>
  <td>optional</td>
  <td>Specifies extra database parameters which can be used for additional filtering
      of data to be queried. Currently only filtering by equal string value is
      supported</td>
</tr>

</table>

Used additional configuration data types:
- @subpage WFS_PG_CFG_LAYER_ITEM "cfgPGLayerMapItem"

@b Notes:
- if neither @b layerDbTableNameFormat nor @b layerMap is provided a default value
  '<b>mosaic.\%1\%</b> of @b layerDbTableNameFormat is being used


@section WFS_SQ_QE_DOWNLOAD_HANDLER_DB_FILTER Filtering data by additional database fields

One can define optional filtering by additionl database parameter by
- specifying filter named parameter names in the configuration array @b dbSelectParams
- adding array parameters with these names (mandatory) to list of named parameter.
  Added named parameters must have size either
  - @b 1 - in this case filter is ignored
  - @b 2 - in this case first array element acts as database column name, the second one
           as string value to be expected in this column. Note that
           - only string values
           - only check for equality is supported
  - other array sizes causes an error be triggered

*/

/**

@page WFS_PG_CFG_LAYER_ITEM GeoServer layer description

<table border="1">

<tr>
<th>Entry name</th>
<th>Type</th>
<th>Use</th>
<th>Description</th>
</tr>

<tr>
  <td>name</td>
  <td>string</td>
  <td>mandatory</td>
  <td>The layer name</td>
</tr>

<tr>
  <td>alias</td>
  <td>array of strings</td>
  <td>optional</td>
  <td>Defines alias names of layer. Layer can be queries bith by its name or
      by its aloas name</td>
</tr>

<tr>
  <td>db_table</td>
  <td>string</td>
  <td>mandatory</td>
  <td>PostGIS database table name for the layer</td>
</tr>

</table>

*/

/**

@page WFS_CFG_LAYER_PARAMETER_NAME_ITEM GeoServer layer name and parameter name mapping item

<table border="1">

<tr>
<th>Entry name</th>
<th>Type</th>
<th>Use</th>
<th>Description</th>
</tr>

<tr>
  <td>layer</td>
  <td>string</td>
  <td>mandatory</td>
  <td>The layer name</td>
</tr>

<tr>
  <td>param</td>
  <td>string</td>
  <td>mandatory</td>
  <td>Name of the parameter of the data used in the layer.</td>
</tr>

</table>

*/
