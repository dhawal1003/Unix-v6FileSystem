# Unix-v6FileSystem


Unix v6 FileSystem which supports file transfers upto 32 mb, which otherwise had support only for 16 mb.

In order to compile the program:

Enter the mentioned below commands:

1.To compile the program: 
	cc fsaccess.c
2.To Run the program: 
	./a.out  v6filesystem

Supported Commands:

1) initfs <number of blocks> <number of inode blocks>

	It initializes the disk by setting all the data blocks to free and all the inode blocks as unallocated.

2) mkdir <v6-dir>

	It checks if the v6-dir exists in the current filesystem and it creates the directory if it doesnt exist.

3) cpin externalFileName v6-file

	It creates a new file called v6-file in the filesystem and copies the content of the external file into the v6-file.

4) cpout v6-file externalFileName

	It checks if the v6-file exists and if it exists, it copies the content of the v6-file in the external file.

5) Rm <v6-dir/v6-file>

	If the v6-dir/v6-file exists in the filesystem, it deletes the directory/file and frees the data blocks and inode blocks associated with it.


Commands examples:

initfs 5000 400
mkdir /dhawal/project
cpin externalFileName /dhawal/project/story.txt
cpout /dhawal/project/story.txt externalFileName
Rm /dhawal/project/story.txt  