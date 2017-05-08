#!/bin/sh
TOOL_BASE="/opt/onecloud/script/server/appdata"
TOOL_FUNC="${TOOL_BASE}/bin/funcs.sh"

. ${TOOL_FUNC}

state="$(getUserLoginStatus)"

if [ "${state}" == "true" ]
then
	qs="${QUERY_STRING}"
	server1ip=$(echo $qs | awk -F"&|=" '{print $2}')
	server1mask=$(echo $qs | awk -F"&|=" '{print $4}')
	server1gateway=$(echo $qs | awk -F"&|=" '{print $6}')
	server1mac=$(echo $qs | awk -F"&|=" '{print $8}')
	server1catagory=$(echo $qs | awk -F"&|=" '{print $10}')

	server2ip=$(echo $qs | awk -F"&|=" '{print $12}')
        server2mask=$(echo $qs | awk -F"&|=" '{print $14}')
        server2gateway=$(echo $qs | awk -F"&|=" '{print $16}')
        server2mac=$(echo $qs | awk -F"&|=" '{print $18}')
        server2catagory=$(echo $qs | awk -F"&|=" '{print $20}')

	server1mac=$(echo ${server1mac} | sed "s/%3A/:/g")
	server2mac=$(echo ${server2mac} | sed "s/%3A/:/g")


	if [ -n "${server1ip}" ] && [ -n "${server1mask}" ] && [ -n "${server1gateway}" ] && [ -n "${server1mac}" ] && [ -n "${server1catagory}" ] && [ -n "${server2ip}" ] && [ -n "${server2mask}" ] && [ -n "${server2gateway}" ] && [ -n "${server2mac}" ] && [ -n "${server2catagory}" ]
	then
		$(modifyServerInfoConfig ${server1ip} ${server1mask} ${server1gateway} ${server1mac} ${server1catagory} ${server2ip} ${server2mask} ${server2gateway} ${server2mac} ${server2catagory})
		log "SERVER_CONFIG_MODIFY" "SUCCESS" "modify server configuration and parameters are: ${server1ip} ${server1mask} ${server1gateway} ${server1mac} ${server1catagory} ${server2ip} ${server2mask} ${server2gateway} ${server2mac} ${server2catagory}"	
cat << EOF
$(cat ${TOOL_BASE}/modifyserverconfigsucc.html)
EOF
	else
cat << EOF
$(cat ${TOOL_BASE}/modifyserverconfigfail.html)
EOF
	fi
else
cat << EOF
$(cat ${TOOL_BASE}/index.html)
EOF
fi
