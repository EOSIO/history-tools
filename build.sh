#/bin/bash

set -o errtrace
set -o nounset
set -o pipefail
trap 'printf "$_ at line $LINENO\\n" >&2; ERR_TRAPPED=1; LOGS=$(ls *.log 2>/dev/null); if [ "x$LOGS" != "x" ]; then [[ -f install_boost.log ]] && fuser -s -INT -k install_boost.log; [[ -f install_pqxx.log ]] && fuser -s -INT -k install_pqxx.log; [[ -f install_js.log ]] && fuser -s -INT -k install_js.log; printf "\\nError encountered during build. Build logs are captured in:\\n$LOGS\\n" >&2; printf "Consult the logs for detailed build information.\\n" >&2; printf "Build logs may be copied and pasted or attached to a Github issue.\\n" >&2; fi; exit $LINENO' ERR

TIME_BEGIN=$( date -u +%s )

CONSOLE=0
ERR_TRAPPED=0
KEEP=0
NONINTERACTIVE=0
CMAKE_BUILD_TYPE=Release
ENABLE_LMDB=0
ENABLE_PQXX=0
ENABLE_JS=0
ENABLE_NINJA=0

txtbld=$(tput bold)
bldred=${txtbld}$(tput setaf 1)
txtrst=$(tput sgr0)

function usage()
{
    printf "\\nUsage: %s \\n  -o        Build Option [<Debug|Release|RelWithDebInfo|MinSizeRel>]\\n  -j        Enable JavaScript [<Y|y|1|N|n|0>] (default: no)\\n  -k        Keep Logs and Downloads\\n  -l        Enable LMDB [<Y|y|1|N|n|0>] (default: no)\\n  -n        Enable Ninja [<Y|y|1|N|n|0>] (default: no)\\n  -p        Enable Postgresql  [<Y|y|1|N|n|0>] (default: no)\\n  -c        Console Friendly -- disables terminal UI\\n  -y        Noninteractive -- no prompts, installs everything\\n\\n" "$0" 1>&2
    exit 1
}

parse_enable() {
    case "$1" in
        1 | [Yy]* )
            eval "$2=1"
            return 0
        ;;
        0 | [Nn]* )
            eval "$2=0"
            return 0
        ;;
        * )
            printf "$3" 1>&2
            eval "$4"
            return 1
        ;;
    esac
}

if [ $# -ne 0 ]; then
    while getopts "h?cj:kl:n:o:p:y" opt; do
        case "${opt}" in
            c )
                CONSOLE=1
            ;;
            j)
                parse_enable $OPTARG ENABLE_JS "Unrecognized argument to -${opt}: $OPTARG\\n" usage
            ;;
            k )
                KEEP=1
            ;;
            l )
                parse_enable $OPTARG ENABLE_LMDB "Unrecognized argument to -${opt}: $OPTARG\\n" usage
            ;;
            n )
                parse_enable $OPTARG ENABLE_NINJA "Unrecognized argument to -${opt}: $OPTARG\\n" usage
            ;;
            o )
                options=( "Debug" "Release" "RelWithDebInfo" "MinSizeRel" )
                if [[ "${options[*]}" =~ "${OPTARG}" ]]; then
                    CMAKE_BUILD_TYPE="${OPTARG}"
                else
                    printf "\\nInvalid argument: %s\\n" "${OPTARG}" 1>&2
                    usage
                fi
            ;;
            p )
                parse_enable $OPTARG ENABLE_PQXX "Unrecognized argument to -${opt}: $OPTARG\\n" usage
            ;;
            y)
                NONINTERACTIVE=1
            ;;
            h|\? )
                usage
            ;;
            : )
                usage
            ;;
            * )
                usage
            ;;
        esac
    done
fi

graceful_exit() {
    if [[ $ERR_TRAPPED = 0 ]]; then
        [[ $KEEP = 0 && -f install_boost.log ]] && rm install_boost.log || :
        [[ $KEEP = 0 && -f install_pqxx.log ]] && rm install_pqxx.log || :
        [[ $KEEP = 0 && -f install_js.log ]] && rm install_js.log || :
        [[ $KEEP = 0 && -f install.log ]] && rm install.log || :
    fi
}

trap graceful_exit EXIT

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
SOURCE_ROOT="${SCRIPT_DIR}"
BUILD_DIR="${SOURCE_ROOT}/build/$(echo $CMAKE_BUILD_TYPE | tr [:upper:] [:lower:])"

export SRC_LOCATION=${HOME}/src
export OPT_LOCATION=${HOME}/opt
export BOOST_VERSION_MAJOR=1
export BOOST_VERSION_MINOR=67
export BOOST_VERSION_PATCH=0
export BOOST_VERSION=${BOOST_VERSION_MAJOR}_${BOOST_VERSION_MINOR}_${BOOST_VERSION_PATCH}
export BOOST_VERSION_PRINT=${BOOST_VERSION_MAJOR}.${BOOST_VERSION_MINOR}.${BOOST_VERSION_PATCH}
export BOOST_ROOT=${SRC_LOCATION}/boost_${BOOST_VERSION}
export BOOST_LINK_LOCATION=${OPT_LOCATION}/boost
export PQXX_VERSION_MAJOR=6
export PQXX_VERSION_MINOR=2
export PQXX_VERSION_PATCH=5
export PQXX_VERSION=${PQXX_VERSION_MAJOR}.${PQXX_VERSION_MINOR}.${PQXX_VERSION_PATCH}
export PQXX_ROOT=${SRC_LOCATION}/libpqxx
export PQXX_LINK_LOCATION=${OPT_LOCATION}/libpqxx
export FIREFOX_VERSION_MAJOR=64
export FIREFOX_VERSION_MINOR=0
export FIREFOX_VERSION=${FIREFOX_VERSION_MAJOR}.${FIREFOX_VERSION_MINOR}
export FIREFOX_ROOT=${SRC_LOCATION}/firefox
export FIREFOX_LINK_LOCATION=${OPT_LOCATION}/firefox

MEM_MEG=$( free -m | sed -n 2p | tr -s ' ' | cut -d\  -f2 || cut -d' ' -f2 )
CPU_CORE=$( nproc )
MEM_GIG=$(( ((MEM_MEG / 1000) / 2) ))
export JOBS=$(( MEM_GIG > CPU_CORE ? CPU_CORE : MEM_GIG ))

SIZE=$(stty size)
HEIGHT=$((${SIZE% *} - 4))
WIDTH=$((${SIZE#* } - 4))
DIALOG=$(which dialog) || true
LOGFD=3

if [[ "x${DIALOG}" = "x" ]]; then CONSOLE=1; fi

DEP_ARRAY=(
    build-essential cmake libboost-all-dev git libssl1.0-dev libgmp-dev pv
)

OPTIONAL_DEP_ARRAY=(
    ninja-build
)

PG_DEP_ARRAY=(
    libpq-dev libpqxx-dev
)

JS_BUILD_ARRAY=(
    autoconf2.13 rustc cargo clang-7 nodejs npm
)

JS_DEP_ARRAY=(
    libmozjs-64-dev
)

LMDB_DEP_ARRAY=(
    liblmdb-dev
)

exec 3<> install.log # cleaned up in exit function

mkdir -p $SRC_LOCATION
mkdir -p $OPT_LOCATION

ver() {
    local version=${1-}
    if [ -n "${version}" ]; then
        printf "%03d%03d%03d%03d" $(echo "$1" | tr '.' ' ');
    else
        printf ""
    fi
}

ask_requirements() {
    if [ $NONINTERACTIVE != 1 ]; then
        while read -p "Do you wish to enable Postgres support? (y/n) " RESPONSE; do
            if parse_enable $RESPONSE ENABLE_PQXX "Please type 'y' for yes or 'n' for no.\\n" ""; then
                break
            fi
        done
        while read -p "Do you wish to enable LMDB support? (y/n) " RESPONSE; do
            if parse_enable $RESPONSE ENABLE_LMDB "Please type 'y' for yes or 'n' for no.\\n" ""; then
                break
            fi
        done
        while read -p "Do you wish to enable Javascript support? (y/n) " RESPONSE; do
            if parse_enable $RESPONSE ENABLE_JS "Please type 'y' for yes or 'n' for no.\\n" ""; then
                break
            fi
        done
        while read -p "Do you wish to enable Ninja build support? (y/n) " RESPONSE; do
            if parse_enable $RESPONSE ENABLE_NINJA "Please type 'y' for yes or 'n' for no.\\n" ""; then
                break
            fi
        done
    fi
}

dialog_ask_requirements() {
    local LIBS=( $(dialog --backtitle "Build Configuration" --nocancel --no-tags \
            --checklist "Select desired library support:" 11 75 4 \
            PQXX "PostgreSQL" ${ENABLE_PQXX} \
            LMDB "Lightning Memory-mapped Database" ${ENABLE_LMDB} \
            JS "Javascript" ${ENABLE_JS} \
            NINJA "Ninja Build" ${ENABLE_NINJA} --output-fd 1) ) || true
    CANCELLED=${PIPESTATUS[0]}
    if [[ $CANCELLED = 1 ]]; then printf "\\nCancelled.  Exiting.\\n"; exit 1; fi
    for lib in "${LIBS[@]}"; do
        eval "ENABLE_$lib=1"
    done
}

ubuntu_check_available_package() {
    local SEARCH=( $(apt-cache search $1 | cut -d ' ' -f 1) )
    local AVAIL_VER=( $(apt-cache show "${SEARCH[@]}" | grep Version | cut -d ' ' -f 2) )
    dpkg --compare-versions ${AVAIL_VER[0]} ge "$2"
}

ubuntu_check_dependencies() {
    local COUNT=1
    local DISPLAY=""
    local DEP=""
    local INSTALL_DEPS=$NONINTERACTIVE
    printf "\\nChecking for installed dependencies...\\n"
    for (( i=0; i<${#DEP_ARRAY[@]}; i++ )); do
        if [ $( dpkg -s "${DEP_ARRAY[$i]}" 2>/dev/null | grep Status | tr -s ' ' | cut -d\  -f4 ) ]; then
            printf " - Package %s found.\\n" "${DEP_ARRAY[$i]}"
            continue
        else
            DEP=$DEP" ${DEP_ARRAY[$i]} "
            DISPLAY="${DISPLAY}${COUNT}. ${DEP_ARRAY[$i]}\\n"
            printf " - Package %s${bldred} NOT${txtrst} found!\\n" "${DEP_ARRAY[$i]}"
            (( COUNT++ ))
        fi
    done
    if [ "${COUNT}" -gt 1 ]; then
        printf "\\nThe following dependencies are required to install\\nhistory-tools with the selected options:\\n"
        printf "${DISPLAY}\\n\\n"
        if [ $NONINTERACTIVE != 1 ]; then
            while read -p "Do you wish to install these packages? (y/n) " INSTALL; do
                if parse_enable $INSTALL INSTALL_DEPS "Please type 'y' for yes or 'n' for no.\\n" ""; then
                    break
                fi
            done
        fi
        if [ $INSTALL_DEPS = 1 ]; then
            printf "Installing packages with sudo.\\n"
            sudo apt-get -y install ${DEP}
        else
            printf "User aborted installation of required dependencies. Exiting.\\n"; exit
        fi
    else
        printf " - No required APT dependencies to install.\\n"
    fi
}

dprintf() {
    local FD="${1:-"1"}"
    local msg="${2:-""}"
    local prefix="${3:-""}"
    local suffix="${4:-"\\n"}"
    printf "$prefix$msg$suffix" | tee -a /dev/fd/${FD} >&${LOGFD}
    if [[ $CONSOLE = 0 ]]; then echo "XXX"; echo "0"; echo "$msg"; echo "XXX"; fi
}

install_boost() {
    local FD="${1-"1"}"
    printf "Checking Boost library installation...\\n" | tee -a /dev/fd/${FD} >&${LOGFD}
    BOOSTVERSION=$( grep "#define BOOST_VERSION " "/usr/include/boost/version.hpp" 2>/dev/null | tail -1 | tr -s ' ' | cut -d\  -f3 )
    if [ "${BOOSTVERSION}" != "${BOOST_VERSION_MAJOR}0${BOOST_VERSION_MINOR}0${BOOST_VERSION_PATCH}" ]; then
        if [ "x${BOOSTVERSION}" != "x" ]; then
            printf " - System installation of Boost $BOOSTVERSION does not match required version $BOOST_VERSION_PRINT.\\n" | tee -a /dev/fd/${FD} >&${LOGFD}
        else
            printf " - No system installation of Boost.\\n" | tee -a /dev/fd/${FD} >&${LOGFD}
        fi
        printf " - Checking for EOS.IO installed Boost ..." | tee -a /dev/fd/${FD} >&${LOGFD}
        BOOSTVERSION=$( grep "#define BOOST_VERSION " "$OPT_LOCATION/boost/boost/version.hpp" 2>/dev/null | tail -1 | tr -s ' ' | cut -d\  -f3 )
        if [ "${BOOSTVERSION}" != "${BOOST_VERSION_MAJOR}0${BOOST_VERSION_MINOR}0${BOOST_VERSION_PATCH}" ]; then
            if [ "x${BOOSTVERSION}" != "x" ]; then
                printf "no match.\\n" | tee -a /dev/fd/${FD} >&${LOGFD}
                printf " - Found EOS.IO installed Boost $BOOSTVERSION which does not match required version $BOOST_VERSION.\\n" | tee -a /dev/fd/${FD} >&${LOGFD}
            else
                printf "not found.\\n" | tee -a /dev/fd/${FD} >&${LOGFD}
            fi
            printf " - Installing Boost ${BOOST_VERSION_PRINT}...\\n" | tee -a /dev/fd/${FD} >&${LOGFD}
            FILENAME=boost_$BOOST_VERSION.tar.bz2
            pushd $SRC_LOCATION >/dev/null
            dprintf $FD "Downloading $FILENAME..." " - "
            [[ $FD != 1 ]] && WGET_DIALOG="2>&1 | stdbuf -o0 awk '/[.] +[0-9][0-9]?[0-9]?%/ { print substr(\$0,63,3) }'" || WGET_DIALOG=''
            eval wget --show-progress https://dl.bintray.com/boostorg/release/${BOOST_VERSION_MAJOR}.${BOOST_VERSION_MINOR}.${BOOST_VERSION_PATCH}/source/"$FILENAME" ${WGET_DIALOG}
            dprintf $FD "Extracting $FILENAME..." " - "
            [[ $FD = 1 ]] && PVARG='' || PVARG='-n'
            (pv -p $PVARG "$FILENAME" | tar xjf -) 2>&1
            dprintf $FD "Bootstrapping Boost build..." " - "
            pushd ${BOOST_ROOT} >/dev/null
            ./bootstrap.sh --prefix=$BOOST_ROOT 1>/dev/fd/${FD} 2>/dev/fd/${FD}
            dprintf $FD "Building Boost ${BOOST_VERSION_PRINT}..." " - "
            ./b2 -q -j"${JOBS}" install 1>/dev/fd/${FD} 2>/dev/fd/${FD}
            popd >/dev/null
            if [[ $KEEP = 0 ]]; then
                rm -f "${FILENAME}"
            fi
            rm -rf ${BOOST_LINK_LOCATION}
            ln -s $BOOST_ROOT $BOOST_LINK_LOCATION
            popd >/dev/null
            printf " - Boost library successfully installed to ${BOOST_ROOT}.  Symlink created at ${BOOST_LINK_LOCATION}.\\n" | tee -a /dev/fd/${FD} >&${LOGFD}
        else
            printf "found.\\n" | tee -a /dev/fd/${FD} >&${LOGFD}
            printf " - Using Boost ${BOOST_VERSION_PRINT} found at ${BOOST_ROOT}.\\n" | tee -a /dev/fd/${FD} >&${LOGFD}
        fi
    else
        printf "Using system installation of Boost ${BOOST_VERSION_PRINT}.\\n" | tee -a /dev/fd/${FD} >&${LOGFD}
    fi
    if [[ $FD -ne 1 ]]; then fuser -s -INT -k install_boost.log; fi
}

install_pqxx() {
    local FD="${1-"1"}"
    dprintf $FD "Checking pqxx library version..."
    PQXXVERSION=$( pkg-config --modversion libpqxx 2>/dev/null ) || true
    if [ $(ver ${PQXXVERSION}) -gt $(ver ${PQXX_VERSION}) ]; then
        printf " - Using system installation of pqxx ${PQXXVERSION}.\\n" | tee -a /dev/fd/${FD} >&${LOGFD}
    else
        if [[ "x${PQXXVERSION}" = "x" ]]; then
            printf " - pqxx not found.  Building...\\n" | tee -a /dev/fd/${FD} >&${LOGFD}
        else
            printf " - System installation of pqxx ${PQXXVERSION} does not meet minimum required version ${PQXX_VERSION}.\\n" | tee -a /dev/fd/${FD} >&${LOGFD}
        fi
        FILENAME=${PQXX_VERSION}.tar.gz
        pushd $SRC_LOCATION >/dev/null
        dprintf $FD "Downloading $FILENAME..." " - "
        [[ FD != 1 ]] && WGET_DIALOG="2>&1 | stdbuf -o0 awk '/[.] +[0-9][0-9]?[0-9]?%/ { print substr(\$0,63,3) }'" || WGET_DIALOG=''
        eval wget --show-progress https://github.com/jtv/libpqxx/archive/${FILENAME} ${WGET_DIALOG}
        dprintf $FD "Extracting $FILENAME..." " - "
        [[ $FD = 1 ]] && PVARG='' || PVARG='-n'
        (pv -p $PVARG "$FILENAME" | tar xzf -) 2>&1
        dprintf $FD "Building libpqxx ${PQXX_VERSION}" " - "
        pushd libpqxx-${PQXX_VERSION} >/dev/null
        ./configure --prefix=${OPT_LOCATION}/libqpxx-${PQXX_VERSION}
        if [ $ENABLE_NINJA = 1 ]; then
            ninja
        else
            make -j${JOBS}
        fi
        popd >/dev/null
        if [ $KEEP = 0 ]; then
            rm -f "${FILENAME}"
        fi
        popd >/dev/null
        printf " - Postgres library successfully installed.\\n" | tee -a /dev/fd/${FD} >&{LOGFD}
    fi
    if [[ $FD -ne 1 ]]; then fuser -s -INT -k install_pqxx.log; fi
}

install_js() {
    local FD="${1-"1"}"
    dprintf $FD "Building Javascript interpreter."
    FILENAME=firefox-${FIREFOX_VERSION}.source.tar.xz
    pushd $SRC_LOCATION >/dev/null
    if [ ! -f $FILENAME ]; then
        dprintf $FD "Downloading $FILENAME..." " - "
        [[ $FD != 1 ]] && WGET_DIALOG="2>&1 | stdbuf -o0 awk '/[.] +[0-9][0-9]?[0-9]?%/ { print substr(\$0,63,3) }'" || WGET_DIALOG=''
        eval wget --show-progress https://archive.mozilla.org/pub/firefox/releases/${FIREFOX_VERSION}/source/${FILENAME} ${WGET_DIALOG}
    fi
    if [ ! -d "firefox-64.0" ]; then
        dprintf $FD "Extracting $FILENAME..." " - "
        [[ $FD = 1 ]] && PVARG='' || PVARG='-n'
        (pv -p $PVARG "$FILENAME" | tar xJf -) 2>&1
    fi
    pushd "firefox-${FIREFOX_VERSION}/js/src" >/dev/null
    dprintf $FD "Building Spidermonkey ${FIREFOX_VERSION}..." " - "
    autoconf2.13
    mkdir -p build
    pushd build >/dev/null
    ../configure --disable-debug --enable-optimize --disable-jemalloc --disable-replace-malloc 1>/dev/fd/${FD} 2>/dev/fd/${FD}
    if [ $ENABLE_NINJA = 1 ]; then
        ninja
    else
        make -j${JOBS}
        if [[ ! -f /usr/local/lib/libjs_static.ajs || ! -f /usr/local/lib/libmozjs-64.so ]]; then
            sudo make install
        fi
    fi
    popd >/dev/null
    popd >/dev/null
    rm -f "${FILENAME}"
    dprintf $FD " - Spidermonkey library successfully installed."
    popd >/dev/null
    if [[ $FD -ne 1 ]]; then fuser -s -INT -k install_js.log; fi
}

if [ $CONSOLE = 1 ]; then
    ask_requirements
else
    dialog_ask_requirements
fi

if [ $ENABLE_PQXX = 1 ]; then
    if ubuntu_check_available_package libpqxx-dev 6.2.4; then
        DEP_ARRAY+=( "${PG_DEP_ARRAY[@]}" )
    fi
fi

if [ $ENABLE_LMDB = 1 ]; then
    if ubuntu_check_available_package liblmdb-dev 0.9.21; then
        DEP_ARRAY+=( "${LMDB_DEP_ARRAY[@]}" )
    fi
fi

if [ $ENABLE_NINJA = 1 ]; then
    if ubuntu_check_available_package ninja-build 1.8.0; then
        DEP_ARRAY+=( "${OPTIONAL_DEP_ARRAY[@]}" )
    fi
fi

if [ $ENABLE_JS = 1 ]; then
    if ubuntu_check_available_package "libmozjs dev" 64.0; then
        DEP_ARRAY+=( "${JS_DEP_ARRAY[@]}" )
    else
        DEP_ARRAY+=( "${JS_BUILD_ARRAY[@]}" )
    fi
fi

ubuntu_check_dependencies

if [[ $CONSOLE = 1 ]]; then
    install_boost 1
else
    [[ -f install_boost.log ]] && rm install_boost.log
    exec 4<> install_boost.log # cleaned up in exit function

    ! install_boost 4 |
      dialog --backtitle "Installing Boost" --input-fd 4 --begin 12 2 --tailboxbg "install_boost.log" 20 75 \
             --and-widget --begin 3 2 --gauge "Working..." 7 75
    [[ ${PIPESTATUS[0]} -ne 0 ]] && false "Error installing Boost"
    exec 4>&-
fi

if [[ $ERR_TRAPPED = 1 ]]; then exit; fi

printf "\\n" >&${LOGFD}

if [ $ENABLE_PQXX = 1 ]; then
    if [[ $CONSOLE = 1 ]]; then
        install_pqxx 1
    else
        [[ -f install_pqxx.log ]] && rm install_pqxx.log
        exec 5<> install_pqxx.log # cleaned up in exit function

        ! install_pqxx 5 |
          dialog --backtitle "Installing libpqxx" --input-fd 5 --begin 12 2 --tailboxbg "install_pqxx.log" 20 75 \
                 --and-widget --begin 3 2 --gauge "Working..." 7 75
        [[ ${PIPESTATUS[0]} -ne 0 ]] && false "Error installing libpqxx"
        exec 5>&-
    fi
fi

printf "\\n" >&${LOGFD}

if [ $ENABLE_JS = 1 ]; then
    if [[ $CONSOLE = 1 ]]; then
        install_js 1
    else
        [[ -f install_js.log ]] && rm install_js.log
        exec 4<> install_js.log # cleaned up in exit function

        ! install_js 4 |
          dialog --backtitle "Installing Javascript Interpreter" --input-fd 4 --begin 12 2 --tailboxbg "install_js.log" 20 75 \
                 --and-widget --begin 3 2 --gauge "Working..." 7 75
        [[ ${PIPESTATUS[0]} -ne 0 ]] && false "Error installing Javascript"
        exec 4>&-
    fi
fi

mkdir -p $BUILD_DIR
pushd $BUILD_DIR

if [ ENABLE_NINJA = 1 ]; then
    cmake -G "Ninja" "${SOURCE_ROOT}" -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
    ninja
else
    cmake -G "Unix Makefiles" "${SOURCE_ROOT}" -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
    make -j${JOBS}
fi

TIME_END=$(( $(date -u +%s) - $TIME_BEGIN ))
printf "\\nhistory-tools have been successfully built. %02d:%02d:%02d\\n" $(($TIME_END/3600)) $(($TIME_END%3600/60)) $(($TIME_END%60)) >&${LOGFD}

if [[ $CONSOLE = 0 ]]; then
    dialog --backtitle "Build Complete" --textbox "install.log" $HEIGHT $WIDTH
fi


[[ $LOGFD = 3 ]] # The word following redirection is subject to expansion.
                 # The word preceeding is not.  Change here and next line to
                 # match definition at the beginning of the file.
exec 3>&-
