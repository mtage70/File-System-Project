/*
 * CS3600, Spring 2014
 * Project 2
 *
 * This program is intended to format your disk file, and should be executed
 * BEFORE any attempt is made to mount your file system.  It will not, however
 * be called before every mount (you will call it manually when you format 
 * your disk file).
 */
#include <math.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "3600fs.h"
#include "disk.h"

//creates a fatent
fatent create_fatent(){
  fatent fe;
  fe.used = 0;
  return fe;
}

//creates a dirent
dirent create_dirent(){
  dirent de;
  de.valid = 0;
  de.size = 0;
  return de;
}
//method creates a vcb
vcb create_vcb(int dbs){
  vcb myvcb;
  myvcb.magic = 7;
  myvcb.dirty = 0;
  myvcb.blocksize = BLOCKSIZE;
  myvcb.de_start = 1;
  myvcb.de_length = 100;
  myvcb.fat_start = myvcb.de_length + myvcb.de_start + 1;
  myvcb.fat_length = dbs;
  myvcb.db_start = ((int)myvcb.fat_length/128) + myvcb.fat_start + 1;
  
  myvcb.user = getuid();
  myvcb.group = getgid();
  myvcb.mode = 0777;
  
  //clock_gettime(CLOCK_REALTIME, &myvcb.access_time);
  //clock_gettime(CLOCK_REALTIME, &myvcb.modify_time);
  //clock_gettime(CLOCK_REALTIME, &myvcb.create_time);
  
  return myvcb;
}

void myformat(int size) {
  // Do not touch or move this function
  dcreate_connect();

  /* 3600: FILL IN CODE HERE.  YOU SHOULD INITIALIZE ANY ON-DISK
           STRUCTURES TO THEIR INITIAL VALUE, AS YOU ARE FORMATTING
           A BLANK DISK.  YOUR DISK SHOULD BE size BLOCKS IN SIZE. */
           
           
  int fat_plus_db = size - 101;
  int dbs = fat_plus_db * 0.992248062;         
  //Volume Control Block First
  vcb myvcb = create_vcb(dbs);
  // copy vcb to a BLOCKSIZE-d location
  char temp [BLOCKSIZE];
  memset(temp, 0, BLOCKSIZE);
  memcpy(temp, &myvcb, BLOCKSIZE);
  //finally actually write it to disk in the 0th block
  dwrite(0, temp);
  
  //Directory Entries
  dirent de = create_dirent();
  char detemp[BLOCKSIZE];
  memset(detemp, 0, BLOCKSIZE);
  memcpy(detemp, &de, BLOCKSIZE);
  
  for(int i = myvcb.de_start; i < myvcb.de_start + myvcb.de_length; i++){
    dwrite(i, detemp);
  }
  
  //Fatents
  fatent fe = create_fatent();

  fatent fat_blk[128];

  int remain = myvcb.fat_length;
  int blk = myvcb.fat_start;

  while(remain > 0){
    for(int i = 0; i < 128; i++){
      fat_blk[i] = fe;
      remain--;
    }
    char fetemp[BLOCKSIZE];
    memset(fetemp, 0, BLOCKSIZE);
    memcpy(fetemp, &fat_blk, sizeof(fat_blk));
    dwrite(blk,fetemp);
    blk++;
  }

  char mtblk[BLOCKSIZE];
  memset(mtblk, 0, BLOCKSIZE);
  for(int i = 0; i < dbs; i++){
     dwrite((myvcb.db_start + i), mtblk);
  }
  
  // Do not touch or move this function
  dunconnect();
}

int main(int argc, char** argv) {
  // Do not touch this function
  if (argc != 2) {
    printf("Invalid number of arguments \n");
    printf("usage: %s diskSizeInBlockSize\n", argv[0]);
    return 1;
  }

  unsigned long size = atoi(argv[1]);
  printf("Formatting the disk with size %lu \n", size);
  myformat(size);
}
