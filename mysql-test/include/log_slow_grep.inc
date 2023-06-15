# Common extensions to the slow query log
--let grep_pattern = ^# Thread_id: .+ Schema: .+  QC_hit: (Yes|No)\$
--let log_expected_matches = $log_slow_verbosity_expected_matches
--source include/log_grep.inc
--let grep_pattern = ^# Query_time: \d+\.\d+  Lock_time: \d+\.\d+  Rows_sent: \d+  Rows_examined: \d+\$
--source include/log_grep.inc
 --let grep_pattern = ^# Rows_affected: \d+  Bytes_sent: \d+\$
--source include/log_grep.inc

# Query plan
--let log_expected_matches = $log_slow_verbosity_queryplan_expected_matches
--let grep_pattern = ^# Full_scan: (Yes|No)  Full_join: (Yes|No)  Tmp_table: (Yes|No)  Tmp_table_on_disk: (Yes|No)\$
--source include/log_grep.inc
--let grep_pattern = ^# Filesort: (Yes|No)  Filesort_on_disk: (Yes|No)  Merge_passes: \d+\ Priority_queue: (Yes|No)\$
--source include/log_grep.inc

# Temp tables
--let log_expected_matches = $log_slow_verbosity_tmptable_expected_matches
--source include/log_grep.inc
--let grep_pattern = ^# Tmp_tables: \d+  Tmp_disk_tables: \d+\$
--source include/log_grep.inc

# InnoDB/Engines
--let log_expected_matches = $log_slow_innodb_expected_matches
--let grep_pattern = ^# Pages_accessed: [1-9]\d*  Pages_reads: [1-9]\d*  Pages_updated: [1-9]\d*\n# Pages_io_time: [0-9]\d*\.\d+  Engine_time: [0-9]\d*\.\d+\$
