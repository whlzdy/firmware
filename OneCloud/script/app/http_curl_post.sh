#bin/bash
###########################################################
# Execute HTTP post using curl
#
###########################################################

HTTP_URL=$1
HTTP_PARAMS=$2

#/bin/curl -l -H "Content-type: application/json" -X POST -d '${JSON_DATA}' ${HTTP_URL}
#/bin/curl -l -H "Content-type: application/x-www-form-urlencoded; charset=utf-8" -X POST -d '${HTTP_PARAMS}' ${HTTP_URL}
/bin/curl -l -g -H "Content-type: application/x-www-form-urlencoded; charset=utf-8" -X POST ${HTTP_URL}?${HTTP_PARAMS}

