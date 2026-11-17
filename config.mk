# Local build settings, read by the Makefile. Tracked, so uncommenting one leaves the tree dirty
# until you commit or revert it. Build only: never credentials, and a build must not need this file.

# Drop the layer's build scratch after staging. Unset keeps it so a rebuild is incremental.
# TUXBLOX_RM_DIST ?= 1

# Parallel jobs. Defaults to nproc.
# JOBS ?= 8
