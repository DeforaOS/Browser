#!/bin/sh
#$Id$
#Copyright (c) 2016 Pierre Pronchery <khorben@defora.org>
#This file is part of DeforaOS Desktop Browser
#Redistribution and use in source and binary forms, with or without
#modification, are permitted provided that the following conditions are met:
#
# * Redistributions of source code must retain the above copyright notice, this
#   list of conditions and the following disclaimer.
# * Redistributions in binary form must reproduce the above copyright notice,
#   this list of conditions and the following disclaimer in the documentation
#   and/or other materials provided with the distribution.
#
#THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
#AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
#IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
#DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
#FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
#DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
#SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
#CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
#OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
#OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.



#variables
PROGNAME="embedded.sh"
TARGETS="all"
#executables
GCC="gcc"
MAKE="make"
MKTEMP="mktemp"
RM="rm -f"


#functions
#embedded
_embedded()
{
	cppflags="-DEMBEDDED"
	make="$MAKE CPPFLAGS=\"$cppflags\""
	objdir=$($MKTEMP -d)
	[ $? -eq 0 ]						|| return 2

	(cd .. && sh -c "$make OBJDIR='$objdir/' $TARGETS")
	ret=$?
	[ $ret -eq 0 ] || echo "$PROGNAME: $1: Could not build for embedded" 1>&2
	$RM -r -- "$objdir"
	return $ret
}


#usage
_usage()
{
	echo "Usage: $PROGNAME [-c] target..." 1>&2
	return 1
}


#main
clean=0
while getopts "cO:P:" name; do
	case "$name" in
		c)
			clean=1
			;;
		O)
			export "${OPTARG%%=*}"="${OPTARG#*=}"
			;;
		P)
			#XXX ignored for compatibility
			;;
		?)
			_usage
			exit $?
			;;
	esac
done
shift $((OPTIND - 1))
if [ $# -lt 1 ]; then
	_usage
	exit $?
fi

#clean
[ $clean -ne 0 ] && exit 0

exec 3>&1
ret=0
while [ $# -gt 0 ]; do
	target="$1"
	dirname="${target%/*}"
	shift

	if [ -n "$dirname" -a "$dirname" != "$target" ]; then
		$MKDIR -- "$dirname"				|| ret=$?
	fi
	_embedded > "$target"					|| ret=$?
done
exit $ret
