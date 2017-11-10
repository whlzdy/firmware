######################################################
#        make_onecloud_firmware.sh
#               author: whl
#                2017-11-8
######################################################
#!/bin/bash
if [ "" = "$1" ]
then 
	echo "please input version number"
	exit
fi
cd OneCloud/
tar -jcvf ../onecloud_firmware_$1.tar.bz2  bin/ script/ config/ log/ 



