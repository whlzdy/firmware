#!/bin/bash

sleep 15
/opt/onecloud/script/server/gprs/startgprs.sh >> /opt/onecloud/log/GPRS.log 2>&1 &
