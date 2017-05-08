#!/bin/bash

rm -f /opt/onecloud/script/server/control/stop_status.tmp

# variables definition
. /opt/onecloud/script/server/control/funcs.sh
. /opt/onecloud/config/onekeycontrol.property

$(stopWriteStatus 1 0)

###########################################################
# Load the cabinet & server config
###########################################################
serverconfigpath="/opt/onecloud/config/servers"
primaryip="$(cat ${serverconfigpath}/primaryserver.property | grep 'ip=' | awk -F '=' {'print $2'})"
primarybmctarget="$(cat ${serverconfigpath}/primaryserver.property | grep '^bmctarget=' | awk -F '=' {'print $2'})"

var=0
secondaryfiles="$(/bin/ls -rt ${serverconfigpath}/secondary* | /bin/grep -v total)"
for file in ${secondaryfiles}
do
        secondaryip="$(cat ${file} | grep 'ip=' | awk -F '=' {'print $2'})"
	secondarybmctarget="$(cat ${file} | grep '^bmctarget=' | awk -F '=' {'print $2'})"
        secondaryid="$(echo ${file} | awk -F "_" '{print $2}' | awk -F "." '{print $1}')"
        secondaryid_ips[${var}]="${secondaryid}_${secondaryip}_${secondarybmctarget}"
        secondaryips[${var}]="${secondaryip}"
        var=`expr $var + 1`
done

log "STOP_POWERONE_BEGIN" "SUCCESS" "begin to stop powerone, and parameter is primaryip:${primaryip} secondaryips:${secondaryips[*]} systemvm1ip:${systemvm1ip} systemvm2ip:${systemvm2ip} appip:${appip} statuscheckerscript:${statuscheckerscript} stopscript:${stopscript}"

###########################################################
# 0:ping app vm
###########################################################
for((j=1;j<10;j++))
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

if [ "${j}" -eq "10" ]
then
        log "PING_APP" "FAIL" "ping app ip ${appip} failed finally, we have waited ${j} loops"
  	log "STOP_POWERONE_END" "FAIL" "ping app vm ${appip} fail"
      	echo "FAIL_PING_APP"
fi


###########################################################
# 1:stop application
###########################################################
$(stopWriteStatus 1 1)
for((retry=1;retry<=2;retry++))
do
status=$(checkAppStatus ${appip} ${statuscheckerscript} "stop")
if [ -n "${status}" ] && [ "${status}" -ne "0" ]
then
	log "APP_STATUS" "FAIL" "Retry loops: ${retry} app is running status is ${status}, we need to stop app anyway!"

	$(stopApp ${appip} ${stopscript})	

	# 2: check status
	for((i=1;i<80;i++))
	do
		statusStop=$(checkAppStatus ${appip} ${statuscheckerscript} "stop")
		if [ -n "${statusStop}" ] && [ "${statusStop}" -ne "0" ]
		then
			log "APP_STATUS" "FAIL" "we have waited for ${i} loops and status is ${statusStop}, we need to wait until app is stopped!"
			sleep 15
		else
			log "APP_STATUS" "SUCCESS" "we have waited for ${i} and status is ${statusStop}, finally stopped!"
			$(stopWriteStatus 1 2)
			break
		fi
	done

	if [ "${i}" -eq "80" ]
	then
		log "APP_STATUS" "FAIL" "we have waited for ${i} loops and app is still not stopped"
		if [ "${retry}" -eq "2" ]
		then
			log "STOP_POWERONE_END" "FAIL" "stop application fail"
	        	echo "FAIL_STOP_APP"
        		$(stopWriteStatus 1 3)
			exit 2
		fi
	fi
else
	$(stopWriteStatus 1 2)
	log "APP_STATUS" "FAIL" "Retry loops: ${retry} app is already stopped, no need to stop app again!"
fi
done

###########################################################
# 3:close system vm
###########################################################
$(stopWriteStatus 2 1 "systemvm1")
for((retry=1;retry<=2;retry++))
do
$(shutdownSystemVm ${systemvm1ip})

# check if is already shutdown
for((k=1;k<32;k++))
do
        isConnectedSys1=$(checkConnection ${systemvm1ip})
        if [ "${isConnectedSys1}" -eq "0" ]
        then
                log "PING_SYSVM" "SUCCESS" "Retry loops: ${retry} we have waited ${k} loops and ping sysvm1 ip ${systemvm1ip} success!"
                sleep 15
        else
                log "PING_SYSVM" "FAIL" "Retry loops: ${retry} ping sysvm1 ip ${systemvm1ip} failed, we have waited ${k} loops and it shutdown!"
                $(stopWriteStatus 2 2 "systemvm1")
		break
        fi
done

if [ "${k}" -eq "32" ]
then
        log "PING_SYSVM" "FAIL" "Retry loops: ${retry} ping sysvm1 ip ${systemvm1ip} success finally, we have waited ${k} loops"
        if [ "${retry}" -eq "2" ]
	then
		log "START_POWERONE_END" "FAIL" "fail shutdown sysvm1 ${systemvm1ip}"
        	echo "FAIL_STOP_SYSVM1"
        	$(stopWriteStatus 2 3 "systemvm1")
		exit 3
	fi
fi
done

# sleep for a while for protection
sleep 30

$(stopWriteStatus 2 1 "systemvm2")
for((retry=1;retry<=2;retry++))
do
$(shutdownSystemVm ${systemvm2ip})
for((k=1;k<32;k++))
do
        isConnectedSys2=$(checkConnection ${systemvm2ip})
        if [ "${isConnectedSys2}" -eq "0" ]
        then
                log "PING_SYSVM" "SUCCESS" "Retry loops: ${retry} we have waited ${k} loops and ping sysvm2 ip ${systemvm2ip} success!"
                sleep 15
        else
                log "PING_SYSVM" "FAIL" "Retry loops: ${retry} ping sysvm2 ip ${systemvm2ip} failed, we have waited ${k} loops and it shutdown!"
                $(stopWriteStatus 2 2 "systemvm2")
		break
        fi
done

if [ "${k}" -eq "32" ]
then
        log "PING_SYSVM" "FAIL" "Retry loops: ${retry} ping sysvm2 ip ${systemvm2ip} success finally, we have waited ${k} loops"
        if [ "${retry}" -eq "2" ]
        then
		log "START_POWERONE_END" "FAIL" "fail shutdown sysvm2 ${systemvm2ip}"
        	echo "FAIL_STOP_SYSVM2"
		$(stopWriteStatus 2 3 "systemvm2")
		exit 4
	fi
fi
done

# sleep for a while for protection
sleep 60

###########################################################
# 4:close xenserver
###########################################################
# stop secondary server using ipmitool command
for secondaryid_ip in ${secondaryid_ips[@]}
do
secondaryid="$(echo ${secondaryid_ip} | awk -F '_' {'print $1'})"
secondaryip="$(echo ${secondaryid_ip} | awk -F '_' {'print $2'})"
secondarybmctarget="$(echo ${secondaryid_ip} | awk -F '_' {'print $3'})"
$(stopWriteStatus 3 1 "secondaryserver_${secondaryid}")

for((retry=1;retry<=2;retry++))
do
ipmitool -I lan -H ${secondarybmctarget} -U ${user} -P ${password} chassis power off
# check secondary server is stopped
for((k=1;k<32;k++))
do
        isConnectedSec=$(checkConnection ${secondaryip})
        if [ "${isConnectedSec}" -eq "0" ]
        then
                log "PING_SECONDARY" "SUCCESS" "Retry loops: ${retry} we have waited ${k} loops and ping secondary ip ${secondaryip} success!"
                sleep 15
        else
                log "PING_SECONDARY" "FAIL" "Retry loops: ${retry} ping secondary ip ${secondaryip} failed, we have waited ${k} loops and it shutdown!"
                $(stopWriteStatus 3 2 "secondaryserver_${secondaryid}")
		break
        fi
done

if [ "${k}" -eq "32" ]
then
        log "PING_SECONDARY" "FAIL" "Retry loops: ${retrt} ping secondaryip ip ${secondaryip} success finally, we have waited ${k} loops"
        if [ "${retry}" -eq "2" ]
	then
		log "START_POWERONE_END" "FAIL" "fail shutdown secondaryip ${secondaryip}"
        	echo "FAIL_STOP_SECONDARY_SERVER"
        	$(stopWriteStatus 3 3 "secondaryserver_${secondaryid}")
		exit 5
	fi
fi
done
done


# sleep for a while for protection
sleep 60

$(stopWriteStatus 4 1 "primaryserver")
for((retry=1;retry<=2;retry++))
do
# stop primary server using ipmitool command
ipmitool -I lan -H ${primarybmctarget} -U ${user} -P ${password} chassis power off

# check secondary server is stopped
for((k=1;k<32;k++))
do
        isConnectedPri=$(checkConnection ${primaryip})
        if [ "${isConnectedPri}" -eq "0" ]
        then
                log "PING_PRIMARY" "SUCCESS" "Retry loops: ${retry} we have waited ${k} loops and ping primaryip  ${primaryip} success!"
                sleep 15
        else
	        log "PING_PRIMARY" "FAIL" "Retry loops: ${retry} ping primaryip ip ${primaryip} failed, we have waited ${k} loops and it shutdown!"
            	log "STOP_POWERONE_END" "SUCCESS" "everything is stopped, congratulations!"
        	echo "SUCCESS_STOP_POWERONE"
		
		# sleep for a while for protection
		sleep 120
		
		$(stopWriteStatus 4 2 "primaryserver")
        	exit 0
        fi
done

if [ "${k}" -eq "32" ]
then
        log "PING_PRIMARY" "FAIL" "Retry loops: ${retry} ping primaryip ip ${primaryip} success finally, we have waited ${k} loops"
        if [ "${retry}" -eq "2" ]
	then
		log "START_POWERONE_END" "FAIL" "fail shutdown primaryip ${primaryip}"
        	echo "FAIL_STOP_PRIMARY_SERVER"
        	$(stopWriteStatus 4 3 "primaryserver")
		exit 6
	fi
fi
done
