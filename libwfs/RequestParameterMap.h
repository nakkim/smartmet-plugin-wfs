#pragma once

#include <ctpp2/CDT.hpp>
#include <macgyver/TypeName.h>
#include <spine/Exception.h>
#include <spine/Value.h>
#include <iterator>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>

namespace SmartMet
{
namespace Plugin
{
namespace WFS
{
class RequestParameterMap
{
  std::multimap<std::string, SmartMet::Spine::Value> params;

 public:
  RequestParameterMap();

  RequestParameterMap(const std::multimap<std::string, SmartMet::Spine::Value>& params);

  virtual ~RequestParameterMap();

  inline const std::multimap<std::string, SmartMet::Spine::Value>& get_map() const
  {
    return params;
  }
  void clear();

  std::size_t size() const;

  std::size_t count(const std::string& name) const;

  void insert_value(const std::string& name, const SmartMet::Spine::Value& value);

  std::set<std::string> get_keys() const;

  std::vector<SmartMet::Spine::Value> get_values(const std::string& name) const;

  void dump_params(CTPP::CDT& hash) const;

  template <typename ValueType>
  void add(const std::string& name, const ValueType& value, bool replace = false);

  template <typename IteratorType>
  void add(const std::string& name, IteratorType begin, IteratorType end, bool replace = false);

  template <typename IteratorType, typename Member>
  void extract_to(const std::string& name,
                  IteratorType begin,
                  IteratorType end,
                  Member member,
                  bool replace = false);

  void remove(const std::string& name);

  template <typename ContainerType>
  void add_vect(const std::string& name, const ContainerType& container, bool replace = false);

  template <typename ValueType, typename OutputIterator>
  std::size_t get(const std::string& name,
                  OutputIterator output,
                  std::size_t min_size = 0,
                  std::size_t max_size = std::numeric_limits<std::size_t>::max(),
                  std::size_t step = 0) const;

  template <typename ResultType, typename ValueType = ResultType>
  ResultType get_single(const std::string& name) const;

  template <typename ResultType, typename ValueType = ResultType>
  boost::optional<ResultType> get_optional(const std::string& name) const;

  template <typename ResultType, typename ValueType = ResultType>
  ResultType get_optional(const std::string& name, const ResultType& default_value) const;

  std::string as_string() const;

  std::string subst(const std::string& src) const;
};

template <typename ValueType>
void RequestParameterMap::add(const std::string& name, const ValueType& value, bool replace)
{
  try
  {
    if (replace)
      params.erase(name);
    params.insert(std::make_pair(name, SmartMet::Spine::Value(value)));
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

template <typename IteratorType>
void RequestParameterMap::add(const std::string& name,
                              IteratorType begin,
                              IteratorType end,
                              bool replace)
{
  try
  {
    typedef typename std::iterator_traits<IteratorType>::value_type SourceType;

    if (replace)
      params.erase(name);
    while (begin != end)
    {
      const SourceType& tmp = *begin++;
      params.insert(std::make_pair(name, SmartMet::Spine::Value(tmp)));
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

template <typename IteratorType, typename Member>
void RequestParameterMap::extract_to(
    const std::string& name, IteratorType begin, IteratorType end, Member member, bool replace)
{
  try
  {
    typedef typename std::iterator_traits<IteratorType>::value_type SourceType;

    if (replace)
      params.erase(name);
    while (begin != end)
    {
      const SourceType& tmp = *begin++;
      params.insert(std::make_pair(name, SmartMet::Spine::Value(tmp.*member)));
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

template <typename ContainerType>
void RequestParameterMap::add_vect(const std::string& name,
                                   const ContainerType& container,
                                   bool replace)
{
  try
  {
    add(name, container.begin(), container.end(), replace);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

template <typename ValueType, typename OutputIteratorType>
std::size_t RequestParameterMap::get(const std::string& name,
                                     OutputIteratorType output,
                                     std::size_t min_size,
                                     std::size_t max_size,
                                     std::size_t step) const
{
  try
  {
    std::size_t cnt = 0;
    auto range = params.equal_range(name);
    while (range.first != range.second)
    {
      if (++cnt > max_size)
      {
        std::ostringstream msg;
        msg << "SmartMet::Plugin::WFS::RequestParameterMap::get<"
            << Fmi::demangle_cpp_type_name(typeid(ValueType).name()) << ">: too many ("
            << params.count(name) << ") values for"
            << " parameter '" << name << "' ("
            << (min_size == max_size ? "exactly " : "no more than ") << max_size << " required)";
        throw SmartMet::Spine::Exception(BCP, msg.str());
      }
      const auto& item = *range.first++;
      const SmartMet::Spine::Value& value = item.second;
      ValueType tmp = value.get<ValueType>();
      *output++ = tmp;
    }

    if (cnt < min_size)
    {
      std::ostringstream msg;
      msg << "SmartMet::Plugin::WFS::RequestParameterMap::get<"
          << Fmi::demangle_cpp_type_name(typeid(ValueType).name()) << ">: too few ("
          << params.count(name) << ") values for"
          << " parameter '" << name << "' (" << (min_size == max_size ? "exactly " : "at least ")
          << min_size << " required)";
      throw SmartMet::Spine::Exception(BCP, msg.str());
    }

    if ((step > 1) and (cnt % step != 0))
    {
      std::ostringstream msg;
      msg << "SmartMet::Plugin::WFS::RequestParameterMap::get<"
          << Fmi::demangle_cpp_type_name(typeid(ValueType).name()) << ">: incorrect number ("
          << params.count(name) << ") values for"
          << " parameter '" << name << "' (full groups of " << step << " values required)";
      throw SmartMet::Spine::Exception(BCP, msg.str());
    }

    return cnt;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

template <typename ResultType, typename ValueType>
ResultType RequestParameterMap::get_single(const std::string& name) const
{
  try
  {
    std::vector<ValueType> tmp;
    get<ValueType>(name, std::back_inserter(tmp), 1, 1);
    return tmp.at(0);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

template <typename ResultType, typename ValueType>
boost::optional<ResultType> RequestParameterMap::get_optional(const std::string& name) const
{
  try
  {
    boost::optional<ResultType> result;
    std::vector<ValueType> tmp;
    get<ValueType>(name, std::back_inserter(tmp), 0, 1);
    if (not tmp.empty())
    {
      result = tmp.at(0);
    }
    return result;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

template <typename ResultType, typename ValueType>
ResultType RequestParameterMap::get_optional(const std::string& name,
                                             const ResultType& default_value) const
{
  try
  {
    std::vector<ValueType> tmp;
    get<ValueType>(name, std::back_inserter(tmp), 0, 1);
    if (tmp.empty())
    {
      return default_value;
    }
    else
    {
      return tmp.at(0);
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

inline std::ostream& operator<<(std::ostream& output, const RequestParameterMap& params)
{
  try
  {
    output << params.as_string();
    return output;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace WFS
}  // namespace Plugin
}  // namespace SmartMet
