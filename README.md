# V6FileSystem
Implementation of V6 File System
 
 V6 file system is highly restrictive. A modification has been done: Block size is 1024 Bytes, i-node
size is 64 Bytes and i-node’s structure has been modified as well.
The following command has been implemented as well and the code for that is available for you to
use (and start from):

$ initfs fname n1 n2

where fname is the name of the (external) file in your Unix machine that represents the V6 file system.
n1 is the number of blocks in the disk (fsize) and n2 is the total number of i-nodes.
This command initializes the file system. All data blocks are in the free list (except for one data
blosk that is allocated to the root /. An example is: initfs /user/venky/disk 8000 300
You need to implement the following commands:

(a) cpin externalfile v6-file
Creat a new file called v6-file in the v6 file system and fill the contents of the newly created
file with the contents of the externalfile.

(b) cpout v6-file externalfile
If the v6-file exists, create externalfile and make the externalfile's contents equal to v6-file.

(c) mkdir v6-dir
If v6-dir does not exist, create the directory and set its first two entries . and ..

(d) rm v6-file
If v6-file exists, delete the file, free the i-node, remove the file name from the
(parent) directory that has this file and add all data blocks of this file to the free list

(e) ls
List the contents of the current directory.

(f) pwd
List the fill pathname of the current directory

(g) cd dirname
change current (working) directory to the dirname

(h) rmdir dir
Remove the directory specified (dir, in this case).

(i) open filename
Open the external file filename, which has a v6 filesystem installed.

(j) q
Save all changes and quit.


Keep in mind that all file names not starting with / are absolute path names and those not starting
with / are relative to current working directory
