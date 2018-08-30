/*
 * Copyright (c) 2017 Trail of Bits, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <fcntl.h>

#include <deepstate/DeepState.hpp>

using namespace deepstate;

extern "C" {
#include "super.h"
#include "posixtfs.h"
#include "dir.h"
#include "inode.h"
#include "block.h"
}

#define LENGTH 20
#define PATH_LEN 10
#define DATA_LEN 2

static char DataChar() {
  symbolic_char c;
  ASSUME ((c == 'x') || (c == 'y'));
  return c;
}

static void MakeNewData(char *data) {
  symbolic_unsigned l;
  ASSUME_GT(l, 0);
  ASSUME_LT(l, DATA_LEN+1);
  unsigned i;
  for (i = 0; i < l; i++) {
    data[i] = DataChar();
  }
  data[i] = 0;
}

static void MakeNewPath(char *path) {
  symbolic_unsigned l;
  ASSUME_GT(l, 0);
  ASSUME_LT(l, PATH_LEN+1);
  int i, max_i = Pump(l);
  for (i = 0; i < max_i; i++) {
    path[i] = OneOf("aAbB/.");
  }
  path[i] = 0;
}

TEST(TestFs, FilesDirs) {
  char storage[MAX_STORAGE];

  memset(storage, 0, MAX_STORAGE);
  
  struct super_block *sb = testfs_make_super_block(storage);
  ASSERT(sb != nullptr)
      << "Couldn't initialize super block";

  LOG(INFO) << "Making inode free map";
  testfs_make_inode_freemap(sb);

  LOG(INFO) << "Making block free map";
  testfs_make_block_freemap(sb);

  LOG(INFO) << "Making checksum table";
  testfs_make_csum_table(sb);

  LOG(INFO) << "Making inode blocks";
  testfs_make_inode_blocks(sb);

  testfs_close_super_block(sb);  

  ASSERT(!testfs_init_super_block(storage, 0, &sb))
    << "Couldn't initialize super block";

  ASSERT(!testfs_make_root_dir(sb))
      << "Couldn't create root directory.";

  testfs_close_super_block(sb);
  LOG(INFO)
      << "Created root directory; File system initialized";

  ASSERT(!testfs_init_super_block(storage, 0, &sb))
    << "Couldn't initialize super block";

  struct inode *root = testfs_get_inode(sb, 0);
  ASSERT(root != NULL) << "Root is null!";

  LOG(INFO) << "Checking the initial file system...";  
  tfs_checkfs(sb);

  char path[PATH_LEN+1];
  char data[DATA_LEN+1] = {};
  int r;

  for (int n = 0; n < LENGTH; n++) {
    OneOf(
      [&r, n, sb, &path] {
        MakeNewPath(path);
        printf("STEP %d: tfs_mkdir(sb, \"%s\");",
               n, path);
        r = tfs_mkdir(sb, path);
	printf("STEP %d: tfs_mkdir(sb, \"%s\") = %d",
	       n, path, r);
      },
      [&r, n, sb, &path] {
        MakeNewPath(path);
        printf("STEP %d: tfs_rmdir(sb, \"%s\");",
               n, path);
        r = tfs_rmdir(sb, path);
	printf("STEP %d: tfs_rmdir(sb, \"%s\") = %d",
	       n, path, r);	
      },
      [&r, n, sb] {
        printf("STEP %d: tfs_ls(sb);", n);
        r = tfs_ls(sb);
	printf("STEP %d: tfs_ls(sb) = %d", n, r);	
      },
      [&r, n, sb] {
        printf("STEP %d: tfs_lsr(sb);", n);
        r = tfs_lsr(sb);
	printf("STEP %d: tfs_lsr(sb) = %d", n, r);	
      },
      [&r, n, sb, &path] {
	MakeNewPath(path);
        printf("STEP %d: create(sb, \"%s\");", 
               n, path);
        r = tfs_create(sb, path);
	printf("STEP %d: create(sb, \"%s\") = %d",
	       n, path, r);	
      },
      [&r, n, sb, &path, &data] {
	MakeNewPath(path);
        MakeNewData(data);
        printf("STEP %d: write(sb, \"%s\", \"%s\");", 
               n, path, data);
        r = tfs_write(sb, path, data);
	printf("STEP %d: write(sb, \"%s\", \"%s\") = %d",
	       n, path, data, r);	
      },
      [&r, n, sb, &path] {
	MakeNewPath(path);
        printf("STEP %d: stat(sb, \"%s\");", 
               n, path);
        r = tfs_stat(sb, path);
	printf("STEP %d: stat(sb, \"%s\") = %d",
	       n, path, r);	
      },
      [n, sb] {
	ASSUME_EQ(get_reset_countdown(), -1); // Only one reset at a time
	symbolic_int k;
	ASSUME_GT(k, 0);
	ASSUME_LT(k, MAX_RESET);
	printf("STEP %d: set_reset-countdown(%d);", n, k);
	set_reset_countdown(k);
      });

    if (get_reset_countdown() == 0) {
      LOG(INFO) << "Reset took place during operation.";
      set_reset_countdown(-1);
      ASSERT(!testfs_init_super_block(storage, 0, &sb))
	<< "Couldn't initialize super block";
    }
    
    LOG(INFO) << "Checking the file system...";
    tfs_checkfs(sb);
  }

  testfs_close_super_block(sb);
}

#ifndef LIBFUZZER
int main(int argc, char *argv[]) {
  DeepState_InitOptions(argc, argv);
  return DeepState_Run();
}
#endif
