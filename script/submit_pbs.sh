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
  -c CLUSTER   [mogon|deep]
EOF
}

# parse command line options
OPTIND=1 # Reset command line option index
while getopts "hf:c:" opt; do
    case "$opt" in
        h)
            show_help
            exit 0
            ;;
        f)  pbsnames=$OPTARG
            ;;
        c)  cluster=$OPTARG
            ;;
        '?')
            show_help >&2
            exit 1
            ;;
    esac
done

#array containing the pbs names
script=( `cat $pbsnames` )

if [ "$cluster" == "deep" ]
then
    # submit the first job
    job=`qsub ${script[0]}`
elif [ "$cluster" == "mogon" ]
then
    # submit the first job
    out=`bsub < ${script[0]}`
    job=`echo $out | sed -E 's/Job\s+<([0-9]+)>\s+.+/\1/')`
fi

# submit all the remaining jobs
for i in `seq 1 $[${#script[@]}-1]`
do
    if [ "$cluster" == "deep" ]
    then
        # a new job can start after the previous is completed
        job=`qsub -W depend=afterok:$job ${script[i]}`
    elif [ "$cluster" == "mogon" ]
    then
        out=`bsub -w "exit($job, 0)" < ${script[i]}`
        job=`echo $out | sed -E 's/Job\s+<([0-9]+)>\s+.+/\1/'`
    fi
done
