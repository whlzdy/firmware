#!/bin/bash

wvdialconf /opt/onecloud/script/server/gprs/wvdial.conf

$(which wvdial) --config=/opt/onecloud/script/server/gprs/wvdial.conf &
