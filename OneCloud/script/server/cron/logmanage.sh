#/bin/bash

DATE_NOW=`date +%Y-%m-%d_%H:%M:%S`
echo "${DATE_NOW} : run log manage job..." >> /opt/onecloud/log/one_cron.log

logpath="/opt/onecloud/script/server/log"
limit=5


currentnum="$(/bin/ls -rt ${logpath} | /bin/grep -v total | /bin/wc -l)"
if [ ${currentnum} -gt "${limit}" ]
then
	delnum=`/bin/expr $currentnum - $limit`
	delfiles="$(/bin/ls -rt ${logpath} | /bin/grep -v total | /bin/head -n ${delnum})"	

	for file in ${delfiles}
	do 
		/bin/rm -f ${logpath}/${file}
	done

fi


systemlogpath="/opt/onecloud/log"
systemlimit=50
systemcurrentnum="$(/bin/ls -rt ${systemlogpath} | /bin/grep -E 'daemon' | /bin/wc -l)"
if [ ${systemcurrentnum} -gt "${systemlimit}" ]
then
	systemdelnum=`/bin/expr $systemcurrentnum - $systemlimit`
	systemdelfiles="$(/bin/ls -rt ${systemlogpath} | /bin/grep -E 'daemon' | /bin/head -n ${systemdelnum})"

	for file in ${systemdelfiles}
        do
		/bin/rm -f ${systemlogpath}/${file}
        done

fi

systemlogpath="/opt/onecloud/log"
systemlimit=2
systemcurrentnum="$(/bin/ls -rt ${systemlogpath} | /bin/grep -E 'watch_dog' | /bin/wc -l)"
if [ ${systemcurrentnum} -gt "${systemlimit}" ]
then
        systemdelnum=`/bin/expr $systemcurrentnum - $systemlimit`
        systemdelfiles="$(/bin/ls -rt ${systemlogpath} | /bin/grep -E 'watch_dog' | /bin/head -n ${systemdelnum})"

        for file in ${systemdelfiles}
        do
                /bin/rm -f ${systemlogpath}/${file}
        done

fi

