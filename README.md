# High Level Implementation



## Tracking device location

`include/linux/gps.h` : 

- Definition of `struct gps_location`

- export `GLOBAL_GPS`, `rwlock`

`kernel/gps.c` : 

- Define and initialize global variable `GLOBAL_GPS` containing current gps location information

- Define and initialize `rwlock` for concurrent access of `GLOBAL_GPS`

- Define `set_gps_location` :

  - modify `GLOBAL_GPS` under `rwlock`

  - consider validity of user input gps location(lat, lng)

  - consider error cases

`test/gpsupdate.c` :

- Get 5 int arguments and make `gps_location` from it, then feed it to system call

- consider error cases

## Add GPS-related operations to inode

`include/linux/fs.h` :

- Prototype of gps inode operations `set_gps_location`, `get_gps_location`

`fs/ext2/ext2.h` :

- include gps-related fields in `ext2_inode`

- include gps-related fields in `ext2_inode_info`

- export `ext2_set_gps_location` and `ext2_get_gps_location`

`fs/ext2/file.c` :

- assign `ext2_set_gps_location`and `get_gps_location` for `struct inode_operations`, `set_gps_location` and `get_gps_location`

`fs/ext2/inode.c` :

- load location fields from `ext2_inode`(disk) to `ext2_inode_info`(memory)

  - cpu inherent endian to fixed endian using `le32_to_cpu`

- save location fields from `ext2_inode_info`(memory) to `ext2_inode`(disk) 

  - fixed endian to cpu inherent endian using `cpu_to_le32`

- Define `ext2_set_gps_location` :

  - readlock using `rwlock`

  - save `GLOBAL_GPS` data to `ext2_inode_info`(memory)

- Define `ext2_get_gps_location` :

  - get location data from `ext2_inode_info`(memory)



## Update location information for files

`fs/ext2/file.c` :

- update gps information on file write(==modify) by applying inode update on `ext2_file_operations.write`(do_sync_write) and create `ext2_file_write`

- make `ext2_file_write` new write method for `ext2_file_operations`

`fs/ext2/namei.c` :

- fill gps fields for inode on `ext2_create`(function creating inode for a new file in a directory)



## User-space testing for location information

`e2fsprogs/` :

- Do as spec says

`kernel/gps.c` :

- Define `get_gps_location` :

  - Bring path to find file and its inode, then get gps location from it and `copy_to_user`

  - consider error cases EACCESS, ENODEV, EINVAL, EFAULT, ENOMEM

`test/file_loc.c` :

- get path from argument and pass to syscall with empty space of gps location

- get gps location and print it and its google map link

- consider error cases of its own and syscall. system call error return stored at global variable errno, with sign changed. system call itself returns only -1 when error is returned.

`test/Makefile` : 

- make all at the top for a single 'make' to compile both `file_loc.c` and `gpsupdate.c`



## Location-baced file access

`fs/ext2/file.c` :

- check file permission by applying gps location comparison on `generic_permission` and create `ext2_permission`

- make `ext2_permission` new permission method for `ext2_inode_operations` (ext2 - layer implementation)

`include/linux/gps.h` :

- export `nearby_created_area` which fetches location info from inode and compare distance with `GLOBAL_GPS`

`kernel/gps.c` :

- Define `nearby_created_area` :

  - Define `struct gps_float` which resembles float type, but has signed int part and unsigned and bounded (0 ~ 999999) fractional part

  - Define initialization and arithmetics addition, subtraction, multiplication, division for `gps_float`

    - precision error less than 10^-5

  - Define advanced functions degree2rad, sin(radian), cos(radian), arccos(radian), PI for `gps_float`

    - Taylor expansion for transcendental function approximation, since only polynomial expression was allowed.

    - Taylor expansion formula for sin(), cos() and formula for 180 degree to radian transition.
    
    - arccos() is approximated to the Lagrange polynomial interpolating (0, 1), (pi/3, 1/2), (pi/2, 0), (2pi/3, -1/2), (pi, -1).

  - Define `cmp_inode_global` which computes distance between **inode** `gps_location` and `GLOBAL_GPS` with `gps_float` methods and Compare with accuracy to determine nearbyness

    - Mathematical Assumption : By [Spherical Law of Cosines](https://en.wikipedia.org/wiki/Spherical_law_of_cosines), we can compute the distance between two point on the sphere. 
    - Implementation : If we assume the point C of spherical triangle on the unit sphere as the pole, then *cos c = cos a cos b + sin a sin b cos C*. Because C is the pole, *a = pi/2 - lat1*, *b = pi/2 - lat2*, and c is the central angle between A and B. Also, we can approximate that *C = lng1 - lng2*. Then we can rearrange the formular to *cos c = sin(lat1) sin(lat2) + cos(lat1) cos(lat2) cos(lng1 - lng2)*. We can compute c after doing arccos() on the above result, then we can finally get the actual distance by multiplying the Eath radius(about 6371000 m).

  - fetch location info from inode and feed it to `cmp_inode_global`



# Evaluation

# Demo

# Lessons Learned

- Creating float and its arithmetics with only int was extremely painful. Overflow on multiplication and division was dealt with using u64 and math64.h 64 bit int div operations.

- implementing location info write and permission was done on ext2 layer, since if we implement in lower layer like in `vfs_write`, unknown dependency from other file system may exist and corrupt the whole file system of artik device, and it is less complicated.

- Always find an easiest way and easiest solution. If you cannot explain why, you don't know it.

- Always google it if you don't know the solution. Your brain is limited, and there are thousands who already have thought your 'new idea'.

- Always check error after a procedure of unknown implementation is called.

- If you don't know how to use it, just follow examples and conventions.


