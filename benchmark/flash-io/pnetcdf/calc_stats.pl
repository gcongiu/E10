#!/usr/bin/perl


if(!defined $ARGV[0]) {
	print "Usage: fbio_stats <filename> <range>\n";
	exit;
	}

if($ARGV[1] !~ /\d-\d+/) {
	print "Usage: fbio <filename> <range>\n";
	exit;
	}
$ARGV[0] =~ tr/\n//d;
$ARGV[1] =~ tr/\n//d;

$filebase = $ARGV[0];

open(FILE, "$filebase.SIZES");
@lsizes = <FILE>;
close(FILE);

@file_type = ("chk","cnt","crn");
$i=0;
for($i=1; $i<=3; $i++) {
	$lsizes[$i] =~ s/^\s+//g;
	$lsizes[$i] =~ s/\s+/:/g;
	@ssizes = split(/:/, $lsizes[$i]);
	$ssizes[0] =~ tr/\n//d;
	$SIZES{"$file_type[$i-1]"} = $ssizes[0];
	}

@r = split(/-/ , $ARGV[1]);

for($i=$r[0]; $i<=$r[1]; $i++) {

	@data = `/bin/grep "time to output" $filebase.$i |/usr/bin/cut -d= -f2`;
	$data[0] =~ s/\s+//g;
	$data[0] =~ tr/\n//d;
	$DATA{"chk.$i"} = $data[0];
	$data[1] =~ s/\s+//g;
	$data[1] =~ tr/\n//d;
	$DATA{"pltcrn.$i"} = $data[1];
	$data[2] =~ s/\s+//g;
        $data[2] =~ tr/\n//d;
        $DATA{"pltcnt.$i"} = $data[2];
	}


$chk_tot = 0;
$plt_crn = 0;
$plt_cnt = 0;
for($i=$r[0]; $i<=$r[1]; $i++) {

	$chk_tot = $chk_tot + $DATA{"chk.$i"};
	open(FILE, ">>$filebase.chk.plot");
	print FILE "$i	$DATA{\"chk.$i\"}\n";
	close(FILE);
	$plt_crn = $plt_crn + $DATA{"pltcrn.$i"};
	open(FILE, ">>$filebase.crn.plot");
        print FILE "$i	$DATA{\"pltcrn.$i\"}\n";
        close(FILE);
	$plt_cnt = $plt_cnt + $DATA{"pltcnt.$i"};
	open(FILE, ">>$filebase.cnt.plot");
        print FILE "$i	$DATA{\"pltcnt.$i\"}\n";
        close(FILE);

	}

$count = $r[1] + 1;
$chk_avg = $chk_tot/$count;
$plt_crn_avg = $plt_crn/$count;
$plt_cnt_avg = $plt_cnt/$count;
$bytes_chk = $SIZES{"chk"}/$chk_avg;
$bytes_cnt = $SIZES{"cnt"}/$plt_cnt_avg;
$bytes_crn = $SIZES{"crn"}/$plt_crn_avg;
print "Totals:\n";
print "_________________________________________________________________________\n\n\n";
print "Checkpoint bytes/sec: $bytes_chk\n";
print "Plotfile Corners bytes/sec: $bytes_crn\n";
print "Plotfile Centered bytes/sec: $bytes_cnt\n";
print "\n\n";
print "Checkpoint Time Average out of $count: $chk_avg\n";
print "Plotfile corners Time Average out of $count: $plt_crn_avg\n";
print "Plotfile centered Time Average out of $count: $plt_cnt_avg\n";
print "\n\n\n";
exit;


