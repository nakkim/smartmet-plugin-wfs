#include <boost/test/included/unit_test.hpp>

#include "StoredQueryParamDef.h"
#include "WfsException.h"
#include <spine/Exception.h>

using namespace boost::unit_test;

test_suite* init_unit_test_suite(int argc, char* argv[])
{
  const char* name = "StoredQueryParamDef tester";
  unit_test_log.set_threshold_level(log_messages);
  framework::master_test_suite().p_name.value = name;
  BOOST_TEST_MESSAGE("");
  BOOST_TEST_MESSAGE(name);
  BOOST_TEST_MESSAGE(std::string(std::strlen(name), '='));
  return NULL;
}

using namespace SmartMet;
using namespace SmartMet::Plugin::WFS;
using SmartMet::Spine::Value;

BOOST_AUTO_TEST_CASE(test_scalar_string_def)
{
  BOOST_TEST_MESSAGE("+ [StoredQueryParamDef] Testing getting scalar string parameter");
  StoredQueryParamDef param_def;
  param_def.parse_def("string");
  BOOST_CHECK(param_def.getValueType() == StoredQueryParamDef::STRING);
  BOOST_CHECK(!param_def.isArray());
  BOOST_CHECK(param_def.getMinSize() == 1);
  BOOST_CHECK(param_def.getMaxSize() == 1);

  Value value;
  std::string s1;
  BOOST_REQUIRE_NO_THROW(value = param_def.readValue("foobar"));
  BOOST_CHECK(value.type() == typeid(std::string));
  BOOST_REQUIRE_NO_THROW(s1 = value.get_string());
  BOOST_CHECK_EQUAL(s1, std::string("foobar"));
}

BOOST_AUTO_TEST_CASE(test_scalar_int_def)
{
  BOOST_TEST_MESSAGE("+ [StoredQueryParamDef] Testing getting scalar int parameter");
  StoredQueryParamDef param_def;
  param_def.parse_def("int");
  BOOST_CHECK(param_def.getValueType() == StoredQueryParamDef::INT);
  BOOST_CHECK(!param_def.isArray());
  BOOST_CHECK(param_def.getMinSize() == 1);
  BOOST_CHECK(param_def.getMaxSize() == 1);

  Value value;
  int64_t i1;
  BOOST_REQUIRE_NO_THROW(value = param_def.readValue("123"));
  BOOST_CHECK(value.type() == typeid(int64_t));
  BOOST_REQUIRE_NO_THROW(i1 = value.get_int());
  BOOST_CHECK_EQUAL(i1, (int64_t)123);

  BOOST_REQUIRE_NO_THROW(value = param_def.readValue("-321"));
  BOOST_CHECK(value.type() == typeid(int64_t));
  BOOST_REQUIRE_NO_THROW(i1 = value.get_int());
  BOOST_CHECK_EQUAL(i1, (int64_t)(-321));

  BOOST_CHECK_THROW(value = param_def.readValue("1.2"), SmartMet::Spine::Exception);
  BOOST_CHECK_THROW(value = param_def.readValue("foo"), SmartMet::Spine::Exception);
}

BOOST_AUTO_TEST_CASE(test_scalar_bool_def)
{
  BOOST_TEST_MESSAGE("+ [StoredQueryParamDef] Testing getting scalar boolean parameter");
  StoredQueryParamDef param_def;
  param_def.parse_def("bool");
  BOOST_CHECK(param_def.getValueType() == StoredQueryParamDef::BOOL);
  BOOST_CHECK(!param_def.isArray());
  BOOST_CHECK(param_def.getMinSize() == 1);
  BOOST_CHECK(param_def.getMaxSize() == 1);

  Value value;
  bool b1;
  BOOST_REQUIRE_NO_THROW(value = param_def.readValue("true"));
  BOOST_CHECK(value.type() == typeid(bool));
  BOOST_REQUIRE_NO_THROW(b1 = value.get_bool());
  BOOST_CHECK_EQUAL(b1, true);

  BOOST_REQUIRE_NO_THROW(value = param_def.readValue("false"));
  BOOST_CHECK(value.type() == typeid(bool));
  BOOST_REQUIRE_NO_THROW(b1 = value.get_bool());
  BOOST_CHECK_EQUAL(b1, false);

  BOOST_CHECK_THROW(value = param_def.readValue("1.2"), SmartMet::Spine::Exception);
  BOOST_CHECK_THROW(value = param_def.readValue("foo"), SmartMet::Spine::Exception);
}

BOOST_AUTO_TEST_CASE(test_scalar_uint_def)
{
  BOOST_TEST_MESSAGE("+ [StoredQueryParamDef] Testing getting scalar unsigned int parameter");
  StoredQueryParamDef param_def;
  param_def.parse_def("uint");
  BOOST_CHECK(param_def.getValueType() == StoredQueryParamDef::UINT);
  BOOST_CHECK(!param_def.isArray());
  BOOST_CHECK(param_def.getMinSize() == 1);
  BOOST_CHECK(param_def.getMaxSize() == 1);

  Value value;
  uint64_t u1;
  BOOST_REQUIRE_NO_THROW(value = param_def.readValue("123"));
  BOOST_REQUIRE(value.type() == typeid(uint64_t));
  BOOST_REQUIRE_NO_THROW(u1 = value.get_uint());
  BOOST_CHECK_EQUAL(u1, (uint64_t)123);

  // FIXME: Fails to throw due to implementation of boost::lexical_cast<>
  // BOOST_CHECK_THROW(value = param_def.readValue("-321"), SmartMet::Spine::Exception);
  BOOST_CHECK_THROW(value = param_def.readValue("1.2"), SmartMet::Spine::Exception);
  BOOST_REQUIRE_THROW(value = param_def.readValue("foo"), SmartMet::Spine::Exception);
}

BOOST_AUTO_TEST_CASE(test_scalar_double_def)
{
  BOOST_TEST_MESSAGE("+ [StoredQueryParamDef] Testing getting scalar double parameter");
  StoredQueryParamDef param_def;
  param_def.parse_def("double");
  BOOST_CHECK(param_def.getValueType() == StoredQueryParamDef::DOUBLE);
  BOOST_CHECK(!param_def.isArray());
  BOOST_CHECK(param_def.getMinSize() == 1);
  BOOST_CHECK(param_def.getMaxSize() == 1);

  Value value;
  double d1;
  BOOST_REQUIRE_NO_THROW(value = param_def.readValue("123"));
  BOOST_REQUIRE(value.type() == typeid(double));
  BOOST_REQUIRE_NO_THROW(d1 = value.get_double());
  BOOST_CHECK_CLOSE_FRACTION(d1, (double)123, 1e-10);

  BOOST_REQUIRE_NO_THROW(value = param_def.readValue("-1.2"));
  BOOST_REQUIRE(value.type() == typeid(double));
  BOOST_REQUIRE_NO_THROW(d1 = value.get_double());
  BOOST_CHECK_CLOSE_FRACTION(d1, (double)(-1.2), 1e-10);

  BOOST_REQUIRE_THROW(value = param_def.readValue("foo"), SmartMet::Spine::Exception);
}

BOOST_AUTO_TEST_CASE(test_scalar_time_def)
{
  StoredQueryParamDef param_def;
  BOOST_TEST_MESSAGE(
      "+ [StoredQueryParamDef] Testing getting scalar boost::posix_time::ptime parameter");
  param_def.parse_def("time");
  BOOST_CHECK(param_def.getValueType() == StoredQueryParamDef::TIME);
  BOOST_CHECK(!param_def.isArray());
  BOOST_CHECK(param_def.getMinSize() == 1);
  BOOST_CHECK(param_def.getMaxSize() == 1);
}
