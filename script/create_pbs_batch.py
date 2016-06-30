#!/usr/bin/python
###########################################################
# This script is used to create a bunch of pbs scripts,
# each of which stands for a specific configurations for
# variables including:
#   1) aggregators
#   2) collective buffer size
###########################################################
import os
from optparse import OptionParser

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
def create_script_files(prog,  # program name
                        args,  # program arguments
                        nodes, # number of nodes
                        ppn,   # processes per node
                        aggregators, # number of aggrs
                        col_buffer,  # col buffer size
                        ind_buffer,  # ind buffer size
                        stripe_unit, # stripe size
                        stripe_factor, # srtipe count
                        workdir,     # working dir
                        cache,       # cache mode
                        cachedir,    # cache directory
                        flush_flag,  # flush flag
                        discard_flag,# discard flag
                        hinttarget,  # file to hint
                        guardnames,  # list of coma separated guardname
                        ramdisksize, # size of ramdisk
                        cluster      # cluster to use: mogon or deep
                       ):

    for ch in cache:
        for ag in aggregators:
            for cb in col_buffer:
                for ib in ind_buffer:
                    hints_name = os.getcwd()+'/'+ch+'_'+str(ag)+'_'+str(cb)+'_'+str(ib)+'.hints'
                    f2 = open(hints_name, 'w')
                    if cluster == "mogon":
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
                    f2.close()
                    # benchmark dependent parameters
                    pbs_name = ch+'_'+str(ag)+'_'+str(cb)+'_'+str(ib)+'.pbs'
                    clog2_target = os.getcwd()+'/'+pbs_name
                    f = open(pbs_name, 'w')
                    if cluster == "mogon":
			ppn = 64
                        f.write(pbs_content_mogon%(pbs_name,
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
                    elif cluster == "deep":
                        f.write(pbs_content%(pbs_name,
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
                    f.close()

if __name__ == "__main__":

    # check the command line input
    parser = OptionParser()

    parser.add_option("-p", "--prog", dest="prog", \
                      help="full pathname of the input program")
    parser.add_option("-a", "--args", dest="args", \
                      help="args to be passed to input program")
    parser.add_option("-c", "--cluster", dest="cluster", \
                      help="select the cluster [mogon|deep]")

    (options, args) = parser.parse_args()

    prog          = str(options.prog)
    args          = str(options.args)
    cluster       = str(options.cluster)
    nodes         = 64
    ppn           = 8
    aggregators   = [4, 8, 16, 32, 64]
    col_buffer    = [4194304, 8388608, 16777216, 33554432, 67108864]
    #col_buffer    = [8388608]
    ind_buffer    = [524288]
    #ind_buffer    = [524288, 1048576, 4194304, 8388608, 16777216]
    stripe_unit   = 4194304
    stripe_factor = 4
    cache         = ['enable', 'disable']
    flush_flag    = 'flush_immediate'
    discard_flag  = 'enable'
    workdir       = 'replace_me_with_run_dir'
    if cluster == "mogon":
        cachedir = os.path.abspath('/jobdir')+'/LSB_JOBID'+'/ramdisk'
    elif cluster == "deep":
        cachedir = 'replace_me_with_cachedir'
    hinttarget    = 'replace_me_with_hint_target'
    guardnames    = 'replace_me_with_guardnames'
    ramdisksize   = 'replace_me_with_ramdisk_size'

    if args is None:
        args = ""

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
