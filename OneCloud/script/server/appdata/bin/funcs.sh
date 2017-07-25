#!/bin/bash
# initiat according to specific path
basepath="/opt/onecloud/script/server/appdata"
logpath="/opt/onecloud/script/server/log"
logfile="${logpath}/running.log"
maxsize=10485760
authfile="/opt/onecloud/config/control.auth"

adminuser="${basepath}/conf/adminuser.property"
businessUserDir="${basepath}/conf/businessusers"
serverconfigDir="/opt/onecloud/config/servers"
hardwareconfigDir="${basepath}/conf/hardware"

function getUserLoginStatus {
	userState="$(getValue 'active' ${adminuser})"
	echo "${userState}"
}

function getUserName {
	userName="$(getValue 'username' ${adminuser})"
	echo "${userName}"
}

function getPassword {
	password="$(getValue 'password' ${adminuser})"
	echo "${password}"
}

function getValue {
	key="${1}"
        value="$(cat ${2} | grep ${key} | awk -F '=' {'print $2'})"
        if [ -z "${value}" ]
        then
                log "GET_PROPERTY" "FAIL" "get value for ${1} in file ${2} fail"
        fi
        echo "${value}"
	
}

function log {
	# max size is 10M
	size="$(stat --format=%s ${logfile})"
	if [ "${size}" -gt "${maxsize}" ]
	then
		cp -f ${logfile} "${logpath}/running$(date +'%Y-%m-%d-%H-%M-%S').log"
		rm -f ${logfile}
	fi	

	echo "`date`   ${1}   ${2}   ${3}" >> "${logfile}"
}

function modifyUserLoginStatusTrue {
	cp -f "${adminuser}" "${adminuser}.bak"
	sed -i "s/^active=.*$/active=true/g" "${adminuser}.bak"
	curtime="$(date +%s)"
	sed -i "s/^lastlogin=.*$/lastlogin=${curtime}/g" "${adminuser}.bak"
	size="$(du ${adminuser}.bak | awk {'print $1'})"
	if [ "${size}" -gt 0 ]
	then
		cp -f "${adminuser}.bak" "${adminuser}"
	fi
	rm -f "${adminuser}.bak"
}

function modifyUserLoginStatusFalse {
	cp -f "${adminuser}" "${adminuser}.bak"
	sed -i "s/^active=.*$/active=false/g" "${adminuser}.bak"
	size="$(du ${adminuser}.bak | awk {'print $1'})"
        if [ "${size}" -gt 0 ]
        then
		cp -f "${adminuser}.bak" "${adminuser}"
        fi
	rm -f "${adminuser}.bak"
}

function modifyAdminPassword {
	cp -f "${adminuser}" "${adminuser}.bak"
	newpass="${1}"
	sed -i "s/^password=.*$/password=${newpass}/g" "${adminuser}.bak"
	size="$(du ${adminuser}.bak | awk {'print $1'})"
        if [ "${size}" -gt 0 ]
        then
		cp -f "${adminuser}.bak" "${adminuser}"
        fi
	rm -f "${adminuser}.bak"
}


function getUserList {
	files=$(ls -t ${businessUserDir})
	if [ -z "${files}" ]
	then
		echo "<h1 style='margin-top:30px' align='center'>Business User List</h1>"
                echo "<div align='center'><input type='button' class='buttonAdd' value='add user' onclick=\"window.open('/cgi-bin/addBusiUser.sh')\"><input type='button' class='buttonRefresh' value='refresh list' onclick=\"window.location.href='/cgi-bin/showBusiUserPassword.sh'\"</div>"
		echo "<p align='center'>buisness user list is <em style='color:red'>Empty</em></p>"
	else
		echo "<h1 style='margin-top:30px' align='center'>Business User List</h1>"
		echo "<div align='center'><input type='button' class='buttonAdd' value='add user' onclick=\"window.open('/cgi-bin/addBusiUser.sh')\"><input type='button' class='buttonRefresh' value='refresh list' onclick=\"window.location.href='/cgi-bin/showBusiUserPassword.sh'\"</div>"
		echo "<table align='center'>
                <tr>
                        <th style='width:200px' align='left'>Username</th>
                        <th style='width:300px' align='left'>Password</th>
                        <th style=></th>
                        <th ></th>
                </tr>"
		for file in ${files}
		do	
			password="$(cat ${businessUserDir}/${file})"
			echo "<tr style='background-color:#eeeeee; '>
				<td align='left'>${file}</td>
				<td align='left'>${password}</td>
				<td ><input type='button' class='buttonDelete' value='delete' onclick=\"window.location.href='/cgi-bin/deletebusinessuser.sh?username=${file}'\"></td>
				<td ><input type='button' class='buttonModify' value='modify' onclick=\"window.open('/cgi-bin/modifybusinessuser.sh?username=${file}')\"></td>
		      	     </tr>"
		done
		echo "</table>"
	fi
}

function addBusinessUser {
	username="${1}"
	password="${2}"
	touch "${businessUserDir}/${username}"
	echo "${password}" > "${businessUserDir}/${username}"
}

function isUserExist {
	username="${1}"
	file=$(ls ${businessUserDir}/${username} 2>/dev/null)
	if [ -n "${file}" ]
	then
		echo "true"
	else
		echo "false"
	fi
}

function deleteBusinessUser {
	username="${1}"
	rm -rf ${businessUserDir}/${username} 2>/dev/null
}

function modifybusinessuser {
	username="${1}"
        password="${2}"
	touch "${businessUserDir}/${username}.bak"
	echo "${password}" > "${businessUserDir}/${username}.bak"
	size="$(du ${businessUserDir}/${username}.bak | awk {'print $1'})"
	if [ "${size}" -gt 0 ]
        then
                cp -f "${businessUserDir}/${username}.bak" "${businessUserDir}/${username}"
        fi
        rm -f "${businessUserDir}/${username}.bak"
}

function getSpeServerInfo {
	server="${1}"
	echo "<table align='center'>
                <tr>
                        <th style='width:200px' align='left'>Parameter</th>
                        <th style='width:200px' align='left'>Value</th>
                </tr>"

	if [ ${server} == "${serverconfigDir}/primaryserver.property" ]
        then
                serverid="primaryserver"
        else
                serverid="secondaryserver_2"
        fi

	output="$(/opt/onecloud/bin/monitor_client type=server server_id=${serverid} 2>&1 | grep -v Debug)"
	#output='{"count":1, "data":[{"type":0,"name":"primaryserver","mac":"","ip":"172.28.101.11","status":0}]}'
	
	echo "<tr style='background-color:#eeeeee;'>
                        <td align='left'>name</td>
                        <td align='left'>${serverid}</td>
                    </tr>"


	while read LINE
	do
		key="$(echo $LINE | awk -F '=' {'print $1'})"
		value="$(echo $LINE | awk -F '=' {'print $2'})"
		if [ ${key} == "ip" ] || [ ${key} == "mask" ] || [ ${key} == "gateway" ]
		then
		echo "<tr style='background-color:#eeeeee;'>
			<td align='left'>${key}</td>
			<td align='left'>${value}</td>
		    </tr>"
		fi
	done  < $server

	status=`echo "${output}" | awk -F "status\":" {'print $2'} | awk -F ',|}' {'print $1'}`
	
	if [ "${status}" -eq "0" ]
	then
		statusout="stopped"
	else
		statusout="running"
	fi

        echo "<tr style='background-color:#eeeeee;'>
                        <td align='left'>status</td>
                        <td align='left'>${statusout}</td>
                    </tr>"

	echo "</table>"
}

function getServerInfot {
	server1="${serverconfigDir}/primaryserver.property"
	server2="${serverconfigDir}/secondaryserver_2.property"
	outhtml1=$(getSpeServerInfo ${server1})
	outhtml2=$(getSpeServerInfo ${server2})
	echo "<h1 style='margin-top:30px; margin-bottom:20px;' align='center'>Server Information Display</h1>"
	echo "<div style='float: left; margin-left: 100px; '>"
	echo "<h2 style='color:blue' align='center'>Primary</h2>"
	echo ${outhtml1}
	echo "</div><div style='float: right; margin-right: 100px'>"
	echo "<h2 style='color:blue' align='center'>Secondary</h2>"
        echo ${outhtml2}
	echo "</div>"
}

function modifySpeServerInfo {
	server="${1}"
	parameterkey="${2}"
	while read LINE
        do
		key="$(echo $LINE | awk -F '=' {'print $1'})"
		value="$(echo $LINE | awk -F '=' {'print $2'})"
		echo "<div style='margin-bottom:5px;'><span>${key}</span><input type='text' name='${parameterkey}${key}' value='${value}'></div>"	
	done  < $server
}

function modifyServerInfo {
	server1="${serverconfigDir}/server1.property"
        server2="${serverconfigDir}/server2.property"
	outhtml1=$(modifySpeServerInfo ${server1} "server1")
	outhtml2=$(modifySpeServerInfo ${server2} "server2")
	echo "<h1 style='margin-top:30px; margin-bottom:20px;' align='center'>Modify Server Info</h1>"
	echo "<div align='center'><form  name='login' action='/cgi-bin/modifyServerInfoAction.sh' method='GET' onsubmit='return checkSubmit()'>"
	echo "<h2 style='color:blue' align='center'>Server1</h2>"
	echo ${outhtml1}
	echo "<h2 style='color:blue' align='center'>Server2</h2>"
	echo ${outhtml2}
	echo "<input type='submit'  style='border-radius:4px; height:25px; width:80px; background-color: LightGreen; margin-top:20px' value='Submit'>"
	echo "</form></div>"
}

function modifyServerInfoConfig {
	server1ip="${1}"
        server1mask="${2}"
        server1gateway="${3}"
        server1mac="${4}"
        server1catagory="${5}"

        server2ip="${6}"
        server2mask="${7}"
        server2gateway="${8}"
        server2mac="${9}"
        server2catagory="${10}"

	server1bak="${serverconfigDir}/server1.property.bak"
	echo "ip=${server1ip}" > "${server1bak}"
	echo "mask=${server1mask}" >> "${server1bak}"
	echo "gateway=${server1gateway}" >> "${server1bak}"
	echo "mac=${server1mac}" >> "${server1bak}"
	echo "catagory=${server1catagory}" >> "${server1bak}"
	
	size="$(du ${server1bak} | awk {'print $1'})"
        if [ "${size}" -gt 0 ]
        then
		cp -f "${server1bak}" "${serverconfigDir}/server1.property"
	fi
	rm -rf ${server1bak}

	server2bak="${serverconfigDir}/server2.property.bak"
        echo "ip=${server2ip}" > "${server2bak}"
        echo "mask=${server2mask}" >> "${server2bak}"
        echo "gateway=${server2gateway}" >> "${server2bak}"
        echo "mac=${server2mac}" >> "${server2bak}"
        echo "catagory=${server2catagory}" >> "${server2bak}"

        size="$(du ${server2bak} | awk {'print $1'})"
        if [ "${size}" -gt 0 ]
        then
                cp -f "${server2bak}" "${serverconfigDir}/server2.property"
        fi
        rm -rf ${server2bak}
}

function getHardwareInfo {
	upper=$(getMainPageUpper)
        footer=$(getMainPageFooter)
        echo "${upper}"

	hardware="/opt/onecloud/config/cabinet.info"
        outhtml=$(getSpeHardwareInfo ${hardware})
        echo "<h1 style='margin-top:30px; margin-bottom:20px;' align='center'>Cabinet Information Display</h1>"
        echo ${outhtml}

	echo "${footer}"
}

function getSpeHardwareInfo {
	server="${1}"
        echo "<table align='center'>
                <tr>
                        <th style='width:200px' align='left'>Parameter</th>
                        <th style='width:200px' align='left'>Value</th>
                </tr>"
        while read LINE
        do
                key="$(echo $LINE | awk -F '=' {'print $1'})"
                value="$(echo $LINE | awk -F '=' {'print $2'})"
		if [ ${key} == "poid" ] || [ ${key} == "name" ] 
                then	
                echo "<tr style='background-color:#eeeeee;'>
                        <td align='left'>${key}</td>
                        <td align='left'>${value}</td>
                    </tr>"
		fi
        done  < $server
	
	output="$(/opt/onecloud/bin/monitor_client type=cabinet 2>&1 | grep -v Debug)"
	#output='{"timestamp":"1493274624","status":0,"kwh":1.22,"voltage":5.00,"current":7.88,"temperature":32.98,"watt":3.88,"decibel":40.00}'	

	status=$(getCabinetValue "status" "${output}")
	
	if [ "${status}" -eq "0" ]
        then
                statusout="stopped"
        elif [ "${status}" -eq "1" ]
	then
		statusout="running"
	else
                statusout="warning"
        fi


	echo "<tr style='background-color:#eeeeee;'>
                        <td align='left'>status</td>
                        <td align='left'>${statusout}</td>
                    </tr>"
	
	kwh=$(getCabinetValue "kwh" "${output}")
        echo "<tr style='background-color:#eeeeee;'>
                        <td align='left'>kwh</td>
                        <td align='left'>${kwh}</td>
                    </tr>"

	voltage=$(getCabinetValue "voltage" "${output}")
        echo "<tr style='background-color:#eeeeee;'>
                        <td align='left'>voltage</td>
                        <td align='left'>${voltage}</td>
                    </tr>"

	current=$(getCabinetValue "current" "${output}")
        echo "<tr style='background-color:#eeeeee;'>
                        <td align='left'>current</td>
                        <td align='left'>${current}</td>
                    </tr>"

	temperature=$(getCabinetValue "temperature" "${output}")
        echo "<tr style='background-color:#eeeeee;'>
                        <td align='left'>temperature</td>
                        <td align='left'>${temperature}</td>
                    </tr>"

	watt=$(getCabinetValue "watt" "${output}")
        echo "<tr style='background-color:#eeeeee;'>
                        <td align='left'>watt</td>
                        <td align='left'>${watt}</td>
                    </tr>"

	decibel=$(getCabinetValue "decibel" "${output}")
        echo "<tr style='background-color:#eeeeee;'>
                        <td align='left'>decibel</td>
                        <td align='left'>${decibel}</td>
                    </tr>"


	
        echo "</table>"	
}

function getCabinetValue {
	key="${1}"
	msg="${2}"
	out=`echo "${msg}" | awk -F "${key}\":" {'print $2'} | awk -F ',|}' {'print $1'}`
	echo "${out}"	
}

function modifyHardwareInfo {
	hardware="${hardwareconfigDir}/hardware.property"
        outhtml=$(modifySpeHardwareInfo ${hardware})	
	echo "<h1 style='margin-top:30px; margin-bottom:20px;' align='center'>Modify Hardware Info</h1>"
        echo "<div align='center'><form  name='login' action='/cgi-bin/modifyHardwareInfoAction.sh' method='GET' onsubmit='return checkSubmit()'>"
        echo ${outhtml}
        echo "<input type='submit'  style='border-radius:4px; height:25px; width:80px; background-color: LightGreen; margin-top:20px' value='Submit'>"
        echo "</form></div>"	
}

function modifySpeHardwareInfo {
	server="${1}"
        while read LINE
        do
                key="$(echo $LINE | awk -F '=' {'print $1'})"
                value="$(echo $LINE | awk -F '=' {'print $2'})"
                echo "<div style='margin-bottom:5px;'><span>${key}</span><input style='width:300px' type='text' name='${key}' value='${value}'></div>"       
        done  < $server
}

function modifyHardwareInfoConfig {
	uuid="${1}"
        com1="${2}"
        com2="${3}"
        
	server1bak="${hardwareconfigDir}/hardware.property.bak"
        echo "uuid=${uuid}" > "${server1bak}"
        echo "com1=${com1}" >> "${server1bak}"
        echo "com2=${com2}" >> "${server1bak}"

        size="$(du ${server1bak} | awk {'print $1'})"
        if [ "${size}" -gt 0 ]
        then
                cp -f "${server1bak}" "${hardwareconfigDir}/hardware.property"
        fi
        rm -rf ${server1bak}
}

function restoreHardwareInfoConfig {
	server1bak="${hardwareconfigDir}/defaulthardware.property"
	size="$(du ${server1bak} | awk {'print $1'})"
        if [ "${size}" -gt 0 ]
        then
                cp -f "${server1bak}" "${hardwareconfigDir}/hardware.property"
        fi
}

function getCabinetInfo {
	echo "<h1 style='margin-top:30px; margin-bottom:20px;' align='center'>Cabinet Running Status Display</h1>"
	echo "<table align='center'>
                <tr>
                        <th style='width:200px' align='left'>Parameter</th>
                        <th style='width:200px' align='left'>Value</th>
                </tr>
                <tr style='background-color:#eeeeee;'>
                        <td align='left'>temperature</td>
                        <td align='left'>22</td>
                </tr>
		<tr style='background-color:#eeeeee;'>
                        <td align='left'>running status</td>
                        <td align='left'>normal</td>
                </tr>
		<tr style='background-color:#eeeeee;'>
                        <td align='left'>voltage</td>
                        <td align='left'>220</td>
                </tr>
		<tr style='background-color:#eeeeee;'>
                        <td align='left'>server num</td>
                        <td align='left'>2</td>
                </tr>
	
		
             </table>" 
	
}

function getServerInfo {
	echo "<h1 style='margin-top:30px; margin-bottom:20px;' align='center'>Server Running Status Display</h1>"
        echo "<table align='center'>
                <tr>
                        <th style='width:200px' align='left'>Parameter</th>
                        <th style='width:200px' align='left'>Value</th>
                </tr>
                <tr style='background-color:#eeeeee;'>
                        <td align='left'>temperature</td>
                        <td align='left'>22</td>
                </tr>
                <tr style='background-color:#eeeeee;'>
                        <td align='left'>running status</td>
                        <td align='left'>normal</td>
                </tr>
                <tr style='background-color:#eeeeee;'>
                        <td align='left'>voltage</td>
                        <td align='left'>220</td>
                </tr>
                <tr style='background-color:#eeeeee;'>
                        <td align='left'>server num</td>
                        <td align='left'>2</td>
                </tr>
        
                
             </table>" 
}

function getSystemLog {
	tail -n1000 ${logfile} > ${logfile}.bak
	while read LINE
        do
        	echo -e "${LINE}\r\n"
	done  < ${logfile}.bak
	rm -f ${logfile}.bak	
}


function doJsonResponse {
	path="${1}"
	qs="${2}"
	log "REST_REQUEST_RECEIVE" "SUCCESS" "path:${path} and query string:${qs}"

	if [ -n "${path}" ]
	then
		if [ "${path}" == "querycabinet" ]
		then
			out="$(/opt/onecloud/bin/monitor_client type=cabinet 2>&1 | grep -v Debug)"
			echo ""
			echo "${out}"
			log "REST_REQUEST_RESPONSE" "SUCCESS" "path:${path} and query string:${qs} and response:${out}"
		elif [ "${path}" == "queryserver" ]
		then
			if [ -z "${qs}" ]
			then	
				out="$(/opt/onecloud/bin/monitor_client type=server 2>&1 | grep -v Debug)"
        	                echo ""
				echo "${out}"
	                        log "REST_REQUEST_RESPONSE" "SUCCESS" "path:${path} and query string:${qs} and response:${out}"
			else
				serverid=$(echo $qs | awk -F"&|=" '{print $2}')
				out="$(/opt/onecloud/bin/monitor_client type=server server_id=${serverid} 2>&1 | grep -v Debug)"
				echo ""
				echo "${out}"
				log "REST_REQUEST_RESPONSE" "SUCC" "path:${path} and query string:${serverid} and response:${out}"
			fi			
		elif [ "${path}" == "all" ]
		then
			out="$(/opt/onecloud/bin/monitor_client type=all 2>&1 | grep -v Debug)"
                        echo ""
                        echo "${out}"
                        log "REST_REQUEST_RESPONSE" "SUCCESS" "path:${path} and query string:${qs} and response:${out}"
		elif [ "${path}" == "updatepwd" ]
		then
			if [ -z "${qs}" ]
                        then
                                out="{\"message\": \"request param null\"}"
                                echo "status:500 Internal Server Error"
	                        echo ""
				echo "${out}"
                                log "REST_REQUEST_RESPONSE" "FAIL" "path:${path} and query string:${qs} and response:${out}"
                        else
				name1=$(echo $qs | awk -F"&|=" '{print $1}')
				value1=$(echo $qs | awk -F"&|=" '{print $2}')
				name2=$(echo $qs | awk -F"&|=" '{print $3}')
				value2=$(echo $qs | awk -F"&|=" '{print $4}')
				
				isvalid=0

				if [ ${name1} == "password" ] && [ ${name2} == "remoteOperation" ]
				then
					password="${value1}"
					remoteOperation="${value2}"
				elif [ ${name2} == "password" ] && [ ${name1} == "remoteOperation" ]
				then
					password="${value2}"
                                        remoteOperation="${value1}"
				else
					isvalid=1
				fi

				if [ "${remoteOperation}" != "true" ] && [ "${remoteOperation}" != "false" ]
				then
					remoteOperation="true"
				fi
					
				if [ "${isvalid}" == "0" ]
				then
					echo "username=clouder" > ${authfile}
        				echo "password="${password} >> ${authfile}
        				echo "remoteOperation="${remoteOperation} >> ${authfile}				

					out="{\"message\": \"\"}"
                                	echo ""
					echo "${out}"

                                	log "REST_REQUEST_RESPONSE" "SUCC" "path:${path} and query string:${serverid} and response:${out}"
				else
					out="{\"message\": \"request param error\"}"
                                        echo "status:500 Internal Server Error"
					echo ""
					echo "${out}"

                                        log "REST_REQUEST_RESPONSE" "FAIL" "path:${path} and query string:${serverid} and response:${out}"

				fi
                        fi
		else
			out="{\"message\": \"unsupported path\"}"
			echo "status:500 Internal Server Error"
			echo ""
			echo "${out}"
			log "REST_REQUEST_RESPONSE" "FAIL" "path:${path} and query string:${qs} and response:${out}"
		fi
	else
		out="{\"message\": \"path null\"}"
		echo "status:500 Internal Server Error"
                echo ""
		echo "${out}"
                log "REST_REQUEST_RESPONSE" "FAIL" "path:${path} and query string:${qs} and response:${out}"
	fi

}

function getForm {
	upper=$(getMainPageUpper)
	footer=$(getMainPageFooter)
	echo "${upper}"
	
	echo "<h1 style='margin-top:100px' align='center'>Modify Administrator Paasword</h1>
        <div align='center'>
        <form style='margin-top: 50px' name='login' action='/cgi-bin/modifypass.sh' method='GET' onsubmit='return checkSubmit()'>
                <span style='text-align:left; font-weight:bold; display:inline-block; width:200px;'>old password</span> <input type='password' name='old'>
                <br /><br />
                <span style='text-align:left; font-weight:bold; display:inline-block; width:200px;'>new password</span> <input type='password' name='newp'>
                <br /><br />
                <span style='text-align:left; font-weight:bold; display:inline-block; width:200px;'>password confirm</span> <input type='password' name='newconf'>
                <br /><br />
                <input type='submit'  style='border-radius:4px; height:25px; width:80px' value='Submit'>
        </form>
        </div>"
	
	echo "${footer}"
}

function modifyPassFail {
	upper=$(getMainPageUpper)
        footer=$(getMainPageFooter)
        echo "${upper}"
	
	echo "<h1 style='font-size: 18px; margin-top:100px; margin-left: 50px; margin-right: 50px' align='center'>Modify Administrator Paasword <span style='color: red'>Fail</span>, please make sure old password is correct and new password length is between 6 and 15!</h1>"

	echo "${footer}"
}

function modifyPassSucc {
	upper=$(getMainPageUpper)
        footer=$(getMainPageFooter)

        echo "${upper}"

	echo "<h1 style='font-size: 18px;margin-top:100px; margin-left: 50px; margin-right: 50px' align='center'>Modify Administrator Paasword <span style='color: red'>Successful</span>.</h1>"

        echo "${footer}"

}

function getMainPageServerInfo {
	upper=$(getMainPageUpper)
        footer=$(getMainPageFooter)
	echo "${upper}"

	out=$(getServerInfot)	
	echo "${out}"


	echo "${footer}"
}

function getMainPageUpper {
	set -f 
	line=`wc -l ${basepath}/mainpage.html | awk -F' ' {'print $1'}`
	splitter=`grep -n "div\ id=\"midright\"" ${basepath}/mainpage.html | awk -F':' {'print $1'}`
	start=1
	while [ $start -le $splitter ]
	do
		echo `sed -n ${start}p ${basepath}/mainpage.html`
		start=`expr $start + 1`
	done
	set +f 
}

function getMainPageFooter {
	line=`wc -l ${basepath}/mainpage.html | awk -F' ' {'print $1'}`
        splitter=`grep -n "div\ id=\"midright\"" ${basepath}/mainpage.html | awk -F':' {'print $1'}`
	
	start=`expr $splitter + 2`
        while [ $start -le $line ]
        do
                echo `sed -n ${start}p ${basepath}/mainpage.html`
                start=`expr $start + 1`
        done
}

