#bin/bash
###########################################################
# Cabinet backgroup job
# 
#
###########################################################
ONECLOUD_HOME=/opt/onecloud
cd ${ONECLOUD_HOME}

CABINET_AUTH_FILE=${ONECLOUD_HOME}/config/cabinet.auth
CABINET_INFO_FILE=${ONECLOUD_HOME}/config/cabinet.info
ONECLOUD_CONFIG_FILE=${ONECLOUD_HOME}/config/onecloud.ini
HTTP_POID_URL=http://internal.powerone.lab:8080/setting/poid.api
CABINET_CONTROL_FILE=${ONECLOUD_HOME}/config/control.auth

LOG_FILE=/opt/onecloud/log/cabinet_cron.log
HTTP_SIGN_BIN=/opt/onecloud/bin/http_sign
HTTP_CURL_SCRIPT=/opt/onecloud/script/app/http_curl_post.sh

IS_INIT=`cat ${CABINET_INFO_FILE} | grep "initialize" | awk -F "=" '{print $2}'`
if [ "1" = "${IS_INIT}" ]
then
    echo "The cabinet controller already initialized."
    exit
fi

if [ -e ${ONECLOUD_CONFIG_FILE} ]
then
    HTTP_POID_URL=`cat ${ONECLOUD_CONFIG_FILE} | grep "app_center_poid_url" | awk -F "=" '{print $2}'`
fi

POID=`cat ${CABINET_AUTH_FILE}|grep "poid" | awk -F "=" '{print $2}'`
ACCESS_KEY=`cat ${CABINET_AUTH_FILE}|grep "accessKey" | awk -F "=" '{print $2}'`
ACCESS_SECRET=`cat ${CABINET_AUTH_FILE}|grep "accessSecret" | awk -F "=" '{print $2}'`
TIMESTAMP=
HTTP_SIGN=
HTTP_URL_PARAM=

# Update the POID
if [ "" = "${POID}" ]
then
    TIMESTAMP=`date +%s`
    HTTP_SIGN=`${HTTP_SIGN_BIN} ${ACCESS_SECRET} accessKey=${ACCESS_KEY} time=${TIMESTAMP}`
    HTTP_URL_PARAM="?accessKey="${ACCESS_KEY}"&time="${TIMESTAMP}"&sign="${HTTP_SIGN}

    JSON_TEXT=`/bin/curl -X GET -H "Content-type: application/json" ${HTTP_POID_URL}${HTTP_URL_PARAM}`
    POID=`echo ${JSON_TEXT} | awk -F "," '{print $1}'| awk -F ":" '{print $2}' | awk -F "\"" '{print $2}'`

    #update the auth file
    echo "poid="${POID} > ${CABINET_AUTH_FILE}
    echo "accessKey="${POID} >> ${CABINET_AUTH_FILE}
    echo "accessSecret="${ACCESS_SECRET} >> ${CABINET_AUTH_FILE}

    #update the info file
    echo "poid="${POID} > ${CABINET_INFO_FILE}
    echo "name=OneControl" >> ${CABINET_INFO_FILE}
    echo "initialize=1" >> ${CABINET_INFO_FILE}
    
fi
