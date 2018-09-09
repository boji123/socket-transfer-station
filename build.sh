#g++ -c server.cpp
#g++ server.cpp -L. -lmsc -o server
g++ server.cpp -o server include/libmsc.so -std=c++11
g++ client.cpp -o client
export LD_LIBRARY_PATH=$(pwd)

rm -rf pack
mkdir pack
echo 'export LD_LIBRARY_PATH=$(pwd)' > pack/run.sh
echo './server' >> pack/run.sh
chmod 700 pack/run.sh
cp include/libmsc.so pack/
cp server pack/

#echo "starting server..."
#./server
