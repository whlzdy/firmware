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
        <title>Show Business User</title>
	<style type=text/css>
	.buttonDelete {
		width: 80px;
                height: 25px;
                border-radius:4px;
                cursor: pointer;
		background-color: red;
	}
	.buttonModify {
                width: 100px;
                height: 25px;
                border-radius:4px;
                cursor: pointer;
		background-color: skyblue;
        }
	.buttonAdd {
                width: 100px;
                height: 25px;
                border-radius:4px;
                cursor: pointer;
                background-color: LightGreen;
		margin-bottom: 10px;
        }
	.buttonRefresh {
                width: 100px;
                height: 25px;
                border-radius:4px;
                cursor: pointer;
                background-color: Gold;
		margin-left: 40px;
		margin-bottom: 10px;
        }
	</style>
</head>
<body>
$(getUserList)
</body>
</html>
EOF
else
cat << EOF
$(cat ${TOOL_BASE}/index.html)
EOF
fi
