#!/bin/bash
###########################################################
# Check server status shell
#
# output: 
#        0: success, 1: fail
###########################################################



ONECLOUD_HOME=/opt/onecloud
cd ${ONECLOUD_HOME}


SHELL_FILE=${ONECLOUD_HOME}/script/server/control/check_status.sh
if [ -e ${SHELL_FILE} ]
then
    nohup ${SHELL_FILE} >> /dev/null 2>&1 &
    echo 0
else
    echo 1
fi

