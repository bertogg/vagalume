# Include modern UI
!include "MUI2.nsh"

#Language Selection Dialog Settings
#Remember the installer language
#!define MUI_LANGDLL_REGISTRY_ROOT "HKCU" 
#!define MUI_LANGDLL_REGISTRY_KEY "Software\vagalume" 
#!define MUI_LANGDLL_REGISTRY_VALUENAME "Installer Language"
	
# Language files
!insertmacro MUI_LANGUAGE "English"
!insertmacro MUI_LANGUAGE "Spanish"
!include "english.nsh"
!include "spanish.nsh"

###############################################################################
# Constants definition
!define SOURCE_DIR "../../prueba-0.7"
!define APP_NAME "Vagalume"
!define APP_LONGNAME "Vagalume Last.fm Player"
!define VERSION "0.7"
!define PLATFORM "win32"
!define MUI_FINISHPAGE_LINK_LOCATION "http://vagalume.igalia.com"
!define MUI_FINISHPAGE_LINK "http://vagalume.igalia.com"
!define GTK_NAME "GTK2 Runtime 2.12.11"
!define GTK_FILENAME "gtk2-runtime-2.12.11-2008-07-25-ash.exe"
###############################################################################

# MUI definitions
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "../Copying"
#!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_WELCOME
#!insertmacro MUI_UNPAGE_COMPONENTS
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_UNPAGE_FINISH

# redundancy check
CRCCheck on

# define installer name
Name "${APP_LONGNAME}"
Caption "$(Vgl-install)"
BrandingText "${APP_NAME} ${VERSION}"
OutFile "${APP_NAME}-${VERSION}-${PLATFORM}-full.exe"

# set desktop as install directory
InstallDir $PROGRAMFILES\vagalume

# Registry key to check for directory (so if you install again, 
# it will overwrite the old one automatically)
InstallDirRegKey HKLM SOFTWARE\vagalume "Install_Dir"

 
# default section start
section
       # Vagalume files 
       setOutPath $INSTDIR\bin
       File /r "${SOURCE_DIR}\bin\*.*"
       setOutPath $INSTDIR\share
       File /r "${SOURCE_DIR}\share\*.*"      
       
       # Gstreamer files
       setOutPath $COMMONFILES
       File /r "${SOURCE_DIR}\Gstreamer"
       
       # Add Gstreamer to path and set plugins path
       #readRegStr $0 HKLM "System\CurrentControlSet\Control\Session Manager\Environment" Path
       #writeRegStr HKLM "System\CurrentControlSet\Control\Session Manager\Environment" "Path" "$0;$COMMONFILES\GStreamer\0.10\bin"
       #writeRegStr HKLM "System\CurrentControlSet\Control\Session Manager\Environment" "GST_PLUGIN_PATH" "$COMMONFILES\GStreamer\0.10\lib\gstreamer-0.10"
       
       Push "PATH"
       Push "A"
       Push "HKLM"
       Push "$COMMONFILES\GStreamer\0.10\bin"
       Call EnvVarUpdate
       
       Push "GST_PLUGIN_PATH"
       Push "A"
       Push "HKLM"
       Push "$COMMONFILES\GStreamer\0.10\lib\gstreamer-0.10"
       Call EnvVarUpdate
        
       Push "GST_PLUGIN_PATH"
       Push "A"
       Push "HKCU"
       Push "$COMMONFILES\GStreamer\0.10\lib\gstreamer-0.10"
       Call EnvVarUpdate
        
       # Start menu folder and shortcut
       setOutPath $INSTDIR\bin
       createDirectory "$SMPROGRAMS\Vagalume"
       createShortCut "$SMPROGRAMS\Vagalume\vagalume.lnk" "$INSTDIR\bin\vagalume.exe"

       setOutPath $INSTDIR
       
       # Check for GTK installed version
       readRegStr $0 HKLM "SOFTWARE\GTK2-Runtime" Version
       readRegStr $2 HKLM "SOFTWARE\GTK2-Runtime" InstallationDirectory
	   StrCmp $0 "" 0 +2 
			StrCpy $0 "$(Gtk-no-version)"

       # GTK2 runtime installation:
       MessageBox MB_YESNO "$(GTK-install) ${GTK_NAME}? $(Gtk-v-detected): $0" /SD IDYES IDNO endGtk
       File "${SOURCE_DIR}\${GTK_FILENAME}"
       ExecWait "$INSTDIR\${GTK_FILENAME}"
       delete "$INSTDIR\${GTK_FILENAME}"

       endGtk:
 
       # Install hicolor theme for GTK (if not installed yet)
       MessageBox MB_YESNO "$(Gtk-hicolor)"  /SD IDYES IDNO endHiColor
       readRegStr $2 HKLM "SOFTWARE\GTK2-Runtime" InstallationDirectory
       IfFileExists $2\share\icons\hicolor\index.theme endHiColor installHiColor
       
       installHiColor:
       setOutPath "$2"
       createDirectory "share\icons\hicolor"
       setOutPath "$2\share\icons\hicolor" 
       File "${SOURCE_DIR}\index.theme"
       
       endHiColor:
                        
       # Create desktop icon:
       MessageBox MB_YESNO "$(Desktop-icon)" /SD IDYES IDNO endIcon
       setOutPath "$INSTDIR\bin"
       createShortCut "$DESKTOP\Vagalume.lnk" "$INSTDIR\bin\vagalume.exe"
       
       endIcon: 
                  
	   # Uninstall registry values
       WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\vagalume" "DisplayName" "${APP_LONGNAME} ${VERSION} (remove only)"
       WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\vagalume" "UninstallString" '"$INSTDIR\uninstall.exe"'
       WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\App Paths\vagalume.exe" "" '"$INSTDIR\bin\vagalume.exe"'

       # define uninstaller name and uninstall icon
       writeUninstaller $INSTDIR\uninstall.exe
       createShortCut "$SMPROGRAMS\Vagalume\uninstall.lnk" "$INSTDIR\uninstall.exe"

 
# default section end
sectionEnd
 
# create a section to define what the uninstaller does.
# the section will always be named "Uninstall"
section "Uninstall"
 
        MessageBox MB_OKCANCEL "$(Uninstall-ok)" IDOK DoUnInstall

        Abort "$(Uninstall-abort)"
        
        DoUnInstall:

        # Remove registry keys
        DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\vagalume"
        DeleteRegKey HKLM SOFTWARE\vagalume
 
        # removing gstreamer environment variables
        Push "PATH"
        Push "R"
        Push "HKLM"
        Push "$COMMONFILES\GStreamer\0.10\bin"
        Call un.EnvVarUpdate
       
        Push "GST_PLUGIN_PATH"
        Push "R"
        Push "HKLM"
        Push "$COMMONFILES\GStreamer\0.10\lib\gstreamer-0.10"
        Call un.EnvVarUpdate
        
        Push "GST_PLUGIN_PATH"
        Push "R"
        Push "HKCU"
        Push "$COMMONFILES\GStreamer\0.10\lib\gstreamer-0.10"
        Call un.EnvVarUpdate
       
        # Delete vagalume files
        delete "$INSTDIR\uninstall.exe"
        delete "$DESKTOP\Vagalume.lnk"
        delete "$SMPROGRAMS\Vagalume\uninstall.lnk"
        delete "$SMPROGRAMS\Vagalume\vagalume.lnk"
        delete "$INSTDIR\bin\*.*"
        delete "$INSTDIR\share\applications\*.*"
        delete "$INSTDIR\share\icons\*.*"
        delete "$INSTDIR\share\man\*.*" 
        delete "$INSTDIR\share\pixmaps\*.*"
        delete "$INSTDIR\share\vagalume\*.*"
 
        MessageBox MB_OKCANCEL "$(Uninstall-gst)" IDCANCEL DoNothing
        RMdir /r "$COMMONFILES\Gstreamer\*.*"
 
        DoNothing:
 
        # remove start menu folder
        RMdir "$SMPROGRAMS\Vagalume"
        RMdir /r "$INSTDIR"
        
sectionEnd


#######################################################################
# Prompts language selection
Function .onInit
  !insertmacro MUI_LANGDLL_DISPLAY
FunctionEnd
# Automatically sets language during uninstalation
Function un.onInit
  !insertmacro MUI_UNGETLANGUAGE
FunctionEnd
########################################################################

########################################################################
# Function openLinkNewWindow                                           
# -------------------------------------------------------------------- 
# Usage:                                                               
# StrCpy $0 "URL"                                                      
# Call openLinkNewWindow                                               
#
# http://nsis.sourceforge.net/Open_link_in_new_browser_window
########################################################################

# Uses $0
Function openLinkNewWindow
  Push $3 
  Push $2
  Push $1
  Push $0
  ReadRegStr $0 HKCR "http\shell\open\command" ""
# Get browser path
    DetailPrint $0
  StrCpy $2 '"'
  StrCpy $1 $0 1
  StrCmp $1 $2 +2 # if path is not enclosed in " look for space as final char
    StrCpy $2 ' '
  StrCpy $3 1
  loop:
    StrCpy $1 $0 1 $3
    DetailPrint $1
    StrCmp $1 $2 found
    StrCmp $1 "" found
    IntOp $3 $3 + 1
    Goto loop
 
  found:
    StrCpy $1 $0 $3
    StrCmp $2 " " +2
      StrCpy $1 '$1"'
 
  Pop $0
  Exec '$1 $0'
  Pop $1
  Pop $2
  Pop $3
FunctionEnd



########################################################################
# Function {un.}EnvVarUpdateFunction                                   
# -------------------------------------------------------------------- 
# Usage:                                                              
#                                                                      
# Push "Env name" (Ex: "PATH", "HOME")                                 
# Push "Action" ("R": Removes, "P": Preppends, "A": Appends)           
# Push "HKLM" (Register key: "HKLM" (all users) or "HKCU"(current))    
# Push "Value"                                                         
# Call un.EnvVarUpdate (or Call EnvVarUpdate when installing)          
#                                                                      
# http://nsis.sourceforge.net/Environmental_Variables:_append%2C_prepend%2C_and_remove_entries
########################################################################

!ifndef ENVVARUPDATE_FUNCTION
!define ENVVARUPDATE_FUNCTION
!verbose push
!verbose 3
!include "LogicLib.nsh"
!include "WinMessages.NSH"
 
; ---------------------------------- Macro Definitions ----------------------------------------
!macro _EnvVarUpdateConstructor ResultVar EnvVarName Action Regloc PathString
  Push "${EnvVarName}"
  Push "${Action}"
  Push "${RegLoc}"
  Push "${PathString}"
    Call EnvVarUpdate
  Pop "${ResultVar}"
!macroend
!define EnvVarUpdate '!insertmacro "_EnvVarUpdateConstructor"'
 
!macro _unEnvVarUpdateConstructor ResultVar EnvVarName Action Regloc PathString
  Push "${EnvVarName}"
  Push "${Action}"
  Push "${RegLoc}"
  Push "${PathString}"
    Call un.EnvVarUpdate
  Pop "${ResultVar}"
!macroend
!define un.EnvVarUpdate '!insertmacro "_unEnvVarUpdateConstructor"'
 
!macro _StrTokConstructor ResultVar String Separators ResultPart SkipEmptyParts
  Push "${String}"
  Push "${Separators}"
  Push "${ResultPart}"
  Push "${SkipEmptyParts}"
   Call ${UN}StrTok
  Pop "${ResultVar}"
!macroend
!define StrTok '!insertmacro "_StrTokConstructor"'
 
!macro _StrContainsConstructor OUT NEEDLE HAYSTACK
  Push "${HAYSTACK}"
  Push "${NEEDLE}"
   Call ${UN}StrContains
  Pop "${OUT}"
!macroend
!define StrContains '!insertmacro "_StrContainsConstructor"'
 
!macro _strReplaceConstructor OUT NEEDLE NEEDLE2 HAYSTACK
  Push "${HAYSTACK}"
  Push "${NEEDLE}"
  Push "${NEEDLE2}"
   Call ${UN}StrReplace
  Pop "${OUT}"
!macroend
!define StrReplace '!insertmacro "_strReplaceConstructor"'
 
; ---------------------------------- Macro Definitions end-------------------------------------
 
;----------------------------------- EnvVarUpdate start----------------------------------------
!define hklm_all_users     'HKLM "SYSTEM\CurrentControlSet\Control\Session Manager\Environment"'
!define hkcu_current_user  'HKCU "Environment"'
 
!macro EnvVarUpdate UN
 
Function ${UN}EnvVarUpdate
 
  Push $0
  Exch 4
  Exch $1
  Exch 3
  Exch $2
  Exch 2
  Exch $3
  Exch
  Exch $4
  Push $5
  Push $6
  Push $7
  Push $8
  Push $9
  Push $R0
 
  /* After this point:
  -------------------------
     $0 = ResultVar     (returned)
     $1 = EnvVarName    (input)
     $2 = Action        (input)
     $3 = RegLoc        (input)
     $4 = PathString    (input)
     $5 = Orig EnvVar   (read from registry)
     $6 = Len of $0     (temp)
     $7 = tempstr1      (temp)
     $8 = Entry counter (temp)
     $9 = tempstr2      (temp)
     $R0 = tempChar     (temp)  */
 
  ; Step 1:  Read contents of EnvVarName from RegLoc
  ;
  ; Check for empty EnvVarName
  ${If} $1 == ""
    SetErrors
    DetailPrint "ERROR: EnvVarName is blank"
    Goto EnvVarUpdate_Restore_Vars
  ${EndIf}
 
  ; Check for valid Action
  ${If}    $2 != "A"
  ${AndIf} $2 != "P"
  ${AndIf} $2 != "R"
    SetErrors
    DetailPrint "ERROR: Invalid Action - must be A, P, or R"
    Goto EnvVarUpdate_Restore_Vars
  ${EndIf}
 
  ${If} $3 == HKLM
    ReadRegStr $5 ${hklm_all_users} $1     ; Get EnvVarName from all users into $5
  ${ElseIf} $3 == HKCU
    ReadRegStr $5 ${hkcu_current_user} $1  ; Read EnvVarName from current user into $5
  ${Else}
    SetErrors
    DetailPrint 'ERROR: Action is [$3] but must be "HKLM" or HKCU"'
    Goto EnvVarUpdate_Restore_Vars
  ${EndIf}
 
  ; Check for empty PathString
  ${If} $4 == ""
    SetErrors
    DetailPrint "ERROR: PathString is blank"
    Goto EnvVarUpdate_Restore_Vars
  ${EndIf}
 
  ; Make sure we've got some work to do
  ${If} $5 == ""
  ${AndIf} $2 == "R"
    SetErrors
    DetailPrint "$1 is empty - Nothing to remove"
    Goto EnvVarUpdate_Restore_Vars
  ${EndIf}
 
  ; Step 2: Scrub EnvVar
  ;
  StrCpy $0 $5                             ; Copy the contents to $0
  ; Remove spaces around semicolons (NOTE: spaces before the 1st entry or
  ; after the last one are not removed here but instead in Step 3)
  ${If} $0 != ""                           ; If EnvVar is not empty ...
    ${Do}
      ${StrContains} $7 " ;" $0
      ${If} $7 == ""
        ${ExitDo}
      ${EndIf}
      ${StrReplace} $0 " ;" ";" $0         ; Remove '<space>;'
    ${Loop}
    ${Do}
      ${StrContains} $7 "; " $0
      ${If} $7 == ""
        ${ExitDo}
      ${EndIf}
      ${StrReplace} $0 "; " ";" $0         ; Remove ';<space>'
    ${Loop}
    ${Do}
      ${StrContains} $7 ";;" $0
      ${If} $7 == ""
        ${ExitDo}
      ${EndIf}
      ${StrReplace} $0 ";;" ";" $0
    ${Loop}
 
    ; Remove a leading or trailing semicolon from EnvVar
    StrCpy  $7  $0 1 0
    ${If} $7 == ";"
      StrCpy $0  $0 "" 1                   ; Change ';<EnvVar>' to '<EnvVar>'
    ${EndIf}
    StrLen $6 $0
    IntOp $6 $6 - 1
    StrCpy $7  $0 1 $6
    ${If} $7 == ";"
     StrCpy $0  $0 $6                      ; Change ';<EnvVar>' to '<EnvVar>'
    ${EndIf}
    ; DetailPrint "Scrubbed $1: [$0]"      ; Uncomment to debug
  ${EndIf}
 
  /* Step 3. Remove all instances of the target path/string (even if "A" or "P")
     $6 = bool flag (1 = found and removed PathString)
     $7 = a string (e.g. path) delimited by semicolon(s)
     $8 = entry counter starting at 0
     $9 = copy of $0
     $R0 = tempChar      */
 
  ${If} $5 != ""                           ; If EnvVar is not empty ...
    StrCpy $9 $0
    StrCpy $0 ""
    StrCpy $8 0
    StrCpy $6 0
 
    ${Do}
      ${StrTok} $7 $9 ";" $8 "0"      ; $7 = next entry, $8 = entry counter
 
      ${If} $7 == ""                       ; If we've run out of entries,
        ${ExitDo}                          ;    were done
      ${EndIf}                             ;
 
      ; Remove leading and trailing spaces from this entry (critical step for Action=Remove)
      ${Do}
        StrCpy $R0  $7 1
        ${If} $R0 != " "
          ${ExitDo}
        ${EndIf}
        StrCpy $7   $7 "" 1                ;  Remove leading space
      ${Loop}
      ${Do}
        StrCpy $R0  $7 1 -1
        ${If} $R0 != " "
          ${ExitDo}
        ${EndIf}
        StrCpy $7   $7 -1                  ;  Remove trailing space
      ${Loop}
      ${If} $7 == $4                       ; If string matches, remove it by not appending it
        StrCpy $6 1                        ; Set 'found' flag
      ${ElseIf} $7 != $4                   ; If string does NOT match
      ${AndIf}  $0 == ""                   ;    and the 1st string being added to $0,
        StrCpy $0 $7                       ;    copy it to $0 without a prepended semicolon
      ${ElseIf} $7 != $4                   ; If string does NOT match
      ${AndIf}  $0 != ""                   ;    and this is NOT the 1st string to be added to $0,
        StrCpy $0 $0;$7                    ;    append path to $0 with a prepended semicolon
      ${EndIf}                             ;
 
      IntOp $8 $8 + 1                      ; Bump counter
    ${Loop}                                ; Check for duplicates until we run out of paths
  ${EndIf}
 
  ; Step 4:  Perform the requested Action
  ;
  ${If} $2 != "R"                          ; If Append or Prepend
    ${If} $6 == 1                          ; And if we found the target
      DetailPrint "Target is already present in $1. It will be removed and"
    ${EndIf}
    ${If} $0 == ""                         ; If EnvVar is (now) empty
      StrCpy $0 $4                         ;   just copy PathString to EnvVar
      ${If} $6 == 0                        ; If found flag is either 0
      ${OrIf} $6 == ""                     ; or blank (if EnvVarName is empty)
        DetailPrint "$1 was empty and has been updated with the target"
      ${EndIf}
    ${ElseIf} $2 == "A"                    ;  If Append (and EnvVar is not empty),
      StrCpy $0 $0;$4                      ;     append PathString
      ${If} $6 == 1
        DetailPrint "appended to $1"
      ${Else}
        DetailPrint "Target was appended to $1"
      ${EndIf}
    ${Else}                                ;  If Prepend (and EnvVar is not empty),
      StrCpy $0 $4;$0                      ;     prepend PathString
      ${If} $6 == 1
        DetailPrint "prepended to $1"
      ${Else}
        DetailPrint "Target was prepended to $1"
      ${EndIf}
    ${EndIf}
  ${Else}                                  ; If Action = Remove
    ${If} $6 == 1                          ;   and we found the target
      DetailPrint "Target was found and removed from $1"
    ${Else}
      DetailPrint "Target was NOT found in $1 (nothing to remove)"
    ${EndIf}
    ${If} $0 == ""
      DetailPrint "$1 is now empty"
    ${EndIf}
  ${EndIf}
 
  ; Step 5:  Update the registry at RegLoc with the updated EnvVar and announce the change
  ;
  ClearErrors
  ${If} $3  == HKLM
    WriteRegExpandStr ${hklm_all_users} $1 $0     ; Write it in all users section
  ${ElseIf} $3 == HKCU
    WriteRegExpandStr ${hkcu_current_user} $1 $0  ; Write it to current user section
  ${EndIf}
 
  IfErrors 0 +4
    MessageBox MB_OK|MB_ICONEXCLAMATION "Could not write updated $1 to $3"
    DetailPrint "Could not write updated $1 to $3"
    Goto EnvVarUpdate_Restore_Vars
 
  ; "Export" our change
  SendMessage ${HWND_BROADCAST} ${WM_WININICHANGE} 0 "STR:Environment" /TIMEOUT=5000
 
  EnvVarUpdate_Restore_Vars:
  ;
  ; Restore the user's variables and return ResultVar
  Pop $R0
  Pop $9
  Pop $8
  Pop $7
  Pop $6
  Pop $5
  Pop $4
  Pop $3
  Pop $2
  Pop $1
  Push $0  ; Push my $0 (ResultVar)
  Exch
  Pop $0   ; Restore his $0
 
FunctionEnd
 
!macroend   ; EnvVarUpdate UN
!insertmacro EnvVarUpdate ""
!insertmacro EnvVarUpdate "un."
;----------------------------------- EnvVarUpdate end----------------------------------------
 
 
;-------------------------------------- StrTok start ------------------------------------------
; Written by Diego Pedroso (deguix)
;
!macro StrTok UN
 
Function ${UN}StrTok
 
/*After this point:
  ------------------------------------------
  $0 = SkipEmptyParts (input)
  $1 = ResultPart (input)
  $2 = Separators (input)
  $3 = String (input)
  $4 = SeparatorsLen (temp)
  $5 = StrLen (temp)
  $6 = StartCharPos (temp)
  $7 = TempStr (temp)
  $8 = CurrentLoop
  $9 = CurrentSepChar
  $R0 = CurrentSepCharNum  */
 
  ;Get input from user
  Exch $0
  Exch
  Exch $1
  Exch
  Exch 2
  Exch $2
  Exch 2
  Exch 3
  Exch $3
  Exch 3
  Push $4
  Push $5
  Push $6
  Push $7
  Push $8
  Push $9
  Push $R0
 
  ;Parameter defaults
  ${IfThen} $2 == `` ${|} StrCpy $2 `|` ${|}
  ${IfThen} $1 == `` ${|} StrCpy $1 `L` ${|}
  ${IfThen} $0 == `` ${|} StrCpy $0 `0` ${|}
 
  ;Get "String" and "Separators" length
  StrLen $4 $2
  StrLen $5 $3
  ;Start "StartCharPos" and "ResultPart" counters
  StrCpy $6 0
  StrCpy $8 -1
 
  ;Loop until "ResultPart" is met, "Separators" is found or
  ;"String" reaches its end
  ResultPartLoop: ;"CurrentLoop" Loop
 
  ;Increase "CurrentLoop" counter
  IntOp $8 $8 + 1
 
  StrSearchLoop:
  ${Do} ;"String" Loop
    ;Remove everything before and after the searched part ("TempStr")
    StrCpy $7 $3 1 $6
 
    ;Verify if it's the "String" end
    ${If} $6 >= $5
      ;If "CurrentLoop" is what the user wants, remove the part
      ;after "TempStr" and itself and get out of here
      ${If} $8 == $1
      ${OrIf} $1 == `L`
        StrCpy $3 $3 $6
      ${Else} ;If not, empty "String" and get out of here
        StrCpy $3 ``
      ${EndIf}
      StrCpy $R0 `End`
      ${ExitDo}
    ${EndIf}
 
    ;Start "CurrentSepCharNum" counter (for "Separators" Loop)
    StrCpy $R0 0
 
    ${Do} ;"Separators" Loop
      ;Use one "Separators" character at a time
      ${If} $R0 <> 0
        StrCpy $9 $2 1 $R0
      ${Else}
        StrCpy $9 $2 1
      ${EndIf}
 
      ;Go to the next "String" char if it's "Separators" end
      ${IfThen} $R0 >= $4 ${|} ${ExitDo} ${|}
 
      ;Or, if "TempStr" equals "CurrentSepChar", then...
      ${If} $7 == $9
        StrCpy $7 $3 $6
 
        ;If "String" is empty because this result part doesn't
        ;contain data, verify if "SkipEmptyParts" is activated,
        ;so we don't return the output to user yet
 
        ${If} $7 == ``
        ${AndIf} $0 = 1 ;${TRUE}
          IntOp $6 $6 + 1
          StrCpy $3 $3 `` $6
          StrCpy $6 0
          Goto StrSearchLoop
        ${ElseIf} $8 == $1
          StrCpy $3 $3 $6
          StrCpy $R0 "End"
          ${ExitDo}
        ${EndIf} ;If not, go to the next result part
        IntOp $6 $6 + 1
        StrCpy $3 $3 `` $6
        StrCpy $6 0
        Goto ResultPartLoop
      ${EndIf}
 
      ;Increase "CurrentSepCharNum" counter
      IntOp $R0 $R0 + 1
    ${Loop}
    ${IfThen} $R0 == "End" ${|} ${ExitDo} ${|}
 
    ;Increase "StartCharPos" counter
    IntOp $6 $6 + 1
  ${Loop}
 
  /*After this point:
  ------------------------------------------
  $3 = ResultVar (output)*/
 
  ;Return output to user
  Pop $R0
  Pop $9
  Pop $8
  Pop $7
  Pop $6
  Pop $5
  Pop $4
  Pop $0
  Pop $1
  Pop $2
  Exch $3
FunctionEnd
 
!macroend ; StrTok UN
!insertmacro StrTok ""
!insertmacro StrTok "un."
 
;----------------------------------------- StrTok end -------------------------------------------
 
 
;---------------------------------------- StrContains start -------------------------------------
; StrContains
; This function does a case sensitive searches for an occurrence of a substring in a string.
; It returns the substring if it is found; otherwise, it returns null("").
; Usage: ${StrContains} "$result_var" "Needle" "Haystack"
; Written by kenglish_hi
; Adapted from StrReplace written by dandaman32
 
Var STR_HAYSTACK
Var STR_NEEDLE
Var STR_CONTAINS_VAR_1
Var STR_CONTAINS_VAR_2
Var STR_CONTAINS_VAR_3
Var STR_CONTAINS_VAR_4
Var STR_RETURN_VAR
 
!macro StrContains UN
 
Function ${UN}StrContains
 
  Exch $STR_NEEDLE
  Exch
  Exch $STR_HAYSTACK
  ; Uncomment to debug
  ;MessageBox MB_OK 'STR_NEEDLE = $STR_NEEDLE STR_HAYSTACK = $STR_HAYSTACK '
    StrCpy $STR_RETURN_VAR ""
    StrCpy $STR_CONTAINS_VAR_1 -1
    StrLen $STR_CONTAINS_VAR_2 $STR_NEEDLE
    StrLen $STR_CONTAINS_VAR_4 $STR_HAYSTACK
    loop:
      IntOp $STR_CONTAINS_VAR_1 $STR_CONTAINS_VAR_1 + 1
      StrCpy $STR_CONTAINS_VAR_3 $STR_HAYSTACK $STR_CONTAINS_VAR_2 $STR_CONTAINS_VAR_1
      StrCmp $STR_CONTAINS_VAR_3 $STR_NEEDLE found
      StrCmp $STR_CONTAINS_VAR_1 $STR_CONTAINS_VAR_4 done
      Goto loop
    found:
      StrCpy $STR_RETURN_VAR $STR_NEEDLE
      Goto done
    done:
   Pop  $STR_HAYSTACK       ;Prevent "invalid opcode" errors and keep the stack intact
   Exch $STR_RETURN_VAR
FunctionEnd
 
!macroend
!insertmacro StrContains ""
!insertmacro StrContains "un."
;--------------------------------------- StrContains end ---------------------------------------
 
 
;--------------------------------------- StrReplace start --------------------------------------
; NOTE: Do not substitute 'StrReplaceV4' for this function. It will fail due to the way I call it.
;
; StrReplace
; Replaces all occurences of a given needle within a haystack with another string
; Usage: ${StrReplace} "$result_var" "SubString" "RepString" "String"
; Written by dandaman32
 
Var STR_REPLACE_VAR_0
Var STR_REPLACE_VAR_1
Var STR_REPLACE_VAR_2
Var STR_REPLACE_VAR_3
Var STR_REPLACE_VAR_4
Var STR_REPLACE_VAR_5
Var STR_REPLACE_VAR_6
Var STR_REPLACE_VAR_7
Var STR_REPLACE_VAR_8
 
!macro StrReplace UN
 
Function ${UN}StrReplace
 
  Exch $STR_REPLACE_VAR_2
  Exch 1
  Exch $STR_REPLACE_VAR_1
  Exch 2
  Exch $STR_REPLACE_VAR_0
    StrCpy $STR_REPLACE_VAR_3 -1
    StrLen $STR_REPLACE_VAR_4 $STR_REPLACE_VAR_1
    StrLen $STR_REPLACE_VAR_6 $STR_REPLACE_VAR_0
    loop:
      IntOp $STR_REPLACE_VAR_3 $STR_REPLACE_VAR_3 + 1
      StrCpy $STR_REPLACE_VAR_5 $STR_REPLACE_VAR_0 $STR_REPLACE_VAR_4 $STR_REPLACE_VAR_3
      StrCmp $STR_REPLACE_VAR_5 $STR_REPLACE_VAR_1 found
      StrCmp $STR_REPLACE_VAR_3 $STR_REPLACE_VAR_6 done
      Goto loop
    found:
      StrCpy $STR_REPLACE_VAR_5 $STR_REPLACE_VAR_0 $STR_REPLACE_VAR_3
      IntOp $STR_REPLACE_VAR_8 $STR_REPLACE_VAR_3 + $STR_REPLACE_VAR_4
      StrCpy $STR_REPLACE_VAR_7 $STR_REPLACE_VAR_0 "" $STR_REPLACE_VAR_8
      StrCpy $STR_REPLACE_VAR_0 $STR_REPLACE_VAR_5$STR_REPLACE_VAR_2$STR_REPLACE_VAR_7
      StrLen $STR_REPLACE_VAR_6 $STR_REPLACE_VAR_0
      Goto loop
    done:
  Pop $STR_REPLACE_VAR_1 ; Prevent "invalid opcode" errors and keep the
  Pop $STR_REPLACE_VAR_1 ; stack as it was before the function was called
  Exch $STR_REPLACE_VAR_0
 
FunctionEnd
 
!macroend   ; StrContains UN
!insertmacro StrReplace ""
!insertmacro StrReplace "un."
 
;----------------------------------------- StrReplace end ---------------------------------------
 
!verbose pop
!endif
