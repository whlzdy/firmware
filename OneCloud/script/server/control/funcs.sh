#!/bin/bash
logpath="/opt/onecloud/script/server/log"
logfile="${logpath}/running.log"
maxsize=10485760
startstatusfile="/opt/onecloud/script/server/control/start_status.tmp"
stopstatusfile="/opt/onecloud/script/server/control/stop_status.tmp"

function checkConnection {
	ip="${1}"
	ping -q -c3 ${ip} > /dev/null 2>&1
	echo "$?"
}

function checkIscsiConnection {
	ip1="${1}"
	#ip2="${2}"
        #ping -q -c3 ${ip1} > /dev/null 2>&1
        #result1=$?
	#ping -q -c3 ${ip2} > /dev/null 2>&1
	#resutl2=$?
	#if [ "${result1}" -eq "0" ] && [ "${result2}" -eq "0" ]
	#then
	#	echo "0"	
	#else
	#	log "CONNECT_ISCSI" "FAIL" "we have check iscsi and result is ${result1} and ${result2}!"	
	#	echo "1"
	#fi

	out=`curl -m 5 http://${ip1}/v2/index.html`
	result="$?"
	if [ "${result}" -eq "0" ]
	then
		notfound=`echo ${out} | grep 404`
		if [ -z "${notfound}" ]
		then
			echo "0"
		else
			echo "1"
		fi
	else
		echo "${result}"
	fi
}

function log {
        # max size is 10M
        size="$(stat --format=%s ${logfile})"
        if [ "${size}" -gt "${maxsize}" ]
        then
                cp -f ${logfile} "${logpath}/running$(date +'%Y-%m-%d-%H-%M-%S').log"
                rm -f ${logfile}
        fi     
	echo "`date`   ${1}   ${2}   ${3}" >> "${logfile}" 
}   

function checkAppStatus {
	ip="${1}"
	script="${2}"
	state="${3}"

	result=$(ssh -o ConnectTimeout=30 -o StrictHostKeyChecking=no clouder@${ip} "/bin/bash ${script} ${state}")
	echo "$result"
}

function stopApp {
	ip="${1}"
        script="${2}"
	
	ssh -o ConnectTimeout=30 -o StrictHostKeyChecking=no clouder@${ip} "/bin/bash ${script} > /dev/null 2>&1 &"
}

function startApp {
	ip="${1}"
        script="${2}"

        ssh -o ConnectTimeout=30 -o StrictHostKeyChecking=no clouder@${ip} "/bin/bash ${script} > /dev/null 2>&1 &"
}

function shutdownSystemVm {
	systemvmip="${1}"
	
	ssh -o ConnectTimeout=30 -o StrictHostKeyChecking=no ${systemvmip} "shutdown -h now > /dev/null 2>&1 &"
}

function startWriteStatus {
	 echo "$(date +%s) ${1} ${2} ${3}" > ${startstatusfile}
}

function stopWriteStatus {
	echo "$(date +%s) ${1} ${2} ${3}" > ${stopstatusfile}
}

function getType {
	filename="${1}"
	if [ "${filename}" == "firewall.property" ]
	then
		echo "2"
	elif [ "${filename}" == "router.property" ]
	then
		echo "4"
	elif [ "${filename}" == "storage.property" ]
	then
		echo "3"
	elif [ "${filename}" == "switch.property" ]
	then
		echo "1"
	elif [ "${filename}" == "ups.property" ]
	then
		echo "5"
	else
		echo "0"
	fi
}
