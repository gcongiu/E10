dnl
dnl record top-level directory (this one)
dnl A problem.  Some systems use an NFS automounter.  This can generate
dnl paths of the form /tmp_mnt/... . On SOME systems, that path is
dnl not recognized, and you need to strip off the /tmp_mnt. On others, 
dnl it IS recognized, so you need to leave it in.  Grumble.
dnl The real problem is that OTHER nodes on the same NFS system may not
dnl be able to find a directory based on a /tmp_mnt/... name.
dnl
dnl It is WRONG to use $PWD, since that is maintained only by the C shell,
dnl and if we use it, we may find the 'wrong' directory.  To test this, we
dnl try writing a file to the directory and then looking for it in the 
dnl current directory.  Life would be so much easier if the NFS automounter
dnl worked correctly.
dnl
dnl PAC_GETWD(varname [, filename ] )
dnl 
dnl Set varname to current directory.  Use filename (relative to current
dnl directory) if provided to double check.
dnl
dnl Need a way to use "automounter fix" for this.
dnl
define(PAC_GETWD,[
AC_MSG_CHECKING(for current directory name)
$1=$PWD
if test "${$1}" != "" -a -d "${$1}" ; then 
    if test -r ${$1}/.foo$$ ; then
        rm -f ${$1}/.foo$$
	rm -f .foo$$
    fi
    if test -r ${$1}/.foo$$ -o -r .foo$$ ; then
	$1=
    else
	echo "test" > ${$1}/.foo$$
	if test ! -r .foo$$ ; then
            rm -f ${$1}/.foo$$
	    $1=
        else
 	    rm -f ${$1}/.foo$$
	fi
    fi
fi
if test "${$1}" = "" ; then
    $1=`pwd | sed -e 's%/tmp_mnt/%/%g'`
fi
dnl
dnl First, test the PWD is sensible
ifelse($2,,,
if test ! -r ${$1}/$2 ; then
    dnl PWD must be messed up
    $1=`pwd`
    if test ! -r ${$1}/$2 ; then
	print_error "Cannot determine the root directory!" 
        exit 1
    fi
    $1=`pwd | sed -e 's%/tmp_mnt/%/%g'`
    if test ! -d ${$1} ; then 
        print_error "Warning: your default path uses the automounter; this may"
        print_error "cause some problems if you use other NFS-connected systems."
        $1=`pwd`
    fi
fi)
if test -z "${$1}" ; then
    $1=`pwd | sed -e 's%/tmp_mnt/%/%g'`
    if test ! -d ${$1} ; then 
        print_error "Warning: your default path uses the automounter; this may"
        print_error "cause some problems if you use other NFS-connected systems."
        $1=`pwd`
    fi
fi
AC_MSG_RESULT(${$1})
])dnl
dnl
dnl PAC_OUTPUT_EXEC(files[,mode]) - takes files (as shell script or others),
dnl and applies configure to the them.  Basically, this is what AC_OUTPUT
dnl should do, but without adding a comment line at the top.
dnl Must be used ONLY after AC_OUTPUT (it needs config.status, which 
dnl AC_OUTPUT creates).
dnl Optionally, set the mode (+x, a+x, etc)
dnl
define(PAC_OUTPUT_EXEC,[
CONFIG_FILES="$1"
export CONFIG_FILES
./config.status
CONFIG_FILES=""
for pac_file in $1 ; do 
    rm -f .pactmp
    sed -e '1d' $pac_file > .pactmp
    rm -f $pac_file
    mv .pactmp $pac_file
    ifelse($2,,,chmod $2 $pac_file)
done
])dnl
dnl
dnl We need routines to check that make works.  Possible problems with
dnl make include
dnl
dnl It is really gnumake, and contrary to the documentation on gnumake,
dnl it insists on screaming everytime a directory is changed.  The fix
dnl is to add the argument --no-print-directory to the make
dnl
dnl It is really BSD 4.4 make, and can't handle 'include'.  For some
dnl systems, this can be fatal; there is no fix (other than removing this
dnl aleged make).
dnl
dnl It is the OSF V3 make, and can't handle a comment in a block of targe
dnl code.  There is no acceptable fix.
dnl
dnl This assumes that "MAKE" holds the name of the make program.  If it
dnl determines that it is an improperly built gnumake, it adds
dnl --no-print-directorytries to the symbol MAKE.
define(PAC_MAKE_IS_GNUMAKE,[
AC_MSG_CHECKING(gnumake)
rm -f conftest
cat > conftest <<.
SHELL=/bin/sh
ALL:
	@(dir=`pwd` ; cd .. ; \$(MAKE) -f \$\$dir/conftest SUB)
SUB:
	@echo "success"
.
str=`$MAKE -f conftest 2>&1`
if test "$str" != "success" ; then
    str=`$MAKE --no-print-directory -f conftest 2>&1`
    if test "$str" = "success" ; then
        MAKE="$MAKE --no-print-directory"
	AC_MSG_RESULT(yes using --no-print-directory)
    else
	AC_MSG_RESULT(no)
    fi
else
    AC_MSG_RESULT(no)
fi
rm -f conftest
str=""
])dnl
dnl
dnl PAC_MAKE_IS_BSD44([true text])
dnl
define(PAC_MAKE_IS_BSD44,[
AC_MSG_CHECKING(BSD 4.4 make)
rm -f conftest
cat > conftest <<.
ALL:
	@echo "success"
.
cat > conftest1 <<.
include conftest
.
str=`$MAKE -f conftest1 2>&1`
rm -f conftest conftest1
if test "$str" != "success" ; then
    AC_MSG_RESULT(Found BSD 4.4 so-called make)
    echo "The BSD 4.4 make is INCOMPATIBLE with all other makes."
    echo "Using this so-called make may cause problems when building programs."
    echo "You should consider using gnumake instead."
    ifelse([$1],,[$1])
else
    AC_MSG_RESULT(no - whew)
fi
str=""
])dnl
dnl
dnl PAC_MAKE_IS_OSF([true text])
dnl
define(PAC_MAKE_IS_OSF,[
AC_MSG_CHECKING(OSF V3 make)
rm -f conftest
cat > conftest <<.
SHELL=/bin/sh
ALL:
	@# This is a valid comment!
	@echo "success"
.
str=`$MAKE -f conftest 2>&1`
rm -f conftest 
if test "$str" != "success" ; then
    AC_MSG_RESULT(Found OSF V3 make)
    echo "The OSF V3 make does not allow comments in target code."
    echo "Using this make may cause problems when building programs."
    echo "You should consider using gnumake instead."
    ifelse([$1],,[$1])
else
    AC_MSG_RESULT(no)
fi
str=""
])dnl
dnl
dnl Look for a style of VPATH.  Known forms are
dnl VPATH = .:dir
dnl .PATH: . dir
dnl
dnl Defines VPATH or .PATH with . $(srcdir)
dnl Requires that vpath work with implicit targets
dnl NEED TO DO: Check that $< works on explicit targets.
dnl
define(PAC_MAKE_VPATH,[
AC_SUBST(VPATH)
AC_MSG_CHECKING(for virtual path format)
rm -rf conftest*
mkdir conftestdir
cat >conftestdir/a.c <<EOF
A sample file
EOF
cat > conftest <<EOF
all: a.o
VPATH=.:conftestdir
.c.o:
	@echo \$<
EOF
ac_out=`$MAKE -f conftest 2>&1 | grep 'conftestdir/a.c'`
if test -n "$ac_out" ; then 
    AC_MSG_RESULT(VPATH)
    VPATH='VPATH=.:$(srcdir)'
else
    rm -f conftest
    cat > conftest <<EOF
all: a.o
.PATH: . conftestdir
.c.o:
	@echo \$<
EOF
    ac_out=`$MAKE -f conftest 2>&1 | grep 'conftestdir/a.c'`
    if test -n "$ac_out" ; then 
        AC_MSG_RESULT(.PATH)
        VPATH='.PATH: . $(srcdir)'
    else
	AC_MSG_RESULT(neither VPATH nor .PATH works)
    fi
fi
rm -rf conftest*
])dnl
dnl
dnl PAC_MSG_ERROR($enable_softerr,ErrorMsg) - 
dnl return AC_MSG_ERROR(ErrorMsg) if "$enable_softerr" = "yes"
dnl return AC_MSG_WARN(ErrorMsg) + exit 0 otherwise
dnl
define(PAC_MSG_ERROR,[
if test "$1" = "yes" ; then
    AC_MSG_WARN([ $2 ])
    exit 0
else
    AC_MSG_ERROR([ $2 ])
fi
])dnl
dnl/*D
dnl PAC_PROG_CHECK_INSTALL_WORKS - Check whether the install program in INSTALL
dnl works.
dnl
dnl Synopsis:
dnl PAC_PROG_CHECK_INSTALL_WORKS
dnl
dnl Output Effect:
dnl   Sets the variable 'INSTALL' to the value of 'ac_sh_install' if 
dnl   a file cannot be installed into a local directory with the 'INSTALL'
dnl   program
dnl
dnl Notes:
dnl   The 'AC_PROG_INSTALL' scripts tries to avoid broken versions of 
dnl   install by avoiding directories such as '/usr/sbin' where some 
dnl   systems are known to have bad versions of 'install'.  Unfortunately, 
dnl   this is exactly the sort of test-on-name instead of test-on-capability
dnl   that 'autoconf' is meant to eliminate.  The test in this script
dnl   is very simple but has been adequate for working around problems 
dnl   on Solaris, where the '/usr/sbin/install' program (known by 
dnl   autoconf to be bad because it is in /usr/sbin) is also reached by a 
dnl   soft link through /bin, which autoconf believes is good.
dnl
dnl   No variables are cached to ensure that we do not make a mistake in 
dnl   our choice of install program.
dnl
dnl   The Solaris configure requires the directory name to immediately
dnl   follow the '-c' argument, rather than the more common 
dnl.vb
dnl      args sourcefiles destination-dir
dnl.ve
dnl D*/
AC_DEFUN([PAC_PROG_CHECK_INSTALL_WORKS],[
if test -z "$INSTALL" ; then
    AC_MSG_RESULT([No install program available])
else
    # Check that this install really works
    rm -f conftest
    echo "Test file" > conftest
    if test ! -d .conftest ; then mkdir .conftest ; fi
    AC_MSG_CHECKING([whether install works])
    if $INSTALL conftest .conftest >/dev/null 2>&1 ; then
        installOk=yes
    else
        installOk=no
    fi
    rm -rf .conftest conftest
    AC_MSG_RESULT($installOk)
    if test "$installOk" = no ; then
        if test -n "$ac_install_sh" ; then
            INSTALL=$ac_install_sh
        else
	    AC_MSG_ERROR([Unable to find working install])
        fi
    fi
fi
])
dnl
dnl Check for a broken install (fails to preserve file modification times,
dnl thus breaking libraries.
dnl
dnl Create a library, install it, and then try to link against it.
AC_DEFUN([PAC_PROG_INSTALL_BREAKS_LIBS],[
AC_CACHE_CHECK([whether install breaks libraries],
ac_cv_prog_install_breaks_libs,[
AC_REQUIRE([AC_PROG_RANLIB])
AC_REQUIRE([AC_PROG_INSTALL])
AC_REQUIRE([AC_PROG_CC])
ac_cv_prog_install_breaks_libs=yes
rm -f libconftest* conftest*
echo 'int foo(int);int foo(int a){return a;}' > conftest1.c
echo 'extern int foo(int); int main( int argc, char **argv){ return foo(0); }' > conftest2.c
if ${CC-cc} $CFLAGS -c conftest1.c >conftest.out 2>&1 ; then
    if ${AR-ar} cr libconftest.a conftest1.o >/dev/null 2>&1 ; then
        if ${RANLIB-:} libconftest.a >/dev/null 2>&1 ; then
            # Anything less than sleep 10, and Mac OS/X (Darwin)
            # will claim that install works because ranlib won't complain
            sleep 10
            libinstall="$INSTALL"
            eval "libinstall=\"$libinstall\""
            if ${libinstall} libconftest.a libconftest1.a  >/dev/null 2>&1 ; then
                if ${CC-cc} $CFLAGS -o conftest conftest2.c $LDFLAGS libconftest1.a >>conftest.out 2>&1 && test -x conftest ; then
                    # Success!  Install works
                    ac_cv_prog_install_breaks_libs=no
                else
                    # Failure!  Does install -p work?
                    rm -f libconftest1.a
                    if ${libinstall} -p libconftest.a libconftest1.a >/dev/null 2>&1 ; then
                        if ${CC-cc} $CFLAGS -o conftest conftest2.c $LDFLAGS libconftest1.a >>conftest.out 2>&1 && test -x conftest ; then
                        # Success!  Install works
                            ac_cv_prog_install_breaks_libs="no, with -p"
                        fi
                    fi
                fi
            fi
        fi
    fi
fi
rm -f conftest* libconftest*])

if test -z "$RANLIB_AFTER_INSTALL" ; then
    RANLIB_AFTER_INSTALL=no
fi
case "$ac_cv_prog_install_breaks_libs" in
        yes)
            RANLIB_AFTER_INSTALL=yes
        ;;
        "no, with -p")
            INSTALL="$INSTALL -p"
        ;;
        *)
        # Do nothing
        :
        ;;
esac
AC_SUBST(RANLIB_AFTER_INSTALL)
])
dnl
dnl
dnl
dnl Fixes to bugs in AC_xxx macros
dnl 
dnl (AC_TRY_COMPILE is missing a newline after the end in the Fortran
dnl branch; that has been fixed in-place)
dnl
dnl (AC_PROG_CC makes many dubious assumptions.  One is that -O is safe
dnl with -g, even with gcc.  This isn't true; gcc will eliminate dead code
dnl when -O is used, even if you added code explicitly for debugging 
dnl purposes.  -O shouldn't do dead code elimination when -g is selected, 
dnl unless a specific option is selected.  Unfortunately, there is no
dnl documented option to turn off dead code elimination.
dnl
dnl
dnl (AC_CHECK_HEADER and AC_CHECK_HEADERS both make the erroneous assumption
dnl that the C-preprocessor and the C (or C++) compilers are the same program
dnl and have the same search paths.  In addition, CHECK_HEADER looks for 
dnl error messages to decide that the file is not available; unfortunately,
dnl it also interprets messages such as "evaluation copy" and warning messages
dnl from broken CPP programs (such as IBM's xlc -E, which often warns about 
dnl "lm not a valid option").  Instead, we try a compilation step with the 
dnl C compiler.
dnl
dnl AC_CONFIG_AUX_DIRS only checks for install-sh, but assumes other
dnl values are present.  Also doesn't provide a way to override the
dnl sources of the various configure scripts.  This replacement
dnl version of AC_CONFIG_AUX_DIRS overcomes this.
dnl Internal subroutine.
dnl Search for the configuration auxiliary files in directory list $1.
dnl We look only for install-sh, so users of AC_PROG_INSTALL
dnl do not automatically need to distribute the other auxiliary files.
dnl AC_CONFIG_AUX_DIRS(DIR ...)
dnl Also note that since AC_CONFIG_AUX_DIR_DEFAULT calls this, there
dnl isn't a easy way to fix it other than replacing it completely.
dnl This fix applies to 2.13
dnl/*D
dnl AC_CONFIG_AUX_DIRS - Find the directory containing auxillery scripts
dnl for configure
dnl
dnl Synopsis:
dnl AC_CONFIG_AUX_DIRS( [ directories to search ] )
dnl
dnl Output Effect:
dnl Sets 'ac_config_guess' to location of 'config.guess', 'ac_config_sub'
dnl to location of 'config.sub', 'ac_install_sh' to the location of
dnl 'install-sh' or 'install.sh', and 'ac_configure' to the location of a
dnl Cygnus-style 'configure'.  Only 'install-sh' is guaranteed to exist,
dnl since the other scripts are needed only by some special macros.
dnl
dnl The environment variable 'CONFIG_AUX_DIR', if set, overrides the
dnl directories listed.  This is an extension to the 'autoconf' version of
dnl this macro. 
dnl D*/
undefine([AC_CONFIG_AUX_DIRS])
AC_DEFUN(AC_CONFIG_AUX_DIRS,
[if test -f $CONFIG_AUX_DIR/install-sh ; then ac_aux_dir=$CONFIG_AUX_DIR 
else
ac_aux_dir=
# We force the test to use the absolute path to ensure that the install
# program can be used if we cd to a different directory before using
# install.
for ac_dir in $1; do
  if test -f $ac_dir/install-sh; then
    ac_aux_dir=$ac_dir
    abs_ac_aux_dir=`(cd $ac_aux_dir && pwd)`
    ac_install_sh="$abs_ac_aux_dir/install-sh -c"
    break
  elif test -f $ac_dir/install.sh; then
    ac_aux_dir=$ac_dir
    abs_ac_aux_dir=`(cd $ac_aux_dir && pwd)`
    ac_install_sh="$abs_ac_aux_dir/install.sh -c"
    break
  fi
done
fi
if test -z "$ac_aux_dir"; then
  AC_MSG_ERROR([can not find install-sh or install.sh in $1])
fi
ac_config_guess=$ac_aux_dir/config.guess
ac_config_sub=$ac_aux_dir/config.sub
ac_configure=$ac_aux_dir/configure # This should be Cygnus configure.
AC_PROVIDE([AC_CONFIG_AUX_DIR_DEFAULT])dnl
])
