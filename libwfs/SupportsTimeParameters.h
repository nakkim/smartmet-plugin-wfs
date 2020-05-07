#pragma once

#include "ArrayParameterTemplate.h"
#include "ScalarParameterTemplate.h"
#include "StoredQueryConfig.h"
#include "StoredQueryParamRegistry.h"
#include "SupportsExtraHandlerParams.h"
#include <boost/shared_ptr.hpp>
#include <spine/TimeSeriesGenerator.h>
#include <map>
#include <string>
#include <vector>

namespace SmartMet
{
namespace Plugin
{
namespace WFS
{
/**
 *   @brief Base class for implementing time related stored queries
 *          parameter support
 */
class SupportsTimeParameters : protected virtual SupportsExtraHandlerParams,
                               protected virtual StoredQueryParamRegistry
{
 public:
  SupportsTimeParameters(boost::shared_ptr<SmartMet::Plugin::WFS::StoredQueryConfig> config);

  virtual ~SupportsTimeParameters();

  boost::shared_ptr<SmartMet::Spine::TimeSeriesGeneratorOptions> get_time_generator_options(
      const RequestParameterMap &param_values) const;

 private:
  int debug_level;

 protected:
  static const char *P_HOURS;
  static const char *P_TIMES;
  static const char *P_BEGIN_TIME;
  static const char *P_START_STEP;
  static const char *P_END_TIME;
  static const char *P_TIME_STEP;
  static const char *P_NUM_STEPS;
};

}  // namespace WFS
}  // namespace Plugin
}  // namespace SmartMet
