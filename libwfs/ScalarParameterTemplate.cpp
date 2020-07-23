#include "ScalarParameterTemplate.h"
#include <spine/Exception.h>
#include <spine/Value.h>

namespace
{
typedef std::map<std::string, std::vector<SmartMet::Spine::Value> > param_map_t;
}

namespace SmartMet
{
namespace Plugin
{
namespace WFS
{
ScalarParameterTemplate::ScalarParameterTemplate(StoredQueryConfig& config,
                                                 const std::string& config_path)
    : ParameterTemplateBase(config, HANDLER_PARAM_NAME, config_path)
{
  try
  {
    try
    {
      init();
    }
    catch (...)
    {
      handle_exceptions(METHOD_NAME);
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

ScalarParameterTemplate::ScalarParameterTemplate(StoredQueryConfig& config,
                                                 const std::string& base_path,
                                                 const std::string& config_path)
    : ParameterTemplateBase(config, base_path, config_path)
{
  try
  {
    try
    {
      init();
    }
    catch (...)
    {
      handle_exceptions(METHOD_NAME);
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

ScalarParameterTemplate::~ScalarParameterTemplate() {}

SmartMet::Spine::Value ScalarParameterTemplate::get_value(
    const RequestParameterMap& req_param_map, const SupportsExtraHandlerParams* extra_params) const
{
  try
  {
    const boost::variant<SmartMet::Spine::Value, std::vector<SmartMet::Spine::Value> > value =
        item.get_value(req_param_map, extra_params);
    if (value.which() == 0) {
      return boost::get<SmartMet::Spine::Value>(value);
    } else {
      const std::vector<SmartMet::Spine::Value>& arr = boost::get<std::vector<SmartMet::Spine::Value> >(value);
      if (arr.size() == 0) {
        // Empty array: treat as empty value
          return SmartMet::Spine::Value();
      } else if (arr.size() == 1) {
        return arr.at(0);
      } else {
        throw SmartMet::Spine::Exception::Trace(BCP,
            METHOD_NAME + ": cannot convert array of size > 2 to scalar value")
            .disableStackTrace();
      }
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

bool ScalarParameterTemplate::get_value(SmartMet::Spine::Value& result,
                                        const RequestParameterMap& req_param_map,
                                        const SupportsExtraHandlerParams* extra_params) const
{
  try
  {
    boost::variant<SmartMet::Spine::Value, std::vector<SmartMet::Spine::Value> > tmp;
    boost::tribool ok = get_value(tmp, req_param_map, extra_params);
    if (ok)
      result = boost::get<SmartMet::Spine::Value>(tmp);
    return (ok ? true : false);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

boost::tribool ScalarParameterTemplate::get_value(
    boost::variant<SmartMet::Spine::Value, std::vector<SmartMet::Spine::Value> >& result,
    const RequestParameterMap& req_param_map,
    const SupportsExtraHandlerParams* extra_params,
    bool strict) const
{
  try
  {
    (void)strict;
    boost::variant<SmartMet::Spine::Value, std::vector<SmartMet::Spine::Value> > tmp;
    bool found = item.get_value(tmp, req_param_map, extra_params, false);
    if (found)
    {
      int which = tmp.which();
      if (which == 0)
      {
        result = boost::get<SmartMet::Spine::Value>(tmp);
        return true;
      }
      else /* (which == 1) */
      {
        const std::vector<SmartMet::Spine::Value>& vect =
            boost::get<std::vector<SmartMet::Spine::Value> >(tmp);
        switch (vect.size())
        {
          case 0:  // No data available
            return false;

          case 1:
            result = *vect.begin();
            return true;

          default:
          {
            throw SmartMet::Spine::Exception(
                BCP, "Scalar value expected. Got '" + as_string(vect) + "'!");
          }
        }
      }
    }
    return found;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

int64_t ScalarParameterTemplate::get_int_value(const RequestParameterMap& req_param_map,
                                               const SupportsExtraHandlerParams* extra_params) const
{
  try
  {
    try
    {
      SmartMet::Spine::Value tmp = ScalarParameterTemplate::get_value(req_param_map, extra_params);
      return tmp.get_int();
    }
    catch (...)
    {
      handle_exceptions(METHOD_NAME);
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

uint64_t ScalarParameterTemplate::get_uint_value(
    const RequestParameterMap& req_param_map, const SupportsExtraHandlerParams* extra_params) const
{
  try
  {
    try
    {
      SmartMet::Spine::Value tmp = ScalarParameterTemplate::get_value(req_param_map, extra_params);
      return tmp.get_uint();
    }
    catch (...)
    {
      handle_exceptions(METHOD_NAME);
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

double ScalarParameterTemplate::get_double_value(
    const RequestParameterMap& req_param_map, const SupportsExtraHandlerParams* extra_params) const
{
  try
  {
    try
    {
      SmartMet::Spine::Value tmp = ScalarParameterTemplate::get_value(req_param_map, extra_params);
      return tmp.get_double();
    }
    catch (...)
    {
      handle_exceptions(METHOD_NAME);
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

std::string ScalarParameterTemplate::get_string_value(
    const RequestParameterMap& req_param_map, const SupportsExtraHandlerParams* extra_params) const
{
  try
  {
    try
    {
      SmartMet::Spine::Value tmp = ScalarParameterTemplate::get_value(req_param_map, extra_params);
      return tmp.get_string();
    }
    catch (...)
    {
      handle_exceptions(METHOD_NAME);
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

boost::posix_time::ptime ScalarParameterTemplate::get_ptime_value(
    const RequestParameterMap& req_param_map, const SupportsExtraHandlerParams* extra_params) const
{
  try
  {
    try
    {
      SmartMet::Spine::Value tmp = ScalarParameterTemplate::get_value(req_param_map, extra_params);
      return tmp.get_ptime(true);
    }
    catch (...)
    {
      handle_exceptions(METHOD_NAME);
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

SmartMet::Spine::Point ScalarParameterTemplate::get_point_value(
    const RequestParameterMap& req_param_map, const SupportsExtraHandlerParams* extra_params) const
{
  try
  {
    try
    {
      SmartMet::Spine::Value tmp = ScalarParameterTemplate::get_value(req_param_map, extra_params);
      return tmp.get_point();
    }
    catch (...)
    {
      handle_exceptions(METHOD_NAME);
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

SmartMet::Spine::BoundingBox ScalarParameterTemplate::get_bbox_value(
    const RequestParameterMap& req_param_map, const SupportsExtraHandlerParams* extra_params) const
{
  try
  {
    try
    {
      SmartMet::Spine::Value tmp = ScalarParameterTemplate::get_value(req_param_map, extra_params);
      return tmp.get_bbox();
    }
    catch (...)
    {
      handle_exceptions(METHOD_NAME);
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void ScalarParameterTemplate::init()
{
  try
  {
    libconfig::Setting* setting = get_setting_root();
    if (setting) {
        const SmartMet::Spine::Value value = SmartMet::Spine::Value::from_config(*setting);
        item.parse(value, true);
        if (not item.weak and item.param_ref)
        {
            const StoredQueryConfig::ParamDesc& param = get_param_desc(*item.param_ref);

            if (param.isArray())
            {
                // The source for parameters is an array.
                if (not item.param_ind)
                {
                    // Do not accept request to copy entire array over scalar value
                    std::ostringstream msg;
                    msg << "Cannot copy entire parameter array '" << *item.param_ref << "' to scalar value";
                    throw SmartMet::Spine::Exception(BCP, msg.str());
                }
                else if (*item.param_ind >= param.getMaxSize())
                {
                    // The requested index is above max. possible size of parameter array
                    std::ostringstream msg;
                    msg << "The array index " << *item.param_ind << " is out of range 0.."
                        << param.getMaxSize();
                    throw SmartMet::Spine::Exception(BCP, msg.str());
                }
            }
        }
    } else {
        std::cout << "WARNING: stored query scalar parameter '" << get_config_path()
                  << "' definition missing in '" << get_config().get_file_name() << '\''
                  << std::endl;
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

template <>
boost::posix_time::ptime
ScalarParameterTemplate::Getter<boost::posix_time::ptime>::extract(const SmartMet::Spine::Value& src) const
{
    return src.get_ptime(true);
}

}  // namespace WFS
}  // namespace Plugin
}  // namespace SmartMet

/**

@page WFS_CFG_SCALAR_PARAM_TMPL Stored query handler scalar parameter template

@section WFS_CFG_SCALAR_PARAM_TMPL_INTRO Introduction

Scalar parameter template provides mapping of stored query handler scalar parameter to actual stored
query
parameter or constant. Only mapping to stored query scalar parameter not array parameter is
permitted.

Scalar parameter template consists on a single @ref WFS_CFG_PARAM_TMPL_ITEM value (except that
mapping to entire
array is not permitted)

Scalar parameter template support is implemented in C++ class
SmartMet::Plugin::WFS::ScalarParameterTemplate

@section WFS_CFG_SCALAR_PARAM_TMPL_EXAMPLES Introduction

Let us assume that the following stored query parameters are defined:
@verbatim
parameters:
(
{
    name = "starttime";
    title = {eng = "Begin of the time interval"; fin = "Alkuaika"; };
    abstract = { eng = "Parameter begin specifies the begin of time interval in ISO-format (for
example 2012-02-27T00:00:00Z)."; fin = "Aikajakson alkuaika ISO-muodossa (esim.
2012-02-27T00:00:00Z)."; };
    xmlType = "dateTime";
    type = "time";
},
{
    name = "bbox";
    title = { eng = "Bounding box of area for which to return data"; fin = "Aluerajaus"; };
    abstract = { eng = "Bounding box of area for which to return data (lon,lat,lon,lat). For example
21,61,22,62"; fin = "Aluerajaus, jolta palauttaa dataa (lon,lat,lon,lat). Esimerkiksi 21,61,22,62";
};
    xmlType = "xsi:string";
    type = "bbox";
    minOccurs = 0;
    maxOccurs = 1;
}
);
@endverbatim

Actual stored query may have more parameters, but their presence is not required for the following
examples.
Let us also assume that following additional named parameters are defined in stored query
configuration:

@verbatim
handler_params = (
    {
        name = "defaultBBox";
        def = "19.1,59.7,31.7,70.1";
    }
);
@endverbatim

Several examples of mapping stored query handler scalar parameter are provided in the table below

<table>

<tr>
<th>Mapping</th>
<th>Description</th>
</tr>

<tr>
<td>
@verbatim
startTime = "${starttime : 24 hours ago rounded down 60 min}"
@endverbatim
</td>
<td>
Maps to stored query parameter @b starttime and use as the default time 24 hours ago rounded
downwards to
the nearest hour
</td>
</tr>

<tr>
<td>
@verbatim
startTime = "${starttime}"
@endverbatim
</td>
<td>
Maps unconditionally to stored query parameter @b starttime
</td>
</tr>

<tr>
<td>
@verbatim
boundingBox = "{bbox > defaultBBox}"
@endverbatim
</td>
<td>
Maps to stored query parameter @b bbox and use the specified value of named parameter @b defaultBBox
as
the default value
</td>
</tr>

</table>

 */
