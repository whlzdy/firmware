#!/bin/sh
TOOL_BASE="/opt/onecloud/script/server/appdata"
TOOL_FUNC="${TOOL_BASE}/bin/funcs.sh"

. ${TOOL_FUNC}

state="$(getUserLoginStatus)"

if [ "${state}" == "true" ]
then
cat << EOF
<!doctype html>
<html lang="en">
<head>
	<meta charset="utf-8">
        <title>Show Cabinet Info</title>
</head>
<body>
$(getCabinetInfo)
</body>
</html>
EOF
else
cat << EOF
$(cat ${TOOL_BASE}/index.html)
EOF
fi
