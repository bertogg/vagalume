#######################################
### Initialization and basic checks ###
#######################################
AC_INIT(Makefile.am)
AM_INIT_AUTOMAKE(vagalume, 0.6)
AM_CONFIG_HEADER(config.h)
AC_CANONICAL_HOST
AC_PROG_CC
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LIBTOOL
AC_PROG_LN_S
AC_HEADER_STDC

# Some variables
VAGALUME_CONFIG_DIR=".vagalume"

# Defines for config.h
AC_DEFINE_UNQUOTED([KERNEL_NAME],"`uname -s`",[Name of the kernel])
AC_DEFINE_UNQUOTED([KERNEL_NAME_LC],"`uname -s|tr A-Z a-z`",
                   [Name of the kernel (lowercase)])
AC_DEFINE_UNQUOTED([ARCH],"`uname -m`",[Architecture])
AC_DEFINE_UNQUOTED(VAGALUME_CONFIG_DIR, "$VAGALUME_CONFIG_DIR",
                   [Vagalume configuration directory])

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

PKG_CHECK_MODULES(libnotify, libnotify >= 0.4.1,
                  [have_libnotify="yes"], [have_libnotify="no"])

# D-BUS versions < 1.0 require DBUS_API_SUBJECT_TO_CHANGE
if test "$have_dbus_glib" = "yes"; then
   if ! $PKG_CONFIG --atleast-version=1.0 dbus-1; then
      dbus_glib_CFLAGS="$dbus_glib_CFLAGS -DDBUS_API_SUBJECT_TO_CHANGE"
   fi
fi

# Tray icon support requires Gtk 2.10 and libnotify
if test "$have_libnotify" = "yes"; then
   echo -n "checking for Gtk2 >= 2.10... "
   if $PKG_CONFIG --atleast-version=2.10 gtk+-2.0; then
      have_tray_icon="yes"
   else
      have_tray_icon="no"
   fi
   echo $have_tray_icon
fi

# GChecksum requires glib 2.16
echo -n "checking for glib >= 2.16... "
if $PKG_CONFIG --atleast-version=2.16 glib-2.0; then
   have_gchecksum="yes"
else
   have_gchecksum="no"
fi
echo $have_gchecksum

# Check if we have getopt
AC_CHECK_LIB(c, getopt,
   AC_DEFINE([HAVE_GETOPT],[1],[Defined if getopt() exists]),
   have_getopt="no")

# Check if libgcrypt is installed
AC_CHECK_LIB(gcrypt, gcry_md_hash_buffer,
   have_libgcrypt="yes", have_libgcrypt="no")

# Check if libcurl has curl_easy_escape()
AC_CHECK_LIB(curl, curl_easy_escape,
   AC_DEFINE([HAVE_CURL_EASY_ESCAPE],[1],[If curl_easy_escape() exists]),
   have_curl_easy_escape="no")

#################################
### User configurable options ###
#################################
# Autodetect platform (with ugly hack to detect scratchbox, I know)
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
  --with-platform=maemo   compile for Maemo
  --with-platform=moblin  compile for Moblin (experimental)
  --with-platform=windows compile for Windows (experimental)],
  PLATFORM=$withval, PLATFORM=detect)

if test "$PLATFORM" = "detect"; then
   PLATFORM=$DETECTED_PLATFORM
fi
PLATFORMVERS=""

# Check whether to use libgcrypt
AC_ARG_ENABLE(libgcrypt, [
  --enable-libgcrypt      use libgcrypt if glib doesn't support MD5
                          checksums (default in Gnome if libgcrypt exists)
  --disable-libgcrypt     prefer internal MD5 implementation over libgcrypt
                          (default in Maemo)],
  use_libgcrypt=$enableval, use_libgcrypt=default)

# If neither --enable nor --disable have been specified, select the default
if test "$use_libgcrypt" = "default"; then
   if test "$PLATFORM" = "gnome"; then
      use_libgcrypt="$have_libgcrypt"
   else
      use_libgcrypt="no"
   fi
fi

# Decide the actual MD5 implementation to be used
if test "$have_gchecksum" = "yes"; then
   AC_DEFINE([HAVE_GCHECKSUM],[1],[Defined if GChecksum exists])
   use_libgcrypt="no"
   md5_method="glib"
elif test "$use_libgcrypt" = "yes" -a "$have_libgcrypt" = "no"; then
   AC_MSG_ERROR([ligcrypt not found, install it or use --disable-libgcrypt])
elif test "$use_libgcrypt" = "yes"; then
   AC_DEFINE([HAVE_LIBGCRYPT],[1],[Defined if compiling with libgcrypt])
   EXTRA_LIBS="$EXTRA_LIBS -lgcrypt"
   md5_method="libgcrypt"
else
   use_internalmd5="yes"
   md5_method="internal implementation"
fi
echo "Using $md5_method to compute md5sums"
AM_CONDITIONAL(USE_INTERNAL_MD5, test "x$use_internalmd5" = "xyes")
AM_CONDITIONAL(USE_LIBGCRYPT, test "x$use_libgcrypt" = "xyes")

# Check whether to use the "Set IM status" feature
AC_ARG_ENABLE(im-status, [
  --enable-im-status      Enable "Set IM status" feature (default if available)
  --disable-im-status     Disable "Set IM status" feature],
  set_im_status=$enableval, set_im_status=$have_dbus_glib)

if test "$set_im_status" = "yes" -a "$have_dbus_glib" = "no"; then
   AC_MSG_ERROR([dbus-glib not found, install it or use --disable-im-status])
elif test "$set_im_status" = "yes"; then
   AC_DEFINE([SET_IM_STATUS],[1],[Defined if "Set IM Status" is enabled])
   EXTRA_CFLAGS="$EXTRA_CFLAGS $dbus_glib_CFLAGS"
   EXTRA_LIBS="$EXTRA_LIBS $dbus_glib_LIBS"
   echo "\"Set IM status\" feature enabled"
else
   echo "\"Set IM status\" feature disabled"
fi
AM_CONDITIONAL(SET_IM_STATUS, test "$set_im_status" = "yes")

# Check whether to enable the tray icon and libnotify support
AC_ARG_ENABLE(tray-icon, [
  --enable-tray-icon      Enable tray icon and libnotify support
                          (Gnome only, default if available)
  --disable-tray-icon     Disable tray icon and libnotify support],
  enable_tray_icon=$enableval, enable_tray_icon=$have_tray_icon)

#################################
### Platform dependent checks ###
#################################
if test "$PLATFORM" = "maemo"; then

   # Disable tray icon support on Maemo
   have_tray_icon="no"

   # Detect the version of the Maemo SDK
   libossovers=`$PKG_CONFIG libosso --modversion | cut -d . -f 1`

   if test "$have_curl_easy_escape" = "no"; then
      PLATFORMVERS=2
   elif test "$libossovers" = "1"; then
      PLATFORMVERS=3
   elif test "$libossovers" = "2"; then
      PLATFORMVERS=4
   else
      AC_MSG_ERROR([Unknown Maemo version])
   fi
   echo "Compiling for Maemo $PLATFORMVERS"

   # These are required but their names vary between Maemo versions
   if test "$PLATFORMVERS" = "2" -o "$PLATFORMVERS" = "3"; then
      PKG_CHECK_MODULES(hildon, hildon-libs)
      PKG_CHECK_MODULES(hildonfm, hildon-fm)
      AC_DEFINE([HILDON_LIBS], [1], [Defined if using hildon-libs])
      AC_DEFINE([HILDON_FM], [1], [Defined if using hildon-fm])
   else
      PKG_CHECK_MODULES(hildon, hildon-1)
      PKG_CHECK_MODULES(hildonfm, hildon-fm-2)
      AC_DEFINE([HILDON_1], [1], [Defined if using hildon-1])
      AC_DEFINE([HILDON_FM_2], [1], [Defined if using hildon-fm-2])
   fi
   EXTRA_CFLAGS="$EXTRA_CFLAGS $hildon_CFLAGS $hildonfm_CFLAGS"
   EXTRA_LIBS="$EXTRA_LIBS $hildon_LIBS $hildonfm_LIBS"

   # Libosso is required
   PKG_CHECK_MODULES(libosso, libosso osso-af-settings)
   EXTRA_CFLAGS="$EXTRA_CFLAGS $libosso_CFLAGS"
   EXTRA_LIBS="$EXTRA_LIBS $libosso_LIBS"

   # Libconic is not present in all Maemo versions
   PKG_CHECK_MODULES(libconic, conic, [have_conic="yes"], [have_conic="no"])
   if test "$have_conic" = "yes"; then
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

   # These are optional and only required for the Maemo status bar plugin
   echo "Checking dependencies to compile the Maemo status bar plugin"
   have_sb_plugin="yes"
   PKG_CHECK_MODULES(hildondesktop, libhildondesktop, [], [have_sb_plugin="no"])
   PKG_CHECK_MODULES(hildonwm, libhildonwm, [], [have_sb_plugin="no"])
   PKG_CHECK_MODULES(gconf2, gconf-2.0, [], [have_sb_plugin="no"])
   PKG_CHECK_MODULES(dbus, dbus-1, [], [have_sb_plugin="no"])

   if test "$have_sb_plugin" = "yes"; then
      SBPLUGIN_CFLAGS="$Gtk2_CFLAGS $hildon_CFLAGS $libosso_CFLAGS \
                       $hildondesktop_CFLAGS $hildonwm_CFLAGS \
                       $gconf2_CFLAGS $dbus_CFLAGS"
      SBPLUGIN_LIBS="$Gtk2_LIBS $hildon_LIBS $libosso_LIBS \
                     $hildondesktop_LIBS $hildonwm_LIBS \
                     $gconf2_LIBS $dbus_LIBS"
      sbplugindesktopentrydir=`$PKG_CONFIG osso-af-settings \
                               --variable=statusbardesktopentrydir`
      sbplugindir=`$PKG_CONFIG osso-af-settings --variable=hildondesktoplibdir`

      if test "x$sbplugindesktopentrydir" = "x"; then
         AC_MSG_ERROR([Unable to find .desktop directory])
      elif test "x$sbplugindir" = "x"; then
         AC_MSG_ERROR([Unable to find hildon desktop libs directory])
      fi
      echo "All required libs found: compiling Maemo status bar plugin"
   else
      echo "Some libs are missing: Unable to compile Maemo status bar plugin"
   fi

   AC_DEFINE([MAEMO], [], [Defined if building for Maemo])
   AC_DEFINE([USE_HILDON_WINDOW], [1], [Defined if using HildonWindow])
   if test "$PLATFORMVERS" = "2"; then
      AC_DEFINE([MAEMO2], [], [Defined if building for Maemo 2 (gregale)])
   elif test "$PLATFORMVERS" = "3"; then
      AC_DEFINE([MAEMO3], [], [Defined if building for Maemo 3 (bora)])
   else
      AC_DEFINE([MAEMO4], [], [Defined if building for Maemo 4 (chinook)])
   fi

   VAGALUME_CONF_FULLPATH="/home/user/$VAGALUME_CONFIG_DIR"
   AC_SUBST(VAGALUME_CONF_FULLPATH)

   BACKUP_CONFIGURATION_DIR="/etc/osso-backup/applications"
   AC_SUBST(BACKUP_CONFIGURATION_DIR)

   RESTORE_SETTINGS_SCRIPTS_DIR="/etc/osso-rfs-scripts"
   AC_SUBST(RESTORE_SETTINGS_SCRIPTS_DIR)

   CLEAR_DEVICE_SCRIPTS_DIR="/etc/osso-cud-scripts"
   AC_SUBST(CLEAR_DEVICE_SCRIPTS_DIR)

elif test "$PLATFORM" = "gnome"; then

   echo "Compiling for Gnome"

   if test "$have_dbus_glib" = "yes"; then
      serviceentrydir=`$PKG_CONFIG dbus-1 --variable=session_bus_services_dir`
      AC_DEFINE([HAVE_GSD_MEDIA_PLAYER_KEYS], [1],
                [Media player keys in some keyboards])
   fi

   desktopentrydir="\${datadir}/applications"

   if test "$enable_tray_icon" = "yes" -a "$have_tray_icon" = "no"; then
      AC_MSG_ERROR([Unable to compile with tray icon and libnotify support])
   elif test "$enable_tray_icon" = "yes"; then
      echo "Compiling with tray icon and notifications"
      # Add extra required parameters
      EXTRA_CFLAGS="$EXTRA_CFLAGS $libnotify_CFLAGS"
      EXTRA_LIBS="$EXTRA_LIBS $libnotify_LIBS"
   else
      have_tray_icon="no"
      echo "Compiling without tray icon and notifications"
   fi

elif test "$PLATFORM" = "moblin"; then

   # Disable tray icon support on Moblin
   have_tray_icon="no"

   desktopentrydir="\${datadir}/applications"

   echo "Compiling for Moblin"

   PKG_CHECK_MODULES(hildon, hildon-1)
   EXTRA_CFLAGS="$EXTRA_CFLAGS $hildon_CFLAGS"
   EXTRA_LIBS="$EXTRA_LIBS $hildon_LIBS"

   AC_DEFINE([MOBLIN], [1], [Defined if building for Moblin])
   AC_DEFINE([HILDON_1], [1], [Defined if using hildon-1])
   AC_DEFINE([USE_HILDON_WINDOW], [1], [Defined if using HildonWindow])

elif test "$PLATFORM" = "windows"; then

     echo "Compiling for windows"

     desktopentrydir="\${datadir}/applications"
     
	 if test "$enable_tray_icon" = "yes"; then
		have_tray_icon="yes"   
	 fi

     AC_DEFINE([WINDOWS], [], [Defined if building for windows])
   
else

   AC_MSG_ERROR([Unknown platform $PLATFORM selected])

fi

####################
### Translations ###
####################
ALL_LINGUAS="de es fi fr gl it lv pt pt_BR"

VAGALUME_LOCALE_DIR='$prefix/${DATADIRNAME}/locale'
AM_GLIB_DEFINE_LOCALEDIR([VAGALUME_LOCALE_DIR])
AM_GLIB_GNU_GETTEXT

GETTEXT_PACKAGE=vagalume
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, "$GETTEXT_PACKAGE", [Gettext package])

##################################################################
### Everything has been checked, now define Automake variables ###
##################################################################
AM_CONDITIONAL(MAEMO_PLATFORM, test "$PLATFORM" = "maemo")
AM_CONDITIONAL(WINDOWS_PLATFORM, test "$PLATFORM" = "windows")

AM_CONDITIONAL(HAVE_DBUS_SUPPORT, test -n "$serviceentrydir")
if test -n "$serviceentrydir"; then
   AC_DEFINE([HAVE_DBUS_SUPPORT], [1], [Defined if compiling with D-BUS])
fi

AM_CONDITIONAL(HAVE_CONIC, test "$have_conic" = "yes")
if test "$have_conic" = "yes"; then
   AC_DEFINE([HAVE_CONIC], [1], [Defined if building with libconic])
fi

AM_CONDITIONAL(HAVE_MAEMO_SB_PLUGIN, test "$have_sb_plugin" = "yes")
if test "$have_sb_plugin" = "yes"; then
   AC_DEFINE([HAVE_MAEMO_SB_PLUGIN], [1],
             [Defined if building the Maemo status bar plugin])
   AC_SUBST(SBPLUGIN_LIBS)
   AC_SUBST(SBPLUGIN_CFLAGS)
   AC_SUBST(sbplugindesktopentrydir)
   AC_SUBST(sbplugindir)
fi

AM_CONDITIONAL(HAVE_TRAY_ICON, test "$have_tray_icon" = "yes")
if test "$have_tray_icon" = "yes"; then
   AC_DEFINE([HAVE_TRAY_ICON], [1], [Defined if building the FD tray icon])
fi

AC_SUBST(appdatadir, "\${datadir}/vagalume")
AC_SUBST(pixmapdir, "\${datadir}/pixmaps")
AC_SUBST(icon48dir, "\${datadir}/icons/hicolor/48x48/apps")
AC_SUBST(serviceentrydir)
AC_SUBST(desktopentrydir)
AC_SUBST(EXTRA_CFLAGS)
AC_SUBST(EXTRA_LIBS)
AC_SUBST(PLATFORM)

############################
### Produce output files ###
############################
AC_OUTPUT(
	  Makefile \
	  src/Makefile
	  po/Makefile.in
)

if test "$PLATFORM" = "maemo"; then
   AC_OUTPUT(data/vagalume.conf data/vagalume-rfs.sh)
fi

######################
### Status summary ###
######################
echo ""
echo "****************************************"
echo "*** Vagalume is ready to be compiled ***"
echo "****************************************"
echo ""
echo "Target platform: $PLATFORM $PLATFORMVERS"
echo "Method to compute MD5 sums: $md5_method"
echo "\"Set IM status\" enabled: $set_im_status"
if test "$PLATFORM" = "gnome"; then
   echo "Tray icon and notifications enabled: $have_tray_icon"
elif test "$PLATFORM" = "maemo"; then
   echo "Maemo status bar plugin enabled: $have_sb_plugin"
   echo "Libconic support: $have_conic"
fi
echo "D-Bus Support enabled:" $have_dbus_glib
echo ""
echo "Type 'make' to compile Vagalume"
echo ""
