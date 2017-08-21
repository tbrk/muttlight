#!/bin/sh

#
# Copyright (C) 2017 Timothy Bourke (tim@tbrk.org)
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation; either version 2 of the License, or (at your
# option) any later version.
#
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
# more details.
#
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
#

#
# Shell script to lauch iTerm2, or, failing that, Terminal with mutt opened at
# the messages given as paths on the command-line.
#

MUTT=/usr/local/bin/mutt

while [ $# -gt 0 ]; do
    fpath="$1"
    dpath=$(dirname "$fpath")

    case $(basename "$dpath") in
	cur|new|tmp) dpath=$(dirname "$dpath") ;;
	*) ;;
    esac
    dpath_escaped=$(echo "$dpath" | sed -e 's/ /\\\\ /g')

    if [ -f "$fpath" ]; then
	msgid=$(sed -ne 's/^[Mm][Ee][Ss][Ss][Aa][Gg][Ee]-[Ii][Dd]: *\([^ ]*\)/\1/p' "$fpath")
	if [ "$msgid" ]; then
	    # prefer iTerm2
	    osascript 2>/dev/null <<EOF
tell application "iTerm2"
    create window with default profile command "${MUTT} -f ${dpath_escaped} -e push\\\\ <limit>~i${msgid}<enter><limit>all<enter><display-message>"
end tell
EOF
	    if [ $? -ne 0 ]; then
		# but accept Terminal otherwise
		osascript <<EOF
tell application "Terminal"
    do script "${MUTT} -f '${dpath}' -e 'push <limit>~i${msgid}<enter><limit>all<enter><display-message>'"
end tell
tell application "Terminal" to activate
EOF
	    fi
	else
	    printf "no Message-Id in %s\n" "$fpath" >&2
	fi
    else
	printf "cannot find %s\n" "$fpath" >&2
    fi

    shift
done

