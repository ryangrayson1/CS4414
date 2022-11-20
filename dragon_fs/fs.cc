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
int TOTAL_BLOCKS;
int INODE_BLOCKS;
int DATA_BLOCKS;
int INODE_SZ = sizeof(struct inode);
int INODES_PER_BLOCK = BLOCK_SZ / INODE_SZ;
int INT_SZ = sizeof(int);
int CHAR_SZ = sizeof(char);
int next_data_block = 0;
int data_blocks_in_use = 0;

// here data blocks are indexed starting from 0. We do not keep any bitmap data for inode blocks.
int get_free_data_block()
{
  if (data_blocks_in_use == DATA_BLOCKS) {
    printf("\nDATA BLOCKS: %d\n", DATA_BLOCKS);
    perror("\nERROR:\n all data blocks are in use\n");
    exit(-1);
  }
  while (bitmap[next_data_block]) {
    if (next_data_block < DATA_BLOCKS - 1) {
      ++next_data_block;
    } else {
      next_data_block = 0;
    }
  }
  bitmap[next_data_block] = 1;
  ++data_blocks_in_use;
  return INODE_BLOCKS + next_data_block;
}

// takes an absolute blockno and sets the bitmap
void set_bitmap(int absolute_blockno) {
  int datablockno = absolute_blockno - INODE_BLOCKS;
  if (datablockno < 0 || datablockno >= DATA_BLOCKS) {
    perror("\nERROR:\n attempt to set bitmap for invalid data block #:\n");
    printf("%d\n", absolute_blockno);
    exit(-1);
  }
  if (bitmap[datablockno]) {
    perror("\nERROR:\n attempt to set bitmap for occupied block\n");
  }
  bitmap[datablockno] = 1;
  ++data_blocks_in_use;
}

int read_int(int pos) {
  int *ptr = (int*)&rawdata[pos];
  return *ptr;
}

int write_char(int pos, char val)
{
  char *ptr = (char*)&rawdata[pos];
  *ptr = val;

  //P
  // printf("\nCHAR val = %c -> written at position %d: %c\n", val, pos, rawdata[pos]);
  //P

  return CHAR_SZ;
}

int write_int(int pos, int val)
{
  int *ptr = (int*)&rawdata[pos];
  *ptr = val;

  //P
  printf("\nINT val = %d -> written at position %d: %d\n", val, pos, rawdata[pos]);
  //P

  return INT_SZ;
}

// true if done writing file, false if we need to keep writing more of the file
bool write_dblock(FILE *fptr, char *buf, int &file_bytes_written, int dblockno) {
  int data_pos = dblockno * BLOCK_SZ;
  int cur_bytes = fread(buf, CHAR_SZ, BLOCK_SZ, fptr);
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

void place_file(const char *input_filename, int uid, int gid, int inode_block, int inode_offset)
{
  int inode_byte_pos = inode_block * BLOCK_SZ + inode_offset * INODE_SZ;

  if (rawdata[inode_byte_pos + INT_SZ]) { // checks if nlink = 1 has been set
    perror("\nERROR:\nspecified inode position is occupied");
    exit(-1);
  }

  struct inode *ip = (struct inode *)malloc(INODE_SZ);
  
  ip->mode = 1;
  inode_byte_pos += write_int(inode_byte_pos, ip->mode);
  ip->nlink = 1;
  inode_byte_pos += write_int(inode_byte_pos, ip->nlink);
  ip->uid = uid;
  inode_byte_pos += write_int(inode_byte_pos, ip->uid);
  ip->gid = gid;
  inode_byte_pos += write_int(inode_byte_pos, ip->gid);
  int size_byte_pos = inode_byte_pos; // keep track of where to write the size for later
  inode_byte_pos += INT_SZ;
  ip->ctime = 1;
  inode_byte_pos += write_int(inode_byte_pos, ip->ctime);
  ip->mtime = 2;
  inode_byte_pos += write_int(inode_byte_pos, ip->mtime);
  ip->atime = 3;
  inode_byte_pos += write_int(inode_byte_pos, ip->atime);

  FILE *fptr = fopen(input_filename, "rb");
  if (!fptr) {
    free(ip);
    perror(input_filename);
    exit(-1);
  }
  int i, file_bytes_written = 0;
  char buf[BLOCK_SZ];

  // DBLOCKS
  int dblockno;
  for (i = 0; i < N_DBLOCKS; ++i) {
    dblockno = get_free_data_block();
    ip->dblocks[i] = dblockno;

    //P
    // printf("dblock %d allocates blockno %d\n", i, dblockno);
    //P

    inode_byte_pos += write_int(inode_byte_pos, dblockno);
    if (write_dblock(fptr, buf, file_bytes_written, dblockno)) {
      ip->size = file_bytes_written;  // total number of data bytes written for file
      write_int(size_byte_pos, ip->size);
      printf("\nsuccessfully wrote %d bytes of file %s\n", file_bytes_written, input_filename);
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
      write_int(size_byte_pos, ip->size);
      printf("\nsuccessfully wrote %d bytes of file %s\n", file_bytes_written, input_filename);
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
    write_int(size_byte_pos, ip->size);
    printf("\nsuccessfully wrote %d bytes of file %s\n", file_bytes_written, input_filename);
    free(ip);
    return;
  }

  //I3BLOCK
  int i3blockno = get_free_data_block();
  ip->i3block = i3blockno;
  inode_byte_pos += write_int(inode_byte_pos, i3blockno);
  if (write_i3block(fptr, buf, file_bytes_written, i3blockno)) {
    ip->size = file_bytes_written;  // total number of data bytes written for file
    write_int(size_byte_pos, ip->size);
    printf("\nsuccessfully wrote %d bytes of file %s\n", file_bytes_written, input_filename);
    free(ip);
    return;
  }

  free(ip);
  printf("\nfailed to write all of file %s\n", input_filename);
}


void read_iblock(int iblockno) {
  set_bitmap(iblockno);
  int iblock_byte_pos = iblockno * BLOCK_SZ;
  int dblockno;
  for (int ioffset = 0; ioffset < BLOCK_SZ; ioffset += INT_SZ) {
    dblockno = read_int(iblock_byte_pos + ioffset);
    if (dblockno > 0) {

      //P
      // printf("iblockno: %d, ipos: %d\n", iblockno, ipos);
      // printf("\n\ndblockno:\n%d\n\n\n", dblockno);
      //P
      
      set_bitmap(dblockno);
    }
  }
}

void read_i2block(int i2blockno) {
  set_bitmap(i2blockno);
  int i2block_byte_pos = i2blockno * BLOCK_SZ;
  int iblockno;
  for (int i2offset = 0; i2offset < BLOCK_SZ; i2offset += INT_SZ) {
    iblockno = read_int(i2block_byte_pos + i2offset);
    if (iblockno > 0) {
      read_iblock(iblockno);
    }
  }
}

void read_i3block(int i3blockno) {

  //P
  printf("\n\ni3blockno:\n%d\n\n\n", i3blockno);
  //P

  set_bitmap(i3blockno);
  int i3block_byte_pos = i3blockno * BLOCK_SZ;
  int i2blockno;
  for (int i3offset = 0; i3offset < BLOCK_SZ; i3offset += INT_SZ) {
    i2blockno = read_int(i3block_byte_pos + i3offset);
    if (i2blockno > 0) {
      read_i2block(i2blockno);
    }
  }
}

void traverse_inode(int inode_byte_pos) {

  if (read_int(inode_byte_pos + INT_SZ) == 0) { 
    return; // if nlink is 0, there is not a present inode
  }

  struct inode *ip = (struct inode *)malloc(INODE_SZ);

  ip->mode = read_int(inode_byte_pos);
  inode_byte_pos += INT_SZ;
  ip->nlink = read_int(inode_byte_pos);
  inode_byte_pos += INT_SZ;
  ip->uid = read_int(inode_byte_pos);
  inode_byte_pos += INT_SZ;
  ip->gid = read_int(inode_byte_pos);
  inode_byte_pos += INT_SZ;
  ip->size = read_int(inode_byte_pos);
  inode_byte_pos += INT_SZ;
  ip->ctime = read_int(inode_byte_pos);
  inode_byte_pos += INT_SZ;
  ip->mtime = read_int(inode_byte_pos);
  inode_byte_pos += INT_SZ;
  ip->atime = read_int(inode_byte_pos);
  inode_byte_pos += INT_SZ;

  for (int i = 0; i < N_DBLOCKS; ++i) {
    ip->dblocks[i] = read_int(inode_byte_pos);
    inode_byte_pos += INT_SZ;
  }

  for (int i = 0; i < N_IBLOCKS; ++i) {
    ip->iblocks[i] = read_int(inode_byte_pos);
    inode_byte_pos += INT_SZ;
  }

  ip->i2block = read_int(inode_byte_pos);
  inode_byte_pos += INT_SZ;

  ip->i3block = read_int(inode_byte_pos);

  // set dblocks
  for (int i = 0; i < N_DBLOCKS; ++i) {
    if (ip->dblocks[i] > 0) {
      set_bitmap(ip->dblocks[i]);
    }
  }

  // set iblocks
  for (int i = 0; i < N_IBLOCKS; ++i) {
    if (ip->iblocks[i] > 0) {
      read_iblock(ip->iblocks[i]);
    }
  }

  // set i2block
  if (ip->i2block > 0) {
    read_i2block(ip->i2block);
  }

  // set i3block
  if (ip->i3block > 0) {
    read_i3block(ip->i3block);
  }

  free(ip);

}

void construct_image_from_file(const char *image_filename) {
  FILE *image_fptr = fopen(image_filename, "rb");
  if (!image_fptr) {
    perror("\nERROR:\n while opening existing image\n");
    exit(-1);
  }

  int r = fread(rawdata, CHAR_SZ, TOTAL_BLOCKS * BLOCK_SZ, image_fptr);
  fclose(image_fptr);
  if (r < TOTAL_BLOCKS * BLOCK_SZ) {
    printf("\nWARNING:\n input image size (%d) less than size specified by parameters (%d)\n", r, TOTAL_BLOCKS * BLOCK_SZ);
  };
  
  //P
  printf("\nEXISTING IMAGE BEFORE INSERTION:\n");
  for (int i = 0; i < INODE_SZ*2; i += INT_SZ) {
    // printf("%d\n", i);
    if (i % INODE_SZ == 0) {
      printf("\n");
    }
    printf("%d, ", read_int(i));
  }
  printf("\n--------------\n");
  // exit(0);
  //P

  int block_byte_pos, inode_byte_pos;
  for (int block = 0; block < INODE_BLOCKS; ++block) {
    block_byte_pos = block * BLOCK_SZ;
    for (int inode_idx = 0; inode_idx < INODES_PER_BLOCK; ++inode_idx) {
      inode_byte_pos = block_byte_pos + inode_idx * INODE_SZ;
      traverse_inode(inode_byte_pos);
    }
  }

  //P
  printf("\nBITMAP TAKEN SPACES:\n");
  for (int i = INODE_BLOCKS; i < INODE_BLOCKS + 50; ++i) {
    if (bitmap[i]) {
      printf("%d\n", i);
    }
  }
  // exit(0);
  //P

}

int main(int argc, char **argv) // add argument handling
{
  if (strcmp(argv[1], "-create") == 0 || strcmp(argv[1], "-insert") == 0) {
    if (argc != 18) {
      perror("INVALID COMMAND\n usage: disk_image -create/-insert -image IMAGE_FILE -nblocks N -iblocks M -inputfile FILE -u UID -g GID -block D -inodepos I\n");
      exit(-1);
    }
    int N, M, D, I, UID, GID;
    const char *input_filename, *image_filename;
    for (int i = 2; i < 17; ++i) {
      if (strcmp(argv[i], "-image") == 0) {
        image_filename = argv[i+1];
      } else if (strcmp(argv[i], "-nblocks") == 0) {
        N = atoi(argv[i+1]);
      } else if (strcmp(argv[i], "-iblocks") == 0) {
        M = atoi(argv[i+1]);
      } else if (strcmp(argv[i], "-inputfile") == 0) {
        input_filename = argv[i+1];
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
      perror("\nERROR:\n D must be less than M\n");
      exit(-1);
    }
    if (I >= INODES_PER_BLOCK) {
      perror("\nERROR:\n invalid inode position\n");
      exit(-1);
    }
    TOTAL_BLOCKS = N;
    INODE_BLOCKS = M;
    DATA_BLOCKS = N - M;
    
    rawdata = (char*)calloc((TOTAL_BLOCKS+1) * BLOCK_SZ, CHAR_SZ);
    bitmap = (char*)calloc(DATA_BLOCKS+1, CHAR_SZ);
    if (!rawdata || !bitmap) {
      perror("\nERROR:\n memory allocation failed\n");
    }
    if (strcmp(argv[1], "-insert") == 0) { // need to read the image file in if we are inserting
      construct_image_from_file(image_filename);
    }

    place_file(input_filename, UID, GID, D, I);

    //P
    printf("FIRST INODE BLOCK AFTER FILE PLACED:\n");
    for (int i = 0; i < INODE_SZ*INODES_PER_BLOCK; i += INT_SZ) {
      // printf("%d\n", i);
      if (i % INODE_SZ == 0) {
        printf("\n");
      }
      printf("%d, ", read_int(i));

    }
    //P

    FILE *outfile = fopen(image_filename, "wb");
    if (!outfile) {
      perror("\nERROR:\n outfile open\n");
      exit(-1);
    }
    int i = fwrite(rawdata, 1, TOTAL_BLOCKS * BLOCK_SZ, outfile);
    if (i != TOTAL_BLOCKS * BLOCK_SZ) {
      perror("\nERROR:\n fwrite\n");
      exit(-1);
    }
    i = fclose(outfile);
    if (i) {
      perror("\nERROR:\n outfile close\n");
      exit(-1);
    }
    free(rawdata);
    free(bitmap);
    printf("Done.\n");
    exit(0);
  }

  if (strcmp(argv[1], "-extract") == 0) {
    if (argc != 10) {
      perror("INVALID COMMAND\n usage: disk_image -extract -image IMAGE_FILE -u UID -g GID -o PATH\n");
      exit(-1);
    }
    int UID, GID;
    const char *input_filename, *image_filename, *output_path;
    for (int i = 2; i < 17; ++i) {
      if (strcmp(argv[i], "-image") == 0) {
        image_filename = argv[i+1];
      } else if (strcmp(argv[i], "-u") == 0) {
        UID = atoi(argv[i+1]);
      } else if (strcmp(argv[i], "-g") == 0) {
        GID = atoi(argv[i+1]);
      } else if (strcmp(argv[i], "-o") == 0) {
        output_path = argv[i+1];
      }
    }



    exit(0);
  }
  
}
