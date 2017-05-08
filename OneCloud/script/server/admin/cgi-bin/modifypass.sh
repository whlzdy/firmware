#!/bin/sh
TOOL_BASE="/opt/onecloud/script/server/appdata"
TOOL_FUNC="${TOOL_BASE}/bin/funcs.sh"

. ${TOOL_FUNC}

state="$(getUserLoginStatus)"

if [ "${state}" == "true" ]
then
	qs="${QUERY_STRING}"
	old=$(echo $qs | awk -F"&|=" '{print $2}')
       	new=$(echo $qs | awk -F"&|=" '{print $4}')
	newconf=$(echo $qs | awk -F"&|=" '{print $6}')
	
        password_file="$(getPassword)"
	newlength="${#new}"

	if [ "${old}" == "${password_file}" ] && [ -n "${new}" ] && [ "${new}" == "${newconf}" ] && [ "${newlength}" -ge 6 ] && [ "${newlength}" -le 15 ]
	then
		$(modifyAdminPassword ${new})
		log "ADMIN_PASSWORD_MODIFY" "SUCCESS" "admin user modify password"
cat << EOF
$(cat $TOOL_BASE/adminsuccjumpmainpage.html)
EOF
	else
		log "ADMIN_PASSWORD_MODIFY" "FAIL" "admin user fail to modify password"
cat << EOF
$(modifyPassFail)
EOF
	fi
else
cat << EOF
$(cat ${TOOL_BASE}/index.html)
EOF
fi
