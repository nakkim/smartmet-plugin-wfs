#include "SupportsTimeParameters.h"
#include "WfsConvenience.h"
#include "WfsException.h"
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/foreach.hpp>
#include <boost/optional/optional_io.hpp>
#include <spine/Exception.h>
#include <stdint.h>

namespace bw = SmartMet::Plugin::WFS;
namespace pt = boost::posix_time;

const char *bw::SupportsTimeParameters::P_HOURS = "hours";
const char *bw::SupportsTimeParameters::P_TIMES = "times";
const char *bw::SupportsTimeParameters::P_BEGIN_TIME = "beginTime";
const char *bw::SupportsTimeParameters::P_START_STEP = "startStep";
const char *bw::SupportsTimeParameters::P_END_TIME = "endTime";
const char *bw::SupportsTimeParameters::P_TIME_STEP = "timeStep";
const char *bw::SupportsTimeParameters::P_NUM_STEPS = "timeSteps";

bw::SupportsTimeParameters::SupportsTimeParameters(boost::shared_ptr<bw::StoredQueryConfig> config)
    : SupportsExtraHandlerParams(config, false), debug_level(config->get_debug_level())
{
  try
  {
    register_array_param<uint64_t>(P_HOURS, *config);
    register_array_param<uint64_t>(P_TIMES, *config);
    register_scalar_param<pt::ptime>(P_BEGIN_TIME, *config, false);
    register_scalar_param<uint64_t>(P_START_STEP, *config, false);
    register_scalar_param<pt::ptime>(P_END_TIME, *config, false);
    register_scalar_param<uint64_t>(P_TIME_STEP, *config, false);
    register_scalar_param<uint64_t>(P_NUM_STEPS, *config, false);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

bw::SupportsTimeParameters::~SupportsTimeParameters() {}

boost::shared_ptr<SmartMet::Spine::TimeSeriesGeneratorOptions>
bw::SupportsTimeParameters::get_time_generator_options(const RequestParameterMap &param) const
{
  try
  {
    boost::shared_ptr<SmartMet::Spine::TimeSeriesGeneratorOptions> options(
        new SmartMet::Spine::TimeSeriesGeneratorOptions);

    options->endTimeUTC = true;
    options->startTimeUTC = true;

    std::vector<unsigned> hours;
    param.get<uint64_t>(P_HOURS, std::back_inserter(hours));
    if (not hours.empty())
    {
      options->mode = SmartMet::Spine::TimeSeriesGeneratorOptions::FixedTimes;
      BOOST_FOREACH (int64_t hour, hours)
      {
        if ((hour >= 0) and (hour <= 23))
        {
          options->timeList.insert(hour * 100);
        }
        else
        {
          SmartMet::Spine::Exception exception(BCP, "Invalid hour value!");
          exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PARSING_FAILED);
          exception.addParameter("Requested hour", std::to_string(hour));
          throw exception;
        }
      }
    }

    std::vector<unsigned> times;
    param.get<uint64_t>(P_TIMES, std::back_inserter(times));
    if (not times.empty())
    {
      options->mode = SmartMet::Spine::TimeSeriesGeneratorOptions::FixedTimes;
      BOOST_FOREACH (int64_t tm, times)
      {
        int h = tm / 100;
        int m = tm % 100;
        if ((h >= 0) and (h <= 23) and (m >= 0) and (m <= 59))
        {
          options->timeList.insert(tm);
        }
        else
        {
          SmartMet::Spine::Exception exception(BCP, "Invalid time value!");
          exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PARSING_FAILED);
          exception.addParameter("Requested time", std::to_string(tm));
          throw exception;
        }
      }
    }

    boost::optional<uint64_t> time_step = param.get_optional<uint64_t>(P_TIME_STEP);
    if (time_step)
    {
      if (options->mode != SmartMet::Spine::TimeSeriesGeneratorOptions::TimeSteps)
      {
        SmartMet::Spine::Exception exception(BCP, "Cannot use timestep option in this time mode!");
        exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PARSING_FAILED);
        throw exception;
      }

      if (*time_step == 0)
      {
        options->mode = SmartMet::Spine::TimeSeriesGeneratorOptions::DataTimes;
      }
      else if (1440 % *time_step != 0)
      {
        SmartMet::Spine::Exception exception(BCP, "Invalid time step value!");
        exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PARSING_FAILED);
        exception.addParameter("Time step", std::to_string(*time_step));
        throw exception;
      }
      else
      {
        options->timeStep = *time_step;
        if (debug_level > 3)
          std::cout << __FILE__ << "#" << __LINE__ << ": timestep=" << options->timeStep
                    << std::endl;
      }
    }

    boost::optional<uint64_t> num_steps = param.get_optional<uint64_t>(P_NUM_STEPS);
    if (num_steps)
    {
      if ((*num_steps > 0) and
          (*num_steps <= static_cast<uint64_t>(std::numeric_limits<int>::max())))
      {
        options->timeSteps = num_steps;
      }
      else
      {
        SmartMet::Spine::Exception exception(BCP, "Invalid number of time steps!");
        exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PARSING_FAILED);
        exception.addParameter("Time steps", std::to_string(*num_steps));
        throw exception;
      }
    }

    if (not options->timeStep)
    {
      options->timeStep = 60;
      if (debug_level > 3)
        std::cout << __FILE__ << "#" << __LINE__ << ": timestep=" << options->timeStep << std::endl;
    }

    if (param.count(P_BEGIN_TIME))  // using boost::optional<> cause compiler warning on RHEL6
    {
      options->startTime = param.get_single<pt::ptime>(P_BEGIN_TIME);
      if (debug_level > 2)
        std::cout << __FILE__ << "#" << __LINE__ << ": startTime=" << options->startTime
                  << std::endl;
    }

    boost::optional<uint64_t> start_step = param.get_optional<uint64_t>(P_START_STEP);
    if (start_step)
    {
      if (*start_step > 10000)
      {
        SmartMet::Spine::Exception exception(BCP, "Invalid start step value!");
        exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PARSING_FAILED);
        exception.addParameter("Start step", std::to_string(*start_step));
        throw exception;
      }

      options->startTime += pt::minutes(*start_step * *options->timeStep);
      if (debug_level > 2)
        std::cout << __FILE__ << "#" << __LINE__
                  << ": startTime=" << pt::to_simple_string(options->startTime) << std::endl;
    }

    if (param.count(P_END_TIME))
    {
      if (!!options->timeSteps)
      {
        SmartMet::Spine::Exception exception(
            BCP, "Cannot specify the time steps count and the end time simultaneusly!");
        exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PARSING_FAILED);
        throw exception;
      }

      options->endTime = param.get_single<pt::ptime>(P_END_TIME);
      if (debug_level > 2)
        std::cout << __FILE__ << "#" << __LINE__
                  << ": endTime=" << pt::to_simple_string(options->endTime) << std::endl;
    }
    else if (!!options->timeSteps)
    {
      options->endTime = options->startTime + pt::minutes(*options->timeStep * *options->timeSteps);
      if (debug_level > 2)
        std::cout << __FILE__ << "#" << __LINE__
                  << ": endTime=" << pt::to_simple_string(options->endTime) << std::endl;
    }
    else
    {
      options->endTime = options->startTime + pt::hours(24);
      if (debug_level > 2)
        std::cout << __FILE__ << "#" << __LINE__
                  << ": endTime=" << pt::to_simple_string(options->endTime) << std::endl;
    }

    return options;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

/**

@page WFS_SQ_TIME_PARAMS Time related parameters

@section WFS_SQ_TIME_PARAMS_INTRO Introduction

Stored query handler time parameter group contains parameters which characterizes
- the time interval
- specific time values to use
- the time step for data selection and number of time steps

Time related parameter support is implemented in C++ class
SmartMet::Plugin::WFS::SupportsTimeParameters .

The actual generation of time moments to use is implemented in C++ class
SmartMet::Spine::TimeSeriesGenerator


@section WFS_SQ_TIME_PARAMS_PARAMS The stored query handler parameters of this group

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
  <td>The time of the begin of the time interval</td>
</tr>

<tr>
  <td>endTime</td>
  <td>@ref WFS_CFG_SCALAR_PARAM_TMPL</td>
  <td>time</td>
  <td>The end time of the time interval</td>
</tr>

<tr>
  <td>hours</td>
  <td>@ref WFS_CFG_ARRAY_PARAM_TMPL</td>
  <td>unsigned integer</td>
  <td>Array of hours for which to provide data. For example specifying
      @verbatim
      hours = [0, 3, 6, 9, 12, 15, 18, 21];
      @endverbatim
      requests to provided data for each third hour
</tr>

<tr>
  <td>times</td>
  <td>@ref WFS_CFG_ARRAY_PARAM_TMPL</td>
  <td>unsigned integer</td>
  <td>Similar as @b hours, but values are in format @b HHMM , where HH is hours
      and MM is minutes</td>
</tr>

<tr>
  <td>startStep</td>
  <td>@ref WFS_CFG_SCALAR_PARAM_TMPL</td>
  <td>unsigned integer</td>
  <td>Specifies for which step to begin. For example 3 means that first 2 steps will be skipped</td>
</tr>

<tr>
  <td>timeStep</td>
  <td>@ref WFS_CFG_SCALAR_PARAM_TMPL</td>
  <td>unsigned integer</td>
  <td>Specifies time step in minutes. SmartMet::Spine::Value 0 means to provide all available
data</td>
</tr>

<tr>
  <td>timeSteps</td>
  <td>@ref WFS_CFG_SCALAR_PARAM_TMPL</td>
  <td>unsigned integer</td>
  <td>Specifies for how many time steps to provide data</td>
</tr>

</table>

@section WFS_SQ_TIME_PARAMS_EXAPLES Examples

@verbatim
hours = [];
times = [];
timeSteps = "${}";
beginTime = "${starttime: now}";
endTime = "${endtime: after 36 hours}";
timeStep = "${timestep: 60}";
@endverbatim

 */
