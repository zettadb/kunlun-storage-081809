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

#include <vector>
#include <fcntl.h>

#include "boost/tokenizer.hpp"
#include "consistent_view.h"

/*
 delimiter:';' 
 '1;7023615557370905173;commit;2021-10-27 13:35:31'
*/
#define COMMIT_LOG_ELEMENT_LEGAL_SIZE 4 
/*
 delimiter:'-' 
 '1-1635312910-949211'
*/
#define XID_ELEMENT_LEGAL_SIZE 3
typedef boost::tokenizer<boost::char_separator<char>> tokenizer;


bool Xid_info::init_xid_info(const char * xid_str){

  bool retval = init_dynamic_string(&org_xid_str, 
                                    xid_str, 512, 1024);
  if(retval){
    return false;
  }
  xid_str_inited = true;

  return xid_parse();
}

void Xid_info::set_info_seperate(const COMP_ID & compid,
                       const time_t & ts,
                       const TXN_SEQ_COMP & seq){
  comp_node_id = compid;
  prepare_ts = ts;
  trx_seq = seq;
}

bool Xid_info::xid_parse(){

  /*
   Here decleare the container for the tokenizer result.
   */
  std::vector<std::string> tokens;

  std::string str_tmp = org_xid_str.str;
  boost::char_separator<char> sep{"-"};
  tokenizer tok{str_tmp,sep};
  for(const auto &t:tok){
    tokens.push_back(t);
  }

  /* 
    The format of a valid xid is '1-1635312910-949211', It's consist 
    of 'compute_node_id - unixtimestamp - compute_node_local_txnid'
    The delimter is '-'.So we test the counter of the delimetr.
  */
  if( tokens.size() != XID_ELEMENT_LEGAL_SIZE ){
    return false;
  }

  comp_node_id = atoi(tokens[0].c_str());//Fisrt element is comput node id.
  prepare_ts = atol(tokens[1].c_str());//Second element is prepare timestamp.
  trx_seq = atoi(tokens[2].c_str());//Third element is trx id.
  return true;

}

Xid_info::~Xid_info(){ 
  /* release the dynstr */
  if(xid_str_inited) dynstr_free(&org_xid_str);
}


Commit_log_entry::Commit_log_entry(char * commitlog_str)
  :org_commit_log_entity(commitlog_str),xid_action(Commit_log_entry::XTXN_COMMIT){
}

bool Commit_log_entry::is_later_than(const Xid_info &xidinfo) const {
  return prepare_ts > xidinfo.get_prep_ts();
}

Retval_Status Commit_log_entry::init_commit_log_entry(){
  std::string pre_tok_str = org_commit_log_entity;
  std::vector<std::string> tokens;
  boost::char_separator<char> sep{";"};
  tokenizer tok{pre_tok_str,sep};
  for(const auto &t : tok){
    tokens.push_back(t);
  }

  if(tokens.size() != COMMIT_LOG_ELEMENT_LEGAL_SIZE){
    return ERROR_STOP;
  }

  comp_node_id = atoi(tokens[0].c_str());
  //Fisrt element is comput node id.
  uint64_t var_tmp = static_cast<uint64_t>(atol(tokens[1].c_str())); 
  //TODO 
  //to be elegant
  prepare_ts = var_tmp >> 32;
  trx_seq = var_tmp & 0x00000000FFFFFFFF;

  set_xid_aciton(tokens[2]);
  prepare_ts_str = tokens[3].c_str();//The fourth element is prepare timestamp.
  return OK_STOP;
}

void Commit_log_entry::set_xid_aciton(const std::string &str){
  xid_action = str == "commit" ? XTXN_COMMIT:XTXN_ABORT;
}

Consistent_view::Consistent_view():m_inited(false){
}

Consistent_view::~Consistent_view(){
}

typedef my_bitmap_map BITMPA_BUF;
#define BUFFER_SIZE 2048
/*
  Main purpose is to build the consistent_view 
  from the commit_log file.
  Include the stop_point map and the releated 
  bitmap.
*/
Retval_Status Consistent_view::init_and_parse(const char * clog_path){
  if(m_inited){
    return OK_STOP;
  }

  m_inited = true;
  //clean the bit map
  m_bitmap.clear();
  commit_logpath = const_cast<char *>(clog_path);

  FILE *fptr = nullptr;
  fptr = my_fopen(commit_logpath,O_RDONLY,MYF(MY_FFNF));
  if(!fptr) return ERROR_STOP;

  //Read commit log file.
  Retval_Status retval;
  char buff[BUFFER_SIZE] = {'\0'};
  while( fgets(buff,BUFFER_SIZE,fptr) != nullptr ){
    Commit_log_entry * clog_ptr = new Commit_log_entry(buff);
    retval = clog_ptr->init_commit_log_entry();
    if(retval != OK_STOP) goto err;
    store_clog(clog_ptr);
  }
  goto end;

err:
  my_fclose(fptr,MYF(0));
  return retval;

end:
  my_fclose(fptr,MYF(0));
  return OK_STOP;
}

void Consistent_view::set_bitmap_by_xid(const COMP_ID & comp_id ,
                                        bool value ){
  m_bitmap[comp_id] = value;
}

bool Consistent_view::all_finish(){
  bool tmp;
  for(auto &t:m_bitmap){
    tmp &= t.second; 
  }
  return tmp;
}

Retval_Status Consistent_view::consistent_check_global(const Xid_info & xidinfo){
  COMP_ID comp_id = xidinfo.get_comp_id();
  auto iter = stop_point_by_id.find(comp_id);
  if(iter == stop_point_by_id.end()){
    return OK_CONTINUE;
  }
  Commit_log_entry *clog_ptr = iter->second;
  if(clog_ptr->is_later_than(xidinfo)){
    //set the releated bitmap
    set_bitmap_by_xid(xidinfo.get_comp_id(),true);
    if(all_finish()) return OK_STOP;
  }
  return OK_CONTINUE;
}

/*
 store the clog handler to the hash map 
 by the way deal the bitset.
*/
void Consistent_view::store_clog(Commit_log_entry *& clog_ptr){
  //deal the map
  COMP_ID id = clog_ptr->get_comp_id();
  auto iter = stop_point_by_id.find(id); 
  if(iter != stop_point_by_id.end()){
    delete iter->second;
    iter->second = nullptr;
  }
  stop_point_by_id[id] = clog_ptr;
  m_bitmap[id] = false;
}


