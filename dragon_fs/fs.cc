#include <stdlib.h>
#include <stdio.h>
#include <strings.h>
#include <errno.h>
#include <assert.h>
#include <bits/stdc++.h>
#include "inode.h"
using namespace std;

int next_inode_block = 0;
int inode_blocks_in_use = 0;
int next_data_block = 0;
int data_blocks_in_use = 0;

int get_free_inode_block(char *bitmap, int INODE_BLOCKS)
{
  if (inode_blocks_in_use == INODE_BLOCKS) {
    return -1;
  }
  while (bitmap[next_inode_block]) {
    if (next_inode_block == INODE_BLOCKS - 1) {
      next_inode_block = 0;
    } else {
      ++next_inode_block;
    }
  }
  bitmap[next_inode_block] = 1;
  ++inode_blocks_in_use;
  return next_inode_block++;
}

int get_free_data_block(char *bitmap, int TOTAL_BLOCKS, int INODE_BLOCKS)
{
  if (data_blocks_in_use == TOTAL_BLOCKS - INODE_BLOCKS) {
    return -1;
  }
  while (bitmap[INODE_BLOCKS + next_data_block]) {
    if (INODE_BLOCKS + next_data_block == TOTAL_BLOCKS - 1) {
      next_data_block = 0;
    } else {
      ++next_data_block;
    }
  }
  bitmap[next_data_block + INODE_BLOCKS] = 1;
  ++data_blocks_in_use;
  return INODE_BLOCKS + next_data_block++;
}

void write_char(char *rawdata, int &pos, char val)
{
  char *ptr = (char*)&rawdata[pos];
  *ptr = val;
  pos += sizeof(char);
}

void write_int(char *rawdata, int &pos, int val)
{
  int *ptr = (int *)&rawdata[pos];
  *ptr = val;
  pos += sizeof(char);
}

// true if done, false if we need to keep writing more of the file
bool write_iblock(char *rawdata, char *bitmap, int N, int M, FILE *fptr, char *buf, int &nbytes, int iblockno) {
  int iblock_pos = iblockno * BLOCK_SZ;
  int iblock_end = iblock_pos + BLOCK_SZ;
  while (iblock_pos < iblock_end) {
    int dblockno = get_free_data_block(bitmap, N, M);
    write_int(rawdata, iblock_pos, dblockno);

    int data_pos = dblockno * BLOCK_SZ;
    int bytes = fread(buf, sizeof(char), BLOCK_SZ, fptr);
    nbytes += bytes;

    for (int b = 0; b < bytes; ++b) {
      write_char(rawdata, data_pos, buf[b]);
    }
    if (bytes < BLOCK_SZ) {
      return true;
    }
  }
  return false;
}

void place_file(char *rawdata, char *bitmap, const char *file, int uid, int gid, int N, int M, int D, int I)
{
  int i, nbytes = 0;
  struct inode *ip;
  FILE *fptr;
  char buf[BLOCK_SZ];

  if (bitmap[D]) {
    perror("ERROR:\nspecified block is occupied");
    exit(-1);
  } else {
    bitmap[D] = 1;
  }

  ip = (struct inode *)malloc(sizeof(struct inode));
  int inode_pos = D * BLOCK_SZ + I;

  ip->mode = 0;
  write_int(rawdata, inode_pos, ip->mode);
  ip->nlink = 1;
  write_int(rawdata, inode_pos, ip->nlink);
  ip->uid = uid;
  write_int(rawdata, inode_pos, ip->uid);
  ip->gid = gid;
  write_int(rawdata, inode_pos, ip->gid);
  ip->ctime = random();
  write_int(rawdata, inode_pos, ip->ctime);
  ip->mtime = random();
  write_int(rawdata, inode_pos, ip->mtime);
  ip->atime = random();
  write_int(rawdata, inode_pos, ip->atime);

  fptr = fopen(file, "rb");
  if (!fptr) {
    perror(file);
    exit(-1);
  }

  // DBLOCKS
  int bytes, data_pos, dblockno, b;
  for (i = 0; i < N_DBLOCKS; i++) {
    dblockno = get_free_data_block(bitmap, N, M);
    ip->dblocks[i] = dblockno;
    write_int(rawdata, inode_pos, dblockno);

    data_pos = dblockno * BLOCK_SZ;
    bytes = fread(buf, sizeof(char), BLOCK_SZ, fptr);
    nbytes += bytes;
    for (b = 0; b < bytes; ++b) {
      write_char(rawdata, data_pos, buf[b]);
    }

    if (bytes < BLOCK_SZ) { // done writing file
      return;
    }
  }

  // IBLOCKS
  int iblockno;
  for (i = 0; i < N_IBLOCKS; ++i) {
    iblockno = get_free_inode_block(bitmap, N);
    ip->iblocks[i] = iblockno;
    write_int(rawdata, inode_pos, iblockno);
    if (write_iblock(rawdata, bitmap, N, M, fptr, buf, nbytes, iblockno)) {
      ip->size = nbytes;  // total number of data bytes written for file
      printf("successfully wrote %d bytes of file %s\n", nbytes, file);
      return;
    }
  }
  // fill in here if IBLOCKS needed
  // if so, you will first need to get an empty block to use for your IBLOCK

  // I2BLOCK
  int i2blockno = get_free_inode_block(bitmap, N);
  ip->i2block = i2blockno;
  write_int(rawdata, inode_pos, i2blockno);
  int i2block_pos = i2blockno * BLOCK_SZ;
  int i2block_end = i2block_pos + BLOCK_SZ;
  while (i2block_pos < i2block_end) {
    iblockno = get_free_inode_block(bitmap, N);
    write_int(rawdata, i2block_pos, iblockno);
    if (write_iblock(rawdata, bitmap, N, M, fptr, buf, nbytes, iblockno)) {
      ip->size = nbytes;  // total number of data bytes written for file
      printf("successfully wrote %d bytes of file %s\n", nbytes, file);
      return;
    }
  }

  //I3BLOCK
  int i3blockno = get_free_inode_block(bitmap, N);
  ip->i3block = i3blockno;
  write_int(rawdata, inode_pos, i3blockno);
  int i3block_pos = i3blockno * BLOCK_SZ;
  int i3block_end = i3block_pos + BLOCK_SZ;
  while (i3block_pos < i3block_end) {
    i2blockno = get_free_inode_block(bitmap, N);
    write_int(rawdata, i3block_pos, i2blockno);
    int i2block_pos = i2blockno * BLOCK_SZ;
    int i2block_end = i2block_pos + BLOCK_SZ;
    while (i2block_pos < i2block_end) {
      iblockno = get_free_inode_block(bitmap, N);
      write_int(rawdata, i2block_pos, iblockno);
      if (write_iblock(rawdata, bitmap, N, M, fptr, buf, nbytes, iblockno)) {
        ip->size = nbytes;  // total number of data bytes written for file
        printf("successfully wrote %d bytes of file %s\n", nbytes, file);
        return;
      }
    }
  }

}

int main(int argc, char **argv) // add argument handling
{
  if (strcmp(argv[1], "-create") == 0) {
    if (argc != 18) {
      perror("INVALID COMMAND\nusage: disk_image -create -image IMAGE_FILE -nblocks N -iblocks M -inputfile FILE -u UID -g GID -block D -inodepos I\n");
      exit(-1);
    }
    int N, M, D, I, UID, GID;
    const char *inputfilename, *outputfilename;
    for (int i = 2; i < 17; ++i) {
      if (strcmp(argv[i], "-image") == 0) {
        outputfilename = argv[i+1];
      } else if (strcmp(argv[i], "-nblocks") == 0) {
        N = atoi(argv[i+1]);
      } else if (strcmp(argv[i], "-iblocks") == 0) {
        M = atoi(argv[i+1]);
      } else if (strcmp(argv[i], "-inputfile") == 0) {
        inputfilename = argv[i+1];
      } else if (strcmp(argv[i], "-u") == 0) {
        UID = atoi(argv[i+1]);
      } else if (strcmp(argv[i], "-g") == 0) {
        GID = atoi(argv[i+1]);
      } else if (strcmp(argv[i], "-block") == 0) {
        D = atoi(argv[i+1]);
      } else if (strcmp(argv[i], "-inodepos") == 0) {
        I = atoi(argv[i+1]);
      } 
    }
    if (D >= M) {
      perror("ERROR:\nD must be less than M\n");
      exit(-1);
    }
    if (I + sizeof(struct inode) >= BLOCK_SZ) {
      perror("ERROR:\ninode doesn't fit in block\n");
      exit(-1);
    }
    char *rawdata = (char*)malloc(N * BLOCK_SZ * sizeof(char));
    char *bitmap = (char*)malloc(N * sizeof(char));
    for (int i = 0; i < N; ++i) {
      bitmap[i] = 0;
    }
    place_file(rawdata, bitmap, inputfilename, UID, GID, N, M, D, I);

    FILE *outfile = fopen(outputfilename, "wb");
    if (!outfile) {
      perror("outfile open");
      exit(-1);
    }
    int i;
    i = fwrite(rawdata, 1, N*BLOCK_SZ, outfile);
    if (i != N*BLOCK_SZ) {
      perror("fwrite");
      exit(-1);
    }
    i = fclose(outfile);
    if (i) {
      perror("outfile close");
      exit(-1);
    }
    printf("Done.\n");
    exit(0);
  }
  
}
