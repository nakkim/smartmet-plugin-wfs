#pragma once

#include "StoredQueryConfig.h"
#include <boost/shared_ptr.hpp>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace SmartMet
{
namespace Plugin
{
namespace WFS
{
struct MeteoParameterOptionItem
{
  size_t precision;  // The precision defines the number of digits to the right of the decimal
                     // point.
  double missing_value;
  std::string missing_text;
  MeteoParameterOptionItem() : precision(1), missing_value(32700.0), missing_text("NaN") {}
};

typedef std::map<std::string, MeteoParameterOptionItem> MeteoParameterOptions;

/**
 *   @brief The class implementing options for meteo parameters.
 */
class SupportsMeteoParameterOptions
{
 public:
  SupportsMeteoParameterOptions(boost::shared_ptr<SmartMet::Plugin::WFS::StoredQueryConfig> config);

  virtual ~SupportsMeteoParameterOptions();

  /**
   * @brief Change the default precision.
   */
  void setDefaultPrecision(const size_t& value);

  /**
   * @brief Change the default missing text.
   */
  void setDefaultMissingText(const std::string& text);

  /**
   * @brief Change the default missing value.
   */
  void setDefaultMissingValue(const double& value);

  /**
   * @brief Get meteoparameter options.
   * @param[in] name Meteorological parameter name (case insensitive)
   * @return Meteorological parameter option item.
   */
  std::shared_ptr<MeteoParameterOptionItem> get_meteo_parameter_options(
      const std::string& name) const;

 private:
  int debug_level;
  MeteoParameterOptions options_map;
  MeteoParameterOptionItem default_option_item;
};

}  // namespace WFS
}  // namespace Plugin
}  // namespace SmartMet
