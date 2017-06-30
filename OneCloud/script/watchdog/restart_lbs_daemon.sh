#bin/bash -x

ONECLOUD_HOME=/opt/onecloud
cd ${ONECLOUD_HOME}

PROC_ID=`ps -ef|grep "bin/lbs_daemon"|grep -v "grep"|awk '{print $2}'`
if [ "${PROC_ID}" != "" ]
then
    kill -9 ${PROC_ID}
fi

DATE_STR=`date +%Y-%m-%d`
LOG_FILE=/opt/onecloud/log/lbs_daemon.log.${DATE_STR}

nohup /opt/onecloud/bin/lbs_daemon >> ${LOG_FILE} 2>&1 &
