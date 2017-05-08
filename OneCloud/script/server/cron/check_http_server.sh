#!/bin/bash
DATE_NOW=`date +%Y-%m-%d_%H:%M:%S`
echo "${DATE_NOW} : run check http server job..." >> /opt/onecloud/log/one_cron.log

process="$(/bin/ps -ef | /bin/grep lighttpd | /bin/grep -v grep)"
if [ -z "${process}" ]
then
	/usr/local/lighttpd/sbin/lighttpd -f /opt/onecloud/script/lighttpd.conf
fi
