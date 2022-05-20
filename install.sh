set -x

source ./build.sh install
cd ../$BUILD_TYPE-install-cpp11
sudo rm -rf /usr/include/mymuduo
sudo cp -r include/mymuduo/ /usr/include/mymuduo/
tree /usr/include/mymuduo
sudo cp -f lib/* /usr/local/lib/
sudo ls -l /usr/local/lib/*mymuduo*