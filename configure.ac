dnl Process this file with autoconf to produce a configure script.
dnl ## Initialize autoconf ##

AC_INIT(src/gwave.c)
AM_INIT_AUTOMAKE(gwave, 20081227)
AM_MAINTAINER_MODE
AM_CONFIG_HEADER(config.h)

dnl Use ANSI mode
AM_PROG_CC_STDC

dnl ## Checks for programs ##
AC_PROG_CC
AC_PATH_PROGS(AWK, gawk mawk nawk awk, false)
AC_PROG_RANLIB
AC_PROG_LN_S
AC_PATH_PROGS(PERL, perl perl5, no)
AC_SUBST(PERL)

# this tries to find GNU graph only, not traditional berkeley/sysv graph,
# so we do the right thing on solaris, where an unusable one is in /usr/bin
# and the graph we want is usually in /usr/local/bin or some such.
AC_ARG_WITH(graph, [full pathname to GNU graph for postscript plotting])
dnl AC_MSG_RESULT(with_graph is $with_graph)
if test "$with_graph" != no; then
	if test "$with_graph" != yes && test ! -z "$with_graph" ; then
		AC_DEFINE_UNQUOTED(PROG_GRAPH, "$with_graph", [If present, the pathname of graph, from GNU plotutils.])
	else
	    	AC_PROG_GREPSTDOUT(GRAPH, graph --version, GNU, $ac_dir/$ac_word)
		AC_DEFINE_UNQUOTED(PROG_GRAPH, "$GRAPH", [If present, the pathname of graph, from GNU plotutils.])
	fi
fi

dnl do we need libtool? 
dnl even though we aren't building shared libraries, it seemed to help on HP-UX
dnl AM_PROG_LIBTOOL

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST

dnl Checks for library functions.
AC_FUNC_MEMCMP

dnl check for posix regular expression routines
AC_CHECK_HEADER([regex.h], [AC_CHECK_FUNC([regcomp], [AC_DEFINE(HAVE_POSIX_REGEXP,1,[defined if we have POSIX regular expression routines])])])

AC_CHECK_HEADERS([sys/types.h])

dnl check for GTK+
AM_PATH_GTK_2_0(2.0.8, AC_DEFINE(HAVE_GTK2,1,defined if we have GTK), AC_MSG_ERROR(Can not find GTK+-2.0.8 or later on this system))


dnl check for readline library
AC_CHECK_LIB(termcap, tgoto, [
        AC_CHECK_LIB(readline, readline, [
        READLINE_LIB="-lreadline -ltermcap"
        AC_DEFINE(HAVE_READLINE,1,[Define this if you have the readline library])
        AC_CHECK_LIB(readline, add_history, AC_DEFINE(HAVE_HISTORY,1,[Define this if your readline also has add_history]),,"-ltermcap"
)
        ], READLINE_LIB="", "-ltermcap")  
], AC_CHECK_LIB(ncurses, tgoto, [
        AC_CHECK_LIB(readline, readline, [
                READLINE_LIB="-lreadline -lncurses"
                AC_DEFINE(HAVE_READLINE,1,[Define this if you have the readline library])
                AC_CHECK_LIB(readline, add_history, AC_DEFINE(HAVE_HISTORY,1,[Define this if your readline also has add_history]),,"-l
ncurses")
        ], READLINE_LIB="", "-lncurses")
], READLINE_LIB=""))

AC_SUBST(READLINE_LIB)


dnl check for guile
GUILE_FLAGS

dnl checks for guile version-specific features
dnl most of these cribbed from scwm by Greg Badros and Maciej Stachowiak

dnl This checks (roughly) if the guile lib is a sufficiently recent snapshot
dnl to have a scm_eval_string that is safe against re-entry by continuations. 
dnl I was to lazy to write a real test macro so I just check for a function
dnl that was added soon after.
AC_CHECK_LIB(guile, scm_done_malloc, AC_DEFINE(HAVE_SAFE_SCM_EVAL_STRING, 1, [Define this if your libguile has a scm_eval_string that is safe against re-entry by continuations]),,$GUILE_LDFLAGS)

dnl This checks if we have a guile with readline support
AC_CHECK_LIB(guile, scm_readline, AC_DEFINE(HAVE_SCM_READLINE,1,[Define this if your libguile has readline support.]),,$GUILE_LDFLAGS)

AC_CHECK_LIB(guile, scm_internal_cwdr, AC_DEFINE(HAVE_SCM_INTERNAL_CWDR,1,[ Define this if your libguile has scm_internal_cwdr]),,$GUILE_LDFLAGS)

AC_CHECK_LIB(guile, scm_internal_stack_catch, AC_DEFINE(HAVE_SCM_INTERNAL_STACK_CATCH,1,[Define this if your libguile has scm_internal_stack_catch.]), ,$GUILE_LDFLAGS)

AC_CHECK_LIB(guile, scm_internal_parse_path, AC_DEFINE(HAVE_SCM_INTERNAL_PARSE_PATH,1,[Define this if your libguile has scm_internal_parse_path, which should be used instead of scm_parse_path from C.]), ,$GUILE_LDFLAGS)

AC_CHECK_LIB(guile, scm_make_hook, AC_DEFINE(HAVE_SCM_MAKE_HOOK,1,[Define this if your libguile has scm_make_hook, indicating C-level support for hooks.]), ancient_guile=yes,$GUILE_LDFLAGS)

AC_CHECK_LIB(guile, scm_create_hook, AC_DEFINE(HAVE_SCM_CREATE_HOOK,1,[Define this if your libguile has scm_create_hook]), ancient_guile=yes,$GUILE_LDFLAGS)

AC_CHECK_LIB(guile, scm_load_startup_files, AC_DEFINE(HAVE_SCM_LOAD_STARTUP_FILES,1,[Define this if your libguile has scm_load_startup_files]), ancient_guile=yes,$GUILE_LDFLAGS)

AC_CHECK_LIB(guile, scm_make_smob_type_mfpe, AC_DEFINE(HAVE_SCM_MAKE_SMOB_TYPE_MFPE,1,[Define this if your libguile has scm_make_smob_type_mfpe]), ancient_guile=yes, $GUILE_LDFLAGS)
 
if test "$ancient_guile" = "yes"; then
	AC_ERROR(Your guile is too old; Guile-1.6 or 1.8 is required)
fi

dnl check for guile-gtk

AC_PATH_PROG(bggtk, build-guile-gtk, no)
if test "no" = "$bggtk"; then
  AC_ERROR(Need guile-gtk 2.0 or later)
fi

AC_MSG_CHECKING(for guile-gtk CFLAGS)
GUILEGTK_CFLAGS=`$bggtk cflags`
AC_SUBST(GUILEGTK_CFLAGS)
AC_MSG_RESULT($GUILEGTK_CFLAGS)

AC_MSG_CHECKING(for guile-gtk LIBS)
GUILEGTK_LIBS=`$bggtk libs`
AC_SUBST(GUILEGTK_LIBS)
AC_MSG_RESULT($GUILEGTK_LIBS)

dnl Check to see if we are on a system with HP-UX's peculiar
dnl guile-gtk dynamic linking problems.
dnl should also check guile-gtk version, later ones may not need these hacks.
AC_MSG_CHECKING(with libtool for shlibpath_var)
shlib_var=`libtool --config  | grep '^shlibpath_var' | sed 's/^.*=//'`
AC_MSG_RESULT($shlib_var)
if test "$shlib_var" = SHLIB_PATH; then
	ggtkhack_files=dynlink.scm
	ggtk_extra_loadpath=$datadir/guile/app/gwave/guile-gtk-hack
	ggtk_hack_dir=$ggtk_extra_loadpath/gtk
	ggtk_hack_cflags="-DGUILE_GTK_EXTRA_LOADPATH=\\\"${ggtk_extra_loadpath}\\\""
else
	ggtk_hack_cflags=""
fi

AC_SUBST(ggtkhack_files)
AC_SUBST(ggtk_hack_dir)
AC_SUBST(ggtk_hack_cflags)
AC_SUBST(ggtk_extra_loadpath)

dnl doesn't work if prefix isn't specified.
dnl AC_DEFINE_UNQUOTED(DATADIR,"${datadir}",Installation prefix for finding necessary guile code)

AC_OUTPUT(Makefile \
src/Makefile spicefile/Makefile scheme/Makefile \
src/app/Makefile \
doc/Makefile  \
remote/Makefile \
utilities/Makefile \
utilities/doc-split \
utilities/guile-func-name-check \
utilities/gwave-doc-snarf \
utilities/sweepsplit \
gwave.spec  \
scheme/gwave-config.scm)
