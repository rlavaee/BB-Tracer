#pragma once
#include <unordered_map>
#include <vector>
#include <map>
#include <boost/container/flat_map.hpp>
#include <tuple>
#include "block.h"

#define HASH_MAX 1<<16

typedef uint32_t func_t;
typedef uint16_t bb_t;
typedef uint16_t hash_t;
typedef int16_t wsize_t;
typedef uint64_t num_t;
typedef std::pair<wsize_t,wsize_t> wsize_interval;

#ifdef THREAD_TRACING
typedef std::unordered_map<hash_t, func_t> FuncCountMap;
typedef std::unordered_map <hash_t, std::string> ObjectStrMap;
typedef std::unordered_map <std::string, std::unordered_map<func_t,std::string>> FuncNameMap;
typedef std::unordered_map<hash_t, std::vector<bb_t>> BBCountMap;
typedef std::unordered_map<hash_t, std::vector<std::vector<unsigned>>> BBSizeMap;
typedef std::unordered_map<hash_t, std::vector<std::vector<std::vector<unsigned>>>> CSSizeMap;
typedef std::unordered_map <hash_t, std::vector< std::vector< std::vector<bb_t>>>> UCFGMap;
typedef std::unordered_map <hash_t, std::vector< std::vector< std::vector<std::pair<func_t,int>>>>> BBCFGMap;
typedef std::unordered_map <hash_t, std::vector< std::vector<bb_t>>> BBReturnMap;
typedef std::unordered_map <hash_t, std::vector< std::vector< std::vector< std::pair<BBlock,num_t>>>>> BBReturnCallMap;

typedef std::unordered_map <BBlock, std::map<BBlock, std::vector<num_t>>> JointFreqMap;
typedef std::unordered_map <BBlock, std::vector<num_t>> SingleFreqMap;
typedef std::unordered_map <hash_t, std::vector < std::vector< boost::container::flat_map<bb_t,num_t>>>> FallThroughMap;
typedef std::unordered_map <hash_t, std::vector < std::vector< boost::container::flat_map<func_t,num_t>>>> CallCountMap;
typedef std::map <BBlock, std::map<BBlock, boost::container::flat_map<wsize_interval,num_t>>> JointFreqIntervalMap;
typedef std::unordered_map <hash_t, std::vector < std::vector < num_t >>> BBExecCountMap;

typedef std::unordered_map <hash_t, std::vector< std::vector<std::pair<num_t,std::map<func_t, num_t>>>>> SimpleJointFreqMap;
typedef std::unordered_map <hash_t, std::vector< std::pair<num_t,std::map<BBlock, num_t>>>> VerySimpleJointFreqMap;

#else
typedef std::string ObjectStrMap;
typedef std::vector<std::string> FuncNameMap;
typedef std::vector<bb_t> BBCountMap;
typedef std::vector<std::vector<unsigned>> BBSizeMap;
typedef std::vector<std::vector<std::vector<unsigned>>> CSSizeMap;
typedef std::vector< std::vector< std::vector<bb_t>>> UCFGMap;
typedef std::vector< std::vector< std::vector<std::pair<func_t,int>>>> BBCFGMap;
typedef std::vector< std::vector<bb_t>> BBReturnMap;
typedef std::vector< std::vector< std::vector< std::pair<BBlock,num_t>>>> BBReturnCallMap;

typedef std::unordered_map <BBlock, std::map<BBlock, std::vector<num_t>>> JointFreqMap;
typedef std::unordered_map <BBlock, std::vector<num_t>> SingleFreqMap;
typedef std::vector < std::vector< boost::container::flat_map<bb_t,num_t>>> FallThroughMap;
typedef std::vector < std::vector< boost::container::flat_map<func_t,num_t>>> CallCountMap;
typedef std::map <BBlock, std::map<BBlock, boost::container::flat_map<wsize_interval,num_t>>> JointFreqIntervalMap;
typedef std::unordered_map <std::string, std::vector< std::tuple<std::string,bb_t,bool,bb_t,num_t>>>  BBLayoutMap;
typedef std::unordered_map <std::string, std::vector< std::string>>  FuncLayoutMap;
typedef std::vector < std::vector < num_t >> BBExecCountMap;

typedef std::vector< std::vector<std::pair<num_t,std::map<func_t, num_t>>>> SimpleJointFreqMap;
typedef  std::vector< std::pair<num_t,std::map<BBlock, num_t>>> VerySimpleJointFreqMap;

typedef std::vector < std::tuple<func_t, bb_t, num_t>> BestTailCallerMap;

#endif

typedef std::unordered_map <std::string, std::vector< std::tuple<std::string,bb_t,bool,bb_t,num_t>>>  BBLayoutMap;
typedef std::unordered_map <std::string, std::vector< std::string>>  FuncLayoutMap;


//template <typename T>
//	using Map = std::unordered_map<hash_t, T>;

template <typename T>
	using Vector = std::vector<T>;
	//using Vector = std::array<T,HASH_MAX>;

template <typename T>
	using Map = std::unordered_map<hash_t,T>;
