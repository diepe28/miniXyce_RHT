#!/bin/bash

# https://github.com/andikleen/pmu-tools
#Simple script to upload a new version of miniXyce-RHT to Grid 5K and run them
#chmod +x executable, to give permission

#Example of use: ./myScriptMiniXyce.sh miniXyce_RHT        

#rm -r -f nancy/public/HPCCG-RHT-Clean/
#rm nancy/public/HPCCG-RHT-Clean.tar.gz 
#rm -r -f lille/public/HPCCG-RH T-Clean/
#rm lille/public/HPCCG-RHT-Clean.tar.gz 

folder="$1"
newFolder="$1"-Clean

echo Folder Name: $folder $newFolder

cp -a $folder/ ./$newFolder
rm -rf $newFolder/.git
rm -rf $newFolder/.idea

#copying cirBig.net and cirHuge to tests2
mkdir $newFolder/cmake-build-debug-2
mkdir $newFolder/tests2
cp -a $newFolder/cmake-build-debug/tests/cirBig.net ./$newFolder/tests2
cp -a $newFolder/cmake-build-debug/tests/cirHugeSmall.net ./$newFolder/tests2
cp -a $newFolder/cmake-build-debug/tests/cirHuge.net ./$newFolder/tests2

#copying defaultParams.txt and lastUsedParams.txt to cmake-build-debug-2
cp -a $newFolder/cmake-build-debug/default_params.txt ./$newFolder/cmake-build-debug-2
cp -a $newFolder/cmake-build-debug/last_used_params.txt ./$newFolder/cmake-build-debug-2
cp -a $newFolder/runEcotype.sh ./$newFolder/cmake-build-debug-2
rm -rf $newFolder/cmake-build-debug/
rm -rf $newFolder/tests/

mv $newFolder/cmake-build-debug-2/ $newFolder/cmake-build-debug/
mv $newFolder/tests2/ $newFolder/cmake-build-debug/tests

# creating temp dir and moving everything there with the correct name (e.i original one)
mkdir ./aTestFolderToRemove
mv $newFolder aTestFolderToRemove/$folder
cd aTestFolderToRemove

tar -czvf $folder.tar.gz $folder

cd ../
mv aTestFolderToRemove/$folder.tar.gz $folder.tar.gz

rm -r -f aTestFolderToRemove

echo "zip file created"


#echo "Copying files to Nancy..."
#scp $folder.tar.gz dperez@access.grid5000.fr:nancy/public
echo "Copying files to Nantes..."
scp $folder.tar.gz dperez@access.grid5000.fr:nantes/public
echo "Copying files to Lyon..."
scp $folder.tar.gz dperez@access.grid5000.fr:lyon/public
echo "Files copied to Grid5K Storage"
echo "Removing zip file"
rm $folder.tar.gz
echo "Success!!"

# Removing previously extracted folders in nantes, nancy and lyon
#rm -rf nantes/public/miniXyce_RHT/ && rm -rf nancy/public/miniXyce_RHT/ && rm -rf lyon/public/miniXyce_RHT

# Inside a node (if tar.gz was copied into public/ with correct file structure)
#cd public/ && tar -xzvf miniXyce_RHT.tar.gz && rm miniXyce_RHT.tar.gz && cd miniXyce_RHT/cmake-build-debug/ && cmake .. && make

#nancy
#oarsub -p "cluster='graphite'" -I -l nodes=1,walltime=5

#nantes
#oarsub -p "cluster='econome'" -I -l nodes=1,walltime=5

#nantes
#oarsub -p "cluster='ecotype'" -I -l nodes=1,walltime=5

#lyon
#oarsub -p "cluster='nova'" -I -l nodes=1,walltime=5

#oarsub -p "cluster='ecotype'" -l nodes=1,walltime=4 "/home/dperez/public/miniXyce_RHT/cmake-build-debug/runEcotype.sh"

#ssh dperez@access.grid5000.fr
