#include "StoredQueryParamRegistry.h"
#include "ArrayParameterTemplate.h"
#include "ScalarParameterTemplate.h"
#include "SupportsExtraHandlerParams.h"
#include <boost/foreach.hpp>
#include <macgyver/StringConversion.h>
#include <macgyver/TypeName.h>
#include <spine/Exception.h>
#include <algorithm>
#include <sstream>

namespace bw = SmartMet::Plugin::WFS;

using bw::StoredQueryParamRegistry;
using SmartMet::Spine::Value;

namespace
{
enum TypeInd
{
  P_INT = 1,
  P_UINT,
  P_DOUBLE,
  P_STRING,
  P_TIME,
  P_POINT,
  P_BBOX,
  P_BOOL
};
}

StoredQueryParamRegistry::StoredQueryParamRegistry(StoredQueryConfig::Ptr config)
    : bw::StoredQueryConfig::Wrapper(config)
{
  try
  {
    supported_type_names[typeid(int64_t).name()] = P_INT;
    supported_type_names[typeid(uint64_t).name()] = P_UINT;
    supported_type_names[typeid(double).name()] = P_DOUBLE;
    supported_type_names[typeid(std::string).name()] = P_STRING;
    supported_type_names[typeid(boost::posix_time::ptime).name()] = P_TIME;
    supported_type_names[typeid(SmartMet::Spine::Point).name()] = P_POINT;
    supported_type_names[typeid(SmartMet::Spine::BoundingBox).name()] = P_BBOX;
    supported_type_names[typeid(bool).name()] = P_BOOL;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

StoredQueryParamRegistry::~StoredQueryParamRegistry() {}

boost::shared_ptr<bw::RequestParameterMap> StoredQueryParamRegistry::process_parameters(
    const bw::RequestParameterMap& src, const SupportsExtraHandlerParams* extra_params) const
{
  try
  {
    boost::shared_ptr<bw::RequestParameterMap> result(new bw::RequestParameterMap);

    BOOST_FOREACH (const auto& map_item, param_map)
    {
      const std::string& name = map_item.first;
      int type_ind = supported_type_names.at(map_item.second->type_name);
      if (typeid(*map_item.second) == typeid(ScalarParameterRec))
      {
        const ScalarParameterRec& rec = dynamic_cast<ScalarParameterRec&>(*map_item.second);

        SmartMet::Spine::Value value;
        if (rec.required)
        {
          value = rec.param_def->get_value(src, extra_params);
        }
        else if (not rec.param_def->get_value(value, src, extra_params))
        {
          // Optional scalar parameter ommited.
          continue;
        }

        switch (type_ind)
        {
          case P_INT:
            result->add(name, value.get_int());
            break;

          case P_UINT:
            result->add(name, value.get_uint());
            break;

          case P_DOUBLE:
            result->add(name, value.get_double());
            break;

          case P_STRING:
            result->add(name, value.get_string());
            break;

          case P_TIME:
            result->add(name, value.get_ptime(true));
            break;

          case P_POINT:
            result->add(name, value.get_point());
            break;

          case P_BBOX:
            result->add(name, value.get_bbox());
            break;

          case P_BOOL:
            result->add(name, value.get_bool());
            break;

          default:
            // Not supposed to happen
            throw SmartMet::Spine::Exception(BCP,
                                             "INTERNAL ERROR at line " + Fmi::to_string(__LINE__));
        }
      }
      else if (typeid(*map_item.second) == typeid(ArrayParameterRec))
      {
        const ArrayParameterRec& rec = dynamic_cast<ArrayParameterRec&>(*map_item.second);

        std::vector<SmartMet::Spine::Value> values = rec.param_def->get_value(src, extra_params);
        if (values.size() < rec.min_size or values.size() > rec.max_size)
        {
          std::ostringstream msg;
          msg << "Number of the values '" << values.size() << "' for the parameter '" << name
              << "' is out of the range [" << rec.min_size << ".." << rec.max_size << "]!";
          throw SmartMet::Spine::Exception(BCP, msg.str());
        }
        if (rec.step > 1 and ((values.size() - rec.min_size) % rec.step != 0))
        {
          int cnt = 1 + (rec.max_size - rec.min_size) / rec.step;
          std::ostringstream msg;
          msg << "Number of the values '" << values.size() << "' for the parameter '" << name
              << "' is invalid!";
          switch (cnt)
          {
            case 1:
              msg << " (" << rec.min_size << " values expected)";
              break;

            case 2:
            case 3:
            case 4:
            case 5:
              msg << " (";
              for (int i = 0; i < cnt; i++)
                msg << (i == 0 ? "" : i == cnt - 1 ? " or " : ", ") << rec.min_size + i * rec.step;
              msg << " values expected)";
              break;

            default:
              msg << " (" << rec.min_size << ", " << rec.min_size + rec.step << ",... , "
                  << rec.min_size + (cnt - 1) * rec.step << " values expected)";
              break;
          }

          throw SmartMet::Spine::Exception(BCP, msg.str());
        }

        BOOST_FOREACH (const SmartMet::Spine::Value& value, values)
        {
          switch (type_ind)
          {
            case P_INT:
              result->add(name, value.get_int());
              break;

            case P_UINT:
              result->add(name, value.get_uint());
              break;

            case P_DOUBLE:
              result->add(name, value.get_double());
              break;

            case P_STRING:
              result->add(name, value.get_string());
              break;

            case P_TIME:
              result->add(name, value.get_ptime(true));
              break;

            default:
              // Not supposed to happen
              throw SmartMet::Spine::Exception(
                  BCP, "INTERNAL ERROR at line " + Fmi::to_string(__LINE__));
          }
        }
      }
      else
      {
        // Not supposed to happen
        throw SmartMet::Spine::Exception(BCP, "INTERNAL ERROR at line " + Fmi::to_string(__LINE__));
      }
    }

    return result;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

std::set<std::string> StoredQueryParamRegistry::get_param_names() const
{
  try
  {
    std::set<std::string> result;
    std::transform(
        param_map.begin(),
        param_map.end(),
        std::inserter(result, result.begin()),
        boost::bind(&std::pair<const std::string, boost::shared_ptr<ParamRecBase> >::first, ::_1));
    return result;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void StoredQueryParamRegistry::register_scalar_param(
    const std::string& name, boost::shared_ptr<ScalarParameterTemplate> param_def, bool required)
{
  try
  {
    boost::shared_ptr<ScalarParameterRec> rec(new ScalarParameterRec);
    rec->name = name;
    rec->param_def = param_def;
    rec->type_name = typeid(std::string).name();
    rec->required = required;
    add_param_rec(rec);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

/* Full scope here only to satisfy Doxygen */
void SmartMet::Plugin::WFS::StoredQueryParamRegistry::register_array_param(
    const std::string& name,
    boost::shared_ptr<ArrayParameterTemplate> param_def,
    std::size_t min_size,
    std::size_t max_size)
{
  try
  {
    boost::shared_ptr<ArrayParameterRec> rec(new ArrayParameterRec);
    rec->name = name;
    rec->param_def = param_def;
    rec->type_name = typeid(std::string).name();
    rec->min_size = min_size;
    rec->max_size = max_size, rec->step = 1;
    add_param_rec(rec);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void StoredQueryParamRegistry::add_param_rec(boost::shared_ptr<ParamRecBase> rec)
{
  try
  {
    using Fmi::demangle_cpp_type_name;

    // Verify that template argument value is supported
    const std::string& type_name = rec->type_name;
    if (supported_type_names.count(type_name) == 0)
    {
      std::string sep = "";
      std::ostringstream msg;
      std::ostringstream msg2;
      msg << "Not supported type '" << demangle_cpp_type_name(type_name) << "' "
          << "['" << type_name << "']";

      msg2 << "Use one of ";
      BOOST_FOREACH (const auto& map_item, supported_type_names)
      {
        msg2 << sep << '\'' << demangle_cpp_type_name(map_item.first) << '\'';
        sep = ", ";
      }
      SmartMet::Spine::Exception exception(BCP, msg.str());
      exception.addDetail(msg2.str());
      throw exception;
    }

    const std::string& name = rec->name;
    if (not param_map.insert(std::make_pair(name, rec)).second)
    {
      throw SmartMet::Spine::Exception(BCP, "Duplicate parameter name '" + name + "'!");
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}
