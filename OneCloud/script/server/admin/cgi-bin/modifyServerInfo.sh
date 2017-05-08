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
        <title>Modify Server Info</title>
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
                        var server1ip = login.server1ip.value;
                        var server1mask = login.server1mask.value;
                        var server1gateway = login.server1gateway.value;
			var server1mac = login.server1mac.value;
			var server1catagory = login.server1catagory.value;                        

			var server2ip = login.server2ip.value;
                        var server2mask = login.server2mask.value;
                        var server2gateway = login.server2gateway.value;
                        var server2mac = login.server2mac.value;
                        var server2catagory = login.server2catagory.value;     


                        if(server1ip.length == 0 || server1mask.length == 0 || server1gateway.length ==0 || server1mac.length == 0 || server1catagory.length == 0 || server2ip.length == 0 || server2mask.length == 0 || server2gateway.length ==0 || server2mac.length == 0 || server2catagory.length == 0) {
                                alert("please input all fields!")
                                return false;
                        }

			if (!ValidateIPaddress(server1ip) || !ValidateIPaddress(server1mask) || !ValidateIPaddress(server1gateway) || !ValidateIPaddress(server2ip) || !ValidateIPaddress(server2mask) || !ValidateIPaddress(server2gateway)) {
				alert("please make sure all ip related fields are valid!")
				return false;
			}           
		
			if (!ValidateMac(server1mac) || !ValidateMac(server2mac)) {
				alert("please make sure all mac related fileds are valid")
				return false;
			}

			if(! /^[0-9a-zA-Z]*$/.test(server1catagory) || ! /^[0-9a-zA-Z]*$/.test(server2catagory)){
                                alert("catagory field only include digits[0-9] and characters[a-zA-Z]!")
                                return false;
                        }
			
			return true;
                }      
		
		function ValidateIPaddress(ipaddress) {  
  			if (/^(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$/.test(ipaddress)) {  
    				return (true)  
  			}	  
  			return (false)  
		}  

		function ValidateMac(mac) {
			if (/^([0-9A-Fa-f]{2}[:-]){5}([0-9A-Fa-f]{2})$/.test(mac) ) {
				return (true)
			}
			return (false)
		} 
        </script>
</head>
<body>
$(modifyServerInfo)
</body>
</html>
EOF
else
cat << EOF
$(cat ${TOOL_BASE}/index.html)
EOF
fi
