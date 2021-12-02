/* Copyright (c) 2014, 2019, Oracle and/or its affiliates. All rights reserved.

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

/*
 * This file contains the consistent view data structrue used by
 * relay SQL thread apply event.
 */

#ifndef PLUGIN_CONSISTENT_VIEW_H_
#define PLUGIN_CONSISTENT_VIEW_H_

#include <unordered_map>
#include <time.h>

#include "mysql/service_mysql_alloc.h"
#include "my_sys.h"
#include "my_bitmap.h"

#define MAX_COMPUT_NODE_NUM 100
typedef uint32 COMP_ID;
typedef uint32 TXN_SEQ_COMP;

/* This is the infras for the RAII operation */
class Gc_RAII{
 public:
  Gc_RAII() = default;

  void bind_resource(void *opaque_ptr){
    ptr = opaque_ptr;
  }

  ~Gc_RAII(){
      my_free(ptr);
  }

  /* Forbide copy */
  Gc_RAII( Gc_RAII const&) = delete;
  Gc_RAII &operator=( Gc_RAII const&) = delete;

 private:
  void * ptr;
};

enum Retval_Status{

  /** No error occurred and execution should continue. */
  OK_CONTINUE = 0,
  /** An error occurred and execution should stop. */
  ERROR_STOP,
  /** No error occurred but execution should stop. */
  OK_STOP
};

class Xid_info{

 public:
  Xid_info(){
    xid_str_inited = false;
  }
  bool init_xid_info(const char *);

  /* Setter */
  void set_info_seperate(const COMP_ID &,
                        const time_t &,
                        const TXN_SEQ_COMP &);

  /* Getter */
  COMP_ID get_comp_id() const{
    return comp_node_id;
  }
  time_t get_prep_ts() const{
    return prepare_ts;
  }
  TXN_SEQ_COMP get_txn_seq() const{
    return trx_seq;
  }

  ~Xid_info();

 private:
  bool xid_parse();
  bool xid_str_inited;
  DYNAMIC_STRING org_xid_str;
  

 protected:
  COMP_ID comp_node_id;
  time_t prepare_ts; //unix timestamp
  TXN_SEQ_COMP trx_seq;
};



class Commit_log_entry: public Xid_info{
 public:

  enum TXN_ACTION{
    XTXN_COMMIT = 0,
    XTXN_ABORT
  };
  
  Commit_log_entry(char * commit_log_str);
  Retval_Status init_commit_log_entry();
  void set_xid_aciton(const std::string &);
  bool is_later_than(const Xid_info &) const;
  
  TXN_ACTION get_xid_action() const {
    return xid_action;
  }
  std::string get_prepare_ts_str() const {
    return prepare_ts_str;
  }


  /* forbide copy */
  Commit_log_entry(const Commit_log_entry&) = delete;
  Commit_log_entry &operator=(const Commit_log_entry&) = delete;

 private:
  char * org_commit_log_entity;
  TXN_ACTION xid_action;
  std::string prepare_ts_str;
};


typedef std::unordered_map<int,bool> Consistent_bitmap;
/*
  *No Thread-Safe*
  The Consistent_view object will only performed by 
  the single SQL thread in the relay log apply process.
*/
class Consistent_view{

 public:
  Consistent_view();  

  Retval_Status init_and_parse(const char * clog_path);

  Retval_Status consistent_check_global(const Xid_info &);

  void store_clog(Commit_log_entry *&);

  void set_bitmap_by_xid(const COMP_ID &,bool);

  
  /* Forbide copy */
  Consistent_view(Consistent_view const &) = delete;
  Consistent_view &operator=(Consistent_view const &) = delete;

  ~Consistent_view();

 private:
  bool all_finish();

 private:
  bool m_inited;
  char *commit_logpath;

  /* specify the stop xid info by compute id */
  std::unordered_map<int,Commit_log_entry *> stop_point_by_id;

  /* 
    Bitmap to indentify whether the specified compute
    id's stop_point have already reached.
    0 means unreached.
    1 means reached.
  */
  Consistent_bitmap m_bitmap;
};

#endif /*PLUGIN_CONSISTENT_VIEW_H_*/

