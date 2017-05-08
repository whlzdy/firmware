dmesg|grep "ch341-uart converter now attached"|awk '{print $NF}'|tail -1
