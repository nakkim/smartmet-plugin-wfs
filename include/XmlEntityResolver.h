#pragma once

#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/serialization/map.hpp>
#include <boost/serialization/map.hpp>
#include <xercesc/sax/InputSource.hpp>
#include <xercesc/util/XMLEntityResolver.hpp>

namespace SmartMet {
  namespace Plugin {
    namespace WFS {
      namespace Xml {

	class EntityResolver : public xercesc::XMLEntityResolver
	{
          std::map<std::string, std::string> cache;

	public:
          EntityResolver();

          virtual ~EntityResolver();

          virtual xercesc::InputSource *resolveEntity(xercesc::XMLResourceIdentifier *resource_identifier);



          template <class Archive>
          void serialize(Archive &ar, const unsigned int version);

	private:
	  xercesc::InputSource* try_preloaded_schema_cache(const std::string& remote_uri);
	};


	template <class Archive>
	void EntityResolver::serialize(Archive &ar, const unsigned int version)
	{
	  (void)version;
	  ar &BOOST_SERIALIZATION_NVP(cache);
	}
      } // namespace Xml
    } // namespace WFS
  } // Plugin
} // namespace Xml
