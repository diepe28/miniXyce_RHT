#!/bin/bash

#Simple script to upload a new version of miniXyce-RHT to Grid 5K and run them
#chmod +x executable, to give permission

#Example of use: ./uploadToGrid5K.sh
#It assumes is at the root of /miniXyce_RHT repo

folder="${PWD##*/}"
newFolder=$folder-Clean
cd ..
echo Folder Name: $folder $newFolder

cp -a $folder/ ./$newFolder
rm -rf $newFolder/.git
rm -rf $newFolder/.idea

#copying cirBig.net, cirHugeSmall, cirHuge to tests2
mkdir $newFolder/cmake-build-debug-2
mkdir $newFolder/tests2
cp -a $newFolder/cmake-build-debug/tests/cirBig.net ./$newFolder/tests2
cp -a $newFolder/cmake-build-debug/tests/cirHugeSmall.net ./$newFolder/tests2
cp -a $newFolder/cmake-build-debug/tests/cirHuge.net ./$newFolder/tests2

#copying defaultParams.txt and lastUsedParams.txt to cmake-build-debug-2
cp -a $newFolder/cmake-build-debug/default_params.txt ./$newFolder/cmake-build-debug-2
cp -a $newFolder/cmake-build-debug/last_used_params.txt ./$newFolder/cmake-build-debug-2
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

#ssh dperez@access.grid5000.fr

# Copying Files to Grid 5k machines
#echo "Copying files to Nancy..."
#scp $folder.tar.gz dperez@access.grid5000.fr:nancy/public
#echo "Copying files to Nantes..."
#scp $folder.tar.gz dperez@access.grid5000.fr:nantes/public
echo "Copying files to Lyon..."
scp $folder.tar.gz dperez@access.grid5000.fr:lyon/public
echo "Files copied to Grid5K Storage"
echo "Removing zip file"
rm $folder.tar.gz
echo "Success!!"

# Removing previously extracted folders (if any) in nantes, nancy and lyon
#rm -rf nantes/public/miniXyce_RHT/ && rm -rf nancy/public/miniXyce_RHT/ && rm -rf lyon/public/miniXyce_RHT

# Inside a node (if tar.gz was copied into public/ with correct file structure), this will compile everything
#cd public/ && tar -xzvf miniXyce_RHT.tar.gz && rm miniXyce_RHT.tar.gz && cd miniXyce_RHT/cmake-build-debug/ && cmake .. && make

# Asking for nodes, remember to kill job when disconected with oardel jobId

#nancy
#oarsub -p "cluster='graphite'" -I -l nodes=1,walltime=5

#nantes
#oarsub -p "cluster='econome'" -I -l nodes=1,walltime=5

#nantes
#oarsub -p "cluster='ecotype'" -I -l nodes=1,walltime=5

#lyon
#oarsub -p "cluster='nova'" -I -l nodes=1,walltime=5

# These are the commands for lyon/nova, 

#Lyon - Nova 0,16 2,18 4,20... are hyperThreads (lstopo result)
#Core0       Core1     Core2        Core3     Core4       Core5    Core6      Core7
#0,16        2,18      4,20         6,22      8,24        10,26    12,28      14,30

#Core8       Core9     Core10       Core11    Core12      Core13   Core14     Core15
#1,17        3,19      5,21         7,23      9,25        11,27    13,29      15,31

# this as the commands, must copy and paste 
if [ 1 -eq 0 ]; then
#After last "$PWD", the parameters are:
#numExecutions numThreads ListOfCoresWhereThreadPinning... Examples:

#5 2 0 2 =
#5 executions, 2 threads, Leading in core 0, Trailing in core 2
#5 32 0 16 2 18 4 20 6 22 8 24 10 26 12 28 14 30 1 17 3 19 5 21 7 23 9 25 11 27 13 29 15 31 = 
#5 executions, 32 threads, L1 core 0, T1 core 16, L2 core 2, T2 core 18, ..., L16 core 15, T16 core 31

# Not replicated app with different mpi ranks (to see HT results)
mpirun -np 1 miniXyceRHT -c "$PWD"/tests/cirHuge.net "$PWD" 5 > noRep-1rank.txt  &&
mpirun -np 16 miniXyceRHT -c "$PWD"/tests/cirHuge.net "$PWD" 5 > noRep-16ranks.txt      &&
mpirun -np 32 miniXyceRHT -c "$PWD"/tests/cirHuge.net "$PWD" 5 > noRep-32ranks.txt      

# 1 rank test baselines vs Our Best Approach (NewLimit + varGrouping 8)
mpirun -np 1 miniXyce-AC -c "$PWD"/tests/cirHuge.net "$PWD" 5 2 0 2 > rep-ac-1rank-noHT.txt  &&
mpirun -np 1 miniXyce-AC -c "$PWD"/tests/cirHuge.net "$PWD" 5 2 0 16 > rep-ac-1rank-HT.txt  &&

mpirun -np 1 miniXyce-SRMT -c "$PWD"/tests/cirHuge.net "$PWD" 5 2 0 2 > rep-srmt-1rank-noHT.txt  &&
mpirun -np 1 miniXyce-SRMT -c "$PWD"/tests/cirHuge.net "$PWD" 5 2 0 16 > rep-srmt-1rank-HT.txt  &&

mpirun -np 1 miniXyce-RHT -c "$PWD"/tests/cirHuge.net "$PWD" 5 2 0 2  > rep-RHT-1rank-noHT.txt &&
mpirun -np 1 miniXyce-RHT -c "$PWD"/tests/cirHuge.net "$PWD" 5 2 0 16 > rep-RHT-1rank-HT.txt 

# 16 ranks test baselines vs Our Best Approach (NewLimit + varGrouping 8)

mpirun -np 16 miniXyce-AC -c "$PWD"/tests/cirHuge.net "$PWD" 5 32 0 2 4 6 8 10 12 14 1 3 5 7 9 11 13 15 16 18 20 22 24 26 28 30 17 19 21 23 25 27 29 31 > rep-ac-16rank-noHT.txt  &&
mpirun -np 16 miniXyce-AC -c "$PWD"/tests/cirHuge.net "$PWD" 5 32 0 16 2 18 4 20 6 22 8 24 10 26 12 28 14 30 1 17 3 19 5 21 7 23 9 25 11 27 13 29 15 31 > rep-ac-16rank-HT.txt  &&

mpirun -np 16 miniXyce-SRMT -c "$PWD"/tests/cirHuge.net "$PWD" 5 32 0 2 4 6 8 10 12 14 1 3 5 7 9 11 13 15 16 18 20 22 24 26 28 30 17 19 21 23 25 27 29 31 > rep-ac-16rank-noHT.txt  &&
mpirun -np 16 miniXyce-SRMT -c "$PWD"/tests/cirHuge.net "$PWD" 5 32 0 16 2 18 4 20 6 22 8 24 10 26 12 28 14 30 1 17 3 19 5 21 7 23 9 25 11 27 13 29 15 31 > rep-ac-16rank-HT.txt  &&

mpirun -np 16 miniXyce-RHT -c "$PWD"/tests/cirHuge.net "$PWD" 5 32 0 2 4 6 8 10 12 14 1 3 5 7 9 11 13 15 16 18 20 22 24 26 28 30 17 19 21 23 25 27 29 31 > rep-RHT-16rank-noHT.txt  &&
mpirun -np 16 miniXyce-RHT -c "$PWD"/tests/cirHuge.net "$PWD" 5 32 0 16 2 18 4 20 6 22 8 24 10 26 12 28 14 30 1 17 3 19 5 21 7 23 9 25 11 27 13 29 15 31 > rep-RHT-16rank-HT.txt

fi





