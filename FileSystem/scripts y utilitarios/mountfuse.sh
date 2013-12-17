#!/bin/bash
# -*- ENCODING: UTF-8 -*-

options="-o"
mountpoint="/tmp/fusea/"
debug=""
loglevel="None"
logpath=""
exp="False"
exppath="../../Shared-Library/Release"
discpath="disk.bin"


if [[ $1 == "--help" ]]
then
	echo ""
	echo "	Help for mountfuse."
	echo ""
	echo ""
	echo "	Mountfuse will mount a FUSE implementation in MOUNTPOINT. It will use a disc found at DISK."
	echo ""
	echo "	Usage:"
	echo "		./mountfuse.sh [-D DISK] [MOUNTPOINT] [direct_io] [-d] [--export] [-u]"
	echo ""
	echo "	Options could be sent in any order as long as you don't mess up with any of them. Horrible thing will happen to MOUNTPOINT if yo do. Any option is optional, unless it's specified at it's descriptions. Many of them have several dependencies (that I must correct in this lifetime)."
	echo ""
	echo "	Param description:"
	echo ""
	echo "		-D Disc_Path	-	Will indicate in which path FUSE should find a disk formatted in GRASA."
	echo ""
	echo "		-d		-	Will mount FUSE in Debug mode. It will show any call that FUSE makes to its functions. It's turned off by default. IMPORTANT: FUSE calls have no necessarily a 1 on 1 correspondence with system calls, as it involves its own cache. You will have to mount with direct_io for that."
	echo ""
	echo "		direct_io	- 	Tells FUSE to use Direct Input/Output mode. It will stop using its internal cache, and start working less as a layer and more like than a translator of system calls. By default, it's turned off."
	echo ""
	echo "		--export	-	Implies an export of the shared libraries needed to run GRASA. Shared libraries MUST be located in ../../Shared-Library/Release. I hope to change that in future versions. By default, turned off."
	echo ""
	echo "		-u		-	Unmounts FUSE in [MOUNTPOINT]. It requieres that you first provide the MOUNTPOINT argument, and then the -u. It also requieres an existing unmountfuse.sh file in the same folder."
	echo ""
	echo "		MOUNTPOINT	-	Will be the mountpoint in which GRASA will be mounted. By default it will be /tmp/fusea/, and will be created if it hasn't already"
	echo ""
	echo "Important considerations:"
	echo ""
	echo " 	For this script to work, there MUST be a FileSystem executable at path ../Release/FileSystem. Other case scenario, this thing will not only not work, but it's behaviour is both undefined and apotheotic (yeah, apotheotic, so take care of your kernel). In fact, several kittens will die for any misscall to this script."
	echo "	There are several more options that could be sent to both FUSE and GRASA implementations, but this script is currently not considering them. The ones that are indeed considered are more than enough to make an almost full use of the FileSystem."
	echo "	Anyway, you could still edit this script and add (and by add, I mean hardcode) more of fuse options. This will happen if you edit the 'options' variable. Be careful separate all options with ',' -including direct_io."
	echo ""
	echo "					mountfuse.sh	V1.0 By Maximiliano Felice."
	echo "					Ingenieria en Sistemas de Informacion - UTN - FRBA "
	exit
fi
i=1
disc=0
for param in $@
do
	if [[ $param == "direct_io" ]]
	then
		options="$options direct_io"
	elif [[ $param == "-d" ]]
	then
		debug="-d"
	elif [[ $param == "--export" ]]
	then
		exp="True"
	elif [[ $param == "-u" ]]
	then
		./unmountfuse.sh $mountpoint
	elif [[ $param == "-D" ]]
	then
		disc=1
	elif [[ $disc != 0 ]]
	then
		discpath=$param
		disc=0
	else
		mountpoint=$param
	fi
	i=`expr $i + 1`
done
echo -n "Unmounting previos FileSystems in $mountpoint ...	"
fusermount -q -u $mountpoint
echo "DONE!"
rm -r $mountpoint
mkdir $mountpoint

if [[ $exp == "True" ]]
then
	echo -n "Exporting libraries in $exppath...	"
	export LD_LIBRARY_PATH=$exppath
	echo "DONE!"
fi

echo -n "Mounting GRASA with params: $options $debug --ll=$loglevel --Disc-Path=$discpath in folder $mountpoint...	"
../Release/FileSystem $options $debug --ll=$loglevel --Disc-Path=$discpath $mountpoint
echo "DONE!"
echo ""
echo "Mounted, and ready to go!!!"
echo ""
pcmanfm $mountpoint
