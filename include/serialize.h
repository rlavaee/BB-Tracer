/** 
 * Copyright (c) 2013, Simone Pellegrini All rights oserved.
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

#include <vector>
#include <unordered_map>
#include <map>
#include <string>
#include <tuple>
#include <numeric>
#include <iostream>

#include <cassert>
#include <boost/container/flat_map.hpp>
#include <boost/unordered_map.hpp>
#include "block.h"

//typedef std::basic_istream<uint8_t> InputStream;
//typedef std::basic_ostream<uint8_t> OutputStream;

typedef std::istream InputStream;
typedef std::ostream OutputStream;

template <class T>
inline void serialize(const T&, OutputStream&);

namespace detail {

	template<std::size_t> struct int_{};

} // end detail namespace 

// get_size 
template <class T>
size_t get_size(const T& obj);

namespace detail {

	template <class T>
	struct get_size_helper; 

	template <class T>
	struct get_size_helper<std::vector<T>> {
		static size_t value(const std::vector<T>& obj) {
		return std::accumulate(obj.begin(), obj.end(), sizeof(size_t), 
				[](const size_t& acc, const T& cur) { return acc+get_size(cur); });
		}
	};

	template <>
	struct get_size_helper<std::string> {
		static size_t value(const std::string& obj) {
			return sizeof(size_t) + obj.length()*sizeof(char);
		}
	};

	template <class tuple_type>
	inline size_t get_tuple_size(const tuple_type& obj, int_<0>) {

		constexpr size_t idx = std::tuple_size<tuple_type>::value-1;
		return get_size(std::get<idx>(obj));
	}

	template <class tuple_type, size_t pos>
	inline size_t get_tuple_size(const tuple_type& obj, int_<pos>) {
		
		constexpr size_t idx = std::tuple_size<tuple_type>::value-pos-1;
		size_t acc = get_size(std::get<idx>(obj));

		// recur
		return acc+get_tuple_size(obj, int_<pos-1>());
	}

	template <class ...T>
	struct get_size_helper<std::tuple<T...>> {

		static size_t value(const std::tuple<T...>& obj) {
			return get_tuple_size(obj, int_<sizeof...(T)-1>());
		}

	};

	template <class T>
	struct get_size_helper {
		static size_t value(const T& obj) { return sizeof(T); }
	};

} // end detail namespace 

template <class T>
inline size_t get_size(const T& obj) { 
	return detail::get_size_helper<T>::value(obj); 
}

namespace detail {

	template <class T>
	struct serialize_helper;

	template <class T>
	void serializer(const T& obj, OutputStream& os);

	template <class tuple_type>
	inline void serialize_tuple(const tuple_type& obj, OutputStream& os, int_<0>) {

		constexpr size_t idx = std::tuple_size<tuple_type>::value-1;
		serializer(std::get<idx>(obj), os);

	}

	template <class tuple_type, size_t pos>
	inline void serialize_tuple(const tuple_type& obj, OutputStream& os, int_<pos>) {
		
		constexpr size_t idx = std::tuple_size<tuple_type>::value-pos-1;
		serializer(std::get<idx>(obj), os);

		// recur
		serialize_tuple(obj, os, int_<pos-1>());

	}

	template <class... T>
	struct serialize_helper<std::tuple<T...>> {
		
		static void apply(const std::tuple<T...>& obj, OutputStream& os) {
			detail::serialize_tuple(obj, os, detail::int_<sizeof...(T)-1>());
		}

	};

	template <>
	struct serialize_helper<std::string> {

		static void apply(const std::string& obj, OutputStream& os) {
			// store the number of elements of this vector at the beginning
			serializer(obj.length(), os);
			for(const auto& cur : obj) { serializer(cur, os); }
		}

	};

	template <class T>
	struct serialize_helper<std::vector<T>> {

		static void apply(const std::vector<T>& obj, OutputStream& os) {
			// store the number of elements of this vector at the beginning
			serializer(obj.size(), os);
			//std::cerr << "size is: " << obj.size() << "\n";
			for(const auto& cur : obj) { serializer(cur, os); }
		}

	};


	template <class S, class T>
	struct serialize_helper< std::pair<S,T>> {
		static void apply(const std::pair<S,T>& obj, OutputStream& os) {
				serializer(obj.first,os);
				serializer(obj.second,os);
		}
	};

	template <
			class K,
			class V,
			class Hash,
			class EQ,
			class Alloc
	>
	struct serialize_helper<std::unordered_map<K,V,Hash,EQ,Alloc>> {
		static void  apply(const std::unordered_map<K,V,Hash,EQ,Alloc>& obj, OutputStream& os) {
      //std::cerr << "serializiation: size is " << obj.size() << "\n";
			serializer(obj.size(),os);
			for(const auto& cur : obj) {
				serializer(cur.first, os); 
				serializer(cur.second, os);
			}
		}
	};

	template <
			class K,
			class V,
			class Cmp,
			class Alloc
	>
	struct serialize_helper<boost::container::flat_map<K,V,Cmp,Alloc>> {
		static void  apply(const boost::container::flat_map<K,V,Cmp,Alloc>& obj, OutputStream& os) {
			serializer(obj.size(),os);
			for(const auto& cur : obj) {
				//fprintf(stderr,"from serialize: %p\n",static_cast<const char *>(cur.first));
				serializer(cur.first, os); 
				serializer(cur.second, os);
			}
		}
	};

  template <
			class K,
			class V,
			class Cmp,
			class Alloc
	>
	struct serialize_helper<std::map<K,V,Cmp,Alloc>> {
		static void  apply(const std::map<K,V,Cmp,Alloc>& obj, OutputStream& os) {
			serializer(obj.size(),os);
			for(const auto& cur : obj) {
				//fprintf(stderr,"from serialize: %p\n",static_cast<const char *>(cur.first));
				serializer(cur.first, os); 
				serializer(cur.second, os);
			}
		}
	};


	template <class T>
	struct serialize_helper {
		
		static void apply(const T& obj, OutputStream& os) {
			//fprintf(stderr,"size of: %d\n",sizeof(T));
			const char* ptr = reinterpret_cast<const char*>(&obj);
			os.write(ptr,sizeof(T));
		}

	};

	template <class T>
	inline void serializer(const T& obj, OutputStream& os) {	
		serialize_helper<T>::apply(obj,os);

	}

} // end detail namespace 

template <class T>
inline void serialize(const T& obj, OutputStream& os) {

	detail::serializer(obj,os);
}

namespace detail {

	template <class T>
	struct deserialize_helper;

	template <class T>
	struct deserialize_helper {
		
		static T apply(InputStream& is) {
			T val;
			char* ptr = reinterpret_cast<char*>(&val);
			is.read(ptr,sizeof(T));
			return val;
		}
	};


	
	template <class S,class T>
	struct deserialize_helper<std::pair<S,T>> {
	
		static std::pair<S,T> apply(InputStream& is)
		{
			S first = deserialize_helper<S>::apply(is);
			T second = deserialize_helper<T>::apply(is);

			return std::pair<S,T>(std::move(first),std::move(second));
		}
	};


	template <class T>
	struct deserialize_helper<std::vector<T>> {
	
		static std::vector<T> apply(InputStream& is)
		{
			// retrieve the number of elements 
			size_t size = deserialize_helper<size_t>::apply(is);
			//std::cout << "size is: " << size << "\n";

			std::vector<T> vect(size);
			for(size_t i=0; i<size; ++i) {
				vect[i] = std::move(deserialize_helper<T>::apply(is));
			}
			return vect;
		}
	};

	template <
			class K,
			class V,
			class Hash,
			class EQ,
			class Alloc
	>
	struct deserialize_helper<std::unordered_map<K,V,Hash,EQ,Alloc>> {
		static std::unordered_map<K,V,Hash,EQ,Alloc>  apply( InputStream& is)
		{
			// retrieve the number of elements 
			size_t size = deserialize_helper<size_t>::apply(is);

			std::unordered_map<K,V,Hash,EQ,Alloc> umap (size);
			for(size_t i=0; i<size; ++i){
				K key = deserialize_helper<K>::apply(is);
				V value = deserialize_helper<V>::apply(is);
				umap.insert(std::move(std::pair<K,V>(std::move(key),std::move(value))));
			}
			return umap;
		}
	};

	template <
			class K,
			class V,
			class Cmp,
			class Alloc
	>
	struct deserialize_helper<boost::container::flat_map<K,V,Cmp,Alloc>> {
		static boost::container::flat_map<K,V,Cmp,Alloc>  apply( InputStream& is)
		{
			// retrieve the number of elements 
			size_t size = deserialize_helper<size_t>::apply(is);

			boost::container::flat_map<K,V,Cmp,Alloc> fmap;
			for(size_t i=0; i<size; ++i){
				K key = deserialize_helper<K>::apply(is);
				V value = deserialize_helper<V>::apply(is);
				fmap.insert(std::move(std::pair<K,V>(std::move(key),std::move(value))));
			}
			return fmap;
		}
	};

  template <
			class K,
			class V,
			class Cmp,
			class Alloc
	>
	struct deserialize_helper<std::map<K,V,Cmp,Alloc>> {
		static std::map<K,V,Cmp,Alloc>  apply( InputStream& is)
		{
			// retrieve the number of elements 
			size_t size = deserialize_helper<size_t>::apply(is);

			std::map<K,V,Cmp,Alloc> omap;
			for(size_t i=0; i<size; ++i){
				K key = deserialize_helper<K>::apply(is);
				V value = deserialize_helper<V>::apply(is);
				omap.insert(std::move(std::pair<K,V>(std::move(key),std::move(value))));
			}
			return omap;
		}
	};




	template <>
	struct deserialize_helper<std::string> {
	
		static std::string apply(InputStream& is)
		{
			// retrieve the number of elements 
			size_t size = deserialize_helper<size_t>::apply(is);

			if (size == 0u) return std::string();
		
			std::string str(size,'\0');
			for(size_t i=0; i<size; ++i) {
				str.at(i) = deserialize_helper<char>::apply(is);
			}
			return str;
		}
	};

	template <class tuple_type>
	inline void deserialize_tuple(tuple_type& obj, 
								  InputStream& is, int_<0>) {

		constexpr size_t idx = std::tuple_size<tuple_type>::value-1;
		typedef typename std::tuple_element<idx,tuple_type>::type T;

		std::get<idx>(obj) = std::move(deserialize_helper<T>::apply(is));
	}

	template <class tuple_type, size_t pos>
	inline void deserialize_tuple(tuple_type& obj, 
								  InputStream& is, int_<pos>) {
		
		constexpr size_t idx = std::tuple_size<tuple_type>::value-pos-1;
		typedef typename std::tuple_element<idx,tuple_type>::type T;

		std::get<idx>(obj) = std::move(deserialize_helper<T>::apply(is));

		// recur
		deserialize_tuple(obj, is, int_<pos-1>());
	}

	template <class... T>
	struct deserialize_helper<std::tuple<T...>> {
	
		static std::tuple<T...> apply(InputStream& is)
		{
			//return std::make_tuple(deserialize(begin,begin+sizeof(T),T())...);
			std::tuple<T...> ret;
			deserialize_tuple(ret, is, int_<sizeof...(T)-1>());
			return ret;
		}

	};

	

} // end detail namespace 

template <class T>
inline T deserialize(InputStream& is) {

	return detail::deserialize_helper<T>::apply(is);
}
