#pragma once

#include "ArrayParameterTemplate.h"
#include "RequestParameterMap.h"
#include "ScalarParameterTemplate.h"
#include "StoredQueryConfig.h"
#include <boost/shared_ptr.hpp>
#include <boost/variant.hpp>
#include <map>
#include <set>
#include <typeinfo>

namespace SmartMet
{
namespace Plugin
{
namespace WFS
{
class SupportsExtraHandlerParams;
class ScalarParameterTemplate;
class ArrayParameterTemplate;

/**
 *   @brief Virtual base class for implementing registry of stored query
 *          parameters
 */
class StoredQueryParamRegistry
{
  struct ParamRecBase
  {
    std::string name;
    std::string type_name;

    virtual ~ParamRecBase() {}
  };

  struct ScalarParameterRec : public ParamRecBase
  {
    boost::shared_ptr<ScalarParameterTemplate> param_def;
    bool required;
  };

  struct ArrayParameterRec : public ParamRecBase
  {
    boost::shared_ptr<ArrayParameterTemplate> param_def;
    std::size_t min_size;
    std::size_t max_size;
    std::size_t step;
  };

 public:
  StoredQueryParamRegistry();

  virtual ~StoredQueryParamRegistry();

  boost::shared_ptr<RequestParameterMap> process_parameters(
      const RequestParameterMap& src, const SupportsExtraHandlerParams* extra_params = nullptr) const;

  std::set<std::string> get_param_names() const;

  template <typename ParamType>
  void register_scalar_param(const std::string& name,
                             StoredQueryConfig& config,
                             bool required = true);

  template <typename ParamType>
  void register_array_param(const std::string& name,
                            StoredQueryConfig& config,
                            std::size_t min_size = 0,
                            std::size_t max_size = std::numeric_limits<uint16_t>::max(),
                            std::size_t step = 0);

  void register_scalar_param(const std::string& name,
                             boost::shared_ptr<ScalarParameterTemplate> param_def,
                             bool required);

  void register_array_param(const std::string& name,
                            boost::shared_ptr<ArrayParameterTemplate> param_def,
                            std::size_t min_size = 0,
                            std::size_t max_size = std::numeric_limits<uint16_t>::max());

 private:
  void add_param_rec(boost::shared_ptr<ParamRecBase> rec);

 private:
  std::map<std::string, boost::shared_ptr<ParamRecBase> > param_map;
  std::map<std::string, int> supported_type_names;
};

template <typename ParamType>
void StoredQueryParamRegistry::register_scalar_param(const std::string& name,
                                                     StoredQueryConfig& config,
                                                     bool required)
{
  boost::shared_ptr<ScalarParameterRec> rec(new ScalarParameterRec);
  rec->name = name;
  rec->param_def.reset(new ScalarParameterTemplate(config, name));
  rec->type_name = typeid(ParamType).name();
  rec->required = required;
  add_param_rec(rec);
}

template <typename ParamType>
void StoredQueryParamRegistry::register_array_param(const std::string& name,
                                                    StoredQueryConfig& config,
                                                    std::size_t min_size,
                                                    std::size_t max_size,
                                                    std::size_t step)
{
  boost::shared_ptr<ArrayParameterRec> rec(new ArrayParameterRec);
  rec->name = name;
  rec->param_def.reset(new ArrayParameterTemplate(config, name, min_size, max_size));
  rec->type_name = typeid(ParamType).name();
  rec->min_size = min_size;
  rec->max_size = max_size;
  rec->step = step;
  add_param_rec(rec);
}

}  // namespace WFS
}  // namespace Plugin
}  // namespace SmartMet
