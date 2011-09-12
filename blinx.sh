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
--- x	2011-09-03 20:34:31.982767951 +0100
+++ y	2011-09-03 20:34:40.732767956 +0100
@@ -21,1 +21,1 @@
-PRE_BUILD        := \$(ROOT)/3rd/fx2lib/lib/fx2.lib gen_fw gen_xsvf
+#PRE_BUILD        := \$(ROOT)/3rd/fx2lib/lib/fx2.lib gen_fw gen_xsvf
EOF
make MACHINE=i686 deps
