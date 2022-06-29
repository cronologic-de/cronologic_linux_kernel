printf " 
_______________________________________________________________________________
Overview:

This file is used on development environment to save time writing down all 
commands to build project on Windows/Linux.

All steps and values should be aligned with the build instructions 
mentioned in the file(s) 
https://git.cronologic.de/soft/z-group/-/wikis/Build-Project-on-Windows.

Support 'x64_32' configuration is autmatically applied if 
'CRONO_64_32_SUPPORTED' is found in 'CMakeLists.txt'.

flags:
======
   -c  Build Conan packages and upload them to local cache.
       Values: N for No, otherwise it is assumed as Yes.
   -v  Valid only for Visual Studio generator on Windows. 
       Values are: 14, 17, ALL. Choosing ALL will build for both 14 and 17. 
   -p  Only create packages using Conan.
   -b  Create '-bin' package, after building the project.
   -h  Display this help.
_______________________________________________________________________________
"
# _____________________________________________________________________________
# Initialziations
#
BASEDIR=$(dirname "$0") # All paths should be set relative to base directory in case
                        # the batch is called from another directory

windows_types="msys cygwin win32"
[[ $windows_types =~ (^|[[:space:]])$OSTYPE($|[[:space:]]) ]] && CRONO_OS="windows" || CRONO_OS="linux"

# _____________________________________________________________________________
# Parse arguments
#
while getopts "c:v:xpbh" flag
do
    case "${flag}" in
        c) 
            PUB_PKG=${OPTARG}
            if [ "$PUB_PKG" = "N" ]; then 
                printf "Crono: Publishing packages is OFF.\n"
            else
                printf "Crono: Publishing packages is ON.\n"
            fi
            ;;
        v) 
            CONAN_COMP_VER=${OPTARG}
            printf "Crono: Using Visual Studio compiler version $CONAN_COMP_VER.\n"
            CONAN_COMP="Visual Studio"
            ;; 
        p) 
            conan_create_only=true
            printf "Crono: Using \'conan create\' to create packages.\n"
            ;; 
        b) 
            build_bin_pkg=true
            if [ ! -d ./bin_pkg ]; then
                printf "Crono: Error: No binary package folder found.\n"
                sleep 10s
                exit 0
            fi
            printf "Crono: Creating binary packages.\n"
            ;; 
        h) 
            sleep 10s
            exit 0
            ;;
    esac
done

# _____________________________________________________________________________
# $1: Output directory
# $2: CMake configuration flags
# $3: CMake build flags
# $4: Compiler version
build_config() {
    printf "\nCrono: Cleaning Up CMake Build Directories If Found...\n" 
    if [ -d $1 ]; then
        rm -rf "$1"/*
    fi

    printf "\nCrono: Building Project Buildsystem ...\n"
    if [ "$4" = "14" ]; then 
        cmake -B $1 -S $BASEDIR $2 -G """Visual Studio 14 2015""" -DCRONO_PUBLISH_LOCAL_PKG=$PUB_PKG
    elif [ "$4" = "17" ]; then 
        cmake -B $1 -S $BASEDIR $2 -G """Visual Studio 17 2022""" -DCRONO_PUBLISH_LOCAL_PKG=$PUB_PKG
    else
        cmake -B $1 -S $BASEDIR $2 -DCRONO_PUBLISH_LOCAL_PKG=$PUB_PKG
    fi

    printf "\nCrono: Building Project ..."
    cmake --build $1 $3
}

# _____________________________________________________________________________
# $1: Error code
print_err_exit() {
    printf "Crono: error code <%s> is encountered from last command\n" "$1"

    printf "Crono: Press any key to exit\n"
    # Wait for the user’s input in every 10 seconds and if the user doesn’t 
    # press any key then it will print the message
    while [ true ] ; do
        read -t 10 -n 1
        if [ $? = 0 ] ; then
            exit ;
        else
            printf "Crono: Waiting for the keypress\n";
        fi
    done
    exit $1
}

# _____________________________________________________________________________
# $1: conan `arch`
# $2: conan `build_type`
# $3: conan `compiler`
# $4: conan `compiler.verion`
crono_create_pkg() {
    printf "Crono: creating package ...\n"

    args=()
    args+=( "create" )
    args+=( '.' )

    # Add settings to conan `conan create` command line, 
    # ONLY that have values in function arguments
    if [[ $1 != "" ]]; then args+=( '-s' ); args+=( arch=$1 ); fi
    if [[ $2 != "" ]]; then args+=( '-s' ); args+=( build_type=$2 ); fi
    if [[ $3 != "" ]]; then args+=( '-s' ); args+=( compiler="""$3""" ); fi
    if [[ $4 != "" ]]; then args+=( '-s' ); args+=( compiler.version=$4 ); fi

    conan "${args[@]}"
    retVal=$?; if [ ! "$retVal" = "0" ]; then print_err_exit "$retVal"; fi
}

# _____________________________________________________________________________
# $1: conan `arch`
# $2: conan `build_type`
# $3: conan `compiler`
# $4: conan `compiler.verion`
# $5: conan build_folder
crono_export_bin_pkg() {
    printf "Crono: exporting bin package ...\n"

    args=()
    args+=( "export-pkg" )
    args+=( './bin_pkg' )
    args+=( '-f' )
    
    # Add settings to conan `conan create` command line, 
    # ONLY that have values in function arguments
    if [[ $1 != "" ]]; then args+=( '-s' ); args+=( arch=$1 ); fi
    if [[ $2 != "" ]]; then args+=( '-s' ); args+=( build_type=$2 ); fi
    if [[ $3 != "" ]]; then args+=( '-s' ); args+=( compiler="""$3""" ); fi
    if [[ $4 != "" ]]; then args+=( '-s' ); args+=( compiler.version=$4 ); fi
    if [[ $5 != "" ]]; then args+=( '-bf' ); args+=( $5 ); fi

    conan "${args[@]}"
    retVal=$?; if [ ! "$retVal" = "0" ]; then print_err_exit "$retVal"; fi
}

# _____________________________________________________________________________
# $1: compiler
# $2: compiler version
build_windows() {
    printf "Crono: Building For Windows ...\n"
    
    # Build x86_64 ____________________________________________________________
    if [ "$conan_create_only" = true ]; then
        # Run conan create . only  ____________________________________________
        crono_create_pkg "x86_64" "Debug"     "$1" "$2"
        crono_create_pkg "x86_64" "Release"   "$1" "$2"
    else
        # Build using CMake  __________________________________________________
        build_config "$BASEDIR/../build/bfD" "-A x64" "--config Debug" "$2"
        build_config "$BASEDIR/../build/bfR" "-A x64" "--config Release" "$2"
        
        if [ "$build_bin_pkg" = true ]; then
            # Build -bin packages _____________________________________________
            crono_export_bin_pkg "x86_64" "Debug"   "$1" "$2" "$BASEDIR/../build/bfD"
            crono_export_bin_pkg "x86_64" "Release" "$1" "$2" "$BASEDIR/../build/bfR"
        fi
    fi
    
    # Build x86 _______________________________________________________________
    if [ "$conan_create_only" = true ]; then
        # Run conan create . only  ____________________________________________
        crono_create_pkg "x86" "Debug"     "$1" "$2"
        crono_create_pkg "x86" "Release"   "$1" "$2"
    else
        # Build using CMake  __________________________________________________
        build_config "$BASEDIR/../build/bf32D" "-A Win32" "--config Debug" "$2"
        build_config "$BASEDIR/../build/bf32R" "-A Win32" "--config Release" "$2"

        if [ "$build_bin_pkg" = true ]; then
            crono_export_bin_pkg "x86" "Debug"   "$1" "$2" "$BASEDIR/../build/bf32D"
            crono_export_bin_pkg "x86" "Release" "$1" "$2" "$BASEDIR/../build/bf32R"
        fi
    fi

    # Check x64_32 is supported  ______________________________________________
    if      grep -Fq "CRONO_64_32_SUPPORTED" CMakeLists.txt; 
    then
        if [ "$conan_create_only" = true ]; then
            # Run conan create . only  ________________________________________
            crono_create_pkg "x86" "Debug64_32"     "$1" "$2"
            crono_create_pkg "x86" "Release64_32"   "$1" "$2"
        else
            # Build using CMake  ______________________________________________
            build_config "$BASEDIR/../build/bf64_32D" "-A Win32" "--config Debug64_32" "$2"
            build_config "$BASEDIR/../build/bf64_32R" "-A Win32" "--config Release64_32" "$2"

            if [ "$build_bin_pkg" = true ]; then
                # Build -bin packages _________________________________________
                crono_export_bin_pkg "x86" "Debug64_32"                         \
                    "$1" "$2" "$BASEDIR/../build/bf64_32D"
                crono_export_bin_pkg "x86" "Release64_32"                       \
                    "$1" "$2" "$BASEDIR/../build/bf64_32R"
            fi
        fi
    fi
}

# =============================================================================
#####                             Main Logic                             ######
# =============================================================================
if [ "$CRONO_OS" = "windows" ]; then
    if [ "$CONAN_COMP_VER" = "14" ]; then 
        build_windows "$CONAN_COMP" "$CONAN_COMP_VER"
    elif [ "$CONAN_COMP_VER" = "17" ]; then 
        build_windows "$CONAN_COMP" "$CONAN_COMP_VER"
    elif [ "$CONAN_COMP_VER" = "ALL" ]; then 
        printf "Crono: WARNING - Removing dep_pkg folder to guarantee fetching the needed compiler's package...\n"
        [ -d "../dep_pkg" ] && rm -d -r ../dep_pkg --verbose
        build_windows "$CONAN_COMP" "14"
        [ -d "../dep_pkg" ] && rm -d -r ../dep_pkg --verbose
        build_windows "$CONAN_COMP" "17"
        [ -d "../dep_pkg" ] && rm -d -r ../dep_pkg --verbose
    else
    # Run with default compiler settings
        build_windows "" ""
    fi
else
    # Build Linux _____________________________________________________________
    printf "Crono: Building For Linux ...\n"
    if [ "$conan_create_only" = true ]; then
        # Run conan create . only  ____________________________________________
        crono_create_pkg "x86_64" "Debug"     "$CONAN_COMP" "$CONAN_COMP_VER"
        crono_create_pkg "x86_64" "Release"   "$CONAN_COMP" "$CONAN_COMP_VER"
    else
        build_config "$BASEDIR/../build/bfD" "-DCMAKE_BUILD_TYPE=Debug" "" ""
        build_config "$BASEDIR/../build/bfR" "-DCMAKE_BUILD_TYPE=Release" "" ""
    fi
fi

printf "\nCrono: SUCCESS, ALL IS DONE\n"
if [ "$CRONO_OS" = "windows" ]; then
    printf "Exiting in 10 seconds ...\n"
    sleep 10s
fi
