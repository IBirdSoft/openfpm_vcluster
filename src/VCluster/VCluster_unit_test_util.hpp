/*
 * VCluster_unit_test_util.hpp
 *
 *  Created on: May 30, 2015
 *      Author: i-bird
 */

#ifndef VCLUSTER_UNIT_TEST_UTIL_HPP_
#define VCLUSTER_UNIT_TEST_UTIL_HPP_

#include "Point_test.hpp"
#include "VCluster_base.hpp"
#include "Vector/vector_test_util.hpp"

#define RECEIVE_UNKNOWN 1
#define RECEIVE_SIZE_UNKNOWN 2

#define NBX 1

#define N_TRY 2
#define N_LOOP 67108864
#define BUFF_STEP 524288
#define P_STRIDE 17

bool totp_check;
size_t global_step = 0;
size_t global_rank;

/*! \brief calculate the x mob m
 *
 * \param x
 * \param m
 *
 */
int mod(int x, int m) {
    return (x%m + m)%m;
}

// Alloc the buffer to receive the messages

//! [message alloc]

void * msg_alloc(size_t msg_i ,size_t total_msg, size_t total_p, size_t i,size_t ri, void * ptr)
{
	// convert the void pointer argument into a pointer to receiving buffers
	openfpm::vector<openfpm::vector<unsigned char>> * v = static_cast<openfpm::vector<openfpm::vector<unsigned char>> *>(ptr);

	/////////////////////// IGNORE THESE LINES IN VCLUSTER DOCUMENTATION ////////////////////////
	/////////////////////// THEY COME FROM UNIT TESTING /////////////////////////////////////////

	if (create_vcluster().getProcessingUnits() <= 8)
	{if (totp_check) BOOST_REQUIRE_EQUAL(total_p,create_vcluster().getProcessingUnits()-1);}
	else
	{if (totp_check) BOOST_REQUIRE_EQUAL(total_p,(size_t)8);}

	BOOST_REQUIRE_EQUAL(msg_i, global_step);

	//////////////////////////////////////////////////////////////////////////

	// Create the memory to receive the message
	// msg_i contain the size of the message to receive
	// i contain the processor id
	v->get(i).resize(msg_i);

	// return the pointer of the allocated memory
	return &(v->get(i).get(0));
}

//! [message alloc]

// Alloc the buffer to receive the messages

size_t id = 0;
openfpm::vector<size_t> prc_recv;

void * msg_alloc2(size_t msg_i ,size_t total_msg, size_t total_p, size_t i, size_t ri, void * ptr)
{
	openfpm::vector<openfpm::vector<unsigned char>> * v = static_cast<openfpm::vector<openfpm::vector<unsigned char>> *>(ptr);

	v->resize(total_p);
	prc_recv.resize(total_p);

	BOOST_REQUIRE_EQUAL(msg_i, global_step);

	id++;
	v->get(id-1).resize(msg_i);
	prc_recv.get(id-1) = i;
	return &(v->get(id-1).get(0));
}

void * msg_alloc3(size_t msg_i ,size_t total_msg, size_t total_p, size_t i, size_t ri, void * ptr)
{
	openfpm::vector<openfpm::vector<unsigned char>> * v = static_cast<openfpm::vector<openfpm::vector<unsigned char>> *>(ptr);

	v->add();

	prc_recv.add();

	BOOST_REQUIRE_EQUAL(msg_i, global_step);

	v->last().resize(msg_i);
	prc_recv.last() = i;
	return &(v->last().get(0));
}

template<unsigned int ip, typename T> void commFunc(Vcluster & vcl,openfpm::vector< size_t > & prc, openfpm::vector< T > & data, void * (* msg_alloc)(size_t,size_t,size_t,size_t,size_t,void *), void * ptr_arg)
{
	vcl.sendrecvMultipleMessagesNBX(prc,data,msg_alloc,ptr_arg);
}

template<unsigned int ip, typename T> void commFunc_null_odd(Vcluster & vcl,openfpm::vector< size_t > & prc, openfpm::vector< T > & data, void * (* msg_alloc)(size_t,size_t,size_t,size_t,size_t,void *), void * ptr_arg)
{
	if (vcl.getProcessUnitID() % 2 == 0)
		vcl.sendrecvMultipleMessagesNBX(prc,data,msg_alloc,ptr_arg);
	else
	{
		// No send check if passing null to sendrecv  work
		vcl.sendrecvMultipleMessagesNBX(prc.size(),(size_t *)NULL,(size_t *)NULL,(void **)NULL,msg_alloc,ptr_arg,NONE);
	}
}

template <unsigned int ip> std::string method()
{
	return std::string("NBX");
}

template<unsigned int ip> void test_no_send_some_peer()
{
	Vcluster & vcl = create_vcluster();

	size_t n_proc = vcl.getProcessingUnits();

	// Check long communication with some peer not comunication

	size_t j = 4567;

	global_step = j;
	// Processor step
	long int ps = n_proc / (8 + 1);

	// send message
	openfpm::vector<openfpm::vector<unsigned char>> message;
	// recv message
	openfpm::vector<openfpm::vector<unsigned char>> recv_message(n_proc);

	openfpm::vector<size_t> prc;

	// only even communicate

	if (vcl.getProcessUnitID() % 2 == 0)
	{
		for (size_t i = 0 ; i < 8  && i < n_proc ; i++)
		{
			size_t p_id = ((i+1) * ps + vcl.getProcessUnitID()) % n_proc;
			if (p_id != vcl.getProcessUnitID())
			{
				prc.add(p_id);
				message.add();
				std::ostringstream msg;
				msg << "Hello from " << vcl.getProcessUnitID() << " to " << p_id;
				std::string str(msg.str());
				message.last().resize(j);
				memset(message.last().getPointer(),0,j);
				std::copy(str.c_str(),&(str.c_str())[msg.str().size()],&(message.last().get(0)));
			}
		}
	}

	recv_message.resize(n_proc);

#ifdef VERBOSE_TEST
	timer t;
	t.start();
#endif

	commFunc_null_odd<ip>(vcl,prc,message,msg_alloc,&recv_message);

#ifdef VERBOSE_TEST
	t.stop();
	double clk = t.getwct();
	double clk_max = clk;

	size_t size_send_recv = 2 * j * (prc.size());
	vcl.sum(size_send_recv);
	vcl.max(clk_max);
	vcl.execute();

	if (vcl.getProcessUnitID() == 0)
		std::cout << "(Long pattern: " << method<ip>() << ")Buffer size: " << j << "    Bandwidth (Average): " << size_send_recv / vcl.getProcessingUnits() / clk / 1e6 << " MB/s  " << "    Bandwidth (Total): " << size_send_recv / clk / 1e6 << " MB/s    Clock: " << clk << "   Clock MAX: " << clk_max <<"\n";
#endif

	// Check the message
	for (long int i = 0 ; i < 8  && i < (long int)n_proc ; i++)
	{
		long int p_id = (- (i+1) * ps + (long int)vcl.getProcessUnitID());
		if (p_id < 0)
			p_id += n_proc;
		else
			p_id = p_id % n_proc;

		if (p_id != (long int)vcl.getProcessUnitID())
		{
			// only even processor communicate
			if (p_id % 2 == 1)
				continue;

			std::ostringstream msg;
			msg << "Hello from " << p_id << " to " << vcl.getProcessUnitID();
			std::string str(msg.str());
			BOOST_REQUIRE_EQUAL(std::equal(str.c_str(),str.c_str() + str.size() ,&(recv_message.get(p_id).get(0))),true);
		}
		else
		{
			BOOST_REQUIRE_EQUAL((size_t)0,recv_message.get(p_id).size());
		}
	}
}

template<unsigned int ip> void test_known()
{
	Vcluster & vcl = create_vcluster();

	// send/recv messages

	global_rank = vcl.getProcessUnitID();
	size_t n_proc = vcl.getProcessingUnits();

	// Checking short communication pattern

	for (size_t s = 0 ; s < N_TRY ; s++)
	{
		for (size_t j = 32 ; j < N_LOOP ; j*=2)
		{
			global_step = j;
			// send message
			openfpm::vector<openfpm::vector<unsigned char>> message;
			// recv message
			openfpm::vector<openfpm::vector<unsigned char>> recv_message(n_proc);
			recv_message.reserve(n_proc);

			openfpm::vector<size_t> prc_recv;
			openfpm::vector<size_t> recv_sz;

			openfpm::vector<size_t> prc;

			for (size_t i = 0 ; i < 8  && i < n_proc ; i++)
			{
				size_t p_id = (i + 1 + vcl.getProcessUnitID()) % n_proc;
				if (p_id != vcl.getProcessUnitID())
				{
					prc.add(p_id);
					message.add();
					std::ostringstream msg;
					msg << "Hello from " << vcl.getProcessUnitID() << " to " << p_id;
					std::string str(msg.str());
					message.last().resize(j);
					memset(message.last().getPointer(),0,j);
					std::copy(str.c_str(),&(str.c_str())[msg.str().size()],&(message.last().get(0)));
				}
			}

			recv_message.resize(n_proc);
			// The pattern is not really random preallocate the receive buffer
			for (size_t i = 0 ; i < 8  && i < n_proc ; i++)
			{
				long int p_id = vcl.getProcessUnitID() - i - 1;
				if (p_id < 0)
					p_id += n_proc;
				else
					p_id = p_id % n_proc;

				if (p_id != (long int)vcl.getProcessUnitID())
				{
					prc_recv.add(p_id);
					recv_message.get(p_id).resize(j);
					recv_sz.add(j);
				}
			}

#ifdef VERBOSE_TEST
			timer t;
			t.start();
#endif

			vcl.sendrecvMultipleMessagesNBX(prc,message,prc_recv,recv_sz,msg_alloc,&recv_message);

#ifdef VERBOSE_TEST
			t.stop();

			double clk = t.getwct();
			double clk_max = clk;

			size_t size_send_recv = 2 * j * (prc.size());
			vcl.sum(size_send_recv);
			vcl.max(clk_max);
			vcl.execute();

			if (vcl.getProcessUnitID() == 0)
				std::cout << "(Short pattern: " << method<ip>() << ")Buffer size: " << j << "    Bandwidth (Average): " << size_send_recv / vcl.getProcessingUnits() / clk / 1e6 << " MB/s  " << "    Bandwidth (Total): " << size_send_recv / clk / 1e6 << " MB/s    Clock: " << clk << "   Clock MAX: " << clk_max <<"\n";
#endif

			// Check the message
			for (size_t i = 0 ; i < 8  && i < n_proc ; i++)
			{
				long int p_id = vcl.getProcessUnitID() - i - 1;
				if (p_id < 0)
					p_id += n_proc;
				else
					p_id = p_id % n_proc;

				if (p_id != (long int)vcl.getProcessUnitID())
				{
					std::ostringstream msg;
					msg << "Hello from " << p_id << " to " << vcl.getProcessUnitID();
					std::string str(msg.str());
					BOOST_REQUIRE_EQUAL(std::equal(str.c_str(),str.c_str() + str.size() ,&(recv_message.get(p_id).get(0))),true);
				}
				else
				{
					BOOST_REQUIRE_EQUAL((size_t)0,recv_message.get(p_id).size());
				}
			}
		}
	}
}

template<unsigned int ip> void test(unsigned int opt)
{
	Vcluster & vcl = create_vcluster();

	// send/recv messages

	global_rank = vcl.getProcessUnitID();
	size_t n_proc = vcl.getProcessingUnits();

	// Checking short communication pattern

	for (size_t s = 0 ; s < N_TRY ; s++)
	{
		for (size_t j = 32 ; j < N_LOOP ; j*=2)
		{
			global_step = j;

			//! [dsde]

			// We send one message for each processor (one message is an openfpm::vector<unsigned char>)
			// or an array of bytes
			openfpm::vector<openfpm::vector<unsigned char>> message;

			// receving messages. Each receiving message is an openfpm::vector<unsigned char>
			// or an array if bytes
			openfpm::vector<openfpm::vector<unsigned char>> recv_message(n_proc);

			// each processor communicate based on a list of processor
			openfpm::vector<size_t> prc;

			// We construct the processor list in particular in this case
			// each processor communicate with the 8 next (in id) processors
			for (size_t i = 0 ; i < 8  && i < n_proc ; i++)
			{
				size_t p_id = (i + 1 + vcl.getProcessUnitID()) % n_proc;

				// avoid to communicate with yourself
				if (p_id != vcl.getProcessUnitID())
				{
					// Create an hello message
					prc.add(p_id);
					message.add();
					std::ostringstream msg;
					msg << "Hello from " << vcl.getProcessUnitID() << " to " << p_id;
					std::string str(msg.str());
					message.last().resize(j);
					memset(message.last().getPointer(),0,j);
					std::copy(str.c_str(),&(str.c_str())[msg.str().size()],&(message.last().get(0)));
				}
			}

			// For simplicity we create in advance a receiving buffer for all processors
			recv_message.resize(n_proc);

			// The pattern is not really random preallocate the receive buffer
			for (size_t i = 0 ; i < 8  && i < n_proc ; i++)
			{
				long int p_id = vcl.getProcessUnitID() - i - 1;
				if (p_id < 0)
					p_id += n_proc;
				else
					p_id = p_id % n_proc;

				if (p_id != (long int)vcl.getProcessUnitID())
					recv_message.get(p_id).resize(j);
			}

			if (opt == RECEIVE_UNKNOWN)
			{
				// Send and receive
				vcl.sendrecvMultipleMessagesNBX(prc,message,msg_alloc,&recv_message);
			}
			//! [dsde]
			else if (opt == RECEIVE_SIZE_UNKNOWN)
			{
				openfpm::vector<size_t> sz_send;
				openfpm::vector<void *> ptr;

				openfpm::vector<size_t> prc_recv;

				sz_send.resize(prc.size());
				ptr.resize(prc.size());

				for (size_t i = 0 ; i < prc.size() ; i++)
				{
					sz_send.get(i) = message.get(i).size();
					ptr.get(i) = &message.get(i).get(0);
				}

				// Calculate the receiving part

				// Check the message
				for (size_t i = 0 ; i < 8  && i < n_proc ; i++)
				{
					long int p_id = vcl.getProcessUnitID() - i - 1;
					if (p_id < 0)
					{p_id += n_proc;}
					else
					{p_id = p_id % n_proc;}

					if (p_id != (long int)vcl.getProcessUnitID())
					{
						prc_recv.add(p_id);
					}
				}

				//! [dsde]

				// Send and receive
				vcl.sendrecvMultipleMessagesNBX(prc.size(),&sz_send.get(0),&prc.get(0),
												&ptr.get(0),prc_recv.size(),&prc_recv.get(0),msg_alloc,&recv_message);
			}

#ifdef VERBOSE_TEST
			timer t;
			t.start();
#endif


#ifdef VERBOSE_TEST
			t.stop();

			double clk = t.getwct();
			double clk_max = clk;

			size_t size_send_recv = 2 * j * (prc.size());
			vcl.sum(size_send_recv);
			vcl.max(clk_max);
			vcl.execute();

			if (vcl.getProcessUnitID() == 0)
				std::cout << "(Short pattern: " << method<ip>() << ")Buffer size: " << j << "    Bandwidth (Average): " << size_send_recv / vcl.getProcessingUnits() / clk / 1e6 << " MB/s  " << "    Bandwidth (Total): " << size_send_recv / clk / 1e6 << " MB/s    Clock: " << clk << "   Clock MAX: " << clk_max <<"\n";
#endif

			// Check the message
			for (size_t i = 0 ; i < 8  && i < n_proc ; i++)
			{
				long int p_id = vcl.getProcessUnitID() - i - 1;
				if (p_id < 0)
					p_id += n_proc;
				else
					p_id = p_id % n_proc;

				if (p_id != (long int)vcl.getProcessUnitID())
				{
					std::ostringstream msg;
					msg << "Hello from " << p_id << " to " << vcl.getProcessUnitID();
					std::string str(msg.str());
					BOOST_REQUIRE_EQUAL(std::equal(str.c_str(),str.c_str() + str.size() ,&(recv_message.get(p_id).get(0))),true);
				}
				else
				{
					BOOST_REQUIRE_EQUAL((size_t)0,recv_message.get(p_id).size());
				}
			}
		}

		if (opt == RECEIVE_SIZE_UNKNOWN)
		{return;}

		std::srand(create_vcluster().getProcessUnitID());
		std::default_random_engine eg;
		std::uniform_int_distribution<int> d(0,n_proc/8);

		// Check random pattern (maximum 16 processors)

		for (size_t j = 32 ; j < N_LOOP && n_proc < 16 ; j*=2)
		{
			global_step = j;
			// original send
			openfpm::vector<size_t> o_send;
			// send message
			openfpm::vector<openfpm::vector<unsigned char>> message;
			// recv message
			openfpm::vector<openfpm::vector<unsigned char>> recv_message;
//			recv_message.reserve(n_proc);

			openfpm::vector<size_t> prc;

			for (size_t i = 0 ; i < n_proc ; i++)
			{
				// randomly with which processor communicate
				if (d(eg) == 0)
				{
					prc.add(i);
					o_send.add(i);
					message.add();
					message.last().fill(0);
					std::ostringstream msg;
					msg << "Hello from " << vcl.getProcessUnitID() << " to " << i;
					std::string str(msg.str());
					message.last().resize(str.size());
					std::copy(str.c_str(),&(str.c_str())[msg.str().size()],&(message.last().get(0)));
					message.last().resize(j);
				}
			}

			id = 0;
			prc_recv.clear();


#ifdef VERBOSE_TEST
			timer t;
			t.start();
#endif

			commFunc<ip>(vcl,prc,message,msg_alloc3,&recv_message);

#ifdef VERBOSE_TEST
			t.stop();
			double clk = t.getwct();
			double clk_max = clk;

			size_t size_send_recv = (prc.size() + recv_message.size()) * j;
			vcl.sum(size_send_recv);
			vcl.sum(clk);
			vcl.max(clk_max);
			vcl.execute();
			clk /= vcl.getProcessingUnits();

			if (vcl.getProcessUnitID() == 0)
				std::cout << "(Random Pattern: " << method<ip>() << ") Buffer size: " << j << "    Bandwidth (Average): " << size_send_recv / vcl.getProcessingUnits() / clk / 1e6 << " MB/s  " << "    Bandwidth (Total): " << size_send_recv / clk / 1e6 <<  " MB/s    Clock: " << clk << "   Clock MAX: " << clk_max << "\n";
#endif

			// Check the message

			for (size_t i = 0 ; i < recv_message.size() ; i++)
			{
				std::ostringstream msg;
				msg << "Hello from " << prc_recv.get(i) << " to " << vcl.getProcessUnitID();
				std::string str(msg.str());
				BOOST_REQUIRE_EQUAL(std::equal(str.c_str(),str.c_str() + str.size() ,&(recv_message.get(i).get(0))),true);
			}

			// Reply back

			// Create the message

			prc.clear();
			message.clear();
			for (size_t i = 0 ; i < prc_recv.size() ; i++)
			{
				prc.add(prc_recv.get(i));
				message.add();
				std::ostringstream msg;
				msg << "Hey from " << vcl.getProcessUnitID() << " to " << prc_recv.get(i);
				std::string str(msg.str());
				message.last().resize(str.size());
				std::copy(str.c_str(),&(str.c_str())[msg.str().size()],&(message.last().get(0)));
				message.last().resize(j);
			}

			id = 0;
			prc_recv.clear();
			recv_message.clear();

			commFunc<ip>(vcl,prc,message,msg_alloc3,&recv_message);

			// Check if the received hey message match the original send

			BOOST_REQUIRE_EQUAL(o_send.size(),prc_recv.size());

			for (size_t i = 0 ; i < o_send.size() ; i++)
			{
				size_t j = 0;
				for ( ; j < prc_recv.size() ; j++)
				{
					if (o_send.get(i) == prc_recv.get(j))
					{
						// found the message check it

						std::ostringstream msg;
						msg << "Hey from " << prc_recv.get(i) << " to " << vcl.getProcessUnitID();
						std::string str(msg.str());
						BOOST_REQUIRE_EQUAL(std::equal(str.c_str(),str.c_str() + str.size() ,&(recv_message.get(i).get(0))),true);
						break;
					}
				}
				// Check that we find always a match
				BOOST_REQUIRE_EQUAL(j != prc_recv.size(),true);
			}
		}

		// Check long communication pattern

		for (size_t j = 32 ; j < N_LOOP ; j*=2)
		{
			global_step = j;
			// Processor step
			long int ps = n_proc / (8 + 1);

			// send message
			openfpm::vector<openfpm::vector<unsigned char>> message;
			// recv message
			openfpm::vector<openfpm::vector<unsigned char>> recv_message(n_proc);

			openfpm::vector<size_t> prc;

			for (size_t i = 0 ; i < 8  && i < n_proc ; i++)
			{
				size_t p_id = ((i+1) * ps + vcl.getProcessUnitID()) % n_proc;
				if (p_id != vcl.getProcessUnitID())
				{
					prc.add(p_id);
					message.add();
					std::ostringstream msg;
					msg << "Hello from " << vcl.getProcessUnitID() << " to " << p_id;
					std::string str(msg.str());
					message.last().resize(j);
					memset(message.last().getPointer(),0,j);
					std::copy(str.c_str(),&(str.c_str())[msg.str().size()],&(message.last().get(0)));
				}
			}

			recv_message.resize(n_proc);
			// The pattern is not really random preallocate the receive buffer
			for (size_t i = 0 ; i < 8  && i < n_proc ; i++)
			{
				long int p_id = (- (i+1) * ps + (long int)vcl.getProcessUnitID());
				if (p_id < 0)
					p_id += n_proc;
				else
					p_id = p_id % n_proc;

				if (p_id != (long int)vcl.getProcessUnitID())
					recv_message.get(p_id).resize(j);
			}

#ifdef VERBOSE_TEST
			timer t;
			t.start();
#endif

			commFunc<ip>(vcl,prc,message,msg_alloc,&recv_message);

#ifdef VERBOSE_TEST
			t.stop();
			double clk = t.getwct();
			double clk_max = clk;

			size_t size_send_recv = 2 * j * (prc.size());
			vcl.sum(size_send_recv);
			vcl.max(clk_max);
			vcl.execute();

			if (vcl.getProcessUnitID() == 0)
				std::cout << "(Long pattern: " << method<ip>() << ")Buffer size: " << j << "    Bandwidth (Average): " << size_send_recv / vcl.getProcessingUnits() / clk / 1e6 << " MB/s  " << "    Bandwidth (Total): " << size_send_recv / clk / 1e6 << " MB/s    Clock: " << clk << "   Clock MAX: " << clk_max <<"\n";
#endif

			// Check the message
			for (long int i = 0 ; i < 8  && i < (long int)n_proc ; i++)
			{
				long int p_id = (- (i+1) * ps + (long int)vcl.getProcessUnitID());
				if (p_id < 0)
					p_id += n_proc;
				else
					p_id = p_id % n_proc;

				if (p_id != (long int)vcl.getProcessUnitID())
				{
					std::ostringstream msg;
					msg << "Hello from " << p_id << " to " << vcl.getProcessUnitID();
					std::string str(msg.str());
					BOOST_REQUIRE_EQUAL(std::equal(str.c_str(),str.c_str() + str.size() ,&(recv_message.get(p_id).get(0))),true);
				}
				else
				{
					BOOST_REQUIRE_EQUAL((size_t)0,recv_message.get(p_id).size());
				}
			}
		}
	}
}

/*! \brief Test vectors send for complex structures
 *
 * \param n test size
 * \param vcl VCluster
 *
 */
void test_send_recv_complex(const size_t n, Vcluster & vcl)
{
	//! [Send and receive vectors of complex]

	// Point test typedef
	typedef Point_test<float> p;

	openfpm::vector<Point_test<float>> v_send = allocate_openfpm_fill(n,vcl.getProcessUnitID());

	// Send to 8 processors
	for (size_t i = 0 ; i < 8 ; i++)
		vcl.send( mod(vcl.getProcessUnitID() + i * P_STRIDE, vcl.getProcessingUnits()) ,i,v_send);

	openfpm::vector<openfpm::vector<Point_test<float>> > pt_buf;
	pt_buf.resize(8);

	// Recv from 8 processors
	for (size_t i = 0 ; i < 8 ; i++)
	{
		pt_buf.get(i).resize(n);
		vcl.recv( mod( (vcl.getProcessUnitID() - i * P_STRIDE), vcl.getProcessingUnits()) ,i,pt_buf.get(i));
	}

	vcl.execute();

	//! [Send and receive vectors of complex]

	// Check the received buffers (careful at negative modulo)
	for (size_t i = 0 ; i < 8 ; i++)
	{
		for (size_t j = 0 ; j < n ; j++)
		{
			Point_test<float> pt = pt_buf.get(i).get(j);

			size_t p_recv = mod( (vcl.getProcessUnitID() - i * P_STRIDE), vcl.getProcessingUnits());

			BOOST_REQUIRE_EQUAL(pt.template get<p::x>(),p_recv);
			BOOST_REQUIRE_EQUAL(pt.template get<p::y>(),p_recv);
			BOOST_REQUIRE_EQUAL(pt.template get<p::z>(),p_recv);
			BOOST_REQUIRE_EQUAL(pt.template get<p::s>(),p_recv);
			BOOST_REQUIRE_EQUAL(pt.template get<p::v>()[0],p_recv);
			BOOST_REQUIRE_EQUAL(pt.template get<p::v>()[1],p_recv);
			BOOST_REQUIRE_EQUAL(pt.template get<p::v>()[2],p_recv);
			BOOST_REQUIRE_EQUAL(pt.template get<p::t>()[0][0],p_recv);
			BOOST_REQUIRE_EQUAL(pt.template get<p::t>()[0][1],p_recv);
			BOOST_REQUIRE_EQUAL(pt.template get<p::t>()[0][2],p_recv);
			BOOST_REQUIRE_EQUAL(pt.template get<p::t>()[1][0],p_recv);
			BOOST_REQUIRE_EQUAL(pt.template get<p::t>()[1][1],p_recv);
			BOOST_REQUIRE_EQUAL(pt.template get<p::t>()[1][2],p_recv);
			BOOST_REQUIRE_EQUAL(pt.template get<p::t>()[2][0],p_recv);
			BOOST_REQUIRE_EQUAL(pt.template get<p::t>()[2][1],p_recv);
			BOOST_REQUIRE_EQUAL(pt.template get<p::t>()[2][2],p_recv);
		}
	}
}

/*! \brief Test vectors send for complex structures
 *
 * \tparam T primitives
 *
 * \param n size
 *
 */
template<typename T> void test_send_recv_primitives(size_t n, Vcluster & vcl)
{
	openfpm::vector<T> v_send = allocate_openfpm_primitive<T>(n,vcl.getProcessUnitID());

	{
	//! [Sending and receiving primitives]

	// Send to 8 processors
	for (size_t i = 0 ; i < 8 ; i++)
		vcl.send( mod(vcl.getProcessUnitID() + i * P_STRIDE, vcl.getProcessingUnits()) ,i,v_send);

	openfpm::vector<openfpm::vector<T> > pt_buf;
	pt_buf.resize(8);

	// Recv from 8 processors
	for (size_t i = 0 ; i < 8 ; i++)
	{
		pt_buf.get(i).resize(n);
		vcl.recv( mod( (vcl.getProcessUnitID() - i * P_STRIDE), vcl.getProcessingUnits()) ,i,pt_buf.get(i));
	}

	vcl.execute();

	//! [Sending and receiving primitives]

	// Check the received buffers (careful at negative modulo)
	for (size_t i = 0 ; i < 8 ; i++)
	{
		for (size_t j = 0 ; j < n ; j++)
		{
			T pt = pt_buf.get(i).get(j);

			T p_recv = mod( (vcl.getProcessUnitID() - i * P_STRIDE), vcl.getProcessingUnits());

			BOOST_REQUIRE_EQUAL(pt,p_recv);
		}
	}

	}

	{
	//! [Send and receive plain buffer data]

	// Send to 8 processors
	for (size_t i = 0 ; i < 8 ; i++)
		vcl.send( mod(vcl.getProcessUnitID() + i * P_STRIDE, vcl.getProcessingUnits()) ,i,v_send.getPointer(),v_send.size()*sizeof(T));

	openfpm::vector<openfpm::vector<T> > pt_buf;
	pt_buf.resize(8);

	// Recv from 8 processors
	for (size_t i = 0 ; i < 8 ; i++)
	{
		pt_buf.get(i).resize(n);
		vcl.recv( mod( (vcl.getProcessUnitID() - i * P_STRIDE), vcl.getProcessingUnits()) ,i,pt_buf.get(i).getPointer(),pt_buf.get(i).size()*sizeof(T));
	}

	vcl.execute();

	//! [Send and receive plain buffer data]

	// Check the received buffers (careful at negative modulo)
	for (size_t i = 0 ; i < 8 ; i++)
	{
		for (size_t j = 0 ; j < n ; j++)
		{
			T pt = pt_buf.get(i).get(j);

			T p_recv = mod( (vcl.getProcessUnitID() - i * P_STRIDE), vcl.getProcessingUnits());

			BOOST_REQUIRE_EQUAL(pt,p_recv);
		}
	}

	}
}

template<typename T>  void test_single_all_gather_primitives(Vcluster & vcl)
{
	//! [allGather numbers]

	openfpm::vector<T> clt;
	T data = vcl.getProcessUnitID();

	vcl.allGather(data,clt);
	vcl.execute();

	for (size_t i = 0 ; i < vcl.getProcessingUnits() ; i++)
		BOOST_REQUIRE_EQUAL(i,(size_t)clt.get(i));

	//! [allGather numbers]

}

template<typename T>  void test_single_all_broadcast_primitives(Vcluster & vcl)
{
	//! [bcast numbers]

	openfpm::vector<T> bdata;

	if (vcl.getProcessUnitID() == 0)
	{
		bdata.add(0);
		bdata.add(1);
		bdata.add(2);
		bdata.add(3);
		bdata.add(4);
		bdata.add(5);
		bdata.add(6);
	}
	else
	{
		bdata.resize(7);
	}

	vcl.Bcast(bdata,0);
	vcl.execute();

	for (size_t i = 0 ; i < bdata.size() ; i++)
		BOOST_REQUIRE_EQUAL(i,(size_t)bdata.get(i));

	//! [bcast numbers]

}

#endif /* VCLUSTER_UNIT_TEST_UTIL_HPP_ */
