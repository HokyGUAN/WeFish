#!/bin/bash

start() {
    echo "Starting WeFish Server"
    ~/WeFish/build/server/server $1 >/dev/null 2>&1 &
    echo $! > ~/WeFish/WeFishServer.pid
	FP=`expr $1 + 1`
    ~/WeFish/build/fserver/fserver ${FP} >/dev/null 2>&1 &
    echo $! > ~/WeFish/WeFishFServer.pid
    echo "done"
}

stop() {
    echo "Stopping WeFish Server"
    kill $(cat ~/WeFish/WeFishServer.pid)
    kill $(cat ~/WeFish/WeFishFServer.pid)
    echo "done"
}

case "$1" in
    start)
        start $2
        ;;
    stop)
        stop
        ;;
    *)
    echo $"Usage: $0 {start|stop}"
    exit 1
esac
