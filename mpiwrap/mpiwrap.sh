#!/bin/bash
#########################################################
# Author: Giuseppe Congiu <giuseppe.congiu@seagate.com> #
# Name:   mpiwrap.sh                                    #
# Date:   14-07-2015                                    # 
#########################################################

# help message 
show_help() {
cat << EOF
Usage: $0 [options]

options:
  -h           show this message and exit
  -f FILENAME  name of the .a archive object
  -w           wrap replacing old symbols with __real_old
  -u           unwrap replacing __real_old symbols with old

EOF
}

# parse command line options
MODE="wrap"
OPTIND=1 # Reset command line option index
while getopts "hwuf:" opt; do
    case "$opt" in 
        h)
            show_help
            exit 0
            ;;
        f)  name=$OPTARG
            ;;
        w)  MODE="wrap"
            ;;
        u)  MODE="unwrap"
            ;;
        \?)
            show_help >&2
            exit 1
            ;;
    esac
done

# extract the dirname 
dir=`dirname $name`

# change to directory
cd $dir

# extract archive
arname=`basename $name`

# this check is to avoid multiple wrap/unwrap
CHECK=`nm $arname | grep __real_MPI_`
if [ -n "$CHECK" ] && [ $MODE == "wrap" ]
then 
    echo "exit, library already wrapped"
    exit 1
elif [ -z "$CHECK" ] && [ $MODE == "unwrap" ]
then
    echo "exit, library already unwrapped"
    exit 1
fi

ar x $arname

# to support more symbols add them in the following 
if [ $MODE == "wrap" ]; then
    # replace old symbols with new symbols
    objcopy --redefine-sym MPI_Init=__real_MPI_Init \
	    --redefine-sym MPI_Init_thread=__real_MPI_Init_thread \
            --redefine-sym MPI_Finalize=__real_MPI_Finalize \
            --redefine-sym MPI_File_open=__real_MPI_File_open \
            --redefine-sym MPI_File_close=__real_MPI_File_close \
	    --redefine-sym MPI_File_write_all=__real_MPI_File_write_all \
	    --redefine-sym MPI_File_write_at_all=__real_MPI_File_write_at_all \
            log_mpi_core.o log_mpi_core_new.o
elif [ $MODE == "unwrap" ]; then
    # replace new symbols with old symbols
    objcopy --redefine-sym __real_MPI_Init=MPI_Init \
	    --redefine-sym __read_MPI_Init_thread=MPI_Init_thread \
            --redefine-sym __real_MPI_Finalize=MPI_Finalize \
            --redefine-sym __real_MPI_File_open=MPI_File_open \
            --redefine-sym __real_MPI_File_close=MPI_File_close \
	    --redefine-sym __real_MPI_File_write_all=MPI_File_write_all \
	    --refering-sym __real_MPI_File_write_at_all=MPI_File_write_at_all \
            log_mpi_core.o log_mpi_core_new.o
fi

# overwrite old object file
mv log_mpi_core_new.o log_mpi_core.o

# replace old object with new
ar r $arname log_mpi_core.o

# cleanup 
rm log_mpi_core.o log_mpi_util.o

# return to OLD directory
cd - 
