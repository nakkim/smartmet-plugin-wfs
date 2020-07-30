#define BOOST_TEST_MODULE TStoredQueryConfig
#define BOOST_TEST_DYN_LINK 1
#include <boost/test/unit_test.hpp>
#include <array>
#include "ArrayParameterTemplate.h"
#include "RequestParameterMap.h"
#include "ScalarParameterTemplate.h"
#include "StoredQueryConfig.h"
#include "WfsException.h"
#include "ConfigBuild.h"

using namespace boost::unit_test;
using SmartMet::Plugin::WFS::RequestParameterMap;

test_suite* init_unit_test_suite(int argc, char* argv[])
{
  const char* name = "StoredQueryConfig tester (parameter support)";
  unit_test_log.set_threshold_level(log_messages);
  framework::master_test_suite().p_name.value = name;
  BOOST_TEST_MESSAGE("");
  BOOST_TEST_MESSAGE(name);
  BOOST_TEST_MESSAGE(std::string(std::strlen(name), '='));
  return NULL;
}

namespace
{
void log_error()
{
  try
  {
    throw;
  }
  catch (const std::exception& err)
  {
    std::cerr << "C++ exception of type " << Fmi::current_exception_type() << ": " << err.what()
              << std::endl;
    throw;
  }
  catch (...)
  {
    std::cerr << "C++ exception of type " << Fmi::current_exception_type();
    throw;
  }
}
}

#define NO_THROW(x)                      \
  try                                    \
  {                                      \
    x;                                   \
  }                                      \
  catch (...)                            \
  {                                      \
    BOOST_REQUIRE_NO_THROW(log_error()); \
  }

// This procedure is here only for putting breakpoint in it
void dummy_proc(const char* text)
{
  (void)text;
}
namespace bw = SmartMet::Plugin::WFS;
namespace pt = boost::posix_time;

using SmartMet::Spine::Value;
using libconfig::Setting;
using Test::TestConfig;
using Test::add_values;

typedef boost::shared_ptr<bw::StoredQueryConfig> StoredQueryConfigP;
typedef boost::shared_ptr<bw::ScalarParameterTemplate> ScalarParameterTemplateP;
typedef boost::shared_ptr<bw::ArrayParameterTemplate> ArrayParameterTemplateP;

#if 0
BOOST_AUTO_TEST_CASE(test_config_create)
{
  BOOST_TEST_MESSAGE("+ [Create test config]");

  boost::shared_ptr<TestConfig> raw_config(new TestConfig);

  raw_config->add_param("foo", "xxx", "string")
    .min_occurs(1)
    .max_occurs(1);

  raw_config->add_param("bar", "xxx", "string[0..4]")
    .min_occurs(0)
    .max_occurs(1);

  raw_config->add_scalar("foo", "${foo}");
  raw_config->add_array<3>("bar", {{"foo", "bar", "baz"}});

  //SmartMet::Spine::ConfigBase::dump_config(std::cout, *raw_config);
}
#endif

/****************************************************************************************/

BOOST_AUTO_TEST_CASE(test_single_scalar_parameter_without_default)
{
  dummy_proc(__PRETTY_FUNCTION__);
  BOOST_TEST_MESSAGE("+ [Single scalar parameter without default value]");

  boost::shared_ptr<TestConfig> raw_config(new TestConfig);

  raw_config->add_param("foo", "xxx", "int");
  raw_config->add_scalar("p", "${foo}");

  int val;
  RequestParameterMap param_map;
  StoredQueryConfigP config;
  ScalarParameterTemplateP param;
  // SmartMet::Spine::ConfigBase::dump_config(std::cout, *raw_config);
  BOOST_REQUIRE_NO_THROW(config.reset(new bw::StoredQueryConfig(raw_config, nullptr)));
  BOOST_REQUIRE_NO_THROW(param.reset(new bw::ScalarParameterTemplate(*config, "p")));

  // No mandatory parameter provided
  BOOST_REQUIRE_THROW(param->get<int64_t>(param_map), SmartMet::Spine::Exception);

  // Single parameter provided (must succeed)
  std::array<int, 1> i001 = {{123}};
  Test::add_values<int, 1>(param_map, "foo", i001);
  BOOST_REQUIRE_NO_THROW(val = param->get<int64_t>(param_map));
  BOOST_CHECK_EQUAL(123, val);

  // Too many parameters provided
  std::array<int, 1> i002 = {{321}};
  Test::add_values<int, 1>(param_map, "foo", i002);
  BOOST_REQUIRE_THROW(param->get<int64_t>(param_map), SmartMet::Spine::Exception);
}

/****************************************************************************************/

BOOST_AUTO_TEST_CASE(test_single_scalar_parameter_with_default_value)
{
  dummy_proc(__PRETTY_FUNCTION__);
  BOOST_TEST_MESSAGE("+ [Single scalar parameter with default value]");

  boost::shared_ptr<TestConfig> raw_config(new TestConfig);

  raw_config->add_param("foo", "xxx", "int");
  raw_config->add_scalar("p", "${foo : 512}");

  int val;
  RequestParameterMap param_map;
  StoredQueryConfigP config;
  ScalarParameterTemplateP param;
  // SmartMet::Spine::ConfigBase::dump_config(std::cout, *raw_config);
  BOOST_REQUIRE_NO_THROW(config.reset(new bw::StoredQueryConfig(raw_config, nullptr)));
  BOOST_REQUIRE_NO_THROW(param.reset(new bw::ScalarParameterTemplate(*config, "p")));

  // No mandatory parameter provided
  BOOST_REQUIRE_NO_THROW(val = param->get<int64_t>(param_map));
  BOOST_CHECK_EQUAL(512, val);

  // Single parameter provided (must succeed)
  std::array<int, 1> i001 = {{123}};
  Test::add_values<int, 1>(param_map, "foo", i001);
  BOOST_REQUIRE_NO_THROW(val = param->get<int64_t>(param_map));
  BOOST_CHECK_EQUAL(123, val);

  // Too many parameters provided
  std::array<int, 1> i002 = {{321}};
  Test::add_values<int, 1>(param_map, "foo", i002);
  BOOST_REQUIRE_THROW(param->get<int64_t>(param_map), SmartMet::Spine::Exception);
}

/****************************************************************************************/

BOOST_AUTO_TEST_CASE(test_single_scalar_parameter_with_lower_limit)
{
  dummy_proc(__PRETTY_FUNCTION__);
  BOOST_TEST_MESSAGE("+ [Single scalar parameter with lower limit specified]");

  boost::shared_ptr<TestConfig> raw_config(new TestConfig);

  raw_config->add_param("foo", "xxx", "time").lower_limit("2012-01-01T00:00:00Z");
  raw_config->add_scalar("p", "${foo}");

  pt::ptime val;
  RequestParameterMap param_map;
  StoredQueryConfigP config;
  ScalarParameterTemplateP param;
  // SmartMet::Spine::ConfigBase::dump_config(std::cout, *raw_config);
  BOOST_REQUIRE_NO_THROW(config.reset(new bw::StoredQueryConfig(raw_config, nullptr)));
  BOOST_REQUIRE_NO_THROW(param.reset(new bw::ScalarParameterTemplate(*config, "p")));

  // No mandatory parameter provided
  BOOST_CHECK_THROW(val = param->get<pt::ptime>(param_map), SmartMet::Spine::Exception);

  // Single parameter provided and is in allowed range (must succeed)
  std::array<std::string, 1> s001 = {{"2012-01-02T00:11:22Z"}};
  Test::add_values<std::string, 1>(param_map, "foo", s001);
  BOOST_REQUIRE_NO_THROW(val = param->get<pt::ptime>(param_map));
  BOOST_CHECK_EQUAL(std::string("2012-Jan-02 00:11:22"), pt::to_simple_string(val));

  // Single parameter provided and is out of allowed range (must succeed)
  std::array<std::string, 1> s002 = {{"2011-12-31T00:11:22Z"}};
  Test::add_values<std::string, 1>(param_map, "foo", s002);
  BOOST_CHECK_THROW(val = param->get<pt::ptime>(param_map), SmartMet::Spine::Exception);

  // Too many values provided
  std::array<std::string, 1> s003 = {{"2013-06-23T00:11:22Z"}};
  Test::add_values<std::string, 1>(param_map, "foo", s003);
  BOOST_CHECK_THROW(param->get<int64_t>(param_map), SmartMet::Spine::Exception);
}

/****************************************************************************************/

BOOST_AUTO_TEST_CASE(test_single_mandatory_array_parameter_with_fixed_length)
{
  dummy_proc(__PRETTY_FUNCTION__);
  BOOST_TEST_MESSAGE("+ [Single mandatory array parameter of constant size]");

  boost::shared_ptr<TestConfig> raw_config(new TestConfig);

  raw_config->add_param("foo", "xxx", "double[2]").min_occurs(1).max_occurs(1);
  std::array<std::string, 1> p001 = {{"${foo}"}};
  raw_config->add_array<1>("p", p001);

  std::vector<double> data;
  RequestParameterMap param_map;
  StoredQueryConfigP config;
  ArrayParameterTemplateP param;

  // SmartMet::Spine::ConfigBase::dump_config(std::cout, *raw_config);

  BOOST_REQUIRE_NO_THROW(config.reset(new bw::StoredQueryConfig(raw_config, nullptr)));
  BOOST_REQUIRE_NO_THROW(param.reset(new bw::ArrayParameterTemplate(*config, "p", 2, 2)));

  // Mandatory array with 2 elements required not provided at all
  BOOST_CHECK_THROW(param->get_double_array(param_map), SmartMet::Spine::Exception);

  // Mandatory array with 2 elements required but only 1 provided
  std::array<int, 1> i001 = {{123}};
  Test::add_values<int, 1>(param_map, "foo", i001);
  BOOST_CHECK_THROW(param->get_double_array(param_map), SmartMet::Spine::Exception);

  // Mandatory array with 2 elements required, 2 provided
  std::array<int, 1> i002 = {{321}};
  Test::add_values<int, 1>(param_map, "foo", i002);
  BOOST_CHECK_NO_THROW(data = param->get_double_array(param_map));
  BOOST_CHECK_EQUAL(2, (int)data.size());
  if (data.size() == 2)
  {
    BOOST_CHECK_EQUAL(123.0, data.at(0));
    BOOST_CHECK_EQUAL(321.0, data.at(1));
  }

  // Mandatory array with 2 elements required, too many provided
  std::array<int, 1> i003 = {{999}};
  Test::add_values<int, 1>(param_map, "foo", i003);
  BOOST_CHECK_THROW(param->get_double_array(param_map), SmartMet::Spine::Exception);
}
