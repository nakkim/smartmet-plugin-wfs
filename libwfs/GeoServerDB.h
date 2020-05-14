#pragma once

#include <boost/noncopyable.hpp>
#include <macgyver/ObjectPool.h>
#include <pqxx/pqxx>

namespace SmartMet
{
namespace Plugin
{
namespace WFS
{
class GeoServerDB : virtual protected boost::noncopyable
{
 public:
  GeoServerDB(const std::string& conn_str, std::size_t keep_conn = 5);

  virtual ~GeoServerDB();

  boost::shared_ptr<pqxx::connection> get_conn();

  void update();

 private:
  boost::shared_ptr<pqxx::connection> create_new_conn();

 private:
  const std::string conn_str;
  Fmi::ObjectPool<pqxx::connection> conn_pool;
};

}  // namespace WFS
}  // namespace Plugin
}  // namespace SmartMet
