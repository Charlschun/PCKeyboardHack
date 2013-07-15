#!/bin/sh

PATH=/bin:/sbin:/usr/bin:/usr/sbin; export PATH

version=$(cat version)

packagemaker=/Applications/Utilities/PackageMaker.app/Contents/MacOS/PackageMaker
pkgName="PCKeyboardHack.pkg"
archiveName="PCKeyboardHack-${version}"

make clean build || exit $?

# --------------------------------------------------
# http://developer.apple.com/documentation/Darwin/Conceptual/KEXTConcept/KEXTConceptPackaging/packaging_kext.html
echo "Copy Files"

rm -rf pkgroot
mkdir -p pkgroot

mkdir -p "pkgroot/Applications"
cp -R "src/core/server/build/Release/PCKeyboardHack.app" "pkgroot/Applications"

basedir="pkgroot/Applications/PCKeyboardHack.app/Contents/Applications"
mkdir -p "$basedir"
for d in \
    src/util/uninstaller/automator/PCKeyboardHackUninstaller.app \
    ;
do
    cp -R "$d" "$basedir"
done

basedir="pkgroot/Applications/PCKeyboardHack.app/Contents/Library"
mkdir -p "$basedir"
for ostype in 10.7 10.8; do
    cp -R src/core/kext/${ostype}/build/Release/PCKeyboardHack.kext "$basedir/PCKeyboardHack.${ostype}.kext"
done
for d in \
    files/scripts \
    ;
do
    cp -R "$d" "$basedir"
done

basedir="pkgroot/Applications/PCKeyboardHack.app/Contents/Library/extra"
mkdir -p "$basedir"
cp -R pkginfo/Resources/preflight "$basedir/uninstall_core.sh"
for f in \
    files/extra/launchUninstaller.sh \
    files/extra/setpermissions.sh \
    files/extra/uninstall.sh \
    ;
do
    cp -R "$f" "$basedir"
done

basedir="pkgroot/Library"
mkdir -p "$basedir"
for d in \
    files/LaunchDaemons \
    files/LaunchAgents \
    ;
do
    cp -R "$d" "$basedir"
done

# Setting file permissions.
#
# Note:
#   If target files are already exists in system disk,
#   PackageMaker uses their permissions.
#
#   For example:
#     If /Applications/PCKeyboardHack.app permission is 0777 by accidental reasons,
#     the directory permission will be 0777 in Archive.bom
#     even if we set this directory permission to 0755 by setpermissions.sh.
#
#   Then, we need to repair file permissions in postflight script.
#   Please also see postflight.
#
sh "files/extra/setpermissions.sh" pkgroot
sh "files/extra/setpermissions.sh" pkginfo
chmod 755 \
    pkginfo/Resources/InstallationCheck \
    pkginfo/Resources/postflight \
    pkginfo/Resources/preflight \
    pkginfo/fixbom.rb

# --------------------------------------------------
echo "Exec PackageMaker"

rm -rf $archiveName/$pkgName
mkdir $archiveName

# Note: Don't add --no-recommend option.
# It might break /Library permission.
# (It overwrites /Library permission with pkgroot/Library permission.)
# - Mac OS X 10.6: /Library is 1775
# - Mac OS X 10.7: /Library is 0755
# - Mac OS X 10.8: /Library is 40755
$packagemaker \
    --root pkgroot \
    --info pkginfo/Info.plist \
    --resources pkginfo/Resources \
    --title "PCKeyboardHack $version" \
    --no-relocate \
    --discard-forks \
    --out $archiveName/$pkgName

# --------------------------------------------------
echo "Fix Archive.bom"
ruby pkginfo/fixbom.rb $archiveName/$pkgName/Contents/Archive.bom pkgroot/

# --------------------------------------------------
echo "Make Archive"

# Note:
# Some third vendor archiver fails to extract zip archive.
# Therefore, we use dmg instead of zip.

rm -f $archiveName.dmg
hdiutil create -nospotlight $archiveName.dmg -srcfolder $archiveName
rm -rf $archiveName
chmod 644 $archiveName.dmg
