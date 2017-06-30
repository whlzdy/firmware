dmesg|grep "pl2303 converter now attached to"|awk '{print $NF}'|tail -1
