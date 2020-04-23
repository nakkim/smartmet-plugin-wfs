#include <iostream>
#include <boost/test/unit_test.hpp>
#include <newbase/NFmiPoint.h>
#include "RequestParameterMap.h"

using namespace boost::unit_test;

test_suite* init_unit_test_suite(int argc, char* argv[])
{
  const char* name = "RequestParameterMap tester";
  unit_test_log.set_threshold_level(log_messages);
  framework::master_test_suite().p_name.value = name;
  BOOST_TEST_MESSAGE("");
  BOOST_TEST_MESSAGE(name);
  BOOST_TEST_MESSAGE(std::string(std::strlen(name), '='));
  return NULL;
}

BOOST_AUTO_TEST_CASE(test_1)
{
  using namespace SmartMet::Plugin::WFS;
  // namespace pt = boost::posix_time;

  BOOST_TEST_MESSAGE("+ [Basic use of RequestParameterMap]");

  RequestParameterMap params;
  params.add<int64_t>("A", 123, true);
  params.add<std::string>("B", "foo", true);
  params.add<std::string>("B", "bar", false);
  params.add<std::string>("B", "baz", false);

  BOOST_REQUIRE_EQUAL(1, params.count("A"));

  std::size_t n = 0;
  std::vector<int> i1v;
  BOOST_REQUIRE_NO_THROW(n = params.get<int64_t>("A", std::back_inserter(i1v)));
  BOOST_CHECK_EQUAL(1, (int)n);
  BOOST_CHECK_EQUAL(1, (int)i1v.size());
  BOOST_CHECK_EQUAL(123, i1v.at(0));

  std::vector<std::string> s1v;
  BOOST_REQUIRE_NO_THROW(n = params.get<std::string>("B", std::back_inserter(s1v)));
  BOOST_CHECK_EQUAL(3, (int)n);
  BOOST_CHECK_EQUAL(3, (int)s1v.size());
  BOOST_CHECK_EQUAL(std::string("foo"), s1v.at(0));
  BOOST_CHECK_EQUAL(std::string("bar"), s1v.at(1));
  BOOST_CHECK_EQUAL(std::string("baz"), s1v.at(2));

  s1v.clear();
  BOOST_REQUIRE_THROW(params.get<std::string>("B", std::back_inserter(s1v), 0, 2),
                      SmartMet::Spine::Exception);

  s1v.clear();
  BOOST_REQUIRE_THROW(params.get<std::string>("B", std::back_inserter(s1v), 4, 4),
                      SmartMet::Spine::Exception);

  s1v.clear();
  BOOST_REQUIRE_THROW(params.get<std::string>("B", std::back_inserter(s1v), 0, 4, 2),
                      SmartMet::Spine::Exception);
}

BOOST_AUTO_TEST_CASE(test_substitution_into_string)
{
  using namespace SmartMet::Plugin::WFS;

  RequestParameterMap params;
  params.add<int64_t>("A", 123, true);
  params.add<std::string>("B", "foo", true);
  params.add<std::string>("B", "bar", false);
  params.add<std::string>("B", "baz", false);
  params.add<int64_t>("answer", 42, false);

  std::string result;

  BOOST_REQUIRE_NO_THROW(result = params.subst("a=${A} answer=${answer}"));
  BOOST_CHECK_EQUAL(std::string("a=123 answer=42"), result);

  BOOST_REQUIRE_NO_THROW(result = params.subst("a=\\$${A} foo"));
  BOOST_CHECK_EQUAL(std::string("a=$123 foo"), result);

  BOOST_REQUIRE_THROW(result = params.subst("a=${A1} foo"), SmartMet::Spine::Exception);

  BOOST_REQUIRE_NO_THROW(result = params.subst("a=${?A1} foo"));
  BOOST_CHECK_EQUAL(std::string("a= foo"), result);

  BOOST_REQUIRE_THROW(result = params.subst("b=${B} foo"), SmartMet::Spine::Exception);

  BOOST_REQUIRE_THROW(result = params.subst("b=${?B} foo"), SmartMet::Spine::Exception);

  BOOST_REQUIRE_NO_THROW(result = params.subst("b=${*B} foo"));
  BOOST_CHECK_EQUAL(std::string("b=foo,bar,baz foo"), result);
}
