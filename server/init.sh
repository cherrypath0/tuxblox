#!/bin/bash

# server initiator shell script
# initializes all ".js" files on the "server/modules/" directory

cd "$(dirname "$0")"

echo "cleaning up ports"
pids=$(lsof -t -i:7000-7010) # the server side binds on ports 7000-7010, reverse proxied through nginx to port 80/443
if [ ! -z "$pids" ]; then
    kill -9 $pids >/dev/null 2>&1
fi

MODULE_PIDS=()
LOGS_DIR="./logs/traffic"

cleanup() {
    echo -e "\nstopping all server modules..."
    for pid in "${MODULE_PIDS[@]}"; do
        if kill -0 "$pid" 2>/dev/null; then
            kill -15 "$pid" 2>/dev/null
        fi
    done

    wait 2>/dev/null

    echo "combining timestamp log files..."
    if [ -d "$LOGS_DIR" ]; then
        cd "$LOGS_DIR" || exit 1
        
        timestamps=$(ls *.log 2>/dev/null | grep -oE '[0-9]{2}-[0-9]{2}-[0-9]{4}-[0-9]{2}-[0-9]{2}' | sort -u)

        for ts in $timestamps; do
            target_file="GeneralTraffic-${ts}.log"
            echo "processing logs for runtime: ${ts} -> ${target_file}"
            
            ls *-${ts}-*.log 2>/dev/null | grep -v "^GeneralTraffic-" | while read -r logfile; do
                if [ ! -f "$target_file" ]; then
                    echo "=== CONSOLIDATED: ${logfile} ===" >> "$target_file"
                else
                    echo -e "\n=== CONSOLIDATED: ${logfile} ===" >> "$target_file"
                fi
                
                cat "$logfile" >> "$target_file"
                rm "$logfile"
            done
        done
        cd - >/dev/null || exit 1
    else
        echo "WARNING: logs directory not found at $LOGS_DIR, skipping!"
    fi

    echo "cleanup final success"
    exit 0
}

trap cleanup SIGINT

echo "starting server modules"

if [ -d "modules" ]; then
    for file in modules/*.js; do
        if [ -f "$file" ]; then
            echo "Starting: $file"
            node "$file" &
            MODULE_PIDS+=($!)
        fi
    done
else
    echo "fatal error: modules/ directory not found"
    exit 1
fi

wait