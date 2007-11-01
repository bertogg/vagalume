AC_INIT(Makefile.am)
AM_INIT_AUTOMAKE(vagalume, 0.1)
AM_CONFIG_HEADER(config.h)

AC_CANONICAL_HOST

AC_PROG_CC
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_RANLIB
AC_PROG_LIBTOOL
AC_HEADER_STDC

# Basic dependencies
PKG_CHECK_MODULES(GTK2, gtk+-2.0)
AC_SUBST(GTK2_CFLAGS)
AC_SUBST(GTK2_LIBS)

PKG_CHECK_MODULES(GSTREAMER, gstreamer-0.10)
AC_SUBST(GSTREAMER_CFLAGS)
AC_SUBST(GSTREAMER_LIBS)

PKG_CHECK_MODULES(LIBXML2, libxml-2.0)
AC_SUBST(LIBXML2_CFLAGS)
AC_SUBST(LIBXML2_LIBS)

PKG_CHECK_MODULES(LIBCURL, libcurl)
AC_SUBST(LIBCURL_CFLAGS)
AC_SUBST(LIBCURL_LIBS)

# Platform support
AC_ARG_WITH(platform, [
  --with-platform=gnome   compile for Gnome (default)
  --with-platform=maemo3  compile for Maemo 3],
  PLATFORM=$withval, PLATFORM=gnome)

if test "$PLATFORM" = "maemo3"; then

   echo "Compiling for Maemo 3"

   PKG_CHECK_MODULES(HILDON, hildon-libs)
   AC_SUBST(HILDON_LIBS)
   AC_SUBST(HILDON_CFLAGS)

   PKG_CHECK_MODULES(OSSO, libosso osso-af-settings)
   AC_SUBST(OSSO_LIBS)
   AC_SUBST(OSSO_CFLAGS)

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
   AC_DEFINE([MAEMO3], [], [Defined if building for Maemo 3 (bora)])

elif test "$PLATFORM" = "gnome"; then

   echo "Compiling for Gnome"

else

   AC_MSG_ERROR([Unknown platform $PLATFORM selected])

fi

# Produce output files
AC_OUTPUT(
	  Makefile \
	  src/Makefile
)
