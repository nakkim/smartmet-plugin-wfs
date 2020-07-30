#include "stored_queries/GetDataSetByIdHandler.h"
#include "StoredQueryHandlerBase.h"
#include "StoredQueryHandlerFactoryDef.h"
#include "StoredQueryMap.h"
#include "WfsConvenience.h"
#include <boost/algorithm/string.hpp>
#include <boost/foreach.hpp>
#include <macgyver/StringConversion.h>
#include <macgyver/TypeName.h>
#include <smartmet/spine/Exception.h>
#include <smartmet/spine/Value.h>
#include <sstream>
#include <stdexcept>

namespace ba = boost::algorithm;
namespace bw = SmartMet::Plugin::WFS;

namespace
{
const char* P_DATA_SET_ID = "datasetid";
}

bw::GetDataSetByIdHandler::GetDataSetByIdHandler(SmartMet::Spine::Reactor* reactor,
                                                 bw::StoredQueryConfig::Ptr config,
                                                 PluginImpl& plugin_data)
    : bw::StoredQueryParamRegistry(config),
      bw::SupportsExtraHandlerParams(config),
      bw::StoredQueryHandlerBase(reactor, config, plugin_data, boost::optional<std::string>())
{
  try
  {
    register_scalar_param<std::string>(P_DATA_SET_ID);

    auto& ds_list = config->get_mandatory_config_param<libconfig::Setting&>("datasetids");
    config->assert_is_list(ds_list);
    const int N = ds_list.getLength();
    for (int i = 0; i < N; i++)
    {
      libconfig::Setting& item = ds_list[i];
      config->assert_is_group(item);
      const std::string ds_id = config->get_mandatory_config_param<std::string>(item, "data_set");
      const std::string sq_id =
          config->get_mandatory_config_param<std::string>(item, "stored_query");
      const std::string ns =
          config->get_optional_config_param<std::string>(item, "namespace", "FI");
      if (not data_set_map.insert(std::make_pair(ds_id, sq_id)).second)
      {
        throw SmartMet::Spine::Exception(BCP, "Duplicate data set ID '" + ds_id + "'!");
      }

      plugin_data.get_capabilities().register_data_set(ds_id, ns);
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

bw::GetDataSetByIdHandler::~GetDataSetByIdHandler() {}

void bw::GetDataSetByIdHandler::init_handler() {}

void bw::GetDataSetByIdHandler::query(const StoredQuery& query,
                                      const std::string& language,
				      const boost::optional<std::string>& hostname,
                                      std::ostream& output) const
{
  try
  {
    (void)query;
    (void)language;
    (void)output;

    SmartMet::Spine::Exception exception(BCP, "Operation processing failed!");
    exception.addDetail("The method not expected to be called (should have been redirected).");
    exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PROCESSING_FAILED);
    throw exception;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

bool bw::GetDataSetByIdHandler::redirect(const StoredQuery& query,
                                         std::string& new_stored_query_id) const
{
  try
  {
    const std::string data_set_id = query.get_param_map().get_single<std::string>(P_DATA_SET_ID);
    auto it2 = data_set_map.find(Fmi::ascii_tolower_copy(data_set_id));
    if (it2 == data_set_map.end())
    {
      bool start = true;
      std::ostringstream msg;
      msg << "Available data set IDs are ";
      BOOST_FOREACH (const auto& x, data_set_map)
      {
        if (not start)
          msg << ", ";
        msg << "'" << x.first << "'";
        start = false;
      }
      SmartMet::Spine::Exception exception(BCP, "Data set ID '" + data_set_id + "' not found!");
      exception.addDetail(msg.str());
      exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PROCESSING_FAILED);
      throw exception.disableStackTrace();
    }
    else
    {
      new_stored_query_id = it2->second;
      return true;
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

std::vector<std::string> bw::GetDataSetByIdHandler::get_return_types() const
{
  try
  {
    // FIXME: update this
    return get_stored_query_map().get_return_type_names();
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

namespace
{
boost::shared_ptr<bw::StoredQueryHandlerBase> wfs_get_data_set_by_id_handler_create(
    SmartMet::Spine::Reactor* reactor,
    bw::StoredQueryConfig::Ptr config,
    bw::PluginImpl& plugin_data,
    boost::optional<std::string> /* unused template_file_name */)
{
  try
  {
    bw::StoredQueryHandlerBase* qh = new bw::GetDataSetByIdHandler(reactor, config, plugin_data);
    boost::shared_ptr<bw::StoredQueryHandlerBase> result(qh);
    return result;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}
}  // namespace

bw::StoredQueryHandlerFactoryDef wfs_get_data_set_by_id_handler_factory(
    &wfs_get_data_set_by_id_handler_create);

/**

@page WFS_SQ_DATA_SET_BY_ID Stored query handler for getting predefined data sets

@section WFS_SQ_DATA_SET_BY_ID_INTRO Introduction

This stored query handler provides access to predefined data sets. The access to predefined data
sets
is implemented by redirecting request to another stored query specified in configuration. One may
use stored query configuration entry @b hidden to hide these stored queries which implements
access to data sets

<table border="1">
  <tr>
    <td>Implementation</td>
    <td>SmartMet::Plugin::WFS::GetDataSetByIdHandler</td>
  </tr>
  <tr>
    <td>constructor name (for stored query configuration)</td>
    <td>@b wfs_get_data_set_by_id_handler_factory</td>
</table>

@section WFS_SQ_DATA_SET_BY_ID_PARAM Stored query handler built-in parameters

<table border="1">

<tr>
<th>Entry name</th>
<th>Type</th>
<th>Data type</th>
<th>Description</th>
</tr>

<tr>
  <td>datasetid</td>
  <td>@ref WFS_CFG_SCALAR_PARAM_TMPL "cfgScalarParameterTemplate"</td>
  <td>string</td>
  <td>Specifies predefined data set to return</td>
</tr>

</table>

@section WFS_SQ_DATA_SET_BY_ID_EXTRA_CFG Additional Stored Query configuration entries

<table border="1">

<tr>
<th>Entry name</th>
<th>Type</th>
<th>Use</th>
<th>Description</th>
</tr>

<tr>
  <td>datasetids</td>
  <td>list of @ref WFS_SQ_DATA_SET_DEF "cfgDataSetDef"</td>
  <td>mandatory</td>
  <td>Lists available data sets</td>
</tr>

</table>

Additional data types
- @subpage WFS_SQ_DATA_SET_DEF "cfgDataSetDef"

@section WFS_SQ_DATA_SET_BY_ID_EXAMPLE Example

@verbatim
disabled = false;
demo = false;

id = "GetDataSetById";

constructor_name = "wfs_get_data_set_by_id_handler_factory";

title:
{
        eng = "GetDataSetById";
        fin = "GetDataSetById";
};

abstract:
{
        eng = "GetDataSetById returns INSPIRE data sets. Consult catalog.fmi.fi to investigate
possible data sets.";
        fin = "GetDataSetById palauttaa INSPIRE-datasettejä. Mahdolliset datasetit löydät
catalog.fmi.fi-palvelusta.";
};

parameters:
(

{
        name = "datasetid";

        abstract:
        {
                eng = "Specifies ID of data set to return";
                fin = "Määritää haettavan datasetin id:n.";
        };

        title:
        {
                eng = "Data set ID";
                fin = "Data setin ID";
        };

        xmlType = "xsi:string";
        type = "string";
}

);

returnTypeNames = [];

datasetids:
(
# Aallokkohavainnot
{ data_set = "1000549"; stored_query = "fmi::observations::wave::timevaluepair"; namespace = "FI";
},
# Asemakohtaiset ilmastolliset vertailuarvot 30-vuotisjaksoilla 1971-2000 sekä 1981-2010
# { data_set = "1000550"; stored_query = ""; namespace = "FI"; },
# Ilmastonmuutosennusteet
{ data_set = "1000551"; stored_query = "fmi::forecast::climatology::scenario::grid"; namespace =
"FI"; },
# Interpoloidut lämpötilan ja sateen kuukausiarvot
{ data_set = "1000552"; stored_query = "fmi::observations::weather::monthly::timevaluepair";
namespace = "FI";  },
# Ilmatieteen laitoksen pintasäähavaintoasemien kuukausiarvot
{ data_set = "1000554"; stored_query =
"fmi::datasets::observations::weather::monthly::timevaluepair"; namespace = "FI"; },
# Ilmatieteen laitoksen meriveden korkeusennuste
{ data_set = "1000555"; stored_query = "fmi::forecast::oaas::sealevel::point::timevaluepair";
namespace = "FI"; },
# Ilmatieteen laitoksen meriveden korkeushavainnot
{ data_set = "1000556"; stored_query = "fmi::observations::mareograph::timevaluepair"; namespace =
"FI"; },
# Ilmatieteen laitoksen pintasäähavainnot
{ data_set = "1000557"; stored_query = "fmi::datasets::observations::weather::timevaluepair";
namespace = "FI"; },
# Ilmatieteen laitoksen säämalli RCR Hirlam
{ data_set = "1000558"; stored_query = "fmi::forecast::hirlam::surface::grid"; namespace = "FI"; },
# Ilmatieteen laitoksen säätutkahavainnot
{ data_set = "1000559"; stored_query = "fmi::radar"; namespace = "FI"; },
# Salamahavainnot
{ data_set = "1000560"; stored_query = "fmi::observations::lightning::multipointcoverage"; namespace
= "FI"; },
# Ilmatieteen laitoksen auringon säteilyhavainnot
{ data_set = "1000561"; stored_query = "fmi::observations::radiation::timevaluepair"; namespace =
"FI"; },
# Taustailmanlaatuhavainnot
# { data_set = "1000562"; stored_query = ""; namespace = "FI"; },
# Ilmatieteen laitoksen vuorokausiarvot
{ data_set = "1000565"; stored_query = "fmi::datasets::observations::weather::daily::timevaluepair";
namespace = "FI"; }
);

handler_params:
{
        datasetid = "${datasetid}";
};
@endverbatim

*/

/**

@page WFS_SQ_DATA_SET_DEF Definition of predefined data set

Only used by @ref WFS_SQ_DATA_SET_BY_ID

<table border="1">

<tr>
<th>Entry name</th>
<th>Type</th>
<th>Use</th>
<th>Description</th>
</tr>

<tr>
  <td>@b data_set</td>
  <td>string</td>
  <td>mandatory</td>
  <td>Specifies data set ID</td>
</tr>

<tr>
  <td>@b stored_query</td>
  <td>string</td>
  <td>mandatory</td>
  <td>Specifies stored query ID to use for getting data set</td>
</tr>

<tr>
  <td>namespace</td>
  <td>string</td>
  <td>optional (default @b FI)</td>
  <td>Namespace value to use for @b GetCapabilities response)</td>
</tr>

</table>




 */
