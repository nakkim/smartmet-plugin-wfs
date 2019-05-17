#include "stored_queries/StoredFileQueryHandler.h"
#include "FeatureID.h"
#include "StoredQueryHandlerFactoryDef.h"
#include <macgyver/StringConversion.h>
#include <macgyver/TypeName.h>
#include <smartmet/spine/Convenience.h>
#include <smartmet/spine/Exception.h>
#include <set>

namespace bw = SmartMet::Plugin::WFS;

namespace pt = boost::posix_time;
using namespace boost::geometry;
using Fmi::current_exception_type;
using SmartMet::Spine::log_time_str;

namespace
{
const char* P_NAME = "name";
const char* P_LEVEL = "levels";
const char* P_PARAM = "params";
const char* P_BBOX = "bbox";
const char* P_BEGIN = "beginTime";
const char* P_END = "endTime";
}  // namespace

bw::StoredFileQueryHandler::StoredFileQueryHandler(SmartMet::Spine::Reactor* reactor,
                                                   boost::shared_ptr<StoredQueryConfig> config,
                                                   PluginImpl& plugin_data,
                                                   boost::optional<std::string> template_file_name)
    : bw::SupportsExtraHandlerParams(config),
      bw::StoredAtomQueryHandlerBase(reactor, config, plugin_data, template_file_name)
{
  try
  {
    register_scalar_param<std::string>(P_NAME, *config, false);
    register_array_param<int64_t>(P_LEVEL, *config);
    register_array_param<std::string>(P_PARAM, *config);
    register_array_param<double>(P_BBOX, *config, 0, 4, 4);
    register_array_param<pt::ptime>(P_BEGIN, *config);
    register_array_param<pt::ptime>(P_END, *config);

    auto& ds_list_cfg = config->get_mandatory_config_param<libconfig::Setting&>("dataSets");
    config->assert_is_list(ds_list_cfg, 1);
    const int len = ds_list_cfg.getLength();
    for (int i = 0; i < len; i++)
    {
      libconfig::Setting& ds_list_item = ds_list_cfg[i];
      try
      {
        boost::shared_ptr<bw::DataSetDefinition> ds_def =
            bw::DataSetDefinition::create(*config, ds_list_item);
        ds_list.push_back(ds_def);
      }
      catch (const std::exception& err)
      {
        std::ostringstream msg;
        msg << SmartMet::Spine::log_time_str() << ": [WFS] [ERROR] [C++ exception of type '"
            << current_exception_type() << "']: " << err.what() << std::endl;
        std::cout << msg.str() << std::flush;
      }
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

bw::StoredFileQueryHandler::~StoredFileQueryHandler() {}

namespace
{
template <typename ValueType>
std::set<ValueType> common_items(const std::set<ValueType>& A, const std::set<ValueType>& B)
{
  try
  {
    std::set<ValueType> common_elements;
    std::set_intersection(A.begin(),
                          A.end(),
                          B.begin(),
                          B.end(),
                          std::inserter(common_elements, common_elements.begin()));
    return common_elements;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}
}  // namespace

void bw::StoredFileQueryHandler::update_parameters(
    const RequestParameterMap& params,
    int seq_id,
    std::vector<boost::shared_ptr<RequestParameterMap> >& result) const
{
  try
  {
    typedef DataSetDefinition::point_t point_t;
    typedef DataSetDefinition::box_t box_t;

    std::set<int> level_set, common_levels;
    std::set<std::string> param_set, common_params;
    std::vector<double> bbox;

    const std::string name = params.get_optional<std::string>(P_NAME, std::string(""));
    params.get<int64_t>(P_LEVEL, std::inserter(level_set, level_set.begin()));
    params.get<std::string>(P_PARAM, std::inserter(param_set, param_set.begin()));
    params.get<double>(P_BBOX, std::back_inserter(bbox));
    const pt::ptime begin = params.get_single<pt::ptime>(P_BEGIN);
    const pt::ptime end = params.get_single<pt::ptime>(P_END);
    const std::string file = params.get_optional<std::string>("file", "");

    result.clear();

    bw::FeatureID feature_id(get_config()->get_query_id(), params.get_map(), seq_id);

    for (auto it1 = ds_list.begin(); it1 != ds_list.end(); ++it1)
    {
      const auto& ds_def = **it1;

      // Skip if name provided in query and is not the expected one
      if ((name != "") and (name != ds_def.get_name()))
        continue;

      // Skip if no common levels found
      if (not level_set.empty())
      {
        common_levels = common_items(level_set, ds_def.get_levels());
        if (common_levels.empty())
          continue;
      }
      else
      {
        common_levels = ds_def.get_levels();
      }

      // Skip if no common parameters found
      if (not param_set.empty())
      {
        common_params = common_items(param_set, ds_def.get_params());
        if (common_params.empty())
          continue;
      }
      else
      {
        common_params = ds_def.get_params();
      }

      if (not bbox.empty())
      {
        if (bbox.size() != 4)
        {
          // Should really be catched earlier and not be here
          throw SmartMet::Spine::Exception(
              BCP, "INTERNAL ERROR: wrong bbox specification in '" + params.as_string() + "'!");
        }

        const box_t sel_box(point_t(bbox[0], bbox[1]), point_t(bbox[2], bbox[3]));
        if (not ds_def.intersects(sel_box))
        {
          continue;
        }
      }

      // FIXME: check also BBOX

      const std::vector<boost::filesystem::path> files = ds_def.query_files(begin, end);
      for (auto it2 = files.begin(); it2 != files.end(); ++it2)
      {
        if ((file == "") or (file == it2->string()))
        {
          pt::ptime origin_time = ds_def.extract_origintime(*it2);
          boost::shared_ptr<RequestParameterMap> pm1(new RequestParameterMap);
          pm1->add("name", ds_def.get_name());
          pm1->add("basename", it2->filename().string());
          pm1->add("levels", common_levels.begin(), common_levels.end());
          pm1->add("params", common_params.begin(), common_params.end());
          pm1->add("file", it2->string());
          pm1->add("originTime", Fmi::to_iso_extended_string(origin_time));
          pm1->add("serverDir", ds_def.get_server_dir());

          feature_id.erase_param("file");
          feature_id.add_param("file", it2->string());
          pm1->add("featureId", feature_id.get_id());

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

namespace
{
using namespace SmartMet::Plugin::WFS;

boost::shared_ptr<SmartMet::Plugin::WFS::StoredQueryHandlerBase> wfs_stored_file_handler_create(
    SmartMet::Spine::Reactor* reactor,
    boost::shared_ptr<StoredQueryConfig> config,
    PluginImpl& plugin_data,
    boost::optional<std::string> template_file_name)
{
  try
  {
    bw::StoredFileQueryHandler* qh =
        new StoredFileQueryHandler(reactor, config, plugin_data, template_file_name);
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

SmartMet::Plugin::WFS::StoredQueryHandlerFactoryDef wfs_stored_file_handler_factory(
    &wfs_stored_file_handler_create);

/**

@page WFS_SQ_FILE_QUERY_HANDLER Stored Query handler for querying file download

@section WFS_SQ_FILE_QUERY_HANDLER_INTRO Introduction

This handler adds support for stored queries for direct data (files) download from
predefined file sets

<table border="1">
  <tr>
     <td>Implementation</td>
     <td>SmartMet::Plugin::WFS::StoredFileQueryHandler</td>
  </tr>
  <tr>
    <td>Constructor name (for stored query configuration)</td>
    <td>@b wfs_stored_file_handler_factory</td>
  </tr>
</table>

@section WFS_SQ_FILE_QUERY_HANDLER_PARAMS Query handler built-in parameters

The following stored query handler are in use

<table border="1">

<tr>
<th>Entry name</th>
<th>Type</th>
<th>Data type</th>
<th>Description</th>
</tr>

<tr>
  <td>name</td>
  <td>@ref WFS_CFG_SCALAR_PARAM_TMPL</td>
  <td>string</td>
  <td>File group name. Both names and aliases can be used</td>
</tr>

<tr>
  <td>levels</td>
  <td>@ref WFS_CFG_ARRAY_PARAM_TMPL</td>
  <td>integer</td>
  <td>Levels to select. Levels for each data set are specified in
      stored query configuration. All available levels are selected if none are
      specified in this parameter</td>
</tr>

<tr>
  <td>params</td>
  <td>@ref WFS_CFG_ARRAY_PARAM_TMPL</td>
  <td>string</td>
  <td>Parameters to select. Parameters for each data set are specified in
      stored query configuration. All available parameters are selected if none are
      specified in this parameter</td>
</tr>

<tr>
  <td>bbox</td>
  <td>@ref WFS_CFG_ARRAY_PARAM_TMPL (size 0 or 4)</td>
  <td>double</td>
  <td>Bounding box (size 0 if absent)</td>
</tr>

<tr>
  <td>beginTime</td>
  <td>@ref WFS_CFG_ARRAY_PARAM_TMPL (size 0 or 1)</td>
  <td>time</td>
  <td>The begin of the time interval</td>
</tr>

<tr>
  <td>endTime</td>
  <td>@ref WFS_CFG_ARRAY_PARAM_TMPL (size 0 or 1)</td>
  <td>time</td>
  <td>The end of time interval</td>
</tr>

</table>

@section WFS_SQ_FILE_QUERY_HANDLER__CONFIG_ENTRIES Additional configuration entries

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
  <td>dataSets</td>
  <td>@ref WFS_CFG_FD_DATA_SET "cfgFDDataSet"</td>
  <td>mandatory</td>
  <td>Specifies available file sets</td>
</tr>

</table>


Additional configuration data types:
- @subpage WFS_CFG_FD_DATA_SET "cfgFDDataSet"

*/
