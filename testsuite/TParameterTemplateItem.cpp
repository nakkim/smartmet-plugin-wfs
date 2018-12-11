#include <boost/test/included/unit_test.hpp>

#include <cstdio>
#include <string>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <macgyver/TypeName.h>
#include "ParameterTemplateItem.h"

using namespace boost::unit_test;

test_suite* init_unit_test_suite(int argc, char* argv[])
{
  const char* name = "ParamTemplateItem tester";
  unit_test_log.set_threshold_level(log_messages);
  framework::master_test_suite().p_name.value = name;
  BOOST_TEST_MESSAGE("");
  BOOST_TEST_MESSAGE(name);
  BOOST_TEST_MESSAGE(std::string(std::strlen(name), '='));
  return NULL;
}

using SmartMet::Plugin::WFS::ParameterTemplateItem;
using SmartMet::Spine::Value;

BOOST_AUTO_TEST_CASE(param_templ_item_not_string)
{
  BOOST_TEST_MESSAGE(
      "+ [ParameterTemplateItem] Parsing from SmartMet::Spine::Value which contains non string value");
  ParameterTemplateItem item;
  double d1;
  Value src(2.0);
  BOOST_REQUIRE_NO_THROW(item.parse(src, false));
  BOOST_REQUIRE(item.plain_text);
  BOOST_CHECK(not item.param_ref);
  BOOST_CHECK(not item.param_ind);
  BOOST_CHECK(not item.default_value);
  BOOST_CHECK(not item.redirect_name);
  BOOST_REQUIRE_NO_THROW(d1 = item.plain_text->get_double());
  BOOST_CHECK_CLOSE(d1, 2.0, 1e-10);
}

BOOST_AUTO_TEST_CASE(param_templ_item_text_value)
{
  BOOST_TEST_MESSAGE("+ [ParameterTemplateItem] Parsing from text string");
  ParameterTemplateItem item;
  std::string s1;
  BOOST_REQUIRE_NO_THROW(item.parse(std::string("foobar"), false));
  BOOST_REQUIRE(item.plain_text);
  BOOST_CHECK(not item.param_ref);
  BOOST_CHECK(not item.param_ind);
  BOOST_CHECK(not item.default_value);
  BOOST_CHECK(not item.redirect_name);
  BOOST_REQUIRE_NO_THROW(s1 = item.plain_text->get_string());
  BOOST_CHECK_EQUAL(std::string("foobar"), s1);
}

BOOST_AUTO_TEST_CASE(param_templ_item_simple_param_reference_no_default)
{
  BOOST_TEST_MESSAGE(
      "+ [ParameterTemplateItem] Parsing reference to parameter without default value");
  ParameterTemplateItem item;
  std::string s1;
  BOOST_REQUIRE_NO_THROW(item.parse(std::string("${foobar}"), false));
  BOOST_CHECK(not item.absent);
  BOOST_CHECK(not item.weak);
  BOOST_CHECK(not item.plain_text);
  BOOST_REQUIRE(item.param_ref);
  BOOST_CHECK(not item.param_ind);
  BOOST_CHECK(not item.default_value);
  BOOST_CHECK(not item.redirect_name);
  BOOST_CHECK_EQUAL(std::string("foobar"), *item.param_ref);
}

BOOST_AUTO_TEST_CASE(param_templ_item_simple_param_weak_reference_no_default)
{
  BOOST_TEST_MESSAGE(
      "+ [ParameterTemplateItem] Parsing reference to parameter without default value");
  ParameterTemplateItem item;
  std::string s1;
  BOOST_REQUIRE_NO_THROW(item.parse(std::string("%{foobar}"), false));
  BOOST_CHECK(not item.absent);
  BOOST_CHECK(item.weak);
  BOOST_CHECK(not item.plain_text);
  BOOST_REQUIRE(item.param_ref);
  BOOST_CHECK(not item.param_ind);
  BOOST_CHECK(not item.default_value);
  BOOST_CHECK(not item.redirect_name);
  BOOST_CHECK_EQUAL(std::string("foobar"), *item.param_ref);
  BOOST_REQUIRE_NO_THROW(item.parse(std::string("\t %{foobar} \t "), false));
}

BOOST_AUTO_TEST_CASE(param_templ_item_array_param_member_reference_no_default)
{
  BOOST_TEST_MESSAGE(
      "+ [ParameterTemplateItem] Parsing reference to array parameter member without default "
      "value");
  ParameterTemplateItem item;
  std::string s1;
  BOOST_REQUIRE_NO_THROW(item.parse(std::string("${foobar[3]}"), false));
  BOOST_CHECK(not item.plain_text);
  BOOST_REQUIRE(item.param_ref);
  BOOST_REQUIRE(item.param_ind);
  BOOST_CHECK(not item.default_value);
  BOOST_CHECK(not item.redirect_name);
  BOOST_CHECK_EQUAL(std::string("foobar"), *item.param_ref);
  BOOST_CHECK_EQUAL(3, *item.param_ind);
}

BOOST_AUTO_TEST_CASE(param_templ_item_simple_param_reference_with_default)
{
  BOOST_TEST_MESSAGE("+ [ParameterTemplateItem] Parsing reference to parameter with default value");
  ParameterTemplateItem item;
  std::string s1;
  BOOST_REQUIRE_NO_THROW(item.parse(std::string("${foobar:FOOBAR}"), false));
  BOOST_CHECK(not item.plain_text);
  BOOST_REQUIRE(item.param_ref);
  BOOST_CHECK(not item.param_ind);
  BOOST_REQUIRE(item.default_value);
  BOOST_CHECK(not item.redirect_name);
  BOOST_CHECK_EQUAL(std::string("foobar"), *item.param_ref);
  BOOST_CHECK_EQUAL(std::string("FOOBAR"), *item.default_value);
}

BOOST_AUTO_TEST_CASE(param_templ_item_simple_param_reference_with_default_redirect)
{
  BOOST_TEST_MESSAGE(
      "+ [ParameterTemplateItem] Parsing reference to parameter with default value as redirect");
  ParameterTemplateItem item;
  std::string s1;
  BOOST_REQUIRE_NO_THROW(item.parse(std::string("${foobar>dft_foobar}"), false));
  BOOST_CHECK(not item.plain_text);
  BOOST_REQUIRE(item.param_ref);
  BOOST_CHECK(not item.param_ind);
  BOOST_CHECK(not item.default_value);
  BOOST_REQUIRE(item.redirect_name);
  BOOST_CHECK_EQUAL(std::string("foobar"), *item.param_ref);
  BOOST_CHECK_EQUAL(std::string("dft_foobar"), *item.redirect_name);
}

BOOST_AUTO_TEST_CASE(param_templ_item_array_param_member_reference_with_default)
{
  BOOST_TEST_MESSAGE(
      "+ [ParameterTemplateItem] Parsing reference to array parameter member with default value");
  ParameterTemplateItem item;
  std::string s1;
  BOOST_REQUIRE_NO_THROW(item.parse(std::string("${foobar[3]:FOOBAR}"), false));
  BOOST_CHECK(not item.plain_text);
  BOOST_REQUIRE(item.param_ref);
  BOOST_REQUIRE(item.param_ind);
  BOOST_REQUIRE(item.default_value);
  BOOST_CHECK(not item.redirect_name);
  BOOST_CHECK_EQUAL(std::string("foobar"), *item.param_ref);
  BOOST_CHECK_EQUAL(3, *item.param_ind);
  BOOST_CHECK_EQUAL(std::string("FOOBAR"), *item.default_value);
}

BOOST_AUTO_TEST_CASE(param_templ_item_array_param_member_reference_with_default_redirect)
{
  BOOST_TEST_MESSAGE(
      "+ [ParameterTemplateItem] Parsing reference to array parameter member with default value as "
      "redirect");
  ParameterTemplateItem item;
  std::string s1;
  BOOST_REQUIRE_NO_THROW(item.parse(std::string("${foobar[3]>dft_foobar}"), false));
  BOOST_CHECK(not item.plain_text);
  BOOST_REQUIRE(item.param_ref);
  BOOST_REQUIRE(item.param_ind);
  BOOST_CHECK(not item.default_value);
  BOOST_REQUIRE(item.redirect_name);
  BOOST_CHECK_EQUAL(std::string("foobar"), *item.param_ref);
  BOOST_CHECK_EQUAL(3, *item.param_ind);
  BOOST_CHECK_EQUAL(std::string("dft_foobar"), *item.redirect_name);
}

BOOST_AUTO_TEST_CASE(param_templ_item_garbage_after_param_ref)
{
  BOOST_TEST_MESSAGE("+ [ParameterTemplateItem] Garbage after parameter reference");
  ParameterTemplateItem item;
  std::string s1;
  BOOST_REQUIRE_THROW(item.parse(std::string("%{foobar}:XXXX"), false), SmartMet::Spine::Exception);
  BOOST_REQUIRE_THROW(item.parse(std::string("%{} XXXX"), false), SmartMet::Spine::Exception);
  BOOST_REQUIRE_THROW(item.parse(std::string("%{foo[3] > bar} jfjhf "), false), SmartMet::Spine::Exception);
}
