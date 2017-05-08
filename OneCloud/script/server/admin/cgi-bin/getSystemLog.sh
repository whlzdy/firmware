#!/bin/sh
TOOL_BASE="/opt/onecloud/script/server/appdata"
TOOL_FUNC="${TOOL_BASE}/bin/funcs.sh"

. ${TOOL_FUNC}

state="$(getUserLoginStatus)"

if [ "${state}" == "true" ]
then
cat << EOF
Content-type:text/html; charset=UTF-8
Content-Disposition:attachment;filename=system$(date +'%Y-%m-%d-%H-%M-%S').log

$(getSystemLog)
EOF
else
cat << EOF
$(cat ${TOOL_BASE}/index.html)
EOF
fi
