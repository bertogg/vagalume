#!/bin/sh
#
# Add a Vagalume Last.FM client button to moblin
#
# Author: Costas Stylianou
#
echo "Adding Vagalume button!"

#copy the vagalume.desktop file
cp vagalume.desktop /usr/share/applications/

#copy the icon
cp ../data/icons/48x48/vagalume.png /usr/share/pixmaps
