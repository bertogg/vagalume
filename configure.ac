# Initialization and basic checks
AC_INIT(Makefile.am)
AM_INIT_AUTOMAKE(vagalume, 0.5.1)
AM_CONFIG_HEADER(config.h)

AC_CANONICAL_HOST

AC_PROG_CC
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LIBTOOL
AC_HEADER_STDC

# Defines for config.h
AC_DEFINE_UNQUOTED([KERNEL_NAME],"`uname -s`",[Name of the kernel])
AC_DEFINE_UNQUOTED([KERNEL_NAME_LC],"`uname -s|tr A-Z a-z`",
                   [Name of the kernel (lowercase)])
AC_DEFINE_UNQUOTED([ARCH],"`uname -m`",[Architecture])

# Extra flags for compiler and linker
EXTRA_CFLAGS=""
EXTRA_LIBS=""

#####################################
### Basic dependencies - Required ###
#####################################
PKG_CHECK_MODULES(Gtk2, gtk+-2.0)
EXTRA_CFLAGS="$EXTRA_CFLAGS $Gtk2_CFLAGS"
EXTRA_LIBS="$EXTRA_LIBS $Gtk2_LIBS"

PKG_CHECK_MODULES(GStreamer, gstreamer-0.10)
EXTRA_CFLAGS="$EXTRA_CFLAGS $GStreamer_CFLAGS"
EXTRA_LIBS="$EXTRA_LIBS $GStreamer_LIBS"

PKG_CHECK_MODULES(libxml2, libxml-2.0)
EXTRA_CFLAGS="$EXTRA_CFLAGS $libxml2_CFLAGS"
EXTRA_LIBS="$EXTRA_LIBS $libxml2_LIBS"

PKG_CHECK_MODULES(libcurl, libcurl)
EXTRA_CFLAGS="$EXTRA_CFLAGS $libcurl_CFLAGS"
EXTRA_LIBS="$EXTRA_LIBS $libcurl_LIBS"

#####################################
### Other dependencies - Optional ###
#####################################
PKG_CHECK_MODULES(dbus_glib, dbus-glib-1, [have_dbus_glib="yes"],
                  [have_dbus_glib="no"])

# Check if libgcrypt is installed
AC_CHECK_LIB(gcrypt, gcry_md_hash_buffer,
   have_libgcrypt="yes", have_libgcrypt="no")

# Check if libcurl has curl_easy_escape()
AC_CHECK_LIB(curl, curl_easy_escape,
   AC_DEFINE([HAVE_CURL_EASY_ESCAPE],[1],[If curl_easy_escape() exists]),
   have_curl_easy_escape=0
)

# Check whether to use libgcrypt
AC_ARG_ENABLE(libgcrypt, [
  --enable-libgcrypt      compute MD5 using libgcrypt (default if available)
  --disable-libgcrypt     use internal MD5 algorithm],
  use_libgcrypt=$enableval, use_libgcrypt=detect)

if test "$use_libgcrypt" = "detect"; then
   use_libgcrypt="$have_libgcrypt"
elif test "$use_libgcrypt" = "yes" -a "$have_libgcrypt" = "no"; then
   AC_MSG_ERROR([ligcrypt not found, install it or use --disable-libgcrypt])
fi

if test "$use_libgcrypt" = "yes"; then
   AC_DEFINE([HAVE_LIBGCRYPT],[1],[Defined if compiling with libgcrypt])
   EXTRA_LIBS="$EXTRA_LIBS -lgcrypt"
   echo "Using libgcrypt to compute md5sums"
else
   echo "Using internal implementation to compute md5sums"
fi
AM_CONDITIONAL(USE_INTERNAL_MD5, test "$use_libgcrypt" = "no")

# Check whether to use the "Set IM status" feature
AC_ARG_ENABLE(im-status, [
  --enable-im-status      Enable "Set IM status" feature (default if available)
  --disable-im-status     Disable "Set IM status" feature],
  set_im_status=$enableval, set_im_status=detect)

if test "$set_im_status" = "detect"; then
   set_im_status="$have_dbus_glib"
elif test "$set_im_status" = "yes" -a "$have_dbus_glib" = "no"; then
   AC_MSG_ERROR([dbus-glib not found, install it or use --disable-im-status])
fi

if test "$set_im_status" = "yes"; then
   AC_DEFINE([SET_IM_STATUS],[1],[Defined if "Set IM Status" is enabled])
   EXTRA_CFLAGS="$EXTRA_CFLAGS $dbus_glib_CFLAGS"
   EXTRA_LIBS="$EXTRA_LIBS $dbus_glib_LIBS"
   echo "\"Set IM status\" feature enabled"
else
   echo "\"Set IM status\" feature disabled"
fi
AM_CONDITIONAL(SET_IM_STATUS, test "$set_im_status" = "yes")

# Autodetect platform
if $PKG_CONFIG --exists libosso && readlink /etc|grep -q ^/targets; then
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
      PKG_CHECK_MODULES(hildon, hildon-libs)
      PKG_CHECK_MODULES(hildonfm, hildon-fm)
   else
      PKG_CHECK_MODULES(hildon, hildon-1)
      PKG_CHECK_MODULES(hildonfm, hildon-fm-2)
   fi
   EXTRA_CFLAGS="$EXTRA_CFLAGS $hildon_CFLAGS $hildonfm_CFLAGS"
   EXTRA_LIBS="$EXTRA_LIBS $hildon_LIBS $hildonfm_LIBS"

   PKG_CHECK_MODULES(libosso, libosso osso-af-settings)
   EXTRA_CFLAGS="$EXTRA_CFLAGS $libosso_CFLAGS"
   EXTRA_LIBS="$EXTRA_LIBS $libosso_LIBS"

   PKG_CHECK_MODULES(libconic, conic, [have_conic=1], [have_conic=0])
   if test "$have_conic" = "1"; then
      EXTRA_CFLAGS="$EXTRA_CFLAGS $libconic_CFLAGS"
      EXTRA_LIBS="$EXTRA_LIBS $libconic_LIBS"
   fi

   desktopentrydir=`$PKG_CONFIG osso-af-settings --variable=desktopentrydir`
   serviceentrydir=`$PKG_CONFIG osso-af-settings --variable=dbusservicedir`

   if test "x$desktopentrydir" = "x"; then
      AC_MSG_ERROR([Unable to find .desktop directory])
   elif test "x$serviceentrydir" = "x"; then
      AC_MSG_ERROR([Unable to find dbus service directory])
   fi

   PKG_CHECK_MODULES(SBPLUGIN, gtk+-2.0 glib-2.0 hildon-1 libhildondesktop
                               gconf-2.0 libhildonwm libosso osso-af-settings
                               dbus-1, [have_sb_plugin=1], [have_sb_plugin=0])

   if test "$have_sb_plugin" = "1"; then
      sbplugindesktopentrydir=`$PKG_CONFIG osso-af-settings \
                               --variable=statusbardesktopentrydir`
      sbplugindir=`$PKG_CONFIG osso-af-settings --variable=hildondesktoplibdir`

      if test "x$sbplugindesktopentrydir" = "x"; then
         AC_MSG_ERROR([Unable to find .desktop directory])
      elif test "x$sbplugindir" = "x"; then
         AC_MSG_ERROR([Unable to find hildon desktop libs directory])
      fi
   fi

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

   # serviceentrydir=`$PKG_CONFIG dbus-1 --variable=session_bus_services_dir`
   desktopentrydir="\${datadir}/applications"

else

   AC_MSG_ERROR([Unknown platform $PLATFORM selected])

fi

AM_CONDITIONAL(MAEMO_PLATFORM, test "$PLATFORM" = "maemo")

AM_CONDITIONAL(HAVE_DBUS_SUPPORT, test -n "$serviceentrydir")
if test -n "$serviceentrydir"; then
   AC_DEFINE([HAVE_DBUS_SUPPORT], [1], [Defined if compiling with D-BUS])
fi

AM_CONDITIONAL(HAVE_CONIC, test "$have_conic" = "1")
if test "$have_conic" = "1"; then
   AC_DEFINE([HAVE_CONIC], [1], [Defined if building with libconic])
fi

AM_CONDITIONAL(HAVE_MAEMO_SB_PLUGIN, test "$have_sb_plugin" = "1")
if test "$have_sb_plugin" = "1"; then
   AC_DEFINE([HAVE_MAEMO_SB_PLUGIN], [1],
             [Defined if building the Maemo status bar plugin])
   AC_SUBST(SBPLUGIN_LIBS)
   AC_SUBST(SBPLUGIN_CFLAGS)
   AC_SUBST(sbplugindesktopentrydir)
   AC_SUBST(sbplugindir)
fi

AC_SUBST(appdatadir, "\${datadir}/vagalume")
AC_SUBST(icondir, "\${datadir}/pixmaps")
AC_SUBST(icon48dir, "\${datadir}/icons/hicolor/48x48/apps")
AC_SUBST(serviceentrydir)
AC_SUBST(desktopentrydir)
AC_SUBST(EXTRA_CFLAGS)
AC_SUBST(EXTRA_LIBS)
AC_SUBST(PLATFORM)

# Produce output files
AC_OUTPUT(
	  Makefile \
	  src/Makefile
)
