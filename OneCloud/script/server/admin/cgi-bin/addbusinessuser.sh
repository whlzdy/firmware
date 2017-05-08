#!/bin/sh
TOOL_BASE="/opt/onecloud/script/server/appdata"
TOOL_FUNC="${TOOL_BASE}/bin/funcs.sh"

. ${TOOL_FUNC}

state="$(getUserLoginStatus)"

if [ "${state}" == "true" ]
then
	qs="${QUERY_STRING}"
	username=$(echo $qs | awk -F"&|=" '{print $2}')
       	password=$(echo $qs | awk -F"&|=" '{print $4}')
	passwordconf=$(echo $qs | awk -F"&|=" '{print $6}')
	
	usernamelen="${#username}"
	passwordlen="${#password}"
		
	if [ -n "${username}" ] && [ -n "${password}" ] && [ "${password}" == "${passwordconf}" ] && [ "${usernamelen}" -ge 6 ] && [ "${usernamelen}" -le 15 ] && [ "${passwordlen}" -ge 6 ] && [ "${passwordlen}" -le 15 ]
	then
		isExist=$(isUserExist ${username})
		if [ "${isExist}" == "false" ]
		then
		$(addBusinessUser ${username} ${password})
		log "BUSINESS_USER_ADD" "SUCCESS" "add business user and username is ${username}"
cat << EOF
$(cat ${TOOL_BASE}/addbusinessusersucc.html)
EOF
		else
		log "BUSINESS_USER_ADD" "FAIL" "add business user exist and username is ${username}"
cat << EOF
$(cat ${TOOL_BASE}/addbusinessuseralreadyexist.html)
EOF
		fi
	else
		log "BUSINESS_USER_ADD" "FAIL" "add business user fail and username is ${username}"
cat << EOF
$(cat ${TOOL_BASE}/addbusinessuserfail.html)
EOF
	fi
else
cat << EOF
$(cat ${TOOL_BASE}/index.html)
EOF
fi
