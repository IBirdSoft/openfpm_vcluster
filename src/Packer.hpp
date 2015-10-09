/*
 * Packer_cls.hpp
 *
 *  Created on: Jul 15, 2015
 *      Author: i-bird
 */

#ifndef SRC_PACKER_HPP_
#define SRC_PACKER_HPP_

#include "util/object_util.hpp"
#include "Grid/util.hpp"
#include "Vector/util.hpp"
#include "memory/ExtPreAlloc.hpp"
#include "util/util_debug.hpp"
#include "Pack_stat.hpp"
#include "Pack_selector.hpp"
#include "Vector/map_vector.hpp"

/*! \brief Packing class
 *
 * This class pack objects primitives vectors and grids, the general usage is to create a vector of
 * packing request (std::vector<size_t>) that contain the size of the required space needed to pack
 * the information. Calculate the total size, allocating it on HeapMemory (for example), Create an
 * ExtPreAlloc memory object giving the preallocated memory to it and finally Pack all the objects
 * subsequently
 *
 * In order to unpack the information the Unpacker class can be used
 *
 * \see Unpacker
 *
 * \snippet Packer_unit_tests.hpp Pack into a message primitives objects vectors and grids
 *
 * \tparam T object type to pack
 * \tparam Mem Memory origin HeapMemory CudaMemory ...
 * \tparam Implementation of the packer (the Pack_selector choose the correct one)
 *
 */
template<typename T, typename Mem, int pack_type=Pack_selector<T>::value >
class Packer
{
public:

	/*! \brief Error, no implementation
	 *
	 */
	static void pack(ExtPreAlloc<Mem> , const T & obj)
	{
		std::cerr << "Error: " << __FILE__ << ":" << __LINE__ << " packing for the type " << demangle(typeid(T).name()) << " is not implemented\n";
	}

	/*! \brief Error, no implementation
	 *
	 */
	static size_t packRequest(std::vector<size_t> & req)
	{
		std::cerr << "Error: " << __FILE__ << ":" << __LINE__ << " packing for the type " << demangle(typeid(T).name()) << " is not implemented\n";
		return 0;
	}
};

/*! \brief Packer for primitives
 *
 * \tparam T object type to pack
 * \tparam Mem Memory origin HeapMemory CudaMemory ...
 *
 */
template<typename T, typename Mem>
class Packer<T,Mem,PACKER_PRIMITIVE>
{
public:

	/*! \brief It pack any C++ primitives
	 *
	 * \param ext preallocated memory where to pack the object
	 * \param obj object to pack
	 * \param sts pack-stat info
	 *
	 */
	inline static void pack(ExtPreAlloc<Mem> & ext, const T & obj, Pack_stat & sts)
	{
#ifdef DEBUG
		if (ext.ref() == 0)
			std::cerr << "Error : " << __FILE__ << ":" << __LINE__ << " the reference counter of mem should never be zero when packing \n";
#endif

		ext.allocate(sizeof(T));
		*(T *)ext.getPointer() = obj;

		// update statistic
		sts.incReq();
	}

	/*! \brief It add a request to pack a C++ primitive
	 *
	 * \param req requests vector
	 *
	 */
	static void packRequest(std::vector<size_t> & req)
	{
		req.push_back(sizeof(T));
	}
};


/*! \brief Packer for objects, with impossibility to check for internal pointers
 *
 * \tparam T object type to pack
 * \tparam Mem Memory origin HeapMemory CudaMemory ...
 *
 */
template<typename T, typename Mem>
class Packer<T,Mem,PACKER_OBJECTS_WITH_WARNING_POINTERS>
{
public:

	/*! \brief It pack an object
	 *
	 * \param ext preallocated memory where to pack the objects
	 * \param obj object to pack
	 * \param sts pack-stat info
	 *
	 */
	static void pack(ExtPreAlloc<Mem> & ext, T & obj, Pack_stat & sts)
	{
#ifdef DEBUG
		if (ext.ref() == 0)
			std::cerr << "Error : " << __FILE__ << ":" << __LINE__ << " the reference counter of mem should never be zero when packing \n";

		std::cerr << "Warning: " << __FILE__ << ":" << __LINE__ << " impossible to check the type " << demangle(typeid(T).name()) << " please consider to add a static method \"void noPointers()\" \n" ;
#endif
		ext.allocate(sizeof(T));
		memcpy((T *)ext.getPointer(),&obj,sizeof(T));

		// update statistic
		sts.incReq();
	}

	/*! \brief it add a request to pack an object
	 *
	 * \param req requests vector
	 *
	 */
	static void packRequest(std::vector<size_t> & req)
	{
		req.push_back(sizeof(T));
	}
};

/*! \brief Packer class for objects
 *
 * \tparam T object type to pack
 * \tparam Mem Memory origin HeapMemory CudaMemory ...
 *
 */
template<typename T, typename Mem>
class Packer<T,Mem,PACKER_OBJECTS_WITH_POINTER_CHECK>
{
public:

	/*! \brief It pack any object checking that the object does not have pointers inside
	 *
	 * \param ext preallocated memory where to pack the objects
	 * \param obj object to pack
	 * \param sts pack-stat info
	 *
	 */
	static void pack(ExtPreAlloc<Mem> & ext, T & obj, Pack_stat & sts)
	{
#ifdef DEBUG
		if (ext.ref() == 0)
			std::cerr << "Error : " << __FILE__ << ":" << __LINE__ << " the reference counter of mem should never be zero when packing \n";

		if (obj.noPointers() == false)
			std::cerr << "Error: " << __FILE__ << ":" << __LINE__ << " the type " << demangle(typeid(T).name()) << " has pointers inside, sending pointers values has no sense\n";
#endif
		ext.allocate(sizeof(T));
		memcpy((T *)ext.getPointer(),&obj,sizeof(T));

		// Update statistic
		sts.incReq();
	}

	/*! \brief it add a request to pack an object
	 *
	 * \param req requests vector
	 *
	 */
	static void packRequest(std::vector<size_t> & req)
	{
		req.push_back(sizeof(T));
	}
};

/*! \brief Packer class for vectors
 *
 * \tparam T vector type to pack
 * \tparam Mem Memory origin HeapMemory CudaMemory ...
 *
 */
template<typename T, typename Mem>
class Packer<T,Mem,PACKER_VECTOR>
{
public:

	template<int ... prp> static void packRequest(T & obj, std::vector<size_t> & v)
	{
		obj.packRequest<prp...>(obj, v);
	};

	template<int ... prp> static void pack(ExtPreAlloc<Mem> & mem, T & obj, Pack_stat & sts)
	{
		obj.pack<prp...>(mem, obj, sts);
	};

};

/*! \brief Packer for grids and sub-grids
 *
 * \tparam T grid type to pack
 * \tparam Mem Memory origin HeapMemory CudaMemory ...
 *
 */
template<typename T, typename Mem>
class Packer<T,Mem,PACKER_GRID>
{
public:

	template<int ... prp> static void packRequest(T & obj, std::vector<size_t> & v)
	{
		obj.packRequest<prp...>(v);
	};

	template<int ... prp> static void packRequest(T & obj, grid_key_dx_iterator_sub<T::dims> & sub, std::vector<size_t> & v)
	{
		obj.packRequest<prp...>(sub, v);
	};

	template<int ... prp> static void pack(ExtPreAlloc<Mem> & mem, T & obj, Pack_stat & sts)
	{
		obj.pack<prp...>(mem, sts);
	}

	template<int ... prp> static void pack(ExtPreAlloc<Mem> & mem, T & obj, grid_key_dx_iterator_sub<T::dims> & sub_it, Pack_stat & sts)
	{
		obj.pack<prp...>(mem, sub_it, sts);
	}

};

template<typename T, typename Mem>
class Packer<T,Mem,PACKER_ENCAP_OBJECTS>
{
public:

	/*! \brief
	 *
	 *
	 */
	void pack(ExtPreAlloc<Mem> & mem, T & eobj)
	{
#ifdef DEBUG
		if (mem.ref() == 0)
			std::cerr << "Error : " << __FILE__ << ":" << __LINE__ << " the reference counter of mem should never be zero when packing \n";
#endif

		// Create an object out of the encapsulated object and copy
		typename T::type obj = eobj;

		memcpy(mem.getPointer(),&obj,sizeof(T::type));
	}

	/*! \brief
	 *
	 *
	 */
	void packRequest(std::vector<size_t> & v)
	{
		v.push_back(sizeof(T::type));
	}
};

#endif /* SRC_PACKER_HPP_ */
