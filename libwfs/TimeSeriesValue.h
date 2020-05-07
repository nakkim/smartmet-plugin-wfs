#pragma ones

#include <boost/date_time/posix_time/posix_time.hpp>
#include <macgyver/StringConversion.h>
#include <spine/Exception.h>
#include <spine/TimeSeries.h>

namespace SmartMet
{
namespace Plugin
{
namespace WFS
{
class TimeSeriesValue
{
 public:
  typedef SmartMet::Spine::TimeSeries::LonLat LonLat;
  typedef SmartMet::Spine::TimeSeries::Value Value;

  TimeSeriesValue() : m_missingValue(32700.0), m_missingText("NaN") {}
  virtual ~TimeSeriesValue() {}
  void setMissingTextForValue(const std::string& text, const double& value)
  {
    m_missingText = text;
    m_missingValue = value;
  }

  const std::string toString(const SmartMet::Spine::Value& value, const uint& precision = 0)
  {
    try
    {
      if (const double* val = boost::get<double>(&value))
      {
        std::ostringstream out;
        if (*val == m_missingValue)
          out << m_missingText;
        else
          out << std::setprecision(precision) << std::fixed << *val;
        return out.str();
      }
      else if (const int* val = boost::get<int>(&value))
      {
        if (*val == static_cast<int>(m_missingValue))
          return m_missingText;
        else
          return std::to_string(*val);
      }
      else if (const std::string* val = boost::get<std::string>(&value))
      {
        if (val->empty())
          return m_missingText;
        else
          return *val;
      }
      else if (const LonLat* val = boost::get<LonLat>(&value))
      {
        std::ostringstream out;
        if (val->lon == m_missingValue or val->lat == m_missingValue)
          out << m_missingText << " " << m_missingText;
        else
          out << std::setprecision(precision) << std::fixed << val->lon << " " << val->lat;

        return out.str();
      }
      else if (const boost::local_time::local_date_time* val =
                   boost::get<boost::local_time::local_date_time>(&value))
      {
        return Fmi::to_iso_string(val->utc_time());
      }
      else
      {
        std::ostringstream msg;
        msg << "TimeSeriesValue::toString : unknown valuetype";
        throw SmartMet::Spine::Exception(BCP, msg.str());
      }
    }
    catch (...)
    {
      throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
    }
  }

 private:
  TimeSeriesValue& operator=(const TimeSeriesValue& value);
  TimeSeriesValue(const TimeSeriesValue& value);

  double m_missingValue;
  std::string m_missingText;
};

}  // namespace WFS
}  // namespace Plugin
}  // namespace SmartMet
