/* Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <string>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#define MYSQL_DYNAMIC_PLUGIN
#include "plugin/global_consistent_plugin/consistent_view.h"

namespace global_consistent_unittest {

class xid_info_test : public ::testing::Test {
 public:
  xid_info_test() = default;
  ~xid_info_test() = default;
};

const char * xid_str = "1-1635312910-949211";
const char * commit_log_entry = "1;7023615583140708963;commit;2021-10-27 13:35:37";
const char * clog_path = "/home/summerxwu/play_ground/commitlog";

void test_xid_parse() {
  Xid_info xid_info;
  bool retval = xid_info.init_xid_info(xid_str);
  printf("%d-%ld-%d\n",
         xid_info.get_comp_id(),
         xid_info.get_prep_ts(),
         xid_info.get_txn_seq());
  EXPECT_EQ(true, retval);

}

void test_commitlog_entry_parse(){
  Commit_log_entry c_log_entry(const_cast<char*>(commit_log_entry));
  Retval_Status retval = c_log_entry.init_commit_log_entry();
  printf("%d-%ld-%d\n",
         c_log_entry.get_comp_id(),
         c_log_entry.get_prep_ts(),
         c_log_entry.get_txn_seq());
  printf("prepare_ts_str: %s,xid_action: %d\n",
         c_log_entry.get_prepare_ts_str().c_str(),
         c_log_entry.get_xid_action());
  EXPECT_EQ(OK_STOP, retval);
}

void test_commitlog_init(){
  Retval_Status ret;
  Consistent_view consistent_view;
  ret = consistent_view.init_and_parse(clog_path);
  printf("%d\n",ret);
  EXPECT_EQ(OK_STOP, ret);
}

//Test the infras of the xid_info
TEST(xid_info_test, utilTest) {
  //test_xid_parse();
  //test_commitlog_entry_parse();
  test_commitlog_init();

}

}  // namespace ddl_rewriter_unittest
