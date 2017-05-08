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

	usernamelen="${#username}"
        passwordlen="${#password}"

	if [ -n "${username}" ] && [ -n "${password}" ] && [ "${usernamelen}" -ge 6 ] && [ "${usernamelen}" -le 15 ] && [ "${passwordlen}" -ge 6 ] && [ "${passwordlen}" -le 15 ] 
	then
		isExist=$(isUserExist ${username})
                if [ "${isExist}" == "false" ]
                then
cat << EOF
$(cat ${TOOL_BASE}/modifybusinessusernotexist.html)
EOF
		else
			$(modifybusinessuser ${username} ${password})
			log "BUSINESS_USER_MODIFY" "SUCCESS" "modify business user and username is ${username}"	
cat << EOF
$(cat ${TOOL_BASE}/modifybusinessusersucc.html)
EOF
		fi	
	else
cat << EOF
$(cat ${TOOL_BASE}/modifybusinessuserfail.html)
EOF
	fi
else
cat << EOF
$(cat ${TOOL_BASE}/index.html)
EOF
fi
