#! /bin/bash

echo "Set up for Raspberry Pi Departure Board"
echo " "
echo "Making sure there are executable permissions on config_server.py"
chmod 755 ./config_server.py

echo " "
echo "Making sure there are executable permissions on run.sh"
chmod 755 ./run.sh

echo " "
echo "Making sure $HOME has execute/read permissions (so root can run the config server)"
chmod 755 $HOME

echo " "
echo "Making sure $PWD has execute/read permissions"
chmod 755 $PWD

echo " "
echo "Making a copy of config.txt to use as a default configuration"
cp config.txt default-config.txt

echo " "
echo "Creating the UI configuration file"

echo "Executable_directory - $PWD" > ui-config.txt
echo "Executable_command_line - $PWD/departureboard -f $PWD/config.txt -d" >> ui-config.txt
echo "Port - 80" >> ui-config.txt
echo "Defaults_Config_file - $PWD/default-config.txt" >> ui-config.txt

echo " "
echo "Creating the Makefile"
chmod 755 make_makefile.sh 
./make_makefile.sh 

echo " "
echo "Building the traindisplay executable (this may take a while)"
mkdir Obj >& /dev/null
make all

echo " "
echo "If everything has gone to plan execute './run.sh' to start the UI"
echo " "
