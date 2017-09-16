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

#include <code_layout.h>
#include "tracer.h"
#include <stdio.h>
#include <fstream>
#include <algorithm>
#include <string.h>
#include <vector>
#include <tgmath.h>
#include <serialize.h>
#include <thread>
#include <unistd.h>
#include <atomic>
#include <semaphore.h>
#include <boost/lockfree/queue.hpp>
#include <boost/lockfree/spsc_queue.hpp>
#include <errno.h>
#include <pthread.h>
#include <time.h>
#include <stack>
#include <fcntl.h>
#include <condition_variable>
#include <mutex>
#include <sys/types.h>
#include <sys/wait.h>

#define MAX_TRACE_LEN 1<<20
#define get_rec_it(p) p->second.first
#define get_pw_it(p) p->second.second

#define set_rec_it(p,rec_it) p->second.first = rec_it
#define set_pw_it(p,pw_it) p->second.second = pw_it

int total_trace_size = 0;


std::atomic<int> prof_th;

bool DEBUG;
int sampleRateLog;
uint32_t sampleSize;
uint32_t sampleMask;


std::ofstream debug_os;


func_t * func_count;
std::string* object_hash_map;
FuncNameMap * func_names;
BBCountMap * bb_count;
BBSizeMap * bb_size;
CSSizeMap * cs_size;
UCFGMap * ucfg;
BBCFGMap * bb_cfg;
BBReturnMap * bb_return;

unsigned short prof_time=0;




pthread_mutex_t flush_mutex =  PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP;





class BBProfiler {
	public:
		BBlock pred;
		bool pred_valid;
		bool locked = false;
		bool destroying = false;

		Vector<FallThroughMap> fall_thrus;
		Vector<CallCountMap> calls;
		Vector<BBExecCountMap> bb_exec_count;
		Vector<SimpleJointFreqMap> joint_freqs;
		Vector<std::vector<bool>> maps_initialized;
		Vector<UCFGMap> local_ucfg;


		std::unordered_map <BBlock, std::pair< list<BBlock>::iterator,list<PartialWindow>::iterator>> trace_map;
		std::unordered_map <BBlock, list<PartialWindow>::iterator> window_map;

		list<PartialWindow> trace_list;

		unsigned trace_code_size;
		unsigned total_trace_size;

		inline void emit_graphFile(){

			std::filebuf graph_buf;
			graph_buf.open (get_thread_versioned_filepath("graph"),std::ios::out);

			OutputStream graph_os(&graph_buf);
			serialize(getMap(bb_exec_count,maps_initialized),graph_os);
			serialize(getMap(fall_thrus,maps_initialized),graph_os);
			serialize(getMap(calls,maps_initialized),graph_os);
		}

		void emit_joint_freqs(){
			if(!do_affinity)
				return;
			std::filebuf graph_buf;
			graph_buf.open (get_timestamped_filepath("joint-freqs"),std::ios::out);
			OutputStream graph_os(&graph_buf);
			serialize(getMap(joint_freqs,maps_initialized),graph_os);

		}



		inline void sequential_update_affinity(const BBlock& rec, list<PartialWindow>::iterator pw_end, bool sampled){

			auto pw_it = trace_list.begin();
			if(sampled)
				pw_it++;

			while(pw_it!= pw_end){

				if(rec.equal_object(pw_it->owner)){
					auto res = (*pw_it->joint_freq_it).emplace(std::piecewise_construct,
							std::forward_as_tuple(rec.get_fid()),
							std::forward_as_tuple(1));
					if(!res.second)
						res.first->second++;
					else
						total_trace_size++;

				}

				pw_it++;
			}

		} 



		inline void flush_trace(){

			while(!trace_list.empty()){
				auto& pw = trace_list.back();

				window_map.erase(pw.owner);

				list<BBlock>& plist= pw.partial_list;

				while(!plist.empty()){
					auto oldRec=plist.front();
					trace_map.erase(oldRec);
					plist.pop_front();
				}

				trace_code_size-=pw.code_size;
				trace_list.pop_back();
			}

			assert(trace_code_size==0);

		}

		void record_bb_exec(BBlock rec){

			bool sampled=((rand() & sampleMask)==0);

			if(sampled){
				PartialWindow pw(rec);
				trace_list.push_front(pw);
			}

			if(trace_code_size || sampled){
				unsigned rec_bb_size = bb_size[rec.get_obj_id()].at(rec.get_fid()).at(rec.get_bbid());
				trace_list.front().push_front(rec,rec_bb_size);

				auto trace_map_it = trace_map.find(rec);

				if(trace_map_it == trace_map.end()){
					trace_code_size+=rec_bb_size;

					while(trace_code_size >= max_code_size){ /*trace_list overflowed, reducing from back*/
						auto &pw = trace_list.back();

						window_map.erase(pw.owner);

						for(auto oldRec : pw.partial_list)
							trace_map.erase(oldRec);

						trace_code_size -= pw.code_size;
						trace_list.pop_back();
					}

					if(trace_code_size>0){
						if(rec.get_bbid()==0)
							sequential_update_affinity(rec,trace_list.end(),sampled);

						if(sampled)
							window_map[rec] = trace_list.begin();


						trace_map[rec] = std::make_pair < list<BBlock>::iterator, list<PartialWindow>::iterator > (trace_list.front().partial_list.begin() , trace_list.begin());
					}

				}else{
					auto& pw_it = get_pw_it(trace_map_it);
					auto& rec_it = get_rec_it(trace_map_it);

					pw_it->erase(rec_it,rec_bb_size);

					if(rec.get_bbid()==0)
						sequential_update_affinity(rec,pw_it,sampled);

					set_pw_it(trace_map_it,trace_list.begin());
					set_rec_it(trace_map_it,trace_list.front().partial_list.begin());


					if(sampled){
						auto res = window_map.emplace(std::piecewise_construct,
								std::forward_as_tuple(rec),
								std::forward_as_tuple(trace_list.begin()));
						if(!res.second){
							res.first->second = trace_list.begin();
						}

					}else  {
						auto window_map_it = window_map.find(rec);
						if(window_map_it!=window_map.end()){
							window_map.erase(window_map_it);
						} 
					}
				}
			}
		}



		BBProfiler(){
			locked = true;
			pred_valid = false;
			destroying  = false;
			total_trace_size=0;
			trace_code_size=0;
			fall_thrus.resize(HASH_MAX);
			calls.resize(HASH_MAX);
			bb_exec_count.resize(HASH_MAX);
			local_ucfg.resize(HASH_MAX);
			maps_initialized.resize(HASH_MAX);
			if(do_affinity)
				joint_freqs.resize(HASH_MAX);
			locked = false;

		}

		~BBProfiler(){
			destroying = true;
			locked = true;

			for(unsigned obj_id=0; obj_id<HASH_MAX; obj_id++)
				if(!maps_initialized[obj_id].empty()){
					auto& _maps_init = maps_initialized[obj_id];
					auto& _ucfg = local_ucfg[obj_id];
					auto& _fall_thru = fall_thrus[obj_id];
					auto& _exec_count = bb_exec_count[obj_id];
					for(func_t fid=0; fid< _ucfg.size(); ++fid)
						if(_maps_init[fid]){
							auto& _ucfg_func = _ucfg[fid];
							auto& _exec_count_func = _exec_count[fid];
							auto& _fall_thru_func = _fall_thru[fid];
							for(bb_t bbid=0; bbid< _ucfg_func.size(); ++bbid)
								if(_ucfg_func[bbid].size()==1 && _exec_count_func[bbid]!=0)
									_fall_thru_func[bbid][_ucfg_func[bbid][0]] = _exec_count_func[bbid];
						}
				}

			emit_graphFile();

			if(do_affinity)
				flush_trace();



			pred_valid = false;
			locked = false;
		}


		inline bool initializeMap(hash_t obj_id, func_t fid){
			auto& _maps_init = maps_initialized[obj_id];
			if(_maps_init.empty()){
				pthread_mutex_lock(&flush_mutex);

				if(read_metadata[obj_id]==true){
					auto fcount = func_count[obj_id];
					//assert(fcount!=0 && "fcount==0");
					cerr << "first initialization for obj_id: " << obj_id << "\t: size:" << fcount << "\t from tid: "<< _gettid() << "\n";
					auto& _exec_count = bb_exec_count[obj_id];
					_exec_count.resize(fcount);
					_exec_count.shrink_to_fit();
					auto& _fall_thrus = fall_thrus[obj_id];
					_fall_thrus.resize(fcount);
					_fall_thrus.shrink_to_fit();
					auto& _calls = calls[obj_id];
					_calls.resize(fcount);
					_calls.shrink_to_fit();
					if(do_affinity){
						//joint_freqs[obj_id].resize(fcount);
						//joint_freqs[obj_id].shrink_to_fit();
						//local_bb_size[obj_id].resize(fcount);
						//local_bb_size[obj_id].shrink_to_fit();
					}

					_maps_init.resize(fcount,false);
					_maps_init.shrink_to_fit();

					auto& _ucfg = local_ucfg[obj_id];
					_ucfg.resize(fcount);
					_ucfg.shrink_to_fit();

				}

				pthread_mutex_unlock(&flush_mutex);

			}

			if(!_maps_init.empty()){

				//cerr << "maps_initialized size; " << obj_id << " : " << maps_initialized[obj_id].size() << "\n";
				if(!_maps_init[fid]){
					pthread_mutex_lock(&flush_mutex);
					//assert(read_metadata[obj_id]);

					auto bbcount = bb_count[obj_id][fid];
					//cerr << "initialized for: " << BBlock(obj_id,fid,0) << "\t" << bbcount << "\n";
					auto& _exec_count = bb_exec_count[obj_id][fid];
					_exec_count.resize(bbcount,0);
					_exec_count.shrink_to_fit();
					auto& _fall_thrus = fall_thrus[obj_id][fid];
					_fall_thrus.resize(bbcount);
					_fall_thrus.shrink_to_fit();
					auto& _calls = calls[obj_id][fid];
					_calls.resize(bbcount);
					_calls.shrink_to_fit();
					if(do_affinity){
						joint_freqs[obj_id].at(fid).resize(bbcount,std::make_pair(0,std::map<func_t,num_t>()));
						//local_bb_size[obj_id].at(fid) = bb_size[obj_id].at(fid);
					}

					local_ucfg[obj_id][fid] = ucfg[obj_id][fid];

					_maps_init[fid] = true;
					pthread_mutex_unlock(&flush_mutex);
				}
				return true;
			}else
				return false;


		}

		inline void incCallCount(hash_t obj_id, func_t fid){
			if(pred_valid && pred.get_obj_id()==obj_id){
				auto res = calls[obj_id][pred.get_fid()][pred.get_bbid()].emplace(fid,1);
				if(!res.second)
					res.first->second++;
			}	
		}


		inline void incExecCount(hash_t obj_id, func_t fid, bb_t bbid){
			bb_exec_count[obj_id][fid][bbid]++;
		}



		inline void incFallThru(hash_t obj_id, func_t fid, bb_t bbid){
			if(pred_valid && pred.get_obj_id()==obj_id && pred.get_fid()==fid && local_ucfg[obj_id][fid][pred.get_bbid()].size()>=2){
				auto res = fall_thrus[obj_id][fid][pred.get_bbid()].emplace((bb_t)bbid,1);
				if(!res.second)
					res.first->second++;
			}
		}

};


thread_local BBProfiler bb_profiler;




PartialWindow::PartialWindow(BBlock rec){
	owner = rec;
	code_size = 0;
	//bb_profiler.joint_freqs[rec.get_obj_id()].at(rec.get_fid()).at(rec.get_bbid()).first++;
	//joint_freq_it = &bb_profiler.joint_freqs[rec.get_obj_id()].at(rec.get_fid()).at(rec.get_bbid()).second;
}





/* The data allocation function (totalFuncs need to be set before entering this function) */
void initialize_affinity_data(){
	srand(time(NULL));

	timestamp = static_cast<long int>(time(NULL));
	//cerr << "timestamp is : \t" << timestamp << "\t" << getpid() << "\n";

	if(DEBUG)
		debug_os.open("debug.txt");


	//std::filebuf graph_buf;
	//graph_buf.open (get_thread_versioned_filepath("graph"),std::ios::in);

}


void emit_code_metadata(){

	std::filebuf code_buf;
	code_buf.open (get_process_versioned_filepath("code"),std::ios::out);
	OutputStream code_os(&code_buf);

	serialize(getMap(func_count),code_os);
	serialize(getMap(object_hash_map),code_os);
	serialize(getMap(func_names),code_os);
	serialize(getMap(bb_count),code_os);
	serialize(getMap(ucfg),code_os);
	serialize(getMap(bb_size),code_os);
	serialize(getMap(cs_size),code_os);
	serialize(getMap(bb_cfg),code_os);
	serialize(getMap(bb_return),code_os);

}


/* Must be called at exit*/
extern "C" void affinityAtExitHandler(){
	//cerr << "exiting\t" << getpid() << "\n";
	pthread_mutex_lock(&flush_mutex);
	fprintf(stderr,"emitting graphs: %s\n",profilePath);
	//emit_graphFile();
	emit_code_metadata();
	pthread_mutex_unlock(&flush_mutex);
}

/*
	 void print_trace(const list<PartialWindow> &tlist){

	 debug_os << "---------------------------------------------\n";
	 debug_os << "trace size : " << trace_code_size << "\n";
	 unsigned total_code_size = 0;
	 for(auto pw: tlist){
	 debug_os << "sampled window: \t";
	 total_code_size += pw.code_size;
	 debug_os << "^^^^^^^^^  " << pw.owner << "\t" << pw.code_size << "\n >>>>>>>> ";
	 debug_os << pw.code_size;
	 for(auto b: pw.partial_list)
	 debug_os << b << "##";

	 debug_os << "\n";
	 debug_os.flush();

	 }
	 debug_os << "---------------------------------------------\n\n\n";
	 debug_os.flush();
	 }
 */



static void save_affinity_environment_variables(void) {
	const char *SampleRateEnvVar,  *DebugEnvVar, *MaxCodeSizeLgVar,  *DoAffinityEnvVar;

	if((MaxCodeSizeLgVar = getenv("BBABC_CODE_SIZE")) != NULL){
		int MaxCodeSizeLg = atoi(MaxCodeSizeLgVar);
		max_code_size = 1 << MaxCodeSizeLg;
	}else
		max_code_size = 1 << 11;

	mws = get_lg(max_code_size);

	benchmarkName = getenv("BBABC_BENCHMARK");

	if((DebugEnvVar = getenv("BBABC_DEBUG")) !=NULL)
		DEBUG = (strcmp(DebugEnvVar,"ON")==0);

	if ((SampleRateEnvVar = getenv("BBABC_SAMPLE_RATE")) != NULL) 
		sampleRateLog = atoi(SampleRateEnvVar);
	else
		sampleRateLog = 8;

	sampleSize= RAND_MAX >> sampleRateLog;
	sampleMask = sampleSize ^ RAND_MAX;

	if((DoAffinityEnvVar = getenv("BBABC_DO_AFFINITY")) != NULL) 
		do_affinity = (strcmp(DoAffinityEnvVar,"ON")==0);


	if((profilePath = getenv("BBABC_PROF_PATH"))==NULL || strcmp(profilePath,"")==0){
		cerr << "BBABC_PROF_PATH is not set\n";
		exit(-1);
	}

	if((doProfiling = getenv("BBABC_DO_PROF"))!=NULL)
		do_profile = (strcmp(doProfiling,"ON")==0);

}

/* llvm_start_basic_block_tracing - This is the main entry point of the basic
 * block tracing library.  It is responsible for setting up the atexit
 * handler and allocating the trace buffer.
 */
extern "C" int set_func_count(uint64_t arg){
	BBlock b(arg);
	if(b.get_obj_id()==0){
		cerr << "using special hash object is invalid, use a different hash function\n";
		exit(-1);
	}

	auto obj_id = b.get_obj_id();
	auto fid = b.get_fid();

	/*
		 if(func_count.count(b.get_obj_id())!=0){
		 fprintf(stderr,"conflicting hashes, use a different hash function %x!\n",b.get_obj_id());
		 exit(-1);
		 }
	 */
	//fprintf(stderr,"new func_count: (%d) -> (%d)\n",b.get_obj_id(),b.get_fid());
	func_count[obj_id] = fid;
	ucfg[obj_id].resize(fid, std::vector< std::vector< bb_t> >());
	ucfg[obj_id].shrink_to_fit();
	bb_count[obj_id].resize(fid);
	bb_count[obj_id].shrink_to_fit();
	bb_size[obj_id].resize(fid);
	bb_size[obj_id].shrink_to_fit();
	cs_size[obj_id].resize(fid);
	cs_size[obj_id].shrink_to_fit();
	//fall_thrus[b.get_obj_id()].resize(b.get_fid(),std::vector< boost::container::flat_map<bb_t,uint32_t>>());
	bb_cfg[obj_id].resize(fid,std::vector<std::vector<std::pair<func_t,int>>>());
	bb_cfg[obj_id].shrink_to_fit();
	bb_return[obj_id].resize(fid,std::vector<bb_t>());
	bb_return[obj_id].shrink_to_fit();
	//bb_exec_count[b.get_obj_id()].resize(b.get_fid(),std::vector<uint32_t>());
	//joint_freqs[b.get_obj_id()].resize(b.get_fid());
	//cerr << "after set: " << fall_thrus[b.get_obj_id()].size() << "\n";
	return 1;
}



extern "C" void start_bb_call_site_tracing() {
	save_affinity_environment_variables();  
	//std::cerr << doProfiling << "\n";
	if(!do_profile)
		return;

	initialize_affinity_data();


	/* Set up the atexit handler. */
	atexit (affinityAtExitHandler);

	//pthread_create(&prof_switch_th,NULL,prof_switch_toggle, (void *) 0);
}



extern "C" void record_callsite(uint64_t arg){
	if(!do_profile)
		return;

	if(++prof_time & 0xF800){
		bb_profiler.pred_valid = false;
		return;
	}


	if(bb_profiler.destroying)
		return;

	if(bb_profiler.locked)
		return;

	bb_profiler.locked = true;
	BBlock b(arg);
	if(bb_profiler.initializeMap(b.get_obj_id(),b.get_fid())){
		bb_profiler.pred = b;
		bb_profiler.pred_valid = true;
	}else
		bb_profiler.pred_valid = false;
	bb_profiler.locked = false;

}

extern "C" void record_bb_entry(uint64_t arg){
	if(!do_profile)
		return;

	if(++prof_time & 0xF800){
		bb_profiler.pred_valid = false;
		return;
	}

	if(bb_profiler.destroying)
		return;

	if(bb_profiler.locked)
		return;

	bb_profiler.locked = true;


	BBlock b(arg);
	auto obj_id = b.get_obj_id();
	auto fid = b.get_fid();
	auto bbid = b.get_bbid();


	if(bb_profiler.initializeMap(obj_id,fid)){
		bb_profiler.incExecCount(obj_id, fid, bbid); 
		bb_profiler.incFallThru(obj_id,fid,bbid);
		bb_profiler.pred = b;
		bb_profiler.pred_valid = true;
		if(do_affinity)
			bb_profiler.record_bb_exec(b);
	}else
		bb_profiler.pred_valid = false;

	bb_profiler.locked = false;

}



extern "C" void  record_func_entry(uint64_t arg){
	if(!do_profile)
		return;

	if(++prof_time & 0xF800){
		bb_profiler.pred_valid = false;
		return;
	}


	if(bb_profiler.destroying)
		return;

	if(bb_profiler.locked)
		return;



	bb_profiler.locked = true;

	BBlock b(arg);
	auto obj_id = b.get_obj_id();


	auto fid = b.get_fid();
	auto bbid = b.get_bbid();

	if(bb_profiler.initializeMap(obj_id,fid)){
		bb_profiler.incExecCount(obj_id, fid, bbid); 
		bb_profiler.incCallCount(obj_id,fid);
		bb_profiler.pred = b;
		bb_profiler.pred_valid = true;
		if(do_affinity)
			bb_profiler.record_bb_exec(b);
	}else
		bb_profiler.pred_valid = false;
	bb_profiler.locked = false;

}

bool initialized_first = false;

extern "C" void register_object(const char * obj_str, func_t fid, short exer){
	hash_t obj_id = (hash_t)std::hash<std::string>()(std::string(obj_str));
	//if(exer==1 || true){
	if(exer==1){
		const char * _basename = basename((char*)obj_str);
		affinity_basename = new char [strlen(_basename)+1];
		strcpy(affinity_basename,_basename);
		//affinity_basename = std::string(basename((char *)obj_str));
		fprintf(stderr,"basename is %s\n",affinity_basename);
		//std::cerr << "affinity basename: " << affinity_basename << "\n";
	}
	if(!initialized_first){
		initialized_first = true;
		func_count = new func_t[HASH_MAX]();
		object_hash_map = new std::string[HASH_MAX];
		func_names = new FuncNameMap[HASH_MAX];
		bb_count = new BBCountMap[HASH_MAX];
		bb_size = new BBSizeMap[HASH_MAX];
		cs_size = new CSSizeMap[HASH_MAX];
		ucfg = new UCFGMap[HASH_MAX];
		bb_cfg = new BBCFGMap[HASH_MAX];
		bb_return = new BBReturnMap[HASH_MAX];
		read_metadata = new bool[HASH_MAX]();


	}

	fprintf(stderr, "object is: %s\t hash: %u\n",obj_str,obj_id);
	//assert((object_hash_map[obj_id].compare("")==0 || object_hash_map[obj_id].compare(obj_str)==0) && "hash already exists fro another DSO\n");
	object_hash_map[obj_id]=std::string(obj_str);
	//std::cerr << "exe : " << exer << "\n";
	//assert(object_hash_map.count(obj_id)==0 && "hash already exists");
	func_count[obj_id] = fid;
	func_names[obj_id].resize(fid);
	func_names[obj_id].shrink_to_fit();
	ucfg[obj_id].resize(fid);
	ucfg[obj_id].shrink_to_fit();
	bb_count[obj_id].resize(fid);
	bb_count[obj_id].shrink_to_fit();
	bb_size[obj_id].resize(fid);
	bb_size[obj_id].shrink_to_fit();
	cs_size[obj_id].resize(fid);
	cs_size[obj_id].shrink_to_fit();
	bb_cfg[obj_id].resize(fid,std::vector<std::vector<std::pair<func_t,int>>>());
	bb_cfg[obj_id].shrink_to_fit();
	bb_return[obj_id].resize(fid,std::vector<bb_t>());
	bb_return[obj_id].shrink_to_fit();

}





extern "C" void set_bb_size(uint64_t arg, unsigned _size){
	BBlock b(arg);
	//cerr << b << "\n";
	//cerr << "Basic Block: " << b << "\tsize\t" << _size << "\n";
	bb_size[b.get_obj_id()].at(b.get_fid()).at(b.get_bbid()) = _size;
}

extern "C" void set_cs_size(uint64_t arg, unsigned _size){
	BBlock b(arg);
	//cerr << b << "\n";
	//cerr << "Basic Block: " << b << "\tsize\t" << _size << "\n";
	cs_size[b.get_obj_id()].at(b.get_fid()).at(b.get_bbid()).push_back(_size);
}



extern "C" void set_func_name(const char * obj_str, func_t fn_number, const char * func_str){
	//cerr << arg << "\t" << _size << "\n";
	//cerr << obj_str << "\t" << fn_number << "\t" << func_str << "\n";
	std::string _func_str(func_str);
	std::string _obj_str(obj_str);
	hash_t obj_str_hash = (hash_t)std::hash<std::string>()(obj_str);
	func_names[obj_str_hash][fn_number]= _func_str;
}
extern "C" void set_bb_call_entry(uint64_t caller, uint64_t callee){
	BBlock caller_bb(caller);
	BBlock callee_bb(callee);
	func_t callee_fid = callee_bb.get_fid();
	func_t caller_fid = caller_bb.get_fid();
	bb_t caller_bbid = caller_bb.get_bbid();
	hash_t obj_id = caller_bb.get_obj_id();
	bb_t isTail = callee_bb.get_bbid();

	int callee_bbid = -1;
	//std::cerr << "for call: " << b << "\t returns : " << bb_return[b.get_obj_id()][fid].size() << "\n";
	if(bb_return[obj_id][callee_fid].size()==1 && isTail==0)
		callee_bbid = bb_return[obj_id][callee_fid].at(0);

	bb_cfg[obj_id][caller_fid][caller_bbid].push_back(std::make_pair((func_t)callee_fid,callee_bbid));
}

extern "C" void set_bb_return_entry(uint64_t arg){
	BBlock b(arg);
	//std::cerr << "return: " << b << "\n";
	bb_return[b.get_obj_id()][b.get_fid()].push_back(b.get_bbid());
}



extern "C" void accquire_mutex(uint16_t obj_id){ 
	pthread_mutex_lock(&flush_mutex);
}

extern "C" void release_mutex(uint16_t obj_id){ 
	for(func_t fid = 0; fid < func_count[obj_id]; ++fid)
		for(bb_t bbid = 0; bbid < bb_count[obj_id].at(fid); ++bbid)
			ucfg[obj_id].at(fid).at(bbid).shrink_to_fit();
	read_metadata[obj_id] = true;
	pthread_mutex_unlock(&flush_mutex);
}
extern "C" void set_bb_count_for_fid(uint64_t arg){
	//cerr << "set bb count for fid: " << arg << "\n";
	BBlock b(arg);
	auto obj_id = b.get_obj_id();
	auto fid = b.get_fid();
	auto bbid = b.get_bbid();
	bb_count[obj_id].at(fid)=bbid;
	bb_size[obj_id].at(fid).resize(bbid);
	bb_size[obj_id].at(fid).shrink_to_fit();

	cs_size[obj_id].at(fid).resize(bbid);
	cs_size[obj_id].at(fid).shrink_to_fit();
	//fall_thrus[b.get_obj_id()].at(b.get_fid()).resize(b.get_bbid(),boost::container::flat_map<bb_t,uint32_t>());
	ucfg[obj_id].at(fid).resize(bbid,std::vector< bb_t>());
	ucfg[obj_id].at(fid).shrink_to_fit();
	bb_cfg[obj_id].at(fid).resize(bbid,std::vector<std::pair<func_t,int>>());
	bb_cfg[obj_id].at(fid).shrink_to_fit();
	//bb_exec_count[b.get_obj_id()].at(b.get_fid()).resize(b.get_bbid(),0);
}

extern "C" void set_target_for_bb(uint64_t arg, bb_t target_bbid){
	BBlock b(arg);
	auto obj_id = b.get_obj_id();
	auto fid = b.get_fid();
	auto bbid = b.get_bbid();
	ucfg[obj_id].at(fid).at(bbid).push_back(target_bbid);
}

extern "C" void initialize_post_bb_count_data(hash_t obj_id){
	/*
		 for(func_t fid=0; fid < func_count[obj_id]; ++fid){
		 bb_count_cum[obj_id][fid+1]=bb_count[obj_id][fid]+bb_count_cum[obj_id][fid];
		 }
	 */
}
extern "C" void set_bb_cs_size(uint64_t arg, unsigned _size){
	BBlock b(arg);
	//cerr << b << "\n";
	//cerr << "Basic Block: " << b << "\tsize\t" << _size << "\n";
	bb_size[b.get_obj_id()].at(b.get_fid()).at(b.get_bbid()) = _size;
}


