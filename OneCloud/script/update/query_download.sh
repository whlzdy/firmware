###############################################
#               www.onecloud.cn
#          onecloud firmware_update.sh
#                 Author: whl
#               date:2017-11-2
###############################################

#!/bin/bash
. /opt/onecloud/script/update/firmware_common.sh 
#/opt/onecloud
ONECLOUD_HOME=/opt/onecloud/
cd ${ONECLOUD_HOME}
CABINET_AUTH_FILE=${ONECLOUD_HOME}/config/cabinet.auth
ONECLOUD_CONFIG_FILE=${ONECLOUD_HOME}/config/onecloud.ini
LOG_FILE=/opt/log/firmware_update.log
FIRMWARE_UPDATE=/opt/onecloud/script/update/firmware_update.sh 
#current firmware dir
CRT_FIRMWARE_DIR="/opt/onecloud"
#last firmware dir
LAST_FIRMWARE_DIR="/opt/onecloud/last"
#httpd store dir"
HTTPD_STORE_DIR="/opt/httpd"

$(echo_log ": ******welcome to qurey-download.sh *******! " ${LOG_FILE})

POID=`cat ${CABINET_AUTH_FILE}|grep "poid" | awk -F "=" '{print $2}'`
if [ "" = "${POID}" ]
then
	$(echo_log ":POID is NULL so exit " ${LOG_FILE})
	exit 1
fi
if [ -e ${ONECLOUD_CONFIG_FILE} ]
then
    	FIRMWARE_VERSION=`cat ${ONECLOUD_CONFIG_FILE} | grep "firmware_version" | awk -F "=" '{print $2}'`
    	FIRMWARE_SERVER_URL=`cat ${ONECLOUD_CONFIG_FILE} | grep "firmware_server" | awk -F "=" '{print $2}'`
	FIRMWARE_Download_URL_HEADER=`cat ${ONECLOUD_CONFIG_FILE} | grep "download_server" | awk -F "=" '{print $2}'`
fi

#Tag (cron/init) As script param
#1: get poid
#2: get version
echo "Poid is ${POID}"
echo "Version is ${FIRMWARE_VERSION}"
echo "Tag is $1" 
echo "APPCC is ${FIRMWARE_SERVER_URL}"
if [ "" = "$1" ]
then
	$(echo_log ":Tag is null so exit " ${LOG_FILE})
	exit 2
fi

HTTP_URL_PARAM="?poid="${POID}"&version="${FIRMWARE_VERSION}"&tag="$1""
echo "HTTP_URL_PARAM is ${HTTP_URL_PARAM}"
#3: query is need download
#3.1：parse rtn json get status,version,md5
QUERY_TEXT=`/bin/curl -m 30 -X GET ${FIRMWARE_SERVER_URL}${HTTP_URL_PARAM}`
if [ $? != "0" ]
then 
	$(echo_log ":/bin/curl connect server timeout ! " ${LOG_FILE})
	exit 6
fi

if [ "init" = $1 ]
then
	
	$(echo_log ":init excute complete normal exit " ${LOG_FILE})
	exit 0
fi

STATUS=`echo ${QUERY_TEXT} | awk -F "[,]" '/status/{print$4}' | awk -F "[:]" '{print $2}'`
echo "status is ${STATUS}"
if [ "200" != ${STATUS:1:3} ]
then
	$(echo_log ":status is error so exit " ${LOG_FILE})
	exit 3
fi
$(echo_log ":status verify passed ! " ${LOG_FILE})

NEXT_VERSION=`echo ${QUERY_TEXT} | awk -F "[,]" '/nextversion/{print$5}' | awk -F "[:]" '{print $2}'|awk -F "[}]" '{print $1}'`

$(echo_log "next_version is ${NEXT_VERSION} " ${LOG_FILE})

if [  ${FIRMWARE_VERSION} = ${NEXT_VERSION:1:-1}  ] || [  ${NEXT_VERSION} = '"null"' ]
then
	$(echo_log ":next_version is null or not need update so exit " ${LOG_FILE})
	exit 4
fi
#4.1: get download url
#http://192.168.1.113:8110/downloads/boardversionfile/onecloud_firmware_0.0.0.1.tar.bz2
FIRMWARE_Download_URL=${FIRMWARE_Download_URL_HEADER}"/onecloud_firmware_"${NEXT_VERSION:1:-1}".tar.bz2"
echo "FIRMWARE_Download_URL is ${FIRMWARE_Download_URL}"
$(echo_log "FIRMWARE_Download_URL is ${FIRMWARE_Download_URL} " ${LOG_FILE})
#4.2 wget download file
$(echo_log ":now is ready to download file ...  " ${LOG_FILE})
for ((i=1;i<=3;i=i+1))
do 
	
	$(echo_log ":count=${i} to download file...  " ${LOG_FILE})
	rm /opt/httpd/* -rf
	wget ${FIRMWARE_Download_URL} -c -O ${HTTPD_STORE_DIR}/onecloud_firmware.tar.bz2.tmp
	if [ "0" = "$?" ]
	then
		$(echo_log ":sucess download file complete ! " ${LOG_FILE})
		break
	fi
	sleep 30
done
if [ $i -le 3 ]
then
	$(echo_log ":now is update firmware... " ${LOG_FILE})
	nohup ${FIRMWARE_UPDATE} ${NEXT_VERSION:1:-1} >> ${LOG_FILE} 2>&1 &
	exit 0
fi
$(echo_log ":time out to download file! " ${LOG_FILE})













