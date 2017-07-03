#bin/bash

ONECLOUD_HOME=/opt/onecloud
cd ${ONECLOUD_HOME}

LOG_FILE=/opt/onecloud/log/one_control.log

###########################################################
# shutdown the daemon proc
###########################################################
${ONECLOUD_HOME}/script/shutdown_one_control.sh

###########################################################
# startup the daemon proc
###########################################################
DATE_NOW=`date +%Y-%m-%d_%H:%M:%S`
echo ${DATE_NOW}": Execute startup_one_control.sh..." >> ${LOG_FILE}

DATE_STR=`date +%Y-%m-%d`

LOG_FILE_DAEMON=/opt/onecloud/log/lbs_daemon.log.${DATE_STR}
nohup /opt/onecloud/bin/lbs_daemon >> ${LOG_FILE_DAEMON} 2>&1 &
echo "start lbs daemon..."$ >> ${LOG_FILE}
sleep 1

#LOG_FILE_DAEMON=/opt/onecloud/log/gpsusb_daemon.log.${DATE_STR}
#nohup /opt/onecloud/bin/gpsusb_daemon >> ${LOG_FILE_DAEMON} 2>&1 &
#echo "start gpsusb daemon..."$ >> ${LOG_FILE}
#sleep 1

#LOG_FILE_DAEMON=/opt/onecloud/log/gps_daemon.log.${DATE_STR}
#nohup /opt/onecloud/bin/gps_daemon >> ${LOG_FILE_DAEMON} 2>&1 &
#echo "start gps daemon..."$ >> ${LOG_FILE}
#sleep 1

LOG_FILE_DAEMON=/opt/onecloud/log/voice_daemon.log.${DATE_STR}
nohup /opt/onecloud/bin/voice_daemon >> ${LOG_FILE_DAEMON} 2>&1 &
echo "start voice daemon..."$ >> ${LOG_FILE}
sleep 1

LOG_FILE_DAEMON=/opt/onecloud/log/electricity_daemon.log.${DATE_STR}
nohup /opt/onecloud/bin/electricity_daemon >> ${LOG_FILE_DAEMON} 2>&1 &
echo "start electricity daemon..."$ >> ${LOG_FILE}
sleep 1

LOG_FILE_DAEMON=/opt/onecloud/log/temperature_daemon.log.${DATE_STR}
nohup /opt/onecloud/bin/temperature_daemon >> ${LOG_FILE_DAEMON} 2>&1 &
echo "start temperature daemon..."$ >> ${LOG_FILE}

LOG_FILE_DAEMON=/opt/onecloud/log/watch_dog.log.${DATE_STR}
nohup /opt/onecloud/bin/watch_dog >> ${LOG_FILE_DAEMON} 2>&1 &
echo "start watch dog..."$ >> ${LOG_FILE}
sleep 1

LOG_FILE_DAEMON=/opt/onecloud/log/script_daemon.log.${DATE_STR}
nohup /opt/onecloud/bin/script_daemon >> ${LOG_FILE_DAEMON} 2>&1 &
echo "start script daemon..."$ >> ${LOG_FILE}
sleep 1

LOG_FILE_DAEMON=/opt/onecloud/log/gpio_daemon.log.${DATE_STR}
nohup /opt/onecloud/bin/gpio_daemon >> ${LOG_FILE_DAEMON} 2>&1 &
echo "start gpio daemon..."$ >> ${LOG_FILE}
sleep 1

LOG_FILE_DAEMON=/opt/onecloud/log/sim_daemon.log.${DATE_STR}
nohup /opt/onecloud/bin/sim_daemon >> ${LOG_FILE_DAEMON} 2>&1 &
echo "start sim daemon..."$ >> ${LOG_FILE}

LOG_FILE_DAEMON=/opt/onecloud/log/cabinet_cron.log
/opt/onecloud/script/cabinet/cabinet_cron.sh >> ${LOG_FILE_DAEMON} 2>&1 &
echo "invoke cabinet cron to check cabinet info..."$ >> ${LOG_FILE}

LOG_FILE_DAEMON=/opt/onecloud/log/main_daemon.log.${DATE_STR}
nohup /opt/onecloud/bin/main_daemon >> ${LOG_FILE_DAEMON} 2>&1 &
echo "start main daemon..."$ >> ${LOG_FILE}

sleep 18
LOG_FILE_DAEMON=/opt/onecloud/log/app_service.log.${DATE_STR}
nohup /opt/onecloud/bin/app_service >> ${LOG_FILE_DAEMON} 2>&1 &
echo "start app_service..."$ >> ${LOG_FILE}


