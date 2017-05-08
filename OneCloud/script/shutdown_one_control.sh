#bin/bash

ONECLOUD_HOME=/opt/onecloud
cd ${ONECLOUD_HOME}

DATE_NOW=`date +%Y-%m-%d_%H:%M:%S`
LOG_FILE=/opt/onecloud/log/one_control.log

echo ${DATE_NOW}": Execute shutdown_one_control.sh..." >> ${LOG_FILE}

PROC_ID=`ps -ef|grep "bin/watch_dog"|grep -v "grep"|awk '{print $2}'`
if [ "${PROC_ID}" != "" ]
then
    kill -9 ${PROC_ID}
fi
echo "stop watch dog...PID="${PROC_ID} >> ${LOG_FILE}

PROC_ID=`ps -ef|grep "bin/app_service"|grep -v "grep"|awk '{print $2}'`
if [ "${PROC_ID}" != "" ]
then
    kill -9 ${PROC_ID}
fi
echo "stop app service...PID="${PROC_ID} >> ${LOG_FILE}

PROC_ID=`ps -ef|grep "bin/main_daemon"|grep -v "grep"|awk '{print $2}'`
if [ "${PROC_ID}" != "" ]
then
    kill -9 ${PROC_ID}
fi
echo "stop main daemon...PID="${PROC_ID} >> ${LOG_FILE}

PROC_ID=`ps -ef|grep "bin/sim_daemon"|grep -v "grep"|awk '{print $2}'`
if [ "${PROC_ID}" != "" ]
then
    kill -9 ${PROC_ID}
fi
echo "stop sim daemon...PID="${PROC_ID} >> ${LOG_FILE}

PROC_ID=`ps -ef|grep "bin/script_daemon"|grep -v "grep"|awk '{print $2}'`
if [ "${PROC_ID}" != "" ]
then
    kill -9 ${PROC_ID}
fi
echo "stop script daemon...PID="${PROC_ID} >> ${LOG_FILE}

PROC_ID=`ps -ef|grep "bin/gpio_daemon"|grep -v "grep"|awk '{print $2}'`
if [ "${PROC_ID}" != "" ]
then
    kill -9 ${PROC_ID}
fi
echo "stop gpio daemon...PID="${PROC_ID} >> ${LOG_FILE}

PROC_ID=`ps -ef|grep "bin/electricity_daemon"|grep -v "grep"|awk '{print $2}'`
if [ "${PROC_ID}" != "" ]
then
    kill -9 ${PROC_ID}
fi
echo "stop electricity daemon...PID="${PROC_ID} >> ${LOG_FILE}

PROC_ID=`ps -ef|grep "bin/temperature_daemon"|grep -v "grep"|awk '{print $2}'`
if [ "${PROC_ID}" != "" ]
then
    kill -9 ${PROC_ID}
fi
echo "stop temperature daemon...PID="${PROC_ID} >> ${LOG_FILE}



