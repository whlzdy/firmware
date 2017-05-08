#!/bin/bash
DATE_NOW=`date +%Y-%m-%d_%H:%M:%S`
echo "${DATE_NOW} : run check gprs job..." >> /opt/onecloud/log/one_cron.log

wvstatus=`ps -ef | grep 'wvdial' | grep -v 'grep'`
netint=`/usr/sbin/ifconfig | grep ppp`

if [ -z "${wvstatus}" ] && [ -z "${netint}" ]
then
	killall wvdial
	sleep 10
	#$(which wvdial) --config=/opt/onecloud/script/server/gprs/wvdial.conf &	
	/opt/onecloud/script/server/gprs/startgprs.sh	
fi
