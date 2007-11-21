AC_INIT(Makefile.am)
AM_INIT_AUTOMAKE(vagalume, 0.2)
AM_CONFIG_HEADER(config.h)

AC_CANONICAL_HOST

AC_PROG_CC
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LIBTOOL
AC_HEADER_STDC

AC_DEFINE_UNQUOTED([KERNEL_NAME],"`uname -s`",[Name of the kernel])
AC_DEFINE_UNQUOTED([KERNEL_NAME_LC],"`uname -s|tr A-Z a-z`",
                   [Name of the kernel (lowercase)])
AC_DEFINE_UNQUOTED([ARCH],"`uname -m`",[Architecture])

vagalumedatadir="\${datadir}/vagalume"
AC_SUBST(vagalumedatadir)

EXTRA_CFLAGS=""
EXTRA_LIBS=""

# Basic dependencies
PKG_CHECK_MODULES(GTK2, gtk+-2.0)
EXTRA_CFLAGS="$EXTRA_CFLAGS $GTK2_CFLAGS"
EXTRA_LIBS="$EXTRA_LIBS $GTK2_LIBS"

PKG_CHECK_MODULES(GSTREAMER, gstreamer-0.10)
EXTRA_CFLAGS="$EXTRA_CFLAGS $GSTREAMER_CFLAGS"
EXTRA_LIBS="$EXTRA_LIBS $GSTREAMER_LIBS"

PKG_CHECK_MODULES(LIBXML2, libxml-2.0)
EXTRA_CFLAGS="$EXTRA_CFLAGS $LIBXML2_CFLAGS"
EXTRA_LIBS="$EXTRA_LIBS $LIBXML2_LIBS"

PKG_CHECK_MODULES(LIBCURL, libcurl)
EXTRA_CFLAGS="$EXTRA_CFLAGS $LIBCURL_CFLAGS"
EXTRA_LIBS="$EXTRA_LIBS $LIBCURL_LIBS"

# Check if libcurl has curl_easy_escape()
AC_CHECK_LIB(curl, curl_easy_escape,
   AC_DEFINE([HAVE_CURL_EASY_ESCAPE],[1],[If curl_easy_escape() exists]),
   have_curl_easy_escape=0
)

# Autodetect platform
if $PKG_CONFIG --exists libosso; then
   DETECTED_PLATFORM=maemo
else
   DETECTED_PLATFORM=gnome
fi

# Platform support
AC_ARG_WITH(platform, [
Target platform:
  --with-platform=detect  detect target platform (default)
  --with-platform=gnome   compile for Gnome
  --with-platform=maemo   compile for Maemo],
  PLATFORM=$withval, PLATFORM=detect)

if test "$PLATFORM" = "detect"; then
   PLATFORM=$DETECTED_PLATFORM
fi

if test "$PLATFORM" = "maemo"; then

   libossovers=`$PKG_CONFIG libosso --modversion | cut -d . -f 1`

   if test "$have_curl_easy_escape" = "0"; then
      MAEMOVERS=2
      echo "Compiling for Maemo 2"
   elif test "$libossovers" = "1"; then
      MAEMOVERS=3
      echo "Compiling for Maemo 3"
   elif test "$libossovers" = "2"; then
      MAEMOVERS=4
      echo "Compiling for Maemo 4"
   else
      AC_MSG_ERROR([Unknown Maemo version])
   fi

   if test "$MAEMOVERS" = "2" -o "$MAEMOVERS" = "3"; then
      PKG_CHECK_MODULES(HILDON, hildon-libs)
   else
      PKG_CHECK_MODULES(HILDON, hildon-1)
   fi
   EXTRA_CFLAGS="$EXTRA_CFLAGS $HILDON_CFLAGS"
   EXTRA_LIBS="$EXTRA_LIBS $HILDON_LIBS"


   PKG_CHECK_MODULES(OSSO, libosso osso-af-settings)
   EXTRA_CFLAGS="$EXTRA_CFLAGS $OSSO_CFLAGS"
   EXTRA_LIBS="$EXTRA_LIBS $OSSO_LIBS"

   desktopentrydir=`$PKG_CONFIG osso-af-settings --variable=desktopentrydir`
   serviceentrydir=`$PKG_CONFIG osso-af-settings --variable=dbusservicedir`

   if test "x$desktopentrydir" = "x"; then
      AC_MSG_ERROR([Unable to find .desktop directory])
   elif test "x$serviceentrydir" = "x"; then
      AC_MSG_ERROR([Unable to find dbus service directory])
   fi

   AC_SUBST(desktopentrydir)
   AC_SUBST(serviceentrydir)

   AC_DEFINE([MAEMO], [], [Defined if building for Maemo])
   if test "$MAEMOVERS" = "2"; then
      AC_DEFINE([MAEMO2], [], [Defined if building for Maemo 2 (gregale)])
   elif test "$MAEMOVERS" = "3"; then
      AC_DEFINE([MAEMO3], [], [Defined if building for Maemo 3 (bora)])
   else
      AC_DEFINE([MAEMO4], [], [Defined if building for Maemo 4 (chinook)])
   fi

elif test "$PLATFORM" = "gnome"; then

   echo "Compiling for Gnome"

else

   AC_MSG_ERROR([Unknown platform $PLATFORM selected])

fi

AM_CONDITIONAL(MAEMO, test "$PLATFORM" = "maemo")

AC_SUBST(EXTRA_CFLAGS)
AC_SUBST(EXTRA_LIBS)

# Produce output files
AC_OUTPUT(
	  Makefile \
	  src/Makefile
)
