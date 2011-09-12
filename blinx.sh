# Helper script for building the binary distribution. It's unlikely you'll need this unless you're
# forking the project.
#
# Before calling release.sh, each architecture must be built. It makes sense to only generate XSVF
# files and firmware includes once, so this script comments out the pre- and post-build steps after
# building the x86_64 binaries. Once the i686 build starts, it's safe to run the build on MacOSX
# and Windows. Once all the builds have completed you can run release.sh.
#
#!/bin/bash -x
make deps
patch Makefile <<EOF
--- Makefile.old	2011-09-12 19:34:32.153318241 +0100
+++ Makefile.new	2011-09-12 19:35:02.563318261 +0100
@@ -20,2 +20,2 @@
-SUBDIRS          := tests-unit
-PRE_BUILD        := \$(ROOT)/3rd/fx2lib/lib/fx2.lib gen_fw gen_xsvf
+#SUBDIRS          := tests-unit
+#PRE_BUILD        := \$(ROOT)/3rd/fx2lib/lib/fx2.lib gen_fw gen_xsvf
EOF
make MACHINE=i686 deps
