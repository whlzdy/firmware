for i in $(dmesg|grep "GSM modem (1-port) converter now attached to"|awk '{print $NF}')
do 
 j=$j" /dev/"$i
done
echo $j
