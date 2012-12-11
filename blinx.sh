#
# Copyright (C) 2009-2012 Chris McClelland
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
# Helper script for building the binary distribution. It's unlikely you'll need
# this unless you're forking the project.
#
# Before calling release.sh, each architecture must be built. It makes sense to
# only generate XSVF files and firmware includes once, so this script comments
# out the pre- and post-build steps after building the x86_64 binaries. Once all
# the builds have completed you can run release.sh.
#
#!/bin/bash -x
make deps
patch Makefile <<EOF
--- Makefile.old	2012-12-11 15:15:48.197138743 +0000
+++ Makefile.new	2012-12-11 15:16:06.053138618 +0000
@@ -21 +21 @@
-PRE_BUILD        := \$(ROOT)/3rd/fx2lib/lib/fx2.lib gen_fw
+#PRE_BUILD        := \$(ROOT)/3rd/fx2lib/lib/fx2.lib gen_fw
EOF
make MACHINE=x86 deps

# TODO: User Manual & AVR
#cd docs/user_manual
#./build.sh
#cd ../../firmware/avr
#make
