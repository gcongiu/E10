#!/usr/bin/python
#########################################################
# Author: Giuseppe Congiu <giuseppe.congiu@seagate.com> #
# Name:   from_clog2_to_csv.py                          #
#########################################################
import re
from subprocess import call
from optparse import OptionParser

# NOTE: in the following we always assume MPI_File operations follow the schema:
#   - MPI_File_open
#    - MPI_File_write
#    - MPI_File_read
#   - MPI_File_close
#   We do not deal with multiple asynchronous read/write operations since it is
#   not possible to associate an ADIOI_{read,write} operation to the corresponding
#   MPI_File_{read,write} operation in MPE (see CreatCsvProfilingFile for an example).

# return the event tags for every collective IO operation in the clog2 file
def GetMpiIoEventTags(clog2):

    # line regular expression
    regex = re.compile('.+icomm=([0-9]+).+s_et=([0-9]+)\s+e_et=([0-9]+).+name=(.+)\s+.+')

    events = {}

    # initialize all the events
    init = [-100, -100]
    events['MPI_File_open']         = init
    events['MPI_File_close']        = init
    events['MPI_File_write']        = init
    events['MPI_File_write_at']     = init
    events['MPI_File_read']         = init
    events['MPI_File_read_at']      = init
    events['MPI_File_write_all']    = init
    events['MPI_File_write_at_all'] = init
    events['MPI_File_read_all']     = init
    events['MPI_File_read_at_all']  = init
    events['ADIOI_ext2ph_all2all']  = init
    events['ADIOI_ext2ph_shuffle']  = init
    events['ADIOI_ext2ph_waitall']  = init
    events['ADIOI_ext2ph_isend']    = init
    events['ADIOI_ext2ph_irecv']    = init
    events['ADIOI_ext2ph_recv']     = init
    events['ADIOI_ext2ph_startup']  = init
    events['ADIOI_e10_sync']        = init
    events['ADIOI_e10_sync_read']   = init
    events['ADIOI_e10_sync_write']  = init
    events['read']                  = init
    events['write']                 = init

    # scan the file to find out the event tags for every single op
    for line in clog2:
        res = re.search(regex, line)
        if res is not None:
            event = res.group(4)
            if   event ==  'MPI_File_open':
                events['MPI_File_open']         = [res.group(2), res.group(3)]
            elif event ==  'MPI_File_close':
                events['MPI_File_close']        = [res.group(2), res.group(3)]
            elif event ==  'MPI_File_write_all':
                events['MPI_File_write_all']    = [res.group(2), res.group(3)]
            elif event ==  'MPI_File_read_all':
                events['MPI_File_read_all']     = [res.group(2), res.group(3)]
            elif event ==  'MPI_File_write':
                events['MPI_File_write']        = [res.group(2), res.group(3)]
            elif event ==  'MPI_File_read':
                events['MPI_File_read']         = [res.group(2), res.group(3)]
            elif event ==  'MPI_File_write_at':
                events['MPI_File_write_at']     = [res.group(2), res.group(3)]
            elif event ==  'MPI_File_read_at':
                events['MPI_File_read_at']      = [res.group(2), res.group(3)]
            elif event ==  'MPI_File_write_at_all':
                events['MPI_File_write_at_all'] = [res.group(2), res.group(3)]
            elif event ==  'MPI_File_read_at_all':
                events['MPI_File_read_at_all']  = [res.group(2), res.group(3)]
            elif event ==  'ADIOI_ext2ph_shuffle':
                events['ADIOI_ext2ph_shuffle']  = [res.group(2), res.group(3)]
            elif event ==  'ADIOI_ext2ph_waitall':
                events['ADIOI_ext2ph_waitall']  = [res.group(2), res.group(3)]
            elif event ==  'ADIOI_ext2ph_isend':
                events['ADIOI_ext2ph_isend']    = [res.group(2), res.group(3)]
            elif event ==  'ADIOI_ext2ph_irecv':
                events['ADIOI_ext2ph_irecv']    = [res.group(2), res.group(3)]
            elif event ==  'ADIOI_ext2ph_startup':
                events['ADIOI_ext2ph_startup']  = [res.group(2), res.group(3)]
            elif event ==  'ADIOI_ext2ph_recv':
                events['ADIOI_ext2ph_recv']     = [res.group(2), res.group(3)]
            elif event ==  'ADIOI_ext2ph_all2all':
                events['ADIOI_ext2ph_all2all']  = [res.group(2), res.group(3)]
            elif event ==  'ADIOI_e10_sync':
                events['ADIOI_e10_sync']        = [res.group(2), res.group(3)]
            elif event ==  'ADIOI_e10_sync_read':
                events['ADIOI_e10_sync_read']   = [res.group(2), res.group(3)]
            elif event ==  'ADIOI_e10_sync_write':
                events['ADIOI_e10_sync_write']  = [res.group(2), res.group(3)]
            elif event ==  'read':
                events['read']                  = [res.group(2), res.group(3)]
            elif event ==  'write':
                events['write']                 = [res.group(2), res.group(3)]
            elif event ==  'postwrite':
                events['postwrite']             = [res.group(2), res.group(3)]

    return events

# scan the clog2 file and return the rank of IO aggregators (the ones receiving data)
def GetMpiIoAggregators(clog2, event, comm):

    # line regular expression
    regex = re.compile('.+icomm=([0-9]+)\s+rank=([0-9]+).+et=([0-9]+)')

    # aggregators list
    aggregators = []
    # NOTE: using readers and writers to identify aggregators works only if the application is using exclusively collective I/O
    #       a better solution is to instrument MPI_Send and MPI_Recv inside collective I/O code and use them to identify aggregators.
    writers     = []
    readers     = []

    # convert comm to string
    comm = str(comm)

    for line in clog2:
        res = re.search(regex, line)
        if res is not None:
            icomm = res.group(1)
            rank  = res.group(2)
            et    = res.group(3)
            if   icomm >= comm and et == event.get('ADIOI_ext2ph_irecv')[0] and writers.count(rank) == 0:
                writers.append(rank)
            elif icomm >= comm and et == event.get('ADIOI_ext2ph_isend')[0] and readers.count(rank) == 0:
                readers.append(rank)
            elif icomm >= comm and et == event.get('ADIOI_ext2ph_recv')[0] and writers.count(rank) == 0:
                writers.append(rank)

    # the number of aggregators is the smallest between sender and receiver
    if   len(readers) >= len(writers):
        aggregators = writers
    elif len(writers) >= len(readers):
        aggregators = readers

    return aggregators


# extract the information from the clog2 file and create a csv file containing detailed timings
def CreatCsvProfilingFile(clog2, csv, comm, MPI_events, MPI_aggrs):

    # line regular expression
    regex = re.compile('ts=([0-9]+\.[0-9]{6})\s+icomm=([0-9]+)\s+rank=([0-9]+).+type=bare\s+et=([0-9]+)')

    # convert comm to string
    comm = str(comm)
    e10_cache_overhead = 0

    ioprof = {}
    for rank in MPI_aggrs:
        ioprof[rank] = {}
        # init dict
        ioprof[rank]['MPI_File_write_all'] = {'Elapsed': 0.0}
        ioprof[rank]['MPI_File_write_all']['ADIOI_ext2ph_shuffle'] = 0.0
        ioprof[rank]['MPI_File_write_all']['ADIOI_ext2ph_startup'] = 0.0
        ioprof[rank]['MPI_File_write_all']['ADIOI_ext2ph_isend'] = 0.0
        ioprof[rank]['MPI_File_write_all']['ADIOI_ext2ph_irecv'] = 0.0
        ioprof[rank]['MPI_File_write_all']['ADIOI_ext2ph_recv'] = 0.0
        ioprof[rank]['MPI_File_write_all']['ADIOI_ext2ph_waitall'] = 0.0
        ioprof[rank]['MPI_File_write_all']['ADIOI_ext2ph_all2all'] = 0.0
        ioprof[rank]['MPI_File_write_all']['read'] = 0.0 # for read/modify/write
        ioprof[rank]['MPI_File_write_all']['write'] = 0.0
        ioprof[rank]['MPI_File_write_all']['postwrite'] = 0.0
        ioprof[rank]['MPI_File_write_at_all'] = {'Elapsed': 0.0}
        ioprof[rank]['MPI_File_write_at_all']['ADIOI_ext2ph_shuffle'] = 0.0
        ioprof[rank]['MPI_File_write_at_all']['ADIOI_ext2ph_startup'] = 0.0
        ioprof[rank]['MPI_File_write_at_all']['ADIOI_ext2ph_isend'] = 0.0
        ioprof[rank]['MPI_File_write_at_all']['ADIOI_ext2ph_irecv'] = 0.0
        ioprof[rank]['MPI_File_write_at_all']['ADIOI_ext2ph_recv'] = 0.0
        ioprof[rank]['MPI_File_write_at_all']['ADIOI_ext2ph_waitall'] = 0.0
        ioprof[rank]['MPI_File_write_at_all']['ADIOI_ext2ph_all2all'] = 0.0
        ioprof[rank]['MPI_File_write_at_all']['read'] = 0.0
        ioprof[rank]['MPI_File_write_at_all']['write'] = 0.0
        ioprof[rank]['MPI_File_write_at_all']['postwrite'] = 0.0
        ioprof[rank]['MPI_File_read_all'] = {'Elapsed': 0.0}
        ioprof[rank]['MPI_File_read_all']['ADIOI_ext2ph_shuffle'] = 0.0
        ioprof[rank]['MPI_File_read_all']['ADIOI_ext2ph_isend'] = 0.0
        ioprof[rank]['MPI_File_read_all']['ADIOI_ext2ph_irecv'] = 0.0
        ioprof[rank]['MPI_File_read_all']['ADIOI_ext2ph_recv'] = 0.0
        ioprof[rank]['MPI_File_read_all']['ADIOI_ext2ph_waitall'] = 0.0
        ioprof[rank]['MPI_File_read_all']['ADIOI_ext2ph_all2all'] = 0.0
        ioprof[rank]['MPI_File_read_all']['ADIOI_ext2ph_startup'] = 0.0
        ioprof[rank]['MPI_File_read_all']['read'] = 0.0
        ioprof[rank]['MPI_File_read_at_all'] = {'Elapsed': 0.0}
        ioprof[rank]['MPI_File_read_at_all']['ADIOI_ext2ph_shuffle'] = 0.0
        ioprof[rank]['MPI_File_read_at_all']['ADIOI_ext2ph_startup'] = 0.0
        ioprof[rank]['MPI_File_read_at_all']['ADIOI_ext2ph_irecv'] = 0.0
        ioprof[rank]['MPI_File_read_at_all']['ADIOI_ext2ph_recv'] = 0.0
        ioprof[rank]['MPI_File_read_at_all']['ADIOI_ext2ph_waitall'] = 0.0
        ioprof[rank]['MPI_File_read_at_all']['ADIOI_ext2ph_all2all'] = 0.0
        ioprof[rank]['MPI_File_read_at_all']['read'] = 0.0
        ioprof[rank]['MPI_File_close'] = {'Elapsed': 0.0}
        ioprof[rank]['ADIOI_e10_sync'] = {'Elapsed': 0.0}
        ioprof[rank]['ADIOI_e10_sync']['ADIOI_e10_sync_read'] = 0.0
        ioprof[rank]['ADIOI_e10_sync']['ADIOI_e10_sync_write'] = 0.0

    for line in clog2:
        res = re.search(regex, line)
        if res is not None:
            icomm = res.group(2)
            rank  = res.group(3)
            et    = res.group(4)
            if icomm >= comm and MPI_aggrs.count(rank) == 1:
                # get time stamp
                ts = res.group(1)
                # measure the arriving time for each event of interest -> MPI_events['event'][0]
                if   et == MPI_events['MPI_File_write_all'][0]:
                    ioprof[rank]['MPI_File_write_all']['Elapsed'] -= float(ts)
                elif et == MPI_events['MPI_File_write_at_all'][0]:
                    ioprof[rank]['MPI_File_write_at_all']['Elapsed'] -= float(ts)
                elif et == MPI_events['MPI_File_read_all'][0]:
                    ioprof[rank]['MPI_File_read_all']['Elapsed'] -= float(ts)
                elif et == MPI_events['MPI_File_read_at_all'][0]:
                    ioprof[rank]['MPI_File_read_at_all']['Elapsed'] -= float(ts)
                elif et == MPI_events['write'][0]:
                    # write follows either write_all or write_at_all
                    if   ioprof[rank]['MPI_File_write_all']['Elapsed'] != 0:
                        ioprof[rank]['MPI_File_write_all']['write'] -= float(ts)
                    elif ioprof[rank]['MPI_File_write_at_all']['Elapsed'] != 0:
                        ioprof[rank]['MPI_File_write_at_all']['write'] -= float(ts)
                elif et == MPI_events['postwrite'][0]:
                    # postwrite follows either write_all or write_at_all
                    if   ioprof[rank]['MPI_File_write_all']['Elapsed'] != 0:
                        ioprof[rank]['MPI_File_write_all']['postwrite'] -= float(ts)
                    elif ioprof[rank]['MPI_File_write_at_all']['Elapsed'] != 0:
                        ioprof[rank]['MPI_File_write_at_all']['postwrite'] -= float(ts)
                elif et == MPI_events['read'][0]:
                    # read follows either read_all, read_at_all, write_all or write_at_all:
                    # thus we do consider data sieving in collective write operations
                    if   ioprof[rank]['MPI_File_read_all']['Elapsed'] != 0:
                        ioprof[rank]['MPI_File_read_all']['read'] -= float(ts)
                    elif ioprof[rank]['MPI_File_read_at_all']['Elapsed'] != 0:
                        ioprof[rank]['MPI_File_read_at_all']['read'] -= float(ts)
                    elif ioprof[rank]['MPI_File_write_all']['Elapsed'] != 0:
                        ioprof[rank]['MPI_File_write_all']['read'] -= float(ts)
                    elif ioprof[rank]['MPI_File_write_at_all']['Elapsed'] != 0:
                        ioprof[rank]['MPI_File_write_at_all']['read'] -= float(ts)
                elif et == MPI_events['ADIOI_ext2ph_shuffle'][0]:
                    # shuffle follows either read_all/write_all or read_at_all/write_at_all
                    if   ioprof[rank]['MPI_File_write_all']['Elapsed'] != 0:
                        ioprof[rank]['MPI_File_write_all']['ADIOI_ext2ph_shuffle'] -= float(ts)
                    elif ioprof[rank]['MPI_File_write_at_all']['Elapsed'] != 0:
                        ioprof[rank]['MPI_File_write_at_all']['ADIOI_ext2ph_shuffle'] -= float(ts)
                    elif ioprof[rank]['MPI_File_read_all']['Elapsed'] != 0:
                        ioprof[rank]['MPI_File_read_all']['ADIOI_ext2ph_shuffle'] -= float(ts)
                    elif ioprof[rank]['MPI_File_read_at_all']['Elapsed'] != 0:
                        ioprof[rank]['MPI_File_read_at_all']['ADIOI_ext2ph_shuffle'] -= float(ts)
                elif et == MPI_events['ADIOI_ext2ph_startup'][0]:
                    # startup follows either read_all/write_all or read_at_all/write_at_all
                    if   ioprof[rank]['MPI_File_write_all']['Elapsed'] != 0:
                        ioprof[rank]['MPI_File_write_all']['ADIOI_ext2ph_startup'] -= float(ts)
                    elif ioprof[rank]['MPI_File_write_at_all']['Elapsed'] != 0:
                        ioprof[rank]['MPI_File_write_at_all']['ADIOI_ext2ph_startup'] -= float(ts)
                    elif ioprof[rank]['MPI_File_read_all']['Elapsed'] != 0:
                        ioprof[rank]['MPI_File_read_all']['ADIOI_ext2ph_startup'] -= float(ts)
                    elif ioprof[rank]['MPI_File_read_at_all']['Elapsed'] != 0:
                        ioprof[rank]['MPI_File_read_at_all']['ADIOI_ext2ph_startup'] -= float(ts)
                elif et == MPI_events['ADIOI_ext2ph_waitall'][0]:
                    # waitall follows either read_all/write_all or read_at_all/write_at_all
                    if   ioprof[rank]['MPI_File_write_all']['Elapsed'] != 0:
                        ioprof[rank]['MPI_File_write_all']['ADIOI_ext2ph_waitall'] -= float(ts)
                    elif ioprof[rank]['MPI_File_write_at_all']['Elapsed'] != 0:
                        ioprof[rank]['MPI_File_write_at_all']['ADIOI_ext2ph_waitall'] -= float(ts)
                    elif ioprof[rank]['MPI_File_read_all']['Elapsed'] != 0:
                        ioprof[rank]['MPI_File_read_all']['ADIOI_ext2ph_waitall'] -= float(ts)
                    elif ioprof[rank]['MPI_File_read_at_all']['Elapsed'] != 0:
                        ioprof[rank]['MPI_File_read_at_all']['ADIOI_ext2ph_waitall'] -= float(ts)
                elif et == MPI_events['ADIOI_ext2ph_isend'][0]:
                    # isend follows either read_all/write_all or read_at_all/write_at_all
                    if   ioprof[rank]['MPI_File_write_all']['Elapsed'] != 0:
                        ioprof[rank]['MPI_File_write_all']['ADIOI_ext2ph_isend'] -= float(ts)
                    elif ioprof[rank]['MPI_File_write_at_all']['Elapsed'] != 0:
                        ioprof[rank]['MPI_File_write_at_all']['ADIOI_ext2ph_isend'] -= float(ts)
                    elif ioprof[rank]['MPI_File_read_all']['Elapsed'] != 0:
                        ioprof[rank]['MPI_File_read_all']['ADIOI_ext2ph_isend'] -= float(ts)
                    elif ioprof[rank]['MPI_File_read_at_all']['Elapsed'] != 0:
                        ioprof[rank]['MPI_File_read_at_all']['ADIOI_ext2ph_isend'] -= float(ts)
                elif et == MPI_events['ADIOI_ext2ph_irecv'][0]:
                    # irecv follows either read_all/write_all or read_at_all/write_at_all
                    if   ioprof[rank]['MPI_File_write_all']['Elapsed'] != 0:
                        ioprof[rank]['MPI_File_write_all']['ADIOI_ext2ph_irecv'] -= float(ts)
                    elif ioprof[rank]['MPI_File_write_at_all']['Elapsed'] != 0:
                        ioprof[rank]['MPI_File_write_at_all']['ADIOI_ext2ph_irecv'] -= float(ts)
                    elif ioprof[rank]['MPI_File_read_all']['Elapsed'] != 0:
                        ioprof[rank]['MPI_File_read_all']['ADIOI_ext2ph_irecv'] -= float(ts)
                    elif ioprof[rank]['MPI_File_read_at_all']['Elapsed'] != 0:
                        ioprof[rank]['MPI_File_read_at_all']['ADIOI_ext2ph_irecv'] -= float(ts)
                elif et == MPI_events['ADIOI_ext2ph_recv'][0]:
                    # recv follows either read_all/write_all or read_at_all/write_at_all
                    if   ioprof[rank]['MPI_File_write_all']['Elapsed'] != 0:
                        ioprof[rank]['MPI_File_write_all']['ADIOI_ext2ph_recv'] -= float(ts)
                    elif ioprof[rank]['MPI_File_write_at_all']['Elapsed'] != 0:
                        ioprof[rank]['MPI_File_write_at_all']['ADIOI_ext2ph_recv'] -= float(ts)
                    elif ioprof[rank]['MPI_File_read_all']['Elapsed'] != 0:
                        ioprof[rank]['MPI_File_read_all']['ADIOI_ext2ph_recv'] -= float(ts)
                    elif ioprof[rank]['MPI_File_read_at_all']['Elapsed'] != 0:
                        ioprof[rank]['MPI_File_read_at_all']['ADIOI_ext2ph_recv'] -= float(ts)
                elif et == MPI_events['ADIOI_ext2ph_all2all'][0]:
                    # all2all follows either read_all/write_all or read_at_all/write_at_all
                    if   ioprof[rank]['MPI_File_write_all']['Elapsed'] != 0:
                        ioprof[rank]['MPI_File_write_all']['ADIOI_ext2ph_all2all'] -= float(ts)
                    elif ioprof[rank]['MPI_File_write_at_all']['Elapsed'] != 0:
                        ioprof[rank]['MPI_File_write_at_all']['ADIOI_ext2ph_all2all'] -= float(ts)
                    elif ioprof[rank]['MPI_File_read_all']['Elapsed'] != 0:
                        ioprof[rank]['MPI_File_read_all']['ADIOI_ext2ph_all2all'] -= float(ts)
                    elif ioprof[rank]['MPI_File_read_at_all']['Elapsed'] != 0:
                        ioprof[rank]['MPI_File_read_at_all']['ADIOI_ext2ph_all2all'] -= float(ts)
                elif et == MPI_events['MPI_File_close'][0]:
                    # close event
                    ioprof[rank]['MPI_File_close']['Elapsed'] -= float(ts)
                elif et == MPI_events['ADIOI_e10_sync'][0]:
                    # cache sync event
                    ioprof[rank]['ADIOI_e10_sync']['Elapsed'] -= float(ts)
                elif et == MPI_events['ADIOI_e10_sync_read'][0]:
                    # cache read sync event
                    ioprof[rank]['ADIOI_e10_sync']['ADIOI_e10_sync_read'] -= float(ts)
                elif et == MPI_events['ADIOI_e10_sync_write'][0]:
                    # cache write sync event
                    ioprof[rank]['ADIOI_e10_sync']['ADIOI_e10_sync_write'] -= float(ts)
                elif et == MPI_events['MPI_File_close'][1]:
                    # close follows either read_all/write_all or read_at_all/write_at_all
                    ioprof[rank]['MPI_File_close']['Elapsed'] += float(ts)
		    non_hidden_sync = ioprof[rank]['MPI_File_close']['Elapsed']
                    #print 'MPI_File_close(%s): %s'%(rank, ioprof[rank]['MPI_File_close']['Elapsed'])
                    if   ioprof[rank]['MPI_File_write_all']['Elapsed'] != 0:
                        #print 'MPI_File_write_all(%s): %s'%(rank, ioprof[rank]['MPI_File_write_all']['Elapsed'])
                        # write results to file
                        csv.write('MPI_File_write_all'+\
                                  ','+str(rank)+\
                                  ','+str(ioprof[rank]['MPI_File_write_all']['ADIOI_ext2ph_startup'])+\
                                  ','+str(ioprof[rank]['MPI_File_write_all']['ADIOI_ext2ph_all2all'])+\
                                  ','+str(ioprof[rank]['MPI_File_write_all']['ADIOI_ext2ph_isend'])+\
                                  ','+str(ioprof[rank]['MPI_File_write_all']['ADIOI_ext2ph_irecv'])+\
                                  ','+str(ioprof[rank]['MPI_File_write_all']['ADIOI_ext2ph_waitall'])+\
                                  ','+str(ioprof[rank]['MPI_File_write_all']['ADIOI_ext2ph_shuffle'])+\
                                  ','+str(ioprof[rank]['MPI_File_write_all']['read'])+\
                                  ','+str(ioprof[rank]['MPI_File_write_all']['write'])+\
                                  ','+str(ioprof[rank]['MPI_File_write_all']['postwrite'])+\
                                  ','+str(ioprof[rank]['MPI_File_write_all']['Elapsed'])+\
                                  ','+str(ioprof[rank]['ADIOI_e10_sync']['ADIOI_e10_sync_read'])+\
                                  ','+str(ioprof[rank]['ADIOI_e10_sync']['ADIOI_e10_sync_write'])+\
                                  ','+str(ioprof[rank]['ADIOI_e10_sync']['Elapsed'])+\
				  ','+str(non_hidden_sync)+\
                                  '\n')
                        # reset all the counters
                        ioprof[rank]['MPI_File_write_all']['ADIOI_ext2ph_startup'] = 0.0
                        ioprof[rank]['MPI_File_write_all']['ADIOI_ext2ph_all2all'] = 0.0
                        ioprof[rank]['MPI_File_write_all']['ADIOI_ext2ph_isend'] = 0.0
                        ioprof[rank]['MPI_File_write_all']['ADIOI_ext2ph_irecv'] = 0.0
                        ioprof[rank]['MPI_File_write_all']['ADIOI_ext2ph_waitall'] = 0.0
                        ioprof[rank]['MPI_File_write_all']['ADIOI_ext2ph_shuffle'] = 0.0
                        ioprof[rank]['MPI_File_write_all']['read'] = 0.0
                        ioprof[rank]['MPI_File_write_all']['write'] = 0.0
                        ioprof[rank]['MPI_File_write_all']['postwrite'] = 0.0
                        ioprof[rank]['MPI_File_write_all']['Elapsed'] = 0.0
                        ioprof[rank]['ADIOI_e10_sync']['Elapsed'] = 0.0
                        ioprof[rank]['ADIOI_e10_sync']['ADIOI_e10_sync_read'] = 0.0
                        ioprof[rank]['ADIOI_e10_sync']['ADIOI_e10_sync_write'] = 0.0
                    elif ioprof[rank]['MPI_File_write_at_all']['Elapsed'] != 0:
                        #print 'MPI_File_write_at_all(%s): %s'%(rank, ioprof[rank]['MPI_File_write_at_all']['Elapsed'])
                        # write results to file
                        csv.write('MPI_File_write_at_all'+\
                                  ','+str(rank)+\
                                  ','+str(ioprof[rank]['MPI_File_write_at_all']['ADIOI_ext2ph_startup'])+\
                                  ','+str(ioprof[rank]['MPI_File_write_at_all']['ADIOI_ext2ph_all2all'])+\
                                  ','+str(ioprof[rank]['MPI_File_write_at_all']['ADIOI_ext2ph_isend'])+\
                                  ','+str(ioprof[rank]['MPI_File_write_at_all']['ADIOI_ext2ph_irecv'])+\
                                  ','+str(ioprof[rank]['MPI_File_write_at_all']['ADIOI_ext2ph_waitall'])+\
                                  ','+str(ioprof[rank]['MPI_File_write_at_all']['ADIOI_ext2ph_shuffle'])+\
                                  ','+str(ioprof[rank]['MPI_File_write_at_all']['read'])+\
                                  ','+str(ioprof[rank]['MPI_File_write_at_all']['write'])+\
                                  ','+str(ioprof[rank]['MPI_File_write_at_all']['postwrite'])+\
                                  ','+str(ioprof[rank]['MPI_File_write_at_all']['Elapsed'])+\
                                  ','+str(ioprof[rank]['ADIOI_e10_sync']['ADIOI_e10_sync_read'])+\
                                  ','+str(ioprof[rank]['ADIOI_e10_sync']['ADIOI_e10_sync_write'])+\
                                  ','+str(ioprof[rank]['ADIOI_e10_sync']['Elapsed'])+\
				  ','+str(non_hidden_sync)+\
                                  '\n')
                        # reset all the counters
                        ioprof[rank]['MPI_File_write_at_all']['ADIOI_ext2ph_startup'] = 0.0
                        ioprof[rank]['MPI_File_write_at_all']['ADIOI_ext2ph_all2all'] = 0.0
                        ioprof[rank]['MPI_File_write_at_all']['ADIOI_ext2ph_isend'] = 0.0
                        ioprof[rank]['MPI_File_write_at_all']['ADIOI_ext2ph_irecv'] = 0.0
                        ioprof[rank]['MPI_File_write_at_all']['ADIOI_ext2ph_waitall'] = 0.0
                        ioprof[rank]['MPI_File_write_at_all']['ADIOI_ext2ph_shuffle'] = 0.0
                        ioprof[rank]['MPI_File_write_at_all']['read'] = 0.0
                        ioprof[rank]['MPI_File_write_at_all']['write'] = 0.0
                        ioprof[rank]['MPI_File_write_at_all']['postwrite'] = 0.0
                        ioprof[rank]['MPI_File_write_at_all']['Elapsed'] = 0.0
                        ioprof[rank]['ADIOI_e10_sync']['Elapsed'] = 0.0
                        ioprof[rank]['ADIOI_e10_sync']['ADIOI_e10_sync_read'] = 0.0
                        ioprof[rank]['ADIOI_e10_sync']['ADIOI_e10_sync_write'] = 0.0
                    if   ioprof[rank]['MPI_File_read_all']['Elapsed'] != 0:
                        # write results to file
                        csv.write('MPI_File_read_all'+\
                                  ','+str(rank)+\
                                  ','+str(ioprof[rank]['MPI_File_read_all']['ADIOI_ext2ph_startup'])+\
                                  ','+str(ioprof[rank]['MPI_File_read_all']['ADIOI_ext2ph_all2all'])+\
                                  ','+str(ioprof[rank]['MPI_File_read_all']['ADIOI_ext2ph_isend'])+\
                                  ','+str(ioprof[rank]['MPI_File_read_all']['ADIOI_ext2ph_irecv'])+\
                                  ','+str(ioprof[rank]['MPI_File_read_all']['ADIOI_ext2ph_waitall'])+\
                                  ','+str(ioprof[rank]['MPI_File_read_all']['ADIOI_ext2ph_shuffle'])+\
                                  ','+str(ioprof[rank]['MPI_File_read_all']['read']) +\
                                  ',0'+\
                                  ',0'+\
                                  ','+str(ioprof[rank]['MPI_File_read_all']['Elapsed'])+\
                                  ',0'+\
                                  ',0'+\
                                  ',0'+\
                                  '\n')
                        # reset all the counters
                        ioprof[rank]['MPI_File_read_all']['ADIOI_ext2ph_startup'] = 0.0
                        ioprof[rank]['MPI_File_read_all']['ADIOI_ext2ph_all2all'] = 0.0
                        ioprof[rank]['MPI_File_read_all']['ADIOI_ext2ph_isend'] = 0.0
                        ioprof[rank]['MPI_File_read_all']['ADIOI_ext2ph_irecv'] = 0.0
                        ioprof[rank]['MPI_File_read_all']['ADIOI_ext2ph_waitall'] = 0.0
                        ioprof[rank]['MPI_File_read_all']['ADIOI_ext2ph_shuffle'] = 0.0
                        ioprof[rank]['MPI_File_read_all']['read'] = 0.0
                        ioprof[rank]['MPI_File_read_all']['Elapsed'] = 0.0
                    ioprof[rank]['MPI_File_close']['Elapsed'] = 0.0
                elif et == MPI_events['ADIOI_ext2ph_startup'][1]:
                    # startup end event can happen after write*/read*
                    if   ioprof[rank]['MPI_File_write_all']['Elapsed'] != 0:
                        ioprof[rank]['MPI_File_write_all']['ADIOI_ext2ph_startup'] += float(ts)
                    elif ioprof[rank]['MPI_File_write_at_all']['Elapsed'] != 0:
                        ioprof[rank]['MPI_File_write_at_all']['ADIOI_ext2ph_startup'] += float(ts)
                    elif ioprof[rank]['MPI_File_read_all']['Elapsed'] != 0:
                        ioprof[rank]['MPI_File_read_all']['ADIOI_ext2ph_startup'] += float(ts)
                    elif ioprof[rank]['MPI_File_read_at_all']['Elapsed'] != 0:
                        ioprof[rank]['MPI_File_read_at_all']['ADIOI_ext2ph_startup'] += float(ts)
                elif et == MPI_events['MPI_File_write_all'][1]:
                    # write_all end event
                    ioprof[rank]['MPI_File_write_all']['Elapsed'] += float(ts)
                elif et == MPI_events['MPI_File_write_at_all'][1]:
                    # write_at_all end event
                    ioprof[rank]['MPI_File_write_at_all']['Elapsed'] += float(ts)
                elif et == MPI_events['MPI_File_read_all'][1]:
                    # read_all end event
                    ioprof[rank]['MPI_File_read_all']['Elapsed'] += float(ts)
                elif et == MPI_events['MPI_File_read_at_all'][1]:
                    # read_at_all end event
                    ioprof[rank]['MPI_File_read_at_all']['Elapsed'] += float(ts)
                elif et == MPI_events['write'][1]:
                    # write end event can happen after write_all/write_at_all
                    if   ioprof[rank]['MPI_File_write_all']['Elapsed'] != 0:
                        ioprof[rank]['MPI_File_write_all']['write'] += float(ts)
                    elif ioprof[rank]['MPI_File_write_at_all']['Elapsed'] != 0:
                        ioprof[rank]['MPI_File_write_at_all']['write'] += float(ts)
                elif et == MPI_events['postwrite'][1]:
                    # postwrite end event can happen after write_all/write_at_all
                    if   ioprof[rank]['MPI_File_write_all']['Elapsed'] != 0:
                        ioprof[rank]['MPI_File_write_all']['postwrite'] += float(ts)
                    elif ioprof[rank]['MPI_File_write_at_all']['Elapsed'] != 0:
                        ioprof[rank]['MPI_File_write_at_all']['postwrite'] += float(ts)
                elif et == MPI_events['read'][1]:
                    # read end event can happen after read_all/read_at_all
                    if   ioprof[rank]['MPI_File_read_all']['Elapsed'] != 0:
                        ioprof[rank]['MPI_File_read_all']['read'] += float(ts)
                    elif ioprof[rank]['MPI_File_read_at_all']['Elapsed'] != 0:
                        ioprof[rank]['MPI_File_read_at_all']['read'] += float(ts)
                    elif ioprof[rank]['MPI_File_write_all']['Elapsed'] != 0:
                        ioprof[rank]['MPI_File_write_all']['read'] += float(ts)
                    elif ioprof[rank]['MPI_File_write_at_all']['Elapsed'] != 0:
                        ioprof[rank]['MPI_File_write_at_all']['read'] += float(ts)
                elif et == MPI_events['ADIOI_ext2ph_shuffle'][1]:
                    # shuffle end event can happen after read*_all/write*_all
                    if   ioprof[rank]['MPI_File_write_all']['Elapsed'] != 0:
                        ioprof[rank]['MPI_File_write_all']['ADIOI_ext2ph_shuffle'] += float(ts)
                    elif ioprof[rank]['MPI_File_write_at_all']['Elapsed'] != 0:
                        ioprof[rank]['MPI_File_write_at_all']['ADIOI_ext2ph_shuffle'] += float(ts)
                    elif ioprof[rank]['MPI_File_read_all']['Elapsed'] != 0:
                        ioprof[rank]['MPI_File_read_all']['ADIOI_ext2ph_shuffle'] += float(ts)
                    elif ioprof[rank]['MPI_File_read_at_all']['Elapsed'] != 0:
                        ioprof[rank]['MPI_File_read_at_all']['ADIOI_ext2ph_shuffle'] += float(ts)
                elif et == MPI_events['ADIOI_ext2ph_waitall'][1]:
                    # waitall end event can happen after read*_all/write*_all
                    if   ioprof[rank]['MPI_File_write_all']['Elapsed'] != 0:
                        ioprof[rank]['MPI_File_write_all']['ADIOI_ext2ph_waitall'] += float(ts)
                    elif ioprof[rank]['MPI_File_write_at_all']['Elapsed'] != 0:
                        ioprof[rank]['MPI_File_write_at_all']['ADIOI_ext2ph_waitall'] += float(ts)
                    elif ioprof[rank]['MPI_File_read_all']['Elapsed'] != 0:
                        ioprof[rank]['MPI_File_read_all']['ADIOI_ext2ph_waitall'] += float(ts)
                    elif ioprof[rank]['MPI_File_read_at_all']['Elapsed'] != 0:
                        ioprof[rank]['MPI_File_read_at_all']['ADIOI_ext2ph_waitall'] += float(ts)
                elif et == MPI_events['ADIOI_ext2ph_isend'][1]:
                    # isend end event can happen after read*_all/write*_all
                    if   ioprof[rank]['MPI_File_write_all']['Elapsed'] != 0:
                        ioprof[rank]['MPI_File_write_all']['ADIOI_ext2ph_isend'] += float(ts)
                    elif ioprof[rank]['MPI_File_write_at_all']['Elapsed'] != 0:
                        ioprof[rank]['MPI_File_write_at_all']['ADIOI_ext2ph_isend'] += float(ts)
                    elif ioprof[rank]['MPI_File_read_all']['Elapsed'] != 0:
                        ioprof[rank]['MPI_File_read_all']['ADIOI_ext2ph_isend'] += float(ts)
                    elif ioprof[rank]['MPI_File_read_at_all']['Elapsed'] != 0:
                        ioprof[rank]['MPI_File_read_at_all']['ADIOI_ext2ph_isend'] += float(ts)
                elif et == MPI_events['ADIOI_ext2ph_irecv'][1]:
                    # irecv end event can happen after read*_all/write*_all
                    if   ioprof[rank]['MPI_File_write_all']['Elapsed'] != 0:
                        ioprof[rank]['MPI_File_write_all']['ADIOI_ext2ph_irecv'] += float(ts)
                    elif ioprof[rank]['MPI_File_write_at_all']['Elapsed'] != 0:
                        ioprof[rank]['MPI_File_write_at_all']['ADIOI_ext2ph_irecv'] += float(ts)
                    elif ioprof[rank]['MPI_File_read_all']['Elapsed'] != 0:
                        ioprof[rank]['MPI_File_read_all']['ADIOI_ext2ph_irecv'] += float(ts)
                    elif ioprof[rank]['MPI_File_read_at_all']['Elapsed'] != 0:
                        ioprof[rank]['MPI_File_read_at_all']['ADIOI_ext2ph_irecv'] += float(ts)
                elif et == MPI_events['ADIOI_ext2ph_recv'][1]:
                    # recv end event can happen after read*_all/write*_all
                    if   ioprof[rank]['MPI_File_write_all']['Elapsed'] != 0:
                        ioprof[rank]['MPI_File_write_all']['ADIOI_ext2ph_recv'] += float(ts)
                    elif ioprof[rank]['MPI_File_write_at_all']['Elapsed'] != 0:
                        ioprof[rank]['MPI_File_write_at_all']['ADIOI_ext2ph_recv'] += float(ts)
                    elif ioprof[rank]['MPI_File_read_all']['Elapsed'] != 0:
                        ioprof[rank]['MPI_File_read_all']['ADIOI_ext2ph_recv'] += float(ts)
                    elif ioprof[rank]['MPI_File_read_at_all']['Elapsed'] != 0:
                        ioprof[rank]['MPI_File_read_at_all']['ADIOI_ext2ph_recv'] += float(ts)
                elif et == MPI_events['ADIOI_ext2ph_all2all'][1]:
                    # all2all end event can happen after read*_all/write*_all
                    if   ioprof[rank]['MPI_File_write_all']['Elapsed'] != 0:
                        ioprof[rank]['MPI_File_write_all']['ADIOI_ext2ph_all2all'] += float(ts)
                    elif ioprof[rank]['MPI_File_write_at_all']['Elapsed'] != 0:
                        ioprof[rank]['MPI_File_write_at_all']['ADIOI_ext2ph_all2all'] += float(ts)
                    elif ioprof[rank]['MPI_File_read_all']['Elapsed'] != 0:
                        ioprof[rank]['MPI_File_read_all']['ADIOI_ext2ph_all2all'] += float(ts)
                    elif ioprof[rank]['MPI_File_read_at_all']['Elapsed'] != 0:
                        ioprof[rank]['MPI_File_read_at_all']['ADIOI_ext2ph_all2all'] += float(ts)
                elif et == MPI_events['ADIOI_e10_sync'][1]:
                    # cache sync end event
                    ioprof[rank]['ADIOI_e10_sync']['Elapsed'] += float(ts)
                elif et == MPI_events['ADIOI_e10_sync_read'][1]:
                    # cache sync read end event
                    ioprof[rank]['ADIOI_e10_sync']['ADIOI_e10_sync_read'] += float(ts)
                elif et == MPI_events['ADIOI_e10_sync_write'][1]:
                    # cache sync write end event
                    ioprof[rank]['ADIOI_e10_sync']['ADIOI_e10_sync_write'] += float(ts)


if __name__ == "__main__":

    # check the command line input

    parser = OptionParser()
    parser.add_option("-f", "--file", dest="filename",
                      help="clog2 input FILE to be processed", metavar="FILE")

    (options, args) = parser.parse_args()

    logfile = str(options.filename)
    outfile = str(options.filename)+'.csv'

    # create a tmp file to hold the log extracted with clog2_print
    clog2 = open('tmp', 'w+')

    # extract the clog2 file into tmp using clog2_print
    try:
        call(['clog2_print', logfile], stdout=clog2)
    except OSError:
        print '> Error: please ensure that clog2_print is installed in your system'
        exit()

    #print 'processing %s'%logfile

    # get the event tags for every monitored collective IO operation
    clog2.seek(0, 0)
    MPI_events = GetMpiIoEventTags(clog2)
    # print MPI_events

    # get the collective IO aggregators rank (assume icomm=0)
    # TODO: there might be multiple communicators -> need to consider this case
    clog2.seek(0, 0)
    MPI_aggrs = GetMpiIoAggregators(clog2, MPI_events, 0)
    #print '# MPI Aggregators %s' % MPI_aggrs

    # initialize the header of the csv file
    csv = open(outfile, 'w+')
    csv.write(',rank,ext2ph_startup,ext2ph_all2all,ext2ph_send,ext2ph_recv,ext2ph_waitall,ext2ph_shuffle,ext2ph_read,ext2ph_write,ext2ph_postwrite,ext2ph_tot,e10_sync_read,e10_sync_write,e10_sync_tot,nn_hid_sync\n')

    # for every monitored operation extract the timing information
    clog2.seek(0, 0)
    CreatCsvProfilingFile(clog2, csv, 0, MPI_events, MPI_aggrs)

    # finilize the csv file and close it
    csv.close()

    # delete the tmp file
    clog2.close()
    try:
        call(['rm', 'tmp'])
    except OSError:
        print '> Error: cannot remove tmp'
        exit()
