#!/bin/sh
TOOL_BASE="/opt/onecloud/script/server/appdata"
TOOL_FUNC="${TOOL_BASE}/bin/funcs.sh"

. ${TOOL_FUNC}


$(modifyUserLoginStatusFalse)
log "ADMIN_LOGOUT" "SUCCESS" "administrator succeed to logout"
cat << EOF
$(cat ${TOOL_BASE}/jumpmainpage.html)
EOF
