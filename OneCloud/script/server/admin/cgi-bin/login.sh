#!/bin/sh
TOOL_BASE="/opt/onecloud/script/server/appdata"
TOOL_FUNC="${TOOL_BASE}/bin/funcs.sh"

. ${TOOL_FUNC}

state="$(getUserLoginStatus)"

if [ "${state}" == "true" ]
then
cat << EOF
$(cat ${TOOL_BASE}/mainpage.html)
EOF
else
	qs="${QUERY_STRING}"
	username=$(echo $qs | awk -F"&|=" '{print $2}')
	password=$(echo $qs | awk -F"&|=" '{print $4}')
	
	username_file="$(getUserName)"
	password_file="$(getPassword)"
	
	if [ -n "${username}" ] && [ -n "${password}" ] && [ "${username}" == "${username_file}" ] &&  [ "${password}" == "${password_file}" ]
	then
		$(modifyUserLoginStatusTrue)
		log "ADMIN_LOGIN" "SUCCESS" "username and password match"
cat << EOF
$(cat ${TOOL_BASE}/jumpmainpage.html)
EOF
	else
		if [ -n "${username}" ] && [ -n ""${password} ]
		then
			log "ADMIN_LOGIN" "FAIL" "username and password not match and username is ${username}"
		fi
cat << EOF
$(cat ${TOOL_BASE}/index.html)
EOF
	fi
fi
