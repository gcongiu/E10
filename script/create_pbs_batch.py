#!/usr/bin/python
###########################################################
# This script is used to create a bunch of pbs scripts,
# each of which stands for a specific configurations for
# variables including:
#   1) aggregators
#   2) collective buffer size
#   3) independent buffer size
###########################################################
import os
import re
from optparse import OptionParser

###### deep cluster
pbs_content = '''
#!/bin/bash -l
#PBS -N %s
#PBS -e %s.err
#PBS -o %s.log
#PBS -l walltime=00:30:00
#PBS -l nodes=%s:ppn=%s
#PBS -m abe
#PBS -M giuseppe.congiu@seagate.com
#PBS -v MPE_TMPDIR=$HOME/.tmp

cd %s
rm *
module load parastation
mpiexec --env=MPI_HINTS_CONFIG %s -np %s %s %s

mv %s.clog2 %s.clog2
'''

hints_content = '''
{
    "Guardnames": "%s",
    "File": [
    {
        "Path": "%s",
        "Type": "MPI",
        "cb_buffer_size": "%s",
        "ind_wr_buffer_size": "%s",
        "cb_nodes": "%s",
        "romio_cb_read": "enable",
        "romio_cb_write": "enable",
        "romio_no_indep_rw": "true",
        "striping_unit": "%s",
        "striping_factor": "%s",
        "e10_cache": "%s",
        "e10_cache_path": "%s",
        "e10_cache_flush_flag": "%s",
        "e10_cache_discard_flag": "%s"
    }]
}
'''

###### mogon cluster
pbs_content_mogon = '''
#!/bin/bash
#BSUB -q nodeshort
#BSUB -J %s
#BSUB -e %s.err
#BSUB -o %s.log
#BSUB -W 00:30
#BSUB -n %s
#BSUB -R "span[ptile=%s]"
#BSUB -R "rusage[ramdisk=%s]"
#BSUB -R select[mem>1672]
#BSUB -R rusage[mem=1672]
#BSUB -R affinity[core(8)]
#BSUB -u giuseppe.congiu@seagate.com
#BSUB -N

cd %s
rm *

# ramdisk is created in jobdir/$LSB_JOBID/ramdisk
# -> change the dir inside hint file accordingly
sed -i 's/LSB_JOBID/'"`echo $LSB_JOBID`"'/' %s

mpirun -env MPI_HINTS_CONFIG %s %s %s

# restore LSB_JOBID macro in the hint file
sed -i 's/'"`echo $LSB_JOBID`"'/LSB_JOBID/' %s

mv %s.clog2 %s.clog2

# replace_me_with_* fields are benchmark dependent and
# should be customised after this script using the
# command line, e.g.:
#     sed -i 's/replace_me_with_pathname/pathname'
#
'''

hints_content_mogon = '''
{
    "Guardnames": "%s",
    "File": [
    {
        "Path": "%s",
        "Type": "MPI",
        "cb_buffer_size": "%s",
        "ind_wr_buffer_size": "%s",
        "cb_nodes": "%s",
        "romio_cb_read": "enable",
        "romio_cb_write": "enable",
        "romio_no_indep_rw": "true",
        "e10_cache": "%s",
        "e10_cache_path": "%s",
        "e10_cache_flush_flag": "%s",
        "e10_cache_discard_flag": "%s"
    }]
}
'''

# create hints files and pbs scripts
def create_script_files(prog,           # program name
                        args,           # program arguments
                        nodes,          # number of nodes
                        ppn,            # processes per node
                        aggregators,    # number of aggrs
                        col_buffer,     # col buffer size
                        ind_buffer,     # ind buffer size
                        stripe_unit,    # stripe size
                        stripe_factor,  # stripe count
                        workdir,        # working dir
                        cache,          # cache mode
                        cachedir,       # cache directory
                        flush_flag,     # flush flag
                        discard_flag,   # discard flag
                        hinttarget,     # file to hint
                        guardnames,     # list of coma separated guardname
                        ramdisksize,    # size of ramdisk
                        cluster         # cluster to use: mogon or deep
                       ):

    for ch in cache:                    # for each cache mode
        for ag in aggregators:          # for each aggregator combination
            for cb in col_buffer:       # for each collective buffer combination
                for ib in ind_buffer:   # for each independent buffer combination
                    pbs_name = ch+'_'+str(ag)+'_'+str(cb)+'_'+str(ib)+'.pbs'
                    f1 = open(pbs_name, 'w')
                    hints_name = os.getcwd()+'/'+ch+'_'+str(ag)+'_'+str(cb)+'_'+str(ib)+'.hints'
                    f2 = open(hints_name, 'w')
                    clog2_target = os.getcwd()+'/'+pbs_name
                    if cluster == "mogon":
                        f1.write(pbs_content_mogon%(pbs_name,
                                                    pbs_name,
                                                    pbs_name,
                                                    nodes*ppn,
                                                    ppn,
                                                    ramdisksize,
                                                    workdir,
                                                    hints_name,
                                                    hints_name,
                                                    prog,
                                                    args,
                                                    hints_name,
                                                    prog,
                                                    clog2_target
                                                   )
                                )
                        f2.write(hints_content_mogon%(guardnames,
                                                      hinttarget,
                                                      cb,
                                                      ib,
                                                      ag,
                                                      ch,
                                                      cachedir,
                                                      flush_flag,
                                                      discard_flag
                                                     )
                                )
                    elif cluster == "deep":
                        f1.write(pbs_content%(pbs_name,
                                              pbs_name,
                                              pbs_name,
                                              nodes,
                                              ppn,
                                              workdir,
                                              hints_name,
                                              nodes*ppn,
                                              prog,
                                              args,
                                              prog,
                                              clog2_target
                                             )
                                )
                        f2.write(hints_content%(guardnames,
                                                hinttarget,
                                                cb,
                                                ib,
                                                ag,
                                                stripe_unit,
                                                stripe_factor,
                                                ch,
                                                cachedir,
                                                flush_flag,
                                                discard_flag
                                               )
                                )
                    f1.close()
                    f2.close()

if __name__ == "__main__":

    # check the command line input
    parser = OptionParser()

    parser.add_option("-p", "--prog", dest="prog", \
                      help="full pathname of the input program")
    parser.add_option("-a", "--args", dest="args", \
                      help="args to be passed to input program")
    parser.add_option("-e", "--env", dest="env", \
                      help="file containing environment params: NODES, PPN, AGGREGATORS, COLL_BUFFER_SIZES, IND_BUFFER_SIZES, STRIPE_UNIT, STRIPE_FACTOR, CACHE, FLUSH_FLAG, DISCARD_FLAG, TARGET_DIR, CACHE_DIR, HINT_TARGET, GUARDNAMES, RAMDISK_SIZE")
    parser.add_option("-c", "--cluster", dest="cluster", \
                      help="select the cluster [mogon|deep]")

    (options, args) = parser.parse_args()
    prog          = str(options.prog)
    args          = str(options.args)
    cluster       = str(options.cluster)
    env           = str(options.env)

    # set env variables for simulation
    line_regex = re.compile('(.+)=(.+)')
    env_file = open(env, "r")
    for line in env_file:
        res = re.search(line_regex, line)
        if res is not None:
            key = res.group(1)
            value = res.group(2)
            os.environ[key] = value
    env_file.close()

    # get configuration from env variables
    nodes         = int(os.getenv("NODES"))
    ppn           = int(os.getenv("PPN"))
    aggregators   = os.getenv("AGGREGATORS")
    col_buffer    = os.getenv("COLL_BUFFER_SIZES")
    ind_buffer    = os.getenv("IND_BUFFER_SIZES")
    stripe_unit   = os.getenv("STRIPE_UNIT")
    stripe_factor = os.getenv("STRIPE_FACTOR")
    cache         = os.getenv("CACHE")
    flush_flag    = os.getenv("FLUSH_FLAG")
    discard_flag  = os.getenv("DISCARD_FLAG")
    workdir       = os.getenv("TARGET_DIR")
    hinttarget    = os.getenv("HINT_TARGET")
    guardnames    = os.getenv("GUARDNAMES")
    ramdisksize   = os.getenv("RAMDISK_SIZE")
    if cluster == "mogon":
        cachedir = os.path.abspath('/jobdir')+'/LSB_JOBID'+'/ramdisk'
    elif cluster == "deep":
        cachedir = os.getenv("CACHE_DIR")

    if args is None:
        args = ""

    # convert string into array of int '[8, 16, 32, 64]' -> [8, 16, 32, 64]
    aggr_size = aggregators.count(',') + 1
    if aggr_size > 1:
        aggregators = [int(x) for x in aggregators.replace('[','').replace(']','').replace(' ','').split(',', aggr_size)]
    elif aggr_size == 1:
        aggregators = [int(aggregators.replace('[','').replace(']',''))]
    #print aggregators

    col_buf_size = col_buffer.count(',') + 1
    if col_buf_size > 1:
        col_buffer = [int(x) for x in col_buffer.replace('[','').replace(']','').replace(' ','').split(',', col_buf_size)]
    elif col_buf_size == 1:
        col_buffer = [int(col_buffer.replace('[','').replace(']',''))]
    #print col_buffer

    ind_buf_size = ind_buffer.count(',') + 1
    if ind_buf_size > 1:
        ind_buffer = [int(x) for x in ind_buffer.replace('[','').replace(']','').replace(' ','').split(',', ind_buf_size)]
    elif ind_buf_size == 1:
        ind_buffer = [int(ind_buffer.replace('[','').replace(']',''))]
    #print ind_buffer

    # convert string into array of char '[enable, disable]' -> ['enable', 'disable']
    cache = cache.replace('[','').replace(']','').replace(' ','').split(',', 2)
    #print cache

    # create script files
    create_script_files(prog,
                        args,
                        nodes,
                        ppn,
                        aggregators,
                        col_buffer,
                        ind_buffer,
                        stripe_unit,
                        stripe_factor,
                        workdir,
                        cache,
                        cachedir,
                        flush_flag,
                        discard_flag,
                        hinttarget,
                        guardnames,
                        ramdisksize,
                        cluster)
