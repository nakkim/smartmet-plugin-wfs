#pragma once

#include "ParameterTemplateBase.h"
#include "StoredQueryHandlerBase.h"
#include "SupportsExtraHandlerParams.h"
#include "UrlTemplateGenerator.h"
#include <boost/shared_ptr.hpp>
#include <map>
#include <vector>

namespace SmartMet
{
namespace Plugin
{
namespace WFS
{
class StoredAtomQueryHandlerBase : public StoredQueryHandlerBase,
                                   virtual public SupportsExtraHandlerParams
{
 public:
  StoredAtomQueryHandlerBase(SmartMet::Spine::Reactor* reactor,
                             StoredQueryConfig::Ptr config,
                             PluginImpl& plugin_impl,
                             boost::optional<std::string> template_file_name);

  virtual ~StoredAtomQueryHandlerBase();

  virtual void init_handler();

  virtual void query(const StoredQuery& query,
                     const std::string& language,
		     const boost::optional<std::string>& hostname,
                     std::ostream& output) const;

 protected:
  /**
   *   @brief Returns separate parameter maps for each URL to be generated
   *
   *   Arbitrary parameter transformations (like coordinate transformations)
   *   may also be performed by this method in derived classes
   *
   *   This method returns copy of the original parameter map as the first
   *   only member of vector in the base class.
   *
   *   Return of empty vector is threated as an error (no data available)
   */
  virtual void update_parameters(
      const RequestParameterMap& request_params,
      int seq_id,
      std::vector<boost::shared_ptr<RequestParameterMap> >& result) const;

 private:
  std::vector<std::string> get_param_callback(const std::string& param_name,
                                              const RequestParameterMap* param_map) const;

 private:
  boost::shared_ptr<UrlTemplateGenerator> url_generator;
};

}  // namespace WFS
}  // namespace Plugin
}  // namespace SmartMet
