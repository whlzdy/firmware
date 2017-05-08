#!/bin/sh
TOOL_BASE="/opt/onecloud/script/server/appdata"
TOOL_FUNC="${TOOL_BASE}/bin/funcs.sh"

. ${TOOL_FUNC}

state="$(getUserLoginStatus)"

if [ "${state}" == "true" ]
then
cat << EOF
$(getMainPageServerInfo)
EOF
else
cat << EOF
$(cat ${TOOL_BASE}/index.html)
EOF
fi
