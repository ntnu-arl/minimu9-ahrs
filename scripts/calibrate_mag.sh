#!/bin/sh
folder_name=config

# Use the find command to search for the folder
result=$(find /home/root/arl_ws/src/3rdparty/minimu9-ahrs/ -type d -name "$folder_name" 2>/dev/null)

# Check if the folder was found
if [ -n "$result" ]; then
    echo "Folder '$folder_name' found at: $result"
else
    echo "Folder '$folder_name' not found."
fi

echo "Data will be saved in $result"
echo "- MAG calibration START -"
echo "start rotating as much as possible"
minimu9-ahrs --mode raw $@ | head -n2000 > $result/minimu9-ahrs-cal-data
echo "- MAG calibration STOP -"
minimu9-ahrs-calibrator < $result/minimu9-ahrs-cal-data > $result/minimu9-ahrs-cal