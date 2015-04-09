hrprof or High-Resolution profiler allows to periodically read CPU performance counters 
as they provided by perf subsystem of Linux Kernel. Note that it only supports 
x86 RDPMC assembly instruction. 

Period may be relatively small comparing to 1ms value for system kernel timer thus allowing 
to monitor "quantum" performance effects. It can be used from SystemTap.

Originally written for Linux 3.12, but tested on Linux 4.0. 

### Installation 

- Edit LINUX variable in Makefile so it will point to correct Linux source tree:
```
    LINUX := /root/linux-3.12.12/
```

- Build hrprof module

```
    # make
````

-  Add hrprof symbols to Modules.symvers so it can be recognized by SystemTap
    (you'll need to manually remove it during reinstallation)

```
    # cat Module.symvers >> /lib/modules/$(uname -r)/source/Module.symvers
```

### Usage

- Load hrprof module:
 
```
    # modprobe hrprof hrprof_mode=X
```

Where `hrprof_mode` value is one of:

|  | READ | WRITE |
| --- | --- | --- |
| **PERF_COUNT_HW_CACHE_LL misses** | 1 | 4097 |
| **PERF_COUNT_HW_CACHE_DTLB misses** | 2 | 4098 |
| **PERF_COUNT_HW_CACHE_NODE misses** | 3 | 4099 |
| **PERF_COUNT_HW_STALLED_CYCLES_FRONTEND** | 4 | 4 |

-  Call `extern int hrprof_get(u64* array, int n)` which writes `n` recent values
collected by hrprof to an array. Zero entry of array contains most recent value.

For example, it can be used from SystemTap, like `hrprof.stp` does. That script shows following output:

```
    hrprof 10 177067931499714 281474976180835
    hrprof 10 177067931699714 281474976187729
    hrprof 10 177067931899714 281474976194269
    hrprof 10 177067932099714 281474976201217
    hrprof 10 177067932299714 281474976207632
```

Where second column is CPU id, third column is relative time and last column is performance 
counter value.

NOTE: it assumes that `timer.profile` is called once per 1ms and hrprof_resolution is set to 200 us. 
For another combination of values, alter `n` parameter (now it 1ms / 200us = 5).

 - When finished, unload module
 
```
    # rmmod hrprof
```

### License

Copyright (C) 2013, Sergey Klyaus, ITMO University

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
