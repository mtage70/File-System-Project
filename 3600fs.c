/*
 * CS3600, Spring 2014
 * Project 2
 *
 * This file contains all of the basic functions that you will need 
 * to implement for this project.  Please see the project handout
 * for more details on any particular function, and ask on Piazza if
 * you get stuck.
 */

#define FUSE_USE_VERSION 26

#ifdef linux
/* For pread()/pwrite() */
#define _XOPEN_SOURCE 500
#endif

#define _POSIX_C_SOURCE 199309

#include <time.h>
#include <fuse.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <assert.h>
#include <sys/statfs.h>

#ifdef HAVE_SETXATTR
#include <sys/xattr.h>
#endif

#include "3600fs.h"
#include "disk.h"

/*
 * Initialize filesystem. Read in file system metadata and initialize
 * memory structures. If there are inconsistencies, now would also be
 * a good time to deal with that. 
 *
 * HINT: You don't need to deal with the 'conn' parameter AND you may
 * just return NULL.
 *
 */


//HELPER METHODS FOR USING THE STRUCTS 
vcb readvcb(){
  vcb vb;
  char temp_vb[BLOCKSIZE];
  memset(temp_vb, 0, BLOCKSIZE);
  dread(0, temp_vb);
  memcpy(&vb, temp_vb, sizeof(vcb));
  return vb;
}

void writevcb(vcb vb){
  char temp_vb[BLOCKSIZE];
  memset(temp_vb, 0, BLOCKSIZE);
  memcpy(temp_vb, &vb, sizeof(vcb));
  dwrite(0, temp_vb);
}

dirent readdirent(int idx){
  dirent de;
  char temp_de[BLOCKSIZE];
  memset(temp_de, 0, BLOCKSIZE);
  dread(idx, temp_de);
  memcpy(&de, temp_de, sizeof(de));
  return de;
}

void writedirent(int idx, dirent de){
  char temp_de[BLOCKSIZE];
  memset(temp_de,0,BLOCKSIZE);
  memcpy(temp_de,&de,sizeof(de));
  dwrite(idx, temp_de);
}

fatent readfe(int offset){
  vcb myvcb = readvcb();
  char block[BLOCKSIZE];
  memset(block, 0, BLOCKSIZE);
  dread(((int)(offset/128) + myvcb.fat_start),block);
  if(offset > 128){
    offset = offset % 128;
  }
  fatent fe;
  memcpy(&fe, &block[offset], sizeof(fe));
  return fe;
}

// appends new FAT entry onto a FAT entry
int add_fat(fatent* fe){
  vcb myvcb = readvcb();
  fatent free_fatent[128];
  
  char blk[BLOCKSIZE];
  //find free entry
  for(int count_fat_blocks = 0; count_fat_blocks < (myvcb.fat_length/128);
          count_fat_blocks++){
    
    memset(blk,0,BLOCKSIZE);
    dread(myvcb.fat_start + count_fat_blocks, blk);
    
    for(int i = 0; i < 128; i++){
      memcpy(&free_fatent[i], &blk[i*4], sizeof(fatent));
    }
    for(int j = 0; j < 128; j++){
      if(free_fatent[j].used == 0){
	    // change end of file and append at ->next
	    fe->eof = 0;
	    fe->next = j + (count_fat_blocks * 128);
	    free_fatent[j].eof = 1;
	    free_fatent[j].next = 0;
        free_fatent[j].used = 1;
	    memcpy(&blk[j*4], &free_fatent[j], sizeof(fatent));
	    dwrite(myvcb.fat_start + count_fat_blocks, blk);
	    return 0;
      }
    }
  }
  return -1;
}

// get index of fe that contains eof
int get_eof_fe(fatent* fe){
  int eof_dblock = 0;
  fatent tmp_fe;
  while(fe->eof != 1){
    eof_dblock = fe->next;
    tmp_fe = readfe(fe->next);
    fe = &tmp_fe;
  }
  return eof_dblock;
}
 
// helper function to make sure the path given is valid
int valid_path(const char* path){
  const char* tmp = path;
  int slashes = 0;

  while(*tmp){
    if(*tmp == '/')
      slashes++;
    tmp++;
  }
  if(slashes != 1)
    return -1;
  else
    return 0;	
} 
 
static void* vfs_mount(struct fuse_conn_info *conn) {
  fprintf(stderr, "vfs_mount called\n");

  // Do not touch or move this code; connects the disk
  dconnect();

  /* 3600: YOU SHOULD ADD CODE HERE TO CHECK THE CONSISTENCY OF YOUR DISK
           AND LOAD ANY DATA STRUCTURES INTO MEMORY */
           
  //allocate your VCB
  vcb myvcb = readvcb();
  

  // invalid magic number
  if (myvcb.magic != MAGICNUM) {
    fprintf(stderr, "Invalid Disk: Unrecognized ID.\n");
  }
  // Disk was not unmounted cleanly last time.
  else if (myvcb.dirty) { 
    fprintf(stderr, "Error: Disk was not unmounted properly.\n");
  }
  
  // disk is dirty until it is unmounted.
  myvcb.dirty = 1;
  
  //write modified vcb back to disk
  writevcb(myvcb);

  return NULL;
}

/*
 * Called when your file system is unmounted.
 *
 */
static void vfs_unmount (void *private_data) {
  fprintf(stderr, "vfs_unmount called\n");

  /* 3600: YOU SHOULD ADD CODE HERE TO MAKE SURE YOUR ON-DISK STRUCTURES
           ARE IN-SYNC BEFORE THE DISK IS UNMOUNTED (ONLY NECESSARY IF YOU
           KEEP DATA CACHED THAT'S NOT ON DISK */
                      
  // Read VCB from disk.
  vcb myvcb = readvcb();
  // set dirty to 0 to show success in unmounting
  myvcb.dirty = 0;
  // write modified VCB back to the disk
  writevcb(myvcb);


  // Do not touch or move this code; unconnects the disk
  dunconnect();
}

/* 
 *
 * Given an absolute path to a file/directory (i.e., /foo ---all
 * paths will start with the root directory of the CS3600 file
 * system, "/"), you need to return the file attributes that is
 * similar stat system call.
 *
 * HINT: You must implement stbuf->stmode, stbuf->st_size, and
 * stbuf->st_blocks correctly.
 *
 */
static int vfs_getattr(const char *path, struct stat *stbuf) {
  fprintf(stderr, "vfs_getattr called\n");

  // Do not mess with this code 
  stbuf->st_nlink = 1; // hard links
  stbuf->st_rdev  = 0;
  stbuf->st_blksize = BLOCKSIZE;

  /* 3600: YOU MUST UNCOMMENT BELOW AND IMPLEMENT THIS CORRECTLY */
  vcb myvcb = readvcb();
  //path is the root directory
  if (strcmp(path, "/") == 0) {
    struct tm * t1;
    struct tm * t2;
    struct tm * t3;
    t1 = localtime(&((myvcb.access_time).tv_sec));
    t2 = localtime(&((myvcb.modify_time).tv_sec));
    t3 = localtime(&((myvcb.create_time).tv_sec));
    
    stbuf->st_mode = 0777 | S_IFDIR;
    
    stbuf->st_uid = myvcb.user;
    stbuf->st_gid = myvcb.group;
    stbuf->st_atime = mktime(t1);
    stbuf->st_mtime = mktime(t2);
    stbuf->st_ctime = mktime(t3);
    stbuf->st_size = BLOCKSIZE;
    stbuf->st_blocks = 1;
    return 0;
  }
  else {
    //if the path isn't valid   
    if (valid_path(path) != 0) {
        return -1;                      
    }
    path++;
    dirent de;
    //loop through the DE's
    for (int i = 1; i < 101; i++) {
      
      de = readdirent(i);
      
      if (de.valid == 1) {
        //if this de's name == path take it's info
        if (strcmp(de.name, path) == 0) {                   
          struct tm * tm1;
	      struct tm * tm2;
	      struct tm * tm3;
	      tm1 = localtime(&((de.access_time).tv_sec));
	      tm2 = localtime(&((de.modify_time).tv_sec));
	      tm3 = localtime(&((de.create_time).tv_sec));                                
          stbuf->st_mode  = de.mode | S_IFREG;
          stbuf->st_uid = de.user;
          stbuf->st_gid = de.group;
	      stbuf->st_atime = mktime(tm1);
	      stbuf->st_mtime = mktime(tm2);
	      stbuf->st_ctime = mktime(tm3);
	      stbuf->st_size = de.size;
	      stbuf->st_blocks = (de.size / BLOCKSIZE);
	      return 0;
       }
      }
      
    }
    
    return -ENOENT;
   } 
}   


/*
 * Given an absolute path to a directory (which may or may not end in
 * '/'), vfs_mkdir will create a new directory named dirname in that
 * directory, and will create it with the specified initial mode.
 *
 * HINT: Don't forget to create . and .. while creating a
 * directory.
 */
/*
 * NOTE: YOU CAN IGNORE THIS METHOD, UNLESS YOU ARE COMPLETING THE 
 *       EXTRA CREDIT PORTION OF THE PROJECT.  IF SO, YOU SHOULD
 *       UN-COMMENT THIS METHOD.
static int vfs_mkdir(const char *path, mode_t mode) {

  return -1;
} */

/** Read directory
 *
 * Given an absolute path to a directory, vfs_readdir will return 
 * all the files and directories in that directory.
 *
 * HINT:
 * Use the filler parameter to fill in, look at fusexmp.c to see an example
 * Prototype below
 *
 * Function to add an entry in a readdir() operation
 *
 * @param buf the buffer passed to the readdir() operation
 * @param name the file name of the directory entry
 * @param stat file attributes, can be NULL
 * @param off offset of the next entry or zero
 * @return 1 if buffer is full, zero otherwise
 * typedef int (*fuse_fill_dir_t) (void *buf, const char *name,
 *                                 const struct stat *stbuf, off_t off);
 *			   
 * Your solution should not need to touch fi
 *
 */
static int vfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                       off_t offset, struct fuse_file_info *fi)
{
  
  if(strcmp(path, "/") == 0){
    vcb myvcb = readvcb();
    
    for(int i = myvcb.de_start; i < myvcb.de_start + myvcb.de_length; i++){
      dirent de = readdirent(i);
      if(filler(buf, de.name, NULL, 0) != 0){
	    return -ENOMEM;
      }
    }
    return 0;
  }
  else {
    return -1;
  }
}


/*
 * Given an absolute path to a file (for example /a/b/myFile), vfs_create 
 * will create a new file named myFile in the /a/b directory.
 *
 */
static int vfs_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
  
  vcb myvcb = readvcb();
  // check if path is valid
  if(valid_path(path) != 0) {
    return -1;
  }
  path++;
  // number of free dirent spaces
  int free_count = 0; 
  // index of first free dirent space
  int first_free = 0; 
  
  //search for duplicate and empty dirent space
  for(int i = myvcb.de_start; i < myvcb.de_start + myvcb.de_length; i++){
    dirent de = readdirent(i);
    
    if((de.valid == 1) && (strcmp(de.name, path) == 0)){
      return -EEXIST;
    } 
    else{
      if(free_count == 0) {
        first_free = i;
      }
      free_count++;
    }
  }
  
  // if there is free space
  if(free_count != 0){ 
    //creating the new directory entry
    dirent new_de; 
    new_de.valid = 1;
    new_de.first_block = -1; 
    new_de.size = 0;
    new_de.user = getuid();
    new_de.group = getgid();
    new_de.mode = mode;

    struct timespec new_time;
    clock_gettime(CLOCK_REALTIME, &new_time);
    new_de.access_time = new_time;
    new_de.modify_time = new_time;
    new_de.create_time = new_time;
    
    char new_name[512 - (3 * sizeof(struct timespec)) - 24]; 
    memset(new_name, 0, (512 - (3 * sizeof(struct timespec))- 24));
    strcpy(new_name, path); 
    strcpy(new_de.name, new_name);

    writedirent(first_free, new_de);
    return 0;
    
  }
  //no free space
  return -1; 
}

/*
 * The function vfs_read provides the ability to read data from 
 * an absolute path 'path,' which should specify an existing file.
 * It will attempt to read 'size' bytes starting at the specified
 * offset (offset) from the specified file (path)
 * on your filesystem into the memory address 'buf'. The return 
 * value is the amount of bytes actually read; if the file is 
 * smaller than size, vfs_read will simply return the most amount
 * of bytes it could read. 
 *
 * HINT: You should be able to ignore 'fi'
 *
 */
static int vfs_read(const char *path, char *buf, size_t size, off_t offset,
                    struct fuse_file_info *fi)
{
  
  if(valid_path(path) != 0) {
    return -1;
  }
  
  vcb myvcb = readvcb();
  
  path++;
  
  for(int i = myvcb.de_start; i < myvcb.de_start + myvcb.de_length; i++){
    dirent de;
    de = readdirent(i);
    
    if(de.valid == 1){
      if(strcmp(path, de.name) == 0){
        int bytes = 0;
        int fat_start = myvcb.fat_start;
        int fe_offset = de.first_block;

        int db_idx = myvcb.db_start + fe_offset;
        int offset_dblock = ((int) (offset / 512));
        int offset_in_block = offset % 512;
        
        char dbk_temp[BLOCKSIZE];
        memset(dbk_temp, 0, BLOCKSIZE);
        dread(db_idx + offset_dblock, dbk_temp);
        
        // read from the first block
        for(int i = offset_in_block; (i < BLOCKSIZE) || (size > 0); i++){
	      char* strg; 
	      strcpy(strg, dbk_temp[i]);
	      strcat(buf, strg);
	      bytes++;
	      size--;
        }

	    //read from following blocks
        while(size >= BLOCKSIZE){
          //increment to next block
	      db_idx++; 
	      //load next block into memory
	      memset(dbk_temp, 0, BLOCKSIZE);
	      dread(db_idx, dbk_temp);

	      for(int i = 0; i < BLOCKSIZE; i++){
	        char* strg;
	        strcpy(strg, dbk_temp[i]);
	        strcat(buf, strg);
	        bytes++;
	      } 
	      size -= BLOCKSIZE;
       }

       // read final bytes
       for(int i = 0; i < size; i++){
	     char* strg;
         strcpy(strg, dbk_temp[i]);
	     strcat(buf, strg);
	     bytes++;
	   }        
       return bytes;
      }	
    }
  }
  return -1;

  
}

/*
 * The function vfs_write will attempt to write 'size' bytes from 
 * memory address 'buf' into a file specified by an absolute 'path'.
 * It should do so starting at the specified offset 'offset'.  If
 * offset is beyond the current size of the file, you should pad the
 * file with 0s until you reach the appropriate length.
 *
 * You should return the number of bytes written.
 *
 * HINT: Ignore 'fi'
 */
static int vfs_write(const char *path, const char *buf, size_t size,
                     off_t offset, struct fuse_file_info *fi)
{

  /* 3600: NOTE THAT IF THE OFFSET+SIZE GOES OFF THE END OF THE FILE, YOU
           MAY HAVE TO EXTEND THE FILE (ALLOCATE MORE BLOCKS TO IT). */
  
  if(valid_path(path) != 0){
    return -1;
  }
  vcb myvcb = readvcb();
  path++;
  // # of bytes we've written to disk from buffer               
  int byteswritten = 0; 
  // # 0's to pad between end of file and the offset
  int padding = 0;      
  int wt_offset = 0; 
  dirent de;
  int found_dirent = 0;
  
  for(int i = myvcb.de_start; i < myvcb.de_start + myvcb.de_length || found_dirent == 0; i++){
    de = readdirent(i);
    //if dirent is valid and same name as path
    if((de.valid == 1) && (strcmp(path, de.name) == 0)) {
        found_dirent = 1;      
    }
  }

  if(found_dirent){	
    //check for padding                
    if(offset > de.size){ 
      padding = (offset - de.size);
    }
    if((size + offset) > de.size){
      //modify size of file if necessary
      de.size = (size + offset);
    }
    struct timespec ntime;
    clock_gettime(CLOCK_REALTIME, &ntime);
    de.access_time = ntime;
    de.modify_time = ntime;
    
    if(((int) de.first_block) == -1){
      char blk[BLOCKSIZE];
      int found_free = 0;
      //go through fat blocks
      for(int i = myvcb.fat_start;(i < myvcb.fat_start +
       ((int) (myvcb.fat_length/128))) && found_free == 0; i++){               
	    int block_idx = (i - myvcb.fat_start) * 128;
	    memset(blk, 0, BLOCKSIZE); 
	    //read in blk to block
	    dread(i, blk);
	    for(int k = 0; k < 128 && found_free == 0; k++){
	      fatent fe = readfe(block_idx + k);
	      if(fe.used == 0){
	        de.first_block = block_idx + k;
	        fe.used = 1;
	        fe.eof = 1;
	        fe.next = 0;
	        found_free = 1;
	      }
	    }
      }
      if(found_free != 1){
        return -ENOSPC;
      }
    }
    fatent fe = readfe(de.first_block);
    
    char blk[BLOCKSIZE];
    memset(blk, 0, BLOCKSIZE); 
    // expand and padding
    if(padding > 0){
      //index of EOF
      int eof_fat_idx = get_eof_fe(&fe) + myvcb.db_start;
      dread(eof_fat_idx, blk);
      int eof_data_idx;
      for(int i = 0; blk[i] != EOF; i++){
        eof_data_idx++;
      }
      //increments to EOF index
      eof_data_idx++; 

	  while(padding > 0){
        if(eof_data_idx < BLOCKSIZE){
          memset(blk[eof_data_idx], 0, 1);
          eof_data_idx++;
          padding--;
          if(padding == 0){
            dwrite(eof_data_idx, blk);
            memset(blk, 0, BLOCKSIZE);
          }
        } 
	    else{
	      dwrite(eof_fat_idx, blk);
          memset(blk, 0, BLOCKSIZE);
	      if(add_fat(&fe) != 0){
		    return -ENOSPC;
	      }
	      eof_fat_idx = get_eof_fe(&fe) + myvcb.db_start;
	      eof_data_idx = 0;
	    }
	  }
    }    
            
    //where offset is in the datablock.
    int offset_block = (int)(offset / 512);
    int offset_into_block = offset % 512;
    int buffer_offset = 0;
    
    memset(blk, 0, BLOCKSIZE);
    dread(offset_block + myvcb.db_start, blk);
    
    while((offset_into_block < BLOCKSIZE) && (size > 0)){
      memcpy(&blk[offset_into_block], &buf, 1);
      size--;
      buffer_offset++;
      offset_into_block++;
      byteswritten++;
    }

    
    dwrite(offset_block + myvcb.db_start, blk);
    while(size > 0){
      if(offset_into_block == BLOCKSIZE){
	    
	    dwrite(offset_block + myvcb.db_start, blk);
	    // add new fat
	    if(add_fat(&fe) != 0){
	      return -ENOSPC;
	    }
	    // reset offset_into_block
	    offset_into_block = 0;
	    offset_block = get_eof_fe(&fe);
      }
      
      memset(blk, 0, BLOCKSIZE);
      memcpy(&blk[offset_into_block], &buf[buffer_offset], 1);
      
      size--;
      buffer_offset++;
      offset_into_block++;
      byteswritten++;
    }

    // write in rest of size bytes
    dwrite(offset_block + myvcb.db_start, blk);
    return byteswritten;
  }
  else{
    // no free de's
    return -1;
  }
  

  return 0;
}

/**
 * This function deletes the last component of the path (e.g., /a/b/c you 
 * need to remove the file 'c' from the directory /a/b).
 */
static int vfs_delete(const char *path)
{

  /* 3600: NOTE THAT THE BLOCKS CORRESPONDING TO THE FILE SHOULD BE MARKED
           AS FREE, AND YOU SHOULD MAKE THEM AVAILABLE TO BE USED WITH OTHER FILES */
  if (strcmp(path, "/") == 0) { return -1; }
  vcb myvcb = readvcb();
  if(valid_path(path) != 0) {
    return -1;
  }
  path++; 

  
  for(int i = myvcb.de_start; i < myvcb.de_start + myvcb.de_length; i++){
    dirent de = readdirent(i); 
    if(strcmp(de.name, path) == 0 && de.valid == 1){
      de.valid = 0;
      de.name[0] = '\0';
      
      writedirent(i, de);
      return 0;
    }
  }
   return -EEXIST;

    return 0;
}

/*
 * The function rename will rename a file or directory named by the
 * string 'oldpath' and rename it to the file name specified by 'newpath'.
 *
 * HINT: Renaming could also be moving in disguise
 *
 */
static int vfs_rename(const char *from, const char *to)
{

 vcb myvcb = readvcb();

  if(valid_path(from) != 0)
    return -1;

  from++;
  for(int i = myvcb.de_start; i < myvcb.de_start + myvcb.de_length; i++){
    dirent de = readdirent(i);
    if(strcmp(de.name, from) == 0){
      strcpy(de.name, to);
      writedirent(i, de);
      return 0;
    }
  }
  return -1;
}


/*
 * This function will change the permissions on the file
 * to be mode.  This should only update the file's mode.  
 * Only the permission bits of mode should be examined 
 * (basically, the last 16 bits).  You should do something like
 * 
 * fcb->mode = (mode & 0x0000ffff);
 *
 */
static int vfs_chmod(const char *file, mode_t mode)
{

  vcb myvcb = readvcb();
  
  if(valid_path(file) != 0) {
    return -1;
  }
  
  file++;
  for(int i = myvcb.de_start; i < myvcb.de_start + myvcb.de_length; i++){
    dirent de = readdirent(i);
    if(strcmp(de.name, file) == 0){
      //de.mode = (mode & 0x0000ffff);
      de.mode = mode;
      writedirent(i, de);
      return 0;
    }
  }
  return -1;
}

/*
 * This function will change the user and group of the file
 * to be uid and gid.  This should only update the file's owner
 * and group.
 */
static int vfs_chown(const char *file, uid_t uid, gid_t gid)
{

vcb myvcb = readvcb();
  
  if(valid_path(file) != 0){
    return -1;
  }
  file++;
  
  for(int i = myvcb.de_start; i < myvcb.de_start + myvcb.de_length; i++){
    dirent de = readdirent(i);
    if(strcmp(de.name, file) == 0){
      de.user = uid;
      de.group = gid;
      writedirent(i, de);
      return 0;
    }
  }
  return -1;
}


static int vfs_utimens(const char *file, const struct timespec ts[2])
{

vcb myvcb = readvcb();
  
  if(valid_path(file) != 0) {
    return -1;
  }  
  file++;
  
  for(int i = myvcb.de_start; i < myvcb.de_start + myvcb.de_length; i++){
    dirent de = readdirent(i);
    if(strcmp(de.name, file) == 0){
      de.access_time = ts[0];
      de.modify_time = ts[1];
      writedirent(i, de);
      return 0; 
    }
  }
  return -1; 
}

/*
 * This function will truncate the file at the given offset
 * (essentially, it should shorten the file to only be offset
 * bytes long).
 */
static int vfs_truncate(const char *file, off_t offset)
{

   /* 3600: NOTE THAT ANY BLOCKS FREED BY THIS OPERATION SHOULD
           BE AVAILABLE FOR OTHER FILES TO USE. */
  if(valid_path(file) != 0){
    return -1;
  }
  
  return 0;
}


/*
 * You shouldn't mess with this; it sets up FUSE
 *
 * NOTE: If you're supporting multiple directories for extra credit,
 * you should add 
 *
 *     .mkdir	 = vfs_mkdir,
 */
static struct fuse_operations vfs_oper = {
    .init    = vfs_mount,
    .destroy = vfs_unmount,
    .getattr = vfs_getattr,
    .readdir = vfs_readdir,
    .create	 = vfs_create,
    .read	 = vfs_read,
    .write	 = vfs_write,
    .unlink	 = vfs_delete,
    .rename	 = vfs_rename,
    .chmod	 = vfs_chmod,
    .chown	 = vfs_chown,
    .utimens	 = vfs_utimens,
    .truncate	 = vfs_truncate,
};

int main(int argc, char *argv[]) {
    /* Do not modify this function */
    umask(0);
    if ((argc < 4) || (strcmp("-s", argv[1])) || (strcmp("-d", argv[2]))) {
      printf("Usage: ./3600fs -s -d <dir>\n");
      exit(-1);
    }
    return fuse_main(argc, argv, &vfs_oper, NULL);
}
