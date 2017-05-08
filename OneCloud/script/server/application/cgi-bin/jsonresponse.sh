#!/bin/sh
TOOL_BASE="/opt/onecloud/script/server/appdata"
TOOL_FUNC="${TOOL_BASE}/bin/funcs.sh"

. ${TOOL_FUNC}

url="${REQUEST_URI}"
path="$(echo ${url} | awk -F'/|?' {'print $2'})"
qs="$(echo ${url} | awk -F'/|?' {'print $3'})"

if [ "${path}" == "favicon.ico" ]
then
	exit 0
fi

cat << EOF
Content-type:application/json; charset=UTF-8
$(doJsonResponse ${path} ${qs})
EOF
