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
        <title>Modify Hardware Info</title>
	<style type=text/css>
        	span {
			display: inline-block;
			width: 150px;
			text-align: left;
			font-weight:bold;
		}	
        </style>	
	<script type="text/javascript">
                function checkSubmit() {
                        var uuid = login.uuid.value;
                        var com1 = login.com1.value;
                        var com2 = login.com2.value;
                        
			if(uuid.length == 0 || com1.length == 0 || com2.length ==0 ) {
                                alert("please input all fields!")
                                return false;
                        }

			if(! /^[0-9a-zA-Z]*$/.test(uuid) || ! /^[0-9a-zA-Z]*$/.test(com1) || ! /^[0-9a-zA-Z]*$/.test(com2)){
                                alert("all field only include digits[0-9] and characters[a-zA-Z]!")
                                return false;
                        }
			
			return true;
                }      
		 
        </script>
</head>
<body>
$(modifyHardwareInfo)
</body>
</html>
EOF
else
cat << EOF
$(cat ${TOOL_BASE}/index.html)
EOF
fi
