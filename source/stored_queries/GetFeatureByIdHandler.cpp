#include "stored_queries/GetFeatureByIdHandler.h"
#include "StoredQueryHandlerBase.h"
#include "StoredQueryHandlerFactoryDef.h"
#include "StoredQueryMap.h"
#include "WfsConvenience.h"
#include <smartmet/spine/Exception.h>
#include <smartmet/spine/Value.h>
#include <sstream>

namespace bw = SmartMet::Plugin::WFS;

namespace
{
const char* P_ID = "feature_id";
}

bw::GetFeatureByIdHandler::GetFeatureByIdHandler(SmartMet::Spine::Reactor* reactor,
                                                 boost::shared_ptr<bw::StoredQueryConfig> config,
                                                 PluginImpl& plugin_data)
    : bw::SupportsExtraHandlerParams(config),
      bw::StoredQueryHandlerBase(reactor, config, plugin_data, boost::optional<std::string>())
{
  try
  {
    register_scalar_param<std::string>(P_ID, *config);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

bw::GetFeatureByIdHandler::~GetFeatureByIdHandler() {}

void bw::GetFeatureByIdHandler::query(const StoredQuery& query,
                                      const std::string& language,
                                      std::ostream& output) const
{
  try
  {
    const auto& params = query.get_param_map();
    const std::string id = params.get_single<std::string>(P_ID);
    auto query_p = bw::StoredQuery::create_from_feature_id(
        id, get_plugin_data().get_stored_query_map(), query);
    query_p->execute(output, language);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

std::vector<std::string> bw::GetFeatureByIdHandler::get_return_types() const
{
  try
  {
    return get_stored_query_map().get_return_type_names();
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

namespace
{
boost::shared_ptr<bw::StoredQueryHandlerBase> wfs_get_feature_by_id_handler_create(
    SmartMet::Spine::Reactor* reactor,
    boost::shared_ptr<bw::StoredQueryConfig> config,
    bw::PluginImpl& plugin_data,
    boost::optional<std::string> /* unused template_file_name */)
{
  try
  {
    bw::StoredQueryHandlerBase* qh = new bw::GetFeatureByIdHandler(reactor, config, plugin_data);
    boost::shared_ptr<bw::StoredQueryHandlerBase> result(qh);
    result->init_handler();
    return result;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}
}  // namespace

bw::StoredQueryHandlerFactoryDef wfs_get_feature_by_id_handler_factory(
    &wfs_get_feature_by_id_handler_create);

/**

@page WFS_SQ_GET_FEATURE_BY_ID Stored Query handler for GetFeatureByID query

@section WFS_SQ_GET_FEATURE_BY_ID_INTRO Introduction

GetFeatureByID provides querying data by feature ID provided in response of some previous request.
FeatureID values generated in smartmet implementation encode stored query handler parameters after
their resolving (retrieving parameters provided in request using the parameter mapping provided in
configuration). These stored query handler parameters are additionally modified in case of returning
more
than one response member to represent query which would return this member only.

C++ class SmartMet::Plugin::WFS::FeatureID provides
- encoding stored query handler parameters to generate feature ID
- decoding feature ID end retrieving stored query handler parameters

GetFeatureByID handler generates SmartMet::Plugin::WFS::StoredQuery object according to decoded
stored query parameters and STOREDQUERY_ID and executes it.

<table border="1">
  <tr>
    <td>Implementation</td>
    <td>SmartMet::Plugin::WFS::GetFeatureByIdHandler</td>
  </tr>
  <tr>
    <td>constructor name (for stored query configuration)</td>
    <td>@b wfs_get_feature_by_id_handler_factory</td>
</table>

@section WFS_SQ_GET_FEATURE_BY_ID_PARAMS Query handler built-in parameters

<table border="1">

<tr>
<th>Entry name</th>
<th>Type</th>
<th>Data type</th>
<th>Description</th>
</tr>

<tr>
  <td>feature_id</td>
  <td>@ref WFS_CFG_SCALAR_PARAM_TMPL "cfgScalarParameterTemplate"</td>
  <td>string</td>
  <td>Specifies feature ID returned by earlier request</td>
</tr>

</table>

*/
