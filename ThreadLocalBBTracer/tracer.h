/** 
 * Copyright (c) 2017, Rahman Lavaee All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  - Redistributions of source code must retain the above copyright notice, 
 *    this list of conditions and the following disclaimer.
 *
 *  - Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.  
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#pragma once
#include <block.h>
#include <vector>
#include <list>
#include <functional>
#include <unordered_map>
#include <stdint.h>
#include <cstring>
#include <ostream>
#include <fstream>
#include <memory>
#include <boost/container/flat_map.hpp>
#include <sys/stat.h>
#include <iostream>
#include <sys/syscall.h>


pid_t _gettid( void )
{
  return syscall( SYS_gettid);
}


bool * read_metadata;

using namespace std;

const size_t nconsumers = 1;

//namespace fs = std::experimental::filesystem;
namespace fs {

  bool create_directory(const std::string& filepath){
    return (mkdir(filepath.c_str(),0777)==0);
  }

  bool exists(const std::string& filepath){
    ifstream f(filepath.c_str());
    return f.good();
    
  }
}
long int timestamp;

const char * profilePath = NULL;
const char * benchmarkName = NULL;
const char * doProfiling = NULL;
const char * doLayout = NULL;
const char * procSuffix = NULL;

const char * version_str = ".bbabc";
bool do_profile = false;
  bool do_affinity = false;
bool do_proc_suffix = false;



wsize_t mws;
unsigned max_code_size;

class PartialWindow{
  public:
  unsigned code_size;
  list<BBlock> partial_list;
  BBlock owner;
  std::map<func_t,num_t>* joint_freq_it;

  PartialWindow(BBlock rec);

  ~PartialWindow(){
    partial_list.clear();
  }

  void erase(const list<BBlock>::iterator &trace_iter,unsigned rec_bb_size){
    partial_list.erase(trace_iter);
    code_size-=rec_bb_size;
  }

  void push_front(BBlock rec, unsigned rec_bb_size){
    partial_list.push_front(rec);
    code_size+=rec_bb_size;
  }

};



char * affinity_basename;

typedef pair<bb_t,uint32_t> bb_count_t;

BBlockPair get_sorted_pair(BBlock b1, BBlock b2){
  if(std::less<BBlock>()(b1,b2))
    return BBlockPair(b1,b2);
  else
    return BBlockPair(b2,b1);
}


typedef std::pair<BBlockPair,wsize_interval> freq_update_t;



//inline void print_trace(const list<PartialWindow>&);
inline void initialize_affinity_data();
//inline void sequential_update_affinity(const BBlock& rec, list<PartialWindow>::iterator pw_end, bool sampled);
extern "C" void affinityAtExitHandler();
void affinityAtExitHandlerInt(int);

std::string get_timestamped_filepath(const char* basename){
   std::string filePath("");


  if(profilePath!=NULL){
    filePath+=std::string(profilePath)+"/";
  }

  //fprintf(stderr,"basename: %s\n",affinity_basename.c_str());

  filePath+=std::string(affinity_basename);

  if(!fs::exists(filePath) && fs::create_directory(filePath)){
      //std::cerr << "Success in creating directory: " << filePath << "\n";
  }


  if(benchmarkName!=NULL){
    filePath += "/"+std::string(benchmarkName);
     if(!fs::exists(filePath) && fs::create_directory(filePath)){
        //std::cerr << "Success in creating directory: " << filePath << "\n";
     }
  }

 
  
     filePath+="/"+std::to_string(timestamp);
     if(!fs::exists(filePath) && fs::create_directory(filePath)){
        //std::cerr << "Success in creating directory: " << filePath << "\n";
     }

   filePath+="/"+std::to_string(_gettid());
     if(!fs::exists(filePath) && fs::create_directory(filePath)){
        //std::cerr << "Success in creating directory: " << filePath << "\n";
     }

  filePath += "/"+std::to_string(static_cast<long int>(time(NULL)));

  if(!fs::exists(filePath) && fs::create_directory(filePath)){
      //std::cerr << "Success in creating directory: " << filePath << "\n";
  }


  filePath+="/"+std::string(basename)+std::string(version_str);

  return filePath;
}


std::string get_thread_versioned_filepath(const char* basename){
  std::string filePath("");

  if(profilePath!=NULL){
    filePath+=std::string(profilePath)+"/";
  }

  filePath+=std::string(affinity_basename);

  cerr << "file path: " << filePath << "\t" << affinity_basename << "\n";

  if(!fs::exists(filePath) && fs::create_directory(filePath)){
      //std::cerr << "Success in creating directory: " << filePath << "\n";
  }

  if(benchmarkName!=NULL){
    filePath += "/"+std::string(benchmarkName);
     if(!fs::exists(filePath) && fs::create_directory(filePath)){
        //std::cerr << "Success in creating directory: " << filePath << "\n";
     }
  }
  
/*
  if(do_proc_suffix){
     filePath+="/"+std::to_string(getpid());
     if(!fs::exists(filePath) && fs::create_directory(filePath)){
        //std::cerr << "Success in creating directory: " << filePath << "\n";
     }
  }
*/

     filePath+="/"+std::to_string(timestamp);
     if(!fs::exists(filePath) && fs::create_directory(filePath)){
        //std::cerr << "Success in creating directory: " << filePath << "\n";
     }

  

     filePath+="/"+std::to_string(_gettid());
     if(!fs::exists(filePath) && fs::create_directory(filePath)){
        //std::cerr << "Success in creating directory: " << filePath << "\n";
     }


  //filePath+=std::string(basename)+std::string(".")+affinity_basename+std::string(version_str);
  filePath+="/"+std::string(basename)+std::string(version_str);

  return filePath;

}

std::string get_process_versioned_filepath(const char* basename){
   std::string filePath("");

  if(profilePath!=NULL){
    filePath+=std::string(profilePath)+"/";
  }

  //cerr << "affinity base name: " << affinity_basename << "\n";

  filePath += std::string(affinity_basename);

  cerr << "file path: " << filePath << "\t" << affinity_basename << "\n";

  if(!fs::exists(filePath) && fs::create_directory(filePath)){
      //std::cerr << "Success in creating directory: " << filePath << "\n";
  }

  if(benchmarkName!=NULL){
    filePath += "/"+std::string(benchmarkName);
     if(!fs::exists(filePath) && fs::create_directory(filePath)){
        //std::cerr << "Success in creating directory: " << filePath << "\n";
     }
  }

     filePath+="/"+std::to_string(timestamp);
     if(!fs::exists(filePath) && fs::create_directory(filePath)){
        //std::cerr << "Success in creating directory: " << filePath << "\n";
     }

  
  //filePath+=std::string(basename)+std::string(".")+affinity_basename+std::string(version_str);
  filePath+="/"+std::string(basename)+std::string(version_str);

  return filePath;
}

inline size_t get_lg(unsigned i){
  size_t l = 0;
  int step = 8;
    while(i>>=step){
      l++;
      //step = (step+1)/2;
      step = (step == 8)?2:1;
    }
    return l;
}

void emit_code_metadata();
void emit_graphFile();

template <class T>
  Map<T> getMap(T *val){
  Map<T> m;
  for(unsigned obj_id=0; obj_id<HASH_MAX; ++obj_id){
    if(read_metadata[obj_id])
      m.insert(std::make_pair((hash_t)obj_id,std::move(val[obj_id])));
  }
  return m;
}

template <class T>
  Map<T> getMap(Vector<T> val, Vector<std::vector<bool>>& maps_initialized){
  Map<T> m;
  for(unsigned obj_id=0; obj_id<HASH_MAX; ++obj_id){
    if(!maps_initialized[obj_id].empty())
      m.insert(std::make_pair((hash_t)obj_id,std::move(val[obj_id])));
  }
  return m;
}

