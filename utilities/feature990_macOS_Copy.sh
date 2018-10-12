#!/bin/bash
# Simple installation script created to copy from the <install> compiled folder to one
# that can be .tar.gz-ed easily.
# Adjusted for macOS compatibility

#Make sure the built_copies and sub-folder exist, in case we've moved this
if [ -d "../built_copies/macOS" ]; then
    echo;
else
    echo "Creating built_copies folder"
    mkdir -p ../built_copies/macOS
fi

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
cp -f ../install64/bin/gridlabd ../package_create
cp -f ../install64/lib/gridlabd/*.* ../package_create
cp -f ../install64/share/gridlabd/tzinfo.txt  ../package_create
cp -f ../install64/share/gridlabd/unitfile.txt ../package_create
cp -f ../utilities/gridlabd_run.sh ../package_create

#Tar-it-up - do it manually
cd ../package_create
tar -cz --exclude=*.tar.gz -f Mac_4p1_Optimize_build.tar.gz *

#While we're here, generate the versions.txt
echo `date '+%B %d, %Y'` > version_dates.txt
echo "Built on " `uname -rv`  >> version_dates.txt
./gridlabd --version >> version_dates.txt

#Now copy this up to the release versions folder
cp -f ../package_create/Mac_4p1_Optimize_build.tar.gz ../built_copies/macOS
cp -f ../package_create/version_dates.txt ../built_copies/macOS

