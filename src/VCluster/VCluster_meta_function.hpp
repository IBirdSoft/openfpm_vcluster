/*
 * VCluster_meta_function.hpp
 *
 *  Created on: Dec 8, 2016
 *      Author: i-bird
 */

#ifndef OPENFPM_VCLUSTER_SRC_VCLUSTER_VCLUSTER_META_FUNCTION_HPP_
#define OPENFPM_VCLUSTER_SRC_VCLUSTER_VCLUSTER_META_FUNCTION_HPP_

#include "memory/BHeapMemory.hpp"
#include "Packer_Unpacker/has_max_prop.hpp"

/*! \brief Return true is MPI is compiled with CUDA
 *
 * \return true i MPI is compiled with CUDA
 */
static inline bool is_mpi_rdma_cuda_active()
{
#if defined(MPIX_CUDA_AWARE_SUPPORT) && MPIX_CUDA_AWARE_SUPPORT
			return true;
#else
			return false;
#endif
}

template<bool result, typename T, typename S, template<typename> class layout_base, typename Memory>
struct unpack_selector_with_prp
{
	template<typename op,
			 int ... prp>
	static void call_unpack(S & recv,
			                openfpm::vector_fr<BMemory<Memory>> & recv_buf,
							openfpm::vector<size_t> * sz,
							openfpm::vector<size_t> * sz_byte,
							op & op_param,
							size_t opt)
	{
		if (sz_byte != NULL)
			sz_byte->resize(recv_buf.size());

		for (size_t i = 0 ; i < recv_buf.size() ; i++)
		{
			T unp;

			ExtPreAlloc<Memory> & mem = *(new ExtPreAlloc<Memory>(recv_buf.get(i).size(),recv_buf.get(i)));
			mem.incRef();

			Unpack_stat ps;

			Unpacker<T,Memory>::template unpack<>(mem, unp, ps);

			size_t recv_size_old = recv.size();
			// Merge the information

			op_param.template execute<true,T,decltype(recv),decltype(unp),layout_base,prp...>(recv,unp,i,opt);

			size_t recv_size_new = recv.size();

			if (sz_byte != NULL)
				sz_byte->get(i) = recv_buf.get(i).size();
			if (sz != NULL)
				sz->get(i) = recv_size_new - recv_size_old;

			mem.decRef();
			delete &mem;
		}
	}
};

template<typename op, typename Vt, typename S, template<typename> class layout_base, typename v_mpl>
struct unpack_each_prop_buffer
{
	S & recv;

	openfpm::vector_fr<BMemory<HeapMemory>> & recv_buf;

	size_t i;

	op & op_param;

    openfpm::vector<size_t> * sz;

    openfpm::vector<size_t> * sz_byte;

	/*! \brief constructor
	 *
	 * \param v set of pointer buffers to set
	 *
	 */
	inline unpack_each_prop_buffer(S & recv,
			                       openfpm::vector_fr<BMemory<HeapMemory>> & recv_buf,
			                       op & op_param,
			                       size_t i,
			                       openfpm::vector<size_t> * sz,
			                       openfpm::vector<size_t> * sz_byte)
	:recv(recv),recv_buf(recv_buf),op_param(op_param),i(i),sz(sz),sz_byte(sz_byte)
	{};

	//! It call the copy function for each property
	template<typename T>
	inline void operator()(T& t) const
	{
		// here we get the the type of the property at position T::value
		typedef typename boost::mpl::at<typename T::value_type::type,boost::mpl::int_<T::value> >::type prp_type;

		// here we get the the type of the property at position T::value
		typedef typename boost::mpl::at<v_mpl,boost::mpl::int_<T::value>>::type prp_num;

		// calculate the number of received elements
		size_t n_ele = recv_buf.get(i).size() / sizeof(prp_type);

		// add the received particles to the vector
		PtrMemory * ptr1 = new PtrMemory(recv_buf.get(i).getPointer(),recv_buf.get(i).size());

		// create vector representation to a piece of memory already allocated
		openfpm::vector<typename Vt::value_type,PtrMemory,layout_base,openfpm::grow_policy_identity> v2;

		v2.template setMemory<prp_num::value>(*ptr1);

		// resize with the number of elements
		v2.resize(n_ele);

		// Merge the information

		size_t recv_size_old = recv.size();

		op_param.template execute<false,T,decltype(recv),decltype(v2),layout_base,prp_num::value>(recv,v2,i);

		size_t recv_size_new = recv.size();

		if (sz_byte != NULL)
			sz_byte->get(i) = recv_buf.get(i).size();
		if (sz != NULL)
			sz->get(i) = recv_size_new - recv_size_old;
	}
};

/*! \brief this class is a functor for "for_each" algorithm
 *
 * This class is a functor for "for_each" algorithm. For each
 * element of the boost::vector the operator() is called.
 * Is mainly used to process the receive buffers in case of memory_traits_inte layout receive
 *
 * \tparam encap source
 * \tparam encap dst
 *
 */

template<typename sT, template<typename> class layout_base,typename Memory>
struct process_receive_mem_traits_inte
{
	//! set of pointers
	size_t i;

	//! Receive buffer
	openfpm::vector_fr<BMemory<Memory>> & recv_buf;

	//! Fake vector that map over received memory
	openfpm::vector<typename sT::value_type,PtrMemory,layout_base,openfpm::grow_policy_identity> & v2;

	size_t n_ele = 0;

	// options
	size_t opt;

	/*! \brief constructor
	 *
	 * \param v set of pointer buffers to set
	 *
	 */
	inline process_receive_mem_traits_inte(openfpm::vector<typename sT::value_type,PtrMemory,layout_base,openfpm::grow_policy_identity> & v2,
			                               openfpm::vector_fr<BMemory<Memory>> & recv_buf,
			                               size_t i,
			                               size_t opt)
	:i(i),recv_buf(recv_buf),v2(v2),opt(opt)
	{};

	//! It call the copy function for each property
	template<typename T>
	inline void operator()(T& t)
	{
		typedef typename boost::mpl::at<typename sT::value_type::type,T>::type type_prp;

		// calculate the number of received elements
		this->n_ele = recv_buf.get(i).size() / sizeof(type_prp);

		PtrMemory * ptr1;

		if (opt & MPI_GPU_DIRECT)
		{
#if defined(MPIX_CUDA_AWARE_SUPPORT) && MPIX_CUDA_AWARE_SUPPORT
			// add the received particles to the vector
			ptr1 = new PtrMemory(recv_buf.get(i).getDevicePointer(),recv_buf.get(i).size());
#else
			// add the received particles to the vector
			ptr1 = new PtrMemory(recv_buf.get(i).getPointer(),recv_buf.get(i).size());
#endif
		}
		else
		{
			// add the received particles to the vector
			ptr1 = new PtrMemory(recv_buf.get(i).getPointer(),recv_buf.get(i).size());
		}

		v2.template setMemory<T::value>(*ptr1);

		++i;
	}
};

template<bool inte_or_lin,typename T, typename S, template<typename> class layout_base,typename Memory>
struct unpack_selector_with_prp_lin
{
	template<typename op, unsigned int ... prp> static int call_unpack_impl(S & recv,
                                                                             openfpm::vector_fr<BMemory<Memory>> & recv_buf,
                                                                             openfpm::vector<size_t> * sz,
                                                                             openfpm::vector<size_t> * sz_byte,
                                                                             op & op_param,
                                                                             size_t i,
                                                                             size_t opt)
	{
		// create vector representation to a piece of memory already allocated
		openfpm::vector<typename T::value_type,PtrMemory,layout_base,openfpm::grow_policy_identity> v2;

		process_receive_mem_traits_inte<T,layout_base,Memory> prmti(v2,recv_buf,i,opt);

		boost::mpl::for_each_ref<boost::mpl::range_c<int,0,T::value_type::max_prop>>(prmti);

		v2.resize(prmti.n_ele);

		// Merge the information

		size_t recv_size_old = recv.size();

		op_param.template execute<false,T,decltype(recv),decltype(v2),layout_base,prp...>(recv,v2,i,opt);

		size_t recv_size_new = recv.size();

		if (sz_byte != NULL)
			sz_byte->get(i) = recv_buf.get(i).size();
		if (sz != NULL)
			sz->get(i) = recv_size_new - recv_size_old;

		return sizeof...(prp);
	}
};

template<typename T, typename S, template<typename> class layout_base, typename Memory>
struct unpack_selector_with_prp_lin<true,T,S,layout_base,Memory>
{
	template<typename op, unsigned int ... prp> static int call_unpack_impl(S & recv,
                                                                             openfpm::vector_fr<BMemory<Memory>> & recv_buf,
                                                                             openfpm::vector<size_t> * sz,
                                                                             openfpm::vector<size_t> * sz_byte,
                                                                             op & op_param,
                                                                             size_t i,
                                                                             size_t opt)
	{
		// calculate the number of received elements
		size_t n_ele = recv_buf.get(i).size() / sizeof(typename T::value_type);

		// add the received particles to the vector
		PtrMemory * ptr1 = new PtrMemory(recv_buf.get(i).getPointer(),recv_buf.get(i).size());
		ptr1->incRef();

		{
		// create vector representation to a piece of memory already allocated
		openfpm::vector<typename T::value_type,PtrMemory,layout_base,openfpm::grow_policy_identity> v2;

		v2.setMemory(*ptr1);

		// resize with the number of elements
		v2.resize(n_ele);

		// Merge the information

		size_t recv_size_old = recv.size();

		op_param.template execute<false,T,decltype(recv),decltype(v2),layout_base,prp...>(recv,v2,i,opt);

		size_t recv_size_new = recv.size();

		if (sz_byte != NULL)
			sz_byte->get(i) = recv_buf.get(i).size();
		if (sz != NULL)
			sz->get(i) = recv_size_new - recv_size_old;
		}

		ptr1->decRef();
		delete ptr1;
		return 1;
	}
};

typedef aggregate<int,int> dummy_type;

//
template<typename T, typename S, template<typename> class layout_base, typename Memory>
struct unpack_selector_with_prp<true,T,S,layout_base,Memory>
{
	template<typename op, unsigned int ... prp> static void call_unpack(S & recv,
			                                                            openfpm::vector_fr<BMemory<Memory>> & recv_buf,
			                                                            openfpm::vector<size_t> * sz,
			                                                            openfpm::vector<size_t> * sz_byte,
			                                                            op & op_param,
			                                                            size_t opt)
	{
		if (sz_byte != NULL)
			sz_byte->resize(recv_buf.size());

		for (size_t i = 0 ; i < recv_buf.size() ; )
		{
			i += unpack_selector_with_prp_lin<is_layout_mlin<layout_base<dummy_type>>::value,T,S,layout_base,Memory>::template call_unpack_impl<op,prp...>(recv,recv_buf,sz,sz_byte,op_param,i,opt);
		}
	}
};


template<typename T>
struct call_serialize_variadic {};

template<int ... prp>
struct call_serialize_variadic<index_tuple<prp...>>
{
	template<typename T> inline static void call_pr(T & send, size_t & tot_size)
	{
		Packer<T,HeapMemory>::template packRequest<prp...>(send,tot_size);
	}

	template<typename T> inline static void call_pack(ExtPreAlloc<HeapMemory> & mem, T & send, Pack_stat & sts)
	{
		Packer<T,HeapMemory>::template pack<prp...>(mem,send,sts);
	}

	template<typename op, typename T, typename S, template<typename> class layout_base, typename Memory>
	inline static void call_unpack(S & recv,
			                       openfpm::vector_fr<BMemory<Memory>> & recv_buf,
			                       openfpm::vector<size_t> * sz,
			                       openfpm::vector<size_t> * sz_byte,
			                       op & op_param,
			                       size_t opt)
	{
		const bool result = has_pack_gen<typename T::value_type>::value == false && is_vector<T>::value == true;

		unpack_selector_with_prp<result, T, S,layout_base,Memory>::template call_unpack<op,prp...>(recv, recv_buf, sz, sz_byte, op_param,opt);
	}
};

/*! \brief this class is a functor for "for_each" algorithm
 *
 * This class is a functor for "for_each" algorithm. For each
 * element of the boost::vector the operator() is called.
 * Is mainly used to copy one encap into another encap object
 *
 * \tparam encap source
 * \tparam encap dst
 *
 */
template<typename sT>
struct set_buf_pointer_for_each_prop
{
	//! set of pointers
	sT & v;

	openfpm::vector<const void *> & send_buf;

	size_t opt;

	/*! \brief constructor
	 *
	 * \param v set of pointer buffers to set
	 *
	 */
	inline set_buf_pointer_for_each_prop(sT & v, openfpm::vector<const void *> & send_buf, size_t opt)
	:v(v),send_buf(send_buf),opt(opt)
	{};

	//! It call the copy function for each property
	template<typename T>
	inline void operator()(T& t) const
	{
		// If we have GPU direct activated use directly the cuda buffer
		if (opt & MPI_GPU_DIRECT)
		{
#if defined(MPIX_CUDA_AWARE_SUPPORT) && MPIX_CUDA_AWARE_SUPPORT
			send_buf.add(v.template getDeviceBuffer<T::value>());
#else
			v.template deviceToHost<T::value>();
			send_buf.add(v.template getPointer<T::value>());
#endif
		}
		else
		{
			send_buf.add(v.template getPointer<T::value>());
		}
	}
};

/*! \brief this class is a functor for "for_each" algorithm
 *
 * This class is a functor for "for_each" algorithm. For each
 * element of the boost::vector the operator() is called.
 * Is mainly used to copy one encap into another encap object
 *
 * \tparam encap source
 * \tparam encap dst
 *
 */

template<typename sT>
struct set_buf_size_for_each_prop
{
	//! set of pointers
	sT & v;

	openfpm::vector<size_t> & sz;

	/*! \brief constructor
	 *
	 * \param v set of pointer buffers to set
	 *
	 */
	inline set_buf_size_for_each_prop(sT & v, openfpm::vector<size_t> & sz)
	:v(v),sz(sz)
	{};

	//! It call the copy function for each property
	template<typename T>
	inline void operator()(T& t) const
	{
		typedef typename boost::mpl::at<typename sT::value_type::type,T>::type type_prp;

		sz.add(sizeof(type_prp)*v.size());
	}
};

template<typename T, bool impl = is_multiple_buffer_each_prp<T>::value >
struct pack_unpack_cond_with_prp_inte_lin
{
	static void set_buffers(T & send, openfpm::vector<const void *> & send_buf, size_t opt)
	{
		send_buf.add(send.getPointer());
	}

	static void set_size_buffers(T & send, openfpm::vector<size_t> & sz)
	{
		sz.add(send.size()*sizeof(typename T::value_type));
	}

	static void construct_prc(openfpm::vector<size_t> & prc_send, openfpm::vector<size_t> & prc_send_)
	{
		for (size_t i = 0 ; i < prc_send.size() ; i++)
		{
			prc_send_.add(prc_send.get(i));
		}
	}
};

// memory_traits_inte
template<typename T>
struct pack_unpack_cond_with_prp_inte_lin<T,true>
{
	static void set_buffers(T & send, openfpm::vector<const void *> & send_buf, size_t opt)
	{
		set_buf_pointer_for_each_prop<T> sbp(send,send_buf,opt);

		boost::mpl::for_each_ref<boost::mpl::range_c<int,0,T::value_type::max_prop>>(sbp);
	}

	static void set_size_buffers(T & send, openfpm::vector<size_t> & sz)
	{
		set_buf_size_for_each_prop<T> sbp(send,sz);

		boost::mpl::for_each_ref<boost::mpl::range_c<int,0,T::value_type::max_prop>>(sbp);
	}

	static void construct_prc(openfpm::vector<size_t> & prc_send, openfpm::vector<size_t> & prc_send_)
	{
		for (size_t i = 0 ; i < prc_send.size() ; i++)
		{
			for (size_t j = 0 ; j < T::value_type::max_prop ; j++)
			{prc_send_.add(prc_send.get(i));}
		}
	}
};

//! There is max_prop inside
template<bool cond,
         typename op,
		 typename T,
		 typename S,
		 template <typename> class layout_base,
		 unsigned int ... prp>
struct pack_unpack_cond_with_prp
{
	static void packingRequest(T & send, size_t & tot_size, openfpm::vector<size_t> & sz)
	{
		typedef typename ::generate_indexes<int, has_max_prop<T, has_value_type_ofp<T>::value>::number, MetaFuncOrd>::result ind_prop_to_pack;
		if (has_pack_gen<typename T::value_type>::value == false && is_vector<T>::value == true)
		{
			pack_unpack_cond_with_prp_inte_lin<T>::set_size_buffers(send,sz);
		}
		else
		{
			call_serialize_variadic<ind_prop_to_pack>::call_pr(send,tot_size);

			sz.add(tot_size);
		}
	}

	static void packing(ExtPreAlloc<HeapMemory> & mem, T & send, Pack_stat & sts, openfpm::vector<const void *> & send_buf, size_t opt = 0)
	{
		typedef typename ::generate_indexes<int, has_max_prop<T, has_value_type_ofp<T>::value>::number, MetaFuncOrd>::result ind_prop_to_pack;
		if (has_pack_gen<typename T::value_type>::value == false && is_vector<T>::value == true)
		{
			pack_unpack_cond_with_prp_inte_lin<T>::set_buffers(send,send_buf,opt);
		}
		else
		{
			send_buf.add(mem.getPointerEnd());
			call_serialize_variadic<ind_prop_to_pack>::call_pack(mem,send,sts);
		}
	}

	template<typename Memory>
	static void unpacking(S & recv,
			              openfpm::vector_fr<BMemory<Memory>> & recv_buf,
						  openfpm::vector<size_t> * sz,
						  openfpm::vector<size_t> * sz_byte,
						  op & op_param,
						  size_t opt)
	{
		typedef index_tuple<prp...> ind_prop_to_pack;
		call_serialize_variadic<ind_prop_to_pack>::template call_unpack<op,T,S,layout_base>(recv, recv_buf, sz, sz_byte, op_param,opt);
	}
};


/////////////////////////////

//! Helper class to add data without serialization
template<bool sr>
struct op_ssend_recv_add_sr
{
	//! Add data
	template<typename T,
	         typename D,
			 typename S,
			 template <typename> class layout_base,
			 int ... prp> static void execute(D & recv,S & v2, size_t i, size_t opt)
	{
		if (opt & MPI_GPU_DIRECT)
		{
#if defined(MPIX_CUDA_AWARE_SUPPORT) && MPIX_CUDA_AWARE_SUPPORT

			// Merge the information
			recv.template add_prp_device<typename T::value_type,
		                      PtrMemory,
							  openfpm::grow_policy_identity,
							  openfpm::vect_isel<typename T::value_type>::value,
							  layout_base,
							  prp...>(v2);
#else
			size_t old_size = recv.size();

			// Merge the information
			recv.template add_prp<typename T::value_type,
		                      PtrMemory,
							  openfpm::grow_policy_identity,
							  openfpm::vect_isel<typename T::value_type>::value,
							  layout_base,
							  prp...>(v2);

			recv.template hostToDevice<prp...>(old_size,old_size+v2.size()-1);

#endif

		}
		else
		{
			// Merge the information
			recv.template add_prp<typename T::value_type,
		                      PtrMemory,
							  openfpm::grow_policy_identity,
							  openfpm::vect_isel<typename T::value_type>::value,
							  layout_base,
							  prp...>(v2);
		}
	}
};

//! Helper class to add data with serialization
template<>
struct op_ssend_recv_add_sr<true>
{
	//! Add data
	template<typename T,
	         typename D,
			 typename S,
			 template <typename> class layout_base,
			 int ... prp>
	static void execute(D & recv,S & v2, size_t i,size_t opt)
	{
		// Merge the information
		recv.template add_prp<typename T::value_type,
		                      HeapMemory,
		                      typename T::grow_policy,
							  openfpm::vect_isel<typename T::value_type>::value,
							  layout_base,
							  prp...>(v2);
	}
};

//! Helper class to add data
template<typename op>
struct op_ssend_recv_add
{
	//! Add data
	template<bool sr,
	         typename T,
			 typename D,
			 typename S,
			 template <typename> class layout_base,
			 int ... prp>
	static void execute(D & recv,S & v2, size_t i, size_t opt)
	{
		// Merge the information
		op_ssend_recv_add_sr<sr>::template execute<T,D,S,layout_base,prp...>(recv,v2,i,opt);
	}
};

//! Helper class to merge data without serialization
template<bool sr,template<typename,typename> class op, typename vector_type_opart>
struct op_ssend_recv_merge_impl
{
	//! Merge the
	template<typename T,
	         typename D,
			 typename S,
			 template <typename> class layout_base,
			 int ... prp>
	inline static void execute(D & recv,S & v2,size_t i,vector_type_opart & opart)
	{
		// Merge the information
		recv.template merge_prp_v<op,
		                          typename T::value_type,
								  PtrMemory,
								  openfpm::grow_policy_identity,
								  layout_base,
								  typename vector_type_opart::value_type,
								  prp...>(v2,opart.get(i));
	}
};

//! Helper class to merge data with serialization
template<template<typename,typename> class op, typename vector_type_opart>
struct op_ssend_recv_merge_impl<true,op,vector_type_opart>
{
	//! merge the data
	template<typename T,
	         typename D,
			 typename S,
			 template <typename> class layout_base,
			 int ... prp>
	inline static void execute(D & recv,S & v2,size_t i,vector_type_opart & opart)
	{
		// Merge the information
		recv.template merge_prp_v<op,
		                          typename T::value_type,
								  HeapMemory,
								  openfpm::grow_policy_double,
								  layout_base,
								  typename vector_type_opart::value_type,
								  prp...>(v2,opart.get(i));
	}
};

//! Helper class to merge data
template<template<typename,typename> class op, typename vector_type_opart>
struct op_ssend_recv_merge
{
	//! For each processor contain the list of the particles with which I must merge the information
	vector_type_opart & opart;

	//! constructor
	op_ssend_recv_merge(vector_type_opart & opart)
	:opart(opart)
	{}

	//! execute the merge
	template<bool sr,
	         typename T,
			 typename D,
			 typename S,
			 template <typename> class layout_base,
			 int ... prp>
	void execute(D & recv,S & v2,size_t i,size_t opt)
	{
		op_ssend_recv_merge_impl<sr,op,vector_type_opart>::template execute<T,D,S,layout_base,prp...>(recv,v2,i,opart);
	}
};

//! Helper class to merge data without serialization
template<bool sr,template<typename,typename> class op, typename vector_type_opart, typename vector_type_prc_offset>
struct op_ssend_recv_merge_gpu_impl
{
	//! Merge the
	template<typename T,
	         typename D,
			 typename S,
			 template <typename> class layout_base,
			 int ... prp>
	inline static void execute(D & recv,S & v2,size_t i,vector_type_opart & opart, vector_type_prc_offset & prc_off)
	{
		prc_off.template deviceToHost<0>();

		unsigned int start = 0;
		unsigned int stop = prc_off.template get<0>(i / sizeof...(prp));

		if (i != 0)
		{start = prc_off.template get<0>(i / sizeof...(prp)-1);}

		// Merge the information
		recv.template merge_prp_v_device<op,
		                          typename T::value_type,
								  PtrMemory,
								  openfpm::grow_policy_identity,
								  layout_base,
								  vector_type_opart,
								  prp...>(v2,opart,start,stop);
	}
};

//! Helper class to merge data with serialization
template<template<typename,typename> class op, typename vector_type_opart, typename vector_type_prc_offset>
struct op_ssend_recv_merge_gpu_impl<true,op,vector_type_opart,vector_type_prc_offset>
{
	//! merge the data
	template<typename T,
	         typename D,
			 typename S,
			 template <typename> class layout_base,
			 int ... prp>
	inline static void execute(D & recv,S & v2,size_t i,vector_type_opart & opart, vector_type_prc_offset & prc_off)
	{
		std::cout << __FILE__ << ":" << __LINE__ << " Error: not implemented" << std::endl;
	}
};

//! Helper class to merge data
template<template<typename,typename> class op, typename vector_type_opart, typename vector_type_prc_offset>
struct op_ssend_recv_merge_gpu
{
	//! For each processor contain the list of the particles with which I must merge the information
	vector_type_opart & opart;

	vector_type_prc_offset & prc_offset;

	//! constructor
	op_ssend_recv_merge_gpu(vector_type_opart & opart, vector_type_prc_offset & prc_offset)
	:opart(opart),prc_offset(prc_offset)
	{}

	//! execute the merge
	template<bool sr,
	         typename T,
			 typename D,
			 typename S,
			 template <typename> class layout_base,
			 int ... prp>
	void execute(D & recv,S & v2,size_t i,size_t opt)
	{
		op_ssend_recv_merge_gpu_impl<sr,op,vector_type_opart,vector_type_prc_offset>::template execute<T,D,S,layout_base,prp...>(recv,v2,i,opart,prc_offset);
	}
};

//! Helper class to merge data without serialization
template<bool sr>
struct op_ssend_gg_recv_merge_impl
{
	//! Merge the
	template<typename T,
	         typename D,
			 typename S,
			 template <typename> class layout_base,
			 int ... prp>
	inline static void execute(D & recv,S & v2,size_t i,size_t & start)
	{
		// Merge the information
		recv.template merge_prp_v<replace_,
		                          typename T::value_type,
								  PtrMemory,
								  openfpm::grow_policy_identity,
								  layout_base,
								  prp...>(v2,start);

		start += v2.size();
	}
};

//! Helper class to merge data without serialization, using host memory
template<bool sr>
struct op_ssend_gg_recv_merge_impl_run_device
{
	//! Merge the
	template<typename T,
	         typename D,
			 typename S,
			 template <typename> class layout_base,
			 int ... prp>
	inline static void execute(D & recv,S & v2,size_t i,size_t & start)
	{
		// Merge the information
		recv.template merge_prp_v<replace_,
		                          typename T::value_type,
								  typename S::Memory_type,
								  openfpm::grow_policy_identity,
								  layout_base,
								  prp...>(v2,start);

		recv.template hostToDevice<prp ...>(start,start+v2.size()-1);

		start += v2.size();
	}
};

//! Helper class to merge data without serialization direct transfer to CUDA buffer
template<bool sr>
struct op_ssend_gg_recv_merge_impl_run_device_direct
{
	//! Merge the
	template<typename T,
	         typename D,
			 typename S,
			 template <typename> class layout_base,
			 int ... prp>
	inline static void execute(D & recv,S & v2,size_t i,size_t & start)
	{
		// Merge the information
		recv.template merge_prp_device<replace_,
		                          typename T::value_type,
								  typename S::Memory_type,
								  openfpm::grow_policy_identity,
								  prp...>(v2,start);

		start += v2.size();
	}
};

//! Helper class to merge data with serialization
template<>
struct op_ssend_gg_recv_merge_impl<true>
{
	//! merge the data
	template<typename T,
	         typename D,
			 typename S,
			 template <typename> class layout_base,
			 int ... prp> inline static void execute(D & recv,S & v2,size_t i,size_t & start)
	{
		// Merge the information
		recv.template merge_prp_v<replace_,
		                          typename T::value_type,
								  HeapMemory,
								  typename S::grow_policy,
								  layout_base,
								  prp...>(v2,start);

		// from
		start += v2.size();
	}
};

//! Helper class to merge data
struct op_ssend_gg_recv_merge
{
	//! starting marker
	size_t start;

	//! constructor
	op_ssend_gg_recv_merge(size_t start)
	:start(start)
	{}

	//! execute the merge
	template<bool sr, typename T, typename D, typename S, template<typename> class layout_base, int ... prp> void execute(D & recv,S & v2,size_t i,size_t opt)
	{
		op_ssend_gg_recv_merge_impl<sr>::template execute<T,D,S,layout_base,prp...>(recv,v2,i,start);
	}
};

//! Helper class to merge data
struct op_ssend_gg_recv_merge_run_device
{
	//! starting marker
	size_t start;

	//! constructor
	op_ssend_gg_recv_merge_run_device(size_t start)
	:start(start)
	{}

	//! execute the merge
	template<bool sr, typename T, typename D, typename S, template<typename> class layout_base, int ... prp> void execute(D & recv,S & v2,size_t i,size_t opt)
	{
		bool active = is_mpi_rdma_cuda_active();
		if (active == true)
		{op_ssend_gg_recv_merge_impl_run_device_direct<sr>::template execute<T,D,S,layout_base,prp...>(recv,v2,i,start);}
		else
		{op_ssend_gg_recv_merge_impl_run_device<sr>::template execute<T,D,S,layout_base,prp...>(recv,v2,i,start);}
	}
};

//////////////////////////////////////////////////



#endif /* OPENFPM_VCLUSTER_SRC_VCLUSTER_VCLUSTER_META_FUNCTION_HPP_ */
