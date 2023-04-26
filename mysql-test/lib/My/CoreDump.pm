# -*- cperl -*-
# Copyright (c) 2008, 2013, Oracle and/or its affiliates. All rights reserved.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 2 of the License.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1335  USA

package My::CoreDump;

use strict;
use Carp;
use My::Platform;
use Text::Wrap;
use Data::Dumper;

use File::Temp qw/ tempfile tempdir /;
use File::Find;
use File::Basename;
use mtr_results;
use mtr_report;

my %opts;
my %config;
my $help = "\n\nOptions for printing core dumps\n\n";

sub register_opt($$$) {
  my ($name, $format, $msg)= @_;
  my @names= split(/\|/, $name);
  my $option_name= $names[0];
  $option_name=~ s/-/_/;
  $opts{$name. $format}= \$config{$option_name};
  $help.= wrap(sprintf("  %-23s", join(', ', @names)), ' 'x25, "$msg\n");
}

# To preserve order we use array instead of hash
my @print_formats= (
  short => {
    description => "Failing stack trace",
    codes => {}
  },
  medium => {
    description => "All stack traces",
    codes => {}
  },
  detailed => {
    description => "All stack traces with debug context",
    codes => {}
  },
  custom => {
    description => "Custom debugger script for printing stack"
  },
  # 'no' must be last (check generated help)
  no => {
    description => "Skip stack trace printing"
  }
);

# TODO: make class for each {method, get_code}
my @print_methods= (IS_WINDOWS) ? (cdb => { method => \&_cdb }) : (
  gdb => {
    method => \&_gdb,
    get_code => \&_gdb_format,
  },
  dbx => {
    method => \&_dbx
  },
  lldb => {
    method => \&_lldb
  },
  # 'auto' must be last (check generated help)
  auto => {
    method => \&_auto
  }
);

# But we also use hash
my %print_formats= @print_formats;
my %print_methods= @print_methods;

# and scalar
my $x= 0;
my $print_formats= join(', ', grep { ++$x % 2 } @print_formats);
$x= 0;
my $print_methods=  join(', ', grep { ++$x % 2 } @print_methods);

# Fill 'short' and 'detailed' formats per each print_method
# that has interface for that
for my $f (keys %print_formats)
{
  next unless exists $print_formats{$f}->{codes};
  for my $m (keys %print_methods)
  {
    next unless exists $print_methods{$m}->{get_code};
    # That calls f.ex. _gdb_format('short')
    # and assigns { gdb => value-of-_gdb_format } into $print_formats{short}->{format}: 
    $print_formats{$f}->{codes}->{$m}= $print_methods{$m}->{get_code}->($f);
  }
}

register_opt('print-core|C', ':s',
  "Print core dump format: ". $print_formats. " (for not printing cores). ".
  "Defaults to value of MTR_PRINT_CORE or 'medium'");
if (!IS_WINDOWS)
{
  register_opt('print-method', '=s',
    "Print core method: ". join(', ', $print_methods). " (try each method until success). ".
    "Defaults to 'auto'");
}

sub options() { %opts }
sub help() { $help }


sub env_or_default($$) {
  my ($default, $env)= @_;
  if (exists $ENV{$env}) {
    my $f= $ENV{$env};
    $f= 'custom'
      if $f =~ m/^custom:/;
    return $ENV{$env}
      if exists $print_formats{$f};
    mtr_verbose("$env value ignored: $ENV{$env}");
  }
  return $default;
}

sub pre_setup() {
  $config{print_core}= env_or_default('medium', 'MTR_PRINT_CORE')
    if not defined $config{print_core};
  $config{print_method}= (IS_WINDOWS) ? 'cdb' : 'auto'
    if not defined $config{print_method};
  # If the user has specified 'custom' we fill appropriate print_format
  # and that will be used automatically
  # Note: this can assign 'custom' to method 'auto'.
  if ($config{print_core} =~ m/^custom:(.+)$/) {
    $config{print_core}= 'custom';
    $print_formats{'custom'}= {
      $config{print_method} => $1 
    }
  } 
  mtr_error "Wrong value for --print-core: $config{print_core}"
    if not exists $print_formats{$config{print_core}};
  mtr_error "Wrong value for --print-method: $config{print_method}"
    if not exists $print_methods{$config{print_method}};

  mtr_debug(Data::Dumper->Dump(
    [\%config, \%print_formats, \%print_methods],
    [qw(config print_formats print_methods)]));
}

my $hint_mysqld;		# Last resort guess for executable path

# If path in core file is 79 chars we assume it's been truncated
# Looks like we can still find the full path using 'strings'
# If that doesn't work, use the hint (mysqld path) as last resort.

sub _verify_binpath {
  my ($binary, $core_name)= @_;
  my $binpath;

  if (length $binary != 79) {
    $binpath= $binary;
    print "Core generated by '$binpath'\n";
  } else {
    # Last occurrence of path ending in /mysql*, cut from first /
    if (`strings '$core_name' | grep "/mysql[^/. ]*\$" | tail -1` =~ /(\/.*)/) {
      $binpath= $1;
      print "Guessing that core was generated by '$binpath'\n";
    } else {
      return unless $hint_mysqld;
      $binpath= $hint_mysqld;
      print "Wild guess that core was generated by '$binpath'\n";
    }
  }
  return $binpath;
}


# Returns GDB code according to specified format

# Note: this is like simple hash, separate interface was made
# in advance for implementing below TODO

# TODO: _gdb_format() and _gdb() should be separate class
# (like the other printing methods)

sub _gdb_format($) {
  my ($format)= @_;
  my %formats= (
    short => "bt\n",
    medium => "thread apply all bt\n",
    detailed =>
      "bt\n".
      "set print sevenbit on\n".
      "set print static-members off\n".
      "set print frame-arguments all\n".
      "thread apply all bt full\n".
      "quit\n"
  );
  confess "Unknown format: ". $format
    unless exists $formats{$format};
  return $formats{$format};
}


sub _gdb {
  my ($core_name, $code)= @_;
  confess "Undefined format"
    unless defined $code;

  # Check that gdb exists
  `gdb --version`;
  if ($?) {
    print "gdb not found, cannot get the stack trace\n";
    return;
  }

  if (-f $core_name) {
    mtr_verbose("Trying 'gdb' to get a backtrace from coredump $core_name");
  } else {
    print "\nCoredump $core_name does not exist, cannot run 'gdb'\n";
    return;
  }

  # Find out name of binary that generated core
  `gdb -c '$core_name' --batch 2>&1` =~
    /Core was generated by `([^\s\'\`]+)/;
  my $binary= $1 or return;

  $binary= _verify_binpath ($binary, $core_name) or return;

  # Create tempfile containing gdb commands
  my ($tmp, $tmp_name) = tempfile();
  print $tmp $code;
  close $tmp or die "Error closing $tmp_name: $!";

  # Run gdb
  my $gdb_output=
    `gdb '$binary' -c '$core_name' -x '$tmp_name' --batch 2>&1`;

  unlink $tmp_name or die "Error removing $tmp_name: $!";

  return if $? >> 8;
  return unless $gdb_output;

  resfile_print <<EOF . $gdb_output . "\n";
Output from gdb follows. The first stack trace is from the failing thread.
The following stack traces are from all threads (so the failing one is
duplicated).
--------------------------
EOF
  return 1;
}


sub _dbx {
  my ($core_name, $format)= @_;

  print "\nTrying 'dbx' to get a backtrace\n";

  return unless -f $core_name;

  # Find out name of binary that generated core
  `echo | dbx - '$core_name' 2>&1` =~
    /Corefile specified executable: "([^"]+)"/;
  my $binary= $1 or return;

  $binary= _verify_binpath ($binary, $core_name) or return;

  # Find all threads
  my @thr_ids = `echo threads | dbx '$binary' '$core_name' 2>&1` =~ /t@\d+/g;

  # Create tempfile containing dbx commands
  my ($tmp, $tmp_name) = tempfile();
  foreach my $thread (@thr_ids) {
    print $tmp "where $thread\n";
  }
  print $tmp "exit\n";
  close $tmp or die "Error closing $tmp_name: $!";

  # Run dbx
  my $dbx_output=
    `cat '$tmp_name' | dbx '$binary' '$core_name' 2>&1`;

  unlink $tmp_name or die "Error removing $tmp_name: $!";

  return if $? >> 8;
  return unless $dbx_output;

  resfile_print <<EOF .  $dbx_output . "\n";
Output from dbx follows. Stack trace is printed for all threads in order,
above this you should see info about which thread was the failing one.
----------------------------
EOF
  return 1;
}


# Check that Debugging tools for Windows are installed
sub cdb_check {
   `cdb -? 2>&1`;
  if ($? >> 8)
  {
    print "Cannot find the cdb debugger. Please install Debugging tools for Windows\n";
    print "and set PATH environment variable to include location of cdb.exe";
  }
}


sub _cdb {
  my ($core_name, $format)= @_;
  print "\nTrying 'cdb' to get a backtrace\n";
  return unless -f $core_name;
  # Read module list, find out the name of executable and 
  # build symbol path (required by cdb if executable was built on 
  # different machine)
  my $tmp_name= $core_name.".cdb_lmv";
  `cdb -z $core_name -c \"lmv;q\" > $tmp_name 2>&1`;
  if ($? >> 8)
  {
    unlink($tmp_name);
    # check if cdb is installed and complain if not
    cdb_check();
    return;
  }
  
  open(temp,"< $tmp_name");
  my %dirhash=();
  while(<temp>)
  {
    if($_ =~ /Image path\: (.*)/)
    {
      if (rindex($1,'\\') != -1)
      {
        my $dir= substr($1, 0, rindex($1,'\\'));
        $dirhash{$dir}++;
      }
    }
  }
  close(temp);
  unlink($tmp_name);
  
  my $image_path= join(";", (keys %dirhash),".");

  # For better callstacks, setup _NT_SYMBOL_PATH to include
  # OS symbols. Note : Dowloading symbols for the first time 
  # can take some minutes
  if (!$ENV{'_NT_SYMBOL_PATH'})
  {
    my $windir= $ENV{'windir'};
    my $symbol_cache= substr($windir ,0, index($windir,'\\'))."\\symbols";

    print "OS debug symbols will be downloaded and stored in $symbol_cache.\n";
    print "You can control the location of symbol cache with _NT_SYMBOL_PATH\n";
    print "environment variable. Please refer to Microsoft KB article\n";
    print "http://support.microsoft.com/kb/311503  for details about _NT_SYMBOL_PATH\n";
    print "-------------------------------------------------------------------------\n";

    $ENV{'_NT_SYMBOL_PATH'}.= 
      "srv*".$symbol_cache."*http://msdl.microsoft.com/download/symbols";
  }
  
  my $symbol_path= $image_path.";".$ENV{'_NT_SYMBOL_PATH'};
  
  
  # Run cdb. Use "analyze" extension to print crashing thread stacktrace
  # and "uniqstack" to print other threads

  my $cdb_cmd = "!sym prompts off; !analyze -v; .ecxr; !for_each_frame dv /t;!uniqstack -p;q";
  my $cdb_output=
    `cdb -c "$cdb_cmd" -z $core_name -i "$image_path" -y "$symbol_path" -t 0 -lines 2>&1`;
  return if $? >> 8;
  return unless $cdb_output;
  
  # Remove comments (lines starting with *), stack pointer and frame 
  # pointer adresses and offsets to function to make output better readable
  $cdb_output=~ s/^\*.*\n//gm;   
  $cdb_output=~ s/^([\:0-9a-fA-F\`]+ )+//gm; 
  $cdb_output=~ s/^ChildEBP RetAddr//gm;
  $cdb_output=~ s/^Child\-SP          RetAddr           Call Site//gm;
  $cdb_output=~ s/\+0x([0-9a-fA-F]+)//gm;
  
  resfile_print <<EOF . $cdb_output . "\n";
Output from cdb follows. Faulting thread is printed twice,with and without function parameters
Search for STACK_TEXT to see the stack trace of 
the faulting thread. Callstacks of other threads are printed after it.
EOF
  return 1;
}


sub _lldb
{
  my ($core_name)= @_;

  print "\nTrying 'lldb' to get a backtrace from coredump $core_name\n";

  # Create tempfile containing lldb commands
  my ($tmp, $tmp_name)= tempfile();
  print $tmp
    "bt\n",
    "thread backtrace all\n",
    "quit\n";
  close $tmp or die "Error closing $tmp_name: $!";

  my $lldb_output= `lldb -c '$core_name' -s '$tmp_name' 2>&1`;

  unlink $tmp_name or die "Error removing $tmp_name: $!";

  if ($? == 127)
  {
    print "lldb not found, cannot get the stack trace\n";
    return;
  }

  return if $?;
  return unless $lldb_output;

  resfile_print <<EOF . $lldb_output . "\n";
Output from lldb follows. The first stack trace is from the failing thread.
The following stack traces are from all threads (so the failing one is
duplicated).
--------------------------
EOF
  return 1;
}


sub _auto
{
  my ($core_name, $code, $rest)= @_;
  # We use ordered array @print_methods and omit auto itself
  my @valid_methods= @print_methods[0 .. $#print_methods - 2];
  my $x= 0;
  my @methods= grep { ++$x % 2} @valid_methods;
  my $f= $config{print_core};
  foreach my $m (@methods)
  {
    my $debugger= $print_methods{$m};
    confess "Broken @print_methods"
      if $debugger->{method} == \&_auto;
    # If we didn't find format for 'auto' (that is only possible for 'custom')
    # we get format for specific debugger
    if (not defined $code && defined $print_formats{$f} and
        exists $print_formats{$f}->{codes}->{$m})
    {
      $code= $print_formats{$f}->{codes}->{$m};
    }
    mtr_verbose2("Trying to print with method ${m}:${f}");
    if ($debugger->{method}->($core_name, $code)) {
      return;
    }
  }
}


sub show {
  my ($core_name, $exe_mysqld, $parallel)= @_;
  if ($config{print_core} ne 'no') {
    my $f= $config{print_core};
    my $m= $config{print_method};
    my $code= undef;
    if (exists $print_formats{$f}->{codes} and
        exists $print_formats{$f}->{codes}->{$m}) {
      $code= $print_formats{$f}->{codes}->{$m};
    }
    mtr_verbose2("Printing core with method ${m}:${f}");
    mtr_debug("code: ${code}");
    $print_methods{$m}->{method}->($core_name, $code);
  }
  return;
}


sub core_wanted($$$$$) {
  my ($num_saved_cores, $opt_max_save_core, $compress,
      $exe_mysqld, $opt_parallel)= @_;
  my $core_file= $File::Find::name;
  my $core_name= basename($core_file);

  # Name beginning with core, not ending in .gz
  if (($core_name =~ /^core/ and $core_name !~ /\.gz$/)
      or (IS_WINDOWS and $core_name =~ /\.dmp$/))
  {
    # Ending with .dmp
    mtr_report(" - found '$core_name'",
               "($$num_saved_cores/$opt_max_save_core)");

    show($core_file, $exe_mysqld, $opt_parallel);

    # Limit number of core files saved
    if ($$num_saved_cores >= $opt_max_save_core)
    {
      mtr_report(" - deleting it, already saved",
                 "$opt_max_save_core");
      unlink("$core_file");
    }
    else
    {
      main::mtr_compress_file($core_file) if $compress;
      ++$$num_saved_cores;
    }
  }
}


1;
