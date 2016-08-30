#!/usr/bin/python
#########################################################
# Author: Giuseppe Congiu <giuseppe.congiu@seagate.com> #
# Name:   summarize_clogs.py                            #
#########################################################
import os
import re
import numpy
from optparse import OptionParser

if __name__ == "__main__":

    # check the command line input
    parser = OptionParser()
    parser.add_option("-f", "--file", dest="filename",
                      help="file of clog2 file names", metavar="FILE")
    parser.add_option("-m", "--mode", dest="mode",
		      help="compute csv(s) and summary (all) or only summary (sum)")
    (options, args) = parser.parse_args()

    # name of clog2 files
    files = open(options.filename)
    mode = options.mode

    # names of csv files generated by 'from_clog2_to_csv.py'
    csv = []

    # create the csv file for every clog2 file
    for line in files:
        csv.append('%s.csv'%line.strip())
        if mode == 'all' and os.path.exists(line.strip()):
            print 'processing %s'%line.strip()
            os.system('/homec/deep/deep47/scripts/from_clog2_to_csv.py -f %s'%line.strip())

    # close files
    files.close()

    # open the final summary file
    summary = open('summary.csv', 'w+')

    summary.write(',startup_read,shuffle_all2all,shuffle_send,shuffle_recv,shuffle_waitall,shuffle_read,read,startup_write,shuffle_all2all,shuffle_send,shuffle_recv,shuffle_waitall,shuffle_write,write,post_write,tot,write_std,sync_read,sync_write,sync,non_hid_sync\n')

    # for each file compute mean time for reads & writes
    # Collective Read & Write operations (include all the components)
    read_max  = {'startup': 0.0, 'all2all': 0.0, 'send': 0.0, 'recv': 0.0, 'waitall': 0.0, 'exch': 0.0, 'read': 0.0, 'tot': 0.0}
    read      = {'startup': 0.0, 'all2all': 0.0, 'send': 0.0, 'recv': 0.0, 'waitall': 0.0, 'exch': 0.0, 'read': 0.0, 'tot': 0.0, 'num': 0}
    write_max = {'startup': 0.0, 'all2all': 0.0, 'send': 0.0, 'recv': 0.0, 'waitall': 0.0, 'exch': 0.0, 'write': 0.0, 'post_write': 0.0, 'non_hid_sync': 0.0, 'tot': 0.0}
    write     = {'startup': 0.0, 'all2all': 0.0, 'send': 0.0, 'recv': 0.0, 'waitall': 0.0, 'exch': 0.0, 'write': 0.0, 'post_write': 0.0, 'non_hid_sync': 0.0, 'tot': 0.0, 'num': 0}
    sync_max  = {'read': 0.0, 'write': 0.0, 'tot': 0.0}
    sync      = {'read': 0.0, 'write': 0.0, 'tot': 0.0}

    # POSIX Read & Write operations (only include POSIX read & write)
    rd        = 0.0
    wr        = 0.0
    rd_max    = 0.0
    wr_max    = 0.0
    rd_min    = 10000.0
    wr_min    = 10000.0
    rd_avg    = 0.0
    wr_avg    = 0.0
    rd_std    = 0.0
    wr_std    = 0.0
    wr_buf    = [] #contains write times to compute variance
    rd_buf    = []

    aggregator = -1 # aggregator number
    aggregators = 0 # tot number of aggregators
    count_wr = 0    # parsed number of aggregators
    count_rd = 0

    line_regex = re.compile('([A-Za-z\_]+),([0-9]+),([0-9\.\-e]+),([0-9\.\-e]+),([0-9\.\-e]+),([0-9\.\-e]+),([0-9\.\-e]+),([0-9\.\-e]+),([0-9\.\-e]+),([0-9\.\-e]+),([0-9\.\-e]+),([0-9\.\-e]+),([0-9\.\-e]+),([0-9\.\-e]+),([0-9\.\-e]+),([0-9\.\-e]+)')

    # we need to workout for each collective write round the slowest aggregator. If there are multiple rounds
    # the slowest aggregator of each round is used to compute an average elapsed value.
    for file in csv:
        m = re.search('(.+).pbs.clog2.csv', file)
        test = m.group(1)
        m = re.search('[a-z]+\_(\d+)\_.+', file)
        aggregators = int(m.group(1))
        fh = open(file)
        for line in fh:
            #print line
            res = re.match(line_regex, line)
            if res is not None and aggregator == -1:
                aggregator = res.group(2)
            if res is not None and (res.group(1) == 'MPI_File_write_all' or res.group(1) == 'MPI_File_write_at_all'):
                #print res.group(1)+' '+res.group(2)+' '+res.group(3)+' '+res.group(4)+' '+res.group(5)+' '+res.group(6)+' '+res.group(7)+' '+res.group(8)+' '+res.group(9)+' '+res.group(10)+' '+res.group(11)
                io_time = float(res.group(12)) + float(res.group(16))
                if count_wr < aggregators:
                    if io_time > (write_max['tot']+write_max['non_hid_sync']) and float(res.group(4)) > write_max['all2all'] :
                        write_max['startup']   = float(res.group(3))
                        write_max['all2all']   = float(res.group(4))
                        write_max['send']      = float(res.group(5))
                        write_max['recv']      = float(res.group(6))
                        write_max['waitall']   = float(res.group(7))
                        write_max['exch']      = float(res.group(8))
                        write_max['write']     = float(res.group(10))
                        write_max['post_write']= float(res.group(11))
                        write_max['tot']       = float(res.group(12))
                        sync_max['read']       = float(res.group(13))
                        sync_max['write']      = float(res.group(14))
                        sync_max['tot']        = float(res.group(15))
                        write_max['non_hid_sync'] = float(res.group(16))
                    # compute posix write min and max
                    #if float(res.group(10)) > wr_max:
                    #    wr_max = float(res.group(10))
                    #if float(res.group(10)) < wr_min:
                    #    wr_min = float(res.group(10))
                    # compute posix write avg
                    #wr_avg += float(res.group(10)) / aggregators
                    wr_buf.append(float(res.group(10)))
                    # increment aggregator count
                    count_wr += 1
                # if we checked the last aggregator ...
                if count_wr == aggregators:
                    # compute variance for write
                    wr_std += numpy.std(wr_buf)
                    write['startup']   += write_max['startup']
                    write['all2all']   += write_max['all2all']
                    write['send']      += write_max['send']
                    write['recv']      += write_max['recv']
                    write['waitall']   += write_max['waitall']
                    write['exch']      += write_max['exch']
                    write['write']     += write_max['write']
                    write['post_write']+= write_max['post_write']
                    write['tot']       += write_max['tot']
                    write['non_hid_sync'] += write_max['non_hid_sync']
                    write['num']       += 1
                    sync['read']       += sync_max['read']
                    sync['write']      += sync_max['write']
                    sync['tot']        += sync_max['tot']
                    write_max['startup']   = 0
                    write_max['all2all']   = 0
                    write_max['send']      = 0
                    write_max['recv']      = 0
                    write_max['waitall']   = 0
                    write_max['exch']      = 0
                    write_max['write']     = 0
                    write_max['post_write']= 0
                    write_max['non_hid_sync']  = 0
                    write_max['tot']       = 0
                    sync_max['read']       = 0
                    sync_max['write']      = 0
                    sync_max['tot']        = 0
                    count_wr               = 0
                    wr_buf                 = []
            elif res is not None and (res.group(1) == 'MPI_File_read_all' or res.group(1) == 'MPI_File_read_at_all'):
                #print res.group(1)+' '+res.group(2)+' '+res.group(3)+' '+res.group(4)+' '+res.group(5)+' '+res.group(6)+' '+res.group(7)
                elapsed = float(res.group(12))
                if count_rd < aggregators:
                    if float(res.group(12)) > read_max['tot'] and float(res.group(4)) > read_max['all2all']:
                        read_max['startup'] = float(res.group(3))
                        read_max['all2all'] = float(res.group(4))
                        read_max['send']    = float(res.group(5))
                        read_max['recv']    = float(res.group(6))
                        read_max['waitall'] = float(res.group(7))
                        read_max['exch']    = float(res.group(8))
                        read_max['read']    = float(res.group(9))
                        read_max['tot']     = elapsed
                    count_rd += 1
                # if we checked the last aggregator ...
                if count_rd == aggregators:
                    read['startup'] += read_max['startup']
                    read['all2all'] += read_max['all2all']
                    read['send']    += read_max['send']
                    read['recv']    += read_max['recv']
                    read['waitall'] += read_max['waitall']
                    read['exch']    += read_max['exch']
                    read['read']    += read_max['read']
                    read['tot']     += read_max['tot']
                    read['num']     += 1
                    read_max['startup'] = 0
                    read_max['all2all'] = 0
                    read_max['send']    = 0
                    read_max['recv']    = 0
                    read_max['waitall'] = 0
                    read_max['exch']    = 0
                    read_max['read']    = 0
                    read_max['tot']     = 0
                    count_rd            = 0
        fh.close()
        # now write the mean to the summary file
        print 'writing %s summary ...'%test

        # fix zero division cases
        if read['num'] == 0:
            read['num'] = 1
        elif write['num'] == 0:
            write['num'] = 1

        summary.write(test+\
                  ','+str(read['startup'] / read['num'])+\
                  ','+str(read['all2all'] / read['num'])+\
                  ','+str(read['send'] / read['num'])+\
                  ','+str(read['recv'] / read['num'])+\
                  ','+str(read['waitall'] / read['num'])+\
                  ','+str(read['exch'] / read['num'])+\
                  ','+str(read['read'] / read['num'])+\
                  ','+str(write['startup'] / write['num'])+\
                  ','+str(write['all2all'] / write['num'])+\
                  ','+str(write['send'] / write['num'])+\
                  ','+str(write['recv'] / write['num'])+\
                  ','+str(write['waitall'] / write['num'])+\
                  ','+str(write['exch'] / write['num'])+\
                  ','+str(write['write'] / write['num'])+\
                  ','+str(write['post_write'] / write['num'])+\
                  ','+str(write['tot'] / write['num'])+\
                  ','+str(wr_std / write['num'])+\
                  ','+str(sync['read'] / write['num'])+\
                  ','+str(sync['write'] / write['num'])+\
                  ','+str(sync['tot'] / write['num'])+\
                  ','+str(write['non_hid_sync'] / write['num'])+\
                  '\n')

        read['startup'] = 0.0
        read['all2all'] = 0.0
        read['send'] = 0.0
        read['recv'] = 0.0
        read['waitall'] = 0.0
        read['exch'] = 0.0
        read['read'] = 0.0
        read['tot'] = 0.0
        read['num'] = 0
        write['startup'] = 0.0
        write['all2all'] = 0.0
        write['send'] = 0.0
        write['recv'] = 0.0
        write['waitall'] = 0.0
        write['exch'] = 0.0
        write['write'] = 0.0
        write['post_write'] = 0.0
        write['non_hid_sync'] = 0.0
        write['tot'] = 0
        write['num'] = 0
        read['tot'] = 0.0
        read['num'] = 0
        sync['read'] = 0.0
        sync['write'] = 0.0
        sync['tot'] = 0.0
        wr_std = 0.0
        aggregator = -1

    summary.close()
