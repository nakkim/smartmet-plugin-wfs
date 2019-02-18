#! /bin/sh

set -x

# # Example: $HOME/.brainstormrc
# TARGET_SERVER="data.fmi.fi"
# FMI_API_KEY="put_your_fmi-apikey_here"

brainstorm_rc=${HOME}/.brainstormrc

# target server
server=${SMARTMET_SERVER-opendata.fmi.fi}

# read user defined parameters
if [ -f ${brainstorm_rc} ]; then
  . ${brainstorm_rc}
  if [ -z "${FMI_API_KEY}" ]; then
    echo -e "\nWARNING: FMI_API_KEY not found or is zero length in file ${brainstorm_rc}\n"
    FMI_API_KEY_URI_SUFFIX=""
  else
    FMI_API_KEY_URI_SUFFIX="/fmi-apikey/${FMI_API_KEY}"
  fi
  if [ -n "${TARGET_SERVER}" ]; then
    server="${TARGET_SERVER}"
    echo "Using user defined target server ${server}."
  fi
else
  echo -e "\nWARNING: file ${brainstorm_rc} not found.\n"
fi

# Use proxy if the script is not run on the same host as target server.
host_name=$(hostname);
if echo $server | egrep -q '^(127\.0\.|192\.168\.)' ; then
  proxy="";
  export http_proxy=$proxy
elif [ ! -z ${http_proxy} ] && [ "${host_name}" != "${server}" ]; then
  proxy="--proxy ${http_proxy}"
  echo "Using http_proxy: ${http_proxy}";
else
  proxy="";
  export http_proxy=$proxy
fi

files=$(cd request && ls -1b *.xml | sort)

rm -rf response
mkdir -p response

for req in $files; do
    curl ${proxy} \
         --fail \
         --output response/$req \
         --header "Content-Type: text/xml" \
         --data-binary "@request/$req" \
         http://$server${FMI_API_KEY_URI_SUFFIX}/wfs || exit 1
done

exit 0;
