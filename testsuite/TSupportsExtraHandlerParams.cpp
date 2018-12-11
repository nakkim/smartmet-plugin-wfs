#include <iostream>
#include <boost/test/included/unit_test.hpp>
#include <newbase/NFmiPoint.h>
#include "ArrayParameterTemplate.h"
#include "SupportsExtraHandlerParams.h"

using namespace boost::unit_test;

test_suite* init_unit_test_suite(int argc, char* argv[])
{
  const char* name = "SupportsExtraHandlerParams tester";
  unit_test_log.set_threshold_level(log_messages);
  framework::master_test_suite().p_name.value = name;
  BOOST_TEST_MESSAGE("");
  BOOST_TEST_MESSAGE(name);
  BOOST_TEST_MESSAGE(std::string(std::strlen(name), '='));
  return NULL;
}

using SmartMet::Spine::Value;

namespace
{
class TestConfig : public libconfig::Config
{
 public:
  TestConfig()
  {
    using libconfig::Setting;
    auto& root = getRoot();
    root.add("disabled", Setting::TypeBoolean) = false;
    root.add("id", Setting::TypeString) = "foo";
    root.add("constructor_name", Setting::TypeString) = "create_foo";
    root.add("title", Setting::TypeGroup).add("eng", Setting::TypeString) = "Title";
    root.add("abstract", Setting::TypeGroup).add("eng", Setting::TypeString) = "Abstract";
    root.add("template", Setting::TypeString) = "foo.c2t";
    root.add("defaultLanguage", Setting::TypeString) = "eng";
    root.add("returnTypeNames", Setting::TypeArray).add(Setting::TypeString) = "fooRetVal";
    auto& p1 = root.add("parameters", Setting::TypeList);
    auto& p11 = p1.add(Setting::TypeGroup);
    p11.add("name", Setting::TypeString) = "bbox";
    p11.add("title", Setting::TypeGroup).add("eng", Setting::TypeString) = "title";
    p11.add("abstract", Setting::TypeGroup).add("eng", Setting::TypeString) = "abstract";
    p11.add("xmlType", Setting::TypeString) = "testType";
    p11.add("type", Setting::TypeString) = "double[4]";
    auto& p12 = p1.add(Setting::TypeGroup);
    p12.add("name", Setting::TypeString) = "foo";
    p12.add("title", Setting::TypeGroup).add("eng", Setting::TypeString) = "foo";
    p12.add("abstract", Setting::TypeGroup).add("eng", Setting::TypeString) = "foo";
    p12.add("xmlType", Setting::TypeString) = "string";
    p12.add("type", Setting::TypeString) = "string";

    auto& handler_params = root.add("handler_params", Setting::TypeGroup);
    handler_params.add("boundingBox", Setting::TypeArray).add(Setting::TypeString) =
        "${bbox>defaultBbox}";

    auto& named_params = root.add("named_params", Setting::TypeList);
    auto& hp1 = named_params.add(Setting::TypeGroup);
    hp1.add("name", Setting::TypeString) = "defaultBbox";
    auto& hp1v = hp1.add("def", Setting::TypeArray);
    hp1v.add(Setting::TypeFloat) = 20.0;
    hp1v.add(Setting::TypeFloat) = 60.0;
    hp1v.add(Setting::TypeFloat) = 25.0;
    hp1v.add(Setting::TypeFloat) = 65.0;
    auto& hp2 = named_params.add(Setting::TypeGroup);
    hp2.add("name", Setting::TypeString) = "foo";
    hp2.add("def", Setting::TypeString) = "${foo : bar}";
  }
};

boost::shared_ptr<libconfig::Config> create_config()
{
  boost::shared_ptr<libconfig::Config> config(new TestConfig);
  return config;
}

void log_exception(const char* file, int line)
{
  try
  {
    throw;
  }
  catch (const std::exception& err)
  {
    std::cerr << "C++ exception of type '" << Fmi::current_exception_type() << "' catched at "
              << file << '#' << line << ": " << err.what() << std::endl;
  }
  catch (...)
  {
    std::cerr << "C++ exception of type '" << Fmi::current_exception_type() << "' catched  at "
              << file << '#' << line << std::endl;
  }
}

template <typename ValueType>
void add(std::multimap<std::string, Value>& param_map, const std::string& name, ValueType value)
{
  Value tmp(value);
  param_map.insert(std::make_pair(name, tmp));
}
}

#define LOG_EXCEPTION(x)               \
  try                                  \
  {                                    \
    x;                                 \
  }                                    \
  catch (...)                          \
  {                                    \
    log_exception(__FILE__, __LINE__); \
    throw;                             \
  }

BOOST_AUTO_TEST_CASE(test_parameters_redirection_1)
{
  using namespace SmartMet;
  using namespace SmartMet::Plugin::WFS;

  auto raw_config = create_config();
  boost::shared_ptr<StoredQueryConfig> config;
  BOOST_REQUIRE_NO_THROW(config.reset(new StoredQueryConfig(raw_config)));

  // config->dump_config(std::cout, *raw_config);
  // std::cout << std::endl;

  boost::shared_ptr<ArrayParameterTemplate> pt;
  BOOST_REQUIRE_NO_THROW(
      LOG_EXCEPTION(pt.reset(new ArrayParameterTemplate(*config, "boundingBox", 4, 4))));

  const auto& items = pt->get_items();
  BOOST_REQUIRE_EQUAL(1, (int)items.size());
  const auto& item1 = items.at(0);
  BOOST_CHECK(not item1.absent);
  BOOST_CHECK(not item1.plain_text);
  BOOST_REQUIRE(item1.param_ref);
  BOOST_CHECK_EQUAL(std::string("bbox"), *item1.param_ref);
  BOOST_CHECK(not item1.param_ind);
  BOOST_CHECK(not item1.default_value);
  BOOST_CHECK(item1.redirect_name);
  BOOST_CHECK_EQUAL(std::string("defaultBbox"), *item1.redirect_name);

  boost::shared_ptr<SupportsExtraHandlerParams> extra_params;
  BOOST_REQUIRE_NO_THROW(LOG_EXCEPTION(
      extra_params.reset(new SupportsExtraHandlerParams(config, true, "named_params"))));

  std::multimap<std::string, Value> param_map;

  std::vector<double> bbox;
  BOOST_REQUIRE_NO_THROW(bbox = pt->get_double_array(param_map, extra_params.get()));
  BOOST_REQUIRE_EQUAL(4, (int)bbox.size());
  BOOST_REQUIRE_CLOSE(20.0, bbox.at(0), 1e-10);
  BOOST_REQUIRE_CLOSE(60.0, bbox.at(1), 1e-10);
  BOOST_REQUIRE_CLOSE(25.0, bbox.at(2), 1e-10);
  BOOST_REQUIRE_CLOSE(65.0, bbox.at(3), 1e-10);

  add(param_map, "bbox", 21.0);
  add(param_map, "bbox", 61.0);
  add(param_map, "bbox", 22.0);
  add(param_map, "bbox", 62.0);
  BOOST_REQUIRE_NO_THROW(bbox = pt->get_double_array(param_map, extra_params.get()));
  BOOST_REQUIRE_EQUAL(4, (int)bbox.size());
  BOOST_REQUIRE_CLOSE(21.0, bbox.at(0), 1e-10);
  BOOST_REQUIRE_CLOSE(61.0, bbox.at(1), 1e-10);
  BOOST_REQUIRE_CLOSE(22.0, bbox.at(2), 1e-10);
  BOOST_REQUIRE_CLOSE(62.0, bbox.at(3), 1e-10);
}
