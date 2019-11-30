#include <stdio.h>

#include <stdlib.h>

#include <unistd.h>

#include <signal.h>

#include <string.h>

//#include <sys/wait.h>

#include <sys/types.h>

#include <ctype.h>

#include <errno.h>

#include <sys/stat.h>

#include <fcntl.h>

#include <time.h>



#define BLOCK_SIZE 1024

#define MAX_FILE_SIZE 4194304 // 4GB of file size

#define FREE_ARRAY_SIZE 248 // free and inode array size

#define INODE_SIZE 64





/*************** super block structure**********************/

typedef struct {

        unsigned int isize; // 4 byte

        unsigned int fsize;

        unsigned int nfree;

        unsigned int ninode;

        unsigned short free[FREE_ARRAY_SIZE];

        unsigned short inode[FREE_ARRAY_SIZE];

        unsigned short flock;

        unsigned short ilock;

        unsigned short fmod;

        unsigned int time[2];

} superBlock_type;



/****************inode structure ************************/

typedef struct {

        unsigned short flags; // 2 bytes

        char nlinks;  // 1 byte

        char uid;

        char gid;

        unsigned int size; // 32bits  2^32 = 4GB filesize

        unsigned short addr[22]; // to make total size = 64 byte inode size

        unsigned int actime;

        unsigned int modtime;

} Inode;



typedef struct

{

        unsigned short inode;

        char filename[14];

}direc_type;



superBlock_type superBlock;

int file_descriptor;

char pwd[100];

int current_inode_number;

char fileSystemPath[100];

int total_inodes_count;


void write_in_to_blocks (int blockNumber, void * write_buffer, int nbytes);

void write_in_to_blocksOffset(int blockNumber, int offset, void * write_buffer, int nbytes);

void read_char_from_block_offset(int blockNumber, int offset, void * buffer, int nbytes);

void add_block_to_free_list(int blockNumber);

int get_block_from_free_list();

void addFreeInode(int inode_nums);

Inode get_free_inode(int inode_nums);

int getFreeInode();

void write_to_inode(int inode_nums, Inode inode);

void create_root();

void list_contents();

void makedir(char* dirName);

int last_index(char s[100], char ch) ;

void path_slicer(const char * str, char * buffer, size_t start, size_t end);

void directory_change(char* dirName);

void extfs_intfs(char* sourcePath, char* filename);

void intfs_extfs(char* destinationPath, char* filename);

void file_delete(char* filename);

void directory_delete(char* filename);

int openfs(const char *filename);

void initfs(char* path, int total_blocks, int total_inodes);

void terminate();








void write_in_to_blocks (int blockNumber, void * write_buffer, int nbytes)

{

        lseek(file_descriptor,BLOCK_SIZE * blockNumber, SEEK_SET);

        write(file_descriptor,write_buffer,nbytes);

}





void write_in_to_blocksOffset(int blockNumber, int offset, void * write_buffer, int nbytes)

{

        lseek(file_descriptor,(BLOCK_SIZE * blockNumber) + offset, SEEK_SET);

        write(file_descriptor,write_buffer,nbytes);

}



void read_char_from_block_offset(int blockNumber, int offset, void * buffer, int nbytes)

{

        lseek(file_descriptor,(BLOCK_SIZE * blockNumber) + offset, SEEK_SET);

        read(file_descriptor,buffer,nbytes);

}





void add_block_to_free_list(int blockNumber){

        if(superBlock.nfree == FREE_ARRAY_SIZE)

        {

                //write to the new block

                write_in_to_blocks(blockNumber, superBlock.free, FREE_ARRAY_SIZE * 2);

                superBlock.nfree = 0;

        }

        superBlock.free[superBlock.nfree] = blockNumber;

        superBlock.nfree++;

}



int get_block_from_free_list(){

        if(superBlock.nfree == 0){

                int blockNumber = superBlock.free[0];

                lseek(file_descriptor,BLOCK_SIZE * blockNumber , SEEK_SET);

                read(file_descriptor,superBlock.free, FREE_ARRAY_SIZE * 2);

                superBlock.nfree = 100;

                return blockNumber;

        }

        superBlock.nfree--;

        return superBlock.free[superBlock.nfree];

}



void addFreeInode(int inode_nums){

        if(superBlock.ninode == FREE_ARRAY_SIZE)

                return;

        superBlock.inode[superBlock.ninode] = inode_nums;

        superBlock.ninode++;

}



Inode get_free_inode(int inode_nums){

        Inode iNode;

        int blockNumber = (inode_nums * INODE_SIZE) / BLOCK_SIZE;    // need to remove

        int offset = (inode_nums * INODE_SIZE) % BLOCK_SIZE;

        lseek(file_descriptor,(BLOCK_SIZE * blockNumber) + offset, SEEK_SET);

        read(file_descriptor,&iNode,INODE_SIZE);

        return iNode;

}



int getFreeInode(){

        if (superBlock.ninode <= 0) {

            int i;

            for (i = 2; i<total_inodes_count; i++) {

                Inode freeInode = get_free_inode(superBlock.inode[i]);

                if (freeInode.flags != 1<<15) {

                    superBlock.inode[superBlock.ninode] = i;

                    superBlock.ninode++;

                }

            }

        }

        superBlock.ninode--;

        return superBlock.inode[superBlock.ninode];

}



void write_to_inode(int inode_nums, Inode inode){

        int blockNumber = (inode_nums * INODE_SIZE )/ BLOCK_SIZE;   //need to remove

        int offset = (inode_nums * INODE_SIZE) % BLOCK_SIZE;

        write_in_to_blocksOffset(blockNumber, offset, &inode, sizeof(Inode));

}



void create_root(){

        int blockNumber = get_block_from_free_list();

        direc_type directory[2];

        directory[0].inode = 0;

        strcpy(directory[0].filename,".");



        directory[1].inode = 0;

        strcpy(directory[1].filename,"..");



        write_in_to_blocks(blockNumber, directory, 2*sizeof(direc_type));



        Inode root;

        root.flags = 1<<14 | 1<<15; // setting 14th and 15th bit to 1, 15: allocated and 14: directory

        root.nlinks = 1;

        root.uid = 0;

        root.gid = 0;

        root.size = 2*sizeof(direc_type);

        root.addr[0] = blockNumber;

        root.actime = time(NULL);

        root.modtime = time(NULL);



        write_to_inode(0,root);

        current_inode_number = 0;

        strcpy(pwd,"/");

}



void list_contents(){                                                              // list directory contents

        Inode current_inode = get_free_inode(current_inode_number);

        int blockNumber = current_inode.addr[0];

        direc_type directory[100];

        int i;

        read_char_from_block_offset(blockNumber,0,directory,current_inode.size);

        for(i = 0; i < current_inode.size/sizeof(direc_type); i++)

        {

                printf("%s\n",directory[i].filename);

        }

}



void makedir(char* dirName)

{

        int blockNumber = get_block_from_free_list(); // block to store directory table

        int inode_nums = getFreeInode(); // inode numbr for directory

        direc_type directory[2];

        directory[0].inode = inode_nums;

        strcpy(directory[0].filename,".");

        printf("%s",directory[0].filename);



        directory[1].inode = current_inode_number;

        strcpy(directory[1].filename,"..");

        printf("%s",directory[1].filename);



        write_in_to_blocks(blockNumber, directory, 2*sizeof(direc_type));

// write directory i node

        Inode dir;

        dir.flags = 1<<14 | 1<<15; // setting 14th and 15th bit to 1, 15: allocated and 14: directory

        dir.nlinks = 1;

        dir.uid = 0;

        dir.gid = 0;

        dir.size = 2*sizeof(direc_type);

        dir.addr[0] = blockNumber;

        dir.actime = time(NULL);

        dir.modtime = time(NULL);



        write_to_inode(inode_nums,dir);



// update pRENT DIR I NODE

        Inode current_inode = get_free_inode(current_inode_number);

        blockNumber = current_inode.addr[0];

        direc_type newDir;

        newDir.inode = inode_nums ;

        strcpy(newDir.filename,dirName);

        int i;

        write_in_to_blocksOffset(blockNumber,current_inode.size,&newDir,sizeof(direc_type));

        current_inode.size += sizeof(direc_type);

        write_to_inode(current_inode_number,current_inode);

}



int last_index(char s[100], char ch) {

    int i, p=-1;

    for (i=0; i<100; i++) {

        if (s[i]==ch)

            p = i;

    }

    return p;

}



void path_slicer(const char * str, char * buffer, size_t start, size_t end)

{

    size_t j = 0;

    size_t i;

    for ( i = start; i <= end; ++i ) {

        buffer[j++] = str[i];

    }

    buffer[j] = 0;

}



void directory_change(char* dirName)

{

        Inode current_inode = get_free_inode(current_inode_number);

        int blockNumber = current_inode.addr[0];

        direc_type directory[100];

        int i;

        read_char_from_block_offset(blockNumber,0,directory,current_inode.size);

        for(i = 0; i < current_inode.size/sizeof(direc_type); i++)

        {

                if(strcmp(dirName,directory[i].filename)==0){

                        Inode dir = get_free_inode(directory[i].inode);

                        if(dir.flags ==( 1<<14 | 1<<15)){

                                if (strcmp(dirName, ".") == 0) {

                                        return;

                                }

                                else if (strcmp(dirName, "..") == 0) {

                                        current_inode_number=directory[i].inode;

                                        int lastSlashPosition = last_index(pwd, '/');

                                        char temp[100];

                                        path_slicer(pwd, temp, 0, lastSlashPosition-1);

                                        strcpy(pwd, temp);

                                }

                                else {

                                        current_inode_number=directory[i].inode;

                                        strcat(pwd, "/");

                                        strcat(pwd, dirName);

                                }

                        }

                        else{

                                printf("\n%s\n","NOT A DIRECTORY!");

                        }

                        return;

                }

        }

}



void extfs_intfs(char* sourcePath, char* filename){

        int source_file_descriptor,blockNumber;

        if((source_file_descriptor = open(sourcePath,O_RDWR|O_CREAT,0600))== -1)

        {

                printf("\n file opening error [%s]\n",strerror(errno));

                return;

        }



        int inode_nums = getFreeInode();

        Inode int_file;

        int_file.flags = 1<<15; // setting 15th bit to 1, 15: allocated

        int_file.nlinks = 1;

        int_file.uid = 0;

        int_file.gid = 0;

        int_file.size = 0;

        int_file.actime = time(NULL);

        int_file.modtime = time(NULL);

// reAD source and copy to desti, block by block

        int bytesRead = BLOCK_SIZE;

        char buffer[BLOCK_SIZE] = {0};

        int i = 0;

        while(bytesRead == BLOCK_SIZE){

                bytesRead = read(source_file_descriptor,buffer,BLOCK_SIZE);

                int_file.size += bytesRead;

                blockNumber = get_block_from_free_list();

                int_file.addr[i++] = blockNumber;

                write_in_to_blocks(blockNumber, buffer, bytesRead);

        }

        write_to_inode(inode_nums,int_file);



        Inode current_inode = get_free_inode(current_inode_number);

        blockNumber = current_inode.addr[0];

        direc_type newFile;

        newFile.inode = inode_nums ;

        strcpy(newFile.filename,filename);

        write_in_to_blocksOffset(blockNumber,current_inode.size,&newFile,sizeof(direc_type));

        current_inode.size += sizeof(direc_type);

        write_to_inode(current_inode_number,current_inode);

}





void intfs_extfs(char* destinationPath, char* filename){

        int dest_file_descriptor,blockNumber,x,i;

        char buffer[BLOCK_SIZE] = {0};

        if((dest_file_descriptor = open(destinationPath,O_RDWR|O_CREAT,0600))== -1)

        {

                printf("\n file opening error [%s]\n",strerror(errno));

                return;

        }



        Inode current_inode = get_free_inode(current_inode_number);

        blockNumber = current_inode.addr[0];

        direc_type directory[100];

        read_char_from_block_offset(blockNumber,0,directory,current_inode.size);

        for(i = 0; i < current_inode.size/sizeof(direc_type); i++)

        {

                if(strcmp(filename,directory[i].filename)==0){

                        Inode file = get_free_inode(directory[i].inode);

	unsigned short* source = file.addr;

                        if(file.flags ==(1<<15)){

                                for(x = 0; x<file.size/BLOCK_SIZE; x++)

                                {

                                        blockNumber = source[x];

                                        read_char_from_block_offset(blockNumber, 0, buffer, BLOCK_SIZE);

                                        write(dest_file_descriptor,buffer,BLOCK_SIZE);

                                }

                                blockNumber = source[x];

                                read_char_from_block_offset(blockNumber, 0, buffer, file.size%BLOCK_SIZE);

                                write(dest_file_descriptor,buffer,file.size%BLOCK_SIZE);



                        }

                        else{

                                printf("\n%s\n","NOT A FILE!");

                        }

                        return;

                }

        }

}


void directory_delete(char* filename){

        int blockNumber,x,i;

        Inode current_inode = get_free_inode(current_inode_number);

        blockNumber = current_inode.addr[0];

        direc_type directory[100];

        read_char_from_block_offset(blockNumber,0,directory,current_inode.size);



        for(i = 0; i < current_inode.size/sizeof(direc_type); i++)

        {

                if(strcmp(filename,directory[i].filename)==0){

                        Inode file = get_free_inode(directory[i].inode);

                         if(file.flags ==( 1<<14 | 1<<15)){

                                for(x = 0; x<file.size/BLOCK_SIZE; x++)

                                {

                                        blockNumber = file.addr[x];

                                        add_block_to_free_list(blockNumber);

                                }

                                if(0<file.size%BLOCK_SIZE){

                                        blockNumber = file.addr[x];

                                        add_block_to_free_list(blockNumber);

                                }

                                addFreeInode(directory[i].inode);

                                directory[i]=directory[(current_inode.size/sizeof(direc_type))-1];

                                current_inode.size-=sizeof(direc_type);

                                write_in_to_blocks(current_inode.addr[0],directory,current_inode.size);

                                write_to_inode(current_inode_number,current_inode);

                            }

                         else{

                                printf("\n%s\n","NOT A DIRECTORY!");

                         }

                        return;

                }

        }



}


void file_delete(char* filename){

        int blockNumber;

        int x,i;

        Inode current_inode = get_free_inode(current_inode_number);

        blockNumber = current_inode.addr[0];

        direc_type directory[100];

        read_char_from_block_offset(blockNumber,0,directory,current_inode.size);



        for(i = 0; i < current_inode.size/sizeof(direc_type); i++)

        {

                if(strcmp(filename,directory[i].filename)==0){

                        Inode file = get_free_inode(directory[i].inode);

                        if(file.flags ==(1<<15)){

                                for(x = 0; x<file.size/BLOCK_SIZE; x++)

                                {

                                        blockNumber = file.addr[x];

                                        add_block_to_free_list(blockNumber);

                                }

                                if(0<file.size%BLOCK_SIZE){

                                        blockNumber = file.addr[x];

                                        add_block_to_free_list(blockNumber);

                                }

                                addFreeInode(directory[i].inode);

                                directory[i]=directory[(current_inode.size/sizeof(direc_type))-1];

                                current_inode.size-=sizeof(direc_type);

                                write_in_to_blocks(current_inode.addr[0],directory,current_inode.size);

                                write_to_inode(current_inode_number,current_inode);

                        }

                        else{

                                printf("\n%s\n","NOT A FILE!");

                        }

                        return;

                }

        }



}


int openfs(const char *filename)                //opens the file system we request

{

	file_descriptor=open(filename,2);

	lseek(file_descriptor,BLOCK_SIZE,SEEK_SET);

	read(file_descriptor,&superBlock,sizeof(superBlock));

	lseek(file_descriptor,2*BLOCK_SIZE,SEEK_SET);

        Inode root = get_free_inode(1);

	read(file_descriptor,&root,sizeof(root));

	return 1;

}

void initfs(char* path, int total_blocks, int total_inodes)

{

        printf("\n filesystem intialization started \n");

        total_inodes_count = total_inodes;

        char emptyBlock[BLOCK_SIZE] = {0};

        int no_of_bytes;
        int i,blockNumber,inode_nums;



        //init isize (Number of blocks for inode

        if(((total_inodes*INODE_SIZE)%BLOCK_SIZE) == 0) // 300*64 % 1024

                superBlock.isize = (total_inodes*INODE_SIZE)/BLOCK_SIZE;

        else

                superBlock.isize = (total_inodes*INODE_SIZE)/BLOCK_SIZE+1;



        //init fsize

        superBlock.fsize = total_blocks;



        //create file for File System

        if((file_descriptor = open(path,O_RDWR|O_CREAT,0600))== -1)

        {

                printf("\n file opening error [%s]\n",strerror(errno));

                return;

        }

        strcpy(fileSystemPath,path);



        write_in_to_blocks(total_blocks-1,emptyBlock,BLOCK_SIZE); // writing empty block to last block



        // add all blocks to the free array

        superBlock.nfree = 0;

        for (blockNumber= 1+superBlock.isize; blockNumber< total_blocks; blockNumber++)

                add_block_to_free_list(blockNumber);



        // add free Inodes to inode array

        superBlock.ninode = 0;

        for (inode_nums=1; inode_nums < total_inodes ; inode_nums++)

                addFreeInode(inode_nums);





        superBlock.flock = 'f';

        superBlock.ilock = 'i';

        superBlock.fmod = 0;

        superBlock.time[0] = 0;

        superBlock.time[1] = 1970;



        //write Super Block

        write_in_to_blocks (0,&superBlock,BLOCK_SIZE);



        //allocate empty space for i-nodes

        for (i=1; i <= superBlock.isize; i++)

                write_in_to_blocks(i,emptyBlock,BLOCK_SIZE);



        create_root();

}



void terminate()

{

        close(file_descriptor);

        exit(0);

}





int main(int argc, char *argv[])

{

        //char spell;
        printf("\n Clearing screen \n");

        system("clear");



        unsigned int bloc_nums =0, nums_of_inode=0;

        char *path_of_fs;

        char  enter_input[512];

        char *number1, *argument2;

        char *splitter;



        while(1)

        {

                printf("\nThis program accepts the following commands with specified parameters\n");
                printf("1. initfs <file_name_to_represent_disk> <total_no_of_blocks> <total_no_of_inodes>\n");
                printf("2. ls\n");
                printf("3. mkdir <file_system_file_name>\n");
                printf("4. cd <changed_directory_file_system_file_name>\n");
                printf("5. q\n");
                printf("6. cpin <external_file_name> <file_system_file_name>\n");
                printf("7. cout <file_system_file_name> <external_file_name>\n");
                printf("8. pwd\n");
                printf("9. rm <file_system_file_name>\n");
                printf("10. rmd <file_system_directory_name>\n");
                printf("11. openfs <file_system_name>\n");

                scanf(" %[^\n]s", enter_input);

                splitter = strtok(enter_input," ");

                if(strcmp(splitter, "initfs")==0)

                {



                        path_of_fs = strtok(NULL, " ");

                        number1 = strtok(NULL, " ");

                        argument2 = strtok(NULL, " ");

                        if(access(path_of_fs, X_OK) != -1)

                        {


                            if((file_descriptor=open(path_of_fs,O_RDWR,0600))==-1){

                             printf("filesystem already exists. but error in opening\n");

                            }


                                printf("same file system will be used\n");




                        }

                        else

                        {

                                if (!number1 || !argument2)

                                        printf(" all arguments not entered to proceed\n");

                                else

                                {

                                        bloc_nums = atoi(number1);

                                        nums_of_inode = atoi(argument2);

                                        initfs(path_of_fs,bloc_nums, nums_of_inode);

                                }

                        }

                        splitter = NULL;

                }

                else if(strcmp(splitter, "ls")==0){

                        list_contents();

                }

                else if(strcmp(splitter, "mkdir")==0){

                        number1 = strtok(NULL, " ");

                        makedir(number1);

                }

                else if(strcmp(splitter, "cd")==0){

                        number1 = strtok(NULL, " ");

                        directory_change(number1);

                }

                else if(strcmp(splitter, "q")==0){

                        terminate();

                }

                else if(strcmp(splitter, "cpin")==0){

                        number1 = strtok(NULL, " ");

                        argument2 = strtok(NULL, " ");

                        extfs_intfs(number1,argument2);

                }

                else if(strcmp(splitter, "cpout")==0){

                        number1 = strtok(NULL, " ");

                        argument2 = strtok(NULL, " ");

                        intfs_extfs(number1,argument2);

                }

                else if(strcmp(splitter, "pwd")==0){

                        printf("%s\n",pwd);

                }

                else if(strcmp(splitter, "rm")==0){

                        number1 = strtok(NULL, " ");

                        file_delete(number1);

                }

                else if(strcmp(splitter, "rmd")==0){

                        number1 = strtok(NULL, " ");

                        directory_delete(number1);

                }

                else if(strcmp(splitter, "openfs")==0){

                        number1 = strtok(NULL, " ");

                        openfs(number1);

                }

               printf("\nFilesystem: %s Present working Directory: %s>>",fileSystemPath,pwd);


        }

}
