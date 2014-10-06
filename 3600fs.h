/*
 * CS3600, Spring 2014
 * Project 2
 *
 */

#ifndef __3600FS_H__
#define __3600FS_H__

#define MAGICNUM 7

//magic number is 7

//struct for the VCB, first block of the file system
typedef struct vcb_s {
  // a magic number to identify your disk
  int magic;
  // "dirty" variable, 0 when unmounted correctly
  int dirty;

  // description of the disk layout
  int blocksize;
  int de_start;
  int de_length;
  int fat_start;
  int fat_length;
  int db_start;

  // metadata for the root directory
  uid_t user;
  gid_t group;
  mode_t mode;
  struct timespec access_time;
  struct timespec modify_time;
  struct timespec create_time;
} vcb;

// struct for Directory Entries
typedef struct dirent_s {
  unsigned int valid;
  unsigned int first_block;
  unsigned int size;
  uid_t user;
  gid_t group;
  mode_t mode;
  struct timespec access_time;
  struct timespec modify_time;
  struct timespec create_time;
  char name[512 - (3 * (sizeof(struct timespec))) - 24];
} dirent;

// struct for FAT
typedef struct fatent_s {
  unsigned int used:1; 
  unsigned int eof:1;
  unsigned int next:30;
} fatent;

#endif
