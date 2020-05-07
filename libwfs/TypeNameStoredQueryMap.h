#pragma once

/*
 * TypeNameStoredQueryMap.h
 *
 *  Created on: Nov 10, 2014
 *      Author: niemitu
 */

#include <map>
#include <string>
#include <vector>

namespace SmartMet
{
namespace Plugin
{
namespace WFS
{
/**
 *  @brief A C++ class for holding information about available typenames
 *         and mapping typenames to available stored queries.
 */
class TypeNameStoredQueryMap
{
 public:
  TypeNameStoredQueryMap();

  virtual ~TypeNameStoredQueryMap();

  const std::vector<std::string>& get_stored_queries_by_name(const std::string& type_name) const;

  void init(const std::map<std::string, std::string>& typename_storedqry);

 private:
 private:
  std::map<std::string, std::vector<std::string> > typename_qry_map;
};

}  // namespace WFS
}  // namespace Plugin
}  // namespace SmartMet
