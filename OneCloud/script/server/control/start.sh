#!/bin/bash
# variables definition
rm -f /opt/onecloud/script/server/control/start_status.tmp  

. /opt/onecloud/script/server/control/funcs.sh
. /opt/onecloud/config/onekeycontrol.property 

$(startWriteStatus 1 0)

###########################################################
# Load the cabinet & server config
###########################################################
serverconfigpath="/opt/onecloud/config/servers"
primaryip="$(cat ${serverconfigpath}/primaryserver.property | grep '^ip=' | awk -F '=' {'print $2'})"
primarybmctarget="$(cat ${serverconfigpath}/primaryserver.property | grep '^bmctarget=' | awk -F '=' {'print $2'})"

var=0
secondaryfiles="$(/bin/ls -rt ${serverconfigpath}/secondary* | /bin/grep -v total)"
for file in ${secondaryfiles}
do
	secondaryip="$(cat ${file} | grep '^ip=' | awk -F '=' {'print $2'})"
	secondarybmctarget="$(cat ${file} | grep '^bmctarget=' | awk -F '=' {'print $2'})"
	secondaryid="$(echo ${file} | awk -F "_" '{print $2}' | awk -F "." '{print $1}')"
	secondaryid_ips[${var}]="${secondaryid}_${secondaryip}_${secondarybmctarget}"
	secondaryips[${var}]="${secondaryip}"
	var=`expr $var + 1`
done

log "START_POWERONE_BEGIN" "SUCCESS" "begin to start powerone, and parameter is iscsiip: ${iscsiip} primaryip:${primaryip} secondaryips:${secondaryips[*]} appip:${appip} statuscheckerscript:${statuscheckerscript}"

###########################################################
# 0: check disk array is already on
###########################################################
if [ ${iscsiip1} != "null" ]
then
	# init
	$(startWriteStatus 1 0)

	# checking
	$(startWriteStatus 1 1)

	for((k=1;k<32;k++))
	do
		result="$(checkIscsiConnection ${iscsiip1})"
		if [ "${result}" -eq "0" ]
		then
			# success
			log "CONNECT_ISCSI" "SUCCESS" "check iscsi ${iscsiip1} ${iscsiip2} success!"
			$(startWriteStatus 1 2 )
			break
		else
			log "CONNECT_ISCSI" "FAIL" "we have check iscsi ${iscsiip1} ${iscsiip2} for ${k} loops and fail!"
			sleep 15
		fi
	done

	if [ "${k}" -eq "32" ]
	then
        	log "CONNECT_ISCSI" "FAIL" "we have waited for ${k} loops! and finally fail"
	        log "START_POWERONE_END" "FAIL" "fail to connect iscsi"
        	echo "FAIL_CONNECT_ISCSI"
		$(startWriteStatus 1 3)
        	exit 5
	fi
fi


# wait bmc ip is up for protection
sleep 60


###########################################################
# 1:start primary server
###########################################################
# check if already startup, if not, then preceed or contiue
for((retry=1;retry<=2;retry++))
do
isConnectedPri=$(checkConnection ${primaryip})
if [ "${isConnectedPri}" -eq "0" ]
then
	# ip can ping correcly
        log "PING_PRIMARY" "SUCCESS" "Retry loops: ${retry} ping primaryip ${primaryip} success, no need to send ipmi message to start server!"
	$(startWriteStatus 2 2 "primaryserver")
else
	# can not ping
	log "PING_PRIMARY" "FAIL" "Retry loops: ${retry} ping primaryip ${primaryip} fail, we need to send ipmi message to start server!"
	$(startWriteStatus 2 0 "primaryserver")

	# start server using ipmitool command
	if [ "${retry}" -eq "1" ]
	then
		ipmitool -I lan -H ${primarybmctarget} -U ${user} -P ${password} chassis power on
	else
		ipmitool -I lan -H ${primarybmctarget} -U ${user} -P ${password} chassis power off
		sleep 30
		ipmitool -I lan -H ${primarybmctarget} -U ${user} -P ${password} chassis power on
	fi
	$(startWriteStatus 2 1 "primaryserver")	

	# wait 5mins for system on
	for((k=1;k<32;k++))
        do
        	statusPri=$(checkConnection ${primaryip})
		if [ "${statusPri}" -ne "0" ]
		then
			# not started, continue to wait
			log "PING_PRIMARY" "FAIL" "ping primaryip ${primaryip} fail, we have waited for ${k} loops!"
			sleep 15
		else
			# already started 
			log "START_PRIMARY" "SUCCESS" "start primaryip ${primaryip} success, we have waited for ${k} loops!"
			$(startWriteStatus 2 2 "primaryserver")
			break
		fi
	done

	if [ "${k}" -eq "32" ]
	then
		log "START_PRIMARY" "FAIL" "start primaryip ${primaryip} fail, we have waited for ${k} loops!"
		if [ "${retry}" -eq "2" ]
		then
			log "START_POWERONE_END" "FAIL" "start primaryip ${primaryip} fail"
			echo "FAIL_START_PRIMARY"
			$(startWriteStatus 2 3 "primaryserver")
			exit 1
		fi
	fi
 
fi
done

###########################################################
# 2:start secondary server
###########################################################
# check if already startup, if not, then preceed or contiue
for secondaryid_ip in ${secondaryid_ips[@]}
do
secondaryid="$(echo ${secondaryid_ip} | awk -F '_' {'print $1'})"
secondaryip="$(echo ${secondaryid_ip} | awk -F '_' {'print $2'})"
secondarybmctarget="$(echo ${secondaryid_ip} | awk -F '_' {'print $3'})"
# retry if not started
for((retry=1;retry<=2;retry++))
do
log "PING_SECONDARY" "SUCCESS" "Retry loops: ${retry} begin to check secondary server, server id: ${secondaryid} and server ip: ${secondaryip}!"
isConnectedSec=$(checkConnection ${secondaryip})
if [ "${isConnectedSec}" -eq "0" ]
then
        # ip can ping correcly
        log "PING_SECONDARY" "SUCCESS" "ping secondaryip ${secondaryip} success, no need to send ipmi message to start server!"
	$(startWriteStatus 3 2 "secondaryserver_${secondaryid}")
else
        # can not ping
        log "PING_SECONDARY" "FAIL" "ping secondaryip ${secondaryip} fail, we need to send ipmi message to start server!"
	$(startWriteStatus 3 0 "secondaryserver_${secondaryid}")

        # start server using ipmitool command
	if [ "${retry}" -eq "1" ]
        then
                ipmitool -I lan -H ${secondarybmctarget} -U ${user} -P ${password} chassis power on
        else
                ipmitool -I lan -H ${secondarybmctarget} -U ${user} -P ${password} chassis power off
		sleep 30
		ipmitool -I lan -H ${secondarybmctarget} -U ${user} -P ${password} chassis power on
        fi

	$(startWriteStatus 3 1 "secondaryserver_${secondaryid}")

        # wait 5mins for system on
        for((q=1;q<32;q++))
        do
                statusSec=$(checkConnection ${secondaryip})
                if [ "${statusSec}" -ne "0" ]
                then
                        # not started, continue to wait
                        log "PING_SECONDARY" "FAIL" "ping secondaryip ${secondaryip} fail, we have waited for ${q} loops!"
                        sleep 15
                else
                        # already started 
                        log "START_SECONDARY" "SUCCESS" "start secondaryip ${secondaryip} success, we have waited for ${q} loops!"
			$(startWriteStatus 3 2 "secondaryserver_${secondaryid}")
                        break
                fi
        done

        if [ "${q}" -eq "32" ]
        then
                log "START_SECCONDARY" "FAIL" "start secondaryip ${secondaryip} fail, we have waited for ${q} loops!"
		if [ "${retry}" -eq "2" ]
		then
			log "START_POWERONE_END" "FAIL" "start secondaryip ${secondaryip} fail"
			echo "FAIL_START_SECONDARY"
			$(startWriteStatus 3 3 "secondaryserver_${secondaryid}")
                	exit 4
		fi
        fi

fi
done
done

###########################################################
# 3:check if ip is accessable
###########################################################
for((j=1;j<32;j++))
do
	isConnected=$(checkConnection ${appip})
	if [ "${isConnected}" -eq "0" ]
	then
		log "PING_APP" "SUCCESS" "ping app ip ${appip} success!"
		break
	else
		log "PING_APP" "FAIL" "ping app ip ${appip} failed, we have waited ${j} loops"
		sleep 15
	fi	
done

if [ "${j}" -eq "32" ]
then
	log "PING_APP" "FAIL" "ping app ip ${appip} failed finally, we have waited ${j} loops"
	log "START_POWERONE_END" "FAIL" "ping appip ${appip} fail"
	echo "FAIL_PING_APP"
	$(startWriteStatus 4 -1)
	exit 2
fi

# wait until secondary server is stable
sleep 300

###########################################################
# 4:check app status every 15 seconds, if not normal in 20 minute then log error
###########################################################
$(startWriteStatus 4 1)

for((retry=1;retry<=2;retry++))
do
status=$(checkAppStatus ${appip} ${statuscheckerscript} "start")
if [ -z "${status}" ] || [ "${status}" -ne "0" ]
then
	$(startApp ${appip} ${startscript})

	for((i=1;i<80;i++))
	do
		status=$(checkAppStatus ${appip} ${statuscheckerscript} "start")
		if [ -z "${status}" ]
		then
			log "APP_STATUS" "FAIL" "result is null, we have waited ${i} loops, so need to wait for a while!"
                	sleep 15 
		elif [ "${status}" -ne "0" ]
		then
			log "APP_STATUS" "FAIL" "app is not ruuning and status is ${status}, we have waited ${i} loops, so need to wait for a while!"
			sleep 15	
		else
			# checker return success and return
			log "APP_START" "SUCCESS" "app is now ruuning and status is ${status}, we have waited ${i} loops, now exit!"	
			log "START_POWERONE_END" "SUCCESS" "every thing is right, congratulations!"
			echo "SUCCESS_START_POWERONE"
			$(startWriteStatus 4 2)
			exit 0
		fi
	done

	log "APP_START" "FAIL" "Retry loops: ${retry} app is not ruuning finally, we have waited ${i} loops!"
	# wait enough loops and fail
	if [ "${retry}" -eq "2" ]
	then	
		log "START_POWERONE_END" "FAIL" "start application fail!"
		echo "FAIL_START_APP"
		$(startWriteStatus 4 3)
		exit 3
	fi
else
	log "APP_START" "SUCCESS" "Retry loops: ${retry} app is already ruuning and status is ${status}, now exit!"
	log "START_POWERONE_END" "SUCCESS" "every thing is right, congratulations!"
   	echo "SUCCESS_START_POWERONE"
	$(startWriteStatus 4 2)
	exit 0
fi
done
