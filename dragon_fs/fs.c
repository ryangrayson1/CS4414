#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include "inode.h"

#define uint unsigned int

char *rawdata;
char *bitmap;
uint TOTAL_BLOCKS;
uint INODE_BLOCKS;
uint DATA_BLOCKS;
const uint INODE_SZ = sizeof(struct inode);
const uint INODES_PER_BLOCK = BLOCK_SZ / INODE_SZ;
const uint UINT_SZ = sizeof(int);
const uint CHAR_SZ = sizeof(char);
uint next_data_block;
uint data_blocks_in_use = 0;
uint file_bytes_written = 0;
FILE *extracted_fptr;

// here data blocks are indexed starting from 0. We do not keep any bitmap data for inode blocks.
uint get_free_data_block()
{
  if (data_blocks_in_use == DATA_BLOCKS) {
    printf("\nDATA BLOCKS: %u\n", DATA_BLOCKS);
    perror("\nERROR:\n all data blocks are in use\n");
    exit(-1);
  }
  while (bitmap[next_data_block]) {
    if (next_data_block < TOTAL_BLOCKS - 1) {
      ++next_data_block;
    } else {
      next_data_block = INODE_BLOCKS;
    }
  }
  bitmap[next_data_block] = 1;
  ++data_blocks_in_use;
  return next_data_block;
}

// takes an absolute blockno and sets the bitmap
uint INODE_BLOCK = 1, DATA_BLOCK = 2;
void set_bitmap(uint blockno, uint block_type) {
  if (blockno >= TOTAL_BLOCKS) {
    perror("\nERROR:\n attempt to set bitmap for invalid data block #:\n");
    printf("\nblockno: %u\n", blockno);
    printf("\ntotal blocks: %u\n", TOTAL_BLOCKS);
    exit(-1);
  }
  if (bitmap[blockno]) {
    perror("\nERROR:\n attempt to set bitmap for occupied block\n");
    exit(-1);
  }
  bitmap[blockno] = block_type;
  if (block_type == DATA_BLOCK) {
    ++data_blocks_in_use;
  }
}

uint read_uint(uint pos) {
  uint *ptr = (uint*)&rawdata[pos];
  return *ptr;
}

char read_char(uint pos) {
  char *ptr = (char*)&rawdata[pos];
  return *ptr;
}

uint write_char(uint pos, char val)
{
  char *ptr = (char*)&rawdata[pos];
  *ptr = val;

  //P
  // printf("\nCHAR val = %c -> written at position %u: %c\n", val, pos, rawdata[pos]);
  //P

  return CHAR_SZ;
}

uint write_uint(uint pos, uint val)
{
  uint *ptr = (uint*)&rawdata[pos];
  *ptr = val;

  //P
  // printf("\nuint val = %u -> written at position %u: %u\n", val, pos, rawdata[pos]);
  //P

  return UINT_SZ;
}

void write_extracted_data(uint blockno, struct inode *ip) {
  // if (blockno >= TOTAL_BLOCKS) {
  //   printf("\nBLOCK ERR\n");
  //   return;
  // }
  uint byte_pos = blockno * BLOCK_SZ;
  // uint block_end = byte_pos + BLOCK_SZ;
  // for (; byte_pos < block_end; ++byte_pos) {
  //   fprintf(extracted_fptr, "%c", read_char(byte_pos));
  // }
  uint to_write = ip->size < BLOCK_SZ ? ip->size : BLOCK_SZ;
  fwrite(&rawdata[byte_pos], to_write, 1, extracted_fptr);
}

// true (1) if done writing file, false (0) if we need to keep writing more of the file
uint write_dblock(FILE *fptr, char *buf, uint dblockno) {
  uint data_pos = dblockno * BLOCK_SZ;
  uint cur_bytes = fread(buf, CHAR_SZ, BLOCK_SZ, fptr);
  file_bytes_written += cur_bytes;
  uint b;
  for (b = 0; b < cur_bytes; ++b) {
    data_pos += write_char(data_pos, buf[b]);
  }
  return cur_bytes < BLOCK_SZ ? 1 : 0; // true if we are now at EOF
}

uint write_iblock(FILE *fptr, char *buf, uint iblockno) {
  uint iblock_pos = iblockno * BLOCK_SZ;
  uint iblock_end = iblock_pos + BLOCK_SZ;
  uint dblockno;
  while (iblock_pos < iblock_end) {
    dblockno = get_free_data_block();
    iblock_pos += write_uint(iblock_pos, dblockno);
    if (write_dblock(fptr, buf, dblockno)) {
      return 1;
    }
  }
  return 0;
}

uint write_i2block(FILE *fptr, char *buf, uint i2blockno) {
  uint i2block_pos = i2blockno * BLOCK_SZ;
  uint i2block_end = i2block_pos + BLOCK_SZ;
  uint iblockno;
  while (i2block_pos < i2block_end) {
    iblockno = get_free_data_block();
    i2block_pos += write_uint(i2block_pos, iblockno);
    if (write_iblock(fptr, buf, iblockno)) {
      return 1;
    }
  }
  return 0;
}

uint write_i3block(FILE *fptr, char *buf, uint i3blockno) {
  uint i3block_pos = i3blockno * BLOCK_SZ;
  uint i3block_end = i3block_pos + BLOCK_SZ;
  uint i2blockno;
  while (i3block_pos < i3block_end) {
    i2blockno = get_free_data_block();
    i3block_pos += write_uint(i3block_pos, i2blockno);
    if (write_i2block(fptr, buf, i2blockno)) {
      return 1;
    }
  }
  return 0;
}

void place_file(const char *input_filename, uint uid, uint gid, uint inode_block, uint inode_offset)
{
  uint inode_byte_pos = inode_block * BLOCK_SZ + inode_offset * INODE_SZ;

  if (rawdata[inode_byte_pos + UINT_SZ]) { // checks if nlink = 1 has been set
    perror("\nERROR:\nspecified inode position is occupied");
    exit(-1);
  }

  struct inode *ip = (struct inode *)malloc(INODE_SZ);
  
  ip->mode = 1;
  inode_byte_pos += write_uint(inode_byte_pos, ip->mode);
  ip->nlink = 1;
  inode_byte_pos += write_uint(inode_byte_pos, ip->nlink);
  ip->uid = uid;
  inode_byte_pos += write_uint(inode_byte_pos, ip->uid);
  ip->gid = gid;
  inode_byte_pos += write_uint(inode_byte_pos, ip->gid);
  uint size_byte_pos = inode_byte_pos; // keep track of where to write the size for later
  inode_byte_pos += UINT_SZ;
  ip->ctime = 1;
  inode_byte_pos += write_uint(inode_byte_pos, ip->ctime);
  ip->mtime = 2;
  inode_byte_pos += write_uint(inode_byte_pos, ip->mtime);
  ip->atime = 3;
  inode_byte_pos += write_uint(inode_byte_pos, ip->atime);

  FILE *fptr = fopen(input_filename, "rb");
  if (!fptr) {
    free(ip);
    perror("\nERROR:\n while opening existing image\n");
    exit(-1);
  }
  char buf[BLOCK_SZ];

  // DBLOCKS
  uint i, dblockno;
  for (i = 0; i < N_DBLOCKS; ++i) {
    dblockno = get_free_data_block();
    ip->dblocks[i] = dblockno;

    //P
    // printf("dblock %u allocates blockno %u\n", i, dblockno);
    //P

    inode_byte_pos += write_uint(inode_byte_pos, dblockno);
    if (write_dblock(fptr, buf, dblockno)) {
      ip->size = file_bytes_written;  // total number of data bytes written for file
      write_uint(size_byte_pos, ip->size);
      printf("\nsuccessfully wrote %u bytes of file %s\n", file_bytes_written, input_filename);
      free(ip);
      return;
    }
  }

  // IBLOCKS
  uint iblockno;
  for (i = 0; i < N_IBLOCKS; ++i) {
    iblockno = get_free_data_block();
    ip->iblocks[i] = iblockno;
    inode_byte_pos += write_uint(inode_byte_pos, iblockno);
    if (write_iblock(fptr, buf, iblockno)) {
      ip->size = file_bytes_written;  // total number of data bytes written for file
      write_uint(size_byte_pos, ip->size);
      printf("\nsuccessfully wrote %u bytes of file %s\n", file_bytes_written, input_filename);
      free(ip);
      return;
    }
  }

  // I2BLOCK
  uint i2blockno = get_free_data_block();
  ip->i2block = i2blockno;
  inode_byte_pos += write_uint(inode_byte_pos, i2blockno);
  if (write_i2block(fptr, buf, i2blockno)) {
    ip->size = file_bytes_written;  // total number of data bytes written for file
    write_uint(size_byte_pos, ip->size);
    printf("\nsuccessfully wrote %u bytes of file %s\n", file_bytes_written, input_filename);
    free(ip);
    return;
  }

  //I3BLOCK
  uint i3blockno = get_free_data_block();
  ip->i3block = i3blockno;
  inode_byte_pos += write_uint(inode_byte_pos, i3blockno);
  if (write_i3block(fptr, buf, i3blockno)) {
    ip->size = file_bytes_written;  // total number of data bytes written for file
    write_uint(size_byte_pos, ip->size);
    printf("\nsuccessfully wrote %u bytes of file %s\n", file_bytes_written, input_filename);
    free(ip);
    return;
  }

  free(ip);
  printf("\nfailed to write all of file %s\n", input_filename);
}


void read_iblock(uint iblockno, struct inode *ip) {
  set_bitmap(iblockno, INODE_BLOCK);
  uint iblock_byte_pos = iblockno * BLOCK_SZ;
  uint dblockno, ioffset;
  for (ioffset = 0; ioffset < BLOCK_SZ && ip->size > 0; ioffset += UINT_SZ) {
    dblockno = read_uint(iblock_byte_pos + ioffset);
    //P
    // printf("iblockno: %u, ipos: %u\n", iblockno, ioffset);
    // printf("\n\ndblockno:\n%u\n\n\n", dblockno);
    //P
    
    set_bitmap(dblockno, DATA_BLOCK);
    if (extracted_fptr) {
      write_extracted_data(dblockno, ip);
    }
    if (ip->size >= BLOCK_SZ) {
      ip->size -= BLOCK_SZ;
    } else {
      ip->size = 0;
    }
  }
}

void read_i2block(uint i2blockno, struct inode *ip) {

  //P
  // printf("\n\ni2blockno:\n%u\n\n\n", i2blockno);
  //P

  set_bitmap(i2blockno, INODE_BLOCK);
  uint i2block_byte_pos = i2blockno * BLOCK_SZ;
  uint iblockno, i2offset;
  for (i2offset = 0; i2offset < BLOCK_SZ && ip->size > 0; i2offset += UINT_SZ) {
    iblockno = read_uint(i2block_byte_pos + i2offset);
    read_iblock(iblockno, ip);
  }
}

void read_i3block(uint i3blockno, struct inode *ip) {

  //P
  // printf("\n\ni3blockno:\n%u\n\n\n", i3blockno);
  //P

  set_bitmap(i3blockno, INODE_BLOCK);
  uint i3block_byte_pos = i3blockno * BLOCK_SZ;
  uint i2blockno, i3offset;
  for (i3offset = 0; i3offset < BLOCK_SZ && ip->size > 0; i3offset += UINT_SZ) {
    i2blockno = read_uint(i3block_byte_pos + i3offset);
    read_i2block(i2blockno, ip);
  }
}

struct inode *get_inode(uint inode_byte_pos) {

  if (read_uint(inode_byte_pos + UINT_SZ) == 0) {
    return 0; // if nlink is 0, there is not a present inode
  }

  struct inode *ip = (struct inode *)malloc(INODE_SZ);

  ip->mode = read_uint(inode_byte_pos);
  inode_byte_pos += UINT_SZ;
  ip->nlink = read_uint(inode_byte_pos);
  inode_byte_pos += UINT_SZ;
  ip->uid = read_uint(inode_byte_pos);
  inode_byte_pos += UINT_SZ;
  ip->gid = read_uint(inode_byte_pos);
  inode_byte_pos += UINT_SZ;
  ip->size = read_uint(inode_byte_pos);
  inode_byte_pos += UINT_SZ;
  ip->ctime = read_uint(inode_byte_pos);
  inode_byte_pos += UINT_SZ;
  ip->mtime = read_uint(inode_byte_pos);
  inode_byte_pos += UINT_SZ;
  ip->atime = read_uint(inode_byte_pos);
  inode_byte_pos += UINT_SZ;

  uint i;
  for (i = 0; i < N_DBLOCKS; ++i) {
    ip->dblocks[i] = read_uint(inode_byte_pos);
    inode_byte_pos += UINT_SZ;
  }

  for (i = 0; i < N_IBLOCKS; ++i) {
    ip->iblocks[i] = read_uint(inode_byte_pos);
    inode_byte_pos += UINT_SZ;
  }

  ip->i2block = read_uint(inode_byte_pos);
  inode_byte_pos += UINT_SZ;

  ip->i3block = read_uint(inode_byte_pos);

  return ip;
}

void traverse_inode(uint inode_byte_pos, struct inode *ip) {
  if (!ip) {
    return;
  }
  uint blockno = inode_byte_pos / BLOCK_SZ;
  if (!bitmap[blockno]) {
    set_bitmap(blockno, INODE_BLOCK);
  }

  uint i; 
  // set dblocks
  for (i = 0; i < N_DBLOCKS && ip->size > 0; ++i) {
    if (ip->size > 0) {
      set_bitmap(ip->dblocks[i], DATA_BLOCK);
      if (extracted_fptr) {
        write_extracted_data(ip->dblocks[i], ip);
      }
      if (ip->size >= BLOCK_SZ) {
        ip->size -= BLOCK_SZ;
      } else {
        ip->size = 0;
      }
    }
  }

  // set iblocks 
  for (i = 0; i < N_IBLOCKS && ip->size > 0; ++i) {
    read_iblock(ip->iblocks[i], ip);
  }

  // set i2block
  if (ip->size > 0) {
    read_i2block(ip->i2block, ip);
  }

  // set i3block
  if (ip->size > 0) {
    read_i3block(ip->i3block, ip);
  }
  
}

void construct_image_from_file(const char *image_filename, uint known_inode_blocks) {
  FILE *image_fptr = fopen(image_filename, "rb");
  if (!image_fptr) {
    perror("\nERROR:\n while opening existing image\n");
    exit(-1);
  }

  uint r = fread(rawdata, CHAR_SZ, TOTAL_BLOCKS * BLOCK_SZ, image_fptr);
  fclose(image_fptr);
  if (r < TOTAL_BLOCKS * BLOCK_SZ) {
  //   printf("\nWARNING:\n input image size (%u) less than size specified by parameters (%u)\n", r, TOTAL_BLOCKS * BLOCK_SZ);
  };

  if (!known_inode_blocks) {
    return;
  }
  
  //P
  printf("\nEXISTING IMAGE BEFORE INSERTION:\n");
  uint i;
  for (i = 0; i < INODE_SZ*2; i += UINT_SZ) {
    // printf("%u\n", i);
    if (i % INODE_SZ == 0) {
      printf("\n");
    }
    printf("%u, ", read_uint(i));
  }
  printf("\n--------------\n");
  // exit(0);
  //P

  uint block_byte_pos, inode_byte_pos, block, inode_idx;
  for (block = 0; block < INODE_BLOCKS; ++block) {
    block_byte_pos = block * BLOCK_SZ;
    for (inode_idx = 0; inode_idx < INODES_PER_BLOCK; ++inode_idx) {
      inode_byte_pos = block_byte_pos + inode_idx * INODE_SZ;
      struct inode *ip = get_inode(inode_byte_pos);
      if (ip) {
        traverse_inode(inode_byte_pos, ip);
        free(ip);
      }
    }
  }

  //P
  // printf("\nBITMAP TAKEN SPACES:\n");
  // for (i = INODE_BLOCKS; i < INODE_BLOCKS + 50; ++i) {
  //   if (bitmap[i]) {
  //     printf("%u\n", i);
  //   }
  // }
  // exit(0);
  //P

}

void extract_files(uint UID, uint GID, const char *output_path) {
  char output_files_found[100];
  strcpy(output_files_found, output_path);
  strcat(output_files_found, "/FILES_FOUND");
  FILE *outfile = fopen(output_files_found, "w");
  if (!outfile) {
    perror("\nERROR:\n outfile open\n");
    exit(-1);
  }

  char extracted_filename[100];
  strcpy(extracted_filename, output_path);
  strcat(extracted_filename, "/extracted");

  uint block_byte_pos, inode_byte_pos, block, inode_idx;
  for (block = 0; block < TOTAL_BLOCKS; ++block) {
    block_byte_pos = block * BLOCK_SZ;
    for (inode_idx = 0; inode_idx < INODES_PER_BLOCK; ++inode_idx) {
      inode_byte_pos = block_byte_pos + inode_idx * INODE_SZ;
      struct inode *ip = get_inode(inode_byte_pos);
      if (!ip) {
        continue;
      }
      if (ip && ip->nlink > 0 && ip->size > 0 && ip->uid == UID && ip->gid == GID) {
        fprintf(outfile, "file found at inode in block %u, file size %u\n", block, ip->size);
        // if (ip->size != 7306130) {
        //   continue;
        // }
        extracted_fptr = fopen(extracted_filename, "wb"); // create a new file for each found file to reconstruct
        if (!extracted_fptr) {
          free(ip);
          perror("\nERROR:\n while opening extraction file destination\n");
          exit(-1);
        }
        traverse_inode(inode_byte_pos, ip); // to set bitmap
        fclose(extracted_fptr);
        strcat(extracted_filename, "_X");
      }
      free(ip);
    }
  }
  fclose(outfile);

  char output_files_free[80];
  strcpy(output_files_free, output_path);
  strcat(output_files_free, "/UNUSED_BLOCKS");
  FILE *outfile2 = fopen(output_files_free, "w");
  if (!outfile2) {
    perror("\nERROR:\n outfile2 open\n");
    exit(-1);
  }

  uint b;
  for (b = 0; b < TOTAL_BLOCKS; ++b) {
    if (!bitmap[b]) {
      fprintf(outfile2, "%u\n", b);
    }
  }

  fclose(outfile2);

}

int main(int argc, char **argv) // add argument handling
{
  if (strcmp(argv[1], "-create") == 0 || strcmp(argv[1], "-insert") == 0) {
    if (argc != 18) {
      perror("INVALID COMMAND\n usage: disk_image -create/-insert -image IMAGE_FILE -nblocks N -iblocks M -inputfile FILE -u UID -g GID -block D -inodepos I\n");
      exit(-1);
    }
    uint N, M, D, I, UID, GID;
    const char *input_filename, *image_filename;
    uint i;
    for (i = 2; i < 17; ++i) {
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
    next_data_block = M;
    
    rawdata = (char*)calloc((TOTAL_BLOCKS+1) * BLOCK_SZ, CHAR_SZ);
    bitmap = (char*)calloc(TOTAL_BLOCKS+1, CHAR_SZ);
    if (!rawdata || !bitmap) {
      perror("\nERROR:\n memory allocation failed\n");
    }
    if (strcmp(argv[1], "-insert") == 0) { // need to read the image file in if we are inserting
      construct_image_from_file(image_filename, 1);
    }

    place_file(input_filename, UID, GID, D, I);

    //P
    printf("FIRST INODE BLOCK AFTER FILE PLACED:\n");
    for (i = 0; i < INODE_SZ*INODES_PER_BLOCK; i += UINT_SZ) {
      // printf("%u\n", i);
      if (i % INODE_SZ == 0) {
        printf("\n");
      }
      printf("%u, ", read_uint(i));

    }
    //P

    FILE *outfile = fopen(image_filename, "wb");
    if (!outfile) {
      perror("\nERROR:\n outfile open\n");
      exit(-1);
    }
    i = fwrite(rawdata, 1, TOTAL_BLOCKS * BLOCK_SZ, outfile);
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
    uint UID = 0, GID = 0;
    const char *image_filename, *output_path;
    uint i;
    for (i = 2; i < 10; ++i) {
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
    if (UID == 0 || GID == 0) {
      perror("\nERROR:\n UID, GID not specified\n");
      exit(-1);
    }
    FILE *image_fptr = fopen(image_filename, "rb");
    if (!image_fptr) {
      perror("\nERROR:\n while opening existing image\n");
      exit(-1);
    }
    fseek(image_fptr, 0, SEEK_END);
    int file_size = ftell(image_fptr);
    rewind(image_fptr);
    fclose(image_fptr);

    TOTAL_BLOCKS = file_size / BLOCK_SZ + 1;

    rawdata = (char*)calloc((TOTAL_BLOCKS+1) * BLOCK_SZ, CHAR_SZ);
    bitmap = (char*)calloc(TOTAL_BLOCKS+1, CHAR_SZ);

    construct_image_from_file(image_filename, 0);

    extract_files(UID, GID, output_path);

    exit(0);
  }
  
  exit(0);
}
