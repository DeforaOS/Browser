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
PACKAGE="Browser"
PKG_CONFIG_PATH="$OBJDIR../data:$PKG_CONFIG_PATH"
PKG_CONFIG_PATH="${PKG_CONFIG_PATH%:}"
#executables
PKGCONFIG="pkg-config"

_pkgconfig()
{
	ret=0
	caption="$1"
	options="$2"
	packages="$3"

	echo -n "$caption"
	output=$($PKGCONFIG $options "$packages")
	ret=$?
	echo "$output"
	return $ret
}

_pkgconfig "EXISTS:" --exists "$PACKAGE" || exit 2

ret=0

_pkgconfig "VERSION:" --modversion "$PACKAGE" || ret=3
_pkgconfig "CFLAGS:	" --cflags "$PACKAGE" || ret=4
_pkgconfig "LIBS:	" --libs "$PACKAGE" || ret=5
_pkgconfig "PROVIDES:" --print-provides "$PACKAGE" || ret=6
_pkgconfig "REQUIRES:" --print-requires "$PACKAGE" || ret=7

exit $ret