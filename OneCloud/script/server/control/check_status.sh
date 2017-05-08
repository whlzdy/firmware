#!/bin/bash
. /opt/onecloud/script/server/control/funcs.sh
serverconfigpath="/opt/onecloud/config/servers"
statusfile="/opt/onecloud/script/server/control/server_status.tmp"

echo "" > "${statusfile}"
files="$(/bin/ls ${serverconfigpath} | /bin/grep -v total)"
for file in ${files}
do
	name="$(cat ${serverconfigpath}/${file} | grep 'name=' | awk -F "=" '{print $2}')"
	ip="$(cat ${serverconfigpath}/${file} | grep 'ip=' | awk -F "=" '{print $2}')"
		
	devicetype=$(getType ${file})
	
	if [ -n "${ip}" ]
	then
		isConnected=$(checkConnection ${ip})
		if [ "${isConnected}" -eq "0" ]
        	then
			status="1"
		else
			status="0"
		fi
		echo "${name} ${devicetype} ${status} ${ip}" >> "${statusfile}"
	fi
done
