
###############################################
#               www.onecloud.cn
#          onecloud firmware_update.sh
#                 Author: whl
#               date:2017-11-2
###############################################

#!/bin/sh
#/opt/onecloud
. /opt/onecloud/script/update/firmware_common.sh 

ONECLOUD_HOME=/opt/onecloud
cd ${ONECLOUD_HOME}
LOG_FILE=/opt/log/firmware_update.log
STOP_SCRIPT=${ONECLOUD_HOME}/script/shutdown_one_control.sh
STAR_SCRIPT=${ONECLOUD_HOME}/script/startup_one_control.sh
QUERY_DOWNLOAD=${ONECLOUD_HOME}/script/update/query_download.sh
ONECLOUD_CONFIG_FILE=${ONECLOUD_HOME}/config/onecloud.ini

#current firmware dir
CRT_FIRMWARE_DIR="/opt/onecloud"
#last firmware dir
LAST_FIRMWARE_DIR="/opt/onecloud/last"
#httpd store dir"
HTTPD_STORE_DIR="/opt/httpd"
ONECLOUD_HTTPD_CONFIG_FILE=${HTTPD_STORE_DIR}/config/onecloud.ini

$(echo_log ":================ welcome to firmware_update.sh ! ==================" ${LOG_FILE})

if [ -f "${HTTPD_STORE_DIR}/onecloud_firmware.tar.bz2.tmp" ];then
    	# decode file 2 destination
	tar -xvjf /opt/httpd/onecloud_firmware.tar.bz2.tmp -C ${HTTPD_STORE_DIR}
	if [ "$?" -eq  "0" ]
	then 
		#s0:firmware verify 
		if [ -e ${ONECLOUD_CONFIG_FILE} ]
		then
		    	FIRMWARE_VERSION=`cat ${ONECLOUD_HTTPD_CONFIG_FILE} | grep "firmware_version" | awk -F "=" '{print $2}'`
			if [ "$1" = $FIRMWARE_VERSION ]
			then
				$(echo_log ":firmware file verify passed! " ${LOG_FILE})
			else
				$(echo_log ":firmware file verify failed! " ${LOG_FILE})
				exit 10
			fi

		else
			$(echo_log ":onecloud.ini is not exist! " ${LOG_FILE})
			exit 9
		fi
	   	#s1: killall progress
		`${STOP_SCRIPT}`
		sleep 30
		#s2: curent version -> last version
		#clear 
		rm /opt/last/* -rf
		cp -rf /opt/onecloud/* /opt/last/ 
		if [ "$?" -eq "0" ]
		then
			$(echo_log ":cp current version -> last version sucess! " ${LOG_FILE})
		else 
			$(echo_log ":cp current version -> last version failed! " ${LOG_FILE})
			exit 2
		fi
   		#s3: tmp version -> curent version
		#clear 
		rm /opt/onecloud/* -rf
		cp -rf /opt/httpd/* /opt/onecloud/ 
		if [ "$?" -eq "0" ]
		then
			$(echo_log ":cp tmp version -> current version sucess! " ${LOG_FILE})
		else
			$(echo_log ":cp tmp version -> current version failed! " ${LOG_FILE})
			exit 1
		fi
		#s4: starup progress
		#remove tmp firmware.tar.bz2
		#clear 
		rm /opt/httpd/* -rf
		#starup 
		# if OK, then init; then rollback 
		`${STAR_SCRIPT}`
		PROC_ID=`ps -ef|grep "bin/main_daemon"|grep -v "grep"|awk '{print $2}'`
		if [ "${PROC_ID}" = "" ]
		then
			#rollback
			$(echo_log ":main daemon start failed so need to rollback... " ${LOG_FILE})
			nohup ${STOP_SCRIPT} >> ${LOG_FILE} 2>&1 &
			sleep 30
			#last version -> current version
			cp -rf /opt/last/* /opt/onecloud
			if [ "$?" -eq "0" ]
			then
				`${STAR_SCRIPT}`
				$(echo_log ":rollback sucess! " ${LOG_FILE})
			else
				$(echo_log ":rollback failed! " ${LOG_FILE})
				exit 3
			fi
		else
			#report current version
			$(echo_log ":update sucess" ${LOG_FILE})
			nohup ${QUERY_DOWNLOAD} "init" > ${LOG_FILE} 2>&1 &
			exit 0
		
		fi

		
	else
		$(echo_log ":tar decode failed!" ${LOG_FILE})
		exit 4
	fi
  
else
	$(echo_log ":onecloud_firmware.tar.bz2 is not exit!" ${LOG_FILE})
	exit 5
fi



