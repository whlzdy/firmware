#!/bin/bash

dev=`dmesg | grep 'GSM modem (1-port) converter now attached to tty' | head -n 1 | awk -F' ' '{print $NF}'`

cat << EOF > /opt/onecloud/script/server/gprs/wvdial.conf
[Dialer Defaults]
Init1 = ATZ
Init2 = ATQ0 V1 E1 S0=0 &C1 &D2 +FCLASS=0
Init3 = AT+CGDCONT=1,"IP","3GNET"
Modem Type = Analog Modem
Baud = 115200
New PPPD = yes
Modem = /dev/${dev}
ISDN = 0
Phone = *99#
Password = 3gnet
Username = 3gnet
EOF


$(which wvdial) --config=/opt/onecloud/script/server/gprs/wvdial.conf &
