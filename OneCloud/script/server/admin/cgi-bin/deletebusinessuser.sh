#!/bin/sh
TOOL_BASE="/opt/onecloud/script/server/appdata"
TOOL_FUNC="${TOOL_BASE}/bin/funcs.sh"

. ${TOOL_FUNC}

state="$(getUserLoginStatus)"

if [ "${state}" == "true" ]
then
	qs="${QUERY_STRING}"
	username=$(echo $qs | awk -F"&|=" '{print $2}')

	if [ -n "${username}" ]
	then
		$(deleteBusinessUser ${username})
		log "BUSINESS_USER_DELETE" "SUCCESS" "delete business user and username is ${username}"
	fi
cat << EOF
$(cat ${TOOL_BASE}/jumpbusinessuser.html)
EOF
else
cat << EOF
$(cat ${TOOL_BASE}/index.html)
EOF
fi
