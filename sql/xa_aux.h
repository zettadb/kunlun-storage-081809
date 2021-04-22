/*
   Copyright (c) 2015, 2019, Oracle and/or its affiliates. All Rights Reserved.
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

#ifndef XA_AUX_H
#define XA_AUX_H
#include "m_string.h"  // _dig_vec_lower
#include <set>
#include <string>
#include <functional>

/**
  Function serializes XID which is characterized by by four last arguments
  of the function.
  Serialized XID is presented in valid hex format and is returned to
  the caller in a buffer pointed by the first argument.
  The buffer size provived by the caller must be not less than
  8 + 2 * XIDDATASIZE +  4 * sizeof(XID::formatID) + 1, see
  XID::serialize_xid() that is a caller and plugin.h for XID declaration.

  @param buf  pointer to a buffer allocated for storing serialized data
  @param fmt  formatID value
  @param gln  gtrid_length value
  @param bln  bqual_length value
  @param dat  data value

  @return  the value of the buffer pointer
*/

char *serialize_xid(char *buf, long fmt, long gln, long bln,
                           const char *dat);

bool deserialize_xid(const char *buf, long &fmt, long &gln, long &bln, char *dat);



class Prepared_xa_txnids
{
  typedef std::set<std::string> Txnids_t;
  class Slot
  {
    pthread_mutex_t mutex;
    Txnids_t txnids;
  public:
    Slot()
    {
      pthread_mutex_init(&mutex, NULL);
    }
    ~Slot()
    {
      pthread_mutex_destroy(&mutex);
    }
    void add_id(const std::string &id)
    {
      pthread_mutex_lock(&mutex);
      txnids.insert(id);
      pthread_mutex_unlock(&mutex);
    }
  
    void del_id(const std::string &id)
    {
      pthread_mutex_lock(&mutex);
      /*
       * It's likely that XA PREPARE not executed so id not found.
       * */
      Txnids_t::iterator i = txnids.find(id);
      if (i != txnids.end())
        txnids.erase(i);
      pthread_mutex_unlock(&mutex);
    }
  
    void serialize(std::string &id);
  };

  const static size_t NSLOTS = 1024;

  Slot m_slots[NSLOTS];
  uint hash_slot(const std::string &id)
  {
    return std::hash<std::string>{}(id) % NSLOTS;
  }
public:
  Prepared_xa_txnids()
  {
  }
  ~Prepared_xa_txnids()
  {
  }

  void add_id(const std::string &id)
  {
    m_slots[hash_slot(id)].add_id(id);
  }
  
  void del_id(const std::string &id)
  {
    m_slots[hash_slot(id)].del_id(id);
  }

  void serialize(std::string &id);

  void from_recovery(Txnids_t &prepared);

  void from_recovery(Txnids_t &prepared, const Txnids_t &committed,
              const Txnids_t &aborted);
  static int parse(const char *str, Txnids_t &ids);
};

extern Prepared_xa_txnids prepared_xa_txnids;

#endif /* XA_AUX_H */
