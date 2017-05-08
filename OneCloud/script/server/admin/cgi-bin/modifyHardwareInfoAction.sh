#!/bin/sh
TOOL_BASE="/opt/onecloud/script/server/appdata"
TOOL_FUNC="${TOOL_BASE}/bin/funcs.sh"

. ${TOOL_FUNC}

state="$(getUserLoginStatus)"

if [ "${state}" == "true" ]
then
	qs="${QUERY_STRING}"
	uuid=$(echo $qs | awk -F"&|=" '{print $2}')
	com1=$(echo $qs | awk -F"&|=" '{print $4}')
	com2=$(echo $qs | awk -F"&|=" '{print $6}')
	
	if [ -n "${uuid}" ] && [ -n "${com1}" ] && [ -n "${com2}" ]
	then
		$(modifyHardwareInfoConfig ${uuid} ${com1} ${com2})
		log "HARDWARE_CONFIG_MODIFY" "SUCCESS" "modify hardware configuration and parameters are: ${uuid} ${com1} ${com2}"	
cat << EOF
$(cat ${TOOL_BASE}/modifyhardwareconfigsucc.html)
EOF
	else
cat << EOF
$(cat ${TOOL_BASE}/modifyharewareconfigfail.html)
EOF
	fi
else
cat << EOF
$(cat ${TOOL_BASE}/index.html)
EOF
fi
