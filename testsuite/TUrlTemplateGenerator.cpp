#define BOOST_TEST_MODULE TUrlTemplateGenerator
#define BOOST_TEST_DYN_LINK 1
#include <iostream>
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/test/unit_test.hpp>
#include <newbase/NFmiPoint.h>
#include <macgyver/TypeName.h>
#include <macgyver/Exception.h>
#include "UrlTemplateGenerator.h"

using namespace boost::unit_test;

test_suite* init_unit_test_suite(int argc, char* argv[])
{
  const char* name = "UrlTemplateGenerator tester";
  unit_test_log.set_threshold_level(log_messages);
  framework::master_test_suite().p_name.value = name;
  BOOST_TEST_MESSAGE("");
  BOOST_TEST_MESSAGE(name);
  BOOST_TEST_MESSAGE(std::string(std::strlen(name), '='));
  return NULL;
}

using namespace SmartMet::Plugin::WFS;

namespace
{
void output_exception_info(const std::exception& err)
{
  std::cerr << "C++ exception of type " << Fmi::get_type_name(&err)
            << " catched: " << err.what() << std::endl;
}
}

#define LOG(x)                      \
  try                               \
  {                                 \
    x;                              \
  }                                 \
  catch (const std::exception& err) \
  {                                 \
    output_exception_info(err);     \
    throw;                          \
  }

std::vector<std::string> get_param(const std::string name,
                                   const std::multimap<std::string, std::string>* param_map)
{
  std::vector<std::string> result;
  auto range = param_map->equal_range(name);
  for (auto it = range.first; it != range.second; ++it)
  {
    result.push_back(it->second);
  }
  return result;
}

BOOST_AUTO_TEST_CASE(test_template_url_generator_1)
{
  BOOST_TEST_MESSAGE("+ [Test template URL generator (single constant parameter)]");

  std::vector<std::string> params;
  params.push_back("foo=bar");

  std::multimap<std::string, std::string> param_map;

  std::string result;
  boost::shared_ptr<UrlTemplateGenerator> test;
  BOOST_REQUIRE_NO_THROW(
      test.reset(new UrlTemplateGenerator("http://www.example.com/test", params)));
  BOOST_REQUIRE_NO_THROW(result = test->generate(boost::bind(&get_param, ::_1, &param_map)));
  BOOST_CHECK_EQUAL(std::string("http://www.example.com/test?foo=bar"), result);

  BOOST_REQUIRE_NO_THROW(
      test.reset(new UrlTemplateGenerator("http://www.example.com/test?", params)));
  BOOST_REQUIRE_NO_THROW(result = test->generate(boost::bind(&get_param, ::_1, &param_map)));
  BOOST_CHECK_EQUAL(std::string("http://www.example.com/test?foo=bar"), result);
}

BOOST_AUTO_TEST_CASE(test_template_url_generator_2)
{
  BOOST_TEST_MESSAGE("+ [Test template URL generator (single parameter reference)]");

  std::vector<std::string> params;
  params.push_back("${foo}");

  std::multimap<std::string, std::string> param_map;

  std::string result;
  boost::shared_ptr<UrlTemplateGenerator> test;
  BOOST_REQUIRE_NO_THROW(
      test.reset(new UrlTemplateGenerator("http://www.example.com/test", params)));
  const auto& pm = test->get_content();
  BOOST_REQUIRE_EQUAL(1, (int)pm.size());
  BOOST_REQUIRE(pm.at(0).type() == typeid(UrlTemplateGenerator::ParamRef));

  BOOST_REQUIRE_NO_THROW(result = test->generate(boost::bind(&get_param, ::_1, &param_map)));
  // std::cout << result << std::endl;
  BOOST_CHECK_EQUAL(std::string("http://www.example.com/test?foo="), result);
  param_map.insert(std::make_pair("foo", "bar"));
  BOOST_REQUIRE_NO_THROW(result = test->generate(boost::bind(&get_param, ::_1, &param_map)));
  BOOST_CHECK_EQUAL(std::string("http://www.example.com/test?foo=bar"), result);
}

BOOST_AUTO_TEST_CASE(test_template_url_generator_3)
{
  BOOST_TEST_MESSAGE("+ [Test template URL generator (several parameters)]");

  std::vector<std::string> params;
  params.push_back("${a1}");
  params.push_back("${?a2}");
  params.push_back("b1=f=oo");

  std::multimap<std::string, std::string> param_map;
  param_map.insert(std::make_pair("a1", "A1"));

  std::string result;
  boost::shared_ptr<UrlTemplateGenerator> test;
  BOOST_REQUIRE_NO_THROW(
      test.reset(new UrlTemplateGenerator("http://www.example.com/test", params)));
  const auto& pm = test->get_content();
  BOOST_REQUIRE_EQUAL(3, (int)pm.size());
  BOOST_REQUIRE(pm.at(0).type() == typeid(UrlTemplateGenerator::ParamRef));
  BOOST_REQUIRE(pm.at(1).type() == typeid(UrlTemplateGenerator::ParamRef));
  BOOST_REQUIRE(pm.at(2).type() == typeid(UrlTemplateGenerator::StringParam));

  BOOST_REQUIRE_NO_THROW(result = test->generate(boost::bind(&get_param, ::_1, &param_map)));
  // std::cout << result << std::endl;
  BOOST_CHECK_EQUAL(std::string("http://www.example.com/test?a1=A1&b1=f%3Doo"), result);
  param_map.insert(std::make_pair("a2", "A2"));
  BOOST_REQUIRE_NO_THROW(result = test->generate(boost::bind(&get_param, ::_1, &param_map)));
  BOOST_CHECK_EQUAL(std::string("http://www.example.com/test?a1=A1&a2=A2&b1=f%3Doo"), result);
}

BOOST_AUTO_TEST_CASE(test_template_url_generator_4)
{
  BOOST_TEST_MESSAGE("+ [Test template URL generator (repeated parameters)]");

  std::vector<std::string> params;
  params.push_back("${a1}");
  params.push_back("${*a2}");
  params.push_back("b1=f=oo");

  std::multimap<std::string, std::string> param_map;
  param_map.insert(std::make_pair("a1", "A1"));
  param_map.insert(std::make_pair("a2", "A2A"));
  param_map.insert(std::make_pair("a2", "A2B"));

  std::string result;
  boost::shared_ptr<UrlTemplateGenerator> test;
  BOOST_REQUIRE_NO_THROW(
      test.reset(new UrlTemplateGenerator("http://www.example.com/test", params)));
  const auto& pm = test->get_content();
  BOOST_REQUIRE_EQUAL(3, (int)pm.size());
  BOOST_REQUIRE(pm.at(0).type() == typeid(UrlTemplateGenerator::ParamRef));
  BOOST_REQUIRE(pm.at(1).type() == typeid(UrlTemplateGenerator::ParamRef));
  BOOST_REQUIRE(pm.at(2).type() == typeid(UrlTemplateGenerator::StringParam));

  BOOST_REQUIRE_NO_THROW(result = test->generate(boost::bind(&get_param, ::_1, &param_map)));
  BOOST_CHECK_EQUAL(std::string("http://www.example.com/test?a1=A1&a2=A2A&a2=A2B&b1=f%3Doo"),
                    result);
}

BOOST_AUTO_TEST_CASE(test_template_url_generator_5)
{
  BOOST_TEST_MESSAGE("+ [Test template URL generator (parameters already in original URL)]");

  std::vector<std::string> params;
  params.push_back("foo=bar");

  std::multimap<std::string, std::string> param_map;

  std::string result;
  boost::shared_ptr<UrlTemplateGenerator> test;
  BOOST_REQUIRE_NO_THROW(
      test.reset(new UrlTemplateGenerator("http://www.example.com/test?a1=A1", params)));
  BOOST_REQUIRE_NO_THROW(result = test->generate(boost::bind(&get_param, ::_1, &param_map)));
  BOOST_CHECK_EQUAL(std::string("http://www.example.com/test?a1=A1&foo=bar"), result);

  BOOST_REQUIRE_NO_THROW(
      test.reset(new UrlTemplateGenerator("http://www.example.com/test?a1=A1&", params)));
  BOOST_REQUIRE_NO_THROW(result = test->generate(boost::bind(&get_param, ::_1, &param_map)));
  BOOST_CHECK_EQUAL(std::string("http://www.example.com/test?a1=A1&foo=bar"), result);
}

BOOST_AUTO_TEST_CASE(test_template_url_generator_6)
{
  BOOST_TEST_MESSAGE("+ [Test template URL generator (evaluate text value)]");

  std::vector<std::string> params;
  params.push_back("${a1}");
  params.push_back("${*a2}");
  params.push_back("b1=foo-${a1}-bar");

  std::multimap<std::string, std::string> param_map;
  param_map.insert(std::make_pair("a1", "A1"));
  param_map.insert(std::make_pair("a2", "A2A"));
  param_map.insert(std::make_pair("a2", "A2B"));

  std::string result;
  boost::shared_ptr<UrlTemplateGenerator> test;
  BOOST_REQUIRE_NO_THROW(
      test.reset(new UrlTemplateGenerator("http://www.example.com/test", params)));
  const auto& pm = test->get_content();
  BOOST_REQUIRE_EQUAL(3, (int)pm.size());
  BOOST_REQUIRE(pm.at(0).type() == typeid(UrlTemplateGenerator::ParamRef));
  BOOST_REQUIRE(pm.at(1).type() == typeid(UrlTemplateGenerator::ParamRef));
  BOOST_REQUIRE(pm.at(2).type() == typeid(UrlTemplateGenerator::StringParam));

  BOOST_REQUIRE_NO_THROW(LOG(result = test->generate(boost::bind(&get_param, ::_1, &param_map))));
  BOOST_CHECK_EQUAL(std::string("http://www.example.com/test?a1=A1&a2=A2A&a2=A2B&b1=foo-A1-bar"),
                    result);
}

BOOST_AUTO_TEST_CASE(test_template_url_generator_7)
{
  BOOST_TEST_MESSAGE("+ [Test substituting values in the original URL]");

  std::vector<std::string> params;

  std::multimap<std::string, std::string> param_map;
  param_map.insert(std::make_pair("a1", "A1"));
  param_map.insert(std::make_pair("a2", "A2A"));
  param_map.insert(std::make_pair("a3", "A3A"));
  param_map.insert(std::make_pair("a3", "A3B"));

  std::string result;
  boost::shared_ptr<UrlTemplateGenerator> test;
  BOOST_REQUIRE_NO_THROW(
      test.reset(new UrlTemplateGenerator("http://www.example.com/test/${a1}/${a2}/foo", params)));
  const auto& pm = test->get_content();
  BOOST_REQUIRE_EQUAL(0, (int)pm.size());

  BOOST_REQUIRE_NO_THROW(LOG(result = test->generate(boost::bind(&get_param, ::_1, &param_map))));
  BOOST_CHECK_EQUAL(std::string("http://www.example.com/test/A1/A2A/foo"), result);
}

BOOST_AUTO_TEST_CASE(test_template_url_generator_8)
{
  BOOST_TEST_MESSAGE("+ [Test substituting values in the original URL (bad)]");

  std::vector<std::string> params;

  std::multimap<std::string, std::string> param_map;
  param_map.insert(std::make_pair("a1", "A1"));
  param_map.insert(std::make_pair("a2", "A2A"));
  param_map.insert(std::make_pair("a3", "A3A"));
  param_map.insert(std::make_pair("a3", "A3B"));

  std::string result;
  boost::shared_ptr<UrlTemplateGenerator> test;
  BOOST_REQUIRE_NO_THROW(
      test.reset(new UrlTemplateGenerator("http://www.example.com/test/${a1}/${a3}/foo", params)));
  const auto& pm = test->get_content();
  BOOST_REQUIRE_EQUAL(0, (int)pm.size());

  BOOST_REQUIRE_THROW(result = test->generate(boost::bind(&get_param, ::_1, &param_map)),
                      Fmi::Exception);
}
