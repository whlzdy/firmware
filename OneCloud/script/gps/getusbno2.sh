dmesg|grep "USB ACM device"|awk '{print $4}'|sed -e "s/://g"
