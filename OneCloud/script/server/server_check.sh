#!/bin/bash
###########################################################
# Check servers status
#
# output: 
#        read the readme.txt for detail.
###########################################################



ONECLOUD_HOME=/opt/onecloud
cd ${ONECLOUD_HOME}


STATUS_FILE=${ONECLOUD_HOME}/script/server/control/server_status.tmp
if [ ! -e ${STATUS_FILE} ]
then
    echo 0
fi

MERGY_TEXT=
while read LINE
do
    if [ "${LINE}" != "" ]
    then
        if [ "${MERGY_TEXT}" != "" ]
        then
            MERGY_TEXT=${MERGY_TEXT}","${LINE}
        else
            MERGY_TEXT=${LINE}
        fi
    fi
done  < $STATUS_FILE

echo ${MERGY_TEXT}
