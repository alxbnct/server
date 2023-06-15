/*****************************************************************************

Copyright (c) 2023, MariaDB Fundation

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1335 USA

*****************************************************************************/

#ifndef mariadb_stats_h
#define mariadb_stats_h

/* Include file to handle mariadbd handler specific stats */

#include "ha_handler_stats.h"
#include "my_rdtsc.h"

extern thread_local ha_handler_stats *mariadb_stats;

inline void mariadb_increment_pages_accessed()
{
  ha_handler_stats *stats= mariadb_stats;
  if (stats)
    stats->pages_accessed++;
}

inline void mariadb_increment_pages_updated(ulonglong count)
{
  ha_handler_stats *stats= mariadb_stats;
  if (stats)
    stats->pages_updated+= count;
}

inline void mariadb_increment_pages_read()
{
  ha_handler_stats *stats= mariadb_stats;
  if (stats)
    stats->pages_read_count++;
}

/*
  The following has to be identical code as measure() in sql_analyze_stmt.h
*/

inline ulonglong mariadb_measure()
{
#if (MY_TIMER_ROUTINE_CYCLES)
    return my_timer_cycles();
#else
    return my_timer_microseconds();
#endif
}

inline void mariadb_increment_pages_read_time(ulonglong start_time)
{
  ha_handler_stats *stats= mariadb_stats;
  if (likely(stats))
  {
    ulonglong end_time= mariadb_measure();
    stats->pages_read_time+= (end_time - start_time);
  }
}

class mariadb_set_stats
{
public:
  uint flag;
  mariadb_set_stats(ha_handler_stats *stats)
  {
    mariadb_stats= stats;
  }
  ~mariadb_set_stats()
  {
    mariadb_stats= 0;
  }
};

#endif /* mariadb_stats_h */
