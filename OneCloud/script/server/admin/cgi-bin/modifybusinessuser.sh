#!/bin/sh
TOOL_BASE="/opt/onecloud/script/server/appdata"
TOOL_FUNC="${TOOL_BASE}/bin/funcs.sh"

. ${TOOL_FUNC}

state="$(getUserLoginStatus)"

if [ "${state}" == "true" ]
then
	qs="${QUERY_STRING}"
	username=$(echo $qs | awk -F"&|=" '{print $2}')
cat << EOF
<!doctype html>
<html lang="en">
<head>
        <meta charset="utf-8">
        <title>Show Business User</title>
	<script type="text/javascript">
                function checkSubmit() {
                        var password = login.password.value;
                        
                        if(password.length == 0){
                                alert("please input all fields!")
                                return false;
                        }

                        if(! /^[0-9a-zA-Z]*$/.test(password) ){
                                alert("password only include digits[0-9] and characters[a-zA-Z]!")
                                return false;
                        }

                        if (password.length < 6 || password.length >15) {
                                alert("password length should between 6 and 15!")
                                return false;
                        }
                                
                        return true;                    
                }       
        </script>
</head>
<body>
	<h1 style="margin-top:100px" align="center">Modify business User</h1>
        <div align="center">
        <form style="margin-top: 50px" name="login" action="/cgi-bin/modifybusinessuserpass.sh" method="GET" onsubmit="return checkSubmit()">
                <span style="text-align:left; font-weight:bold; display:inline-block; width:100px;">username</span> <input type='text' name='username' value='${username}' readonly='true'>
                <br /><br />
                <span style="text-align:left; font-weight:bold; display:inline-block; width:100px;">password</span> <input type='password' name='password'>
                <br /><br />
                <input type="submit"  style="border-radius:4px; height:25px; width:80px" value="Submit">
        </form>
        </div>
</body>
</html>
EOF
else
cat << EOF
$(cat ${TOOL_BASE}/index.html)
EOF
fi
