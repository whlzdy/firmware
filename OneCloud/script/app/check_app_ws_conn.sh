#bin/bash
###########################################################
# Backend job:check the app web socket connection
# 
#
###########################################################
ONECLOUD_HOME=/opt/onecloud
cd ${ONECLOUD_HOME}

DATE_NOW=`date +%Y-%m-%d_%H:%M:%S`
echo "${DATE_NOW} : run check app web socket job..." >> /opt/onecloud/log/one_cron.log

MISS_MATCH_COUNT=0

WS_CONN_IP=`netstat -an|grep :80 | grep ESTABLISHED | awk '{print $4}' | awk -F ":" '{print $1}'`
PPP_IP=`ip addr | grep "global ppp0" | awk '{print $2}'`

TRY_NUM=0
while(($TRY_NUM<5))
do
    if [ "${WS_CONN_IP}" != "" -a "${PPP_IP}" != "" -a "${WS_CONN_IP}" != "${PPP_IP}" ]
    then
       let "MISS_MATCH_COUNT++"
    else
       let "MISS_MATCH_COUNT=0"
    fi
    sleep 2
    let "TRY_NUM++"
done

if [ ${MISS_MATCH_COUNT} -gt 0 ]
then
   echo "Web socket connection exception, restart the app_service  process" >> /opt/onecloud/log/one_cron.log
   bash /opt/onecloud/script/watchdog/restart_app_service.sh
else
   echo "Web socket connection is ok" >> /opt/onecloud/log/one_cron.log
fi

