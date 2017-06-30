dmesg|grep "USB ACM device"|awk '{print $(NF-3)}'|sed -e "s/://g"
