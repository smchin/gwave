## autogen.sh - generate all the twisty little files.

# Generate aclocal.m4 from configure.in
aclocal

# Generate Makefile.in's from Makefile.am's and configure.in
# (adding missing files that may be needed for proper operation)
automake --add-missing --gnu

# Generate config.h.in from acconfig.h and configure.in
autoheader

# Generate configure from configure.in and aclocal.m4
autoconf

# run ./config.status, if it exists, to create new Makefiles in subdirectories.
# (Note that this only is useful if builddir == scrdir, otherwise the
#  user may need to run config.status from their builddir(s) manually)
test -x ./config.status && ./config.status

