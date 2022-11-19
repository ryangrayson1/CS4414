#include <stdlib.h>
#include <stdio.h>
#include <strings.h>
#include <errno.h>
#include <assert.h>
#include <bits/stdc++.h>
#include "inode.h"
using namespace std;

char *rawdata;
char *bitmap;
int INODE_BLOCKS;
int DATA_BLOCKS;
int TOTAL_BLOCKS;
int INODE_SZ = sizeof(struct inode);
int next_data_block = 0;
int data_blocks_in_use = 0;

int get_free_data_block()
{
  if (data_blocks_in_use == DATA_BLOCKS) {
    perror("ERROR:\nfile is too large\n");
    exit(-1);
  }
  while (bitmap[next_data_block]) {
    if (next_data_block == DATA_BLOCKS - 1) {
      next_data_block = 0;
    } else {
      ++next_data_block;
    }
  }
  bitmap[next_data_block] = 1;
  ++data_blocks_in_use;
  return next_data_block;
}

int read_int(int pos) {
  int *ptr = (int *)&rawdata[pos];
  return *ptr;
}

int write_char(int pos, char val)
{
  char *ptr = (char*)&rawdata[pos];
  *ptr = val;
  return sizeof(char);
}

int write_int(int pos, int val)
{
  int *ptr = (int *)&rawdata[pos];
  *ptr = val;
  printf("val = %d -> written at position %d: %d\n", val, pos, rawdata[pos]);
  return sizeof(int);
}

// true if done writing file, false if we need to keep writing more of the file
bool write_dblock(FILE *fptr, char *buf, int &file_bytes_written, int dblockno) {
  int data_pos = dblockno * BLOCK_SZ;
  int cur_bytes = fread(buf, sizeof(char), BLOCK_SZ, fptr);
  file_bytes_written += cur_bytes;
  for (int b = 0; b < cur_bytes; ++b) {
    data_pos += write_char(data_pos, buf[b]);
  }
  return cur_bytes < BLOCK_SZ; // true if we are now at EOF
}


bool write_iblock(FILE *fptr, char *buf, int &file_bytes_written, int iblockno) {
  int iblock_pos = iblockno * BLOCK_SZ;
  int iblock_end = iblock_pos + BLOCK_SZ;
  int dblockno;
  while (iblock_pos < iblock_end) {
    dblockno = get_free_data_block();
    iblock_pos += write_int(iblock_pos, dblockno);
    if (write_dblock(fptr, buf, file_bytes_written, dblockno)) {
      return true;
    }
  }
  return false;
}

bool write_i2block(FILE *fptr, char *buf, int &file_bytes_written, int i2blockno) {
  int i2block_pos = i2blockno * BLOCK_SZ;
  int i2block_end = i2block_pos + BLOCK_SZ;
  int iblockno;
  while (i2block_pos < i2block_end) {
    iblockno = get_free_data_block();
    i2block_pos += write_int(i2block_pos, iblockno);
    if (write_iblock(fptr, buf, file_bytes_written, iblockno)) {
      return true;
    }
  }
  return false;
}

bool write_i3block(FILE *fptr, char *buf, int &file_bytes_written, int i3blockno) {
  int i3block_pos = i3blockno * BLOCK_SZ;
  int i3block_end = i3block_pos + BLOCK_SZ;
  int i2blockno;
  while (i3block_pos < i3block_end) {
    i2blockno = get_free_data_block();
    i3block_pos += write_int(i3block_pos, i2blockno);
    if (write_i2block(fptr, buf, file_bytes_written, i2blockno)) {
      return true;
    }
  }
  return false;
}

void place_file(char *rawdata, char *bitmap, const char *inputfilename, int uid, int gid, int inode_block, int inode_offset)
{
  int i, file_bytes_written = 0;
  struct inode *ip;
  FILE *fptr;
  char buf[BLOCK_SZ];

  int inode_byte_pos = inode_block * BLOCK_SZ + inode_offset * INODE_SZ;
  if (rawdata[inode_byte_pos]) { // checks if mode = 1 has been set
    perror("ERROR:\nspecified inode position is occupied");
    exit(-1);
  }

  ip = (struct inode *)malloc(INODE_SZ);
  
  ip->mode = 1;
  inode_byte_pos += write_int(inode_byte_pos, ip->mode);
  ip->nlink = 1;
  inode_byte_pos += write_int(inode_byte_pos, ip->nlink);
  ip->uid = uid;
  inode_byte_pos += write_int(inode_byte_pos, ip->uid);
  ip->gid = gid;
  inode_byte_pos += write_int(inode_byte_pos, ip->gid);
  ip->ctime = 1;
  inode_byte_pos += write_int(inode_byte_pos, ip->ctime);
  ip->mtime = 2;
  inode_byte_pos += write_int(inode_byte_pos, ip->mtime);
  ip->atime = 3;
  inode_byte_pos += write_int(inode_byte_pos, ip->atime);

  fptr = fopen(inputfilename, "rb");
  if (!fptr) {
    free(ip);
    perror(inputfilename);
    exit(-1);
  }

  // DBLOCKS
  int dblockno;
  for (i = 0; i < N_DBLOCKS; ++i) {
    dblockno = get_free_data_block();
    ip->dblocks[i] = dblockno;
    printf("dblock %d allocates blockno %d\n", i, dblockno);
    inode_byte_pos += write_int(inode_byte_pos, dblockno);
    if (write_dblock(fptr, buf, file_bytes_written, dblockno)) {
      ip->size = file_bytes_written;  // total number of data bytes written for file
      printf("successfully wrote %d bytes of file %s\n", file_bytes_written, inputfilename);
      free(ip);
      return;
    }
  }

  // IBLOCKS
  int iblockno;
  for (i = 0; i < N_IBLOCKS; ++i) {
    iblockno = get_free_data_block();
    ip->iblocks[i] = iblockno;
    inode_byte_pos += write_int(inode_byte_pos, iblockno);
    if (write_iblock(fptr, buf, file_bytes_written, iblockno)) {
      ip->size = file_bytes_written;  // total number of data bytes written for file
      printf("successfully wrote %d bytes of file %s\n", file_bytes_written, inputfilename);
      free(ip);
      return;
    }
  }

  // I2BLOCK
  int i2blockno = get_free_data_block();
  ip->i2block = i2blockno;
  inode_byte_pos += write_int(inode_byte_pos, i2blockno);
  if (write_i2block(fptr, buf, file_bytes_written, i2blockno)) {
    ip->size = file_bytes_written;  // total number of data bytes written for file
    printf("successfully wrote %d bytes of file %s\n", file_bytes_written, inputfilename);
    free(ip);
    return;
  }

  //I3BLOCK
  int i3blockno = get_free_data_block();
  ip->i3block = i3blockno;
  inode_byte_pos += write_int(inode_byte_pos, i3blockno);
  if (write_i3block(fptr, buf, file_bytes_written, i3blockno)) {
    ip->size = file_bytes_written;  // total number of data bytes written for file
    printf("successfully wrote %d bytes of file %s\n", file_bytes_written, inputfilename);
    free(ip);
    return;
  }

  free(ip);
  printf("failed to write all of file %s\n", inputfilename);
}

void traverse_inode(int inode_byte_pos) {
  struct inode *ip = (struct inode *)malloc(INODE_SZ);

  ip->mode = read_int(inode_byte_pos);
  inode_byte_pos += sizeof(int);
  ip->nlink = read_int(inode_byte_pos);
  inode_byte_pos += sizeof(int);
  ip->uid = read_int(inode_byte_pos);
  inode_byte_pos += sizeof(int);
  ip->gid = read_int(inode_byte_pos);
  inode_byte_pos += sizeof(int);
  ip->size = read_int(inode_byte_pos);
  inode_byte_pos += sizeof(int);
  ip->ctime = read_int(inode_byte_pos);
  inode_byte_pos += sizeof(int);
  ip->mtime = read_int(inode_byte_pos);
  inode_byte_pos += sizeof(int);
  ip->atime = read_int(inode_byte_pos);
  inode_byte_pos += sizeof(int);

  for (int i = 0; i < N_DBLOCKS; ++i) {
    ip->dblocks[i] = read_int(inode_byte_pos);
    inode_byte_pos += sizeof(int);
  }

  for (int i = 0; i < N_IBLOCKS; ++i) {
    ip->iblocks[i] = read_int(inode_byte_pos);
    inode_byte_pos += sizeof(int);
  }

  ip->i2block = read_int(inode_byte_pos);
  inode_byte_pos += sizeof(int);

  ip->i3block = read_int(inode_byte_pos);

  for (int i = 0; i < N_DBLOCKS; ++i) {
    bitmap[ip->dblocks[i]] = 1;
  }
  for (int i = 0; i < N_IBLOCKS; ++i) {
    bitmap[ip->iblocks[i]] = 1;
    // read all ints at block ip->iblocks[i] - set bitmap
    for (int pos = 0; pos < BLOCK_SZ; pos += sizeof(int)) {
      bitmap[read_int(ip->iblocks[i] * BLOCK_SZ + pos)] = 1;
    }
  }

  // same for i2 and i3 blocks

}

void construct_image_from_file(const char *image_filename) {
  FILE *fptr_image = fopen(image_filename, "rb");
  if (!fptr_image) {
    perror(image_filename);
    exit(-1);
  }
  fread(rawdata, TOTAL_BLOCKS * BLOCK_SZ, 1, fptr_image);
  fclose(fptr_image);

  int byte_pos;
  for (int block = 0; block < INODE_BLOCKS; ++block) {
    for (int pos = 0; pos < BLOCK_SZ / INODE_SZ; ++pos) {
      byte_pos = block * BLOCK_SZ + pos * INODE_SZ;
      if (rawdata[byte_pos]) {
        traverse_inode(byte_pos);
      }
    }
  }
}

int main(int argc, char **argv) // add argument handling
{
  if (strcmp(argv[1], "-create") == 0 || strcmp(argv[1], "-insert") == 0) {
    if (argc != 18) {
      perror("INVALID COMMAND\nusage: disk_image -create/-insert -image IMAGE_FILE -nblocks N -iblocks M -inputfile FILE -u UID -g GID -block D -inodepos I\n");
      exit(-1);
    }
    int N, M, D, I, UID, GID;
    const char *inputfilename, *image_filename;
    for (int i = 2; i < 17; ++i) {
      if (strcmp(argv[i], "-image") == 0) {
        image_filename = argv[i+1];
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
    if ((I+1) * INODE_SZ >= BLOCK_SZ) {
      perror("ERROR:\ninvalid inode position\n");
      exit(-1);
    }
    INODE_BLOCKS = M;
    DATA_BLOCKS = N - M;
    TOTAL_BLOCKS = N;
    next_data_block = M;
    
    rawdata = (char*)calloc(1, (TOTAL_BLOCKS+1) * BLOCK_SZ * sizeof(char));
    bitmap = (char*)calloc(1, (DATA_BLOCKS+1) * sizeof(char));
    if (strcmp(argv[1], "-insert") == 0) {
      construct_image_from_file(image_filename);
    }

    place_file(rawdata, bitmap, inputfilename, UID, GID, D, I);
    printf("RRR\n");
    for (int i = 0; i < INODE_SZ; i += 4) {
      // printf("%d\n", i);
      printf("%hhx, ", rawdata[i]);
    }
    // cout.flush();

    FILE *outfile = fopen(image_filename, "wb");
    if (!outfile) {
      perror("outfile open");
      exit(-1);
    }
    int i = fwrite(rawdata, 1, TOTAL_BLOCKS * BLOCK_SZ, outfile);
    if (i != TOTAL_BLOCKS * BLOCK_SZ) {
      perror("fwrite");
      exit(-1);
    }
    i = fclose(outfile);
    if (i) {
      perror("outfile close");
      exit(-1);
    }
    free(rawdata);
    free(bitmap);
    printf("Done.\n");
    exit(0);
  }
  
}
