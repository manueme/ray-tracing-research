cd ./external_sources || exit
if [[ -d ./assimp ]]
then
    echo Pulling Assimp...
    cd ./assimp || exit
    git pull
    cd ../..
else
    echo Cloning Assimp...
    git clone https://github.com/assimp/assimp.git
    cd ..
fi

SOURCE_DIR=./external_sources/assimp
echo Cleaning previous builds...
rm -f ${SOURCE_DIR}/CMakeCache.txt
rm -r ${SOURCE_DIR}/build

GENERATOR="Unix Makefiles"


BINARIES_DIR="${SOURCE_DIR}/build"
mkdir -p ${BINARIES_DIR}

echo Generating...
cmake CMakeLists.txt -G "${GENERATOR}" -S ${SOURCE_DIR} -B ${BINARIES_DIR}
echo Building Debug...
cmake --build ${BINARIES_DIR} --config debug
echo Building Release...
cmake --build ${BINARIES_DIR} --config release
