#!/bin/bash
rm -f /tmp/fc_dump_done; touch /tmp/fc_dump_now; for i in 1 2 3 4 5 6; do [ -f /tmp/fc_dump_done ] && break; sleep 1; done
pkill -x flycast; sleep 1; pkill -9 -x flycast; pkill -x Xvfb
