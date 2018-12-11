#pragma once

#include <libconfig.h++>
#include <spine/Value.h>
#include "StoredQueryConfig.h"
#include "RequestParameterMap.h"

namespace Test
{
using ::libconfig::Setting;
using SmartMet::Spine::Value;
using SmartMet::Plugin::WFS::RequestParameterMap;

class ParamProxy
{
 public:
  ParamProxy(Setting& param) : param(param) {}
  ParamProxy& min_occurs(int i)
  {
    param.add("minOccurs", Setting::TypeInt) = i;
    return *this;
  }

  ParamProxy& max_occurs(int i)
  {
    param.add("maxOccurs", Setting::TypeInt) = i;
    return *this;
  }

  ParamProxy& lower_limit(const std::string& text)
  {
    param.add("lowerLimit", Setting::TypeString) = text;
    return *this;
  }

  ParamProxy& upper_limit(const std::string& text)
  {
    param.add("lowerLimit", Setting::TypeString) = text;
    return *this;
  }

  libconfig::Setting& get() const { return param; }
 private:
  libconfig::Setting& param;
};

class TestConfig : public libconfig::Config
{
  libconfig::Setting* param_list;
  libconfig::Setting* param_root;

 public:
  TestConfig(const std::string& id = "foo")
  {
    auto& root = getRoot();
    root.add("disabled", Setting::TypeBoolean) = false;
    root.add("id", Setting::TypeString) = "foo";
    root.add("constructor_name", Setting::TypeString) = "create_foo";
    root.add("title", Setting::TypeGroup).add("eng", Setting::TypeString) = "Title";
    root.add("abstract", Setting::TypeGroup).add("eng", Setting::TypeString) = "Abstract";
    root.add("template", Setting::TypeString) = "foo.c2t";
    root.add("defaultLanguage", Setting::TypeString) = "eng";
    root.add("returnTypeNames", Setting::TypeArray).add(Setting::TypeString) = "fooRetVal";
    param_list = &root.add("parameters", Setting::TypeList);

    param_root = &root.add("handler_params", Setting::TypeGroup);
  }

  static boost::shared_ptr<TestConfig> create(const std::string& id = "foo")
  {
    return boost::shared_ptr<TestConfig>(new TestConfig(id));
  }

  ParamProxy add_param(const std::string& name,
                       const std::string& xml_type,
                       const std::string& type)
  {
    auto& p11 = param_list->add(Setting::TypeGroup);
    p11.add("name", Setting::TypeString) = name;
    p11.add("title", Setting::TypeGroup).add("eng", Setting::TypeString) = "title";
    p11.add("abstract", Setting::TypeGroup).add("eng", Setting::TypeString) = "abstract";
    p11.add("xmlType", Setting::TypeString) = xml_type;
    p11.add("type", Setting::TypeString) = type;
    return ParamProxy(p11);
  }

  void add_scalar(const std::string& name, const std::string& value)
  {
    param_root->add(name, Setting::TypeString) = value;
  }

  void add_array(const char* name, const std::string& item)
  {
    std::array<std::string, 1> items = {{item}};
    add_array<1>(name, items);
  }

  template <int N>
  void add_array(const char* name, const std::array<std::string, N>& items)
  {
    auto& p = param_root->add(name, Setting::TypeArray);
    for (int i = 0; i < N; i++)
      p.add(Setting::TypeString) = items[i];
  }
};

boost::shared_ptr<TestConfig> create_config()
{
  return boost::shared_ptr<TestConfig>(new TestConfig);
}

template <typename T, int N>
void add_values(RequestParameterMap& param_map,
                const std::string& name,
                const std::array<T, N>& values)
{
  for (int i = 0; i < N; i++)
    param_map.insert_value(name, Value(values[i]));
}
}
