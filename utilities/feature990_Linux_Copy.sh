#!/bin/bash
# Simple installation script created to copy from the <install> compiled folder to one
# that can be .tar.gz-ed easily.

#Create the folder (or purge it, if it exists)
if [ -d "../package_create" ]; then
	echo "package_create exists - purge"
	cd ../package_create
	rm -f *
else
	echo "package_create does not exist - create"
	mkdir -p ../package_create
fi

#Copy everything into the monolithic folder, just because
cp -f ../install64/bin/gridlabd.bin ../package_create
cp -f ../install64/lib/gridlabd/*.* ../package_create
cp -f ../install64/share/gridlabd/tzinfo.txt  ../package_create
cp -f ../install64/share/gridlabd/unitfile.txt ../package_create
cp -f ../utilities/gridlabd_gen_standalone ../package_create

#Tar-it-up - do it manually
cd ../package_create
tar -czf Linux_x64_990_build.tar.gz * --exclude=*.tar.gz

#While we're here, generate the versions.txt
echo `date '+%B %d, %Y'` > version_dates.txt
echo "Built on " `uname -mrvo`  >> version_dates.txt
./gridlabd.bin --version >> version_dates.txt

#Now copy this up to the release versions folder
cp -f ../package_create/Linux_x64_990_build.tar.gz ../built_copies/Linux
cp -f ../package_create/version_dates.txt ../built_copies/Linux

