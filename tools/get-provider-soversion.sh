#!/bin/sh
# Copyright (C) 2016 Canonical Ltd
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License version 3 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
# Authors: James Henstridge <james.henstridge@canonical.com>

set -eu

[ -n "${SERIES:-}" ] || SERIES=$(lsb_release -c -s)

case "$SERIES" in
    trusty)
        # TODO: the CI systems are running Trusty, so don't bomb out
        # when they try to build the source package.
        echo 0
        ;;
    vivid)
        # Old C++11 ABI, Boost 1.55
        echo 0
        ;;
    xenial)
        # New C++11 ABI, Boost 1.58
        echo 1
        ;;
    yakkety)
        # New C++11 ABI, Boost 1.60
        echo 2
        ;;
    *)
        echo "Unknown distro series $SERIES" >&2
        exit 1
        ;;
esac
