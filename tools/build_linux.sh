echo _______________________________________________________________________________
echo Overview:
echo .
echo This file is used on development environment to save time writing down all 
echo commands to build project on Linux.
echo All steps and values should be aligned with the build instructions 
echo mentioned in the readme file 
echo https://github.com/cronologic-de/cronologic_linux_kernel/blob/main/README.md.
echo .
echo Output is found on ../build/bfD for Debug, and ../build/bfR for Release. 
echo The batch cleans up the folders and rebuilds the code with every run.
echo .
echo flags:
echo ======
echo    -c  Build Conan packages and upload them to local cache.
echo        Values: 'N' for No, otherwise it is assumed as Yes.
echo
echo Please review values under Custom Values section before you start.
echo _______________________________________________________________________________

BASEDIR=$(dirname "$0") # All paths should be set relative to base directory in case
                        #  the batch is called from another directory
DEBUG_BUILD_DIR="$BASEDIR/../build/bfD"
RELEASE_BUILD_DIR="$BASEDIR/../build/bfR"

while getopts c: flag
do
    case "${flag}" in
        c) PUBLISH_LOCAL_PKG=${OPTARG};;
    esac
done

echo _______________________________________________________________________________
echo Crono: Building x64 Project Buildsystem ...
echo -------------------------------------------------------------------------------
# Clean x64 debug build directory up if already there
if [ -d $DEBUG_BUILD_DIR ]; then
    rm -rf $DEBUG_BUILD_DIR/CMakeFiles
    rm -f $DEBUG_BUILD_DIR/*.*
fi
cmake -B $DEBUG_BUILD_DIR -S $BASEDIR -DCMAKE_BUILD_TYPE=Debug                   \
    -DCRONO_PUBLISH_LOCAL_PKG=$PUBLISH_LOCAL_PKG

# Clean x64 releaswe build directory up if already there
if [ -d $RELEASE_BUILD_DIR ]; then
    rm -rf $RELEASE_BUILD_DIR/CMakeFiles
    rm -f $RELEASE_BUILD_DIR/*.*
fi
cmake -B $RELEASE_BUILD_DIR  -S $BASEDIR -DCMAKE_BUILD_TYPE=Release              \
    -DCRONO_PUBLISH_LOCAL_PKG=$PUBLISH_LOCAL_PKG

echo _______________________________________________________________________________
echo Crono: Building x64 Project ...
echo -------------------------------------------------------------------------------
cmake --build $DEBUG_BUILD_DIR
cmake --build $RELEASE_BUILD_DIR
