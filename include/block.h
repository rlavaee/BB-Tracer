#pragma once
#include <stdint.h>
#include <fstream>

union BBlock{
  uint64_t bits; /* obj_id: 16, fid: 32, bbid: 16 */

    
    bool operator <(const BBlock& rhs) const
    {
      return this->bits < rhs.bits;
    };

  bool operator ==(const BBlock& rhs) const
    {
      return this->bits == rhs.bits;
    };

   bool operator !=(const BBlock& rhs) const
    {
      return this->bits != rhs.bits;
    };



  uint16_t get_obj_id() const {return bits >> 48;}
  uint32_t get_fid() const {return (bits >> 16) & 0xFFFFFFFF;}
  uint16_t get_bbid() const {return bits & 0xFFFF;}

   BBlock(){};

   BBlock(unsigned o, unsigned f, unsigned b){
        bits = ((((uint64_t) o) & 0xFFFF) << 48) | (((uint64_t) f) << 16) | (b & 0xFFFF);
  }

  BBlock(const BBlock &b): bits(b.bits){}

   BBlock(uint64_t _bits): bits(_bits) {}

   bool equal_function(const BBlock &rec) const {
     return  ((this->bits ^ rec.bits) >> 16) == 0;
   }
   
   bool equal_object(const BBlock &rec) const {
    return ((this->bits ^ rec.bits) >> 48) == 0;
   }

   inline friend std::ostream& operator << (std::ostream &out, const BBlock &rec) {
	   out << "[ " << rec.get_obj_id() << " : (" << rec.get_fid() << "," << rec.get_bbid() << ") ]";
	   return out;
   }
};

typedef std::pair<BBlock,BBlock> BBlockPair;

typedef std::pair<uint16_t,uint16_t> LBBlockPair;


union FBlock{
  uint32_t bits; /* obj_id: 12, fid: 20 */

  FBlock(){};

   FBlock(unsigned o, unsigned f){
        bits = ((o & 0xFFF) << 20) | (f & 0xFFFFF);
   }

    bool operator <(const FBlock& rhs) const
    {
      return this->bits < rhs.bits;
    };

  bool operator ==(const FBlock& rhs) const
    {
      return this->bits == rhs.bits;
    };



  uint16_t get_obj_id() const {return (bits >> 20);}
  uint32_t get_fid() const {return (bits & 0xFFFFF);}

  FBlock(const BBlock &b): bits(b.bits) {}

   FBlock(uint32_t _bits): bits(_bits) {}

   bool equal_object(const FBlock &rec) const {
       return (get_obj_id() == rec.get_obj_id());
   }

   inline friend std::ostream& operator << (std::ostream &out, const FBlock &rec) {
	   out << "[ " << rec.get_obj_id() << " : " << rec.get_fid() << " ]";
	   return out;
   }

};

typedef std::pair<FBlock,FBlock> FBlockPair;

namespace std {
 template<> struct hash<FBlock> {
     size_t operator()(const FBlock &rec) const {
        return hash<uint32_t>()(rec.bits);
     }
 };
 

 template<> struct hash<BBlock> {
     size_t operator()(const BBlock &rec) const {
        return hash<uint64_t>()(rec.bits);
     }
 };

 template<> struct hash<BBlockPair>{
    size_t operator()(const BBlockPair &x) const 
      {
        return std::hash<BBlock>()(x.first) ^ std::hash<BBlock>()(x.second);
      }
 };
 
 template<> struct hash<FBlockPair>{
    size_t operator()(const FBlockPair &x) const 
      {
        return std::hash<FBlock>()(x.first) ^ std::hash<FBlock>()(x.second);
      }
 };


}


template<typename T, typename U> std::ostream& operator << (std::ostream &out, const std::pair<T,U> &rec) {
	   out << "< " << rec.first << " , " << rec.second << " >";
	   return out;
   }

/*
struct pairhash {
public:
  template <typename T, typename U>
   std::size_t operator()(const std::pair<T, U> &x) const
    {
      return std::hash<T>()(x.first) ^ std::hash<U>()(x.second);
    }
};
*/


