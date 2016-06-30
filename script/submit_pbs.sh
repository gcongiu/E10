#!/bin/bash
#################################################
# This script is supposed to submit pbs jobs/files
# one by one.
# Type "ls *.pbs > namesOfPbs" first to create
# an input file
#################################################

# help message
show_help() {
cat << EOF
Usage: $0 [options]

options:
  -h           show this message and exit
  -f FILENAME  file of pbs file names
EOF
}

# parse command line options
OPTIND=1 # Reset command line option index
while getopts "hf:" opt; do
    case "$opt" in
        h)
            show_help
            exit 0
            ;;
        f)  pbsnames=$OPTARG
            ;;
        '?')
            show_help >&2
            exit 1
            ;;
    esac
done

#array containing the pbs names
script=( `cat $pbsnames` )

# submit the first job
job=`qsub ${script[0]}`

# submit all the remaining jobs
for i in `seq 1 $[${#script[@]}-1]`
do
    # a new job can start after the previous is completed
    job=`qsub -W depend=afterok:$job ${script[i]}`
done
