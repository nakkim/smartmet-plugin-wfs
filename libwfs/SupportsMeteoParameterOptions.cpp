#include "SupportsMeteoParameterOptions.h"
#include "WfsConvenience.h"
#include "WfsException.h"
#include <boost/algorithm/string.hpp>
#include <macgyver/StringConversion.h>
#include <spine/Exception.h>
#include <stdint.h>

namespace bw = SmartMet::Plugin::WFS;
namespace ba = boost::algorithm;

bw::SupportsMeteoParameterOptions::SupportsMeteoParameterOptions(
    boost::shared_ptr<SmartMet::Plugin::WFS::StoredQueryConfig> config)
    : debug_level(config->get_debug_level()), options_map(), default_option_item()
{
  try
  {
    if (not config->find_setting(config->get_root(), "meteo_parameter_options", false))
      return;

    libconfig::Setting& parameter_options =
        config->get_mandatory_config_param<libconfig::Setting&>("meteo_parameter_options");

    config->assert_is_list(parameter_options);

    const int N = parameter_options.getLength();
    for (int i = 0; i < N; i++)
    {
      auto& param_desc = parameter_options[i];
      const std::string name = config->get_mandatory_config_param<std::string>(param_desc, "name");
      const uint precision = config->get_optional_config_param<uint>(
          param_desc, "precision", default_option_item.precision);
      const double missing_value = config->get_optional_config_param<double>(
          param_desc, "missing_value", default_option_item.missing_value);
      const std::string missing_text = config->get_optional_config_param<std::string>(
          param_desc, "missing_text", default_option_item.missing_text);

      if (precision > 15)
      {
        SmartMet::Spine::Exception exception(BCP, "Invalid precision value!");
        exception.addDetail("Allowed precision for a meteo parameter is between 0 and 15.");
        exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PARSING_FAILED);
        exception.addParameter("Requested precision", std::to_string(precision));
        throw exception;
      }

      MeteoParameterOptionItem optItem;
      optItem.precision = precision;
      optItem.missing_value = missing_value;
      optItem.missing_text = missing_text;
      options_map.emplace(Fmi::ascii_tolower_copy(name), optItem);
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

bw::SupportsMeteoParameterOptions::~SupportsMeteoParameterOptions() {}

std::shared_ptr<bw::MeteoParameterOptionItem>
bw::SupportsMeteoParameterOptions::get_meteo_parameter_options(const std::string& name) const
{
  try
  {
    MeteoParameterOptions::const_iterator it = options_map.find(Fmi::ascii_tolower_copy(name));

    if (it != options_map.end())
      return std::make_shared<MeteoParameterOptionItem>(it->second);

    return std::make_shared<MeteoParameterOptionItem>(default_option_item);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void bw::SupportsMeteoParameterOptions::setDefaultPrecision(const size_t& value)
{
  try
  {
    const size_t lower_limit = boost::numeric_cast<size_t>(std::numeric_limits<uint8_t>::min());
    const size_t upper_limit = boost::numeric_cast<size_t>(std::numeric_limits<uint8_t>::max());
    if (value >= lower_limit and value <= upper_limit)
    {
      default_option_item.precision = value;
    }
    else
    {
      std::ostringstream msg;
      msg << "SmartMet::Plugin::WFS::SupportsMeteoParameterOptions::setDefaultPrecision: value "
          << value << " is out of range " << lower_limit << ".." << upper_limit;
      throw SmartMet::Spine::Exception(BCP, msg.str());
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void bw::SupportsMeteoParameterOptions::setDefaultMissingText(const std::string& text)
{
  try
  {
    const size_t lower_limit = 1;
    const size_t upper_limit = 64;
    const size_t text_length = text.size();
    if (text_length > lower_limit and text_length <= upper_limit)
    {
      default_option_item.missing_text = text;
    }
    else
    {
      std::ostringstream msg;
      msg << "SmartMet::Plugin::WFS::SupportsMeteoParameterOptions::setDefaultMissingText: missing "
             "text "
             "length "
          << text_length << " is out of range " << lower_limit << ".." << upper_limit;
      throw SmartMet::Spine::Exception(BCP, msg.str());
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void bw::SupportsMeteoParameterOptions::setDefaultMissingValue(const double& value)
{
  try
  {
    const double lower_limit = std::numeric_limits<float>::min();
    const double upper_limit = std::numeric_limits<float>::max();
    if (value >= lower_limit and value <= upper_limit)
    {
      default_option_item.missing_value = value;
    }
    else
    {
      std::ostringstream msg;
      msg << "SmartMet::Plugin::WFS::SupportsMeteoParameterOptions::setDefaultMissingValue: value "
          << value << " is out of range " << lower_limit << ".." << upper_limit;
      throw SmartMet::Spine::Exception(BCP, msg.str());
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

/**

@page WFS_SQ_METEO_PARAM_OPTIONS Additional meteological parameter options.

The C++ class SmartMet::Plugin::WFS::SupportsMeteoParameterOptions implements additional option
parameters for meteorological parameters.

@section WFS_SQ_METEO_PARAM_OPTIONS_DEF The stored query meteorological parameter options definition

<table>

<tr>
<th>Parameter</th>
<th>Type</th>
<th>Use</th>
<th>Description</th>
</tr>

<tr>
<td>name</td>
<td>string</td>
<td>mandatory</td>
<td>The name of the meteorological parameters (case insensitive)</td>
</tr>

<tr>
<td>precision</td>
<td>uint</td>
<td>optional (default 1)</td>
<td>The precision defines the number of digits to the right of the decimal point.</td>
</tr>

<tr>
<td>missing_value</td>
<td>double</td>
<td>optional (default 32700.0)</td>
<td>A value to replaced with missing_text</td>
</tr>

<tr>
<td>missing_text</td>
<td>string</td>
<td>optional (default "NaN")</td>
<td>A text to replace the missing_value</td>
</tr>

</table>

Developer can change the default values by using methods with prefix setDefault. It is recommented
to
inheret a handler class from the class in constructor so that the options from the configuration
will be generater only ones. The default values can be changed on the runtime.


@section WFS_SQ_METEO_PARAM_OPTIONS_EXAMPLE Example meteorological parameter options configuration

If user has defined meteorological parameters ( @b handler_params.meteoParameters ) into a stored
query configuration as
the following sample shows it is possible to define additional options by using @b
meteo_parameter_options list.

@verbatim
handler_params:
{
        beginTime = "${starttime: 12 hours ago}";
        endTime = "${endtime: now}";
        places = ["${place}"];
        latlons = [];
        meteoParameters = ["t2m","ws_10min","wg_10min"];

 ...
 ...

};

meteo_parameter_options = (
     { name = "t2m"; precision = 4; missing_value = -1.0; missing_text = "missing"; }
    ,{ name = "ws_10min"; precision = 3; }
);
@endverbatim

*/
