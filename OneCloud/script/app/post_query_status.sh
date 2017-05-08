#bin/bash
###########################################################
# Post control status to APP center
#
# output: 
#        0: success, 1: fail
###########################################################


ONECLOUD_HOME=/opt/onecloud
cd ${ONECLOUD_HOME}

LOG_FILE=/opt/onecloud/log/app_web.log
HTTP_SIGN_BIN=/opt/onecloud/bin/http_sign
HTTP_CURL_SCRIPT=/opt/onecloud/script/app/http_curl_post.sh

HTTP_URL=$1
ACCESS_KEY=$2
ACCESS_SECRET=$3
TIMESTAMP=$4
CUSTOME_ARGS=$5

# json data not include {}
JSON_PARAMS=$6

CUSTOME_TEXT=`echo ${CUSTOME_ARGS//\:/\ }`
POST_PARAM_TEXT=`echo ${CUSTOME_ARGS//\:/\&}`

#Generate the http sign
HTTP_SIGN=`${HTTP_SIGN_BIN} ${ACCESS_SECRET} accessKey=${ACCESS_KEY} time=${TIMESTAMP} ${CUSTOME_TEXT} devices=${JSON_PARAMS}`
#JSON_POST="{\"accessKey\"=\""${ACCESS_KEY}"\",\"time\"="${TIMESTAMP}",\"sign\"=\""${HTTP_SIGN}"\","${JSON_PARAMS}"}"
JSON_DEVICES="devices="${JSON_PARAMS}
echo "" >> ${LOG_FILE}
echo "Debug CUSTOME_TEXT:  "${CUSTOME_TEXT} >> ${LOG_FILE}
echo "Debug JSON PARAMS:  "${JSON_PARAMS} >> ${LOG_FILE}
echo "Debug JSON DEVICES:  "${JSON_DEVICES} >> ${LOG_FILE}

#Post the HTTP request

#HTTP_URL_PARAM="?accessKey="${ACCESS_KEY}"&time="${TIMESTAMP}"&sign="${HTTP_SIGN}
HTTP_URL_PARAM="accessKey="${ACCESS_KEY}"&time="${TIMESTAMP}"&sign="${HTTP_SIGN}"&"${POST_PARAM_TEXT}"&"${JSON_DEVICES}
echo "Debug Web parameter:  "${HTTP_URL_PARAM} >> ${LOG_FILE}
nohup ${HTTP_CURL_SCRIPT} ${HTTP_URL} ${HTTP_URL_PARAM} >> ${LOG_FILE}  2>&1 &

echo 0




