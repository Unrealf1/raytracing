#/bin/bash

cd build
if make -j8
then
    cd ../bin
    ./raytracing
fi
cd ..

