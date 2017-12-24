#include<stdio.h>
#include<fcntl.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<unistd.h>
#include<stdlib.h>
#include<stdint.h>
#include<string.h>

#define max_size 50
#define i_unallocated 0000000
#define i_allocated 0100000
#define i_directory 0140000
#define i_largefile 0110000


//Global variables

char* arg[100]; // Stores the commands entered by the user 
char *path[100]; // Stores the aboslute path of the file or directory
char *filename; // v6filesystem name 

char *externalFile, *v6file, *v6dir, *absolutepath; // Pointer to externalFile, v6file, v6dir and absolutepath

int fileSize; // Size of the file to be copied  

int fd , input_fd; // fd = file descriptor for v6filesystem image   input_fd = file descriptor for the file used to copy from the external location in cpin

int blocks, iNodes; // Total Number of Blocks(n1), Total Number of Inodes(n2) 

int iNodeCounter = 1; // As root directory has the first Inode

int directoryEntryCounter = 0; // Number of files or directories created 

int absolutePathCounter; // Number of arguments in the absolute path eg : /dhawal/project   absolutePathCounter = 2

	// Super block

	typedef struct Super {
		unsigned short isize;
		unsigned short fsize;
		unsigned short nfree;
		unsigned short free[100];
		unsigned short ninode;
		unsigned short inode[100];
		char flock;
		char ilock;
		char fmod;
		unsigned short time[2];
	} sup;

		sup super_block = {0,0,0,{0},0,{0},0,0,0,{0}};

	// Inode
	
	typedef struct Inode {
		unsigned short flags;
		char nlinks;
		char uid;
		char gid;
		char size0;
		unsigned short size1;
		unsigned short addr[8];
		unsigned short actime[2];
		unsigned short modtime[2];
	} inode ;
		
		inode i_node; 
		inode rootNode; // Inode 1 allocated to root directory
	
	// Directory entries (16 Bytes)

	typedef struct Directory {
		unsigned short inodeNum;
		char filename[14];
	} dir ;
		
		dir rootDirectory = {0,{0}}; 


	// Parse the input commands
	
	void parse_string(char *ptr){
        	char *rem;
		char *token;
		int i=0;

		while(token = strtok_r(ptr, " ", &rem))  // Used to split the string into series of tokens
		{
			arg[i] = token;
			ptr = rem;
			i++;
		}
	}	

	// Parse the absolute path of the file or directory 

	void parse_absolutePath(char *ptr)
	{
		
		char *rem;
		char *token;
		int i=0;

		while(token = strtok_r(ptr,"/", &rem))  // Used to split the string into series of tokens
		{
			path[i] = token;
			ptr = rem;
			i++;
		}
	
		absolutePathCounter = i;

	}

	// Total Number of Blocks
	
	int getNumOfBlocks(void){
                int NumOfBlocks = blocks;
	        return NumOfBlocks;
	}
	
	// Total Number of Inodes
	
	int getNumOfInodes(void){
	        int NumOfInodes = iNodes;
	        return NumOfInodes;
	}
	
	// Blocks assigned for total number of Inodes

	int getInodesBlocks(void){
	        int NumOfInodes = getNumOfInodes();
	        int NumInodesBlocks = NumOfInodes/16;
	        return NumInodesBlocks;
	}

	// Number of Free Blocks excluding the blocks assigned to Inodes, first block( Super block ) and 0th block

	int getFreeBlocks(void){
	        int NumOfFreeBlocks = (getNumOfBlocks() - (getInodesBlocks() + 2));
	        return NumOfFreeBlocks;
	}
	 
	
	//Calculates the starting index for free data blocks.'1' is added to point to the starting position of data blocks
	//@return freeBlockIndex Starting index where the free data blocks start
	
									     
        int getFreeBlocksIndex(void){
                int freeBlockIndex;
                freeBlockIndex = getNumOfBlocks() - getFreeBlocks() + 1;
                return freeBlockIndex;
        }
	
	// Initialize file descriptor
	
	void init_fd()
	{
		fd = open(filename ,O_RDWR | O_TRUNC | O_CREAT ,0777); // file descriptor
	}

	// Initialize input file descriptor for cpin
	
	void initialize_inputFileDescriptor()
	{
		input_fd = open( externalFile ,O_RDWR ,0777); // file descriptor
	}
	
	// Initialize inodes
	
	void init_inode()
	{
		int j = 0, i;
		for( i = 1 ; i <= getNumOfInodes() ; i++) {
		
			i_node.flags = 0000000;
			lseek(fd, 1024 + j, 0);
			j = j + 32;	
			write( fd, &i_node , sizeof(struct Inode)  );
		}	
	}


	// Initialize Root directory
	
	void init_root()
	{
		// Set flags for the root directory (Inode  = 1)

		rootNode.flags = 0140000;
		rootNode.addr[0] = (2 + getInodesBlocks());

		lseek( fd, 1024, 0 );
		write (fd,&rootNode, sizeof(struct Inode));

		// Set the first entry of the root Directory ( 1  and ".")

		rootDirectory.inodeNum = 1;
		strcpy( rootDirectory.filename, "." );

		lseek(fd, rootNode.addr[0] * 512 , 0);
		write(fd, &rootDirectory, sizeof(struct Directory) );

		directoryEntryCounter++;

		// Set the second entry of the root Directory ( 1  and "..")
		
		rootDirectory.inodeNum = 1;
		strcpy( rootDirectory.filename, ".." );

		lseek(fd, rootNode.addr[0] * 512  + 16 , 0);
		write(fd, &rootDirectory, sizeof(struct Directory) );

		directoryEntryCounter++;
	}			

	// Save the changes to SuperBlock
	
	void save_superBlock()
	{
		lseek(fd, 512, 0);
		write(fd, &super_block, sizeof(struct Super));

	}

	// Returns free inode for the new file or directory

	int getFreeInode()
	{
		int j = 0, i;
		unsigned int flag;
	
		for( i = 1 ; i <= getNumOfInodes() ; i++)
		{	
			lseek (fd, 1024 + j, 0  );
			read (fd, &flag, 2);			

			if (( flag & i_allocated) == 0 )
			{	
					break;
			}		
		    j = j+32;
		}	
		
		iNodeCounter++;
		super_block.ninode--;

		save_superBlock();
		return i;
	}

	// Returns free data block for assigning it to a file or directory

	int getFreeDataBlock()
	{
		int freeBlock;
		char buffer[512] = {0};
		super_block.nfree--;

			if(super_block.nfree == 0)
			{
				int nextBlock = super_block.free[super_block.nfree];
	
				// Reload nfree and free array from the 0th Block  

				lseek(fd, nextBlock * 512, 0);
				read(fd, &super_block.nfree, 2);

				lseek(fd, nextBlock * 512 + 2, 0);
				read(fd, &super_block.free, 2 * super_block.nfree);
	
				// Flushing the contents of the 0th block as it is loaded in superblock free array

				lseek(fd, nextBlock * 512, 0);
				write(fd, buffer, sizeof(buffer));
				
				freeBlock = super_block.free[--super_block.nfree];

				// Resetting the super_block.free[nfree]  to zero as it is assigned to a file or directory

				super_block.free[super_block.nfree] = 0;
		
			} else
			{

				freeBlock = super_block.free[super_block.nfree];
			
				super_block.free[super_block.nfree] = 0;
			}	
	
			save_superBlock();

		return freeBlock;

	}	

	// Adds the free data blocks to the free array of the super block

	void addFreeBlock( int block )
	{
		if( super_block.nfree == 100 )
		{	
			// Copy nfree in the first 2 bytes of the incoming block number 

			lseek(fd, block * 512 , 0);
			write(fd, &super_block.nfree, 2);
			
			// Copy free array in the next 200 bytes of the incoming block number

			lseek( fd, block * 512 + 2, 0);
			write(fd, &super_block.free, sizeof(super_block.free));
			
			// Reset nfree

			super_block.nfree = 0;
		
			// Reset free array 

			int x;
			for(x = 0; x < 100; x++){
				super_block.free[x] = 0 ;
			}

			super_block.free[super_block.nfree++] = block;

		} else 
		{

			super_block.free[super_block.nfree++] = block;

		}	

		save_superBlock();
	}
	
	// Display the contents of all the directories

	void showDirectory()
	{	
		unsigned short inodeNum, buf, flag;
		char file[14];
		int offset = 0, a ,k, j= 0, count = 0, b;
		dir readDir = {0,{0}};;

		for( k = 1 ; k < getNumOfInodes(); k++)
		{
			lseek (fd, 1024 + j, 0  );
			read (fd, &flag, 2);			
			
		//	Check if its a directory

			if(( flag & i_directory) == i_directory )
			{

				printf("The Directory contents of iNode number %d is: \n",k);
				
				for(b = 0; b < 8; b++)
				{

					lseek( fd, 1024 + (k-1)*32, 0);
					read( fd, &i_node, sizeof(struct Inode));			

					if(i_node.addr[b] == 0)
					{
						break;
					}
					
					offset = 0, count = 0;

					// Iterating through a data block of the directory(32 entries of 16 bytes each). 

					for(a = 1; a <=32 ; a++ )
					{

						lseek( fd, i_node.addr[b] * 512 + offset, 0 );
						read( fd, &readDir , sizeof(struct Directory));

						if(readDir.inodeNum != 0)
						{
							printf("Inode Number: %d  Name: %s\n",readDir.inodeNum, readDir.filename);
							count = 0;
						}	
						else
						{	
						
							count ++;
							if(count == 2)
							{
								printf("\n");
								goto next;
							}	
							else
							{
								continue;
							}	
						}

						offset = offset + 16;	
					}
					
				}


			} // If loop for checking directory ends
			
			next:

			j = j+ 32;

		} // For loop for iterating through directories ends	

	}

	// Copies small file from the external location into the v6filesystem in the corresponding absolute path

	void copySmallFile()
	{

		int size1 = fileSize;
		int freeBlock, quit = 0, seek = 0, i = 0;
		unsigned short lsb, parentInode;
		char msb,dummy;
		char buffer[512];
		dir newDir = {0,{0}};
	
		//Storing size

		lsb = (fileSize & 0xffff);
		dummy= (int)(fileSize >> 16);
		msb = (unsigned int)(dummy);
		
		// Parent Inode of the file to be copied 

		parentInode = searchDirectory();

		if(parentInode == 0)
		{
			printf("The directory doesnt exists in the filesystem. Please create it!!!");
			return;
		}
	
		// Inode for the small file
		
		int iNum = getFreeInode();	
			
		// Set flags of Inode
	
		lseek( fd, 1024 + (iNum-1)*32, 0);  
		read(fd, &i_node, sizeof(struct Inode));
		
		i_node.flags = 0100000;
		i_node.size1 = lsb;
		i_node.size0 = msb;
		
		lseek(fd, 1024 + (iNum-1)*32 , 0);
		write( fd, &i_node, sizeof(struct Inode));
		
		// Copies the contents from externalfile to v6file

		do
		{
			
			// Read from the source file 
			
			lseek( input_fd, 0 + seek, 0 ); 
			read( input_fd, buffer , sizeof(buffer));
			
			freeBlock = getFreeDataBlock() ;

			// write the data into v6file
			
			lseek( fd, freeBlock * 512, 0);
			write( fd, buffer, sizeof(buffer));

			// add the block number to Inode's addr array
			
			lseek( fd, 1024 +  (iNum-1)*32, 0);  
			read ( fd, &i_node, sizeof(struct Inode));
			i_node.addr[i] = freeBlock;
			lseek(fd, 1024+(iNum-1)*32, 0);
			write (fd , &i_node, sizeof(struct Inode));

			i++;
			seek = seek + 512;
		
			size1 = size1 - 512;		
			
			// Quit flag is set if there is no more data to copy from external file

			if(size1 <= 0)
			{
				quit = 1;
			}
			
			// Flushes the contents of the buffer

			memset(buffer, 0, sizeof(buffer));

			
		} while (quit != 1);
		
		// writing the file information in the parent Directory
		
		unsigned short buf;
		int offset = 0, a, b ;

		for( b = 0; b < 8; b++)
		{	
			// Fetch parent's Inode 

			lseek( fd, 1024 + (parentInode-1) * 32, 0);
			read( fd, &i_node, sizeof(struct Inode));
		
			if(i_node.addr[b] ==  0)
			{	
				// Assign new datablock to inode addr array, if the earlier datablock is full (32 entries already exists).

				i_node.addr[b] = getFreeDataBlock();
				
				lseek( fd, 1024 + (parentInode-1) * 32, 0);
				write( fd, &i_node, sizeof(struct Inode));
			}
			
			offset = 0;
			
			for(a = 1; a <= 32 ; a++ )
			{

				lseek( fd, 1024 + (parentInode-1) * 32, 0);
				read( fd, &i_node, sizeof(struct Inode));

				lseek( fd, i_node.addr[b] * 512 + offset, 0 );
				read (fd , &buf, 2);
					
				// Add the file info to directory if there are no more entries in the current directory data block.

				if(buf == 0)
				{	
					newDir.inodeNum = iNum;
					strcpy( newDir.filename, v6file );

					lseek(fd, i_node.addr[b] * 512 + offset , 0);
					write(fd, &newDir, sizeof(struct Directory) );
					
					goto next;
					break;

				}

				offset = offset + 16;	
			}
		}
		
		next:

		directoryEntryCounter++;

		printf("The file is successfully copied into the v6 filesystem!!!\n");

	}

	// Copies large file from the external location into the v6filesystem in the corresponding absolute path

	void copyLargeFile()
	{

		int size1 = fileSize;
		int freeBlock, quit = 0, seek = 0, i = -1;
		unsigned short lsb, parentInode;
		char msb,dummy;
		char buffer[512];
		int iflag = 0;

		dir newDir = {0,{0}};
	
		// Parent Inode of the file to be copied 

		parentInode = searchDirectory();

		if(parentInode == 0)
		{
			printf("The directory doesnt exists in the filesystem. Please create it!!!");
			return;
		}


		//Storing size

		lsb = (fileSize & 0xffff);

		if(fileSize < 16777216) // Check if filesize is less than 16MB
		{
			dummy = (int)(fileSize >> 16);
			msb = (unsigned int)(dummy);
			iflag = 0;

		} else if( fileSize < 33554431 ) // if filesize is greater than 16MB but less than 32MB
		{  
			
			dummy = (int)(fileSize >> 16); // Out of the 9 bits , dummy will be assigned with lower  bits as it's a char type
			msb = (unsigned int)(dummy);
			iflag = 1;

		} else
		{ // if file is greater tha 32MB

			printf("File is too large to copy\n");
			exit(0);
		}
		
		// Inode for the large file
		
		int iNum = getFreeInode();	
			
		// Set flags of Inode
		
		lseek( fd, 1024 + (iNum-1)*32, 0);  
		read(fd, &i_node, sizeof(struct Inode));
		
		if(iflag == 0)
		{	
			i_node.flags = 0110000;
		} else 
		{
			i_node.flags = 0111000; // Setting unused bit to 1 to accomodate 25 bits and hence file size can ne increased to max  32MB
			iflag = 0;
		}

		i_node.size1 = lsb;
		i_node.size0 = msb;
		
		lseek(fd, 1024 + (iNum-1)*32 , 0);
		write( fd, &i_node, sizeof(struct Inode));
		
		int count = 0, indirectBlock,secondIndirectBlock, off, off2= 0;

		// Copies the contents from externalfile to v6file

		do
		{
		
			if((count % 256) == 0)
			{			
				// New indirect block will be created after every 256 data blocks are utilised 

				indirectBlock = getFreeDataBlock();			
				
				i++;
				off = 0;
				off2 = off2 + 2;
				
				if( i <= 6)
				{
					// add the indirect block number to Inode's addr array

					lseek( fd, 1024 +  (iNum-1)*32, 0); 
					read ( fd, &i_node, sizeof(struct Inode));
					
					i_node.addr[i] = indirectBlock;
					
					lseek(fd, 1024+(iNum-1)*32, 0);
					write (fd , &i_node, sizeof(struct Inode));
				}

			}
			

			// Check for last double indirect block
			
			if(i <= 6)
			{  

				// Read from the source file 
				lseek( input_fd, seek, 0 ); 
				read( input_fd, buffer , sizeof(buffer));
				
				freeBlock = getFreeDataBlock() ;

				// write the data into v6file
				lseek( fd, freeBlock * 512, 0); 
				write( fd, buffer, sizeof(buffer));

				// add the data block number to indirectBlock
				lseek( fd, indirectBlock * 512 + off, 0);
				write( fd, &freeBlock, 2);
			}

			else
			{
				// Only executed once and second indirect block is created
				if(i == 7)
				{				
					secondIndirectBlock = getFreeDataBlock();

					// add the second indirect block number to Inode's addr array
			
					lseek( fd, 1024 +  (iNum-1)*32, 0); 
					read ( fd, &i_node, sizeof(struct Inode));
					
					i_node.addr[i] = secondIndirectBlock;
					
					lseek(fd, 1024+(iNum-1)*32, 0);
					write (fd , &i_node, sizeof(struct Inode));
					
					off2 = 0;
				}

				// Read from the source file 
				
				lseek( input_fd, seek, 0 ); 
				read( input_fd, buffer , sizeof(buffer));
				
				freeBlock = getFreeDataBlock() ;

				// write the data into v6file
				
				lseek( fd, freeBlock * 512, 0); 
				write( fd, buffer, sizeof(buffer));

				// add the indirect block number to second indirect Block's data block
				
				lseek( fd, secondIndirectBlock * 512 + off2, 0); 
				write (fd , &indirectBlock, 2);

				// add the data block number to indirectBlock
				
				lseek( fd, indirectBlock * 512 + off, 0);
				write( fd, &freeBlock, 2);

			} // if else loop ends
	
			// for adding freeblock into datablock of indirectBlock (2 Byte addresses)
			
			off = off + 2;
			
			// reading from input external file  
			
			seek = seek + 512;
		
			
			size1 = size1 - 512;		

			// Checking for last block to be read from input source file

			if(size1 <= 0)
			{  
				quit = 1;
			}
			
			// Clearing the buffer 

			memset(buffer, 0, sizeof(buffer)); 

			count ++;

		} while (quit != 1); // do while ends when file copying is done 
		
		
		
		// writing the file information in the parent Directory
		
		unsigned short buf;
		int offset = 0, a, b;

		for( b = 0; b < 8; b++)
		{
			// Fetch parent's Inode 

			lseek( fd, 1024 + (parentInode-1) * 32, 0);
			read( fd, &i_node, sizeof(struct Inode));
		
			if(i_node.addr[b] ==  0)
			{
				// Assign new datablock to inode addr array, if the earlier datablock is full (32 entries already exists).

				i_node.addr[b] = getFreeDataBlock();
				
				lseek( fd, 1024 + (parentInode-1) * 32, 0);
				write( fd, &i_node, sizeof(struct Inode));
			}
			
			offset = 0;
			
			for(a = 1; a <= 32 ; a++ )
			{

				lseek( fd, 1024 + (parentInode-1) * 32, 0);
				read( fd, &i_node, sizeof(struct Inode));

				lseek( fd, i_node.addr[b] * 512 + offset, 0 );
				read (fd , &buf, 2);

				if(buf == 0)
				{	
					newDir.inodeNum = iNum;
					strcpy( newDir.filename, v6file );

					lseek(fd, i_node.addr[b] * 512 + offset , 0);
					write(fd, &newDir, sizeof(struct Directory) );
					
					goto next;

					break;

				}

				offset = offset + 16;	
			}
		}					
		
		next:

		directoryEntryCounter++;

		printf("The file is successfully copied into the v6 filesystem!!!\n");

	}		

	// Copies the v6file to external location
	
	void copyOutFile()
	{
		int j = 0, i;
		unsigned int flag, address, off = 0,filefound = 0 ;
		unsigned short iNodefile, parentInode;
		
		dir checkDir = {0,{0}};
	
		// Parent Inode of the file to be copied 

		parentInode = searchDirectory();

		if(parentInode == 0)
		{
			printf("The directory doesnt exists in the filesystem. Please create it!!!");
			return;	
		}

		for( i = 0 ; i < 8 ; i++)
		{	
			off = 0;

			lseek( fd, 1024 + (parentInode-1)*32, 0);
			read( fd, &i_node, sizeof(struct Inode));
			
			if(i_node.addr[i] == 0)
			{
				break;
			}
			
			lseek( fd, i_node.addr[i] * 512 + off, 0);
			read( fd, &checkDir, sizeof(struct Directory));
			
			
			// Check if the file to be copied to external file exists in the current directory
			do
			{

				if( strcmp(checkDir.filename,v6file) == 0 ) 
				{
					iNodefile = checkDir.inodeNum;	
					filefound = 1;
					goto copy; 

					break;
				}
			
				off = off + 16;

				lseek( fd, i_node.addr[i] * 512 + off, 0);
				read( fd, &checkDir, sizeof(struct Directory));

			} while(checkDir.inodeNum != 0 && off <= 496); // do while ends


		}		
		

		if(filefound == 0)
		{ 
			printf("File does not exist in the v6 filesystem.\n");
			return;
		}

		copy:;

		// Fetch the inode of the file to be copied 
		
		lseek( fd, 1024 + (iNodefile-1) * 32, 0 );
		read( fd, &i_node, sizeof(struct Inode) );

		// External file descriptor

		int fd_out = open( externalFile ,O_RDWR | O_CREAT | O_TRUNC ,0777);

		
		// Check if its a large file or small file

		if( (i_node.flags & i_largefile ) == i_largefile)
		{
			fileSize = ((i_node.flags & 0001000) << 16 ) | i_node.size0 << 16 | i_node.size1 ;
			
			goto largeFile;

		} else if( (i_node.flags & i_allocated) == i_allocated)
		{
			fileSize = i_node.size0 << 16 | i_node.size1 ;
			
			goto smallFile;
		}
		
		// Copies the small file to the file created at external location

		smallFile:;

		
		char buffer[512];
		int addrIndex, exitFlag, offset = 0;
		
		for( addrIndex = 0; addrIndex < 8; addrIndex++ ) {

			if( i_node.addr[addrIndex] == 0 ){ break;}
			
			lseek( fd, i_node.addr[addrIndex] * 512, 0);
			read( fd ,buffer, sizeof(buffer) );

			lseek( fd_out, offset ,0 );
			write( fd_out, buffer, sizeof(buffer));

			offset = offset + 512;

		}


		goto end;

		
		// Copies the large file to the file created at external location
		
		largeFile:;
		
		
		unsigned short dataBlock, indirectBlock, secondIndirectBlock;
		int seek = 0, count = 0, off3 = 0, off2 = 0, quit = 0;
		int size1 = fileSize;
		i = -1;
		
		do
		{
		
			if((count % 256) == 0)
			{						
				i++;
				if(i<=7)
				{
					indirectBlock = i_node.addr[i] ;			
				}
				off3 = 0;
				off2 = off2 + 2;
			}
			

			// Check for last double indirect block
			
			if(i <= 6)
			{  

				// Read  data block number from the indirect block number 
				
				lseek( fd, indirectBlock * 512 + off3, 0 ); 
				read( fd, &dataBlock , 2);
				
				// Read data from data block number 

				lseek( fd, dataBlock * 512, 0 ); 
				read( fd, buffer , sizeof(buffer));

				// write the data into v6file
				lseek( fd_out , seek , 0); 
				write( fd_out , buffer, sizeof(buffer));

			}

			else
			{
				// Only executed once and second indirect block is created
		
				if(i == 7)
				{
					secondIndirectBlock = indirectBlock;
					off2 = 0;
				}
				
				// Read  indirect block number from the  second indirect block number 
				
				lseek( fd, secondIndirectBlock * 512 + off2, 0 ); 
				read( fd, &indirectBlock , 2);

				// Read  data block number from the indirect block number 
				
				lseek( fd, indirectBlock * 512 + off3, 0 ); 
				read( fd, &dataBlock , 2);
				
				// Read data from data block number 

				lseek( fd, dataBlock * 512, 0 ); 
				read( fd, buffer , sizeof(buffer));

				// write the data into v6file
			
				lseek( fd_out , seek , 0); 
				write( fd_out , buffer, sizeof(buffer));
		
			} // if else loop ends
	
			off3 = off3 + 2;
			seek = seek + 512;
		
			size1 = size1 - 512;		
			
			if(size1 <= 0)
			{ 
				quit = 1;
			}
			
			memset(buffer, 0, sizeof(buffer));

			count ++;

		} while (quit != 1); // do while ends when file copying is done 


		goto end;

		end:
		
		printf("The file is successfully copied to the external location!!!\n");

	}


//	Search directory and returns the inode number in which the file needs to be checked 
	
	int searchDirectory()
	{

		int count = 0, index = 0, off = 0,i, flag ;
		unsigned short iNum = 1;

		dir checkDir = {0,{0}};

		if( absolutePathCounter == 1) // The entry needs to be checked in the root directory
		{
			return 1;  // The inode number of root directory

		}	
		else
		{

			while(count < (absolutePathCounter - 1))
			{
				flag = 0;
				
				for(i = 0; i < 8; i++)
				{
					lseek( fd, 1024 + (iNum - 1) * 32, 0);
					read( fd, &i_node, sizeof(struct Inode));
					
					if(i_node.addr[i] == 0)
					{
						break;
					}

					lseek( fd, i_node.addr[i] * 512, 0);
					read( fd, &checkDir, sizeof(struct Directory));

					off = 0;

				//	Check if the directory exists in the v6filesystem
					
					do 
					{
						if( strcmp(checkDir.filename,path[index]) == 0 ) 
						{
							iNum = checkDir.inodeNum;	
							if( count == (absolutePathCounter - 2))
							{	
								return iNum; // Parent Inode	
							}
							else
							{
								flag = 1;
								break;
							}	

						}

						off = off + 16;

						lseek( fd, i_node.addr[i] * 512 + off, 0);
						read( fd, &checkDir, sizeof(struct Directory));

					} while (checkDir.inodeNum != 0 && off <= 496);//  do while ends
				
					if(flag == 1)
					{	
						break;
					}
				}
				
				count++;
				index++;
			}

		}

		return 0;	// The directory mentioned in the absolute path does not exist	
	}



	// Create a new directory 

	void createDirectory()
	{


		unsigned short flag, iNodeDir, parentInode;
		unsigned short buf;
		int i, off = 0, dirfound = 0, j = 0, offset = 0;

		dir newDir = {0,{0}};
		dir checkDir = {0,{0}};

		// Parent Inode of the file to be copied 

		parentInode = searchDirectory();
		
		if(parentInode == 0)
		{

			printf("The directory doesnt exists in the filesystem. Please create it!!!");
			return;
			
		}
	
		for( i = 0; i < 8; i++ )		
		{
			// Fetch parent's Inode

			lseek( fd, 1024 + (parentInode-1)*32, 0);
			read( fd, &i_node, sizeof(struct Inode));
			
			if(i_node.addr[i] == 0)
			{
				break;
			}	

			lseek( fd, i_node.addr[i] * 512 + off, 0);
			read( fd, &checkDir, sizeof(struct Directory));
			
			// Check if the directory exists in the v6filesystem
			do 
			{

				if( strcmp(checkDir.filename,v6dir) == 0 ) 
				{
					iNodeDir = checkDir.inodeNum;	
					dirfound = 1;
					goto directoryFound;
					break;
				}
			
				off = off + 16;

				lseek( fd, i_node.addr[0] * 512 + off, 0);
				read( fd, &checkDir, sizeof(struct Directory));

			} while (checkDir.inodeNum != 0 && off <= 496); // do while ends


		}

		directoryFound:
		
		// if dirfound flag is set, it goes to the else loop and the directory already exits ; else we need to create it.

		if(dirfound == 0)
		{
			char buffer[480] = {0};
			
			// Fetch new inode for the directory

			iNodeDir =  getFreeInode();

			lseek( fd, 1024 + (iNodeDir -1) * 32, 0 );
			read( fd, &i_node , sizeof(struct Inode));

			// Set flags of the directory

			i_node.flags = 0140000;
			i_node.addr[0] = getFreeDataBlock();

			lseek( fd, 1024 + (iNodeDir -1) * 32, 0 );
			write (fd, &i_node, sizeof(struct Inode));
			
			// Set the first entry of the new Directory ( Inode number and ".")

			newDir.inodeNum = iNodeDir;
			strcpy( newDir.filename, "." );

			lseek(fd, i_node.addr[0] * 512 , 0);
			write(fd, &newDir, sizeof(struct Directory) );


			// Set the second entry of the new Directory ( Parent's Inode number and "..")

			newDir.inodeNum = parentInode;
			strcpy( newDir.filename, ".." );

			lseek(fd, i_node.addr[0] * 512  + 16 , 0);
			write(fd, &newDir, sizeof(struct Directory) );

			lseek(fd, i_node.addr[0] * 512  + 32 , 0);
			write(fd, buffer, 480 );
			
			
			// writing the directory information in the parent Directory
			int a, b;
			offset = 0;


			for( b = 0; b < 8; b++)
			{	
			
				lseek( fd, 1024 + (parentInode-1) * 32, 0);
				read( fd, &i_node, sizeof(struct Inode));
			
				if(i_node.addr[b] ==  0)
				{
					i_node.addr[b] = getFreeDataBlock();
					
					lseek( fd, 1024 + (parentInode-1) * 32, 0);
					write( fd, &i_node, sizeof(struct Inode));
				}
				
				offset = 0;
				for(a = 1; a <= 32 ; a++ )
				{
					
					lseek( fd, 1024 + (parentInode-1)*32, 0);
					read( fd, &i_node, sizeof(struct Inode));

					lseek( fd, i_node.addr[b] * 512+ offset, 0 );
					read (fd , &buf, 2);
					
					if(buf == 0)
					{	
						newDir.inodeNum = iNodeDir;
						strcpy( newDir.filename, v6dir );

						lseek(fd, i_node.addr[b] * 512 + offset , 0);
						write(fd, &newDir, sizeof(struct Directory) );
						
						goto next;
						break;
					
					}

					offset = offset + 16;	
				}
			}

			next:

			directoryEntryCounter++;
			
			printf("The directory is successfully created!!!\n");

		} else 
		{

			printf("The directory already exists.\n");

		}

	}

	// Remove file entry from parent  directory
	
	void removeFileEntry(int pInode)
	{
		
		dir checkDir = {0,{0}};
		dir nullDir = {0,{0}};
		int off  = 0, b;
		unsigned short address;

		for(b = 0; b < 8; b++)
		{

			// Fetch parent's Inode

			lseek( fd, 1024 + (pInode - 1) * 32, 0);
			read( fd, &i_node, sizeof(struct Inode));
		
			if(i_node.addr[b] == 0)
			{
				break;
			}

			off = 0;

			lseek( fd, i_node.addr[b] * 512 + off, 0);
			read( fd, &checkDir, sizeof(struct Directory));
			
			// Check if the file to be removed  exists in the directory entries
		
			do 
			{
				if( strcmp( checkDir.filename,v6file ) == 0 ) 
				{
					// If the file exists, remove the entry from the directory

					lseek( fd, i_node.addr[0] * 512 + off, 0);
					write( fd, &nullDir, sizeof(struct Directory));

					goto deleted;
					break;
				}
			
				off = off + 16;

				lseek( fd, i_node.addr[0] * 512 + off, 0);
				read( fd, &checkDir, sizeof(struct Directory));

			} while (checkDir.inodeNum != 0 && off<= 496); // do while ends
	
		}

		deleted:

		directoryEntryCounter--;

	}

	// Remove file from the v6 filesystem
	
	void removeFile()
	{
		int j = 0, i;
		unsigned int flag, address, off = 0,filefound = 0 ;
		unsigned short iNodefile, parentInode;
		
		dir checkDir = {0,{0}};

		// Parent Inode of the file to be removed 

		parentInode = searchDirectory();

		if(parentInode == 0)
		{
			printf("The directory doesnt exists in the filesystem. Please create it!!!");
			return;	
		}

		for( i = 0 ; i < 8 ; i++)
		{	
			off = 0;

			lseek( fd, 1024 + (parentInode-1)*32, 0);
			read( fd, &i_node, sizeof(struct Inode));
			
			if(i_node.addr[i] == 0)
			{
				break;
			}
			
			lseek( fd, i_node.addr[i] * 512 + off, 0);
			read( fd, &checkDir, sizeof(struct Directory));
			
			
			// Check if the file to be removed exists in the parent directory

			do
			{

				if( strcmp(checkDir.filename,v6file) == 0 ) 
				{
					iNodefile = checkDir.inodeNum;	
					filefound = 1;
					goto remove; 

					break;
				}
			
				off = off + 16;

				lseek( fd, i_node.addr[i] * 512 + off, 0);
				read( fd, &checkDir, sizeof(struct Directory));

			} while(checkDir.inodeNum != 0 && off <= 496); // do while ends


		}

		remove:

		if(filefound == 0)
		{ 
			printf("The file does not exist in the filesystem\n");
			return;

		}
		else if (filefound == 1)
		{
			// Fetch inode contents of the file to be removed

			lseek( fd, 1024 + (iNodefile - 1) * 32, 0);
			read( fd, &i_node, sizeof(struct Inode));

			// Check whether the file is a large file or small file

			if( (i_node.flags & i_largefile) == i_largefile )
			{
			
				unsigned short indirectBlock, dataBlock;
				int i, count , offset, quit = 0;
				char buffer[512] = {0};

				for( i = 0; i < 7; i++)
				{
					if(i_node.addr[i] != 0)
					{

						indirectBlock = i_node.addr[i];
						count = 0;
						offset = 0;
						quit = 0;

					} else 
					{
						break;

					}

					lseek( fd, indirectBlock * 512 , 0);
					read( fd, &dataBlock , 2);

					// Fetch datablocks from the indirectBlock 
					do
					{				
						// Clear the contents of the data block

						lseek( fd, dataBlock * 512 , 0);
						write( fd, buffer, sizeof(buffer));
						
						// Add the data block to the free data blocks list

						addFreeBlock(dataBlock);
						offset = offset + 2;
						
						dataBlock = 0;
						
						// Fetch the next data block 

						lseek( fd, indirectBlock * 512 + offset , 0);
						read( fd, &dataBlock , 2);
						
						count++;

						// Quit the loop if there are no further data blocks or we already removed all 256 data blocks

						if(dataBlock == 0 || count == 256)
						{
							quit = 1;
						}	
					

					} while(quit != 1);
					
					// Clear the contents of the indirect Block
					
					lseek( fd, indirectBlock * 512 , 0);
					write( fd, buffer, sizeof(buffer));

					// Add the indirect block to the free data blocks list

					addFreeBlock(indirectBlock);
					
					indirectBlock = 0;
				}

			}
			else if( (i_node.flags & i_allocated) ==  i_allocated )
			{
				
				unsigned short dataBlock;
				int i;
				char buffer[512] = {0};
				
				for( i =0 ; i<=7 ; i++)
				{
					
					if(i_node.addr[i] != 0)
					{
						dataBlock = i_node.addr[i];
						
						// Clear the contents of the data Block
						
						lseek( fd, dataBlock * 512 , 0);
						write( fd, buffer, sizeof(buffer));

						// Add the data block to the free data blocks list
						
						addFreeBlock(dataBlock);

					} else
					{
						break;
					}	

				}

			}	// Small Large File if else ends

			// Flush the contents of the Inode allocated to the file

			memset(&i_node, 0, sizeof(struct Inode));

			lseek( fd, 1024 + (iNodefile - 1) * 32, 0);
			write( fd, &i_node, sizeof(struct Inode));

			// Remove the file entry from the parent directory
			
			removeFileEntry(parentInode);

		} // If loop of file removal ends
		
		iNodeCounter--;

		super_block.ninode++;

		save_superBlock();

		printf("The file is removed successfully from the v6 filesystem!!!\n");

	} // Function ends


	// Main function
	int main ( int argc , char *argv[] )
	{
		// Assign filename with the name of the v6filesystem image
		
		filename = argv[1];

		int initializeFlag = 0;
		
		// Parse input command

		char *command_name = malloc(max_size);
    		
		printf("Please enter initialization commamd\n");		
		scanf("%[^\n]%*c",command_name);  
		parse_string(command_name);            
		
		
		int label;

		do{
		
		if(strcmp(arg[0],"initfs") == 0){
			label = '1' ;
		}
		else if(strcmp(arg[0],"cpin") == 0){
			label = '2' ;
		}
		else if(strcmp(arg[0],"cpout") == 0){
			label = '3' ;
		}
		else if(strcmp(arg[0],"mkdir") == 0){
			label = '4' ;
		}
		else if(strcmp(arg[0],"Rm") == 0){
			label = '5' ;
		}
		else{
			label = '6' ;
		}
	
		// Execute appropriate function depending upon the user input commands

		switch(label)
		{
			case '1' : 	printf("Initializing......\n");
					
					// Assign number of blocks and inodes  

					blocks = atoi(arg[1]);
					iNodes = atoi(arg[2]);
					
					// Set values of the super block

					super_block.fsize = getNumOfBlocks();
					super_block.isize = getInodesBlocks();		
					super_block.ninode = getNumOfInodes();
					
					// Initialize file descriptor for filesystem image 

					init_fd();
					
					save_superBlock();

					init_inode();
					init_root();

					int i;

					// Initial conditions
					
					super_block.ninode--;
					super_block.nfree = 1;
					super_block.free[0] = 0;
					
					save_superBlock();
					
					// Add all the data blocks to free array list
					
					for( i = getFreeBlocksIndex(); i < getNumOfBlocks() ; i++ )
					{
						addFreeBlock(i);
					}
					
					initializeFlag = 1;
					
					printf( "The file system is successfully initialized\n" );
				
					break;

			case '2' : 	if( initializeFlag == 1)
					{
						printf("Copying in the v6fileSystem from external file\n");
					
						// Set externalFile to first argument after cpin command

						externalFile = arg[1];
											
						// Set absolute path to second argument after cpin command
											
						absolutepath = arg[2];
						
						// Parse the absolute path 

						parse_absolutePath(absolutepath);
						
						// Set v6file after parsing the absolute path
						
						v6file  = path[absolutePathCounter - 1];

						printf("The external file is  %s and internal file is %s\n", externalFile, v6file);

						// Initialize file descriptor of the external file from which we need to copy into our filesystem

						initialize_inputFileDescriptor();
						
						// Sets the file size of the external file

						struct stat stat_buf;
						int rc = fstat(input_fd, &stat_buf);
						fileSize =  (rc == 0) ?  stat_buf.st_size : -1;
								
						// Depending upon the filesize, copy small file or large file 
								
						if( fileSize <= 4096 ){
							
							copySmallFile();				

						} else {

							copyLargeFile();

						}
					}	
						
					printf("\n");
					break;
			
			case '3' :	if( initializeFlag == 1)
					{
			
						printf("Copying from the v6filesystem to external file\n");
						
						// Set externalFile to second argument after cpout command
						
						externalFile = arg[2];
						
						// Set absolute path to first argument after cpout command
					
						absolutepath = arg[1];
						
						// Parse the absolute path

						parse_absolutePath(absolutepath);

						// Set v6file after parsing the absolute path
						
						v6file  = path[absolutePathCounter - 1];
						
						printf("The v6file is %s and the external file is %s\n", v6file, externalFile);
						
						// Copies the v6file to the external location

						copyOutFile();
					}

					printf("\n");
					break;

			case '4' :	if( initializeFlag == 1)
					{
					
						printf("Creating a new directory in the v6 filesystem\n");
						
						// Set absolute path to first argument after mkdir command

						absolutepath = arg[1];
						
						// Parse the absolute path
						
						parse_absolutePath(absolutepath);
					
						// Set v6dir after parsing the absolute path
						
						v6dir = path[absolutePathCounter - 1];
						
						// Creates the directory in the appropriate parent directory

						createDirectory();
					}

					printf("\n");
					break;

			case '5' :	if( initializeFlag == 1)
					{
			
						printf("Removing file from the v6 filesystem\n");
						
						// Set absolute path to first argument after Rm command

						absolutepath = arg[1];
						
						// Parse the absolute path

						parse_absolutePath(absolutepath);

						// Set v6file after parsing the absolute path

						v6file  = path[absolutePathCounter - 1];

						// Remove the existing file from the parent directory

						removeFile();
					}

					printf("\n");
					break;

			case '6' :	printf("Please enter the command in format mentioned as follows: \n");
					printf("initfs NumberofBlocks NumberOfInodes\n");
					printf("cpin externalfile v6-filePath\n");
					printf("cpout v6-filePath externalfile\n");
					printf("mkdir v6-dirPath\n");
					printf("Rm v6-filePath\n");

					break;

		}// switch case ends	
		
			printf("\n");
	
			if( initializeFlag == 1 )
			{
				printf("Please enter next commamd\n");
				scanf("%[^\n]%*c",command_name);
				parse_string(command_name);
				printf("\n");
			}
			else
			{

				printf("Please enter initfs commamd first inorder to successfully initialize the file system\n");
				scanf("%[^\n]%*c",command_name);
				parse_string(command_name);
				printf("\n");
			}

			label = 0;

		} while( strcmp(arg[0], "q") != 0);
	
		// Displays the contents of all the directories

		showDirectory();
	
	}


