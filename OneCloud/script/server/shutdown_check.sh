#!/bin/bash
###########################################################
# Shutdown status check shell
#
# output: 
#        read the readme.txt for detail.
###########################################################



ONECLOUD_HOME=/opt/onecloud
cd ${ONECLOUD_HOME}


STATUS_FILE=${ONECLOUD_HOME}/script/server/control/stop_status.tmp
if [ ! -e ${STATUS_FILE} ]
then
    echo 1
fi

OUTPUT_TEXT=`cat ${STATUS_FILE} | head -n 1`
echo "0 "${OUTPUT_TEXT}
